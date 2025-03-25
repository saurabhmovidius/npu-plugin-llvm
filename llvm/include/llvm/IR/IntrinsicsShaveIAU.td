//===- IntrinsicsShaveIAU.td - Defines Shave intrinsics ----*- tablegen -*-===//
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

//  IAU.ADDSI
//    32-bit signed integer addition with saturation
def int_shave_iau_addsi_rr : SHAVE_Intrinsic<"iau_addsi_rr", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty ]>;
def int_shave_iau_addsi_ri : SHAVE_Intrinsic<"iau_addsi_ri", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i8_ty ]>;

//  IAU.ADDSU
//    32-bit unsigned integer addition with saturation
def int_shave_iau_addsu_rr : SHAVE_Intrinsic<"iau_addsu_rr", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty ]>;
def int_shave_iau_addsu_ri : SHAVE_Intrinsic<"iau_addsu_ri", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i8_ty ]>;

//  IAU.ALIGN
//    Align to byte boundary
def int_shave_iau_align_rri : SHAVE_Intrinsic<"iau_align_rri", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty, llvm_i8_ty ]>;

//  IAU.AND
//    Bitwise AND
def int_shave_iau_and_rr : SHAVE_Intrinsic<"iau_and_rr", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty ]>;

//  IAU.BSF
//    Find bit position of least significant bit set to 1/0
def int_shave_iau_bsf_32_r    : SHAVE_Intrinsic<"iau_bsf_32_r",    [ llvm_i32_ty ], [ llvm_i32_ty ]>;
def int_shave_iau_bsfinv_32_r : SHAVE_Intrinsic<"iau_bsfinv_32_r", [ llvm_i32_ty ], [ llvm_i32_ty ]>;

//  IAU.BSR
//    Find bit position of most significant bit set to 1/0
def int_shave_iau_bsr_32_r    : SHAVE_Intrinsic<"iau_bsr_32_r",    [ llvm_i32_ty ], [ llvm_i32_ty ]>;
def int_shave_iau_bsrinv_32_r : SHAVE_Intrinsic<"iau_bsrinv_32_r", [ llvm_i32_ty ], [ llvm_i32_ty ]>;

//  IAU.FEXTI
//    Signed integer field extract with immediate specifiers
def int_shave_iau_fexti_rii : SHAVE_Intrinsic<"iau_fexti_rii", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i8_ty, llvm_i8_ty ]>;

//  IAU.FEXTJI
//    Signed integer field extract with indirect specifiers
def int_shave_iau_fextji_rr : SHAVE_Intrinsic<"iau_fextji_rr", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty ]>;

//  IAU.FEXTJU
//    Unsigned integer field extract with indirect specifiers
def int_shave_iau_fextju_rr : SHAVE_Intrinsic<"iau_fextju_rr", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty ]>;

//  IAU.FEXTU
//    Unsigned integer field extract with immediate specifiers
def int_shave_iau_fextu_rii : SHAVE_Intrinsic<"iau_fextu_rii", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i8_ty, llvm_i8_ty ]>;

//  IAU.FINS
//    Field insert with immediate specifiers
def int_shave_iau_fins_rrii : SHAVE_Intrinsic<"iau_fins_rrii", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty, llvm_i8_ty, llvm_i8_ty ]>;

//  IAU.FINSJ
//    Field insert with indirect specifiers
def int_shave_iau_finsj_rrr : SHAVE_Intrinsic<"iau_finsj_rrr", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty, llvm_i32_ty ]>;

//  IAU.MULSI
//    32-bit signed integer multiplication with saturation
def int_shave_iau_mulsi_rr : SHAVE_Intrinsic<"iau_mulsi_rr", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty ]>;
def int_shave_iau_mulsi_ri : SHAVE_Intrinsic<"iau_mulsi_ri", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i8_ty ]>;

//  IAU.MULSU
//    32-bit unsigned integer multiplication with saturation
def int_shave_iau_mulsu_rr : SHAVE_Intrinsic<"iau_mulsu_rr", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty ]>;
def int_shave_iau_mulsu_ri : SHAVE_Intrinsic<"iau_mulsu_ri", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i8_ty ]>;

//  IAU.ONES
//    Count number of bits set to 1/0
def int_shave_iau_ones_r  : SHAVE_Intrinsic<"iau_ones_r",  [ llvm_i32_ty ], [ llvm_i32_ty ]>;
def int_shave_iau_zeros_r : SHAVE_Intrinsic<"iau_zeros_r", [ llvm_i32_ty ], [ llvm_i32_ty ]>;

//  IAU.RVRS
//    Reverse bit order
def int_shave_iau_rvrs_r : SHAVE_Intrinsic<"iau_rvrs_r", [ llvm_i32_ty ], [ llvm_i32_ty ]>;

//  IAU.SUBSI
//    32-bit signed integer subtraction with saturation
def int_shave_iau_subsi_rr : SHAVE_Intrinsic<"iau_subsi_rr", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty ]>;
def int_shave_iau_subsi_ri : SHAVE_Intrinsic<"iau_subsi_ri", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i8_ty ]>;

//  IAU.SUBSU
//    32-bit unsigned integer subtraction with saturation
def int_shave_iau_subsu_rr : SHAVE_Intrinsic<"iau_subsu_rr", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty ]>;
def int_shave_iau_subsu_ri : SHAVE_Intrinsic<"iau_subsu_ri", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i8_ty ]>;
