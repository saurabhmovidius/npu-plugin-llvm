//===-- SHAVEPreemptionHandler.cpp - Insert Preemption Handling -*- C++ -*-===//
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
//
// Inserts the SHAVE code needed to catch and handle preemption signals
// when they are enabled.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "shave-preemption-handler"

#include "SHAVEPreemptionHandler.h"

#include "llvm/InitializePasses.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Debug.h"

STATISTIC(NumberOfLoopBackEdges, "Number of loop back edges seen by SHAVEPreemptionHandler");
STATISTIC(GenericLoops, "Number of loop back edges with generic preemption by SHAVEPreemptionHandler");
STATISTIC(OptimisedLoops, "Number of loop back edges with optimised preemption by SHAVEPreemptionHandler");

using namespace llvm;

char SHAVEPreemptionHandler::ID = 0;

INITIALIZE_PASS_BEGIN(SHAVEPreemptionHandler, "shavepreemptionhandlerpass", "SHAVE Preemption Handler Pass", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(SHAVEPreemptionHandler, "shavepreemptionhandlerpass", "SHAVE Preemption Handler Pass", false, false)

FunctionPass *llvm::createSHAVEPreemptionHandlerPass() {
  return new SHAVEPreemptionHandler();
}

MachineBasicBlock& SHAVEPreemptionHandler::insertPreemptionBlock(MachineBasicBlock &block,
                                                                 MachineBasicBlock * currentEpilogue,
                                                                 MachineBasicBlock * branchTarget) {
  MachineFunction &function = *(block.getParent());

  // Create a new epilogue and insert it at the end of the function so we don't upset any control-flow
  MachineBasicBlock * preemptionBlock = function.CreateMachineBasicBlock();
  function.insert(function.end(), preemptionBlock);

  preemptionBlock->addSuccessor(branchTarget, BranchProbability(99, 100));
  preemptionBlock->addSuccessor(currentEpilogue, BranchProbability(1, 100));

  return *preemptionBlock;
}

MachineBasicBlock& SHAVEPreemptionHandler::insertNewEpilogue(MachineBasicBlock &block,
                                                             MachineBasicBlock * branchTarget,
                                                             MachineBasicBlock * currentEpilogue,
                                                             MachineBasicBlock * preemptionBlock) {
#ifndef NDEBUG
  if (branchTarget) {
    DEBUG(dbgs() << "  Inserting new epilogue after BB#" << block.getNumber() << " with conditional branch to BB#" << branchTarget->getNumber() << "\n");
  }
  else {
    DEBUG(dbgs() << "  Inserting new epilogue after BB#" << block.getNumber() << " with fallthrough to BB#" << currentEpilogue->getNumber() << "\n");
  }
#endif // NDEBUG

  MachineFunction &function = *(block.getParent());

  // Create a new epilogue and insert it into the function after the loop
  MachineBasicBlock * newEpilogue = function.CreateMachineBasicBlock();
  function.insert(std::next(block.getIterator()), newEpilogue);

  // Update the successors of the loop
  block.replaceSuccessor(currentEpilogue, newEpilogue);
  
  // If there is an unconditional branch at the end of the loop to the epilogue, then we
  // need to move it to the new epilogue to maintain the original control-flow
  bool moveJMP = false;
  SmallVector<MachineInstr *, 3> moveInstructions;
  for (MachineInstr &instruction : block) {
    switch (instruction.getOpcode()) {
    case SHAVE::LSU_LDIL_Label:
      if (instruction.getOperand(1).getMBB() == currentEpilogue) {
        moveJMP = true;
        moveInstructions.push_back(&instruction);
      }
      break;
    case SHAVE::LSU_LDIH_Label:
      if (instruction.getOperand(2).getMBB() == currentEpilogue) {
        moveInstructions.push_back(&instruction);
      }
      break;
    case SHAVE::BRU_JMP:
      if (moveJMP) {
        moveInstructions.push_back(&instruction);
      }
      break;
    default: break;
    }
  }

  for (MachineInstr * instruction : moveInstructions) {
    instruction->removeFromParent();
    newEpilogue->insert(newEpilogue->end(), instruction);
  }

  // Add successors for the new epilogue, branches will be added later
  if (preemptionBlock)
    newEpilogue->addSuccessor(preemptionBlock, BranchProbability(1, 100));
  else if (branchTarget != nullptr)
    newEpilogue->addSuccessor(branchTarget, BranchProbability(1, 100));
  uint32_t probability = branchTarget == nullptr ? 100 : 99;
  newEpilogue->addSuccessor(currentEpilogue, BranchProbability(probability, 100));

  // Add live-ins to new epilogue
  for (const auto &livein : currentEpilogue->liveins())
    newEpilogue->addLiveIn(livein);

  return *newEpilogue;
}

void SHAVEPreemptionHandler::insertCMTIPredicate(MachineBasicBlock &block, MachineBasicBlock::instr_iterator insertPoint, DebugLoc &dbgLoc) {
  MachineInstrBuilder pc1c = BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::PEU_PC1C));
  pc1c.addImm(SHAVECC::NEQ);
  pc1c.addReg(SHAVE::CC_CMU0, RegState::Implicit);
  pc1c->bundleWithSucc();
}

void SHAVEPreemptionHandler::insertPreemptionCall(MachineBasicBlock &block, MachineBasicBlock::instr_iterator insertPoint, DebugLoc &dbgLoc) {
  // CMU.CPTI i0 ISR_SA
  MachineInstrBuilder copyHandlerAddress = BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::CMU_CPTI_preemption), SHAVE::I0);
  copyHandlerAddress.addReg(!SII->hasFeature(SHAVE::HasLegacyPreemptInstrs_Feature) ? SHAVE::B_ISR_SA : SHAVE::ISR_SA);
  SII->finaliseMI(copyHandlerAddress);

  // BRU.SWP i0
  MachineInstrBuilder call = BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::BRU_SWP_preemption));
  call.addReg(SHAVE::I0);
}

void SHAVEPreemptionHandler::insertLoopBoundsReevaluation(MachineBasicBlock &block,
                                                          MachineBasicBlock &insertBlock,
                                                          MachineBasicBlock::instr_iterator insertPoint,
                                                          MachineInstr &pseudoBranch,
                                                          MachineInstr * loopCompare) {
  MachineFunction &function = *(block.getParent());
  DebugLoc dbgLoc = block.rbegin()->getDebugLoc(); // Make this part of the branch on the loop back-edge

  MachineInstr * newCheck = nullptr;
  int64_t CC = SHAVECC::AL;
  if (loopCompare) {
    newCheck = function.CloneMachineInstr(loopCompare);
    insertBlock.insert(insertPoint, newCheck);

    // Set any registers used by the loop comparison as live into this block
    for (auto &operand : loopCompare->operands()) {
      if (operand.isReg() && operand.isUse()) {
        unsigned int reg = operand.getReg();
        if (reg != SHAVE::NoRegister) {
          insertBlock.addLiveIn(reg);

          // Remove "isKill" from any registers used by the loop comparison as they are now live-out of the loop
          if (operand.isKill())
            operand.setIsKill(false);
        }
      }
    }

    CC = pseudoBranch.getOperand(3).getImm();
  }
  else {
    unsigned int incsReg = pseudoBranch.getOperand(2).getReg();
    MachineInstrBuilder loopReevaluate = BuildMI(insertBlock, insertPoint, dbgLoc, SII->get(SHAVE::CMU_CMPI_Raw));
    loopReevaluate.addReg(incsReg);
    loopReevaluate.addImm(1);
    SII->finaliseMI(loopReevaluate);

    newCheck = loopReevaluate;

    insertBlock.addLiveIn(incsReg);

    CC = SHAVECC::LT;
  }

  // PEU.PC1C NEQ || BRU.BRA <loop>
  MachineBasicBlock * target = pseudoBranch.getOperand(1).getMBB();
  MachineInstrBuilder ldilJMP = BuildMI(insertBlock, insertPoint, dbgLoc, SII->get(SHAVE::LSU_LDIL_Label), SHAVE::I0);
  ldilJMP.addMBB(target);
  SII->finaliseMI(ldilJMP);

  MachineInstrBuilder ldihJMP = BuildMI(insertBlock, insertPoint, dbgLoc, SII->get(SHAVE::LSU_LDIH_Label), SHAVE::I0);
  ldihJMP.addReg(SHAVE::I0); // Tied to def
  ldihJMP.addMBB(target);
  SII->finaliseMI(ldihJMP);

  MachineInstrBuilder loopJMP = BuildMI(insertBlock, insertPoint, dbgLoc, SII->get(SHAVE::BRU_JMPcc));
  loopJMP.addReg(SHAVE::I0);
  SII->finaliseMI(loopJMP);

  MachineInstrBuilder pc1cJMP = BuildMI(insertBlock, *loopJMP, dbgLoc, SII->get(SHAVE::PEU_PC1C));
  pc1cJMP.addImm(CC);
  pc1cJMP.addReg(SHAVE::CC_CMU0, RegState::Implicit);
  pc1cJMP->bundleWithSucc();

  // Add all live-ins from the loop to this block. These registers must be kept alive in case
  // the branch back into the loop is taken
  for (const auto &livein : block.liveins())
    insertBlock.addLiveIn(livein);
}

void SHAVEPreemptionHandler::populatePreemptionBlock(MachineBasicBlock &preemptionBlock,
                                                     MachineBasicBlock &block,
                                                     MachineBasicBlock * preexistingEpilogue,
                                                     MachineInstr &pseudoBranch,
                                                     MachineInstr * loopCompare) {
  //
  // Preemption block:
  //   CMU.CPTI i0 ISR_SA
  //   BRU.SWP i0
  //   NOP 6
  // 
  // Then:
  //   CMU.CMPI <reg> 2
  //   PEU.PC1C LT || BRU.BRA <loop>
  //   NOP 6
  // 
  // Or if loopCompare != nullptr:
  //   <copy of loopCompare>
  //   PEU.PC1C <Loop CC> || BRU.BRA <loop>
  //   NOP 6
  // 
  // And finally:
  //   BRU.BRA <preexistingEpilogue>
  //   NOP 6
  //

  DebugLoc dbgLoc = block.rbegin()->getDebugLoc(); // Make this part of the branch on the loop back-edge
  auto insertPoint = preemptionBlock.instr_begin();

  // CMU.CPTI i0 ISR_SA
  // BRU.SWP i0
  insertPreemptionCall(preemptionBlock, insertPoint, dbgLoc);

  // Re-evaluate the loop bounds check and predicate branch back in
  insertLoopBoundsReevaluation(block, preemptionBlock, insertPoint, pseudoBranch, loopCompare);

  // BRU.BRA <existingEpilogue>
  MachineInstrBuilder ldilJMP2 = BuildMI(preemptionBlock, insertPoint, dbgLoc, SII->get(SHAVE::LSU_LDIL_Label), SHAVE::I0);
  ldilJMP2.addMBB(preexistingEpilogue);
  SII->finaliseMI(ldilJMP2);

  MachineInstrBuilder ldihJMP2 = BuildMI(preemptionBlock, insertPoint, dbgLoc, SII->get(SHAVE::LSU_LDIH_Label), SHAVE::I0);
  ldihJMP2.addReg(SHAVE::I0); // Tied to def
  ldihJMP2.addMBB(preexistingEpilogue);
  SII->finaliseMI(ldihJMP2);

  MachineInstrBuilder loopJMP2 = BuildMI(preemptionBlock, insertPoint, dbgLoc, SII->get(SHAVE::BRU_JMPcc));
  loopJMP2.addReg(SHAVE::I0);
  SII->finaliseMI(loopJMP2);
}

void SHAVEPreemptionHandler::populateNewEpilogue(MachineBasicBlock &epilogue, MachineBasicBlock &block, MachineInstr &pseudoBranch, MachineInstr * loopCompare) {
  //
  // New epilogue block:
  //   CMU.CMTI <expected-value> P_GPR
  //   CMU.CPTI i0 ISR_SA
  //   PEU.PC1C NEQ || BRU.SWP i0
  //   NOP 6
  // 
  // Then:
  //   CMU.CMPI <reg> 2
  //   PEU.PC1C LT || BRU.BRA <loop>
  //   NOP 6
  // 
  // Or if loopCompare != nullptr:
  //   <copy of loopCompare>
  //   PEU.PC1C <Loop CC> || BRU.BRA <loop>
  //   NOP 6
  //

  DebugLoc dbgLoc = block.rbegin()->getDebugLoc(); // Make this part of the branch on the loop back-edge
  auto insertPoint = epilogue.instr_begin();

  // CMU.CMTI <expected-value> P_GPR
  insertCMTI(epilogue, insertPoint, dbgLoc);

  // CMU.CPTI i0 ISR_SA
  // BRU.SWP i0
  insertPreemptionCall(epilogue, insertPoint, dbgLoc);

  // PEU.PC1C NEQ (bundled with BRU.SWP)
  insertCMTIPredicate(epilogue, std::prev(insertPoint), dbgLoc);

  // Re-evaluate the loop bounds check and predicate branch back in
  insertLoopBoundsReevaluation(block, epilogue, insertPoint, pseudoBranch, loopCompare);
}

void SHAVEPreemptionHandler::populateNewEpilogue(MachineBasicBlock &epilogue, MachineBasicBlock &block, MachineBasicBlock * preemptionBlock) {
  //
  // New epilogue block:
  //   CMU.CMTI <expected-value> P_GPR
  //   PEU.PC1C EQ || BRU.BRA <preemptionBlock>
  //   NOP 6
  //

  DebugLoc dbgLoc = block.rbegin()->getDebugLoc(); // Make this part of the branch on the loop back-edge
  auto insertPoint = epilogue.instr_begin();

  // CMU.CMTI <expected-value> P_GPR
  insertCMTI(epilogue, insertPoint, dbgLoc);

  // PEU.PC1C NEQ || BRU.BRA <preemptionBlock>
  MachineInstrBuilder ldilJMP = BuildMI(epilogue, insertPoint, dbgLoc, SII->get(SHAVE::LSU_LDIL_Label), SHAVE::I0);
  ldilJMP.addMBB(preemptionBlock);
  SII->finaliseMI(ldilJMP);

  MachineInstrBuilder ldihJMP = BuildMI(epilogue, insertPoint, dbgLoc, SII->get(SHAVE::LSU_LDIH_Label), SHAVE::I0);
  ldihJMP.addReg(SHAVE::I0); // Tied to def
  ldihJMP.addMBB(preemptionBlock);
  SII->finaliseMI(ldihJMP);

  MachineInstrBuilder loopJMP = BuildMI(epilogue, insertPoint, dbgLoc, SII->get(SHAVE::BRU_JMPcc));
  loopJMP.addReg(SHAVE::I0);
  SII->finaliseMI(loopJMP);

  insertCMTIPredicate(epilogue, loopJMP->getIterator(), dbgLoc);
}

void SHAVEPreemptionHandler::insertOptimisedInterruptCheck(MachineInstr &instruction) {
  DEBUG(dbgs() << "  Using generic interrupt check in loop tail\n");

  // Insert for preemption checking:
  //   <loop bounds check>
  //   PEU.ANDACC || CMU.CMTI.C16 <expected> P_GPR
  //   PEU.PC1C <CC> || <loop branch>
  //

  MachineBasicBlock &block = *(instruction.getParent());
  auto insertPoint = instruction.getIterator();
  while (insertPoint->isBundledWithPred())
    --insertPoint;
  DebugLoc dbgLoc = instruction.getDebugLoc();

  SHAVECC::CondCode CC = (SHAVECC::CondCode) instruction.getOperand(3).getImm();

  if (CC == SHAVECC::NEQ || CC == SHAVECC::LT || CC == SHAVECC::GT) {
    // LSU.LDIL i0 1
    // CMU.CMTI.BITN i0 P_GPR
    insertCMTI_BITN(block, insertPoint, dbgLoc);
  }
  else {
    // CMU.CMTI.C16 i0 P_GPR
    insertCMTI(block, insertPoint, dbgLoc);
  }

  // PEU.ANDACC
  MachineInstrBuilder ANDACC = BuildMI(block, std::prev(insertPoint), dbgLoc, SII->get(SHAVE::PEU_ANDACC));
  SII->finaliseMI(ANDACC);
  ANDACC->bundleWithSucc();

  // Replace JMPcc pseudo instruction with actual BRU.JMP
  replaceWithNoInterrupt(instruction);
}

void SHAVEPreemptionHandler::insertGenericInterruptCheck(MachineInstr &instruction) {
  DEBUG(dbgs() << "  Using generic interrupt check in loop tail\n");

  // Insert for preemption checking:
  //   <loop bounds check>
  //   PEU.PC1C <Opposite CC> || IAU.INCS <reg> 2
  //   CMU.CMTI <reg> P_GPR
  //   PEU.PC1C NEQ || IAU.INCS <reg> 1
  //   CMU.CMZ <reg>
  //   PEU.PC1C EQ || <loop branch>
  //
  // <reg> is initialised to 0 before entry to the loop

  MachineBasicBlock &block = *(instruction.getParent());
  DebugLoc dbgLoc = instruction.getDebugLoc();

  unsigned int incsReg = instruction.getOperand(2).getReg();
  SHAVECC::CondCode conditionCode = (SHAVECC::CondCode) instruction.getOperand(3).getImm();

  auto insertPoint = instruction.getIterator();
  while (insertPoint->isBundledWithPred())
    --insertPoint;

  // PEU.PC1C <CC> || IAU.INCS <reg> <reg> 2
  MachineInstrBuilder firstINCS = BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::IAU_INCS_i32), incsReg);
  firstINCS.addReg(incsReg);
  firstINCS.addImm(2);
  SII->finaliseMI(firstINCS);

  MachineInstrBuilder firstPC1C = BuildMI(block, firstINCS->getIterator(), dbgLoc, SII->get(SHAVE::PEU_PC1C));
  firstPC1C.addImm(SHAVECC::getOppositeCondition(conditionCode));
  firstPC1C.addReg(SHAVE::CC_CMU0, RegState::Implicit);

  firstPC1C->bundleWithSucc();

  // CMU.CMTI <expected> P_GPR
  insertCMTI(block, insertPoint, dbgLoc);

  // PEU.PC1C NEQ || IAU.INCS <reg> <reg> 1
  MachineInstrBuilder secondPC1C = BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::PEU_PC1C));
  secondPC1C.addImm(SHAVECC::NEQ);
  secondPC1C.addReg(SHAVE::CC_CMU0, RegState::Implicit);

  MachineInstrBuilder secondINCS = BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::IAU_INCS_i32), incsReg);
  secondINCS.addReg(incsReg);
  secondINCS.addImm(1);
  SII->finaliseMI(secondINCS);

  secondINCS->bundleWithPred();

  // CMU.CMZ <expected>
  MachineInstrBuilder cmz = BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::CMU_CMZ_i32));
  cmz.addReg(incsReg);
  SII->finaliseMI(cmz);

  // Change PEU.PC1C predicate to NEQ to match new CMU.CMZ
  MachineInstr &predicate = *insertPoint;
  predicate.getOperand(0).setImm(SHAVECC::EQ);

  // Replace JMPcc pseudo instruction with actual BRU.JMP
  replaceWithNoInterrupt(instruction);
}

MachineInstr * SHAVEPreemptionHandler::canOptimise(MachineInstr &instruction) {
  SHAVECC::CondCode CC = (SHAVECC::CondCode) instruction.getOperand(3).getImm();
  // CMU.CMTI.BITN will set LT and GT bits when the loop is to continue executing, CMU.CMTI.C16 can do EQ
  if (CC != SHAVECC::EQ && CC != SHAVECC::NEQ && CC != SHAVECC::LT && CC != SHAVECC::GT)
    return nullptr;

  MachineBasicBlock &block = *(instruction.getParent());
  MachineInstr * loopCompare = nullptr;

  // First find the comparison for this conditional branch instruction
  for (auto it = instruction.getIterator()->getReverseIterator(); it != block.rend(); ++it) {
    if (it->definesRegister(SHAVE::CC_CMU0)) {
      loopCompare = &*it;
      break;
    }
  }

  if (loopCompare == nullptr)
    return nullptr;

  // Then trace forwards again, ensuring that all registers used by this comparison are not re-defined
  // before exiting the block. If they are, we cannot safely insert the preemption epilogue as it requires
  // the original comparison values to be live on exit from the loop
  for (MachineOperand &operand : loopCompare->operands()) {
    if (operand.isReg() && operand.isUse()) {
      unsigned int reg = operand.getReg();
      for (auto it = std::next(loopCompare->getIterator()); it != instruction.getIterator(); ++it) {
        if (it->findRegisterDefOperandIdx(reg, false, true, SRI) != -1)
          return nullptr;
      }
    }
  }

  return loopCompare;
}

void SHAVEPreemptionHandler::fallthroughInterrupt(MachineBasicBlock &block, MachineBasicBlock * fallthroughTarget) {
  // Insert preemption checks when the loop latch falls through to the loop header
  // There is no "optimised" version of this, the extra checks are always inserted inside the loop

  DebugLoc dbgLoc = block.rbegin()->getDebugLoc();
  MachineBasicBlock &epilogue = insertNewEpilogue(block, nullptr, fallthroughTarget, nullptr);
  auto insertPoint = epilogue.instr_begin();

  // CMU.CMTI <expected-value> P_GPR
  insertCMTI(epilogue, insertPoint, dbgLoc);

  // CMU.CPTI i0 ISR_SA
  // BRU.SWP i0
  insertPreemptionCall(epilogue, insertPoint, dbgLoc);

  // PEU.PC1C NEQ (bundled with BRU.SWP)
  insertCMTIPredicate(epilogue, std::prev(insertPoint), dbgLoc);
}

void SHAVEPreemptionHandler::replaceWithInterrupt(MachineInstr &instruction) {
  DEBUG(dbgs() << "Replacing pseudo branch in BB#" << instruction.getParent()->getNumber() << " with interrupt\n");

  MachineInstr * loopCompare = canOptimise(instruction);

  // Insert a new epilogue block to confirm interrupt or continue execution
  MachineBasicBlock &block = *(instruction.getParent());
  
  MachineBasicBlock * branchTarget = instruction.getOperand(1).getMBB();
  MachineBasicBlock * currentEpilogue = nullptr;
  for (MachineBasicBlock * successor : block.successors())
    if (successor != branchTarget)
      currentEpilogue = successor;

  assert(currentEpilogue != nullptr && "Failed to find epilogue for loop");

  if (SHAVEOptions::EnableLowImpactPreemption) {
    MachineBasicBlock &preemptionBlock = insertPreemptionBlock(block, currentEpilogue, branchTarget);

    MachineBasicBlock &epilogue = insertNewEpilogue(block, branchTarget, currentEpilogue, &preemptionBlock);

    populateNewEpilogue(epilogue, block, &preemptionBlock);

    populatePreemptionBlock(preemptionBlock, block, currentEpilogue, instruction, loopCompare);
  }
  else {
    MachineBasicBlock &epilogue = insertNewEpilogue(block, branchTarget, currentEpilogue, nullptr);

    populateNewEpilogue(epilogue, block, instruction, loopCompare);
  }

  // Insert check for interrupts in loop body and replace pseudo with real CMU.CM[II|Z] instruction
  if (loopCompare != nullptr) {
    insertOptimisedInterruptCheck(instruction);
    ++OptimisedLoops;
  }
  else {
    insertGenericInterruptCheck(instruction);
    ++GenericLoops;
  }
}

void SHAVEPreemptionHandler::replaceWithNoInterrupt(MachineInstr &instruction) {
  DEBUG(dbgs() << "Replacing pseudo branch in BB#" << instruction.getParent()->getNumber() << " without interrupt\n");

  MachineBasicBlock &block = *(instruction.getParent());
  DebugLoc dbgLoc = instruction.getDebugLoc();

  auto insertPoint = instruction.getIterator();
  while (insertPoint->isBundledWithPred())
    --insertPoint;

  // LSU.LDIL i0 <label>
  MachineBasicBlock *branchTarget = instruction.getOperand(1).getMBB();
  MachineInstrBuilder ldilJMP = BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::LSU_LDIL_Label), SHAVE::I0);
  ldilJMP.addMBB(branchTarget);
  SII->finaliseMI(ldilJMP);

  // LSU.LDIH i0 <label>
  MachineInstrBuilder ldihJMP = BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::LSU_LDIH_Label), SHAVE::I0);
  ldihJMP.addReg(SHAVE::I0); // Tied to def
  ldihJMP.addMBB(branchTarget);
  SII->finaliseMI(ldihJMP);
  
  // BRU.JMP i0 || <existing PEU predicate>
  MachineInstrBuilder loopJMP = BuildMI(block, instruction, dbgLoc, SII->get(SHAVE::BRU_JMPcc));
  loopJMP.addReg(SHAVE::I0);
  SII->finaliseMI(loopJMP);

  instruction.eraseFromBundle();
}

void SHAVEPreemptionHandler::insertCMTI_BITN(MachineBasicBlock &block, MachineBasicBlock::instr_iterator insertPoint, DebugLoc &debugLoc) {
  unsigned int tempReg = SHAVE::I0;
  
  MachineInstrBuilder ldil = BuildMI(block, insertPoint, debugLoc, SII->get(SHAVE::LSU_LDIL), tempReg);
  ldil.addImm(1);
  SII->finaliseMI(ldil);

  MachineInstrBuilder compareTRF = BuildMI(block, insertPoint, debugLoc, SII->get(SHAVE::CMU_CMTI_BITN_preemption));
  compareTRF.addReg(tempReg);
  compareTRF.addReg(SHAVE::P_GPR);
  SII->finaliseMI(compareTRF);
}

void SHAVEPreemptionHandler::insertCMTI(MachineBasicBlock &block, MachineBasicBlock::instr_iterator insertPoint, DebugLoc &debugLoc) {
  unsigned int tempReg = SHAVE::I0;

  if (!SII->hasFeature(SHAVE::HasLegacyPreemptInstrs_Feature)) {
    MachineInstrBuilder ldil = BuildMI(block, insertPoint, debugLoc, SII->get(SHAVE::LSU_LDIL), tempReg);
    ldil.addImm(1);
    SII->finaliseMI(ldil);

    MachineInstrBuilder compareTRF = BuildMI(block, insertPoint, debugLoc, SII->get(SHAVE::CMU_CMTI_BITP_preemption));
    compareTRF.addReg(tempReg);
    compareTRF.addReg(SHAVE::P_GPR);
    SII->finaliseMI(compareTRF);
  }
  else {
    MachineInstrBuilder compareTRF = BuildMI(block, insertPoint, debugLoc, SII->get(SHAVE::CMU_CMTI_C16_preemption));
    compareTRF.addReg(tempReg);
    compareTRF.addReg(SHAVE::P_GPR);
    SII->finaliseMI(compareTRF);
  }
}

void SHAVEPreemptionHandler::insertNoRestoreInterrupt(MachineBasicBlock &block) {
  DEBUG(dbgs() << "Inserting \"no restore\" preemption check into BB#" << block.getNumber() << "\n");

  MachineBasicBlock::instr_iterator insertPoint = block.instr_begin();
  DebugLoc debugLoc = insertPoint == block.instr_end() ? DebugLoc() : insertPoint->getDebugLoc();

  // Compare TRF register used for signalling interrupts with 0
  insertCMTI(block, insertPoint, debugLoc);

  // PEU.PC1C NEQ || BRU.SWIH
  MachineInstrBuilder swih = BuildMI(block, insertPoint, debugLoc, SII->get(SHAVE::BRU_SWIH_imm_preemption));
  swih.addImm(0x4);
  swih.addImm(SHAVECC::NEQ);
  swih.addReg(SHAVE::CC_CMU0); // Predicate on bit-0

  MachineInstrBuilder pc1cCall = BuildMI(block, swih.getInstr(), debugLoc, SII->get(SHAVE::PEU_PC1C));
  pc1cCall.addImm(SHAVECC::NEQ);
  pc1cCall.addReg(SHAVE::CC_CMU0, RegState::Implicit);
  pc1cCall->bundleWithSucc();
}

int SHAVEPreemptionHandler::countInnerLoops(const MachineLoop * loopInfo, int maxDepth) const {
  int depth = 0;

  for (const auto &info : loopInfo->getSubLoops()) {
    depth = std::max(depth, countInnerLoops(info, maxDepth));
    // Exit early if we've reached the max depth
    if (depth > maxDepth)
      return depth+1;
  }

  return depth + 1;
}

bool SHAVEPreemptionHandler::runOnBlock(MachineBasicBlock &block) {
  if (SHAVEOptions::EnablePreemption == SHAVEOptions::Preemption::Restore) {
    int maxDepth = SHAVEOptions::PreemptionMaxLoopDepth;

    for (auto it = block.instr_rbegin(); it != block.instr_rend(); ++it) {
      MachineInstr &instruction = *it;

      // Exit early, there should be no pseudo JMP instructions past a BRU_JMPcc
      if (instruction.getOpcode() == SHAVE::BRU_JMPcc)
        break;

      if (instruction.getOpcode() != SHAVE::JMPcc_TO_LABEL_PREEMPTION)
        continue;

      const MachineLoop * loopInfo = machineLoopInfo->getLoopFor(&block);
      MachineBasicBlock * target = instruction.getOperand(1).getMBB();

      bool isLoopLatch = false;
      bool isHeader = false;
      if (loopInfo) {
        isLoopLatch = loopInfo->contains(target) && loopInfo->isLoopLatch(target);
        isHeader = loopInfo->getHeader() == target;
      }

      if (isLoopLatch || isHeader)
        ++NumberOfLoopBackEdges;

      // If this block belongs to a loop and the target of the branch is the loop header
      if ((isLoopLatch || isHeader) && (maxDepth == -1 || maxDepth >= (int) countInnerLoops(loopInfo, maxDepth))) {
        replaceWithInterrupt(instruction);
      }
      else {
        replaceWithNoInterrupt(instruction);

        if (SHAVEOptions::PreemptionDisableFallthroughChecks)
          return true;

        if (!block.canFallThrough())
          return true;

        const MachineLoop * loopInfo = machineLoopInfo->getLoopFor(&block);
        if (loopInfo == nullptr)
          return true;

        // Special case, the loop header is actually at the end of the loop and
        // the latch is in the middle with a predicated branch to the exit and
        // fall-through to the header
        //    +-- Entry
        //    |
        //    |   Body <--+
        //    |     |     |
        //    |     V     |
        //    |   Latch - | --+
        //    |     |     |   |
        //    |     V     |   |
        //    +-> Header -+   |
        //          |         |
        //          V         |
        //        Exit <------+
        // This case fails because there is no actual branch from the latch to the header
        // DXIL produces this pattern, so we must account for it here
        if (loopInfo->isLoopLatch(&block) && block.getFallThrough() == loopInfo->getHeader() &&
            (maxDepth == -1 || maxDepth >= (int) countInnerLoops(loopInfo, maxDepth)))
          fallthroughInterrupt(block, block.getFallThrough());
      }

      return true;
    }
  }
  else {
    const MachineLoop * loopInfo = machineLoopInfo->getLoopFor(&block);
    bool isLoopLatch = false;
    bool isHeader = false;
    if (loopInfo) {
      isLoopLatch = loopInfo->contains(&block) && loopInfo->isLoopLatch(&block);
      isHeader = loopInfo->getHeader() == &block;
    }

    if (block.pred_size() == 0) {
      // Entry-block
      insertNoRestoreInterrupt(block);
      return true;
    }
    else if (isHeader || isLoopLatch) {
      // Loop header/latch block
      insertNoRestoreInterrupt(block);
      return true;
    }
  }

  return false;
}

bool SHAVEPreemptionHandler::runOnMachineFunction(MachineFunction &MF) {
  DEBUG(dbgs() << "SHAVEPreemptionHandler: Running SHAVE Preemption Handler Pass on function " << MF.getName() << "\n");

  SII = MF.getSubtarget<SHAVESubtarget>().getInstrInfo();

  // Nothing to do if target has Code Protection support, this is handled by SHAVEPostSchedulingPreemption
  if (SII->hasFeature(SHAVE::HasCodeProtection_Feature))
    return false;

  SRI = SII->getSHAVERegisterInfo();
  machineLoopInfo = &getAnalysis<MachineLoopInfo>();

  bool modified = false;

  for (MachineBasicBlock &block : MF) {
    modified |= runOnBlock(block);
  }

  DEBUG(dbgs() << "SHAVEPreemptionHandler: Completed SHAVE Preemption Handler Pass on function " << MF.getName() << "\n");

  return modified;
}

void SHAVEPreemptionHandler::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<MachineLoopInfo>();
  MachineFunctionPass::getAnalysisUsage(AU);
}
