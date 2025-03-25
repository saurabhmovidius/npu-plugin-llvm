//===- IntrinsicsShaveSAU.td - Defines Shave intrinsics ----*- tablegen -*-===//
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

multiclass SHAVE_SAUIntrinsic<string opcode,
                              LLVMType type,
                              bit hasUnsigned = 1> {
  defvar shaveType = SHAVEType<type>;

  def _#opcode#_#shaveType.typeString#_rr :
    SHAVE_Intrinsic<!strconcat(opcode, "_", shaveType.typeString, "_rr"),
                    [ type ],
                    [ type, type ]>;

  // Add an unsigned variant for integer types only
  if !and(hasUnsigned, type.VT.isInteger) then {
    def _#opcode#_#shaveType.unsignedTypeString#_rr :
      SHAVE_Intrinsic<!strconcat(opcode, "_", shaveType.unsignedTypeString, "_rr"),
                      [ type ],
                      [ type, type ]>;
  }
}

multiclass SAU_ONES_ZEROS_Intrinsic<LLVMType type,
                                    bit laneSelect = 0> {
  defvar suffix = !cond(!eq(type.VT.ElementType.Value, i32.Value) : "x32",
                        !eq(type.VT.ElementType.Value, i16.Value) : "x64",
                        true                                      : "x128");

  defvar operands = !cond(laneSelect : "ri", true : "r");
  defvar params = !cond(laneSelect : [ type, llvm_i8_ty ], true : [ type ]);

  def _sau_onesx_#suffix#_#operands  : SHAVE_Intrinsic<!strconcat("sau_onesx_", suffix, "_", operands), [ llvm_i32_ty ], params>;
  def _sau_zerosx_#suffix#_#operands : SHAVE_Intrinsic<!strconcat("sau_zerosx_", suffix, "_r", operands), [ llvm_i32_ty ], params>;
}

multiclass SAU_SUMX_Intrinsic<LLVMType type,
                              string isuffix = "",
                              string usuffix = ""> {
  defvar shaveType = SHAVEType<type>;

  defvar returnType = !cond(
    type.VT.isInteger : llvm_i32_ty,
    true              : LLVMType<shaveType.scalarType>
  );

  // Special-case for small vectors, the type string is the full vector name
  defvar isSmallVector = !and(type.VT.isVector, !le(type.VT.Size, 32));
  defvar typeString = !cond(
    isSmallVector : shaveType.vectorString,
    true          : shaveType.scalarString
  );

  def _sau_sumx_#typeString#_r#isuffix :
    SHAVE_Intrinsic<!strconcat("sumx_", typeString, "_r", isuffix), [ returnType ], [ type ]>;

  if type.VT.isInteger then {
    defvar unsignedTypeString = !cond(
      isSmallVector : shaveType.unsignedVectorString,
      true          : shaveType.unsignedScalarString
    );
    def _sau_sumx_#unsignedTypeString#_r#usuffix :
      SHAVE_Intrinsic<!strconcat("sumx_", unsignedTypeString, "_r", usuffix), [ returnType ], [ type ]>;
  }
}

//===----------------------------------------------------------------------===//
// 8/16/32-bit IRF built-ins
//===----------------------------------------------------------------------===//

//
//  SAU.ABS
//    Short vector/scalar absolute value
//

foreach type = [ llvm_i32_ty, llvm_i16_ty, llvm_i8_ty, llvm_float_ty, llvm_half_ty ] in {
  defvar shaveType = SHAVEType<type>;
  def int_shave_sau_abs_#shaveType.typeString#_r : SHAVE_Intrinsic<!strconcat("sau_abs_", shaveType.typeString, "_r"), [ type ], [ type ]>;
}

//
//  SAU.ADIFF
//    Short vector/scalar absolute difference
//

foreach type = [ llvm_i32_ty, llvm_i16_ty, llvm_i8_ty, llvm_float_ty, llvm_half_ty ] in {
  defm int_shave : SHAVE_SAUIntrinsic<"sau_adiff", type>;
}

//
//  SAU.DIV
//    Short vector/scalar division
//

foreach type = [ llvm_i32_ty, llvm_i16_ty, llvm_float_ty ] in {
  defm int_shave : SHAVE_SAUIntrinsic<"sau_div", type>;
}

//
//  SAU.FRAC
//    Fractional part of floating-point number
//

def int_shave_sau_frac_f32_r : SHAVE_Intrinsic<"sau_frac_f32_r", [ llvm_float_ty ], [ llvm_float_ty ]>;
def int_shave_sau_frac_f16_r : SHAVE_Intrinsic<"sau_frac_f16_r", [ llvm_half_ty ],  [ llvm_half_ty ]>;

// Built-ins with 32-bit, 16-bit and 8-bit integer variants
foreach type = [ llvm_i32_ty, llvm_i16_ty, llvm_i8_ty ] in {
  //
  //  SAU.IADDS
  //    Short vector/scalar integer addition with saturation. Optional unsigned immediate operand
  //

  defm int_shave : SHAVE_SAUIntrinsic<"sau_iadds", type>;

  //
  //  SAU.ILMULH
  //    Short vector/scalar multiple returning higher-order result
  //

  defm int_shave : SHAVE_SAUIntrinsic<"sau_ilmulh", type>;

  //
  //  SAU.IMULS
  //    Short vector/scalar integer multiplication with saturation. Optional immediate operand
  //

  defm int_shave : SHAVE_SAUIntrinsic<"sau_imuls", type>;

  //
  //  SAU.ISUBS
  //    Short vector/scalar integer subtraction with saturation. Optional unsigned immediate operand
  //

  defm int_shave : SHAVE_SAUIntrinsic<"sau_isubs", type>;
}

//
//  SAU.SHR
//    Short vector/scalar logical/arithmetic shift right with optional shift amount
//

// Register-register variants
foreach type = [ llvm_i32_ty, llvm_i16_ty, llvm_i8_ty, llvm_v2i16_ty, llvm_v4i8_ty ] in {
  defm int_shave : SHAVE_SAUIntrinsic<"sau_shr", type>;
}

// Register-immediate scalar variants
foreach type = [ llvm_i32_ty, llvm_i16_ty, llvm_i8_ty ] in {
  defvar shaveType = SHAVEType<type>;

  def int_shave_sau_shr_#shaveType.scalarString#_ri :
    SHAVE_Intrinsic<!strconcat("sau_shr_", shaveType.scalarString, "_ri"), [ type ], [ type, llvm_i8_ty ]>;

  def int_shave_sau_shr_#shaveType.unsignedTypeString#_ri:
    SHAVE_Intrinsic<!strconcat("sau_shr_", shaveType.unsignedTypeString, "_ri"), [ type ], [ type, llvm_i8_ty ]>;
}

// Register-immediate vector variants
def int_shave_sau_shr_v2i16_ri : SHAVE_Intrinsic<"sau_shr_v2i16_ri", [ llvm_v2i16_ty ], [ llvm_v2i16_ty, llvm_i8_ty ]>;
def int_shave_sau_shr_v2u16_ri : SHAVE_Intrinsic<"sau_shr_v2u16_ri", [ llvm_v2i16_ty ], [ llvm_v2i16_ty, llvm_i8_ty ]>;
def int_shave_sau_shr_v4i8_ri  : SHAVE_Intrinsic<"sau_shr_v4i8_ri",  [ llvm_v4i8_ty ],  [ llvm_v4i8_ty, llvm_i8_ty ]>;
def int_shave_sau_shr_v4u8_ri  : SHAVE_Intrinsic<"sau_shr_v4u8_ri",  [ llvm_v4i8_ty ],  [ llvm_v4i8_ty, llvm_i8_ty ]>;

//
//  SAU.RCP
//    FP16 reciprocal
//

def int_shave_sau_rcp_f16_l_r : SHAVE_Intrinsic<"sau_rcp_f16_l_r", [ llvm_half_ty ], [ llvm_half_ty ]>;

//
//  SAU.ROL
//    Scalar rotate left with optional immediate rotate amount
//

// Register-register variants
def int_shave_sau_rol_x32_rr : SHAVE_Intrinsic<"sau_rol_x32_rr", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i32_ty ]>;
def int_shave_sau_rol_x16_rr : SHAVE_Intrinsic<"sau_rol_x16_rr", [ llvm_i16_ty ], [ llvm_i16_ty, llvm_i16_ty ]>;
def int_shave_sau_rol_x8_rr  : SHAVE_Intrinsic<"sau_rol_x8_rr",  [ llvm_i8_ty ],  [ llvm_i8_ty, llvm_i8_ty ]>;

// Register-immediate variants
def int_shave_sau_rol_x32_ri : SHAVE_Intrinsic<"sau_rol_x32_ri", [ llvm_i32_ty ], [ llvm_i32_ty, llvm_i8_ty ]>;
def int_shave_sau_rol_x16_ri : SHAVE_Intrinsic<"sau_rol_x16_ri", [ llvm_i16_ty ], [ llvm_i16_ty, llvm_i8_ty ]>;
def int_shave_sau_rol_x8_ri  : SHAVE_Intrinsic<"sau_rol_x8_ri",  [ llvm_i8_ty ],  [ llvm_i8_ty, llvm_i8_ty ]>;

//
//  SAU.SHL
//    Short vector/scalar logical shift left with optional immediate shift amount
//

def int_shave_sau_shl_x32_rr   : SHAVE_Intrinsic<"sau_shl_x32_rr",   [ llvm_i32_ty ],   [ llvm_i32_ty, llvm_i32_ty ]>;
def int_shave_sau_shl_x16_rr   : SHAVE_Intrinsic<"sau_shl_x16_rr",   [ llvm_i16_ty ],   [ llvm_i16_ty, llvm_i16_ty ]>;
def int_shave_sau_shl_x8_rr    : SHAVE_Intrinsic<"sau_shl_x8_rr",    [ llvm_i8_ty ],    [ llvm_i8_ty, llvm_i8_ty ]>;
def int_shave_sau_shl_v2x16_rr : SHAVE_Intrinsic<"sau_shl_v2x16_rr", [ llvm_v2i16_ty ], [ llvm_v2i16_ty, llvm_v2i16_ty ]>;
def int_shave_sau_shl_v4x8_rr  : SHAVE_Intrinsic<"sau_shl_v4x8_rr",  [ llvm_v4i8_ty ],  [ llvm_v4i8_ty, llvm_v4i8_ty ]>;
def int_shave_sau_shl_v2x16_ri : SHAVE_Intrinsic<"sau_shl_v2x16_ri", [ llvm_v2i16_ty ], [ llvm_v2i16_ty, llvm_i8_ty ]>;
def int_shave_sau_shl_v4x8_ri  : SHAVE_Intrinsic<"sau_shl_v4x8_ri",  [ llvm_v4i8_ty ],  [ llvm_v4i8_ty, llvm_i8_ty ]>;

//
//  SAU.DP4A
//    8-bit signed integer multiplication with sum to 32-bit signed integer and optional accumulate
//

def int_shave_sau_dp4_v4i8_rr : SHAVE_Intrinsic<"sau_dp4_v4i8_rr", [ llvm_i32_ty ], [ llvm_v4i8_ty, llvm_v4i8_ty ]>;

foreach n = [ "0", "1" ] in {
  foreach z = [ "", "z" ] in {
    def int_shave_sau_dp4a#z#_v4i8_sacc#n#_rr      : SHAVE_Intrinsic<!strconcat("sau_dp4a", z, "_v4i8_sacc", n, "_rr"),
                                                                     [ llvm_i32_ty ],
                                                                     [ llvm_v4i8_ty, llvm_v4i8_ty ],
                                                                     []>; // Don't set IntrNoMem
    def int_shave_sau_dp4a#z#_v4i8_sacc#n#_void_rr : SHAVE_Intrinsic<!strconcat("sau_dp4a", z, "_v4i8_sacc", n, "_void_rr"),
                                                                     [],
                                                                     [ llvm_v4i8_ty, llvm_v4i8_ty ],
                                                                     []>; // Don't set IntrNoMem
  }
}

//
//  SAU.DP4AM
//    8-bit signed integer by 8-bit unsigned integer multiplication with sum to 32-bit signed integer and optional accumulate
//

def int_shave_sau_dp4m_v4i8_rr : SHAVE_Intrinsic<"sau_dp4m_v4i8_rr", [ llvm_i32_ty ], [ llvm_v4i8_ty,llvm_v4i8_ty ], [IntrNoMem]>;

foreach n = [ "0", "1" ] in {
  foreach z = [ "", "z" ] in {
    def int_shave_sau_dp4am#z#_v4i8_sacc#n#_rr : SHAVE_Intrinsic<!strconcat("sau_dp4am", z, "_v4i8_sacc", n, "_rr"),
                                                                 [ llvm_i32_ty ],
                                                                 [ llvm_v4i8_ty, llvm_v4i8_ty ],
                                                                 []>; // Don't set IntrNoMem

    def int_shave_sau_dp4am#z#_v4i8_sacc#n#_void_rr : SHAVE_Intrinsic<!strconcat("sau_dp4am", z, "_v4i8_sacc", n, "_void_rr"),
                                                                      [],
                                                                      [ llvm_v4i8_ty, llvm_v4i8_ty ],
                                                                      []>; // Don't set IntrNoMem
  }
}

//===----------------------------------------------------------------------===//
// 16/32-bit IRF Transcendental built-ins
//===----------------------------------------------------------------------===//

//
//  SAU.ATN
//    FP16 arctangent
//

def int_shave_sau_atn_f16_l_r : SHAVE_Intrinsic<"sau_atn_f16_l_r", [ llvm_half_ty ], [ llvm_half_ty ]>;

//
//  SAU.COS
//    FP16 cosine
//

def int_shave_sau_cos_f16_l_r : SHAVE_Intrinsic<"sau_cos_f16_l_r", [ llvm_half_ty ], [ llvm_half_ty ]>;

//
//  SAU.EXP
//    FP16 exponential function
//

def int_shave_sau_exp_f16_r   : SHAVE_Intrinsic<"sau_exp_f16_r",   [ llvm_half_ty ], [ llvm_half_ty ]>;
def int_shave_sau_exp_f16_l_r : SHAVE_Intrinsic<"sau_exp_f16_l_r", [ llvm_half_ty ], [ llvm_half_ty ]>;

//
//  SAU.EXP2
//    FP16 base-2 exponentiation
//

def int_shave_sau_exp2_f16_l_r : SHAVE_Intrinsic<"sau_exp2_f16_l_r", [ llvm_half_ty ], [ llvm_half_ty ]>;
def int_shave_sau_exp2_f16_ri  : SHAVE_Intrinsic<"sau_exp2_f16_ri",  [ llvm_half_ty ], [ llvm_v8f16_ty, llvm_i8_ty ]>;

//
//  SAU.LOG
//    FP16 natural logarithm
//

def int_shave_sau_log_f16_r   : SHAVE_Intrinsic<"sau_log_f16_r",   [ llvm_half_ty ], [ llvm_half_ty ]>;
def int_shave_sau_log_f16_l_r : SHAVE_Intrinsic<"sau_log_f16_l_r", [ llvm_half_ty ], [ llvm_half_ty ]>;

//
// SAU.LOG2
//    FP16 base-2 logarithm
//

def int_shave_sau_log2_f16_l_r : SHAVE_Intrinsic<"sau_log2_f16_l_r", [ llvm_half_ty ], [ llvm_half_ty ]>;

//
//  SAU.RQT
//    FP16 reciprocal square root
//

def int_shave_sau_rqt_f16_l_r : SHAVE_Intrinsic<"sau_rqt_f16_l_r", [ llvm_half_ty ], [ llvm_half_ty ]>;

//
//  SAU.SIN
//    FP16 sine
//

def int_shave_sau_sin_f16_l_r : SHAVE_Intrinsic<"sau_sin_f16_l_r", [ llvm_half_ty ], [ llvm_half_ty ]>;

//
//  SAU.SQT
//    FP16 square root
//

def int_shave_sau_sqt_f16_l_r : SHAVE_Intrinsic<"sau_sqt_f16_l_r", [ llvm_half_ty ], [ llvm_half_ty ]>;
def int_shave_sau_sqt_f16_ri  : SHAVE_Intrinsic<"sau_sqt_f16_ri",  [ llvm_half_ty ], [ llvm_v8f16_ty, llvm_i8_ty ]>;

//
//  SAU.TANH
//    FP16 hyperbolic tangent
//

def int_shave_sau_tanh_f16_l_r : SHAVE_Intrinsic<"sau_tanh_f16_l_r", [ llvm_half_ty ], [ llvm_half_ty ]>;
def int_shave_sau_tanh_f16_ri  : SHAVE_Intrinsic<"sau_tanh_f16_ri",  [ llvm_half_ty ], [ llvm_v8f16_ty, llvm_i8_ty ]>;

//===----------------------------------------------------------------------===//
// 128-bit VRF built-ins
//===----------------------------------------------------------------------===//

//
//  SAU.ANDX
//    Element-wise AND vector elements
//

def int_shave_sau_andx_x32_r : SHAVE_Intrinsic<"sau_andx_x32_r", [ llvm_i32_ty ], [ llvm_v4i32_ty ]>;
def int_shave_sau_andx_x16_r : SHAVE_Intrinsic<"sau_andx_x16_r", [ llvm_i16_ty ], [ llvm_v8i16_ty ]>;
def int_shave_sau_andx_x8_r  : SHAVE_Intrinsic<"sau_andx_x8_r",  [ llvm_i8_ty ],  [ llvm_v16i8_ty ]>;

//
//  SAU.ONESX
//    Long vector ones or zeroes count reduction
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty ] in {
  defm int_shave : SAU_ONES_ZEROS_Intrinsic<type>;
}

//
//  SAU.ORX
//    Element-wise OR of vector
//

def int_shave_sau_orx_x32_r : SHAVE_Intrinsic<"sau_orx_x32_r", [ llvm_i32_ty ], [ llvm_v4i32_ty ]>;
def int_shave_sau_orx_x16_r : SHAVE_Intrinsic<"sau_orx_x16_r", [ llvm_i16_ty ], [ llvm_v8i16_ty ]>;
def int_shave_sau_orx_x8_r  : SHAVE_Intrinsic<"sau_orx_x8_r", [ llvm_i8_ty ], [ llvm_v16i8_ty ]>;

//
//  SAU.SUMX
//    Sum vector elements to a scalar
//

foreach type = [ llvm_v4i32_ty, llvm_v8i16_ty, llvm_v16i8_ty, llvm_v4i8_ty, llvm_v4f32_ty, llvm_v8f16_ty ] in {
  defm int_shave : SAU_SUMX_Intrinsic<type>;
}

//
//  SAU.XORX
//    Element-wise XOR of vector
//

def int_shave_sau_xorx_x32_r : SHAVE_Intrinsic<"sau_xorx_x32_r", [ llvm_i32_ty ], [ llvm_v4i32_ty ]>;
def int_shave_sau_xorx_x16_r : SHAVE_Intrinsic<"sau_xorx_x16_r", [ llvm_i16_ty ], [ llvm_v8i16_ty ]>;
def int_shave_sau_xorx_x8_r  : SHAVE_Intrinsic<"sau_xorx_x8_r",  [ llvm_i8_ty ],  [ llvm_v16i8_ty ]>;

//===----------------------------------------------------------------------===//
// 512-bit VRF built-ins
//===----------------------------------------------------------------------===//

//
//  SAU.ANDX
//    Element-wise AND vector elements
//
defm int_shave_sau_andx_x32_r : SHAVE_SuffixedIntrinsic<"sau_andx_x32_r", [ llvm_i32_ty ], [ llvm_v16i32_ty ], 0, llvm_v16i32_ty>;
defm int_shave_sau_andx_x16_r : SHAVE_SuffixedIntrinsic<"sau_andx_x16_r", [ llvm_i16_ty ], [ llvm_v32i16_ty ], 0, llvm_v32i16_ty>;
defm int_shave_sau_andx_x8_r  : SHAVE_SuffixedIntrinsic<"sau_andx_x8_r",  [ llvm_i8_ty ],  [ llvm_v64i8_ty ], 0, llvm_v64i8_ty>;

//
//  SAU.ONESX
//    Long vector ones or zeroes count reduction
//

foreach type = [ llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defm int_shave : SAU_ONES_ZEROS_Intrinsic<type, 1>;
}

//
//  SAU.ORX
//    Element-wise OR of vector
//

defm int_shave_sau_orx_x32_r : SHAVE_SuffixedIntrinsic<"sau_orx_x32_r", [ llvm_i32_ty ], [ llvm_v16i32_ty ], 0, llvm_v16i32_ty>;
defm int_shave_sau_orx_x16_r : SHAVE_SuffixedIntrinsic<"sau_orx_x16_r", [ llvm_i16_ty ], [ llvm_v32i16_ty ], 0, llvm_v32i16_ty>;
defm int_shave_sau_orx_x8_r  : SHAVE_SuffixedIntrinsic<"sau_orx_x8_r",  [ llvm_i8_ty ],  [ llvm_v64i8_ty ], 0, llvm_v64i8_ty>;

//
//  SAU.SUMX
//    Sum vector elements to a scalar
//

// Integer variants
foreach type = [ llvm_v16i32_ty, llvm_v32i16_ty, llvm_v64i8_ty ] in {
  defvar suffix = SHAVEType<type>.cName;
  defm int_shave : SAU_SUMX_Intrinsic<type, !strconcat("_", suffix), !strconcat("_u", suffix)>;
}

// Floating-point variants
defm int_shave_sau_sumfx_f32_ri : SHAVE_SuffixedIntrinsic<"sau_sumfx_f32_ri", [ llvm_float_ty ], [ llvm_v16f32_ty, llvm_i8_ty ], 0, llvm_v16f32_ty>;
defm int_shave_sau_sumfx_f16_ri : SHAVE_SuffixedIntrinsic<"sau_sumfx_f16_ri", [ llvm_half_ty ],  [ llvm_v32f16_ty, llvm_i8_ty ], 0, llvm_v32f16_ty>;
//  NPU6+ full vector version
def int_shave_sau_sumfx_f16_f32_r : SHAVE_Intrinsic<"sau_sumfx_f16_f32_r", [ llvm_float_ty ], [ llvm_v32f16_ty ]>;

//
//  SAU.XORX
//    Element-wise XOR of vector
//

defm int_shave_sau_xorx_x32_r : SHAVE_SuffixedIntrinsic<"sau_xorx_x32_r", [ llvm_i32_ty ], [ llvm_v16i32_ty ], 0, llvm_v16i32_ty>;
defm int_shave_sau_xorx_x16_r : SHAVE_SuffixedIntrinsic<"sau_xorx_x16_r", [ llvm_i16_ty ], [ llvm_v32i16_ty ], 0, llvm_v32i16_ty>;
defm int_shave_sau_xorx_x8_r  : SHAVE_SuffixedIntrinsic<"sau_xorx_x8_r",  [ llvm_i8_ty ],  [ llvm_v64i8_ty ], 0, llvm_v64i8_ty>;

//
//  SAU Vector Transcendentals
//

foreach name = [ "logv", "sigmv", "tanhv", "expv", "exp2v", "sqtv" ] in {
  def int_shave_sau_#name#_ri  : SHAVE_Intrinsic<!strconcat("sau_", name, "_ri"),  [ llvm_half_ty ], [ llvm_v32f16_ty, llvm_i8_ty ]>;
}
