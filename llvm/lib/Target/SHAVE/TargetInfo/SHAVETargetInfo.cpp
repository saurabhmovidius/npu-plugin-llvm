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
// File       :  SHAVETargetInfo.cpp
// Description:  Declare target name
// ---------------------------------------------------------------------------

#include "llvm/MC/TargetRegistry.h"

#include "SHAVE.h"

using namespace	llvm;


Target llvm::TheSHAVETarget;

extern "C" void LLVMInitializeSHAVETargetInfo() {
  RegisterTarget<Triple::shave, false> SHAVE(TheSHAVETarget, "shave", "Myriad SHAVE Processor", "SHAVE");
}
