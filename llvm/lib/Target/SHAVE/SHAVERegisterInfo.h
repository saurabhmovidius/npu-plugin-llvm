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
// File       :  SHAVERegisterInfo.h
// Description:  Header file
// ---------------------------------------------------------------------------

#ifndef SHAVE_REGISTERINFO_H
#define SHAVE_REGISTERINFO_H (1)


#include "SHAVE.h"
#include "MCTargetDesc/SHAVEMCTargetDesc.h"

#define GET_REGINFO_HEADER
#include "SHAVEGenRegisterInfo.inc"


namespace llvm {
  class SHAVESubtarget;
  class SHAVEInstrInfo;
  class Type;

  class SHAVERegisterInfo : public SHAVEGenRegisterInfo {
  private:
    struct SHAVEPhysicalRegInfo {
      const TargetRegisterClass * regClass = nullptr;
      std::array<unsigned int, 2> subregs = { 0, 0 } ;
    };
    bool getPhysicalRegisterData(unsigned int reg, SHAVEPhysicalRegInfo &info, bool includeTRF) const;

  public:
    enum RegisterFileTypes {
      IRF_RegFile,
      VRF_RegFile,

      Unknown_RegFile
    };

    const SHAVESubtarget &Subtarget;
    const SHAVEInstrInfo &TII;

    SHAVERegisterInfo(const SHAVESubtarget &subtarget, const SHAVEInstrInfo &tii);

    // Register file handling
    RegisterFileTypes getRegisterFileTypeForReg(unsigned regNo) const;
    bool isIRFRegister(unsigned regNo) const;
    bool isVRFRegister(unsigned regNo) const;

    // getPointerRegClass - Return the register class to use to hold pointers.
    // This is used for addressing modes.
    const TargetRegisterClass *getPointerRegClass(const MachineFunction &MF, unsigned Kind = 0) const override;

    // Return the array and classes of callee-saved registers
    const MCPhysReg* getCalleeSavedRegs(const MachineFunction *MF) const override;
    
    // Return an array describing which registers are preserved across a call.
    const uint32_t *getCallPreservedMask(const MachineFunction &MF, CallingConv::ID) const override;

    // Return the reserved registers
    BitVector getReservedRegs(const MachineFunction &MF) const override;

    // Convert frame indicies into machine operands
    bool eliminateFrameIndex(MachineBasicBlock::iterator II,
                             int SPAdj,
                             unsigned FIOperandNum,
                             RegScavenger *RS) const override;

    static unsigned getFPReg() { return SHAVE::I31; } // Frame-Pointer register
    static unsigned getSPReg() { return SHAVE::I19; } // Stack-Pointer register
    static unsigned getLinkReg() { return SHAVE::I30; } // Link-Register, aka return address
    static unsigned getScratchReg() { return SHAVE::I0; } // General purpose physical scratch register

    // Get the stack frame register (SP, aka I19)
    Register getFrameRegister(const MachineFunction &MF) const override;

    /// getRegPressureLimit - Return the register pressure "high water mark" for
    /// the specific register class. The scheduler is in high register pressure
    /// mode (for the specific register class) if it goes over the limit.
    ///
    /// Note: this is the old register pressure model that relies on a manually
    /// specified representative register class per value type.
    unsigned getRegPressureLimit(const TargetRegisterClass *RC,
                                 MachineFunction &MF) const override;

    // Get the register class required to stash member of RC
    const TargetRegisterClass *getVectorStashClass(const TargetRegisterClass *RC,
                                                   const MachineFunction &MF) const override;

    bool trackLivenessAfterRegAlloc(const MachineFunction &MF) const override;

    std::array<unsigned int, 2> getPhysicalSHAVERegisters(unsigned int reg, bool includeTRF = false) const;
    unsigned int getSubSHAVERegister(unsigned int reg, unsigned int correspondingSubReg) const;
  };
} // end namespace llvm


#endif // SHAVE_REGISTERINFO_H
