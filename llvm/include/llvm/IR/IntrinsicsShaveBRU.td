//===- IntrinsicsShaveBRU.td - Defines Shave intrinsics ----*- tablegen -*-===//
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
// This file defines all of the Shave-specific intrinsics.
//
//===----------------------------------------------------------------------===//

//  BRU.SWIH
//    Signal software interrupt and halt
//    The immediate operand has a value between 0 and 8191
def int_shave_bru_swih_i : SHAVE_Intrinsic<"bru_swih_i", [  ], [ llvm_i8_ty ], [IntrNoReturn]>;

//  BRU.SWIC
//    Signal software interrupt and continue
//    The immediate operand has a value between 0 and 8191
def int_shave_bru_swic_i : SHAVE_Intrinsic<"bru_swic_i", [  ], [ llvm_i8_ty ], []>;
