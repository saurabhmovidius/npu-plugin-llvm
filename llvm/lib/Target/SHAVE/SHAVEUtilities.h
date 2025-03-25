//===-- SHAVEUtilities.h - Miscellaneous SHAVE Utilties ------*- C++ -*----===//
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

#pragma once

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"

#include <SHAVEInstrInfo.h>

#include <unordered_map>

namespace llvm {
namespace SHAVE {
class SHAVEUtilities {
private:
  // Private member variables
  AliasAnalysis *AA = nullptr;
  const SHAVEInstrInfo *SII = nullptr;
  // The caches in SHAVEUtilities use the MachineInstr pointers as key values
  // The caches will only remain valid as long no instructions are deleted or created
  // within the scope of the SHAVEUtilities instance. LLVM has a recycler for MachineInstr
  // instances to avoid allocations which can lead to a cache hit for an instruction
  // which is not valid
  // Set useCaches to true only if these requirements are met
  bool useCaches = false;
  std::unordered_map<MachineInstr *, bool> readOnlyCache; // Cache for results of isReadOnly

  // Private member functions
  unsigned int getPointerOperandIndex(MachineInstr &instr) const;
  bool memoryOperandUsesStack(MachineInstr &instr,
                              unsigned int pointerOperandIndex,
                              unsigned int &stackOffset) const;
  MachineMemOperand *extractMemoryOperand(MachineInstr &instr,
                                          bool &usesStack,
                                          unsigned int &stackOffset) const;
  bool isReadOnlyAccess(MachineInstr &instr);
  bool doMemoryRangesOverlap(int offset1,
                             int offset2,
                             int size1,
                             int size2) const;

public:
  SHAVEUtilities(AliasAnalysis *AA,
                 const SHAVEInstrInfo *SII,
                 bool useCaches)
    : AA(AA),
      SII(SII),
      useCaches(useCaches) {}

  AliasResult compareMemoryAccesses(MachineInstr &instr1,
                                    MachineInstr &instr2);
};
}
}
