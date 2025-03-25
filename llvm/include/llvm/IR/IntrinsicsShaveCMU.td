//===- IntrinsicsShaveCMU.td - Defines Shave intrinsics ----*- tablegen -*-===//
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

//===----------------------------------------------------------------------===//
// Helper classes for instructions with multiple variants
//===----------------------------------------------------------------------===//

multiclass CMU_CLAMP_Intrinsic<SHAVEType type> {
  defm _#type.scalarString#_rr : SHAVE_SuffixedIntrinsic<!strconcat("cmu_clamp_", type.scalarString, "_rr"),
                                                         [ type.llvmType ],
                                                         [ type.llvmType, type.llvmType ]>;

  defm 0_#type.scalarString#_rr : SHAVE_SuffixedIntrinsic<!strconcat("cmu_clamp0_", type.scalarString, "_rr"),
                                                          [ type.llvmType ],
                                                          [ type.llvmType, type.llvmType ]>;
}

multiclass CMU_CLAMPAB_Intrinsic<SHAVEType type> {
  defm _#type.scalarString#_rrr : SHAVE_SuffixedIntrinsic<!strconcat("cmu_clampab_", type.scalarString, "_rrr"),
                                                          [ type.llvmType ],
                                                          [ type.llvmType,  type.llvmType,  type.llvmType ]>;

  if type.scalarType.isInteger then {
    // Add an unsigned variant for integer types only
    defm _#type.unsignedScalarString#_rrr : SHAVE_SuffixedIntrinsic<!strconcat("cmu_clampab_", type.unsignedScalarString, "_rrr"),
                                                                    [ type.llvmType ],
                                                                    [ type.llvmType,  type.llvmType,  type.llvmType ],
                                                                    /*isUnsigned*/ 1>;
  }
}

multiclass CMU_MINMAX_Intrinsic<string opcode, SHAVEType type> {
  defm _#type.scalarString#_rr : SHAVE_SuffixedIntrinsic<!strconcat("cmu_", opcode, "_", type.scalarString, "_rr"),
                                                         [ type.llvmType ],
                                                         [ type.llvmType,  type.llvmType ]>;

  if type.scalarType.isInteger then {
    // Add an unsigned variant for integer types only
    defm _#type.unsignedScalarString#_rr : SHAVE_SuffixedIntrinsic<!strconcat("cmu_", opcode, "_", type.unsignedScalarString, "_rr"),
                                                                   [ type.llvmType ],
                                                                   [ type.llvmType,  type.llvmType ],
                                                                   /*isUnsigned*/ 1>;
  }
}

multiclass CMU_PERM_Intrinsic<SHAVEType type> {
  defvar suffix = type.bitwiseString;

  defm _#suffix#_rr : SHAVE_SuffixedIntrinsic<!strconcat("cmu_perm_", suffix, "_rr"),
                                              [ type.llvmType ],
                                              [ type.llvmType, type.llvmType ]>;
  defm b_#suffix#_rrr : SHAVE_SuffixedIntrinsic<!strconcat("cmu_permb_", suffix, "_rrr"),
                                                [ type.llvmType ],
                                                [ type.llvmType, type.llvmType, type.llvmType ]>;

  if type.scalarType.isInteger then {
    // Add an unsigned variant for integer types only
    defm _#suffix#_rr : SHAVE_SuffixedIntrinsic<!strconcat("cmu_perm_", suffix, "_rr"),
                                                [ type.llvmType ],
                                                [ type.llvmType, type.llvmType ],
                                                /*isUnsigned*/ 1>;
    defm b_#suffix#_rrr : SHAVE_SuffixedIntrinsic<!strconcat("cmu_permb_", suffix, "_rrr"),
                                                  [ type.llvmType ],
                                                  [ type.llvmType, type.llvmType, type.llvmType ],
                                                  /*isUnsigned*/ 1>;
  }
}

//===----------------------------------------------------------------------===//
// 128-bit VRF built-ins
//===----------------------------------------------------------------------===//

// Built-ins for all supported 128-bit vector types
foreach type = [ llvm_v4f32_ty, llvm_v8f16_ty, llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defvar shaveType = SHAVEType<type>;

  //
  //  CMU.ALIGNVEC
  //    Long vector align with byte offset
  //
  defm int_shave_cmu_alignvec_rri : SHAVE_SuffixedIntrinsic<"cmu_alignvec_rri",
                                                            [ type ],
                                                            [ type, type, llvm_i8_ty ]>;

  //
  //  CMU.CLAMP
  //    Clamp between zero and a positive value or between +/- a specified value
  //
  defm int_shave_cmu_clamp : CMU_CLAMP_Intrinsic<shaveType>;

  //
  //  CMU.CLAMPAB
  //    Clamp within range
  //
  defm int_shave_cmu_clampab : CMU_CLAMPAB_Intrinsic<shaveType>;

  //
  //  CMU.MAX
  //    Maximum
  //
  defm int_shave_cmu_max : CMU_MINMAX_Intrinsic<"max", shaveType>;

  //
  //  CMU.MIN
  //    Minimum
  //
  defm int_shave_cmu_min : CMU_MINMAX_Intrinsic<"min", shaveType>;
}

//
//  CMU.CMASK
//    Generate VRF mask from IRF contents
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defm int_shave_cmu_cmask_r : SHAVE_SuffixedIntrinsic<"cmu_cmask_r", [ type ], [ llvm_i32_ty ]>;
}

//
//  CMU.CPVV
//    VRF copy with optional conversion.
//

def int_shave_cmu_cpvv_f16_u8fs_r : SHAVE_Intrinsic<"cmu_cpvv_f16_u8fs_r", [ llvm_v8i8_ty ],  [ llvm_v8f16_ty ]>;
def int_shave_cmu_cpvv_u8f_f16_r  : SHAVE_Intrinsic<"cmu_cpvv_u8f_f16_r",  [ llvm_v8f16_ty ], [ llvm_v8i8_ty ]>;

def int_shave_cmu_cpvv_u16_u8s_r  : SHAVE_Intrinsic<"cmu_cpvv_u16_u8s_r",  [ llvm_v8i8_ty ],  [ llvm_v8i16_ty ]>;
def int_shave_cmu_cpvv_f16_u8s_r  : SHAVE_Intrinsic<"cmu_cpvv_f16_u8s_r",  [ llvm_v8i8_ty ],  [ llvm_v8f16_ty ]>;

//
//  CMU.SHIV
//    Shift IRF left or right into VRF
//

def int_shave_cmu_shliv_x32_rr : SHAVE_Intrinsic<"cmu_shliv_x32_rr", [ llvm_v4i32_ty ], [ llvm_v4i32_ty,llvm_i32_ty ]>;
def int_shave_cmu_shliv_x16_rr : SHAVE_Intrinsic<"cmu_shliv_x16_rr", [ llvm_v8i16_ty ], [ llvm_v8i16_ty,llvm_i16_ty ]>;
def int_shave_cmu_shliv_x8_rr  : SHAVE_Intrinsic<"cmu_shliv_x8_rr",  [ llvm_v16i8_ty ], [ llvm_v16i8_ty,llvm_i8_ty ]>;

def int_shave_cmu_shriv_x32_rr : SHAVE_Intrinsic<"cmu_shriv_x32_rr", [ llvm_v4i32_ty ], [ llvm_v4i32_ty,llvm_i32_ty ]>;
def int_shave_cmu_shriv_x16_rr : SHAVE_Intrinsic<"cmu_shriv_x16_rr", [ llvm_v8i16_ty ], [ llvm_v8i16_ty,llvm_i16_ty ]>;
def int_shave_cmu_shriv_x8_rr  : SHAVE_Intrinsic<"cmu_shriv_x8_rr",  [ llvm_v16i8_ty ], [ llvm_v16i8_ty,llvm_i8_ty ]>;

//
//  CMU.VNZ
//    Generate IRF mask from VRF contents
//

def int_shave_cmu_vnz_x32_rri_int4    : SHAVE_Intrinsic<"cmu_vnz_x32_rri_int4", [ llvm_v4i8_ty ],  [ llvm_v4i32_ty, llvm_v4i8_ty, llvm_i8_ty ]>;
def int_shave_cmu_vnz_x32_rri_uint4   : SHAVE_Intrinsic<"cmu_vnz_x32_rri_uint4", [ llvm_v4i8_ty ],  [ llvm_v4i32_ty, llvm_v4i8_ty, llvm_i8_ty ]>;
def int_shave_cmu_vnz_x16_rri_short8  : SHAVE_Intrinsic<"cmu_vnz_x16_rri_short8", [ llvm_v4i8_ty ],  [ llvm_v8i16_ty, llvm_v4i8_ty, llvm_i8_ty ]>;
def int_shave_cmu_vnz_x16_rri_ushort8 : SHAVE_Intrinsic<"cmu_vnz_x16_rri_ushort8", [ llvm_v4i8_ty ],  [ llvm_v8i16_ty, llvm_v4i8_ty, llvm_i8_ty ]>;
def int_shave_cmu_vnz_x8_rri_char16   : SHAVE_Intrinsic<"cmu_vnz_x8_rri_char16",  [ llvm_v2i16_ty ], [ llvm_v16i8_ty, llvm_v2i16_ty, llvm_i8_ty ]>;
def int_shave_cmu_vnz_x8_rri_uchar16  : SHAVE_Intrinsic<"cmu_vnz_x8_rri_uchar16",  [ llvm_v2i16_ty ], [ llvm_v16i8_ty, llvm_v2i16_ty, llvm_i8_ty ]>;

//
//  CMU.VSZM
//    Byte/Word swizzle with optional write disable and optional zeroing
//

// Floating-point variants
defm int_shave_cmu_vszmword_riiii : SHAVE_SuffixedIntrinsic<"cmu_vszmword_riiii", [ llvm_v4f32_ty ], [ llvm_v4f32_ty, llvm_i8_ty, llvm_i8_ty, llvm_i8_ty, llvm_i8_ty ]>;

// Integer variants
foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defm int_shave_cmu_vszmword_riiii : SHAVE_SuffixedIntrinsic<"cmu_vszmword_riiii", [ type ], [ type, llvm_i8_ty, llvm_i8_ty, llvm_i8_ty, llvm_i8_ty ]>;
  defm int_shave_cmu_vszmbyte_riiii : SHAVE_SuffixedIntrinsic<"cmu_vszmbyte_riiii", [ type ], [ type, llvm_i8_ty, llvm_i8_ty, llvm_i8_ty, llvm_i8_ty ]>;
}

//===----------------------------------------------------------------------===//
// 512-bit VRF built-ins
//===----------------------------------------------------------------------===//

//
// CMU.EXPAND
//   Variable length vector scatter to long vector with element indexing by binary bitmap. Destructive (write zero to unused elements) and non-destructive variants
//

multiclass CMU_EXPAND_Intrinsic<string suffix, LLVMType type, LLVMType bitmapType> {
  defm _expand_#suffix   : SHAVE_SuffixedIntrinsic<!strconcat("cmu_expand_", suffix),   [ type ], [ type, bitmapType ]>;
  defm _ndexpand_#suffix : SHAVE_SuffixedIntrinsic<!strconcat("cmu_ndexpand_", suffix), [ type ], [ type, bitmapType ]>;
}

// 32-bit element variants
foreach type = [ llvm_v16f32_ty, llvm_v16i32_ty ] in {
  defm int_shave_cmu : CMU_EXPAND_Intrinsic<"x32_rr", type, llvm_i32_ty>;
}

// 16-bit element variants
foreach type = [ llvm_v32f16_ty, llvm_v32i16_ty ] in {
  defm int_shave_cmu : CMU_EXPAND_Intrinsic<"x16_rr", type, llvm_i32_ty>;
}

// 8-bit element variants
defm int_shave_cmu : CMU_EXPAND_Intrinsic<"x8_rr", llvm_v64i8_ty, llvm_i64_ty>;

foreach type = [ llvm_v16f32_ty, llvm_v32f16_ty, llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defvar shaveType = SHAVEType<type>;

  //
  //  CMU.PERM
  //    Long vector permute or perrmute with blend based on indices specified in another long vector
  //
  defm int_shave_cmu_perm : CMU_PERM_Intrinsic<shaveType>;

  //
  //  CMU.ALIGNVEC
  //    Long vector align with byte offset
  //
  defm int_shave_cmu_alignvec_rri : SHAVE_SuffixedIntrinsic<"cmu_alignvec_rri", [ type ], [ type, type, llvm_i8_ty ]>;

  //
  //  CMU.CLAMP
  //    Clamp between zero and a positive value or between +/- a specified value
  //
  defm int_shave_cmu_clamp : CMU_CLAMP_Intrinsic<shaveType>;

  //
  //  CMU.CLAMPAB
  //    Clamp within range
  //
  defm int_shave_cmu_clampab : CMU_CLAMPAB_Intrinsic<shaveType>;

  //
  //  CMU.MAX
  //    Maximum
  //
  defm int_shave_cmu_max : CMU_MINMAX_Intrinsic<"max", shaveType>;

  //
  //  CMU.MIN
  //    Minimum
  //
  defm int_shave_cmu_min : CMU_MINMAX_Intrinsic<"min", shaveType>;
}

//
//  CMU.SHIV
//    Shift IRF left or right into VRF
//
defm int_shave_cmu_shliv_x32_rr : SHAVE_SuffixedIntrinsic<"cmu_shliv_x32_rr", [ llvm_v16i32_ty ], [ llvm_v16i32_ty, llvm_i32_ty ]>;
defm int_shave_cmu_shliv_x16_rr : SHAVE_SuffixedIntrinsic<"cmu_shliv_x16_rr", [ llvm_v32i16_ty ], [ llvm_v32i16_ty, llvm_i16_ty ]>;
defm int_shave_cmu_shliv_x8_rr  : SHAVE_SuffixedIntrinsic<"cmu_shliv_x8_rr",  [ llvm_v64i8_ty ],  [ llvm_v64i8_ty, llvm_i8_ty ]>;

defm int_shave_cmu_shriv_x32_rr : SHAVE_SuffixedIntrinsic<"cmu_shriv_x32_rr", [ llvm_v16i32_ty ], [ llvm_v16i32_ty, llvm_i32_ty ]>;
defm int_shave_cmu_shriv_x16_rr : SHAVE_SuffixedIntrinsic<"cmu_shriv_x16_rr", [ llvm_v32i16_ty ], [ llvm_v32i16_ty, llvm_i16_ty ]>;
defm int_shave_cmu_shriv_x8_rr  : SHAVE_SuffixedIntrinsic<"cmu_shriv_x8_rr",  [ llvm_v64i8_ty ],  [ llvm_v64i8_ty, llvm_i8_ty ]>;

//===----------------------------------------------------------------------===//
// 8/16/32-bit IRF built-ins
//===----------------------------------------------------------------------===//

// Built-ins for all support scalar types
foreach type = [ llvm_float_ty, llvm_half_ty, llvm_i32_ty, llvm_i16_ty, llvm_i8_ty ] in {
  defvar shaveType = SHAVEType<type>;

  //
  //  CMU.CLAMP
  //    Clamp between zero and a positive value or between +/- a specified value
  //
  defm int_shave_cmu_clamp : CMU_CLAMP_Intrinsic<shaveType>;

  //
  //  CMU.CLAMPAB
  //    Clamp within range
  //
  defm int_shave_cmu_clampab : CMU_CLAMPAB_Intrinsic<shaveType>;

  //
  //  CMU.MAX
  //    Maximum
  //
  defm int_shave_cmu_max : CMU_MINMAX_Intrinsic<"max", shaveType>;

  //
  //  CMU.MIN
  //    Minimum
  //
  defm int_shave_cmu_min : CMU_MINMAX_Intrinsic<"min", shaveType>;
}

//
//  CMU.CPII
//    IRF copy with optional conversion.
//

def int_shave_cmu_cpii_u8f_f16_r  : SHAVE_Intrinsic<"cmu_cpii_u8f_f16_r",  [ llvm_half_ty ], [ llvm_i8_ty ]>;
def int_shave_cmu_cpii_f16_u8fs_r : SHAVE_Intrinsic<"cmu_cpii_f16_u8fs_r", [ llvm_i8_ty ],   [ llvm_half_ty ]>;
def int_shave_cmu_cpii_f16_u8s_r  : SHAVE_Intrinsic<"cmu_cpii_f16_u8s_r",  [ llvm_i8_ty ],   [ llvm_half_ty ]>;
