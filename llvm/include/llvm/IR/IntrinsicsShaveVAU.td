//===- IntrinsicsShaveVAU.td - Defines Shave intrinsics ----*- tablegen -*-===//
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

multiclass SHAVE_VAUIntrinsicRR<string opcode,
                                LLVMType type,
                                bit hasUnsigned = 1,
                                bit hasSuffix = 0> {
  defvar shaveType = SHAVEType<type>;

  defvar iSuffix = !cond(
    hasSuffix : !strconcat("_", SHAVEType<type>.cName),
    true      : ""
  );

  def _#shaveType.scalarString#_rr#iSuffix :
    SHAVE_Intrinsic<!strconcat(opcode, "_", shaveType.scalarString, "_rr", iSuffix),
                    [ type ],
                    [ type, type ]>;

  // Add an unsigned variant for integer types only
  if !and(hasUnsigned, type.VT.isInteger) then {
    defvar uSuffix = !cond(
      hasSuffix : !strconcat("_u", SHAVEType<type>.cName),
      true      : ""
    );

    def _#shaveType.unsignedScalarString#_rr#uSuffix :
      SHAVE_Intrinsic<!strconcat(opcode, "_", shaveType.unsignedScalarString, "_rr", uSuffix),
                      [ type ],
                      [ type, type ]>;
  }
}

multiclass SHAVE_VAUIntrinsicRI<string opcode,
                                LLVMType type,
                                bit hasUnsigned = 1,
                                bit hasSuffix = 0> {
  defvar shaveType = SHAVEType<type>;

  defvar iSuffix = !cond(
    hasSuffix : !strconcat("_", SHAVEType<type>.cName),
    true      : ""
  );

  def _#shaveType.scalarString#_ri#iSuffix :
    SHAVE_Intrinsic<!strconcat(opcode, "_", shaveType.scalarString, "_ri", iSuffix),
                    [ type ],
                    [ type, llvm_i8_ty ]>;

  // Add an unsigned variant for integer types only
  if !and(hasUnsigned, type.VT.isInteger) then {
    defvar uSuffix = !cond(
      hasSuffix : !strconcat("_u", SHAVEType<type>.cName),
      true      : ""
    );

    def _#shaveType.unsignedScalarString#_ri#uSuffix :
      SHAVE_Intrinsic<!strconcat(opcode, "_", shaveType.unsignedScalarString, "_ri", uSuffix),
                      [ type ],
                      [ type, llvm_i8_ty ]>;
  }
}

multiclass SHAVE_VAUIntrinsic<string opcode,
                              LLVMType type,
                              bit hasUnsigned = 1,
                              bit hasImmediateVariant = 1,
                              bit hasSuffix = 0> {
  defm _#opcode : SHAVE_VAUIntrinsicRR<opcode, type, hasUnsigned, hasSuffix>;

  if !and(hasImmediateVariant, type.VT.isInteger) then {
    defm _#opcode : SHAVE_VAUIntrinsicRI<opcode, type, hasUnsigned, hasSuffix>;
  }
}

//===----------------------------------------------------------------------===//
// 128-bit VRF built-ins
//===----------------------------------------------------------------------===//

//
//  VAU.ABS
//    Long vector absolute value
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty, llvm_v4f32_ty, llvm_v8f16_ty ] in {
  defvar shaveType = SHAVEType<type>;
  def int_shave_vau_abs_#shaveType.scalarString#_r : SHAVE_Intrinsic<!strconcat("vau_abs_", shaveType.scalarString, "_r"), [ type ], [ type ]>;
}

//
//  VAU.ADD
//    Vector addition with optional immediate operand
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty, llvm_v4f32_ty, llvm_v8f16_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_add",
                                      type,
                                      0, // hasUnsigned
                                      1>; // hasImmediateVariant
}

//
//  VAU.ADIFF
//    Long vector absolute difference
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty, llvm_v4f32_ty, llvm_v8f16_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_adiff",
                                      type,
                                      1, // hasUnsigned
                                      0>; // hasImmediateVariant
}

//
//  VAU.ALIGNBYTE
//    Long vector align bytes within words
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defvar name = !strconcat("vau_alignbyte_rri_", SHAVEType<type>.cName);
  def int_shave_#name : SHAVE_Intrinsic<name, [ type ], [ type, type, llvm_i8_ty ]>;
}

//
//  VAU.AND
//    Bitwise AND
//

def int_shave_vau_and_32_rr : SHAVE_Intrinsic<"vau_and_32_rr", [ llvm_v4i32_ty ], [ llvm_v4i32_ty, llvm_v4i32_ty ]>;
def int_shave_vau_and_16_rr : SHAVE_Intrinsic<"vau_and_16_rr", [ llvm_v8i16_ty ], [ llvm_v8i16_ty, llvm_v8i16_ty ]>;
def int_shave_vau_and_8_rr  : SHAVE_Intrinsic<"vau_and_8_rr",  [ llvm_v16i8_ty ], [ llvm_v16i8_ty, llvm_v16i8_ty ]>;

//
//  VAU.AVG
//    Integer average
//

def int_shave_vau_avg_x32_rr : SHAVE_Intrinsic<"vau_avg_x32_rr", [ llvm_v4i32_ty ], [ llvm_v4i32_ty, llvm_v4i32_ty ]>;
def int_shave_vau_avg_x16_rr : SHAVE_Intrinsic<"vau_avg_x16_rr", [ llvm_v8i16_ty ], [ llvm_v8i16_ty, llvm_v8i16_ty ]>;
def int_shave_vau_avg_x8_rr  : SHAVE_Intrinsic<"vau_avg_x8_rr",  [ llvm_v16i8_ty ], [ llvm_v16i8_ty, llvm_v16i8_ty ]>;

//
//  VAU.CMB
//    Long vector combine
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defvar suffix = SHAVEType<type>.cName;
  def int_shave_vau_cmbbyte_rriiii_#suffix : SHAVE_Intrinsic<!strconcat("vau_cmbbyte_rriiii_", suffix),
                                                             [ type ],
                                                             [ type, type, llvm_i8_ty, llvm_i8_ty, llvm_i8_ty, llvm_i8_ty ]>;
}

foreach type = [ llvm_v4f32_ty, llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defvar suffix = SHAVEType<type>.cName;
  def int_shave_vau_cmbword_rriiii_#suffix: SHAVE_Intrinsic<!strconcat("vau_cmbword_rriiii_", suffix),
                                                            [ type ],
                                                            [ type, type, llvm_i8_ty, llvm_i8_ty, llvm_i8_ty, llvm_i8_ty ]>;
}

//
//  VAU.IADDS
//    Long vector integer addition with saturation. Optional unsigned immediate operand
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_iadds", type>;
}

//
//  VAU.ILMULH
//    Long vector multiply returning higher-order result
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_ilmulh",
                                      type,
                                      1, // hasUnsigned
                                      0>; // hasImmediateVariant
}

//
//  VAU.IMULS
//    Long vector integer multiplication with saturation. Optional immediate operand
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_imuls", type>;
}

//
//  VAU.ISUBS
//    Long vector integer subtraction with saturation. Optional unsigned immediate operand
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_isubs", type>;
}

//
//  VAU.MUL
//    Long vector multiplication with optional immediate operand
//

foreach type = [ llvm_v4f32_ty, llvm_v4i32_ty, llvm_v8f16_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_mul",
                                      type,
                                      0, // hasUnsigned
                                      1>; // hasImmediateVariant
}

//
//  VAU.NOT
//    Bitwise NOT
//

def int_shave_vau_not_i32_r : SHAVE_Intrinsic<"vau_not_i32_r", [ llvm_v4i32_ty ], [ llvm_v4i32_ty ]>;
def int_shave_vau_not_i16_r : SHAVE_Intrinsic<"vau_not_i16_r", [ llvm_v8i16_ty ], [ llvm_v8i16_ty ]>;
def int_shave_vau_not_i8_r  : SHAVE_Intrinsic<"vau_not_i8_r",  [ llvm_v16i8_ty ], [ llvm_v16i8_ty ]>;

//
//  VAU.OR
//    Bitwise OR
//

def int_shave_vau_or_32_rr : SHAVE_Intrinsic<"vau_or_32_rr", [ llvm_v4i32_ty ], [ llvm_v4i32_ty, llvm_v4i32_ty ]>;
def int_shave_vau_or_16_rr : SHAVE_Intrinsic<"vau_or_16_rr", [ llvm_v8i16_ty ], [ llvm_v8i16_ty, llvm_v8i16_ty ]>;
def int_shave_vau_or_8_rr  : SHAVE_Intrinsic<"vau_or_8_rr",  [ llvm_v16i8_ty ], [ llvm_v16i8_ty, llvm_v16i8_ty ]>;

//
//  VAU.ROL
//    Vector rotate left
//

// Register-register variants
def int_shave_vau_rol_x32_rr : SHAVE_Intrinsic<"vau_rol_x32_rr", [ llvm_v4i32_ty ], [ llvm_v4i32_ty,llvm_v4i32_ty ]>;
def int_shave_vau_rol_x16_rr : SHAVE_Intrinsic<"vau_rol_x16_rr", [ llvm_v8i16_ty ], [ llvm_v8i16_ty,llvm_v8i16_ty ]>;
def int_shave_vau_rol_x8_rr  : SHAVE_Intrinsic<"vau_rol_x8_rr",  [ llvm_v16i8_ty ], [ llvm_v16i8_ty,llvm_v16i8_ty ]>;

// Register-immediate variants
def int_shave_vau_rol_x32_ri : SHAVE_Intrinsic<"vau_rol_x32_ri", [ llvm_v4i32_ty ], [ llvm_v4i32_ty, llvm_i8_ty ]>;
def int_shave_vau_rol_x16_ri : SHAVE_Intrinsic<"vau_rol_x16_ri", [ llvm_v8i16_ty ], [ llvm_v8i16_ty, llvm_i8_ty ]>;
def int_shave_vau_rol_x8_ri  : SHAVE_Intrinsic<"vau_rol_x8_ri",  [ llvm_v16i8_ty ], [ llvm_v16i8_ty, llvm_i8_ty ]>;

//
//  VAU.SHD
//    Long vector arithmetic shift right with set to zero on underflow. Optional immediate shift amount
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_shd",
                                      type,
                                      0, // hasUnsigned
                                      1>; // hasImmediateVariant
}

//
//  VAU.SHL
//    Long vector logical shift left with optional immediate shift amount
//

// Register-register variants
def int_shave_vau_shl_x32_rr : SHAVE_Intrinsic<"vau_shl_x32_rr", [ llvm_v4i32_ty ], [ llvm_v4i32_ty, llvm_v4i32_ty ]>;
def int_shave_vau_shl_x16_rr : SHAVE_Intrinsic<"vau_shl_x16_rr", [ llvm_v8i16_ty ], [ llvm_v8i16_ty, llvm_v8i16_ty ]>;
def int_shave_vau_shl_x8_rr  : SHAVE_Intrinsic<"vau_shl_x8_rr", [ llvm_v16i8_ty ],  [ llvm_v16i8_ty, llvm_v16i8_ty ]>;

// Register-immediate variants
def int_shave_vau_shl_x32_ri : SHAVE_Intrinsic<"vau_shl_x32_ri", [ llvm_v4i32_ty ], [ llvm_v4i32_ty, llvm_i8_ty ]>;
def int_shave_vau_shl_x16_ri : SHAVE_Intrinsic<"vau_shl_x16_ri", [ llvm_v8i16_ty ], [ llvm_v8i16_ty, llvm_i8_ty ]>;
def int_shave_vau_shl_x8_ri  : SHAVE_Intrinsic<"vau_shl_x8_ri", [ llvm_v16i8_ty ],  [ llvm_v16i8_ty, llvm_i8_ty ]>;

//
//  VAU.SHR
//    Long vector logical/arithmetic shift right with optional immediate shift amount
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_shr", type>;
}

//
//  VAU.SUB
//    Long vector subtraction with optional immediate operand
//

foreach type = [ llvm_v4f32_ty, llvm_v8f16_ty, llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_sub",
                                      type,
                                      0, // hasUnsigned
                                      1>; // hasImmediateVariant
}

//
//  VAU.XOR
//    Bitwise XOR
//

def int_shave_vau_xor_32_rr : SHAVE_Intrinsic<"vau_xor_32_rr", [ llvm_v4i32_ty ], [ llvm_v4i32_ty, llvm_v4i32_ty ]>;
def int_shave_vau_xor_16_rr : SHAVE_Intrinsic<"vau_xor_16_rr", [ llvm_v8i16_ty ], [ llvm_v8i16_ty, llvm_v8i16_ty ]>;
def int_shave_vau_xor_8_rr  : SHAVE_Intrinsic<"vau_xor_8_rr",  [ llvm_v16i8_ty ], [ llvm_v16i8_ty, llvm_v16i8_ty ]>;

//
// VAU 128-bit Transcendentals
//

foreach name = [ "tanh", "sigm", "log", "exp", "sqt" ] in {
  def int_shave_vau_#name#_v8f16_r : SHAVE_Intrinsic<!strconcat("vau_", name, "_v8f16_r"), [ llvm_v8f16_ty ], [ llvm_v8f16_ty ]>;
}

//
//  VAU.DP4A
//    8-bit signed integer multiplication with sum to 32-bit signed integer and optional accumulate
//

def int_shave_vau_dp4_v16i8_rr : SHAVE_Intrinsic<"vau_dp4_v16i8_rr", [ llvm_v4i32_ty ], [ llvm_v16i8_ty, llvm_v16i8_ty ]>;

foreach n = [ "0", "1" ] in {
  foreach z = [ "", "z" ] in {
    def int_shave_vau_dp4a#z#_v16i8_vacc#n#_rr      : SHAVE_Intrinsic<!strconcat("vau_dp4a", z, "_v16i8_vacc", n, "_rr"),
                                                                      [ llvm_v4i32_ty ],
                                                                      [ llvm_v16i8_ty, llvm_v16i8_ty ],
                                                                      []>; // Don't set IntrNoMem
    def int_shave_vau_dp4a#z#_v16i8_vacc#n#_void_rr : SHAVE_Intrinsic<!strconcat("vau_dp4a", z, "_v16i8_vacc", n, "_void_rr"),
                                                                      [],
                                                                      [ llvm_v16i8_ty, llvm_v16i8_ty ],
                                                                      []>; // Don't set IntrNoMem
  }
}

//
//  VAU.DP4AM
//    8-bit signed integer by 8-bit unsigned integer multiplication with sum to 32-bit signed integer and optional accumulate
//

def int_shave_vau_dp4m_v16i8_rr : SHAVE_Intrinsic<"vau_dp4m_v16i8_rr", [ llvm_v4i32_ty ], [ llvm_v16i8_ty, llvm_v16i8_ty ]>;

foreach n = [ "0", "1" ] in {
  foreach z = [ "", "z" ] in {
    def int_shave_vau_dp4am#z#_v16i8_vacc#n#_rr : SHAVE_Intrinsic<!strconcat("vau_dp4am", z, "_v16i8_vacc", n, "_rr"),
                                                                  [ llvm_v4i32_ty ],
                                                                  [ llvm_v16i8_ty, llvm_v16i8_ty ],
                                                                  []>; // Don't set IntrNoMem

    def int_shave_vau_dp4am#z#_v16i8_vacc#n#_void_rr : SHAVE_Intrinsic<!strconcat("vau_dp4am", z, "_v16i8_vacc", n, "_void_rr"),
                                                                       [],
                                                                       [ llvm_v16i8_ty, llvm_v16i8_ty ],
                                                                       []>; // Don't set IntrNoMem
  }
}

//===----------------------------------------------------------------------===//
// 64-bit VRF built-ins
//===----------------------------------------------------------------------===//

//
//  VAU.ABS
//    Long vector absolute value
//

def int_shave_vau_abs_v2f32_r : SHAVE_Intrinsic<"vau_abs_v2f32_r", [ llvm_v2f32_ty ], [ llvm_v2f32_ty ]>;
def int_shave_vau_abs_v4f16_r : SHAVE_Intrinsic<"vau_abs_v4f16_r", [ llvm_v4f16_ty ], [ llvm_v4f16_ty ]>;

//
//  VAU.ADD
//    Vector addition with optional immediate operand
//

foreach type = [ llvm_v2f32_ty, llvm_v2i32_ty, llvm_v4f16_ty, llvm_v4i16_ty, llvm_v8i8_ty ] in {
  defvar typeName = SHAVEType<type>.vectorString;
  def int_shave_vau_add_#typeName#_rr : SHAVE_Intrinsic<!strconcat("vau_add_", typeName, "_rr"), [ type ], [ type, type ]>;
}

def int_shave_vau_add_i32_ri_int2   : SHAVE_Intrinsic<"vau_add_i32_ri_int2",   [ llvm_v2i32_ty ], [ llvm_v2i32_ty, llvm_i8_ty ]>;
def int_shave_vau_add_i16_ri_short4 : SHAVE_Intrinsic<"vau_add_i16_ri_short4", [ llvm_v4i16_ty ], [ llvm_v4i16_ty, llvm_i8_ty ]>;
def int_shave_vau_add_i8_ri_schar8  : SHAVE_Intrinsic<"vau_add_i8_ri_schar8",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty, llvm_i8_ty ]>;

//
//  VAU.AND
//    Bitwise AND
//

def int_shave_vau_and_v2i32_rr : SHAVE_Intrinsic<"vau_and_v2i32_rr", [ llvm_v2i32_ty ], [ llvm_v2i32_ty, llvm_v2i32_ty ]>;
def int_shave_vau_and_v4i16_rr : SHAVE_Intrinsic<"vau_and_v4i16_rr", [ llvm_v4i16_ty ], [ llvm_v4i16_ty, llvm_v4i16_ty ]>;
def int_shave_vau_and_v8i8_rr  : SHAVE_Intrinsic<"vau_and_v8i8_rr",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty, llvm_v8i8_ty ]>;

//
//  VAU.MUL
//    Long vector multiplication with optional immediate operand
//

// Register-register variants
foreach type = [ llvm_v2f32_ty, llvm_v2i32_ty, llvm_v4f16_ty, llvm_v4i16_ty, llvm_v8i8_ty ] in {
  defvar typeName = SHAVEType<type>.vectorString;
  def int_shave_vau_mul_#typeName#_rr : SHAVE_Intrinsic<!strconcat("vau_mul_", typeName, "_rr"), [ type ], [ type, type ]>;
}

// Register-immediate variants
def int_shave_vau_mul_i32_ri_int2 : SHAVE_Intrinsic<"vau_mul_i32_ri_int2", [ llvm_v2i32_ty ], [ llvm_v2i32_ty,llvm_i8_ty ], [IntrNoMem]>;
def int_shave_vau_mul_i16_ri_short4 : SHAVE_Intrinsic<"vau_mul_i16_ri_short4", [ llvm_v4i16_ty ], [ llvm_v4i16_ty,llvm_i8_ty ], [IntrNoMem]>;
def int_shave_vau_mul_i8_ri_schar8 : SHAVE_Intrinsic<"vau_mul_i8_ri_schar8", [ llvm_v8i8_ty ], [ llvm_v8i8_ty,llvm_i8_ty ], [IntrNoMem]>;

//
//  VAU.NOT
//    Bitwise NOT
//

def int_shave_vau_not_v2i32_r : SHAVE_Intrinsic<"vau_not_v2i32_r", [ llvm_v2i32_ty ], [ llvm_v2i32_ty ]>;
def int_shave_vau_not_v4i16_r : SHAVE_Intrinsic<"vau_not_v4i16_r", [ llvm_v4i16_ty ], [ llvm_v4i16_ty ]>;
def int_shave_vau_not_v8i8_r  : SHAVE_Intrinsic<"vau_not_v8i8_r",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty ]>;

//
//  VAU.OR
//    Bitwise OR
//

def int_shave_vau_or_v2i32_rr : SHAVE_Intrinsic<"vau_or_v2i32_rr", [ llvm_v2i32_ty ], [ llvm_v2i32_ty, llvm_v2i32_ty ]>;
def int_shave_vau_or_v4i16_rr : SHAVE_Intrinsic<"vau_or_v4i16_rr", [ llvm_v4i16_ty ], [ llvm_v4i16_ty, llvm_v4i16_ty ]>;
def int_shave_vau_or_v8i8_rr  : SHAVE_Intrinsic<"vau_or_v8i8_rr",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty, llvm_v8i8_ty ]>;

//
//  VAU.ROL
//    Vector rotate left
//

// Register-register variants
def int_shave_vau_rol_v2x32_rr : SHAVE_Intrinsic<"vau_rol_v2x32_rr", [ llvm_v2i32_ty ], [ llvm_v2i32_ty, llvm_v2i32_ty ]>;
def int_shave_vau_rol_v4x16_rr : SHAVE_Intrinsic<"vau_rol_v4x16_rr", [ llvm_v4i16_ty ], [ llvm_v4i16_ty, llvm_v4i16_ty ]>;
def int_shave_vau_rol_v8x8_rr  : SHAVE_Intrinsic<"vau_rol_v8x8_rr",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty, llvm_v8i8_ty ]>;

// Register-immediate variants
def int_shave_vau_rol_x32_ri_uint2   : SHAVE_Intrinsic<"vau_rol_x32_ri_uint2",   [ llvm_v2i32_ty ], [ llvm_v2i32_ty, llvm_i8_ty ]>;
def int_shave_vau_rol_x16_ri_ushort4 : SHAVE_Intrinsic<"vau_rol_x16_ri_ushort4", [ llvm_v4i16_ty ], [ llvm_v4i16_ty, llvm_i8_ty ]>;
def int_shave_vau_rol_x8_ri_uchar8   : SHAVE_Intrinsic<"vau_rol_x8_ri_uchar8",   [ llvm_v8i8_ty ],  [ llvm_v8i8_ty, llvm_i8_ty ]>;

//
//  VAU.SHD
//    Long vector arithmetic shift right with set to zero on underflow. Optional immediate shift amount
//

// Register-register variants
def int_shave_vau_shd_v2i32_rr : SHAVE_Intrinsic<"vau_shd_v2i32_rr", [ llvm_v2i32_ty ], [ llvm_v2i32_ty, llvm_v2i32_ty ]>;
def int_shave_vau_shd_v4i16_rr : SHAVE_Intrinsic<"vau_shd_v4i16_rr", [ llvm_v4i16_ty ], [ llvm_v4i16_ty, llvm_v4i16_ty ]>;
def int_shave_vau_shd_v8i8_rr  : SHAVE_Intrinsic<"vau_shd_v8i8_rr",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty ,llvm_v8i8_ty ]>;

// Register-immediate variants
def int_shave_vau_shd_i32_ri_int2   : SHAVE_Intrinsic<"vau_shd_i32_ri_int2",   [ llvm_v2i32_ty ], [ llvm_v2i32_ty,llvm_i8_ty ]>;
def int_shave_vau_shd_i16_ri_short4 : SHAVE_Intrinsic<"vau_shd_i16_ri_short4", [ llvm_v4i16_ty ], [ llvm_v4i16_ty,llvm_i8_ty ]>;
def int_shave_vau_shd_i8_ri_schar8  : SHAVE_Intrinsic<"vau_shd_i8_ri_schar8",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty,llvm_i8_ty ]>;

//
//  VAU.SHL
//    Long vector logical shift left with optional immediate shift amount
//

// Register-register variants
def int_shave_vau_shl_v2x32_rr : SHAVE_Intrinsic<"vau_shl_v2x32_rr", [ llvm_v2i32_ty ], [ llvm_v2i32_ty, llvm_v2i32_ty ]>;
def int_shave_vau_shl_v4x16_rr : SHAVE_Intrinsic<"vau_shl_v4x16_rr", [ llvm_v4i16_ty ], [ llvm_v4i16_ty, llvm_v4i16_ty ]>;
def int_shave_vau_shl_v8x8_rr  : SHAVE_Intrinsic<"vau_shl_v8x8_rr",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty, llvm_v8i8_ty ]>;

// Register-immediate variants
def int_shave_vau_shl_x32_ri_int2   : SHAVE_Intrinsic<"vau_shl_x32_ri_int2",   [ llvm_v2i32_ty ], [ llvm_v2i32_ty, llvm_i8_ty ]>;
def int_shave_vau_shl_x16_ri_short4 : SHAVE_Intrinsic<"vau_shl_x16_ri_short4", [ llvm_v4i16_ty ], [ llvm_v4i16_ty, llvm_i8_ty ]>;
def int_shave_vau_shl_x8_ri_schar8  : SHAVE_Intrinsic<"vau_shl_x8_ri_schar8",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty, llvm_i8_ty ]>;

//
//  VAU.SHR
//    Long vector logical/arithmetic shift right with optional immediate shift amount
//

// Register-register variants
foreach type = [ llvm_v2i32_ty, llvm_v4i16_ty, llvm_v8i8_ty ] in {
  foreach typeString = [ SHAVEType<type>.vectorString, SHAVEType<type>.unsignedVectorString ] in {
    def int_shave_vau_shr_#typeString#_rr : SHAVE_Intrinsic<!strconcat("vau_shr_", typeString, "_rr"), [ type ], [ type, type ]>;
  }
}

// Register-immediate variants
def int_shave_vau_shr_i32_ri_int2   : SHAVE_Intrinsic<"vau_shr_i32_ri_int2",   [ llvm_v2i32_ty ], [ llvm_v2i32_ty, llvm_i8_ty ]>;
def int_shave_vau_shr_u32_ri_int2   : SHAVE_Intrinsic<"vau_shr_u32_ri_int2",   [ llvm_v2i32_ty ], [ llvm_v2i32_ty, llvm_i8_ty ]>;
def int_shave_vau_shr_i16_ri_short4 : SHAVE_Intrinsic<"vau_shr_i16_ri_short4", [ llvm_v4i16_ty ], [ llvm_v4i16_ty, llvm_i8_ty ]>;
def int_shave_vau_shr_u16_ri_short4 : SHAVE_Intrinsic<"vau_shr_u16_ri_short4", [ llvm_v4i16_ty ], [ llvm_v4i16_ty, llvm_i8_ty ]>;
def int_shave_vau_shr_i8_ri_schar8  : SHAVE_Intrinsic<"vau_shr_i8_ri_schar8",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty, llvm_i8_ty ]>;
def int_shave_vau_shr_u8_ri_schar8  : SHAVE_Intrinsic<"vau_shr_u8_ri_schar8",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty, llvm_i8_ty ]>;

//
//  VAU.SUB
//    Long vector subtraction with optional immediate operand
//

// Register-register variants
foreach type = [ llvm_v2f32_ty, llvm_v2i32_ty, llvm_v4f16_ty, llvm_v4i16_ty, llvm_v8i8_ty ] in {
  defvar typeString = SHAVEType<type>.vectorString;
  def int_shave_vau_sub_#typeString#_rr : SHAVE_Intrinsic<!strconcat("vau_sub_", typeString, "_rr"), [ type ], [ type, type ]>;
}

// Register-immediate variants
def int_shave_vau_sub_i32_ri_int2   : SHAVE_Intrinsic<"vau_sub_i32_ri_int2",   [ llvm_v2i32_ty ], [ llvm_v2i32_ty ,llvm_i8_ty ]>;
def int_shave_vau_sub_i16_ri_short4 : SHAVE_Intrinsic<"vau_sub_i16_ri_short4", [ llvm_v4i16_ty ], [ llvm_v4i16_ty, llvm_i8_ty ]>;
def int_shave_vau_sub_i8_ri_schar8  : SHAVE_Intrinsic<"vau_sub_i8_ri_schar8",  [ llvm_v8i8_ty ],  [ llvm_v8i8_ty, llvm_i8_ty ]>;

//
//  VAU.XOR
//    Bitwise XOR
//

def int_shave_vau_xor_v2i32_rr : SHAVE_Intrinsic<"vau_xor_v2i32_rr", [ llvm_v2i32_ty ], [ llvm_v2i32_ty,llvm_v2i32_ty ], [IntrNoMem]>;
def int_shave_vau_xor_v4i16_rr : SHAVE_Intrinsic<"vau_xor_v4i16_rr", [ llvm_v4i16_ty ], [ llvm_v4i16_ty,llvm_v4i16_ty ], [IntrNoMem]>;
def int_shave_vau_xor_v8i8_rr : SHAVE_Intrinsic<"vau_xor_v8i8_rr", [ llvm_v8i8_ty ], [ llvm_v8i8_ty,llvm_v8i8_ty ], [IntrNoMem]>;

//===----------------------------------------------------------------------===//
// 512-bit VRF built-ins
//===----------------------------------------------------------------------===//

//
//  VAU.ABS
//    Long vector absolute value
//

foreach type = [ llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty, llvm_v16f32_ty, llvm_v32f16_ty ] in {
  defvar name = !strconcat("vau_abs_",  SHAVEType<type>.scalarString, "_r_",  SHAVEType<type>.cName);
  def int_shave_#name : SHAVE_Intrinsic<name, [ type ], [ type ]>;
}

//
//  VAU.ADD
//    Vector addition with optional immediate operand
//

foreach type = [ llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty, llvm_v16f32_ty, llvm_v32f16_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_add",
                                      type,
                                      0, // hasUnsigned
                                      1, // hasImmediateVariant
                                      1>; // hasSuffix
}

//
//  VAU.ADIFF
//    Long vector absolute difference
//

foreach type = [ llvm_v16f32_ty, llvm_v32f16_ty, llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_adiff",
                                      type,
                                      1, // hasUnsigned
                                      0, // hasImmediateVariant
                                      1>; // hasSuffix
}

//
//  VAU.ALIGNBYTE
//    Long vector align bytes within words
//

foreach type = [ llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defvar name = !strconcat("vau_alignbyte_rri_", SHAVEType<type>.cName);
  def int_shave_#name : SHAVE_Intrinsic<name, [ type ], [ type, type, llvm_i8_ty ]>;
}

//
//  VAU.AND
//    Bitwise AND
//

defm int_shave_vau_and_32_rr : SHAVE_SuffixedIntrinsic<"vau_and_32_rr", [ llvm_v16i32_ty ], [ llvm_v16i32_ty, llvm_v16i32_ty ]>;
defm int_shave_vau_and_16_rr : SHAVE_SuffixedIntrinsic<"vau_and_16_rr", [ llvm_v32i16_ty ], [ llvm_v32i16_ty, llvm_v32i16_ty ]>;
defm int_shave_vau_and_8_rr  : SHAVE_SuffixedIntrinsic<"vau_and_8_rr",  [ llvm_v64i8_ty ],  [ llvm_v64i8_ty, llvm_v64i8_ty ]>;

//
//  VAU.AVG
//    Integer average
//

def int_shave_vau_avg_x32_rr_uint16   : SHAVE_Intrinsic<"vau_avg_x32_rr_uint16",   [ llvm_v16i32_ty ], [ llvm_v16i32_ty, llvm_v16i32_ty ]>;
def int_shave_vau_avg_x16_rr_ushort32 : SHAVE_Intrinsic<"vau_avg_x16_rr_ushort32", [ llvm_v32i16_ty ], [ llvm_v32i16_ty, llvm_v32i16_ty ]>;
def int_shave_vau_avg_x8_rr_uchar64   : SHAVE_Intrinsic<"vau_avg_x8_rr_uchar64",   [ llvm_v64i8_ty ],  [ llvm_v64i8_ty, llvm_v64i8_ty ]>;

//
//  VAU.IADDS
//    Long vector integer addition with saturation. Optional unsigned immediate operand
//

foreach type = [ llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_iadds",
                                      type,
                                      1, // hasUnsigned
                                      1, // hasImmediateVariant
                                      1>; // hasSuffix
}

//
//  VAU.ILMULH
//    Long vector multiply returning higher-order result
//

foreach type = [ llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_ilmulh",
                                      type,
                                      1, // hasUnsigned
                                      0, // hasImmediateVariant
                                      1>; // hasSuffix
}

//
//  VAU.IMULS
//    Long vector integer multiplication with saturation. Optional immediate operand
//

foreach type = [ llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_imuls",
                                      type,
                                      1, // hasUnsigned
                                      1, // hasImmediateVariant
                                      1>; // hasSuffix
}

//
//  VAU.ISUBS
//    Long vector integer subtraction with saturation. Optional unsigned immediate operand
//

foreach type = [ llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_isubs",
                                      type,
                                      1, // hasUnsigned
                                      1, // hasImmediateVariant
                                      1>; // hasSuffix
}

//
//  VAU.MUL
//    Long vector multiplication with optional immediate operand
//

foreach type = [ llvm_v16f32_ty, llvm_v16i32_ty, llvm_v32f16_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_mul",
                                      type,
                                      0, // hasUnsigned
                                      1, // hasImmediateVariant
                                      1>; // hasSuffix
}

//
//  VAU.NOT
//    Bitwise NOT
//

defm int_shave_vau_not_i32_r : SHAVE_SuffixedIntrinsic<"vau_not_i32_r", [ llvm_v16i32_ty ], [ llvm_v16i32_ty ]>;
defm int_shave_vau_not_i16_r : SHAVE_SuffixedIntrinsic<"vau_not_i16_r", [ llvm_v32i16_ty ], [ llvm_v32i16_ty ]>;
defm int_shave_vau_not_i8_r  : SHAVE_SuffixedIntrinsic<"vau_not_i8_r",  [ llvm_v64i8_ty ],  [ llvm_v64i8_ty ]>;

//
//  VAU.OR
//    Bitwise OR
//

defm int_shave_vau_or_32_rr : SHAVE_SuffixedIntrinsic<"vau_or_32_rr", [ llvm_v16i32_ty ], [ llvm_v16i32_ty,llvm_v16i32_ty ]>;
defm int_shave_vau_or_16_rr : SHAVE_SuffixedIntrinsic<"vau_or_16_rr", [ llvm_v32i16_ty ], [ llvm_v32i16_ty,llvm_v32i16_ty ]>;
defm int_shave_vau_or_8_rr  : SHAVE_SuffixedIntrinsic<"vau_or_8_rr",  [ llvm_v64i8_ty ],  [ llvm_v64i8_ty,llvm_v64i8_ty ]>;

//
//  VAU.ROL
//    Vector rotate left
//

// Register-register variants
defm int_shave_vau_rol_x32_rr : SHAVE_SuffixedIntrinsic<"vau_rol_x32_rr", [ llvm_v16i32_ty ], [ llvm_v16i32_ty, llvm_v16i32_ty ]>;
defm int_shave_vau_rol_x16_rr : SHAVE_SuffixedIntrinsic<"vau_rol_x16_rr", [ llvm_v32i16_ty ], [ llvm_v32i16_ty, llvm_v32i16_ty ]>;
defm int_shave_vau_rol_x8_rr  : SHAVE_SuffixedIntrinsic<"vau_rol_x8_rr",  [ llvm_v64i8_ty ],  [ llvm_v64i8_ty, llvm_v64i8_ty ]>;

// Register-immediate variants
defm int_shave_vau_rol_x32_ri : SHAVE_SuffixedIntrinsic<"vau_rol_x32_ri", [ llvm_v16i32_ty ], [ llvm_v16i32_ty, llvm_i8_ty ]>;
defm int_shave_vau_rol_x16_ri : SHAVE_SuffixedIntrinsic<"vau_rol_x16_ri", [ llvm_v32i16_ty ], [ llvm_v32i16_ty, llvm_i8_ty ]>;
defm int_shave_vau_rol_x8_ri  : SHAVE_SuffixedIntrinsic<"vau_rol_x8_ri",  [ llvm_v64i8_ty ],  [ llvm_v64i8_ty, llvm_i8_ty ]>;

//
//  VAU.SHD
//    Long vector arithmetic shift right with set to zero on underflow. Optional immediate shift amount
//

// Register-register variants
defm int_shave_vau_shd_i32_rr : SHAVE_SuffixedIntrinsic<"vau_shd_i32_rr", [ llvm_v16i32_ty ], [ llvm_v16i32_ty, llvm_v16i32_ty ]>;
defm int_shave_vau_shd_i16_rr : SHAVE_SuffixedIntrinsic<"vau_shd_i16_rr", [ llvm_v32i16_ty ], [ llvm_v32i16_ty, llvm_v32i16_ty ]>;
defm int_shave_vau_shd_i8_rr  : SHAVE_SuffixedIntrinsic<"vau_shd_i8_rr",  [ llvm_v64i8_ty ],  [ llvm_v64i8_ty, llvm_v64i8_ty ]>;

// Register-immediate variants
defm int_shave_vau_shd_i32_ri : SHAVE_SuffixedIntrinsic<"vau_shd_i32_ri", [ llvm_v16i32_ty ], [ llvm_v16i32_ty, llvm_i8_ty ]>;
defm int_shave_vau_shd_i16_ri : SHAVE_SuffixedIntrinsic<"vau_shd_i16_ri", [ llvm_v32i16_ty ], [ llvm_v32i16_ty, llvm_i8_ty ]>;
defm int_shave_vau_shd_i8_ri  : SHAVE_SuffixedIntrinsic<"vau_shd_i8_ri",  [ llvm_v64i8_ty ],  [ llvm_v64i8_ty, llvm_i8_ty ]>;

//
//  VAU.SHL
//    Long vector logical shift left with optional immediate shift amount
//

// Register-register variants
def int_shave_vau_shl_x32_rr_uint16   : SHAVE_Intrinsic<"vau_shl_x32_rr_uint16",   [ llvm_v16i32_ty ], [ llvm_v16i32_ty, llvm_v16i32_ty ]>;
def int_shave_vau_shl_x16_rr_ushort32 : SHAVE_Intrinsic<"vau_shl_x16_rr_ushort32", [ llvm_v32i16_ty ], [ llvm_v32i16_ty, llvm_v32i16_ty ]>;
def int_shave_vau_shl_x8_rr_uchar64   : SHAVE_Intrinsic<"vau_shl_x8_rr_uchar64",   [ llvm_v64i8_ty ],  [ llvm_v64i8_ty, llvm_v64i8_ty ]>;

// Register-immediate variants
def int_shave_vau_shl_x32_ri_uint16   : SHAVE_Intrinsic<"vau_shl_x32_ri_uint16",   [ llvm_v16i32_ty ], [ llvm_v16i32_ty, llvm_i8_ty ]>;
def int_shave_vau_shl_x16_ri_ushort32 : SHAVE_Intrinsic<"vau_shl_x16_ri_ushort32", [ llvm_v32i16_ty ], [ llvm_v32i16_ty, llvm_i8_ty ]>;
def int_shave_vau_shl_x8_ri_uchar64   : SHAVE_Intrinsic<"vau_shl_x8_ri_uchar64",   [ llvm_v64i8_ty ],  [ llvm_v64i8_ty, llvm_i8_ty ]>;


//
//  VAU.SHR
//    Long vector logical/arithmetic shift right with optional immediate shift amount
//

foreach type = [ llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_shr",
                                      type,
                                      1, // hasUnsigned
                                      1, // hasImmediateVariant
                                      1>; // hasSuffix
}

//
//  VAU.SUB
//    Long vector subtraction with optional immediate operand
//

foreach type = [ llvm_v16f32_ty, llvm_v16i32_ty, llvm_v32f16_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defm int_shave : SHAVE_VAUIntrinsic<"vau_sub",
                                      type,
                                      0, // hasUnsigned
                                      1, // hasImmediateVariant
                                      1>; // hasSuffix
}

//
//  VAU.XOR
//    Bitwise XOR
//

defm int_shave_vau_xor_32_rr : SHAVE_SuffixedIntrinsic<"vau_xor_32_rr", [ llvm_v16i32_ty ], [ llvm_v16i32_ty, llvm_v16i32_ty ]>;
defm int_shave_vau_xor_16_rr : SHAVE_SuffixedIntrinsic<"vau_xor_16_rr", [ llvm_v32i16_ty ], [ llvm_v32i16_ty, llvm_v32i16_ty ]>;
defm int_shave_vau_xor_8_rr  : SHAVE_SuffixedIntrinsic<"vau_xor_8_rr",  [ llvm_v64i8_ty ],  [ llvm_v64i8_ty, llvm_v64i8_ty ]>;

//
// VAU 512-bit Transcendentals
//

foreach name = [ "tanh", "sigm", "log", "exp", "sqt" ] in {
  def int_shave_vau_#name#_r_half32 : SHAVE_Intrinsic<!strconcat("vau_", name, "_r_half32"), [ llvm_v32f16_ty ], [ llvm_v32f16_ty ]>;
}


//
//  VAU.DP4A
//    8-bit signed integer multiplication with sum to 32-bit signed integer and optional accumulate
//

def int_shave_vau_dp4_v64i8_rr : SHAVE_Intrinsic<"vau_dp4_v64i8_rr", [ llvm_v16i32_ty ], [ llvm_v64i8_ty, llvm_v64i8_ty ]>;

foreach n = [ "0", "1" ] in {
  foreach z = [ "", "z" ] in {
    def int_shave_vau_dp4a#z#_v64i8_vacc#n#_rr      : SHAVE_Intrinsic<!strconcat("vau_dp4a", z, "_v64i8_vacc", n, "_rr"),
                                                                      [ llvm_v16i32_ty ],
                                                                      [ llvm_v64i8_ty, llvm_v64i8_ty ],
                                                                      []>; // Don't set IntrNoMem
    def int_shave_vau_dp4a#z#_v64i8_vacc#n#_void_rr : SHAVE_Intrinsic<!strconcat("vau_dp4a", z, "_v64i8_vacc", n, "_void_rr"),
                                                                      [],
                                                                      [ llvm_v64i8_ty, llvm_v64i8_ty ],
                                                                      []>; // Don't set IntrNoMem
  }
}

//
//  VAU.DP4AM
//    8-bit signed integer by 8-bit unsigned integer multiplication with sum to 32-bit signed integer and optional accumulate
//

def int_shave_vau_dp4m_v64i8_rr : SHAVE_Intrinsic<"vau_dp4m_v64i8_rr", [ llvm_v16i32_ty ], [ llvm_v64i8_ty, llvm_v64i8_ty ]>;

foreach n = [ "0", "1" ] in {
  foreach z = [ "", "z" ] in {
    def int_shave_vau_dp4am#z#_v64i8_vacc#n#_rr : SHAVE_Intrinsic<!strconcat("vau_dp4am", z, "_v64i8_vacc", n, "_rr"),
                                                                  [ llvm_v16i32_ty ],
                                                                  [ llvm_v64i8_ty, llvm_v64i8_ty ],
                                                                  []>; // Don't set IntrNoMem

    def int_shave_vau_dp4am#z#_v64i8_vacc#n#_void_rr : SHAVE_Intrinsic<!strconcat("vau_dp4am", z, "_v64i8_vacc", n, "_void_rr"),
                                                                       [],
                                                                       [ llvm_v64i8_ty, llvm_v64i8_ty ],
                                                                       []>; // Don't set IntrNoMem
  }
}
