//===-- SHAVEAddressSimplifier.h - Address Simplifier Pass ------*- C++ -*-===//
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

#ifndef SHAVEADDRESSSIMPLIFIER_H
#define SHAVEADDRESSSIMPLIFIER_H (1)

#include "llvm/CodeGen/MachineFunctionPass.h"

#include "SHAVE.h"
#include "SHAVEInstrInfo.h"
#include "SHAVERegisterInfo.h"
#include "SHAVESubtarget.h"

#include <set>

using namespace llvm;

class SHAVEAddressSimplifier : public llvm::MachineFunctionPass {
private:
  //   loop:
  //     %loopIncrementRegister = PHI %updatedIncrementRegister, loop, ...                ; addressPHI
  //     %addressReg = IAU_ADD_32 %base, %loopIncrementRegister                           ; addressComputation
  //     ...
  //     %updatedIncrementRegister = IAU_ADD_32 killed %loopIncrementRegister, %incAmount ; addressIncrement
  struct TransformData {
    unsigned int updatedIncrementRegister = SHAVE::NoRegister;
    unsigned int loopIncrementRegister = SHAVE::NoRegister;
    unsigned int addressRegister = SHAVE::NoRegister;
    MachineInstr *addressPHI = nullptr;
    MachineInstr *addressComputation = nullptr;
    MachineInstr *addressIncrement = nullptr;
    std::vector<std::pair<MachineBasicBlock *, unsigned int>> newPointerRegisters;
  };

  const SHAVEInstrInfo * SII = nullptr;
  MachineFunction * currentFunction = nullptr;

  std::map<unsigned int, std::pair<MachineInstr *, unsigned int>> availableAdds; // Targeted add instructions, with the register for the loop variant operand
  std::set<unsigned int> loopVariantRegisters;

  void generateBlockInfo(MachineBasicBlock &block);
  bool isLoopInvariant(unsigned int reg) const;
  bool isTargetedAdd(MachineInstr &instruction, unsigned int &defRegister, unsigned int &incrementRegister) const;
  bool findAddressComputation(MachineInstr &instruction, TransformData &transformData) const;
  void cloneAddressComputation(MachineBasicBlock &block, TransformData &transformData);
  void fixRegisters(MachineBasicBlock &block, TransformData &transformData);
  bool modifyOffsetAddresses();

public:
  SHAVEAddressSimplifier() : MachineFunctionPass(ID) {
    initializeSHAVEAddressSimplifierPass(*llvm::PassRegistry::getPassRegistry());
  }

  llvm::StringRef getPassName() const override {
    return "SHAVE Address Simplifier Pass";
  }

  bool runOnMachineFunction(llvm::MachineFunction &MF) override;

public:
  static char ID;
};


#endif // SHAVEADDRESSSIMPLIFIER_H
