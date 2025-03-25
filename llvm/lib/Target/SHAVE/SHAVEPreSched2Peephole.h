//===-- SHAVEPreSched2Peephole.h - Pre-Sched2 Peephole Pass ---*- C++ -*-===//
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

#ifndef SHAVEPRESCHED2PEEPHOLE_H
#define SHAVEPRESCHED2PEEPHOLE_H (1)


#include "llvm/CodeGen/MachineFunctionPass.h"

#include "SHAVE.h"
#include "SHAVEInstrInfo.h"
#include "SHAVESubtarget.h"

using namespace llvm;

class SHAVEPreSched2Peephole : public llvm::MachineFunctionPass {
  const SHAVEInstrInfo *SII = nullptr;
  const SHAVERegisterInfo *SRI = nullptr;
  MachineFunction *currentFunction = nullptr;

  bool eliminateIdentityCopies();
  bool sanitizeMyr4LDOSTO();

public:
  SHAVEPreSched2Peephole() : MachineFunctionPass(ID) {
    initializeSHAVEPreSched2PeepholePass(*llvm::PassRegistry::getPassRegistry());
  }

  llvm::StringRef getPassName() const override {
    return "SHAVE Pre-Sched2 Peephole Pass";
  }

  bool runOnMachineFunction(llvm::MachineFunction &MF) override;

public:
  static char ID;
};


#endif // SHAVEPRESCHED2PEEPHOLE_H
