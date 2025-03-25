//===-- SHAVEPredicator.h - Predication Transforms --------------*- C++ -*-===//
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
// Transforms the predicated instructions to VLIW predicated bundles
//
//===----------------------------------------------------------------------===//

#ifndef SHAVEPREDICATOR_H
#define SHAVEPREDICATOR_H (1)


#include "llvm/Pass.h"
#include "llvm/CodeGen/LiveInterval.h"
#include "llvm/CodeGen/LiveVariables.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

#include "SHAVETargetMachine.h"


namespace llvm {

  void initializeSHAVEPredicatorPass(PassRegistry&);

  class SHAVEPredicator : public MachineFunctionPass {
  public:
    SHAVEPredicator() : MachineFunctionPass(ID) {}

    StringRef getPassName() const override { return "SHAVE Predicator"; }
    bool runOnMachineFunction(MachineFunction &MF) override;

    static char ID;

  private:
    struct PredRegAndMask {
      unsigned reg;
      SHAVECC::CondCode mask;
    };

    bool runOnMBB(MachineBasicBlock &MBB);

    unsigned getPredOpcode(const SHAVEInstrInfo &TII, PredRegAndMask prm,
                           unsigned int predicatedOpcode, MachineInstr &I,
                           int &lane) const;
    PredRegAndMask getPredRegAndMask(const TargetInstrInfo &TII, MachineInstr &I);
  };

  void initializeSHAVELoopLoadHoisterPass(PassRegistry&);

  class SHAVELoopLoadHoister : public MachineFunctionPass {
  public:
    SHAVELoopLoadHoister() : MachineFunctionPass(ID) {}

    StringRef getPassName() const override { return "SHAVE loop load hoister pass"; }
    bool runOnMachineFunction(MachineFunction &MF) override;
    void getAnalysisUsage(AnalysisUsage &AU) const override;

    static char ID;

  private:
    bool runOnMBB(MachineBasicBlock &MBB);
  };

  class SHAVECopyAddCombining : public MachineFunctionPass
  {
  public:
    SHAVECopyAddCombining(): MachineFunctionPass(ID) {}

    StringRef getPassName() const override { return "SHAVE loop load hoister pass"; }
    bool runOnMachineFunction(MachineFunction &MF) override;

  private:
    bool runOnMBB(MachineBasicBlock &MBB);
    bool optimiseINCS(MachineInstr &MI /* MachineBasicBlock::iterator I*/);

    static char ID;

    // Maps a register to the copy instruction that defines it.
    // Only contains keys for registers that are currently live.
    std::map<unsigned, MachineInstr *> LiveCopyDefs;
    // Set of registers currently used by copy instructions.
    // Registers are removed from this set when they are overwritten.
    std::set<unsigned> LiveCopyUses;
    std::vector<MachineInstr *> DeleteList;

  };
} // End namespace llvm


#endif  // SHAVEPREDICATOR_H
