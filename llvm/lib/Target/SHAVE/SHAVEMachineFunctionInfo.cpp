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
// File       :  SHAVEMachineFunctionInfo.cpp
// Description:  Function frame information
// ***************************************************************************

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/MachineLocation.h"

#include "SHAVEFrameLowering.h"
#include "SHAVEInstrInfo.h"
#include "SHAVEMachineFunctionInfo.h"
#include "SHAVERegisterInfo.h"
#include "SHAVETargetMachine.h"

using namespace llvm;

void SHAVEMachineFunctionInfo::initialiseFrameInfo(const MachineFunction &MF,
                                                   Align stackAlign) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const Function &FN = MF.getFunction();

  bHasDllExport = FN.hasDLLExportStorageClass();
  bHasNoReturn = FN.hasFnAttribute(Attribute::NoReturn);
  bInitFP = MFI.hasVarSizedObjects();
  bSaveFP = bInitFP && !bHasDllExport && !bHasNoReturn;
  bSaveLR = MFI.hasCalls() && !bHasDllExport && !bHasNoReturn;

  // Computer the stack reservation for this function
  stackSize = static_cast<int64_t>(MFI.getStackSize())
                + (bSaveLR ? 4 : 0)      // Link register
                + (bSaveFP ? 4 : 0);     // Frame pointer

  // Get the strictest (and thus the greatest) alignment of any object on the stack
  Align maxAlign = std::max(MFI.getMaxAlign(), stackAlign);

  // Round up the stack size to be multiple of the maximum alignment
  stackSize = alignTo(stackSize, maxAlign);

  offsetToLR = stackSize - (bSaveLR ? 4 : 0); // Link register offset from top of stack
  offsetToFP = offsetToLR - (bSaveFP ? 4 : 0); // Slot to save the old frame pointer

  bIsFrameInfoReady = true;
}
