//===-- SHAVEUtilities.cpp - Miscellaneous SHAVE Utilties ------*- C++ -*--===//
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

#include <SHAVEUtilities.h>

#include "llvm/CodeGen/MachineFrameInfo.h"

#include <SHAVE.h>
#include "SHAVETargetMachine.h"

using namespace llvm;
using namespace SHAVE;

unsigned int SHAVEUtilities::getPointerOperandIndex(MachineInstr &instr) const {
  switch (instr.getOpcode()) {
  default:
    return 1;
  case SHAVE::LSU_LDOV128_h:
    return 2;
  }
}

bool SHAVEUtilities::memoryOperandUsesStack(MachineInstr &instr,
                                            unsigned int pointerOperandIndex,
                                            unsigned int &stackOffset) const {
  MachineOperand &pointerOperand = instr.getOperand(pointerOperandIndex);
  bool found = false;
  stackOffset = 0;

  if (SHAVEConflicts::check_isLDXVorSTXV(instr.getOpcode()) || SHAVEConflicts::check_isLDXorSTX(instr.getOpcode())) {
    // FIXME: Movidius - It should be possible to work backwards through the block to find an LDIL
    //                   which loads the offset for and LDX/STX accessing the stack. This can happen
    //                   if the offset is outside the range allowed for the equivalent LDO/STO
    return false;
  }

  if (pointerOperand.isReg() && (pointerOperand.getReg() == SHAVERegisterInfo::getSPReg())) {
    found = true;
  } else if (pointerOperand.isFI()) {
    const MachineFrameInfo &MFI = instr.getParent()->getParent()->getFrameInfo();
    stackOffset = MFI.getObjectOffset(pointerOperand.getIndex());
    found = true;
  }

  int immOffset = -1;

  if (found) {
    bool foundOffset = SII->findLoadStoreOffset(&instr, &immOffset);
    if (foundOffset && immOffset >= 0)
      stackOffset += (unsigned int) immOffset;
  }

  return found;
}

MachineMemOperand *SHAVEUtilities::extractMemoryOperand(MachineInstr &instr,
                                                        bool &usesStack,
                                                        unsigned int &stackOffset) const {
  unsigned int pointerOperandIndex = getPointerOperandIndex(instr);

  MachineMemOperand *memOperand = nullptr;
  if (instr.hasOneMemOperand())
    memOperand = *instr.memoperands_begin();

  usesStack = memoryOperandUsesStack(instr, pointerOperandIndex, stackOffset);

  if (!usesStack && memOperand && memOperand->getValue()) {
    // Second chance to detect stack operations.
    if (const PseudoSourceValue * PSV = (*instr.memoperands_begin())->getPseudoValue()) {
      if (isa<FixedStackPseudoSourceValue>(PSV))
        usesStack = true;
      else
        usesStack = PSV->isConstant(nullptr) || PSV->isStack();
    }
  }

  return memOperand;
}

bool SHAVEUtilities::isReadOnlyAccess(MachineInstr &instr) {
  if (useCaches) {
    auto cached = readOnlyCache.find(&instr);
    if (cached != readOnlyCache.end())
      return cached->second;
  }

  auto result = [this, &instr](bool value) {
    if (useCaches)
      readOnlyCache[&instr] = value;
    return value;
  };

  if (instr.mayStore())
    return result(false);

  // Check for constant-pools, which are always read-only on SHAVE
  if (instr.hasOneMemOperand()) {
    const auto * memOperand = *instr.memoperands_begin();
    const auto * pseudoValue = memOperand->getPseudoValue();
    if (pseudoValue && pseudoValue->kind() == PseudoSourceValue::ConstantPool)
      return result(true);
  }

  unsigned int pointerOperandIndex = getPointerOperandIndex(instr);
  MachineOperand &pointer = instr.getOperand(pointerOperandIndex);

  if (!pointer.isReg())
    return result(false);

  MachineBasicBlock::instr_iterator it = instr.getIterator();

  // Find the first instruction which defines the pointer register
  while (it != instr.getParent()->instr_begin()) {
    --it;

    if (it->definesRegister(pointer.getReg())) {
      if (it->getOpcode() == SHAVE::LSU_LDIHSym) {
        // If the LDIHSym is predicated then it might not execute before this load
        // so we must assume it won't and its value is not trustworthy
        if (SII->isPredicated(*it))
          return false;

        MachineOperand &location = it->getOperand(2);

        if (location.isCPI()) {
          return result(true);
        }
        else if (location.isGlobal()) {
          const GlobalVariable *global = dyn_cast<const GlobalVariable>(location.getGlobal());

          if (global != nullptr && global->isConstant())
            return result(true);
        }
      }

      break;
    }
  }

  return result(false);
}

bool SHAVEUtilities::doMemoryRangesOverlap(int offset1,
                                           int offset2,
                                           int size1,
                                           int size2) const {
  int end1 = offset1 + size1;
  int end2 = offset2 + size2;
  return ((offset2 >= offset1) && (offset2 < end1)) || ((offset1 >= offset2) && (offset1 < end2));
}

AliasResult SHAVEUtilities::compareMemoryAccesses(MachineInstr &instr1,
                                                  MachineInstr &instr2) {
  bool usesStack1 = false;
  bool usesStack2 = false;
  unsigned int offset1 = 0;
  unsigned int offset2 = 0;
  MachineMemOperand* memoryOperand1 = extractMemoryOperand(instr1, usesStack1, offset1);
  MachineMemOperand* memoryOperand2 = extractMemoryOperand(instr2, usesStack2, offset2);

  // If one or both of these instruction are loads from a read-only region of memory
  // then there cannot be any memory dependencies with store instructions
  if (isReadOnlyAccess(instr1) || isReadOnlyAccess(instr2))
    return AliasResult::NoAlias;

  // Instructions without memory operand may alias
  if (memoryOperand1 == nullptr || memoryOperand2 == nullptr)
    return AliasResult::MayAlias;

  // Check memory operations that use the stack
  if (usesStack1 && usesStack2) {
    if (offset1 == offset2)
      return AliasResult::MustAlias;
    else if (doMemoryRangesOverlap(offset1, offset2, memoryOperand1->getSize(), memoryOperand2->getSize()))
      return AliasResult::PartialAlias;
    else
      return AliasResult::NoAlias;
  }

  // If either of the memory references are empty, it doesn't matter what the
  // pointer values are
  if (memoryOperand1->getSize() == 0 || memoryOperand2->getSize() == 0)
    return AliasResult::NoAlias;

  // If either of the memory pointers are unknown, they may alias
  const Value* pointer1 = memoryOperand1->getValue();
  const Value* pointer2 = memoryOperand2->getValue();
  if (pointer1 == nullptr || pointer2 == nullptr)
    return AliasResult::MayAlias;

  // Compare memory pointer values
  pointer1 = pointer1->stripPointerCasts();
  pointer2 = pointer2->stripPointerCasts();

  if (pointer1 == pointer2) {
    // Look for intersections in the accessed memory range.
    if (memoryOperand1->getOffset() == memoryOperand2->getOffset())
      return AliasResult::MustAlias;
    else if (doMemoryRangesOverlap(memoryOperand1->getOffset(), memoryOperand2->getOffset(), memoryOperand1->getSize(), memoryOperand2->getSize()))
      return AliasResult::PartialAlias;
    else
      return AliasResult::NoAlias;
  }
  else if (AA) {
    int64_t MinOffset = std::min(memoryOperand1->getOffset(), memoryOperand2->getOffset());
    int64_t Overlap1 = memoryOperand1->getSize() + memoryOperand1->getOffset() - MinOffset;
    int64_t Overlap2 = memoryOperand2->getSize() + memoryOperand2->getOffset() - MinOffset;

    return AA->alias(
      MemoryLocation(memoryOperand1->getValue(), Overlap1, memoryOperand1->getAAInfo()),
      MemoryLocation(memoryOperand2->getValue(), Overlap2, memoryOperand2->getAAInfo()));
  }

  return AliasResult::MayAlias;
}
