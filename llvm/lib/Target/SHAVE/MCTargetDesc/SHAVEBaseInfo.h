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
// File       : SHAVEBaseInfo.h 
// Description: Top level definitions for SHAVE
// ---------------------------------------------------------------------------

//===----------------------------------------------------------------------===//
//
// This file contains small standalone helper functions and enum definitions for
// the SHAVE target useful for the compiler back-end and the MC libraries.
// As such, it deliberately does not include references to LLVM core
// code gen types, passes, etc..
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SHAVE_MCTARGETDESC_SHAVEBASEINFO_H
#define LLVM_LIB_TARGET_SHAVE_MCTARGETDESC_SHAVEBASEINFO_H

#include "llvm/MC/MCInstrDesc.h"
#include "llvm/Support/DataTypes.h"

#define SHAVE_ACCN_MACN_MARKER    (-1)

namespace llvm {


/// SHAVEII - This namespace holds all of the target specific flags that
/// instruction info tracks.
///
namespace SHAVEII {
  enum : uint64_t {
    //===------------------------------------------------------------------===//
    // SHAVE specific instruction flags
    //

    // ACC_MAC_InsnType
    // Type of ACC or MAC instruction
    // This is used to distinguish between the following sets of instructions
    //        {ACCP,ACCN,MACP,MACN} 
    //        {ACCPZ, ACCNZ, MACPZ, MACNZ}, 
    //        {ACCPW, ACCNW, MACPW, MACNW} 
    ACC_MAC_InsnTypeShift = 0,
    ACC_MAC_InsnTypeMask = 0x3,

    ACC_MAC_PN = 1,
    ACC_MAC_W  = 2,
    ACC_MAC_Z  = 3,

    // Set to 1 for all pipelined ACCxx/MACxx instructions
    Pipelined_ACC_MACShift = ACC_MAC_InsnTypeShift + 2,
    Pipelined_ACC_MACMask = 0x1 << Pipelined_ACC_MACShift,

    // Set to 1 for all MAC instructions
    IsMACInsnShift = Pipelined_ACC_MACShift + 1,
    IsMACInsnMask = 0x1 << IsMACInsnShift,

    // FunctionalUnit responsible for handling this instruction
    // The enumeration of Functional Units is maintained in SHAVEInstrInfo.h 
    // as enum SHAVEFuncUnit
    FunctionalUnitShift = IsMACInsnShift + 1,
    FunctionalUnitMask  = 0xf,

  };

  //===------------------------------------------------------------------===//
  // Helper functions to access and interpret the instruction flags encoded
  // in TSFlags
  //
 
  // getACC_MAC_InsnType - Return the bits corresponding to the 
  //                       type of ACC/MAC instruction
  inline unsigned getACC_MAC_InsnType(uint64_t TSFlags) {
    return (TSFlags & ACC_MAC_InsnTypeMask);
  }

  /// isPipelined_ACC_MAC - Return true if this is a pipelined instance of 
  //                        any of the ACC/MAC variants
  inline bool isPipelined_ACC_MAC(uint64_t TSFlags) {
    return (TSFlags & Pipelined_ACC_MACMask);
  }

  // isMACInsn - Return true if this is a MAC isntruction
  inline bool isMACInsn(uint64_t TSFlags) {
    return (TSFlags & IsMACInsnMask);
  }


  inline unsigned getFunctionalUnitFromTSFlags(uint64_t TSFlags) {
    return (TSFlags>>FunctionalUnitShift) & FunctionalUnitMask;
  }

} // end namespace SHAVEII

} // end namespace llvm;

#endif // LLVM_LIB_TARGET_SHAVE_MCTARGETDESC_SHAVEBASEINFO_H
