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
// File       :  SHAVEMachineFunctionInfo.h
// Description:  Store information specific to machine function
// ---------------------------------------------------------------------------

#ifndef SHAVEMACHINEFUNCTIONINFO_H
#define SHAVEMACHINEFUNCTIONINFO_H (1)


#include "llvm/CodeGen/MachineFunction.h"


namespace llvm {
  class SHAVEMachineFunctionInfo : public MachineFunctionInfo {
  public:
    SHAVEMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI)
    : bIsFrameInfoReady(false),
      bSaveLR(false),
      bSaveFP(false),
      bInitFP(false),
      bHasDllExport(false),
      bHasNoReturn(false),
      usesDP4A({false, false}),
      offsetToFP(0),
      offsetToLR(0),
      stackSize(0),
      offsetToVaargs(-1ll)
    {}

    void initialiseFrameInfo(const MachineFunction &MF, Align stackAlign);
    bool isFrameInfoReady() const { return bIsFrameInfoReady; }

    bool saveLR() const { return bSaveLR; }
    bool saveFP() const { return bSaveFP; }
    bool initFP() const { return bInitFP; }

    bool hasDllExport() const { return bHasDllExport; }
    bool hasNoReturn() const { return bHasNoReturn; }

    int64_t getStackSize() const { return stackSize; }
    int64_t getFPStackOffset() const { return offsetToFP; }
    int64_t getLRStackOffset() const { return offsetToLR; }

    int64_t getVarArgsStackOffset() const { return offsetToVaargs; }
    void setVarArgsStackOffset(int64_t stackOffset) { offsetToVaargs = stackOffset; }

    bool hasSAUDP4A() const { return usesDP4A.sau; }
    bool hasVAUDP4A() const { return usesDP4A.vau; }
    void setSAUDP4AUsage(bool usesSAUDP4A) { usesDP4A.sau = usesSAUDP4A; }
    void setVAUDP4AUsage(bool usesVAUDP4A) { usesDP4A.vau = usesVAUDP4A; }

  private:
    bool bIsFrameInfoReady;

    bool bSaveLR;
    bool bSaveFP;
    bool bInitFP;
    bool bHasDllExport;
    bool bHasNoReturn;
    struct {
      bool vau;
      bool sau;
    } usesDP4A;

    // Frame information for the stack variables
    int64_t offsetToFP;
    int64_t offsetToLR;
    int64_t stackSize;

    // The 'va_arg' offset is not part of the computed stack variables, but it is set
    // during argument lowering, and used during 'va_start' lowering
    int64_t offsetToVaargs;
  };
}

#endif // SHAVEMACHINEFUNCTIONINFO_H
