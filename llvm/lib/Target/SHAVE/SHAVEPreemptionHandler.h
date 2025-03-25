//===-- SHAVEPreemptionHandler.h - Insert Preemptioono Handling -*- C++ -*-===//
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

#ifndef SHAVEPreemptionHandler_H
#define SHAVEPreemptionHandler_H (1)

#include "llvm/Pass.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

#include "SHAVETargetMachine.h"

namespace llvm {

  void initializeSHAVEPreemptionHandlerPass(PassRegistry&);

  class SHAVEPreemptionHandler : public MachineFunctionPass {
  private:
    const SHAVEInstrInfo * SII = nullptr;
    const SHAVERegisterInfo *SRI = nullptr;
    MachineLoopInfo *machineLoopInfo = nullptr;

    MachineBasicBlock& insertPreemptionBlock(MachineBasicBlock &block, MachineBasicBlock *currentEpilogue, MachineBasicBlock * branchTarget);
    MachineBasicBlock& insertNewEpilogue(MachineBasicBlock &block, MachineBasicBlock *currentEpilogue, MachineBasicBlock * branchTarget, MachineBasicBlock *preemptionBlock);
    void insertCMTIPredicate(MachineBasicBlock &block, MachineBasicBlock::instr_iterator insertPoint, DebugLoc &dbgLoc);
    void insertPreemptionCall(MachineBasicBlock &block, MachineBasicBlock::instr_iterator insertPoint, DebugLoc &dbgLoc);
    void insertLoopBoundsReevaluation(MachineBasicBlock &block, MachineBasicBlock &insertBlock, MachineBasicBlock::instr_iterator insertPoint, MachineInstr &pseudoBranch, MachineInstr * loopCompare);
    void populatePreemptionBlock(MachineBasicBlock &epilogue, MachineBasicBlock &block, MachineBasicBlock * preexistingEpilogue, MachineInstr &pseudoBranch, MachineInstr * loopCompare);
    void populateNewEpilogue(MachineBasicBlock &epilogue, MachineBasicBlock &block, MachineInstr &pseudoBranch, MachineInstr * loopCompare);
    void populateNewEpilogue(MachineBasicBlock &epilogue, MachineBasicBlock &block, MachineBasicBlock * preemptionBlock);
    void insertOptimisedInterruptCheck(MachineInstr &instruction);
    void insertGenericInterruptCheck(MachineInstr &instruction);
    MachineInstr * canOptimise(MachineInstr &instruction);
    void fallthroughInterrupt(MachineBasicBlock &block, MachineBasicBlock * fallthroughTarget);
    void replaceWithInterrupt(MachineInstr &instruction);
    void replaceWithNoInterrupt(MachineInstr &instruction);
    void insertCMTI_BITN(MachineBasicBlock &block, MachineBasicBlock::instr_iterator insertPoint, DebugLoc &debugLoc);
    void insertCMTI(MachineBasicBlock &block, MachineBasicBlock::instr_iterator insertPoint, DebugLoc &debugLoc);
    void insertNoRestoreInterrupt(MachineBasicBlock &block);
    int countInnerLoops(const MachineLoop * loopInfo, int maxDepth) const;
    bool runOnBlock(MachineBasicBlock &block);
  public:
    SHAVEPreemptionHandler() : MachineFunctionPass(ID) {
      initializeSHAVEPreemptionHandlerPass(*llvm::PassRegistry::getPassRegistry());
    }

    StringRef getPassName() const override { return "SHAVE Interrupt Handler Pass"; }
    bool runOnMachineFunction(MachineFunction &MF) override;
    void getAnalysisUsage(AnalysisUsage &AU) const override;

    static char ID;
  };
} // End namespace llvm


#endif // SHAVEPreemptionHandler_H
