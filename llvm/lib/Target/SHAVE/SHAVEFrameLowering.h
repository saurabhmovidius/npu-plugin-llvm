// ***************************************************************************
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
// ---------------------------------------------------------------------------
// File       :  SHAVEFrameLowering.h
// Description:  Function frame information
// ---------------------------------------------------------------------------

#ifndef SHAVEFRAMELOWERING_H_
#define SHAVEFRAMELOWERING_H_ (1)

#include "llvm/CodeGen/TargetFrameLowering.h"

#include "SHAVEInstrInfo.h"
#include "SHAVERegisterInfo.h"


namespace llvm {
  class SHAVEFrameLowering : public TargetFrameLowering {
  private:
    const SHAVEInstrInfo &TII;
    const SHAVERegisterInfo *registerInfo;

  public:
    SHAVEFrameLowering(const SHAVEInstrInfo &tii, Align stackAl)
    : TargetFrameLowering(StackGrowsDown, stackAl, 0, stackAl), TII(tii) {
      registerInfo = tii.getSHAVERegisterInfo();
    }

    virtual ~SHAVEFrameLowering() {}

    void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
    void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

    // Functions used for stack_probes feature
    // enabled to check stack overflow for LEON processes
    void captureStackInfo(MachineFunction &MF,
                          MachineBasicBlock::iterator MBBI,
                          MachineBasicBlock &MBB,
                          const DebugLoc &dbgLoc) const;
    void checkStackOverflow(MachineFunction &MF,
                            MachineBasicBlock::iterator MBBI,
                            MachineBasicBlock &MBB,
                            const DebugLoc &dbgLoc,
                            SHAVEInstrInfo::FrameContext_t frameContext = SHAVEInstrInfo::inFunctionPrologue) const;
    void updateHighWaterMark(MachineFunction &MF,
                             MachineBasicBlock::iterator MBBI,
                             MachineBasicBlock &MBB,
                             const DebugLoc &dbgLoc,
                             SHAVEInstrInfo::FrameContext_t frameContext = SHAVEInstrInfo::inFunctionPrologue) const;

    // computeStackSize - returns the stack size of the machine functions
    // including storage for link register, old frame pointer, ...
    int64_t computeStackSize(MachineFunction &MF) const;

    bool IsValid32BitOffset(int64_t stackOffset) const;
    void saveRegOnFixedStack(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                             DebugLoc &dbgLoc, int64_t stackOffset, unsigned refReg, unsigned regToSave) const;
    void restoreRegFromFixedStack(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                                  DebugLoc &dbgLoc, int64_t stackOffset, unsigned refReg, unsigned regToRestore) const;

    bool hasFP(const MachineFunction &MF) const override;

    MachineBasicBlock::iterator eliminateCallFramePseudoInstr(MachineFunction &MF,
                                                              MachineBasicBlock &MBB,
                                                              MachineBasicBlock::iterator MI) const override;

    bool assignCalleeSavedSpillSlots(MachineFunction &MF, const TargetRegisterInfo *TRI,
                                     std::vector<CalleeSavedInfo> &CSI) const override;
    // FIXME: Movidius - there are several functions in the 'TargetFrameLowering' base that we should consider overriding
  };
} // end of namespace llvm

#endif // SHAVEFRAMELOWERING_H_
