//===-- SHAVEPreRASchedPeephole.h - Pre-RASched Peephole Pass ---*- C++ -*-===//
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

#ifndef SHAVEPRERASCHEDPEEPHOLE_H
#define SHAVEPRERASCHEDPEEPHOLE_H (1)


#include "llvm/CodeGen/MachineFunctionPass.h"

#include "SHAVE.h"
#include "SHAVEInstrInfo.h"
#include "SHAVETargetMachine.h"

using namespace llvm;

class SHAVEPreRASchedPeephole : public MachineFunctionPass {
private:
  struct ShuffleInfo { 
    unsigned int splatOpcode, extractOpcode;
    bool is32bit;
  };

  MachineFunction * currentFunction = nullptr;
  const SHAVEInstrInfo * SII = nullptr;
  LiveIntervals * LIS = nullptr;

  void setupShuffle(ShuffleInfo shuffleInfo, MachineInstrBuilder &shuffle,
                    int64_t swizzleLan);
  bool replaceShuffles();
  bool expandACCMACPseudos();

public:
  SHAVEPreRASchedPeephole() : MachineFunctionPass(ID) {
    initializeSHAVEPreRASchedPeepholePass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "SHAVE Pre-RA Scheduler Peephole Pass";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool runOnMachineFunction(MachineFunction &MF) override;

public:
  static char ID;
};


#endif // SHAVEPRERASCHEDPEEPHOLE_H
