//===-- SHAVEAddressSimplifier.cpp - Pre-RASched Peephole Pass --*- C++ -*-===//
//
// INTEL CONFIDENTIAL
//
// Copyright 2025 Intel Corporation.
//
// This software and the related documents are Intel copyrighted materials, and
// your use of them is governed by the express license under which they were
// provided to you ("License"). Unless the License provides otherwise, you may
// not use, modify, copy, publish, distribute, disclose or transmit this software
// or the related documents without Intel's prior written permission.
//
// This software and the related documents are provided as is, with no express or
// implied warranties, other than those that are expressly stated in the License.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "shave-address-simplifier"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/Debug.h"

#include "SHAVEAddressSimplifier.h"

using namespace llvm;

//
// Initialisation code required by the Pass Manager
//
char SHAVEAddressSimplifier::ID = 0;

INITIALIZE_PASS_BEGIN(SHAVEAddressSimplifier, "shaveaddresssimplifierpass", "SHAVE Address Simplifier Pass", false, false)
INITIALIZE_PASS_END(SHAVEAddressSimplifier, "shaveaddresssimplifierpass", "SHAVE Address Simplifier Pass", false, false)

MachineFunctionPass *llvm::createSHAVEAddressSimplifierPass() {
  return new SHAVEAddressSimplifier();
}

//
// Definition of private member functions of SHAVEAddressSimplifier
//

void SHAVEAddressSimplifier::generateBlockInfo(MachineBasicBlock &block) {
  loopVariantRegisters.clear();

  for (MachineInstr &instruction : block) {
    for (MachineOperand &def : instruction.defs()) {      
      loopVariantRegisters.insert(def.getReg());
    }
  }
}

bool SHAVEAddressSimplifier::isLoopInvariant(unsigned int reg) const {
  return loopVariantRegisters.find(reg) == loopVariantRegisters.end();
}

/*
 * For an add instruction to be "targeted", its operands must meet these criteria:
 *    - The defined register must not be live-in to any of the exit blocks of the loop
 *    - One of the input operands must be loop invariant
 */
bool SHAVEAddressSimplifier::isTargetedAdd(MachineInstr &instruction, unsigned int &defRegister, unsigned int &incrementRegister) const {
  unsigned int reg1 = SHAVE::NoRegister;
  unsigned int reg2 = SHAVE::NoRegister;
  incrementRegister = SHAVE::NoRegister;

  switch (instruction.getOpcode()) {
  case SHAVE::IAU_ADD_32:
    if (!instruction.getOperand(2).isReg())
      return false;
    reg2 = instruction.getOperand(2).getReg();
    LLVM_FALLTHROUGH;
  case SHAVE::IAU_ADD_32_imm:
    if (!instruction.getOperand(1).isReg())
      return false;
    reg1 = instruction.getOperand(1).getReg();
    defRegister = instruction.getOperand(0).getReg();

    if (isLoopInvariant(reg1)) {
      if (!isLoopInvariant(reg2)) {
        // Only use loop-variant value if it is defined by a PHI
        for (MachineInstr &findPHI : *(instruction.getParent())) {
          if (findPHI.getOpcode() != SHAVE::PHI)
            return false;
          if (findPHI.definesRegister(reg2)) {
            incrementRegister = reg2;
            break;
          }
        }
      }
      return true;
    }
    else if (reg2 != SHAVE::NoRegister && isLoopInvariant(reg2)) {
      // Only use loop-variant value if it is defined by a PHI
      for (MachineInstr &findPHI : *(instruction.getParent())) {
        if (findPHI.getOpcode() != SHAVE::PHI)
          return false;
        if (findPHI.definesRegister(reg1)) {
          incrementRegister = reg1;
          break;
        }
      }
      return true;
    }
    return false;
  default:
    return false;
  }

  return false;
}

bool SHAVEAddressSimplifier::findAddressComputation(MachineInstr &instruction, SHAVEAddressSimplifier::TransformData &transformData) const {
  // Only looking for load instructions, some barrier instructions are marked with mayLoad
  // and mayStore and we need to ignore those
  if (!instruction.mayLoad() || instruction.mayStore())
    return false;

  unsigned int addressReg = SHAVE::NoRegister;
  for (MachineOperand &operand : instruction.operands()) {
    // First non-def register is the memory address
    if (operand.isReg() && !operand.isDef()) {
      addressReg = operand.getReg();
      break;
    }
  }

  // This shouldn't be possible, but safety first
  if (addressReg == SHAVE::NoRegister)
    return false;

  // The address computation is either unsupported or outside of the loop body
  if (availableAdds.find(addressReg) == availableAdds.end())
    return false;

  transformData.addressComputation = availableAdds.at(addressReg).first;

  unsigned int incrementRegister = availableAdds.at(addressReg).second;

  // Early success, there is no loop variant increment that we need to modify
  if (incrementRegister == SHAVE::NoRegister)
    return true;

  MachineBasicBlock &block = *instruction.getParent();
  for (MachineInstr &checkInstruction : block) {
    // No increment exists, so we can safely hoist the address computation on its own
    if (&checkInstruction == transformData.addressComputation)
      return true;
    
    // Searching for the pattern:
    //   loop:
    //     %incrementRegister = PHI %updatedIncrement, loop, ...                ; checkInstruction
    //     %addressReg = IAU_ADD_32 %base, %incrementRegister                   ; availableAdds.at(addressReg)
    //     ...
    //     %updatedIncrement = IAU_ADD_32 killed %incrementRegister, %incAmount ; increment
    if (checkInstruction.getOpcode() == SHAVE::PHI && checkInstruction.getOperand(0).getReg() == incrementRegister) {
      transformData.addressPHI = &checkInstruction;
      unsigned int updatedIncrement = SHAVE::NoRegister;
      // Extract the updatedIncrement register from the PHI instruction which defines the incremented pointer register
      for (unsigned int i = 1; (i+1) < checkInstruction.getNumOperands(); i+=2)
        if (checkInstruction.getOperand(i+1).getMBB() == &block)
          updatedIncrement = checkInstruction.getOperand(i).getReg();

      if (updatedIncrement == SHAVE::NoRegister)
        return false;

      // Find the "copy" instruction

      for (auto increment = block.rbegin(); increment != block.rend(); ++increment) {
        if (increment->definesRegister(updatedIncrement)) {
          unsigned int opcode = increment->getOpcode();
          if (opcode == SHAVE::IAU_ADD_32 || opcode == SHAVE::IAU_ADD_32_imm) {
            for (MachineOperand &operand : increment->uses()) {
              if (operand.isReg() && operand.getReg() == incrementRegister) {
                transformData.addressIncrement = &*increment;
                transformData.updatedIncrementRegister = updatedIncrement;
                transformData.loopIncrementRegister = incrementRegister;
                return true;
              }
            }
          }
          return false;
        }
      }

      return false;
    }
  }

  // Failed to find address increment
  return false;
}

void SHAVEAddressSimplifier::cloneAddressComputation(MachineBasicBlock &block, SHAVEAddressSimplifier::TransformData &transformData) {

  for (MachineBasicBlock *predecessor : block.predecessors()) {
    if (predecessor == &block)
      continue;

    unsigned int newRegister = currentFunction->getRegInfo().createVirtualRegister(&SHAVE::IRF32RegClass);
    
    DEBUG(dbgs() << "Cloning "; transformData.addressComputation->dump());
    DEBUG(dbgs() << "  into block #" << predecessor->getNumber() << ": " << predecessor->getName() << "\n");

    MachineInstr *newInstruction = currentFunction->CloneMachineInstr(transformData.addressComputation);
    auto insertPoint = predecessor->getFirstTerminator();
    predecessor->insert(insertPoint, newInstruction);

    // Replace the base address register with our newly created register
    for (MachineOperand &operand : newInstruction->operands())
      if (operand.isReg() && operand.getReg() == transformData.addressRegister)
        operand.setReg(newRegister);

    // Bypass the copy instructions inserted for PHIs by replacing the register use
    if (transformData.addressPHI != nullptr) {
      unsigned initialBaseRegister = SHAVE::NoRegister;
      for (unsigned int i = 1; (i+1) < transformData.addressPHI->getNumOperands(); i+=2)
        if (transformData.addressPHI->getOperand(i+1).getMBB() == predecessor)
          initialBaseRegister = transformData.addressPHI->getOperand(i).getReg();

      for (MachineOperand &operand : newInstruction->operands())
        if (operand.isReg() && operand.getReg() == transformData.loopIncrementRegister)
          operand.setReg(initialBaseRegister);
    }

    DEBUG(dbgs() << "  New instuction: "; newInstruction->dump());

    transformData.newPointerRegisters.push_back(std::make_pair(predecessor, newRegister));
  }

  // Remove the original computation now that it has been cloned to all predecessor blocks
  transformData.addressComputation->eraseFromParent();
}

void SHAVEAddressSimplifier::fixRegisters(MachineBasicBlock &block,  SHAVEAddressSimplifier::TransformData &transformData) {
  // New incremented pointer register for the new increment instruction to define and the PHI to use
  unsigned int newRegister = currentFunction->getRegInfo().createVirtualRegister(&SHAVE::IRF32RegClass);

  // Insert a new PHI instruction to get the preheader pointer value, or the incremented pointer value on the loop back-edge
  MachineInstrBuilder newPHI = BuildMI(block, block.begin(), block.begin()->getDebugLoc(), SII->get(SHAVE::PHI), transformData.addressRegister);
  for (auto &blockData : transformData.newPointerRegisters) {
    newPHI.addReg(blockData.second);
    newPHI.addMBB(blockData.first);
  }
  if (transformData.addressIncrement != nullptr)
    newPHI.addReg(newRegister);
  else
    newPHI.addReg(transformData.addressRegister);
  newPHI.addMBB(&block);

  DEBUG(dbgs() << "  New PHI: "; newPHI->dump());

  // Insert a new increment instruction
  if (transformData.addressIncrement != nullptr) {
    MachineInstr * newIncrement = currentFunction->CloneMachineInstr(transformData.addressIncrement);
    block.insert(transformData.addressIncrement->getIterator(), newIncrement);
    for (MachineOperand &operand : newIncrement->operands()) {
      if (operand.isReg() && operand.getReg() == transformData.loopIncrementRegister)
        operand.setReg(transformData.addressRegister);
      else if (operand.isReg() && operand.isDef() && operand.getReg() == transformData.updatedIncrementRegister)
        operand.setReg(newRegister);
    }
    DEBUG(dbgs() << "  New address increment: "; newIncrement->dump());
  }

  // The addressRegister is now live until the update instruction, so we must remove isKill from any previous uses
  for (MachineInstr &instruction : block) {
    for (MachineOperand &operand : instruction.uses())
      if (operand.isReg() && operand.getReg() == transformData.addressRegister && operand.isKill())
        operand.setIsKill(false);
  }
}

/*
 * This transformation aims to take this pattern: 
 *   loop:
 *     ptr = IAU_ADD_32 base, offsetv
 *     ... = LSU_LD* ptr, ...
 *     ...
 *     ...
 *     base = IAU_ADD_32_imm base, offsetc
 * 
 * and turn it into this:
 *   preheader:
 *     ptr = IAU_ADD_32 base, offsetv
 * 
 *   loop:
 *     ... = LSU_LD* ptr, ...
 *     ...
 *     ...
 *     ptr = IAU_ADD_32_imm ptr, offsetc
 * 
 * where:
 *   - each of ptr, base and offsetv are distinct registers
 *   - ptr is not live-out from loop (i.e. not live-in to exit block or the loop block)
 *   - offsetv is loop invariant
 * 
 * base and offsetv may be swapped in the IAU_ADD_32, which is also targetted
 * 
 * This will be applied to inner-most loops only
 */
bool SHAVEAddressSimplifier::modifyOffsetAddresses() {
  bool modified = false;

  for (MachineBasicBlock &block : *currentFunction) {
    const auto &successors = block.successors();
    // Inner-most loops only
    if (std::find(successors.begin(), successors.end(), &block) != successors.end()) {
      DEBUG(dbgs() << "Running on basic block #" << block.getNumber() << ": " << block.getName() << "\n");
      
      bool blockChanged = true;
      while (blockChanged) {
        blockChanged = false;
        availableAdds.clear();
        generateBlockInfo(block);

        for (MachineInstr &instruction : block.instrs()) {
          // If this is an add instruction that we are targeting with this optimisation,
          // then save it for later and move on
          unsigned int addRegister = SHAVE::NoRegister;
          unsigned int incrementRegister = SHAVE::NoRegister;
          if (isTargetedAdd(instruction, addRegister, incrementRegister)) {
            DEBUG(dbgs() << "Found add instruction with addRegister %" << Register::virtReg2Index(addRegister)
                         << " incrementRegister %" << Register::virtReg2Index(incrementRegister) << ": ");
            DEBUG(instruction.dump());
            availableAdds[addRegister] = std::make_pair(&instruction, incrementRegister);
            continue;
          }

          TransformData transformData;
        
          // Find load instructions and then work backwards for their address computation
          bool candidateLoad = findAddressComputation(instruction, transformData);
          if (!candidateLoad) {
            // Previous computations may be clobbered, so erase those before continuing
            for (MachineOperand &operand : instruction.operands())
              if (operand.isReg() && operand.isDef())
                availableAdds.erase(operand.getReg());
          
            continue;
          }

          DEBUG(dbgs() << "Candidate load and add instructions:\n");
          DEBUG(transformData.addressComputation->dump());
          DEBUG(instruction.dump());
          DEBUG(if (transformData.addressIncrement != nullptr) transformData.addressIncrement->dump());

          transformData.addressRegister = transformData.addressComputation->getOperand(0).getReg();

          // Clone the address computation block into all predecessors of the loop block
          cloneAddressComputation(block, transformData);
          
          // We need to replace the address register with this one
          fixRegisters(block, transformData);

          blockChanged = modified = true;
          break;
        }
      }
    }
  }

  return modified;
}

//
// Definition of public member functions of SHAVEAddressSimplifier
//
bool SHAVEAddressSimplifier::runOnMachineFunction(MachineFunction &MF) {
  DEBUG(dbgs() << "SHAVEAddressSimplifier: Starting address simplification for machine function " << MF.getName() << "\n");
  bool modified = false;

  currentFunction = &MF;
  SII = currentFunction->getSubtarget<SHAVESubtarget>().getInstrInfo();
  
  modified |= modifyOffsetAddresses();

  DEBUG(dbgs() << "SHAVEAddressSimplifier: Finished address simplification for machine function " << MF.getName() << "\n");

  return modified;
}
