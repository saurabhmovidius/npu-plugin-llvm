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
// File       :  SHAVEMCTargetDesc.h
// Description:  Provides SHAVE specific target descriptions.
// ***************************************************************************

#ifndef SHAVEMCTARGETDESC_H
#define SHAVEMCTARGETDESC_H (1)


namespace llvm {
  class Target;

  extern Target TheSHAVETarget;
}

// Defines symbolic names for registers.
#define GET_INSTRINFO_ENUM
#include "SHAVEGenInstrInfo.inc"

// Defines symbolic names for instructions.
#define GET_REGINFO_ENUM
#include "SHAVEGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "SHAVEGenSubtargetInfo.inc"


#endif // SHAVEMCTARGETDESC_H
