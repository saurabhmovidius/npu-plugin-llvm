//===-- SHAVEInstrInfo_CMU.td - CMU Instructions -----------*- tablegen -*-===//
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
// This file describes the instructions available in the SHAVE Compare and
// Move Unit (CMU).
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// CMU custom SDTypeProfiles, SDNodes and Operands
//===----------------------------------------------------------------------===//

// comparison instructions
def SDTSHAVE_COMPARE : SDTypeProfile<0, 2, [SDTCisSameAs<0,1>]>;
def SHAVECompareSDNode : SDNode<"SHAVEISD::CMU_CM", SDTSHAVE_COMPARE, [SDNPOutGlue], "SHAVECompareSDNode">;

// CMU.SHLIV
def SDTSHAVE_SHLV : SDTypeProfile<1, 2, [SDTCisSameAs<0, 1>, SDTCisVec<0>, SDTCisEltOfVec<2, 1>]>;
def shave_shlv    : SDNode<"SHAVEISD::SHLV"  , SDTSHAVE_SHLV>;

// Clamp instructions
def SDTSHAVE_CLAMP : SDTypeProfile<1, 4, [SDTCisSameAs<0, 1>, SDTCisSameAs<0, 2>, SDTCisSameAs<0, 3>, SDTCisInt<4>]>;
def shave_clamp    : SDNode<"SHAVEISD::CLAMP", SDTSHAVE_CLAMP>;

// Operand types for CMU.VSZM variants
def shave_vszm_opt : Operand<i8> { let PrintMethod = "printVSZMOperand"; }
// Small vector extract operands
def shave_vszm_opt_extract8  : Operand<i8> { let PrintMethod = "printVSZMSelectBitsExtract8"; }
def shave_vszm_opt_extract16 : Operand<i8> { let PrintMethod = "printVSZMSelectBitsExtract16"; }
// Small vector insert operands
def shave_vszm_opt_insert8   : Operand<i8> { let PrintMethod = "printVSZMSelectBitsInsert8"; }
def shave_vszm_opt_insert16  : Operand<i8> { let PrintMethod = "printVSZMSelectBitsInsert16"; }

//===----------------------------------------------------------------------===//
// CMU generic class declarations
//===----------------------------------------------------------------------===//

// Base class for defining compare operations of IRF/VRF
class CMU_CompareC<RegisterClass srcRC, ValueType ty, string asmstr, list<Register> defs>:
  SHAVE_CMUInstr<(outs), (ins srcRC:$src1, srcRC:$src2),
                 !strconcat(asmstr, " $src1 $src2"), 
                 [(SHAVECompareSDNode (ty srcRC:$src1), (ty srcRC:$src2))], [isCMU_Compare]>
{
  let Defs = defs;
  let isCompare = 1;
}

// Base class for all CMU.CP instructions
class CMU_CP<dag OOL, dag IOL, string asmstr, list<dag> pattern = [], list<SHAVEConflictName> confs = []> :
   SHAVE_CMUInstr<OOL, IOL, asmstr, pattern, !listconcat(confs, [isCMU_CP])>;

// Base class for CMU.CP[I|S]V
// sty = src scalar value type
// vty = dst vector value type
class CMU_CP_insert<RegisterClass destRC, RegisterClass srcRC, ValueType sty, ValueType vty, string asmstr, string index> :
  CMU_CP<(outs destRC:$dst), (ins destRC:$vsrc, srcRC:$src, i8imm:$pos),
         !strconcat(".CP", asmstr, " $dst.$pos $src", index),
         [(set (vty destRC:$dst), (vector_insert (vty destRC:$vsrc), (sty srcRC:$src), imm:$pos))],
         [isCMU_CPIV, !if(!eq(sty.Size, 32), isElementCopy32,
                      !if(!eq(sty.Size, 16), isElementCopy16,
                                             isElementCopy8))]>
{
  let Constraints = "$vsrc = $dst";
}

// Base classes for CMU MIN/MAX instructions
class CMU_MinMax_Base<RegisterClass RC, string asmstr, ValueType ty, SDNode op, int isUint, list<Register>defs> :
  SHAVE_CMUInstr<(outs RC:$dst), (ins RC:$src1, RC:$src2), 
                 !strconcat(asmstr, " $dst $src1 $src2"), 
                  [(set (ty RC:$dst), (op (ty RC:$src1), (ty RC:$src2), (i32 isUint)))], [isCMU_MIN_MAX]>
{
  let Defs = defs;
}

class CMU_MinMax_NoPat_Base<RegisterClass RC, string asmstr, list<Register>defs> :
  SHAVE_CMUInstr<(outs RC:$dst), (ins RC:$src1, RC:$src2), 
                 !strconcat(asmstr, " $dst $src1 $src2"), [], [isCMU_MIN_MAX]>
{
  let Defs = defs;
}

//===----------------------------------------------------------------------===//
// CMU instruction definitions (Section 1.4.4 of Fragrak SHAVE Reference Guide)
//===----------------------------------------------------------------------===//
class CMU_SHIFT128<RegisterClass dstRC, RegisterClass srcRC, string asmstr> :
  SHAVE_CMUInstr<(outs dstRC:$dst), (ins srcRC:$src1, i8imm:$align),
                 !strconcat(asmstr," $dst $src1 $align"), []>;

// Need to define a new itinerary for shifts
let Itinerary = II_CMU_ALIGN_VRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_SHIFTLEFT_imm  : CMU_SHIFT128<VRF128, VRF128, ".SHLVEC">;
  def CMU_SHIFTRIGHT_imm : CMU_SHIFT128<VRF128, VRF128, ".SHRVEC">;
}
//
// 1.4.4.1 - CMU.ALIGNVEC
//   Long vector align with byte offset.
//

class CMU_ALIGNVEC<RegisterClass dstRC, RegisterClass srcRC> :
  SHAVE_CMUInstr<(outs dstRC:$dst), (ins srcRC:$src1, srcRC:$src2, i8imm:$align),
                 ".ALIGNVEC $dst $src1 $src2 $align", []>;

let Itinerary = II_CMU_ALIGN_VRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_ALIGNVEC_imm_vrf       : CMU_ALIGNVEC<VRF128,  VRF128>;
  def CMU_ALIGNVEC_imm_vrf64     : CMU_ALIGNVEC<VRF64_l, VRF64_l>;

  // Special version of CMU.ALIGNVEC that is used for extracting a VRF64_l subvector
  // from a VRF128 vector with a constant index ($align)
  def CMU_ALIGNVEC_imm_vrf_vrf64 : CMU_ALIGNVEC<VRF64_l, VRF128>;

  // Special version of CMU.ALIGNVEC that is used to align a VRF64_l
  // into a new VRF128. This is used for subvector insertion, with masks
  def CMU_ALIGNVEC_imm_vrf64_vrf : CMU_ALIGNVEC<VRF128, VRF64_l>;

  // Special version of CMU.ALIGNVEC that is used for inserting a VRF64_l subvector
  // into a VRF128 vector with a constant index ($align)
  // This is used in conjunction with a PEU.PVEN8C to achieve the desired effect
  let Constraints = "$dst = $vector1" in {
    def CMU_ALIGNVEC_imm_vrf64_vrf_insert : SHAVE_CMUInstr<(outs VRF128:$dst), (ins VRF128:$vector1, VRF64_l:$vector2, i8imm:$align),
                                                                ".ALIGNVEC $dst $vector2 $vector2 $align", []>;
  }

  // Special version of CMU.ALIGNVEC that is used for extracting a VRF64_l subvector
  // from a VRF128 vector with a variable index. This is only used in a bundle
  // with a BRU.RPI instruction, which means that the source and destination registers
  // must be the same
  let Constraints = "$dst = $src1" in {
    def CMU_ALIGNVEC_imm_vrf64_SUBVECTOR_EXTRACT : CMU_ALIGNVEC<VRF64_l,  VRF64_l>;
  }
}

//
// 1.4.4.2 - CMU.CLAMP
//   Clamp between zero and a positive value, or between +/- a specified value.
//

class CMU_CLAMP_Raw<string Type, RegisterClass RC, string asmstr> :
  SHAVE_CMUInstr<(outs RC:$dst), (ins RC:$src1, RC:$src2),
                 !strconcat(Type, asmstr, " $dst $src1 $src2"), []>;

multiclass T_CMU_CLAMPV<string Type>
{
  // 128-bit vectors
  def _f32_v4f32 : CMU_CLAMP_Raw<Type, VRF128, ".f32">;
  def _f16_v8f16 : CMU_CLAMP_Raw<Type, VRF128, ".f16">;
  def _i32_v4i32 : CMU_CLAMP_Raw<Type, VRF128, ".i32">;
  def _i16_v8i16 : CMU_CLAMP_Raw<Type, VRF128, ".i16">;
  def _i8_v16i8  : CMU_CLAMP_Raw<Type, VRF128,  ".i8">;

  // 64-bit vectors
  def _f32_v2f32 : CMU_CLAMP_Raw<Type, VRF64_l, ".f32">;
  def _f16_v4f16 : CMU_CLAMP_Raw<Type, VRF64_l, ".f16">;
  def _i32_v2i32 : CMU_CLAMP_Raw<Type, VRF64_l, ".i32">;
  def _i16_v4i16 : CMU_CLAMP_Raw<Type, VRF64_l, ".i16">;
  def _i8_v8i8   : CMU_CLAMP_Raw<Type, VRF64_l,  ".i8">;
}

multiclass T_CMU_CLAMP<string Type>
{
  // Scalar types
  def _f32 : CMU_CLAMP_Raw<Type, IRF32,   ".f32">;
  def _f16 : CMU_CLAMP_Raw<Type, IRF16_l, ".f16">;
  def _i32 : CMU_CLAMP_Raw<Type, IRF32,   ".i32">;
  def _i16 : CMU_CLAMP_Raw<Type, IRF16_l, ".i16">;
  def _i8  : CMU_CLAMP_Raw<Type, IRF8_q0, ".i8">;

  // Vector types
  def _f16_v2f16 : CMU_CLAMP_Raw<Type, IRF32,   ".f16">;
  def _i16_v2i16 : CMU_CLAMP_Raw<Type, IRF32,   ".i16">;
  def _i8_v4i8   : CMU_CLAMP_Raw<Type, IRF32,   ".i8">;
  def _i8_v2i8   : CMU_CLAMP_Raw<Type, IRF16_l, ".i8">;
}

// CMU.CLAMP[0] IRF
let Itinerary = II_CMU_CLAMP_IRF in
{
  defm CMU_CLAMP  : T_CMU_CLAMP<".CLAMP">;
  defm CMU_CLAMP0 : T_CMU_CLAMP<".CLAMP0">;
}

// CMU.CLAMP[0] VRF
let Itinerary = II_CMU_CLAMP_VRF, Predicates = [HasVRF128_Predicate] in
{
  defm CMU_CLAMP  : T_CMU_CLAMPV<".CLAMP">;
  defm CMU_CLAMP0 : T_CMU_CLAMPV<".CLAMP0">;
}

//
// 1.4.4.3 - CMU.CLAMPAB
//   Clamp within range.
//

class CMU_CLAMPAB<RegisterClass RC, ValueType ty, int isUint, string asmstr> :
  SHAVE_CMUInstr<(outs RC:$dst), (ins RC:$src, RC:$min, RC:$max),
                 !strconcat(".CLAMPAB", asmstr, " $src $min $max"),
                 [(set (ty RC:$dst), (shave_clamp (ty RC:$src), (ty RC:$min), (ty RC:$max), (i32 isUint)))]>
{
    let Constraints = "$src = $dst";
}

let Itinerary = II_CMU_CLAMPAB_VRF, Predicates = [HasVRF128_Predicate] in
{
  // 128-bit vectors
  def CMU_CLAMPAB_f32_v4f32 : CMU_CLAMPAB<VRF128, v4f32, 0, ".f32">;
  def CMU_CLAMPAB_f16_v8f16 : CMU_CLAMPAB<VRF128, v8f16, 0, ".f16">;
  def CMU_CLAMPAB_i32_v4i32 : CMU_CLAMPAB<VRF128, v4i32, 0, ".i32">;
  def CMU_CLAMPAB_u32_v4u32 : CMU_CLAMPAB<VRF128, v4i32, 1, ".u32">;
  def CMU_CLAMPAB_i16_v8i16 : CMU_CLAMPAB<VRF128, v8i16, 0, ".i16">;
  def CMU_CLAMPAB_u16_v8u16 : CMU_CLAMPAB<VRF128, v8i16, 1, ".u16">;
  def CMU_CLAMPAB_i8_v16i8  : CMU_CLAMPAB<VRF128, v16i8, 0, ".i8">;
  def CMU_CLAMPAB_u8_v16u8  : CMU_CLAMPAB<VRF128, v16i8, 1, ".u8">;

  // 64-bit vectors
  def CMU_CLAMPAB_f32_v2f32 : CMU_CLAMPAB<VRF64_l, v2f32, 0, ".f32">;
  def CMU_CLAMPAB_f16_v4f16 : CMU_CLAMPAB<VRF64_l, v4f16, 0, ".f16">;
  def CMU_CLAMPAB_i32_v2i32 : CMU_CLAMPAB<VRF64_l, v2i32, 0, ".i32">;
  def CMU_CLAMPAB_u32_v2u32 : CMU_CLAMPAB<VRF64_l, v2i32, 1, ".u32">;
  def CMU_CLAMPAB_i16_v4i16 : CMU_CLAMPAB<VRF64_l, v4i16, 0, ".i16">;
  def CMU_CLAMPAB_u16_v4u16 : CMU_CLAMPAB<VRF64_l, v4i16, 1, ".u16">;
  def CMU_CLAMPAB_i8_v8i8   : CMU_CLAMPAB<VRF64_l, v8i8,  0, ".i8">;
  def CMU_CLAMPAB_u8_v8u8   : CMU_CLAMPAB<VRF64_l, v8i8,  1, ".u8">;
}

let Itinerary = II_CMU_CLAMPAB_IRF in
{
  // Scalar variants
  def CMU_CLAMPAB_f32 : CMU_CLAMPAB<IRF32,   f32,   0, ".f32">;
  def CMU_CLAMPAB_f16 : CMU_CLAMPAB<IRF16_l, f16,   0, ".f16">;
  def CMU_CLAMPAB_i32 : CMU_CLAMPAB<IRF32,   i32,   0, ".i32">;
  def CMU_CLAMPAB_u32 : CMU_CLAMPAB<IRF32,   i32,   1, ".u32">;
  def CMU_CLAMPAB_i16 : CMU_CLAMPAB<IRF16_l, i16,   0, ".i16">;
  def CMU_CLAMPAB_u16 : CMU_CLAMPAB<IRF16_l, i16,   1, ".u16">;
  def CMU_CLAMPAB_i8  : CMU_CLAMPAB<IRF8_q0, i8,    0, ".i8">;
  def CMU_CLAMPAB_u8  : CMU_CLAMPAB<IRF8_q0, i8,    1, ".u8">;
  
  // Vector variants
  def CMU_CLAMPAB_v2f16 : CMU_CLAMPAB<IRF32,   v2f16, 0, ".f16">;
  def CMU_CLAMPAB_v2i16 : CMU_CLAMPAB<IRF32,   v2i16, 0, ".i16">;
  def CMU_CLAMPAB_v2u16 : CMU_CLAMPAB<IRF32,   v2i16, 1, ".u16">;
  def CMU_CLAMPAB_v4i8  : CMU_CLAMPAB<IRF32,   v4i8,  0, ".i8">;
  def CMU_CLAMPAB_v4u8  : CMU_CLAMPAB<IRF32,   v4i8,  1, ".u8">;
  def CMU_CLAMPAB_v2i8  : CMU_CLAMPAB<IRF16_l, v2i8,  0, ".i8">;
  def CMU_CLAMPAB_v2u8  : CMU_CLAMPAB<IRF16_l, v2i8,  1, ".u8">;
}

//
// 1.4.4.4 - CMU.CMASK
//   Generate VRF mask from IRF contents.
//

class CMU_CMASK_Raw<string immVal> : 
  SHAVE_CMUInstr<(outs VRF128:$dst), (ins IRF32:$src), 
                 !strconcat(".CMASK $dst $src ", immVal), []>;

let Itinerary = II_CMU_CMASK, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CMASK_v4i32 : CMU_CMASK_Raw<"2">;
  def CMU_CMASK_v8i16 : CMU_CMASK_Raw<"1">;
  def CMU_CMASK_v16i8 : CMU_CMASK_Raw<"0">;
}

def CMU_CMPI_Raw : SHAVE_CMUInstr<(outs), (ins IRF32:$src1, i32imm:$src2),
                                  ".CMPI $src1 $src2",
                                  [(SHAVECompareSDNode (i32 IRF32:$src1), (i32 simm16:$src2))],
                                  [isCMU_Compare]>
{
  let Defs = [CC_CMU0];
  let isCompare = 1;
  let Itinerary = II_CMU_CMZ_COMPARISON_IRF_WITH_WRITE_1_CONDITION;
}

//
// 1.4.4.5 - CMU.CMII
//   IRF compare with optional absolute.
//

// Floating point comparison operations
let Itinerary = II_CMU_CMII_COMPARISON_WITH_WRITE_1_CONDITION in
{
  def CMU_CMII_f32 : CMU_CompareC<IRF32,     f32,   ".CMII.f32", [CC_CMU0]>;
}

let Itinerary = II_CMU_CMII_COMPARISON_WITH_WRITE_2_CONDITION in
{
  def CMU_CMII_f16   : CMU_CompareC<IRF16_l, f16,   ".CMII.f16", [CC_CMU0, CC_CMU1]>;
  def CMU_CMII_v2f16 : CMU_CompareC<IRF32,   v2f16, ".CMII.f16", [CC_CMU0, CC_CMU1]>;
}

// Integer comparison operations
let Itinerary = II_CMU_CMII_COMPARISON_WITH_WRITE_1_CONDITION in
{
  def CMU_CMII_i32 : CMU_CompareC<IRF32,     i32,   ".CMII.i32", [CC_CMU0]>;
  def CMU_CMII_u32 : CMU_CompareC<IRF32,     i32,   ".CMII.u32", [CC_CMU0]>;
}

let Itinerary = II_CMU_CMII_COMPARISON_WITH_WRITE_2_CONDITION in
{
  def CMU_CMII_i16   : CMU_CompareC<IRF16_l, i16,   ".CMII.i16", [CC_CMU0, CC_CMU1]>;
  def CMU_CMII_u16   : CMU_CompareC<IRF16_l, i16,   ".CMII.u16", [CC_CMU0, CC_CMU1]>;

  def CMU_CMII_v2i16 : CMU_CompareC<IRF32,   v2i16, ".CMII.i16", [CC_CMU0, CC_CMU1]>;
  def CMU_CMII_v2u16 : CMU_CompareC<IRF32,   v2i16, ".CMII.u16", [CC_CMU0, CC_CMU1]>;

  def CMU_CMII_v2i8  : CMU_CompareC<IRF16_l, v2i8,  ".CMII.i8",  [CC_CMU0, CC_CMU1]>;
  def CMU_CMII_v2u8  : CMU_CompareC<IRF16_l, v2i8,  ".CMII.u8",  [CC_CMU0, CC_CMU1]>;
}

let Itinerary = II_CMU_CMII_COMPARISON_WITH_WRITE_4_CONDITION in
{
  def CMU_CMII_i8    : CMU_CompareC<IRF8_q0, i8,    ".CMII.i8",  [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;
  def CMU_CMII_u8    : CMU_CompareC<IRF8_q0, i8,    ".CMII.u8",  [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;

  def CMU_CMII_v4i8    : CMU_CompareC<IRF32, v4i8,    ".CMII.i8",  [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;
  def CMU_CMII_v4u8    : CMU_CompareC<IRF32, v4i8,    ".CMII.u8",  [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;
}

//
// 1.4.4.6 - CMU.CMTI
//   TRF compare with IRF.
//

let Itinerary = II_CMU_CMTI_TRF_IRF, isBarrier = 1, isCompare = 1 in {
  def CMU_CMTI_Raw : SHAVE_CMUInstr<(outs), (ins IRF32:$src1, TRF:$src2), ".CMTI $src1 $src2", []>;
}

// Only for use with preemption handling, these versions of CMTI.* are not marked as a barrier
let Itinerary = II_CMU_CMTI_TRF_IRF, isCompare = 1 in {
  def CMU_CMTI_C16_preemption : SHAVE_CMUInstr<(outs), (ins IRF32:$src1, TRF:$src2),
                                               ".CMTI.C16 $src1 $src2 ${:comment} auto-generated for preemption handler", []> {
    let Defs = [C_CMU0, C_CMU1];
  }

  def CMU_CMTI_BITP_preemption : SHAVE_CMUInstr<(outs), (ins IRF32:$src1, TRF:$src2),
                                                ".CMTI.BITP $src1 $src2 ${:comment} auto-generated for preemption handler", []> {
    let Defs = [CC_CMU0, CC_CMU1];
  }

  def CMU_CMTI_BITN_preemption : SHAVE_CMUInstr<(outs), (ins IRF32:$src1, TRF:$src2),
                                                ".CMTI.BITN $src1 $src2 ${:comment} auto-generated for preemption handler", []> {
    let Defs = [CC_CMU0, CC_CMU1];
  }
}

//
// 1.4.4.7 - CMU.CMVV
//   VRF compare with optional absolute.
//

class CMU_CMVV_VRF128<ValueType vt, string asmstr> : 
  CMU_CompareC<VRF128, vt, asmstr,
               [CC_CMU0,  CC_CMU1,  CC_CMU2,  CC_CMU3,  CC_CMU4,  CC_CMU5,  CC_CMU6,  CC_CMU7,
                CC_CMU8,  CC_CMU9,  CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15]>;

let Itinerary = II_CMU_CMVV_COMPARISON_WITH_WRITE_16_CONDITION, Predicates = [HasVRF128_Predicate] in
{
  // Floating point variants
  def CMU_CMVV_f32 : CMU_CMVV_VRF128<v4f32, ".CMVV.f32" >;
  def CMU_CMVV_f16 : CMU_CMVV_VRF128<v8f16, ".CMVV.f16">;

  // Signed integer variants
  def CMU_CMVV_i32 : CMU_CMVV_VRF128<v4i32, ".CMVV.i32">;
  def CMU_CMVV_i16 : CMU_CMVV_VRF128<v8i16, ".CMVV.i16">;
  def CMU_CMVV_i8  : CMU_CMVV_VRF128<v16i8, ".CMVV.i8">;

  // Unsigned integer variants
  def CMU_CMVV_u32 : CMU_CMVV_VRF128<v4i32, ".CMVV.u32">;
  def CMU_CMVV_u16 : CMU_CMVV_VRF128<v8i16, ".CMVV.u16">;
  def CMU_CMVV_u8  : CMU_CMVV_VRF128<v16i8, ".CMVV.u8">;
}

class CMU_CMVV_VRF64_l<ValueType vt, string asmstr> : 
  CMU_CompareC<VRF64_l, vt, asmstr,
               [CC_CMU0,  CC_CMU1,  CC_CMU2,  CC_CMU3,  CC_CMU4,  CC_CMU5,  CC_CMU6,  CC_CMU7,
                CC_CMU8,  CC_CMU9,  CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15]>;

let Itinerary = II_CMU_CMVV_COMPARISON_WITH_WRITE_8_CONDITION, Predicates = [HasVRF128_Predicate] in
{
  // Floating point variants
  def CMU_CMVV_v2f32 : CMU_CMVV_VRF64_l<v2f32, ".CMVV.f32" >;
  def CMU_CMVV_v4f16 : CMU_CMVV_VRF64_l<v4f16, ".CMVV.f16">;

  // Signed integer variants
  def CMU_CMVV_v2i32 : CMU_CMVV_VRF64_l<v2i32, ".CMVV.i32">;
  def CMU_CMVV_v4i16 : CMU_CMVV_VRF64_l<v4i16, ".CMVV.i16">;
  def CMU_CMVV_v8i8  : CMU_CMVV_VRF64_l<v8i8,  ".CMVV.i8">;

  // Unsigned integer variants
  def CMU_CMVV_v2u32 : CMU_CMVV_VRF64_l<v2i32, ".CMVV.u32">;
  def CMU_CMVV_v4u16 : CMU_CMVV_VRF64_l<v4i16, ".CMVV.u16">;
  def CMU_CMVV_v8u8  : CMU_CMVV_VRF64_l<v8i8,  ".CMVV.u8">;
}

//
// 1.4.4.8 - CMU.CMZ
//   Compare to zero.
//

class CMU_CompareZeroC_Raw<RegisterClass srcRC, string asmstr, list<Register> defs>:
  SHAVE_CMUInstr<(outs), (ins srcRC:$src), !strconcat(asmstr, " $src"), [], [isCMU_Compare]>
{
  let Defs = defs;
  let isCompare = 1;
}

// IRF variants
let Itinerary = II_CMU_CMZ_COMPARISON_IRF_WITH_WRITE_1_CONDITION in
{
  def CMU_CMZ_f32   : CMU_CompareZeroC_Raw<IRF32,   ".CMZ.f32", [CC_CMU0]>;
  def CMU_CMZ_i32   : CMU_CompareZeroC_Raw<IRF32,   ".CMZ.i32", [CC_CMU0]>;
}

let Itinerary = II_CMU_CMZ_COMPARISON_IRF_WITH_WRITE_2_CONDITION in
{
  def CMU_CMZ_f16   : CMU_CompareZeroC_Raw<IRF16_l, ".CMZ.f16", [CC_CMU0, CC_CMU1]>;
  def CMU_CMZ_v2f16 : CMU_CompareZeroC_Raw<IRF32,   ".CMZ.f16", [CC_CMU0, CC_CMU1]>;

  def CMU_CMZ_i16   : CMU_CompareZeroC_Raw<IRF16_l, ".CMZ.i16", [CC_CMU0, CC_CMU1]>;
  def CMU_CMZ_v2i16 : CMU_CompareZeroC_Raw<IRF32,   ".CMZ.i16", [CC_CMU0, CC_CMU1]>;
}

let Itinerary = II_CMU_CMZ_COMPARISON_IRF_WITH_WRITE_4_CONDITION in
{
  def CMU_CMZ_i8         : CMU_CompareZeroC_Raw<IRF8_q0, ".CMZ.i8", [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;
  def CMU_CMZ_v4i8       : CMU_CompareZeroC_Raw<IRF32,   ".CMZ.i8", [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;
}

// VRF variants
let Itinerary = II_CMU_CMZ_COMPARISON_VRF_WITH_WRITE_16_CONDITION, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CMZ_v4f32  : CMU_CompareZeroC_Raw<VRF128, ".CMZ.f32",
     [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7,
      CC_CMU8,  CC_CMU9,  CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15]>;
  def CMU_CMZ_v4i32  : CMU_CompareZeroC_Raw<VRF128, ".CMZ.i32",
    [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7,
     CC_CMU8,  CC_CMU9,  CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15]>;

  def CMU_CMZ_v8f16  : CMU_CompareZeroC_Raw<VRF128, ".CMZ.f16",
     [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7,
      CC_CMU8,  CC_CMU9,  CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15]>;
  def CMU_CMZ_v8i16  : CMU_CompareZeroC_Raw<VRF128, ".CMZ.i16",
    [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7,
     CC_CMU8,  CC_CMU9,  CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15]>;

  def CMU_CMZ_v16i8  : CMU_CompareZeroC_Raw<VRF128, ".CMZ.i8",
    [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7,
     CC_CMU8,  CC_CMU9,  CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15]>;
}

//
// 1.4.4.9 - CMU.CPII
//   IRF copy with optional conversion.
//

// Basic register->register copies
class CMU_CP_Raw<RegisterClass regVT, string asmstr> :
  CMU_CP<(outs regVT:$dst), (ins regVT:$src),
         !strconcat(asmstr, " $dst $src"), []>;

// Converting IRF to IRF copies
class CMU_CPII_32_32_OpC<SDNode cvtopc, ValueType srcVT, ValueType dstVT, string format = "">:
  CMU_CP<(outs IRF32:$dst), (ins IRF32:$src),
         !strconcat(".CP.32", format, " $dst $src"),
         [(set (dstVT IRF32:$dst), (cvtopc (srcVT IRF32:$src)))]>;

class CMU_CPII_XX_32_OpC<SDNode cvtopc, RegisterClass srcRC, ValueType srcVT, ValueType dstVT, string format = "", string srcLane = ".0">:
  CMU_CP<(outs IRF32:$dst), (ins srcRC:$src),
         !strconcat(".CP.32", format, " $dst $src", srcLane),
         [(set (dstVT IRF32:$dst), (cvtopc (srcVT srcRC:$src)))]>;

class CMU_CPII_32_16_OpC<SDNode cvtopc, RegisterClass dstRC,
                       ValueType srcVT, ValueType dstVT, string format = "", string dstLane = ".0">:
  CMU_CP<(outs dstRC:$dst), (ins IRF32:$src),
         !strconcat(".CP.16", format, " $dst", dstLane, " $src"),
         [(set (dstVT dstRC:$dst), (cvtopc (srcVT IRF32:$src)))]>;

class CMU_CPII_XX_16_OpC<SDNode cvtopc, RegisterClass srcRC, RegisterClass dstRC,
                       ValueType srcVT, ValueType dstVT, string format = "", string srcLane = ".0", string dstLane = ".0", string comment = "">:
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src),
         !strconcat(".CP.16", format, " $dst", dstLane, " $src", srcLane, comment),
         [(set (dstVT dstRC:$dst), (cvtopc (srcVT srcRC:$src)))]>;

class CMU_CPII_32_8_OpC<SDNode cvtopc, RegisterClass dstRC,
                       ValueType srcVT, ValueType dstVT, string format = "", string dstLane = ".0">:
  CMU_CP<(outs dstRC:$dst), (ins IRF32:$src),
         !strconcat(".CP.8", format, " $dst", dstLane, " $src"),
         [(set (dstVT dstRC:$dst), (cvtopc (srcVT IRF32:$src)))]>;

class CMU_CPII_XX_8_OpC<SDNode cvtopc, RegisterClass srcRC, RegisterClass dstRC,
                             ValueType srcVT, ValueType dstVT, string format = "", string srcLane = ".0", string dstLane = ".0", string comment = "">:
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src),
         !strconcat(".CP.8", format, " $dst", dstLane, " $src", srcLane, comment),
         [(set (dstVT dstRC:$dst), (cvtopc (srcVT srcRC:$src)))]>;

class CMU_CPII_trunc_XX_16_OpC<RegisterClass srcRC, RegisterClass dstRC,
                               string srcLane = ".0", string dstLane = ".0", string comment = "">:
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src),
         !strconcat(".CP.16 $dst", dstLane, " $src", srcLane, comment),
         []>;

class CMU_CPII_trunc_XX_8_OpC<RegisterClass srcRC, RegisterClass dstRC,
                              string srcLane = ".0", string dstLane = ".0", string comment = "">:
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src),
         !strconcat(".CP.8 $dst", dstLane, " $src", srcLane, comment),
         []>;


// IRF variants
let Itinerary = II_CMU_CPII_IRF in
{
  def CMU_CPII_32_Raw    : CMU_CP_Raw<IRF32,   ".CP.32">;
  def CMU_CPII_16_Raw    : CMU_CP_Raw<IRF16_l, ".CP.32">;
  def CMU_CPII_8_Raw     : CMU_CP_Raw<IRF8_q0, ".CP.32">;

  def CMU_CPII_32_16_trunc : CMU_CPII_trunc_XX_16_OpC<IRF32,   IRF16_l, ".0", ".0", " ${:comment} truncate 'i32' to 'i16'">;
  def CMU_CPII_32_8_trunc  : CMU_CPII_trunc_XX_8_OpC< IRF32,   IRF8_q0, ".0", ".0", " ${:comment} truncate 'i32' to 'i8'">;
  def CMU_CPII_16_8_trunc  : CMU_CPII_trunc_XX_8_OpC< IRF16_l, IRF8_q0, ".0", ".0", " ${:comment} truncate 'i16' to 'i8'">;

  // High to low and low to high copies
  def CMU_CPII_16_low_to_high : SHAVE_CMUInstr<(outs IRF16_h:$dst), (ins IRF16_l:$src), ".CP.16 $dst.1 $src.0", []>;
  def CMU_CPII_16_high_to_low : SHAVE_CMUInstr<(outs IRF16_l:$dst), (ins IRF16_h:$src), ".CP.16 $dst.0 $src.1", []>;
}

let Itinerary = II_CMU_CPII_IRF64 in
{
  def CMU_CPII_64_Raw    : CMU_CP_Raw<IRF64,   ".CP.64">, Requires<[HasIRF64_LSU_CMU_Instrs_Predicate]>;
}


// 1.8.4.18 / 1.8.4.19 - CMU.CP [FTOF/FTOI]
// Myriad 2.3 converting CP instructions
// Converting copies of scalars or vector elements from one RF location to another with direct element specifiers.
class CMU_CPVI_conv_Myr2v3<SDNode opc, string asmStr, ValueType srcElemVT, ValueType srcVT, RegisterClass srcRC, ValueType dstVT, RegisterClass dstRC> :
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src, i8imm:$pos), asmStr,
         [(set (dstVT dstRC:$dst), (opc (srcElemVT (vector_extract (srcVT srcRC:$src), imm:$pos))))]>;

class CMU_CPVI_CPIV_vector_conv_Myr2v3<SDNode opc, string asmStr, ValueType srcVT, RegisterClass srcRC, ValueType dstVT, RegisterClass dstRC> :
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src), asmStr,
         [(set (dstVT dstRC:$dst), (opc (srcVT srcRC:$src)))]>;

class CMU_CPIV_conv_Myr2v3<SDNode opc, string asmStr, ValueType srcVT, RegisterClass srcRC, ValueType dstElemVT, ValueType dstVT, RegisterClass dstRC> :
  CMU_CP<(outs dstRC:$dst), (ins dstRC:$dsrc, srcRC:$src, i8imm:$pos), asmStr,
         [(set (dstVT dstRC:$dst), (vector_insert (dstVT dstRC:$dsrc), (dstElemVT (opc (srcVT srcRC:$src))), imm:$pos))]>
{
  let Constraints = "$dsrc = $dst";
}

class CMU_CPVV_conv_Myr2v3<SDNode opc, string asmStr, ValueType srcElemVT, ValueType srcVT, RegisterClass srcRC, ValueType dstElemVT, ValueType dstVT, RegisterClass dstRC> :
  CMU_CP<(outs dstRC:$dst), (ins  dstRC:$dsrc, srcRC:$src, i8imm:$spos, i8imm:$dpos), asmStr,
      [(set (dstVT dstRC:$dst), (vector_insert (dstVT dstRC:$dsrc), (dstElemVT (opc (srcElemVT (vector_extract (srcVT srcRC:$src), imm:$spos)))), imm:$dpos))]>
{
  let Constraints = "$dsrc = $dst";
}


let Itinerary = II_CMU_CPVI_IRF_VRF, Predicates = [HasVRF128_Predicate] in
{
  //  CMU_CP.FTOF
  //  F16 -> F32
  //                                                   Opcode,                                asmStr, srcElemVT,  srcVT,   srcRC, dstVT, dstRC
  def CMU_CPVI_ftof_v8f16_f32 : CMU_CPVI_conv_Myr2v3<fpextend,       ".CP.32.f16.f32 $dst $src.$pos",       f16,  v8f16,  VRF128,   f32, IRF32   >;
  def CMU_CPVI_ftof_v4f16_f32 : CMU_CPVI_conv_Myr2v3<fpextend,       ".CP.32.f16.f32 $dst $src.$pos",       f16,  v4f16, VRF64_l,   f32, IRF32   >;

  //  F32 -> F16
  def CMU_CPVI_ftof_v4f32_f16 :  CMU_CPVI_conv_Myr2v3<fpround, ".CP.16.f32.f16 $dst.0 $src.$pos",       f32,  v4f32,  VRF128,   f16, IRF16_l >;
  def CMU_CPVI_ftof_v2f32_f16 :  CMU_CPVI_conv_Myr2v3<fpround, ".CP.16.f32.f16 $dst.0 $src.$pos",       f32,  v2f32, VRF64_l,   f16, IRF16_l >;

  // CMU_CP.ITOI
  // I8 -> I32
  //                                                      Opcode,                            asmStr, srcElemVT,  srcVT,   srcRC, dstVT,   dstRC
  def CMU_CPVI_itoi_v16i8_i32 :  CMU_CPVI_conv_Myr2v3<      sext,    ".CP.32.i8.i32 $dst $src.$pos",        i8,  v16i8,  VRF128,   i32,   IRF32 >;
  def CMU_CPVI_itoi_v8i8_i32  :  CMU_CPVI_conv_Myr2v3<      sext,    ".CP.32.i8.i32 $dst $src.$pos",        i8,   v8i8, VRF64_l,   i32,   IRF32 >;

  // CMU_CP.UTOU
  // U8 -> U32
  //                                                      Opcode,                            asmStr, srcElemVT,  srcVT,   srcRC, dstVT,   dstRC
  def CMU_CPVI_utou_v16u8_u32 :  CMU_CPVI_conv_Myr2v3<      zext,    ".CP.32.u8.u32 $dst $src.$pos",        i8,  v16i8,  VRF128,   i32,   IRF32 >;
  def CMU_CPVI_utou_v8u8_u32  :  CMU_CPVI_conv_Myr2v3<      zext,    ".CP.32.u8.u32 $dst $src.$pos",        i8,   v8i8, VRF64_l,   i32,   IRF32 >;

}

let Itinerary = II_CMU_CPVI_IRF_VRF_FCONV, Predicates = [HasVRF128_Predicate] in
{
  // CMU_CP.FTOI
  // F32 -> I32
  //                                                      Opcode,                             asmStr, srcElemVT,  srcVT,   srcRC, dstVT, dstRC
  def CMU_CPVI_ftoi_v4f32_i32 :  CMU_CPVI_conv_Myr2v3<fp_to_sint,   ".CP.32.f32.i32s $dst $src.$pos",       f32,  v4f32,  VRF128,   i32,   IRF32 >;
  def CMU_CPVI_ftoi_v2f32_i32 :  CMU_CPVI_conv_Myr2v3<fp_to_sint,   ".CP.32.f32.i32s $dst $src.$pos",       f32,  v2f32, VRF64_l,   i32,   IRF32 >;

  // F16 -> I32
  def CMU_CPVI_ftoi_v8f16_i32 :  CMU_CPVI_conv_Myr2v3<fp_to_sint,   ".CP.32.f16.i32s $dst $src.$pos",       f16,  v8f16,  VRF128,   i32,   IRF32 >;
  def CMU_CPVI_ftoi_v4f16_i32 :  CMU_CPVI_conv_Myr2v3<fp_to_sint,   ".CP.32.f16.i32s $dst $src.$pos",       f16,  v4f16, VRF64_l,   i32,   IRF32 >;

  // F32 -> I8
  def CMU_CPVI_ftoi_v4f32_i8 :  CMU_CPVI_conv_Myr2v3<fp_to_sint,".CP.8.f32.i8s $dst.0 $src.$pos",       f32,  v4f32,  VRF128,   i8,   IRF8_q0 >;
  def CMU_CPVI_ftoi_v2f32_i8 :  CMU_CPVI_conv_Myr2v3<fp_to_sint,".CP.8.f32.i8s $dst.0 $src.$pos",       f32,  v2f32, VRF64_l,   i8,   IRF8_q0 >;

  // F16 -> I8
  def CMU_CPVI_ftoi_v8f16_i8 :  CMU_CPVI_conv_Myr2v3<fp_to_sint,".CP.8.f16.i8s $dst.0 $src.$pos",       f16,  v8f16,  VRF128,   i8,   IRF8_q0 >;
  def CMU_CPVI_ftoi_v4f16_i8 :  CMU_CPVI_conv_Myr2v3<fp_to_sint,".CP.8.f16.i8s $dst.0 $src.$pos",       f16,  v4f16, VRF64_l,   i8,   IRF8_q0 >;

  // CMU_CP.ITOF
  // I32 -> F32
  //                                                      Opcode,                             asmStr, srcElemVT,  srcVT,   srcRC, dstVT, dstRC
  def CMU_CPVI_itof_v4i32_f32 :  CMU_CPVI_conv_Myr2v3<sint_to_fp,   ".CP.32.i32.f32 $dst $src.$pos",       i32,  v4i32,  VRF128,   f32,   IRF32 >;
  def CMU_CPVI_itof_v2i32_f32 :  CMU_CPVI_conv_Myr2v3<sint_to_fp,   ".CP.32.i32.f32 $dst $src.$pos",       i32,  v2i32, VRF64_l,   f32,   IRF32 >;

  // I32 -> F16
  //                                                      Opcode,                                  asmStr, srcElemVT,  srcVT,   srcRC, dstVT, dstRC
  def CMU_CPVI_itof_v4i32_f16 :  CMU_CPVI_conv_Myr2v3<sint_to_fp,   ".CP.16.i32.f16 $dst.0 $src.$pos",       i32,  v4i32,  VRF128,   f16,   IRF16_l >;
  def CMU_CPVI_itof_v2i32_f16 :  CMU_CPVI_conv_Myr2v3<sint_to_fp,   ".CP.16.i32.f16 $dst.0 $src.$pos",       i32,  v2i32, VRF64_l,   f16,   IRF16_l >;

  // I8 -> F32
  def CMU_CPVI_itof_v16i8_f32 :  CMU_CPVI_conv_Myr2v3<sint_to_fp,    ".CP.32.i8.f32 $dst $src.$pos",              i8,  v16i8,  VRF128,  f32,   IRF32 >;
  def CMU_CPVI_itof_v8i8_f32  :  CMU_CPVI_conv_Myr2v3<sint_to_fp,    ".CP.32.i8.f32 $dst $src.$pos",              i8,   v8i8, VRF64_l,  f32,   IRF32 >;

  // I8 -> F16
  def CMU_CPVI_itof_v16i8_f16 :  CMU_CPVI_conv_Myr2v3<sint_to_fp,    ".CP.16.i8.f16 $dst.0 $src.$pos",        i8,  v16i8,  VRF128,  f16,  IRF16_l >;
  def CMU_CPVI_itof_v8i8_f16  :  CMU_CPVI_conv_Myr2v3<sint_to_fp,    ".CP.16.i8.f16 $dst.0 $src.$pos",        i8,   v8i8, VRF64_l,  f16,  IRF16_l >;


  // CMU_CP.FTOU
  // F32 -> U32
  //                                                      Opcode,                             asmStr, srcElemVT,  srcVT,   srcRC, dstVT, dstRC
  def CMU_CPVI_ftou_v4f32_u32 :  CMU_CPVI_conv_Myr2v3<fp_to_uint,   ".CP.32.f32.u32s $dst $src.$pos",       f32,  v4f32,  VRF128,   i32,   IRF32 >;
  def CMU_CPVI_ftou_v2f32_u32 :  CMU_CPVI_conv_Myr2v3<fp_to_uint,   ".CP.32.f32.u32s $dst $src.$pos",       f32,  v2f32, VRF64_l,   i32,   IRF32 >;

  // F16 -> U32
  def CMU_CPVI_ftou_v8f16_u32 :  CMU_CPVI_conv_Myr2v3<fp_to_uint,   ".CP.32.f16.u32s $dst $src.$pos",       f16,  v8f16,  VRF128,   i32,   IRF32 >;
  def CMU_CPVI_ftou_v4f16_u32 :  CMU_CPVI_conv_Myr2v3<fp_to_uint,   ".CP.32.f16.u32s $dst $src.$pos",       f16,  v4f16, VRF64_l,   i32,   IRF32 >;

  // F32 -> U8
  def CMU_CPVI_ftoi_v4f32_u8 :  CMU_CPVI_conv_Myr2v3<fp_to_uint,".CP.8.f32.u8s $dst.0 $src.$pos",       f32,  v4f32,  VRF128,   i8,   IRF8_q0 >;
  def CMU_CPVI_ftoi_v2f32_u8 :  CMU_CPVI_conv_Myr2v3<fp_to_uint,".CP.8.f32.u8s $dst.0 $src.$pos",       f32,  v2f32, VRF64_l,   i8,   IRF8_q0 >;

  // F16 -> U8
  def CMU_CPVI_ftou_v8f16_u8 :  CMU_CPVI_conv_Myr2v3<fp_to_uint,".CP.8.f16.u8s $dst.0 $src.$pos",       f16,  v8f16,  VRF128,   i8,   IRF8_q0 >;
  def CMU_CPVI_ftou_v4f16_u8 :  CMU_CPVI_conv_Myr2v3<fp_to_uint,".CP.8.f16.u8s $dst.0 $src.$pos",       f16,  v4f16, VRF64_l,   i8,   IRF8_q0 >;


  // CMU_CP.UTOF
  // U32 -> F32
  //                                                      Opcode,                             asmStr, srcElemVT,  srcVT,   srcRC, dstVT, dstRC
  def CMU_CPVI_utof_v4u32_f32 :  CMU_CPVI_conv_Myr2v3<uint_to_fp,   ".CP.32.u32.f32 $dst $src.$pos",       i32,  v4i32,  VRF128,   f32,   IRF32 >;
  def CMU_CPVI_utof_v2u32_f32 :  CMU_CPVI_conv_Myr2v3<uint_to_fp,   ".CP.32.u32.f32 $dst $src.$pos",       i32,  v2i32, VRF64_l,   f32,   IRF32 >;

  // U32 -> F16
  //                                                      Opcode,                                  asmStr, srcElemVT,  srcVT,   srcRC, dstVT, dstRC
  def CMU_CPVI_utof_v4u32_f16 :  CMU_CPVI_conv_Myr2v3<uint_to_fp,   ".CP.16.u32.f16 $dst.0 $src.$pos",       i32,  v4i32,  VRF128,   f16,   IRF16_l >;
  def CMU_CPVI_utof_v2u32_f16 :  CMU_CPVI_conv_Myr2v3<uint_to_fp,   ".CP.16.u32.f16 $dst.0 $src.$pos",       i32,  v2i32, VRF64_l,   f16,   IRF16_l >;

  // U8 -> F32
  def CMU_CPVI_utof_v16i8_f32 :  CMU_CPVI_conv_Myr2v3<uint_to_fp,    ".CP.32.u8.f32 $dst $src.$pos",              i8,  v16i8,  VRF128,  f32,   IRF32 >;
  def CMU_CPVI_utof_v8i8_f32  :  CMU_CPVI_conv_Myr2v3<uint_to_fp,    ".CP.32.u8.f32 $dst $src.$pos",              i8,   v8i8, VRF64_l,  f32,   IRF32 >;

  // U8 -> F16
  def CMU_CPVI_utof_v16u8_f16 :  CMU_CPVI_conv_Myr2v3<uint_to_fp,    ".CP.16.u8.f16 $dst.0 $src.$pos",        i8,  v16i8,  VRF128,  f16,  IRF16_l >;
  def CMU_CPVI_utof_v8u8_f16  :  CMU_CPVI_conv_Myr2v3<uint_to_fp,    ".CP.16.u8.f16 $dst.0 $src.$pos",        i8,   v8i8, VRF64_l,  f16,  IRF16_l >;

  // CMU_CP.FTOU small vectors
  // v4f32 -> v4u8
  def CMU_CPVI_ftou_v4f32_v4u8 : CMU_CPVI_CPIV_vector_conv_Myr2v3<fp_to_uint,    ".CP.32.f32.u8s $dst $src", v4f32, VRF128,  v4i8,  IRF32>;
}

let Itinerary = II_CMU_CPIV_VRF_IRF, Predicates = [HasVRF128_Predicate] in
{
  //  CMU_CP.FTOF
  //  F16 -> F32
  //                                                   Opcode,                                     asmStr, srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPIV_ftof_f16_v4f32 : CMU_CPIV_conv_Myr2v3<fpextend,      ".CP.32.f16.f32 $dst.$pos $src.0",   f16, IRF16_l,       f32, v4f32, VRF128 >;
  def CMU_CPIV_ftof_f16_v2f32 : CMU_CPIV_conv_Myr2v3<fpextend,      ".CP.32.f16.f32 $dst.$pos $src.0",   f16, IRF16_l,       f32, v2f32,VRF64_l >;

  //  F32 -> F16
  def CMU_CPIV_ftof_f32_v8f16 : CMU_CPIV_conv_Myr2v3<fpround,             ".CP.16.f32.f16 $dst.$pos $src",   f32,   IRF32,       f16, v8f16, VRF128 >;
  def CMU_CPIV_ftof_f32_v4f16 : CMU_CPIV_conv_Myr2v3<fpround,             ".CP.16.f32.f16 $dst.$pos $src",   f32,   IRF32,       f16, v4f16,VRF64_l >;

  //  CMU_CP.ITOI
  //  I8 -> I32
  //                                                     Opcode,                                asmStr, srcVT,   srcRC, dstElemVT, dstVT,  dstRC
  def CMU_CPIV_itoi_i8_v4i32 : CMU_CPIV_conv_Myr2v3<      sext,   ".CP.32.i8.i32 $dst.$pos $src.0",    i8, IRF8_q0,       i32, v4i32, VRF128 >;
  def CMU_CPIV_itoi_i8_v2i32 : CMU_CPIV_conv_Myr2v3<      sext,   ".CP.32.i8.i32 $dst.$pos $src.0",    i8, IRF8_q0,       i32, v2i32,VRF64_l >;

  //  CMU_CP.UTOU
  //  U8 -> U32
  //                                                     Opcode,                                asmStr, srcVT,   srcRC, dstElemVT, dstVT,  dstRC
  def CMU_CPIV_utou_u8_v4u32 : CMU_CPIV_conv_Myr2v3<      zext,   ".CP.32.u8.u32 $dst.$pos $src.0",    i8, IRF8_q0,       i32, v4i32, VRF128 >;
  def CMU_CPIV_utou_u8_v2u32 : CMU_CPIV_conv_Myr2v3<      zext,   ".CP.32.u8.u32 $dst.$pos $src.0",    i8, IRF8_q0,       i32, v2i32,VRF64_l >;
}

let Itinerary = II_CMU_CPIV_VRF_IRF_FCONV, Predicates = [HasVRF128_Predicate] in
{
  //  CMU_CP.FTOI
  //  F32 -> I32
  //                                                     Opcode,                                   asmStr, srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPIV_ftoi_f32_v4i32 : CMU_CPIV_conv_Myr2v3<fp_to_sint,         ".CP.32.f32.i32s $dst.$pos $src",   f32,   IRF32,       i32, v4i32, VRF128 >;
  def CMU_CPIV_ftoi_f32_v2i32 : CMU_CPIV_conv_Myr2v3<fp_to_sint,         ".CP.32.f32.i32s $dst.$pos $src",   f32,   IRF32,       i32, v2i32,VRF64_l >;

  //  F16 -> I32
  def CMU_CPIV_ftoi_f16_v4i32 : CMU_CPIV_conv_Myr2v3<fp_to_sint,   ".CP.32.f16.i32s $dst.$pos $src.0",   f16, IRF16_l,       i32, v4i32, VRF128 >;
  def CMU_CPIV_ftoi_f16_v2i32 : CMU_CPIV_conv_Myr2v3<fp_to_sint,   ".CP.32.f16.i32s $dst.$pos $src.0",   f16, IRF16_l,       i32, v2i32,VRF64_l >;

  //  F32 -> I8
  def CMU_CPIV_ftoi_f32_v16i8 : CMU_CPIV_conv_Myr2v3<fp_to_sint,           ".CP.8.f32.i8s $dst.$pos $src",   f32,   IRF32,        i8, v16i8, VRF128 >;
  def CMU_CPIV_ftoi_f32_v8i8  : CMU_CPIV_conv_Myr2v3<fp_to_sint,           ".CP.8.f32.i8s $dst.$pos $src",   f32,   IRF32,        i8,  v8i8,VRF64_l >;

  //  F16 -> I8
  def CMU_CPIV_ftoi_f16_v16i8 : CMU_CPIV_conv_Myr2v3<fp_to_sint,     ".CP.8.f16.i8s $dst.$pos $src.0",   f16, IRF16_l,        i8, v16i8, VRF128 >;
  def CMU_CPIV_ftoi_f16_v8i8  : CMU_CPIV_conv_Myr2v3<fp_to_sint,     ".CP.8.f16.i8s $dst.$pos $src.0",   f16, IRF16_l,        i8,  v8i8,VRF64_l >;


  //  CMU_CP.ITOF
  //  I32 -> F32
  //                                                     Opcode,                                   asmStr, srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPIV_itof_i32_v4f32 : CMU_CPIV_conv_Myr2v3<sint_to_fp,   ".CP.32.i32.f32 $dst.$pos $src",   i32,   IRF32,       f32, v4f32, VRF128 >;
  def CMU_CPIV_itof_i32_v2f32 : CMU_CPIV_conv_Myr2v3<sint_to_fp,   ".CP.32.i32.f32 $dst.$pos $src",   i32,   IRF32,       f32, v2f32,VRF64_l >;

  //  I32 -> F16
  def CMU_CPIV_itof_i32_v8f16  : CMU_CPIV_conv_Myr2v3<sint_to_fp,   ".CP.16.i32.f16 $dst.$pos $src",   i32,   IRF32,       f16, v8f16, VRF128 >;
  def CMU_CPIV_itof_i32_v4f16  : CMU_CPIV_conv_Myr2v3<sint_to_fp,   ".CP.16.i32.f16 $dst.$pos $src",   i32,   IRF32,       f16, v4f16,VRF64_l >;

  //  I8 -> F32
  //                                                     Opcode,                                  asmStr, srcVT,   srcRC, dstElemVT, dstVT,  dstRC
  def CMU_CPIV_itof_i8_v4f32 : CMU_CPIV_conv_Myr2v3<sint_to_fp,     ".CP.32.i8.f32 $dst.$pos $src.0",    i8, IRF8_q0,       f32, v4f32, VRF128 >;
  def CMU_CPIV_itof_i8_v2f32 : CMU_CPIV_conv_Myr2v3<sint_to_fp,     ".CP.32.i8.f32 $dst.$pos $src.0",    i8, IRF8_q0,       f32, v2f32,VRF64_l >;

  //  I8 -> F16
  //                                                     Opcode,                                  asmStr, srcVT,   srcRC, dstElemVT, dstVT,  dstRC
  def CMU_CPIV_itof_i8_v8f16 : CMU_CPIV_conv_Myr2v3<sint_to_fp,     ".CP.16.i8.f16 $dst.$pos $src.0",    i8, IRF8_q0,       f16, v8f16, VRF128 >;
  def CMU_CPIV_itof_i8_v4f16 : CMU_CPIV_conv_Myr2v3<sint_to_fp,     ".CP.16.i8.f16 $dst.$pos $src.0",    i8, IRF8_q0,       f16, v4f16, VRF64_l>;

  // CMU_CP.ITOF small vectors
  // v4u8 -> v4f32
  def CMU_CPIV_utof_v4i8_v4f32 : CMU_CPVI_CPIV_vector_conv_Myr2v3<sint_to_fp, ".CP.128.i8.f32 $dst $src", v4i8, IRF32,   v4f32, VRF128>;
  def CMU_CPIV_utof_v2i8_v2f32 : CMU_CPVI_CPIV_vector_conv_Myr2v3<sint_to_fp, ".CP.128.i8.f32 $dst $src", v2i8, IRF16_l, v2f32, VRF64_l>;

  //  CMU_CP.FTOU
  //  F32 -> U32
  //                                                     Opcode,                                   asmStr, srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPIV_ftoi_f32_v4u32 : CMU_CPIV_conv_Myr2v3<fp_to_uint,         ".CP.32.f32.u32s $dst.$pos $src",   f32,   IRF32,       i32, v4i32, VRF128 >;
  def CMU_CPIV_ftoi_f32_v2u32 : CMU_CPIV_conv_Myr2v3<fp_to_uint,         ".CP.32.f32.u32s $dst.$pos $src",   f32,   IRF32,       i32, v2i32,VRF64_l >;

  //  F16 -> U32
  def CMU_CPIV_ftou_f16_v4u32 : CMU_CPIV_conv_Myr2v3<fp_to_uint,   ".CP.32.f16.u32s $dst.$pos $src.0",   f16, IRF16_l,       i32, v4i32, VRF128 >;
  def CMU_CPIV_ftou_f16_v2u32 : CMU_CPIV_conv_Myr2v3<fp_to_uint,   ".CP.32.f16.u32s $dst.$pos $src.0",   f16, IRF16_l,       i32, v2i32,VRF64_l >;


  //  F32 -> U8
  def CMU_CPIV_ftou_f32_v16u8 : CMU_CPIV_conv_Myr2v3<fp_to_uint,           ".CP.8.f32.u8s $dst.$pos $src",   f32,   IRF32,        i8, v16i8, VRF128 >;
  def CMU_CPIV_ftou_f32_v8u8  : CMU_CPIV_conv_Myr2v3<fp_to_uint,           ".CP.8.f32.u8s $dst.$pos $src",   f32,   IRF32,        i8,  v8i8,VRF64_l >;

  //  F16 -> U8
  def CMU_CPIV_ftou_f16_v16u8 : CMU_CPIV_conv_Myr2v3<fp_to_uint,     ".CP.8.f16.u8s $dst.$pos $src.0",   f16, IRF16_l,        i8, v16i8, VRF128 >;
  def CMU_CPIV_ftou_f16_v8u8  : CMU_CPIV_conv_Myr2v3<fp_to_uint,     ".CP.8.f16.u8s $dst.$pos $src.0",   f16, IRF16_l,        i8,  v8i8,VRF64_l >;

  //  CMU_CP.UTOF
  //  U32 -> F32
  //                                                     Opcode,                                   asmStr, srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPIV_utof_u32_v4f32 : CMU_CPIV_conv_Myr2v3<uint_to_fp,   ".CP.32.u32.f32 $dst.$pos $src",   i32,   IRF32,       f32, v4f32, VRF128 >;
  def CMU_CPIV_utof_u32_v2f32 : CMU_CPIV_conv_Myr2v3<uint_to_fp,   ".CP.32.u32.f32 $dst.$pos $src",   i32,   IRF32,       f32, v2f32,VRF64_l >;

  //  U32 -> F16
  def CMU_CPIV_utof_u32_v8f16  : CMU_CPIV_conv_Myr2v3<uint_to_fp,   ".CP.16.u32.f16 $dst.$pos $src",   i32,   IRF32,       f16, v8f16, VRF128 >;
  def CMU_CPIV_utof_u32_v4f16  : CMU_CPIV_conv_Myr2v3<uint_to_fp,   ".CP.16.u32.f16 $dst.$pos $src",   i32,   IRF32,       f16, v4f16,VRF64_l >;

  //  I8 -> F32
  //                                                     Opcode,                                  asmStr, srcVT,   srcRC, dstElemVT, dstVT,  dstRC
  def CMU_CPIV_utof_u8_v4f32 : CMU_CPIV_conv_Myr2v3<uint_to_fp,     ".CP.32.u8.f32 $dst.$pos $src.0",    i8, IRF8_q0,       f32, v4f32, VRF128 >;
  def CMU_CPIV_utof_u8_v2f32 : CMU_CPIV_conv_Myr2v3<uint_to_fp,     ".CP.32.u8.f32 $dst.$pos $src.0",    i8, IRF8_q0,       f32, v2f32,VRF64_l >;

  //  U8 -> F16
  //                                                     Opcode,                                  asmStr, srcVT,   srcRC, dstElemVT, dstVT,  dstRC
  def CMU_CPIV_utof_u8_v8f16 : CMU_CPIV_conv_Myr2v3<uint_to_fp,     ".CP.16.u8.f16 $dst.$pos $src.0",    i8, IRF8_q0,       f16, v8f16, VRF128 >;
  def CMU_CPIV_utof_u8_v4f16 : CMU_CPIV_conv_Myr2v3<uint_to_fp,     ".CP.16.u8.f16 $dst.$pos $src.0",    i8, IRF8_q0,       f16, v4f16, VRF64_l>;

  // CMU_CP.UTOF small vectors
  // v4u8 -> v4f32
  def CMU_CPIV_utof_v4u8_v4f32 : CMU_CPVI_CPIV_vector_conv_Myr2v3<uint_to_fp, ".CP.128.u8.f32 $dst $src", v4i8, IRF32,   v4f32, VRF128>;
  def CMU_CPIV_utof_v2u8_v2f32 : CMU_CPVI_CPIV_vector_conv_Myr2v3<uint_to_fp, ".CP.128.u8.f32 $dst $src", v2i8, IRF16_l, v2f32, VRF64_l>;
}

let Itinerary = II_CMU_CPVV_VRF, Predicates = [HasVRF128_Predicate] in
{
  //  CMU_CP.UTOU
  //  U8-> U32
  //                                                       Opcode,                                asmStr,srcElemVT,  srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPVV_utou_v16u8_v4u32 : CMU_CPVV_conv_Myr2v3<      zext, ".CP.32.u8.u32 $dst.$dpos $src.$spos",       i8,  v16i8,  VRF128,       i32, v4i32, VRF128 >;
  def CMU_CPVV_utou_v16u8_v2u32 : CMU_CPVV_conv_Myr2v3<      zext, ".CP.32.u8.u32 $dst.$dpos $src.$spos",       i8,  v16i8,  VRF128,       i32, v2i32,VRF64_l >;
  def CMU_CPVV_utou_v8u8_v4u32 : CMU_CPVV_conv_Myr2v3<       zext, ".CP.32.u8.u32 $dst.$dpos $src.$spos",       i8,   v8i8, VRF64_l,       i32, v4i32, VRF128 >;
  def CMU_CPVV_utou_v8u8_v2u32 : CMU_CPVV_conv_Myr2v3<       zext, ".CP.32.u8.u32 $dst.$dpos $src.$spos",       i8,   v8i8, VRF64_l,       i32, v2i32,VRF64_l >;

  //  CMU_CP.FTOF
  //  F16 -> f32
  //                                                     Opcode,                                   asmStr, srcElemVT,  srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPVV_ftof_v8f16_v4f32 : CMU_CPVV_conv_Myr2v3<fpextend,   ".CP.32.f16.f32 $dst.$dpos $src.$spos",       f16,  v8f16,  VRF128,       f32, v4f32, VRF128 >;
  def CMU_CPVV_ftof_v8f16_v2f32 : CMU_CPVV_conv_Myr2v3<fpextend,   ".CP.32.f16.f32 $dst.$dpos $src.$spos",       f16,  v8f16,  VRF128,       f32, v2f32,VRF64_l >;
  def CMU_CPVV_ftof_v4f16_v4f32 : CMU_CPVV_conv_Myr2v3<fpextend,   ".CP.32.f16.f32 $dst.$dpos $src.$spos",       f16,  v4f16, VRF64_l,       f32, v4f32, VRF128 >;
  def CMU_CPVV_ftof_v4f16_v2f32 : CMU_CPVV_conv_Myr2v3<fpextend,   ".CP.32.f16.f32 $dst.$dpos $src.$spos",       f16,  v4f16, VRF64_l,       f32, v2f32,VRF64_l >;

  //  F32 -> F16
  def CMU_CPVV_ftof_v4f32_v8f16 :  CMU_CPVV_conv_Myr2v3<fpround,   ".CP.16.f32.f16 $dst.$dpos $src.$spos",       f32,  v4f32,  VRF128,       f16, v8f16, VRF128 >;
  def CMU_CPVV_ftof_v2f32_v8f16 :  CMU_CPVV_conv_Myr2v3<fpround,   ".CP.16.f32.f16 $dst.$dpos $src.$spos",       f32,  v2f32, VRF64_l,       f16, v8f16, VRF128 >;
  def CMU_CPVV_ftof_v4f32_v4f16 :  CMU_CPVV_conv_Myr2v3<fpround,   ".CP.16.f32.f16 $dst.$dpos $src.$spos",       f32,  v4f32,  VRF128,       f16, v4f16,VRF64_l >;
  def CMU_CPVV_ftof_v2f32_v4f16 :  CMU_CPVV_conv_Myr2v3<fpround,   ".CP.16.f32.f16 $dst.$dpos $src.$spos",       f32,  v2f32, VRF64_l,       f16, v4f16,VRF64_l >;

  //  CMU_CP.ITOI
  //  I8-> I32
  //                                                       Opcode,                                asmStr,srcElemVT,  srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPVV_itoi_v16i8_v4i32 : CMU_CPVV_conv_Myr2v3<      sext, ".CP.32.i8.i32 $dst.$dpos $src.$spos",       i8,  v16i8,  VRF128,       i32, v4i32, VRF128 >;
  def CMU_CPVV_itoi_v16i8_v2i32 : CMU_CPVV_conv_Myr2v3<      sext, ".CP.32.i8.i32 $dst.$dpos $src.$spos",       i8,  v16i8,  VRF128,       i32, v2i32,VRF64_l >;
  def CMU_CPVV_itoi_v8i8_v4i32 : CMU_CPVV_conv_Myr2v3<       sext, ".CP.32.i8.i32 $dst.$dpos $src.$spos",       i8,   v8i8, VRF64_l,       i32, v4i32, VRF128 >;
  def CMU_CPVV_itoi_v8i8_v2i32 : CMU_CPVV_conv_Myr2v3<       sext, ".CP.32.i8.i32 $dst.$dpos $src.$spos",       i8,   v8i8, VRF64_l,       i32, v2i32,VRF64_l >;

  //  I32 -> I8s
  def CMU_CPVV_itoi_v4i32_v16i8 : CMU_CPVV_conv_Myr2v3<     trunc, ".CP.8 $dst.$dpos $src.(4 * $spos)",         i32,  v4i32,  VRF128,        i8, v16i8, VRF128 >;
  def CMU_CPVV_itoi_v4i32_v8i8  : CMU_CPVV_conv_Myr2v3<     trunc, ".CP.8 $dst.$dpos $src.(4 * $spos)",         i32,  v4i32,  VRF128,        i8, v8i8,VRF64_l >; 
  def CMU_CPVV_itoi_v2i32_v16i8 : CMU_CPVV_conv_Myr2v3<     trunc, ".CP.8 $dst.$dpos $src.(4 * $spos)",         i32,  v2i32, VRF64_l,        i8, v16i8, VRF128 >;
  def CMU_CPVV_itoi_v2i32_v8i8  : CMU_CPVV_conv_Myr2v3<     trunc, ".CP.8 $dst.$dpos $src.(4 * $spos)",         i32,  v2i32, VRF64_l,        i8, v8i8,VRF64_l >; 
}

let Itinerary = II_CMU_CPVV_VRF_CONV, Predicates = [HasVRF128_Predicate] in
{
  //  CMU_CP.FTOI
  //  F32 -> I32
  //                                                     Opcode,                                   asmStr,  srcElemVT,  srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPVV_ftoi_v4f32_v4i32 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.32.f32.i32s $dst.$dpos $src.$spos",       f32,  v4f32,  VRF128,       i32, v4i32, VRF128 >;
  def CMU_CPVV_ftoi_v4f32_v2i32 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.32.f32.i32s $dst.$dpos $src.$spos",       f32,  v4f32,  VRF128,       i32, v2i32,VRF64_l >;
  def CMU_CPVV_ftoi_v2f32_v4i32 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.32.f32.i32s $dst.$dpos $src.$spos",       f32,  v2f32, VRF64_l,       i32, v4i32, VRF128 >;
  def CMU_CPVV_ftoi_v2f32_v2i32 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.32.f32.i32s $dst.$dpos $src.$spos",       f32,  v2f32, VRF64_l,       i32, v2i32,VRF64_l >;

  //  F16 -> I32
  def CMU_CPVV_ftoi_v8f16_v4i32 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.32.f16.i32s $dst.$dpos $src.$spos",       f16,  v8f16,  VRF128,       i32, v4i32, VRF128 >;
  def CMU_CPVV_ftoi_v8f16_v2i32 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.32.f16.i32s $dst.$dpos $src.$spos",       f16,  v8f16,  VRF128,       i32, v2i32,VRF64_l >;
  def CMU_CPVV_ftoi_v4f16_v4i32 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.32.f16.i32s $dst.$dpos $src.$spos",       f16,  v4f16, VRF64_l,       i32, v4i32, VRF128 >;
  def CMU_CPVV_ftoi_v4f16_v2i32 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.32.f16.i32s $dst.$dpos $src.$spos",       f16,  v4f16, VRF64_l,       i32, v2i32,VRF64_l >;

  //  F32 -> I8
  def CMU_CPVV_ftoi_v4f32_v16i8 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.8.f32.i8s $dst.$dpos $src.$spos",         f32,  v4f32,  VRF128,        i8, v16i8, VRF128 >;
  def CMU_CPVV_ftoi_v4f32_v8i8  : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.8.f32.i8s $dst.$dpos $src.$spos",         f32,  v4f32,  VRF128,        i8, v8i8,VRF64_l >; 
  def CMU_CPVV_ftoi_v2f32_v16i8 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.8.f32.i8s $dst.$dpos $src.$spos",         f32,  v2f32, VRF64_l,        i8, v16i8, VRF128 >;
  def CMU_CPVV_ftoi_v2f32_v8i8  : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.8.f32.i8s $dst.$dpos $src.$spos",         f32,  v2f32, VRF64_l,        i8, v8i8,VRF64_l >; 

  //  F16 -> I8
  def CMU_CPVV_ftoi_v8f16_v16i8 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.8.f16.i8s $dst.$dpos $src.$spos",         f16,  v8f16,  VRF128,        i8, v16i8, VRF128 >;
  def CMU_CPVV_ftoi_v8f16_v8i8  : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.8.f16.i8s $dst.$dpos $src.$spos",         f16,  v8f16,  VRF128,        i8, v8i8,VRF64_l >;
  def CMU_CPVV_ftoi_v4f16_v16i8 : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.8.f16.i8s $dst.$dpos $src.$spos",         f16,  v4f16, VRF64_l,        i8, v16i8, VRF128 >;
  def CMU_CPVV_ftoi_v4f16_v8i8  : CMU_CPVV_conv_Myr2v3<fp_to_sint, ".CP.8.f16.i8s $dst.$dpos $src.$spos",         f16,  v4f16, VRF64_l,        i8, v8i8,VRF64_l >;

  //  CMU_CP.ITOF
  //  I32 -> F32
  //                                                     Opcode,                                   asmStr,  srcElemVT,  srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPVV_itof_v4i32_v4f32 : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.32.i32.f32 $dst.$dpos $src.$spos",       i32,  v4i32,  VRF128,       f32, v4f32, VRF128 >;
  def CMU_CPVV_itof_v4i32_v2f32 : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.32.i32.f32 $dst.$dpos $src.$spos",       i32,  v4i32,  VRF128,       f32, v2f32,VRF64_l >;
  def CMU_CPVV_itof_v2i32_v4f32 : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.32.i32.f32 $dst.$dpos $src.$spos",       i32,  v2i32, VRF64_l,       f32, v4f32, VRF128 >;
  def CMU_CPVV_itof_v2i32_v2f32 : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.32.i32.f32 $dst.$dpos $src.$spos",       i32,  v2i32, VRF64_l,       f32, v2f32,VRF64_l >;

  //  I32 -> F16
  def CMU_CPVV_itof_v4i32_v8f16 : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.16.i32.f16 $dst.$dpos $src.$spos",       i32,  v4i32,  VRF128,       f16, v8f16, VRF128 >;
  def CMU_CPVV_itof_v4i32_v4f16 : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.16.i32.f16 $dst.$dpos $src.$spos",       i32,  v4i32,  VRF128,       f16, v4f16,VRF64_l>;
  def CMU_CPVV_itof_v232_v8f16 : CMU_CPVV_conv_Myr2v3<sint_to_fp,  ".CP.16.i32.f16 $dst.$dpos $src.$spos",       i32,  v2i32, VRF64_l,       f16, v8f16, VRF128 >;
  def CMU_CPVV_itof_v232_v4f16 : CMU_CPVV_conv_Myr2v3<sint_to_fp,  ".CP.16.i32.f16 $dst.$dpos $src.$spos",       i32,  v2i32, VRF64_l,       f16, v4f16,VRF64_l>;

  //  I8 -> F32
  //                                                     Opcode,                                  asmStr,  srcElemVT,  srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPVV_itof_v16i8_v4f32 : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.32.i8.f32 $dst.$dpos $src.$spos",        i8,  v16i8,  VRF128,       f32, v4f32, VRF128 >;
  def CMU_CPVV_itof_v16i8_v2f32 : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.32.i8.f32 $dst.$dpos $src.$spos",        i8,  v16i8,  VRF128,       f32, v2f32,VRF64_l >;
  def CMU_CPVV_itof_v8i8_v4f32  : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.32.i8.f32 $dst.$dpos $src.$spos",        i8,   v8i8, VRF64_l,       f32, v4f32, VRF128 >;
  def CMU_CPVV_itof_v8i8_v2f32  : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.32.i8.f32 $dst.$dpos $src.$spos",        i8,   v8i8, VRF64_l,       f32, v2f32,VRF64_l >;

  //  I8 -> F16
  //                                                     Opcode,                                  asmStr,  srcElemVT,  srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPVV_itof_v16i8_v8f16 : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.16.i8.f16 $dst.$dpos $src.$spos",        i8,  v16i8,  VRF128,       f16, v8f16, VRF128 >;
  def CMU_CPVV_itof_v16i8_v4f16 : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.16.i8.f16 $dst.$dpos $src.$spos",        i8,  v16i8,  VRF128,       f16, v4f16,VRF64_l >;
  def CMU_CPVV_itof_v8i8_v8f16  : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.16.i8.f16 $dst.$dpos $src.$spos",        i8,   v8i8, VRF64_l,       f16, v8f16, VRF128 >;
  def CMU_CPVV_itof_v8i8_v4f16  : CMU_CPVV_conv_Myr2v3<sint_to_fp, ".CP.16.i8.f16 $dst.$dpos $src.$spos",        i8,   v8i8, VRF64_l,       f16, v4f16,VRF64_l >;

  //  CMU_CP.FTOU
  //  F32 -> U32
  //                                                     Opcode,                                   asmStr,  srcElemVT,  srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPVV_ftou_v4f32_v4u32 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.32.f32.u32s $dst.$dpos $src.$spos",       f32,  v4f32,  VRF128,       i32, v4i32, VRF128 >;
  def CMU_CPVV_ftou_v4f32_v2u32 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.32.f32.u32s $dst.$dpos $src.$spos",       f32,  v4f32,  VRF128,       i32, v2i32,VRF64_l >;
  def CMU_CPVV_ftou_v2f32_v4u32 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.32.f32.u32s $dst.$dpos $src.$spos",       f32,  v2f32, VRF64_l,       i32, v4i32, VRF128 >;
  def CMU_CPVV_ftou_v2f32_v2u32 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.32.f32.u32s $dst.$dpos $src.$spos",       f32,  v2f32, VRF64_l,       i32, v2i32,VRF64_l >;

  //  F16 -> U32
  def CMU_CPVV_ftou_v8f16_v4u32 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.32.f16.u32s $dst.$dpos $src.$spos",       f16,  v8f16,  VRF128,       i32, v4i32, VRF128 >;
  def CMU_CPVV_ftou_v8f16_v2u32 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.32.f16.u32s $dst.$dpos $src.$spos",       f16,  v8f16,  VRF128,       i32, v2i32,VRF64_l >;
  def CMU_CPVV_ftou_v4f16_v4u32 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.32.f16.u32s $dst.$dpos $src.$spos",       f16,  v4f16, VRF64_l,       i32, v4i32, VRF128 >;
  def CMU_CPVV_ftou_v4f16_v2u32 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.32.f16.u32s $dst.$dpos $src.$spos",       f16,  v4f16, VRF64_l,       i32, v2i32,VRF64_l >;

  //  F32 -> U8
  def CMU_CPVV_ftou_v4f32_v16u8 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.8.f32.u8s $dst.$dpos $src.$spos",         f32,  v4f32,  VRF128,        i8, v16i8, VRF128 >;
  def CMU_CPVV_ftou_v4f32_v8u8  : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.8.f32.u8s $dst.$dpos $src.$spos",         f32,  v4f32,  VRF128,        i8, v8i8,VRF64_l >; 
  def CMU_CPVV_ftou_v2f32_v16u8 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.8.f32.u8s $dst.$dpos $src.$spos",         f32,  v2f32, VRF64_l,        i8, v16i8, VRF128 >;
  def CMU_CPVV_ftou_v2f32_v8u8  : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.8.f32.u8s $dst.$dpos $src.$spos",         f32,  v2f32, VRF64_l,        i8, v8i8,VRF64_l >; 

  //  F16 -> U8
  def CMU_CPVV_ftou_v8f16_v16u8 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.8.f16.u8s $dst.$dpos $src.$spos",         f16,  v8f16,  VRF128,        i8, v16i8, VRF128 >;
  def CMU_CPVV_ftou_v8f16_v8u8  : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.8.f16.u8s $dst.$dpos $src.$spos",         f16,  v8f16,  VRF128,        i8, v8i8,VRF64_l >;
  def CMU_CPVV_ftou_v4f16_v16u8 : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.8.f16.u8s $dst.$dpos $src.$spos",         f16,  v4f16, VRF64_l,        i8, v16i8, VRF128 >;
  def CMU_CPVV_ftou_v4f16_v8u8  : CMU_CPVV_conv_Myr2v3<fp_to_uint, ".CP.8.f16.u8s $dst.$dpos $src.$spos",         f16,  v4f16, VRF64_l,        i8, v8i8,VRF64_l >;

  //  CMU_CP.UTOF
  //  U32 -> F32
  //                                                     Opcode,                                   asmStr,  srcElemVT,  srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPVV_utof_v4u32_v4f32 : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.32.u32.f32 $dst.$dpos $src.$spos",       i32,  v4i32,  VRF128,       f32, v4f32, VRF128 >;
  def CMU_CPVV_utof_v4u32_v2f32 : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.32.u32.f32 $dst.$dpos $src.$spos",       i32,  v4i32,  VRF128,       f32, v2f32,VRF64_l >;
  def CMU_CPVV_utof_v2u32_v4f32 : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.32.u32.f32 $dst.$dpos $src.$spos",       i32,  v2i32, VRF64_l,       f32, v4f32, VRF128 >;
  def CMU_CPVV_utof_v2u32_v2f32 : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.32.u32.f32 $dst.$dpos $src.$spos",       i32,  v2i32, VRF64_l,       f32, v2f32,VRF64_l >;

  //  U32 -> F16
  def CMU_CPVV_utof_v4u32_v8f16 : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.16.u32.f16 $dst.$dpos $src.$spos",       i32,  v4i32,  VRF128,       f16, v8f16, VRF128 >;
  def CMU_CPVV_utof_v4u32_v4f16 : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.16.u32.f16 $dst.$dpos $src.$spos",       i32,  v4i32,  VRF128,       f16, v4f16,VRF64_l>;
  def CMU_CPVV_utof_v2u32_v8f16 : CMU_CPVV_conv_Myr2v3<uint_to_fp,  ".CP.16.u32.f16 $dst.$dpos $src.$spos",       i32,  v2i32, VRF64_l,       f16, v8f16, VRF128 >;
  def CMU_CPVV_utof_v2u32_v4f16 : CMU_CPVV_conv_Myr2v3<uint_to_fp,  ".CP.16.u32.f16 $dst.$dpos $src.$spos",       i32,  v2i32, VRF64_l,       f16, v4f16,VRF64_l>;

  //  U8 -> F32
  //                                                     Opcode,                                  asmStr,  srcElemVT,  srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPVV_utof_v16u8_v4f32 : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.32.u8.f32 $dst.$dpos $src.$spos",        i8,  v16i8,  VRF128,       f32, v4f32, VRF128 >;
  def CMU_CPVV_utof_v16u8_v2f32 : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.32.u8.f32 $dst.$dpos $src.$spos",        i8,  v16i8,  VRF128,       f32, v2f32,VRF64_l >;
  def CMU_CPVV_utof_v8u8_v4f32  : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.32.u8.f32 $dst.$dpos $src.$spos",        i8,   v8i8, VRF64_l,       f32, v4f32, VRF128 >;
  def CMU_CPVV_utof_v8u8_v2f32  : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.32.u8.f32 $dst.$dpos $src.$spos",        i8,   v8i8, VRF64_l,       f32, v2f32,VRF64_l >;

  //  U8 -> F16
  //                                                     Opcode,                                  asmStr,  srcElemVT,  srcVT,   srcRC, dstElemVT, dstVT, dstRC
  def CMU_CPVV_utof_v16u8_v8f16 : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.16.u8.f16 $dst.$dpos $src.$spos",        i8,  v16i8,  VRF128,       f16, v8f16, VRF128 >;
  def CMU_CPVV_utof_v16u8_v4f16 : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.16.u8.f16 $dst.$dpos $src.$spos",        i8,  v16i8,  VRF128,       f16, v4f16,VRF64_l >;
  def CMU_CPVV_utof_v8u8_v8f16  : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.16.u8.f16 $dst.$dpos $src.$spos",        i8,   v8i8, VRF64_l,       f16, v8f16, VRF128 >;
  def CMU_CPVV_utof_v8u8_v4f16  : CMU_CPVV_conv_Myr2v3<uint_to_fp, ".CP.16.u8.f16 $dst.$dpos $src.$spos",        i8,   v8i8, VRF64_l,       f16, v4f16,VRF64_l >;

}

// floating point to floating point converting copies
// IRF variants
let Itinerary = II_CMU_CPII_IRF in
{
  // f16 --> f32
  def CMU_CPII_f16_f32 : CMU_CP<(outs IRF32:$dst), (ins IRF16_l:$src),
                                ".CP.32.f16.f32 $dst $src.0",
                                [(set (f32 IRF32:$dst), (fpextend (f16 IRF16_l:$src)))]>;

  // f32 --> f16
  def CMU_CPII_f32_f16 : CMU_CP<(outs IRF16_l:$dst), (ins IRF32:$src),
                                ".CP.16.f32.f16 $dst.0 $src",
                                [(set (f16 IRF16_l:$dst), (fpround (f32 IRF32:$src)))]>;
}

// integer to integer converting copies
class CMU_CPII32_extend<SDNode opc, RegisterClass srcRC, RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr, string srcLane = ""> :
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src),
         !strconcat(".CP.32", asmstr, " $dst $src", srcLane),
         [(set (dstVT dstRC:$dst), (opc (srcVT srcRC:$src)))]>;
class CMU_CPII16_extend<SDNode opc, RegisterClass srcRC, RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr> :
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src),
         !strconcat(".CP.16", asmstr, " $dst.0 $src.0"),
         [(set (dstVT dstRC:$dst), (opc (srcVT srcRC:$src)))]>;

let Itinerary = II_CMU_CPII_IRF, isAsCheapAsAMove = 1 in
{
  // Widening needs to be more selective
  //  FIXME: Movidius - These are the legacy versions - remove when obsoleted
  def CMU_CPII_i16_i32_anyext    : CMU_CPII32_extend<anyext, IRF16_l, IRF32,   i16, i32,    "">;
  def CMU_CPII_i8_i32_anyext     : CMU_CPII32_extend<anyext, IRF8_q0, IRF32,   i8,  i32,    "">;
  def CMU_CPII_i8_i16_anyext     : CMU_CPII16_extend<anyext, IRF8_q0, IRF16_l, i8,  i16,    "">;
    
  // Sign extend
  def CMU_CPII_i16_i32_sext      : CMU_CPII32_extend<sext,   IRF16_l, IRF32,   i16, i32,    ".i16.i32", ".0">;
  def CMU_CPII_i8_i32_sext       : CMU_CPII32_extend<sext,   IRF8_q0, IRF32,   i8,  i32,    ".i8.i32",  ".0">;
  def CMU_CPII_i8_i16_sext       : CMU_CPII16_extend<sext,   IRF8_q0, IRF16_l, i8,  i16,    ".i8.i16">;

  // Zero extend
  def CMU_CPII_u8_u32_zext_Myr2v3: CMU_CPII32_extend<zext,   IRF8_q0, IRF32,   i8,  i32,    ".u8.u32",  ".0">;

}

// integer to float conversions
// IRF variants
let Itinerary = II_CMU_CPII_IRF in
{ // These are used by moviCompile as part of floating point to unsigned int conversions and vice-versa
  // FIXME: Movidius - These look like they could be the source of some of the identity copies generated by moviCompile
  // Int to float reinterpret
  def CMU_CPII_bitconvert_i16f16     : CMU_CPII_XX_16_OpC<bitconvert, IRF16_l, IRF16_l, i16,   f16>;
  def CMU_CPII_bitconvert_i32f32     : CMU_CPII_32_32_OpC<bitconvert,                   i32,   f32>;
  def CMU_CPII_bitconvert_v2i16v2f16 : CMU_CPII_32_32_OpC<bitconvert,                   v2i16, v2f16>;
}

let Itinerary = II_CMU_CPII_IRF_FLOAT_INT in
{ // Conversion between floating-point and integer
  def CMU_CPII_i32f32       : CMU_CPII_32_32_OpC<sint_to_fp,                   i32,   f32,   ".i32.f32">;
  def CMU_CPII_u32f32_Myr2v3: CMU_CPII_32_32_OpC<uint_to_fp,                   i32,   f32,   ".u32.f32">;
  def CMU_CPII_i32f16       : CMU_CPII_32_16_OpC<sint_to_fp,          IRF16_l, i32,   f16,   ".i32.f16">;
  def CMU_CPII_u32f16_Myr2v3: CMU_CPII_32_16_OpC<uint_to_fp,          IRF16_l, i32,   f16,   ".u32.f16">;
  def CMU_CPII_i8f32_Myr2v3 : CMU_CPII_XX_32_OpC<sint_to_fp, IRF8_q0,           i8,   f32,   ".i8.f32" >;
  def CMU_CPII_u8f32_Myr2v3 : CMU_CPII_XX_32_OpC<uint_to_fp, IRF8_q0,           i8,   f32,   ".u8.f32" >;
  def CMU_CPII_i16f32       : CMU_CPII_XX_32_OpC<sint_to_fp, IRF16_l,          i16,   f32,   ".i16.f32">;
  def CMU_CPII_i16f16       : CMU_CPII_XX_16_OpC<sint_to_fp, IRF16_l, IRF16_l, i16,   f16,   ".i16.f16">;
  def CMU_CPII_i8f16        : CMU_CPII_XX_16_OpC<sint_to_fp, IRF8_q0, IRF16_l, i8,    f16,   ".i8.f16">;
  def CMU_CPII_u8f16_Myr2v3 : CMU_CPII_XX_16_OpC<uint_to_fp, IRF8_q0, IRF16_l, i8,    f16,   ".u8.f16">;

  def CMU_CPII_v2i16_v2f16  : CMU_CPII_32_32_OpC<sint_to_fp,                   v2i16, v2f16, ".i16.f16">;
  def CMU_CPII_u8f_f16_Raw  : CMU_CP<(outs IRF16_l:$dst), (ins IRF8_q0:$src),
                                     ".CP.16.u8f.f16 $dst.0 $src.0", []>;
  def CMU_CPII_f16_u8fs_Raw : CMU_CP<(outs IRF8_q0:$dst), (ins IRF16_l:$src),
                                     ".CP.8.f16.u8fs $dst.0 $src.0", []>;
  def CMU_CPII_f16_u8s_Raw : CMU_CP<(outs IRF8_q0:$dst), (ins IRF16_l:$src),
                                     ".CP.8.f16.u8s $dst.0 $src.0", []>;
}

// float to integer conversions
// IRF variants
let Itinerary = II_CMU_CPII_IRF in
{ // These are used by moviCompile as part of floating point to unsigned int conversions and vice-versa
  // FIXME: Movidius - These look like they could be the source of some of the identity copies generated by moviCompile
  // Float to int reinterpret
  def CMU_CPII_bitconvert_f32i32     : CMU_CPII_32_32_OpC<bitconvert,                   f32,   i32>;
  def CMU_CPII_bitconvert_f16i16     : CMU_CPII_XX_16_OpC<bitconvert, IRF16_l, IRF16_l, f16,   i16>;
  def CMU_CPII_bitconvert_v2f16v2i16 : CMU_CPII_32_32_OpC<bitconvert,                   v2f16, v2i16>;
}

let Itinerary = II_CMU_CPII_IRF_FLOAT_INT in
{ // Conversion between floating-point and integer
  def CMU_CPII_v2f16_v2i16 : CMU_CPII_32_32_OpC<fp_to_sint,                   v2f16, v2i16, ".f16.i16s">;

  def CMU_CPII_f32i32      : CMU_CPII_32_32_OpC<fp_to_sint,                   f32,   i32,   ".f32.i32s">;
  def CMU_CPII_f32i16      : CMU_CPII_32_16_OpC<fp_to_sint,          IRF16_l, f32,   i16,   ".f32.i16s">;
  def CMU_CPII_f32u8_Myr2v3: CMU_CPII_32_8_OpC< fp_to_uint,           IRF8_q0, f32,   i8,   ".f32.u8s">;
  def CMU_CPII_f16u8_Myr2v3: CMU_CPII_XX_8_OpC< fp_to_uint, IRF16_l,  IRF8_q0, f16,   i8,   ".f16.u8s">;
  def CMU_CPII_f16i32      : CMU_CPII_XX_32_OpC<fp_to_sint, IRF16_l,          f16,   i32,   ".f16.i32s">;
  def CMU_CPII_f16i16      : CMU_CPII_XX_16_OpC<fp_to_sint, IRF16_l, IRF16_l, f16,   i16,   ".f16.i16s">;
  def CMU_CPII_f16i8       : CMU_CPII_XX_8_OpC< fp_to_sint, IRF16_l, IRF8_q0, f16,   i8,    ".f16.i8s">;

  def CMU_CPII_f32i8_Myr2v3  : CMU_CPII_32_8_OpC<fp_to_sint,           IRF8_q0, f32,    i8,   ".f32.i8s" >;
  def CMU_CPII_f32u32_Myr2v3 : CMU_CPII_32_32_OpC<fp_to_uint,                   f32,   i32,   ".f32.u32s">;
  def CMU_CPII_f16u32_Myr2v3 : CMU_CPII_XX_32_OpC<fp_to_uint, IRF16_l,          f16,   i32,   ".f16.u32s">;
}

//
// 1.4.4.10 - CMU.CPIT
//   Copy IRF to TRF.
//

let Itinerary = II_CMU_CPIT_IRF, isAsCheapAsAMove = 1, isBarrier = 1 in {
  // FIXME: Movidius - this is incorrect.  There are two schedules for 'CMU.CPIT', most registers
  // are written with a latency of 0-cycles, but B_RFB, B_STATE, B_MREPS, B_LEND, B_LBEG and B_SREPS
  // have a latency of 1-cycle.  This will require refactoring the register information
  def CMU_CPIT : CMU_CP<(outs), (ins TRF:$dst, IRF32:$src), ".CPIT $dst $src", []>;
}

// Special definition for calling preemption handler, without the barrier
let Itinerary = II_CMU_CPIT_IRF, isAsCheapAsAMove = 1 in {
  def CMU_CPIT_preemption : CMU_CP<(outs), (ins TRF:$dst, IRF32:$src), ".CPIT $dst $src", []>;
}

//
// 1.4.4.11 - CMU.CPIV (and CPSV)
//   Copy IRF to VRF.
//

// IRF variants
let Itinerary = II_CMU_CPIV_VRF_IRF, isAsCheapAsAMove = 1, Predicates = [HasVRF128_Predicate] in {
  def CMU_CPIV_x32 : CMU_CP<(outs VRF128:$dst), (ins VRF128:$vsrc, IRF32:$src, i8imm:$col), ".CP.32 $dst.$col $src", []> {
    let Constraints = "$vsrc = $dst";
  }

  let Predicates = [HasVRF128_Predicate] in {
    def CMU_CPIV_x8_q0_v8i8_l_Myr2v3    : CMU_CP_insert<VRF64_l, IRF8_q0, i8, v8i8,  ".8",".0">;
    def CMU_CPIV_x8_q0_v16i8_Myr2v3     : CMU_CP_insert<VRF128,  IRF8_q0, i8, v16i8, ".8",".0">;
  }

  def CMU_CPIV_x32_v4i32          : CMU_CP_insert<VRF128,   IRF32,   i32,   v4i32, ".32", "">;
  def CMU_CPIV_x32_v2i32          : CMU_CP_insert<VRF64_l,  IRF32,   i32,   v2i32, ".32", "">;

  def CMU_CPIV_x32_v4i8           : CMU_CP_insert<VRF32_q0, IRF32,   v4i8,  v4i8,  ".32", "">;
  def CMU_CPIV_x32_v2i16          : CMU_CP_insert<VRF32_q0, IRF32,   v2i16, v2i16, ".32", "">;

  def CMU_CPIV_x32_v4f32          : CMU_CP_insert<VRF128,   IRF32,   f32,   v4f32, ".32", "">;
  def CMU_CPIV_x32_v2f32          : CMU_CP_insert<VRF64_l,  IRF32,   f32,   v2f32, ".32", "">;
  def CMU_CPIV_x32_v2f16          : CMU_CP_insert<VRF32_q0, IRF32,   v2f16, v2f16, ".32", "">;

  let Predicates = [HasVRF128_Predicate] in {
    def CMU_CPIV_x16_v8i16_l_Myr2v3 : CMU_CP_insert<VRF128,   IRF16_l, i16,   v8i16, ".16", ".0">;
    def CMU_CPIV_x16_v8i16_h_Myr2v3 : CMU_CP_insert<VRF128,   IRF16_h, i16,   v8i16, ".16", ".1">;
    def CMU_CPIV_x16_v4i16_l_Myr2v3 : CMU_CP_insert<VRF64_l,  IRF16_l, i16,   v4i16, ".16", ".0">;
    def CMU_CPIV_x16_v4i16_h_Myr2v3 : CMU_CP_insert<VRF64_l,  IRF16_h, i16,   v4i16, ".16", ".1">;

    def CMU_CPIV_x16_v8f16_l_Myr2v3 : CMU_CP_insert<VRF128,   IRF16_l, f16,   v8f16, ".16", ".0">;
    def CMU_CPIV_x16_v8f16_h_Myr2v3 : CMU_CP_insert<VRF128,   IRF16_h, f16,   v8f16, ".16", ".1">;
    def CMU_CPIV_x16_v4f16_l_Myr2v3 : CMU_CP_insert<VRF64_l,  IRF16_l, f16,   v4f16, ".16", ".0">;
    def CMU_CPIV_x16_v4f16_h_Myr2v3 : CMU_CP_insert<VRF64_l,  IRF16_h, f16,   v4f16, ".16", ".1">;
  }
}

//
// 1.4.4.12 - CMU.CPIVR (and CPSVR)
//   Copy IRF to VRF with repeat. Optional type conversion.
//

// IRF variants
class CMU_CPR<RegisterClass dstRC, RegisterClass srcRC,
              ValueType vt, ValueType st, string asmstr, string lane = ""> :
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src),
         !strconcat(asmstr, " $dst $src", lane),
         [(set (vt dstRC:$dst), (vt (splat (st srcRC:$src))))]>;

class CMU_CPR_Raw<RegisterClass dstRC, RegisterClass srcRC, string asmstr, string lane = ""> :
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src), !strconcat(asmstr, " $dst $src", lane), []>;

// CMU.CPIVR
let Itinerary = II_CMU_CPIV_VRF_IRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CPIVR_i8       : CMU_CPR<VRF128,   IRF8_q0, v16i8, i8,  ".CP.128.8.r",  ".0">;
  def CMU_CPIVR_i8_q1    : CMU_CPR<VRF128,   IRF8_q1, v16i8, i8,  ".CP.128.8.r",  ".1">;
  def CMU_CPIVR_i16      : CMU_CPR<VRF128,   IRF16_l, v8i16, i16, ".CP.128.16.r", ".0">;
  def CMU_CPIVR_i32      : CMU_CPR<VRF128,   IRF32,   v4i32, i32, ".CP.128.32.r">;
  def CMU_CPIVR_f16      : CMU_CPR<VRF128,   IRF16_l, v8f16, f16, ".CP.128.16.r", ".0">;
  def CMU_CPIVR_f32      : CMU_CPR<VRF128,   IRF32,   v4f32, f32, ".CP.128.32.r">;

  def CMU_CPIVR_v2i32    : CMU_CPR<VRF64_l,  IRF32,   v2i32, i32, ".CP.128.32.r">;
  def CMU_CPIVR_v8i8     : CMU_CPR<VRF64_l,  IRF8_q0, v8i8,  i8,  ".CP.128.8.r",  ".0">;
  def CMU_CPIVR_v8i8_q1  : CMU_CPR<VRF64_l,  IRF8_q1, v8i8,  i8,  ".CP.128.8.r",  ".1">;
  def CMU_CPIVR_v4i16    : CMU_CPR<VRF64_l,  IRF16_l, v4i16, i16, ".CP.128.16.r", ".0">;
  def CMU_CPIVR_v2f32    : CMU_CPR<VRF64_l,  IRF32,   v2f32, f32, ".CP.128.32.r">;
  def CMU_CPIVR_v4f16    : CMU_CPR<VRF64_l,  IRF16_l, v4f16, f16, ".CP.128.16.r", ".0">;

  def CMU_CPIVR_v4i8     : CMU_CPR<VRF32_q0, IRF8_q0, v4i8,  i8,  ".CP.128.8.r",  ".0">;
  def CMU_CPIVR_v2i16    : CMU_CPR<VRF32_q0, IRF16_l, v2i16, i16, ".CP.128.16.r", ".0">;

  def CMU_CPIVR_VRF16    : CMU_CPR_Raw<VRF16_e0, IRF16_l, ".CP.128.16.r", ".0">;
}

//
// 1.4.4.13 - CMU.CPTI
//   Copy TRF to IRF.
//

let Itinerary = II_CMU_CPTI_IRF, isAsCheapAsAMove = 1, isBarrier = 1 in {
  def CMU_CPTI : CMU_CP<(outs IRF32:$dst), (ins TRF:$src), ".CPTI $dst $src", []>;
}

// Special definition for calling preemption handler, without the barrier
let Itinerary = II_CMU_CPTI_IRF, isAsCheapAsAMove = 1 in {
  def CMU_CPTI_preemption : CMU_CP<(outs IRF32:$dst), (ins TRF:$src), ".CPTI $dst $src", []>;
}

//
// 1.4.4.14 - CMU.CPVCR
//   Copy VRF column to row.
//

let Itinerary = II_CMU_CPVCR_VRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CPVCR_imm_x32   : CMU_CP<(outs VRF128:$dst), (ins VRF128:$src, i8imm:$pos), ".CPVCR.x32 $dst $src.$pos",   []>;
  def CMU_CPVCR_imm_x16_l : CMU_CP<(outs VRF128:$dst), (ins VRF128:$src, i8imm:$pos), ".CPVCR.x16 $dst.l $src.$pos", []>;
  def CMU_CPVCR_imm_x16_h : CMU_CP<(outs VRF128:$dst), (ins VRF128:$src, i8imm:$pos), ".CPVCR.x16 $dst.h $src.$pos", []>;
}

//
// 1.4.4.15 - CMU.CPVI (and CPVS)
//   Copy VRF to IRF.
//

// dty = dest scalar value type
// vty = src vector value type
class CMU_CPVI<RegisterClass dstRC, RegisterClass srcRC, ValueType dty,
               ValueType vty, string asmstr, string index> :
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src, i8imm:$pos),
         !strconcat(asmstr, " $dst", index, " $src.$pos"),
         [(set (dty dstRC:$dst), (vector_extract (vty srcRC:$src), imm:$pos))]>;

// IRF variants
let Itinerary = II_CMU_CPVI_IRF_VRF, Predicates = [HasVRF128_Predicate] in
{ // FIXME: Movidius - Should use CMU_VI<...>, but it has a quite different interface
  def CMU_CPVI_f32       : CMU_CPVI<IRF32,   VRF128,   f32,   v4f32, ".CP.32", "">;
  def CMU_CPVI_f16_l     : CMU_CPVI<IRF16_l, VRF128,   f16,   v8f16, ".CP.16", ".0">;

  def CMU_CPVI_f32_v2f32 : CMU_CPVI<IRF32,   VRF64_l,  f32,   v2f32, ".CP.32", "">;
  def CMU_CPVI_v4f16_l   : CMU_CPVI<IRF16_l, VRF64_l,  f16,   v4f16, ".CP.16", ".0">;
}

let Itinerary = II_CMU_CPVI_IRF_VRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CPVI_x32            : CMU_CPVI<IRF32,   VRF128,   i32,   v4i32, ".CP.32", "">;
  def CMU_CPVI_x16_l          : CMU_CPVI<IRF16_l, VRF128,   i16,   v8i16, ".CP.16", ".0">;

  let Predicates = [HasVRF128_Predicate] in {
    def CMU_CPVI_x8_q0_v8i8_l_Myr2v3    : CMU_CPVI<IRF8_q0, VRF64_l,  i8,    v8i8, ".CP.8", ".0">;
    def CMU_CPVI_x8_q0_v168_Myr2v3      : CMU_CPVI<IRF8_q0, VRF128,   i8,   v16i8, ".CP.8", ".0">;
  }

  def CMU_CPVI_x32_v2i32      : CMU_CPVI<IRF32,   VRF64_l,  i32,   v2i32, ".CP.32", "">;
  def CMU_CPVI_v4i16_l        : CMU_CPVI<IRF16_l, VRF64_l,  i16,   v4i16, ".CP.16", ".0">;
}

//
// 1.8.4.11 - CMU.CPEI
// Myriad 2.3 CPEI instructions
// Vector element copy with indirect element specifiers

class CMU_CPEI_Insert<RegisterClass dstRC, RegisterClass srcRC, ValueType dty,
               ValueType vty, string asmstr, string index> :
  CMU_CP<(outs dstRC:$dst), (ins dstRC:$vsrc, srcRC:$src, IRF32:$pos),
    !strconcat(asmstr, " $dst", index, " $src $pos ${:comment} CPEI.Insert"),
    [(set (dty dstRC:$dst), (vector_insert (dty dstRC:$vsrc), (vty srcRC:$src), (i32 IRF32:$pos)))]>
{
  let Constraints = "$vsrc = $dst";
}


let Itinerary = II_CMU_CPEI_VRF_IRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CPEI_Insert_x32_v4i32_Myr2v3      : CMU_CPEI_Insert<VRF128,  IRF32,    v4i32,   i32, ".CP.32", "">;
  def CMU_CPEI_Insert_x32_v2i32_Myr2v3      : CMU_CPEI_Insert<VRF64_l, IRF32,    v2i32,   i32, ".CP.32", "">;

  def CMU_CPEI_Insert_x32_v4f32_Myr2v3      : CMU_CPEI_Insert<VRF128,  IRF32,    v4f32,   f32, ".CP.32", "">;
  def CMU_CPEI_Insert_x32_v2f32_Myr2v3      : CMU_CPEI_Insert<VRF64_l, IRF32,    v2f32,   f32, ".CP.32", "">;
}

class CMU_CPEI_Extract<RegisterClass dstRC, RegisterClass srcRC, ValueType dty,
               ValueType vty, string asmstr, string index> :
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src, IRF32:$pos),
         !strconcat(asmstr, " $dst", index, " $src $pos ${:comment} CPEI.Extract"),
    [(set (dty dstRC:$dst), (vector_extract (vty srcRC:$src), (i32 IRF32:$pos)))]>;



let Itinerary = II_CMU_CPEI_IRF_VRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CPEI_Extract_x32_v4i32_Myr2v3            : CMU_CPEI_Extract<IRF32,   VRF128,   i32,   v4i32, ".CP.32", "">;
  def CMU_CPEI_Extract_x32_v2i32_Myr2v3            : CMU_CPEI_Extract<IRF32,   VRF64_l,  i32,   v2i32, ".CP.32", "">;

  def CMU_CPEI_Extract_x32_v4f32_Myr2v3            : CMU_CPEI_Extract<IRF32,   VRF128,   f32,   v4f32, ".CP.32", "">;
  def CMU_CPEI_Extract_x32_v2f32_Myr2v3            : CMU_CPEI_Extract<IRF32,   VRF64_l,  f32,   v2f32, ".CP.32", "">;
}

// CPEI (Vd[dcol] Vs[scol]) instructions
// VRF->VRF copies
class CMU_CPEI_VV_Myr2v3<ValueType vty, RegisterClass vRC, ValueType elemty, string asmstr>:
  SHAVE_CMUInstr<(outs vRC:$dst), (ins vRC:$dsrc, vRC:$src, IRF32:$srcPos, IRF32:$dstPos), 
                 !strconcat(asmstr, " $dst $src $srcPos $dstPos ${:comment} CPEI VV"), 
                 [(set (vty vRC:$dst), (vector_insert (vty vRC:$dsrc),
                                           (elemty (vector_extract (vty vRC:$src),IRF32:$srcPos)),
                                           IRF32:$dstPos))], [isCMU_CP]>,
                 Requires<[HasVRF128_Predicate]>
{
  let Constraints = "$dsrc = $dst";
}

let Itinerary = II_CMU_CPEI_VRF_VRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CPEI_VV_32_v4i32: CMU_CPEI_VV_Myr2v3<v4i32, VRF128,  i32, ".CP.32">;
  def CMU_CPEI_VV_32_v4f32: CMU_CPEI_VV_Myr2v3<v4f32, VRF128,  f32, ".CP.32">;

  def CMU_CPEI_VV_32_v2i32: CMU_CPEI_VV_Myr2v3<v2i32, VRF64_l, i32, ".CP.32">;
  def CMU_CPEI_VV_32_v2f32: CMU_CPEI_VV_Myr2v3<v2f32, VRF64_l, f32, ".CP.32">;
}

//
// 1.4.4.16 - CMU.CPVRC
//   Copy VRF row to column.
//

let Itinerary = II_CMU_CPVRC_VRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CPVRC_imm_x32   : CMU_CP<(outs VRF128:$dst), (ins VRF128:$src, i8imm:$pos), ".CPVRC.x32 $dst.$pos $src",   []>;
  def CMU_CPVRC_imm_x16_l : CMU_CP<(outs VRF128:$dst), (ins VRF128:$src, i8imm:$pos), ".CPVRC.x16 $dst.$pos $src.l", []>;
  def CMU_CPVRC_imm_x16_h : CMU_CP<(outs VRF128:$dst), (ins VRF128:$src, i8imm:$pos), ".CPVRC.x16 $dst.$pos $src.h", []>;
}

//
// 1.4.4.17 - CMU.CPVV
//   VRF copy with optional conversion.
//

let Itinerary = II_CMU_CPVV_VRF, isAsCheapAsAMove = 1, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CPVV        : CMU_CP<(outs VRF128:$dst),   (ins VRF128:$src),   ".CP.128 $dst $src", []>;
  def CMU_CPVV_64     : CMU_CP<(outs VRF64_l:$dst),  (ins VRF64_l:$src),  ".CP.128 $dst $src", []>;

  // Vector extending copies (used by SHAVEInstrInfo::ExpandVECTOR_EXTEND)
  def CMU_CPVV_64_128 : CMU_CP<(outs VRF128:$dst),   (ins VRF64_l:$src),  ".CP.128 $dst $src", []>;
  def CMU_CPVV_32_128 : CMU_CP<(outs VRF128:$dst),   (ins VRF32_q0:$src), ".CP.128 $dst $src", []>;
  def CMU_CPVV_16_128 : CMU_CP<(outs VRF128:$dst),   (ins VRF16_e0:$src), ".CP.128 $dst $src", []>;

  // Vector truncating copies
  def CMU_CPVV_128_64 : CMU_CP<(outs VRF64_l:$dst),  (ins VRF128:$src),   ".CP.128 $dst $src", []>;
  def CMU_CPVV_128_32 : CMU_CP<(outs VRF32_q0:$dst), (ins VRF128:$src),   ".CP.128 $dst $src", []>;
  def CMU_CPVV_128_16 : CMU_CP<(outs VRF16_e0:$dst), (ins VRF128:$src),   ".CP.128 $dst $src", []>;

  // Vector copy used for inserting a 64 bit subvector into a 128 bit vector (used in conjunction with PEU.PVEN8C)
  let Constraints = "$dst = $vector1" in {
    def CMU_CPVV_64_128_insert : CMU_CP<(outs VRF128:$dst), (ins VRF128:$vector1, VRF64_l:$vector2), ".CP.128 $dst $vector2", []>;
  }
}

class CMU_CPVV_Conv<SDNode opcode, RegisterClass srcRC, RegisterClass dstRC,
                    ValueType srcVT, ValueType dstVT, string asmstr, string srcLane = "", string dstLane = ""> :
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src),
         !strconcat(".CP.", asmstr, " $dst", dstLane, " $src", srcLane),
         [(set (dstVT dstRC:$dst), (opcode (srcVT srcRC:$src)))]>;

class CMU_CPVV_Conv_NoPat<RegisterClass srcRC, RegisterClass dstRC, string asmstr, string srcLane = "", string dstLane = ""> :
  CMU_CP<(outs dstRC:$dst), (ins srcRC:$src),
         !strconcat(".CP.", asmstr, " $dst", dstLane, " $src", srcLane), []>;

let Itinerary = II_CMU_CPVV_VRF_CONV, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CPVV_f16_u8fs_Raw : CMU_CP<(outs VRF64_l:$dst), (ins VRF128:$src), ".CP.64.f16.u8fs $dst.0 $src">;
  def CMU_CPVV_f32_u8fs_Raw : CMU_CP<(outs VRF64_l:$dst), (ins VRF128:$src), ".CP.64.f32.u8fs $dst.0 $src">;
  
  def CMU_CPVV_u8f_f16_Raw  : CMU_CP<(outs VRF128:$dst), (ins VRF64_l:$src), ".CP.128.u8f.f16 $dst $src.0">;
  def CMU_CPVV_u8f_f32_Raw  : CMU_CP<(outs VRF128:$dst), (ins VRF64_l:$src), ".CP.128.u8f.f32 $dst $src.0">;
}

let Itinerary = II_CMU_CPVV_VRF_CONV, Predicates = [HasVRF128_Predicate] in
{
  let Predicates = [HasVRF128_Predicate] in {
    def CMU_CPVV_v4f32_v4u32_conv_Myr2v3 : CMU_CPVV_Conv<fp_to_uint, VRF128,   VRF128,
                                                         v4f32, v4i32, "128.f32.u32s">;
    def CMU_CPVV_v4f16_v4u32_conv_Myr2v3 : CMU_CPVV_Conv<fp_to_uint, VRF64_l,  VRF128,
                                                         v4f16, v4i32, "128.f16.u32s", ".0">;

    def CMU_CPVV_v4u32_v4f32_conv_Myr2v3 : CMU_CPVV_Conv<uint_to_fp, VRF128,   VRF128,
                                                         v4i32, v4f32, "128.u32.f32">;

    def CMU_CPVV_v4i8_v4f32_conv_Myr2v3  : CMU_CPVV_Conv<sint_to_fp, VRF32_q0, VRF128,
                                                         v4i8,  v4f32, "128.i8.f32", ".0">;

    def CMU_CPVV_v4u16_v4f16_conv_Myr2v3 : CMU_CPVV_Conv<uint_to_fp, VRF64_l,  VRF64_l,
                                                         v4i16, v4f16, "128.u16.f16">;

    def CMU_CPVV_v4u16_v4f32_conv_Myr2v3 : CMU_CPVV_Conv<uint_to_fp, VRF64_l,  VRF128,
                                                         v4i16, v4f32, "128.u16.f32", ".0">;
    def CMU_CPVV_v4u8_v4f16_conv_Myr2v3  : CMU_CPVV_Conv<uint_to_fp, VRF32_q0, VRF64_l,
                                                         v4i8, v4f16,  "128.u8.f16", ".0">;

    def CMU_CPVV_v4u32_v4f16_conv_Myr2v3 : CMU_CPVV_Conv<uint_to_fp, VRF128,   VRF64_l,
                                                         v4i32, v4f16, "64.u32.f16", "", ".0">;

    def CMU_CPVV_v8u8_v8f16_conv_Myr2v3  : CMU_CPVV_Conv<uint_to_fp, VRF64_l,  VRF128,
                                                         v8i8,  v8f16, "128.u8.f16", ".0">;
    def CMU_CPVV_v4u8_v4f32_conv_Myr2v3  : CMU_CPVV_Conv<uint_to_fp, VRF32_q0, VRF128,
                                                         v4i8,  v4f32, "128.u8.f32", ".0">;
  }

  def CMU_CPVV_v4f32_v4i32_conv : CMU_CPVV_Conv<fp_to_sint, VRF128, VRF128,
                                                    v4f32, v4i32, "128.f32.i32s">;
  
  def CMU_CPVV_v4i32_v4f32_conv : CMU_CPVV_Conv<sint_to_fp, VRF128, VRF128,
                                                    v4i32, v4f32, "128.i32.f32">;
  def CMU_CPVV_v8f16_v8i16_conv : CMU_CPVV_Conv<fp_to_sint, VRF128, VRF128,
                                                    v8f16, v8i16, "128.f16.i16s">;
  def CMU_CPVV_v8f16_v8i8_conv  : CMU_CPVV_Conv<fp_to_sint, VRF128, VRF64_l,
                                                    v8f16, v8i8,  "64.f16.i8s",    "",       ".0">;
  def CMU_CPVV_v8i16_v8f16_conv : CMU_CPVV_Conv<sint_to_fp, VRF128, VRF128,
                                                    v8i16, v8f16, "128.i16.f16">;

  def CMU_CPVV_v8i8_v8f16_conv  : CMU_CPVV_Conv<sint_to_fp, VRF64_l, VRF128,
                                                    v8i8,  v8f16, "128.i8.f16",    ".0">;
  
  def CMU_CPVV_v4i8_v4f16_conv  : CMU_CPVV_Conv<sint_to_fp, VRF32_q0, VRF64_l,
                                                    v4i8,  v4f16, "128.i8.f16",    ".0">;
  def CMU_CPVV_v4f16_v4i8_conv  : CMU_CPVV_Conv<fp_to_sint, VRF64_l, VRF32_q0,
                                                    v4f16, v4i8,  "64.f16.i8s",    "",       ".0">;

  def CMU_CPVV_v4i16_v4f16_conv : CMU_CPVV_Conv<sint_to_fp, VRF64_l, VRF64_l,
                                                    v4i16, v4f16, "128.i16.f16">;
  def CMU_CPVV_v4f16_v4i16_conv : CMU_CPVV_Conv<fp_to_sint, VRF64_l, VRF64_l,
                                                    v4f16, v4i16, "128.f16.i16s">;
  def CMU_CPVV_v4i16_v4f32_conv : CMU_CPVV_Conv<sint_to_fp, VRF64_l, VRF128,
                                                    v4i16, v4f32, "128.i16.f32",   ".0">;
  def CMU_CPVV_v4f32_v4i16_conv : CMU_CPVV_Conv<fp_to_sint, VRF128, VRF64_l,
                                                    v4f32, v4i16, "64.f32.i16s",   "",       ".0">;
  
  def CMU_CPVV_v4i32_v4f16_conv : CMU_CPVV_Conv<sint_to_fp, VRF128, VRF64_l,
                                                    v4i32, v4f16, "64.i32.f16",    "",       ".0">;
  
  def CMU_CPVV_v4f16_v4i32_conv : CMU_CPVV_Conv<fp_to_sint, VRF64_l, VRF128,
                                                    v4f16, v4i32, "128.f16.i32s",  ".0">;

  def CMU_CPVV_v2i32_v2f32_conv : CMU_CPVV_Conv<sint_to_fp, VRF64_l, VRF64_l,
                                                    v2i32, v2f32, "128.i32.f32">;
  def CMU_CPVV_v2f32_v2i32_conv : CMU_CPVV_Conv<fp_to_sint, VRF64_l, VRF64_l,
                                                    v2f32, v2i32, "128.f32.i32s">;

  def CMU_CPVV_v2i8_v2f16_conv  : CMU_CPVV_Conv<sint_to_fp, VRF16_e0, VRF32_q0,
                                                    v2i8,  v2f16, "128.i8.f16",    ".0">;
  def CMU_CPVV_v2f16_v2i8_conv  : CMU_CPVV_Conv<fp_to_sint, VRF32_q0, VRF16_e0,
                                                    v2f16, v2i8,  "64.f16.i8s",    "",       ".0">;

  def CMU_CPVV_v2i16_v2f16_conv : CMU_CPVV_Conv<sint_to_fp, VRF32_q0, VRF32_q0,
                                                    v2i16, v2f16, "32.i16.f16",    ".0", ".0">;
  def CMU_CPVV_v2f16_v2i16_conv : CMU_CPVV_Conv<fp_to_sint, VRF32_q0, VRF32_q0,
                                                    v2f16, v2i16, "128.f16.i16s">;

  def CMU_CPVV_v2i16_v2f32_conv : CMU_CPVV_Conv<sint_to_fp, VRF32_q0, VRF64_l,
                                                    v2i16, v2f32, "128.i16.f32",   ".0">;
  def CMU_CPVV_v2f32_v2i16_conv : CMU_CPVV_Conv<fp_to_sint, VRF64_l, VRF32_q0,
                                                    v2f32, v2i16, "64.f32.i16s",   "",       ".0">;
  
  def CMU_CPVV_v2i32_v2f16_conv : CMU_CPVV_Conv<sint_to_fp, VRF64_l, VRF32_q0,
                                                    v2i32, v2f16, "64.i32.f16",    "",       ".0">;
  def CMU_CPVV_v2f16_v2i32_conv : CMU_CPVV_Conv<fp_to_sint, VRF32_q0, VRF64_l,
                                                    v2f16, v2i32, "128.f16.i32s",  ".0">;
}

let Itinerary = II_CMU_CPVV_VRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CPVV_v2i8_v2i32_conv  : CMU_CPVV_Conv<sext, VRF16_e0, VRF64_l,
                                                    v2i8,  v2i32, "128.i8.i32",    ".0">;
  def CMU_CPVV_v2i8_v2i16_conv  : CMU_CPVV_Conv<sext, VRF16_e0, VRF32_q0,
                                                    v2i8,  v2i16, "128.i8.i16",    ".0">;
  def CMU_CPVV_v2u8_v2u16_conv  : CMU_CPVV_Conv<zext, VRF16_e0, VRF32_q0,
                                                    v2i8,  v2i16, "128.u8.u16",    ".0">;

  def CMU_CPVV_v2i16_v2i8_conv  : CMU_CPVV_Conv<trunc, VRF32_q0, VRF16_e0,
                                                    v2i16, v2i8,  "16.i16.i8s",    ".0", ".0">;
  def CMU_CPVV_v2u16_v2u8_conv  : CMU_CPVV_Conv<trunc, VRF32_q0, VRF16_e0,
                                                    v2i16, v2i8,  "64.u16.u8s",    "",       ".0">;

  def CMU_CPVV_v4i8_v4i32_conv  : CMU_CPVV_Conv<sext, VRF32_q0, VRF128,
                                                    v4i8,  v4i32, "128.i8.i32",    ".0">;
  let Predicates = [HasVRF128_Predicate] in {
    def CMU_CPVV_v4i8_v4i32_conv_Myr2v3  : CMU_CPVV_Conv<zext, VRF32_q0, VRF128,
                                                    v4i8,  v4i32, "128.u8.u32",    ".0">;
  }
  def CMU_CPVV_v4i8_v4i16_conv  : CMU_CPVV_Conv<sext, VRF32_q0, VRF64_l,
                                                    v4i8,  v4i16, "128.i8.i16",    ".0">;
  def CMU_CPVV_v4u8_v4u16_conv  : CMU_CPVV_Conv<zext, VRF32_q0, VRF64_l,
                                                    v4i8,  v4i16, "128.u8.u16",    ".0">;
  def CMU_CPVV_v2i16_v2i32_conv : CMU_CPVV_Conv<sext, VRF32_q0, VRF64_l,
                                                    v2i16, v2i32, "128.i16.i32",   ".0">;
  def CMU_CPVV_v2u32_v2u16_conv : CMU_CPVV_Conv<trunc, VRF64_l, VRF32_q0,
                                                    v2i32, v2i16, "64.u32.u16s",   "",       ".0">;

  def CMU_CPVV_v4i16_v4i8_conv  : CMU_CPVV_Conv<trunc, VRF64_l, VRF32_q0,
                                                    v4i16, v4i8,  "64.i16.i8s",    "",       ".0">;
  def CMU_CPVV_v4u16_v4u8_conv  : CMU_CPVV_Conv<trunc, VRF64_l, VRF32_q0,
                                                    v4i16, v4i8,  "64.u16.u8s",    "",       ".0">;
  def CMU_CPVV_v4i16_v4i32_conv : CMU_CPVV_Conv<sext, VRF64_l, VRF128,
                                                    v4i16, v4i32, "128.i16.i32",   ".0">;
  def CMU_CPVV_v8u16_v8u8_conv  : CMU_CPVV_Conv<trunc, VRF128, VRF64_l,
                                                    v8i16, v8i8,  "64.u16.u8s",    "",       ".0">;
  def CMU_CPVV_v8i8_v8i16_conv  : CMU_CPVV_Conv<sext, VRF64_l, VRF128,
                                                    v8i8,  v8i16, "128.i8.i16",    ".0">;
  def CMU_CPVV_v8u8_v8u16_conv  : CMU_CPVV_Conv<zext, VRF64_l, VRF128,
                                                    v8i8,  v8i16, "128.u8.u16",    ".0">;
  let Predicates = [HasVRF128_Predicate] in {
    def CMU_CPVV_v4u16_v4u32_conv_Myr2v3 : CMU_CPVV_Conv<zext, VRF64_l, VRF128,
                                                    v4i16,  v4i32, "128.u16.u32",    ".0">;
  }

  def CMU_CPVV_v4i32_v4i8_conv  : CMU_CPVV_Conv<trunc, VRF128, VRF32_q0,
                                                    v4i32, v4i8,  "32.i32.i8s",    "",       ".0">;
  def CMU_CPVV_v4u32_v4u8_conv  : CMU_CPVV_Conv<trunc, VRF128, VRF32_q0,
                                                    v4i32, v4i8,  "32.u32.u8s",    "",       ".0">;
  def CMU_CPVV_v2u32_v2u8_conv  : CMU_CPVV_Conv<trunc, VRF64_l, VRF16_e0,
                                                    v2i32, v2i8,  "32.u32.u8s",    "",       ".0">;
  def CMU_CPVV_v4i32_v4i16_conv : CMU_CPVV_Conv<trunc, VRF128, VRF64_l,
                                                    v4i32, v4i16, "64.i32.i16s",   "",       ".0">;
  def CMU_CPVV_v4u32_v4u16_conv : CMU_CPVV_Conv<trunc, VRF128, VRF64_l,
                                                    v4i32, v4i16, "64.u32.u16s",   "",       ".0">;
  def CMU_CPVV_v2i32_v2i8_conv  : CMU_CPVV_Conv<trunc, VRF64_l, VRF16_e0,
                                                    v2i32, v2i8,  "32.i32.i8s",    "",       ".0">;
  def CMU_CPVV_v2i32_v2i16_conv : CMU_CPVV_Conv<trunc, VRF64_l, VRF32_q0,
                                                    v2i32, v2i16, "32.i32.i16s",   "",       ".0">;

  def CMU_CPVV_v4f32_v4f16_conv : CMU_CPVV_Conv<fpround, VRF128, VRF64_l,
                                                    v4f32, v4f16, "64.f32.f16",    "",       ".0">;
  def CMU_CPVV_v2f32_v2f16_conv : CMU_CPVV_Conv<fpround, VRF64_l, VRF32_q0,
                                                    v2f32, v2f16, "64.f32.f16",    "",       ".0">;
  def CMU_CPVV_v4f16_v4f32_conv : CMU_CPVV_Conv<fpextend, VRF64_l, VRF128,
                                                    v4f16, v4f32, "128.f16.f32", ".0">;
  def CMU_CPVV_v2f16_v2f32_conv : CMU_CPVV_Conv<fpextend, VRF32_q0, VRF64_l,
                                                    v2f16, v2f32, "128.f16.f32", ".0">;

  def CMU_CPVV_u16_u8s_conv : CMU_CPVV_Conv_NoPat<VRF128, VRF64_l, "64.u16.u8s",   "",       ".0">;
  def CMU_CPVV_f16_u8s_conv : CMU_CPVV_Conv_NoPat<VRF128, VRF64_l, "64.f16.u8s",   "",       ".0">;
}

//
// 1.8.4.10 - CMU.CPED
// Myriad 2.3 CPED (Vd.dcol Vs.scol) instructions
// Vector element copy with direct element specifiers
class CMU_CPED_Myr2v3 <ValueType vty, ValueType elemty, string asmstr>:
  SHAVE_CMUInstr<(outs VRF128:$dst), (ins VRF128:$dsrc, VRF128:$src, i8imm:$srcPos, i8imm:$dstPos), 
                 !strconcat(asmstr, " $dst.$dstPos $src.$srcPos ${:comment} CPED"), 
                 [(set (vty VRF128:$dst), (vector_insert (vty VRF128:$dsrc),
                                           (elemty (vector_extract (vty VRF128:$src), imm:$srcPos)),
                                           imm:$dstPos))], [isCMU_CP]>,
                 Requires<[HasVRF128_Predicate]>
{
  let Constraints = "$dsrc = $dst";
}

let Itinerary = II_CMU_CPVV_VRF in
{
  def CMU_CPED_32_v4i32: CMU_CPED_Myr2v3<v4i32, i32, ".CP.32">;
  def CMU_CPED_32_v4f32: CMU_CPED_Myr2v3<v4f32, f32, ".CP.32">;
  def CMU_CPED_16_v8f16: CMU_CPED_Myr2v3<v8f16, f16, ".CP.16">;
  def CMU_CPED_16_v8i16: CMU_CPED_Myr2v3<v8i16, i16, ".CP.16">;
  def CMU_CPED_8:  CMU_CPED_Myr2v3<v16i8, i8,  ".CP.8" >;
}

//
// 1.4.4.18 - CMU.CPZ
//   Copy zero to TRF/VRF
//

// FIXME: Movidius - These instructions actually out a range of registers, so this will need to be
//                  redefined if adding patterns (or explicitly selecting during lowering)
let Itinerary = II_CMU_CPZV_VRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_CPZV         : CMU_CP<(outs VRF128:$dst),  (ins i8imm:$range), ".CPZV $dst $range", []>;
  def CMU_CPZV_VRF64_l : CMU_CP<(outs VRF64_l:$dst), (ins i8imm:$range), ".CPZV $dst $range", []>;
}

let Itinerary = II_CMU_CPZI_IRF in
{
  def CMU_CPZI : CMU_CP<(outs IRF32:$dst), (ins), ".CPZI $dst 0", []>;
}

let Itinerary = II_CMU_CPZT_TRF in
{
  def CMU_CPZT : CMU_CP<(outs TRF:$dst), (ins i8imm:$range), ".CPZT $dst $range", []>;
}

//
// 1.4.4.19 - CMU.LUT
//   VRF look-up, store result as 32-bit signed or unsigned integer.
//

// dty = dest scalar value type
// vty = src vector value type
class CMU_LUT_Raw<RegisterClass dstRC, RegisterClass srcRC, string asmstr> :
  SHAVE_CMUInstr<(outs dstRC:$dst), (ins srcRC:$src, IRF32:$pos),
                 !strconcat(".LUT", asmstr, " $dst $pos 0x0"), [], [isCMU_LUTR, isCMU_LUT]>;

let Itinerary = II_CMU_LUT_IRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_LUT_i32_v4i32     : CMU_LUT_Raw<IRF32,   VRF128,   ".i32">;
  def CMU_LUT_i32_v2i32     : CMU_LUT_Raw<IRF32,   VRF64_l,  ".i32">;
  def CMU_LUT_i16_v8i16     : CMU_LUT_Raw<IRF16_l, VRF128,   ".i16">;
  def CMU_LUT_i16_v4i16     : CMU_LUT_Raw<IRF16_l, VRF64_l,  ".i16">;
  def CMU_LUT_i16_v2i16     : CMU_LUT_Raw<IRF16_l, VRF32_q0, ".i16">;
  def CMU_LUT_i8_v16i8      : CMU_LUT_Raw<IRF8_q0, VRF128,   ".i8">;
  def CMU_LUT_i8_v8i8       : CMU_LUT_Raw<IRF8_q0, VRF64_l,  ".i8">;
  def CMU_LUT_i8_v4i8       : CMU_LUT_Raw<IRF8_q0, VRF32_q0, ".i8">;
  def CMU_LUT_i8_v2i8       : CMU_LUT_Raw<IRF8_q0, VRF16_e0, ".i8">;
}

let Itinerary = II_CMU_LUT_IRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_LUT_u32_v4i32     : CMU_LUT_Raw<IRF32,   VRF128,   ".u32">;
  def CMU_LUT_u32_v2i32     : CMU_LUT_Raw<IRF32,   VRF64_l,  ".u32">;
  def CMU_LUT_u16_v8i16     : CMU_LUT_Raw<IRF16_l, VRF128,   ".u16">;
  def CMU_LUT_u16_v4i16     : CMU_LUT_Raw<IRF16_l, VRF64_l,  ".u16">;
  def CMU_LUT_u16_v2i16     : CMU_LUT_Raw<IRF16_l, VRF32_q0, ".u16">;
  def CMU_LUT_u8            : CMU_LUT_Raw<IRF8_q0, VRF128,   ".u8">;
}

//
// 1.4.4.20 - CMU.LUTR
//   VRF look-up with variable size.
//

let Itinerary = II_CMU_LUT_IRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_LUTR_x32        : SHAVE_CMUInstr<(outs IRF32:$dst),   (ins VRF128:$src, IRF32:$pos),             ".LUTR.x32 $dst $pos",        [], [isCMU_LUT]>;

  let Predicates = [HasVRF128_Predicate] in {
    def CMU_LUTR_x16        : SHAVE_CMUInstr<(outs IRF16_l:$dst), (ins VRF128:$src, IRF32:$pos, i8imm:$sel), ".LUTR.x16 $dst $pos.$sel", [], [isCMU_LUT]>;
    def CMU_LUTR_x8         : SHAVE_CMUInstr<(outs IRF8_q0:$dst), (ins VRF128:$src, IRF32:$pos, i8imm:$sel), ".LUTR.x8 $dst pos.$sel",   [], [isCMU_LUT]>;
  }
}

//
// 1.4.4.21 - CMU.LUTW
//   VRF look-up write.
//

class CMU_LUTw_Raw<RegisterClass dstRC, RegisterClass srcRC, string asmstr> :
  SHAVE_CMUInstr<(outs dstRC:$dst), (ins dstRC:$src, srcRC:$val, IRF32:$pos),
                 !strconcat(".LUTW", asmstr, " $val", " $pos", " 0x0"), [], [isCMU_LUT]>
{
  let Constraints = "$dst = $src";
}

let Itinerary = II_CMU_LUTW_IRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_LUTW_x32_v4i32    : CMU_LUTw_Raw<VRF128,  IRF32,   ".x32">;
  def CMU_LUTW_x32_v2i32    : CMU_LUTw_Raw<VRF64_l, IRF32,   ".x32">;
  def CMU_LUTW_x16_v8i16    : CMU_LUTw_Raw<VRF128,  IRF16_l, ".x16">;
  def CMU_LUTW_x16_v4i16    : CMU_LUTw_Raw<VRF64_l, IRF16_l, ".x16">;
  def CMU_LUTW_x8_v16i8     : CMU_LUTw_Raw<VRF128,  IRF8_q0, ".x8">;
  def CMU_LUTW_x8_v8i8      : CMU_LUTw_Raw<VRF64_l, IRF8_q0, ".x8">;
}

//
// 1.4.4.22/23 - CMU.MAX / CMU.MIN
//   Maximum. / Minimum.
//

class CMU_MinMax<RegisterClass RC, string asmstr, ValueType ty, SDNode op, int isUint, list<Register>myriad2v3_defs, int isVRFOp=0>
 : CMU_MinMax_Base<RC, asmstr, ty, op, isUint, myriad2v3_defs>, Requires<!if(isVRFOp, [HasVRF128_Predicate], [])>
{ 
  let Itinerary = !if(isVRFOp, II_CMU_RRR_VRF_MIN_MAX_Myr2v3, II_CMU_RRR_IRF_MIN_MAX_Myr2v3); 
}

class CMU_MinMax_NoPat<RegisterClass RC, string asmstr, list<Register>myriad2v3_defs, int isVRFOp=0>
: CMU_MinMax_NoPat_Base<RC, asmstr, myriad2v3_defs>, Requires<!if(isVRFOp, [HasVRF128_Predicate], [])>
{ 
  let Itinerary = !if(isVRFOp, II_CMU_RRR_VRF_MIN_MAX_Myr2v3, II_CMU_RRR_IRF_MIN_MAX_Myr2v3); 
}

// IRF variants
multiclass CMU_MinMax_IRF<string asmstr, SDNode op>
{   
  // Defer to the IAU MIN/MAX instructions for i32 and u32
  def _i32        : CMU_MinMax_NoPat<IRF32, !strconcat(asmstr, ".i32"), [CC_CMU0]>;
  def _u32        : CMU_MinMax_NoPat<IRF32, !strconcat(asmstr, ".u32"), [CC_CMU0]>;

  def _i16        : CMU_MinMax<IRF16_l, !strconcat(asmstr, ".i16"), i16, op, 0, [CC_CMU0, CC_CMU1]>;
  def _i8         : CMU_MinMax<IRF8_q0, !strconcat(asmstr, ".i8"),  i8,  op, 0, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;
  def _u16        : CMU_MinMax<IRF16_l, !strconcat(asmstr, ".u16"), i16, op, 1, [CC_CMU0, CC_CMU1]>;
  def _u8         : CMU_MinMax<IRF8_q0, !strconcat(asmstr, ".u8"),  i8,  op, 1, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;

  def _v2i16      : CMU_MinMax<IRF32,   !strconcat(asmstr, ".i16"), v2i16, op, 0, [CC_CMU0, CC_CMU1]>;
  def _v4i8       : CMU_MinMax<IRF32,   !strconcat(asmstr, ".i8"),  v4i8,  op, 0, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;
  def _v2i8       : CMU_MinMax<IRF16_l, !strconcat(asmstr, ".i8"),  v2i8,  op, 0, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;
  def _v2u16      : CMU_MinMax<IRF32,   !strconcat(asmstr, ".u16"), v2i16, op, 1, [CC_CMU0, CC_CMU1]>;
  def _v4u8       : CMU_MinMax<IRF32,   !strconcat(asmstr, ".u8"),  v4i8,  op, 1, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;
  def _v2u8       : CMU_MinMax<IRF16_l, !strconcat(asmstr, ".u8"),  v2i8,  op, 1, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>;

  def _f32   : CMU_MinMax<IRF32,   !strconcat(asmstr, ".f32"), f32,   op, 0, [CC_CMU0]>;
  def _f16   : CMU_MinMax<IRF16_l, !strconcat(asmstr, ".f16"), f16,   op, 0, [CC_CMU0, CC_CMU1]>;
  def _v2f16 : CMU_MinMax<IRF32,   !strconcat(asmstr, ".f16"), v2f16, op, 0, [CC_CMU0, CC_CMU1]>;
}

defm CMU_MAX  : CMU_MinMax_IRF<".MAX", shave_max>;
defm CMU_MIN  : CMU_MinMax_IRF<".MIN", shave_min>;


// VRF variants
multiclass CMU_MinMax_VRF<string asmstr, SDNode op> 
{
  // 128-bit native vectors
  def _f32_v4f32 : CMU_MinMax<VRF128,  !strconcat(asmstr, ".f32"), v4f32, op, 0, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3], 1>;
  def _f16_v8f16 : CMU_MinMax<VRF128,  !strconcat(asmstr, ".f16"), v8f16, op, 0, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7], 1>;

  def _i32_v4i32 : CMU_MinMax<VRF128,  !strconcat(asmstr, ".i32"), v4i32, op, 0, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3], 1>;
  def _i16_v8i16 : CMU_MinMax<VRF128,  !strconcat(asmstr, ".i16"), v8i16, op, 0, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7], 1>;
  def _i8_v16i8  : CMU_MinMax<VRF128,  !strconcat(asmstr, ".i8"),  v16i8, op, 0, [CC_CMU0, CC_CMU1,  CC_CMU2,  CC_CMU3,  CC_CMU4,  CC_CMU5,  CC_CMU6,  CC_CMU7,
										   CC_CMU8, CC_CMU9, CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15], 1>;

  def _u32_v4u32 : CMU_MinMax<VRF128,  !strconcat(asmstr, ".u32"), v4i32, op, 1, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3], 1>;
  def _u16_v8u16 : CMU_MinMax<VRF128,  !strconcat(asmstr, ".u16"), v8i16, op, 1, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7], 1>;
  def _u8_v16u8  : CMU_MinMax<VRF128,  !strconcat(asmstr, ".u8"),  v16i8, op, 1, [CC_CMU0, CC_CMU1,  CC_CMU2,  CC_CMU3,  CC_CMU4,  CC_CMU5,  CC_CMU6,  CC_CMU7,
										   CC_CMU8, CC_CMU9, CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15], 1>; 

  // 64-bit native vectors
  def _f32_v2f32 : CMU_MinMax<VRF64_l, !strconcat(asmstr, ".f32"), v2f32, op, 0, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3], 1>;
  def _f16_v4f16 : CMU_MinMax<VRF64_l, !strconcat(asmstr, ".f16"), v4f16, op, 0, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3], 1>;

  def _i32_v2i32 : CMU_MinMax<VRF64_l, !strconcat(asmstr, ".i32"), v2i32, op, 0, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3], 1>;
  def _i16_v4i16 : CMU_MinMax<VRF64_l, !strconcat(asmstr, ".i16"), v4i16, op, 0, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7], 1>;
  def _i8_v8i8   : CMU_MinMax<VRF64_l, !strconcat(asmstr, ".i8"),  v8i8,  op, 0, [CC_CMU0, CC_CMU1,  CC_CMU2,  CC_CMU3,  CC_CMU4,  CC_CMU5,  CC_CMU6,  CC_CMU7,
                                                                                  CC_CMU8, CC_CMU9, CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15], 1>;

  def _u32_v2u32 : CMU_MinMax<VRF64_l, !strconcat(asmstr, ".u32"), v2i32, op, 1, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3], 1>;
  def _u16_v4u16 : CMU_MinMax<VRF64_l, !strconcat(asmstr, ".u16"), v4i16, op, 1, [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7], 1>;
  def _u8_v8u8   : CMU_MinMax<VRF64_l, !strconcat(asmstr, ".u8"),  v8i8,  op, 1, [CC_CMU0, CC_CMU1,  CC_CMU2,  CC_CMU3,  CC_CMU4,  CC_CMU5,  CC_CMU6,  CC_CMU7,
                                                                                  CC_CMU8, CC_CMU9, CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15], 1>;
}

defm CMU_MAX : CMU_MinMax_VRF<".MAX", shave_max>;
defm CMU_MIN : CMU_MinMax_VRF<".MIN", shave_min>;


//
// 1.4.4.24 - CMU.SHIV
//   Shift IRF left or right into VRF.
//

class CMU_SHL<RegisterClass dstRC, RegisterClass srcRC,
              string asmstr, ValueType scalarType, ValueType vectorType> :
  SHAVE_CMUInstr<(outs dstRC:$dst), (ins dstRC:$src1, srcRC:$src2),
                 !strconcat(".SHL", asmstr, " $dst $src1 $src2"),
                 [(set dstRC:$dst, (shave_shlv (vectorType dstRC:$src1),
                         (scalarType srcRC:$src2)))]>
{
  let Constraints = "$src1 = $dst";
}

class CMU_SHR<RegisterClass srcRC, string asmstr> :
  SHAVE_CMUInstr<(outs VRF128:$dst), (ins VRF128:$src1, srcRC:$src2),
                 !strconcat(".SHR", asmstr, " $dst $src1 $src2"), []>
{
  let Constraints = "$src1 = $dst";
}

let Itinerary = II_CMU_SHLIV, Predicates = [HasVRF128_Predicate] in
{
  // SHLIV
  def CMU_SHLIV_x32       : CMU_SHL<VRF128,  IRF32,   "IV.x32", i32, v4i32>;
  def CMU_SHLIV_x16       : CMU_SHL<VRF128,  IRF16_l, "IV.x16", i16, v8i16>;
  def CMU_SHLIV_x8        : CMU_SHL<VRF128,  IRF8_q0, "IV.x8",  i8,  v16i8>;

  def CMU_SHLIV_x32_v2i32 : CMU_SHL<VRF64_l, IRF32,   "IV.x32", i32, v2i32>;
  def CMU_SHLIV_x16_v4i16 : CMU_SHL<VRF64_l, IRF16_l, "IV.x16", i16, v4i16>;
  def CMU_SHLIV_x8_v8i8   : CMU_SHL<VRF64_l, IRF8_q0, "IV.x8",  i8,  v8i8>;

  def CMU_SHLIV_x16_v2i16 : CMU_SHL<VRF32_q0,  IRF16_l, "IV.x16", i16, v2i16>;
  def CMU_SHLIV_x8_v4i8   : CMU_SHL<VRF32_q0,  IRF8_q0, "IV.x8",  i8,  v4i8>;

  def CMU_SHLIV_x8_v2i8   : CMU_SHL<VRF16_e0,  IRF8_q0, "IV.x8",  i8,  v2i8>;

  // SHRIV
  def CMU_SHRIV_x32       : CMU_SHR<IRF32,   "IV.x32">;
  def CMU_SHRIV_x16       : CMU_SHR<IRF16_l, "IV.x16">;
  def CMU_SHRIV_x8        : CMU_SHR<IRF8_q0, "IV.x8">;
}

//
// 1.4.4.25 - CMU.TP4
//   Matrix transpose/rotate.
//

// FIXME: Movidius - These operate on sets of 4 contiguous VRF registers. We will need VRF512 
//                  super-register support before they will be selectable
// def CMU_TP4R  : SHAVE_CMUInstr<(outs VRF512:$dst), (ins VRF512:$src), ".TP4R $dst $src",  []>;
// def CMU_TP4L  : SHAVE_CMUInstr<(outs VRF512:$dst), (ins VRF512:$src), ".TP4L $dst $src",  []>;
// def CMU_ROT4L : SHAVE_CMUInstr<(outs VRF512:$dst), (ins VRF512:$src), ".ROT4L $dst $src", []>;
// def CMU_ROT4R : SHAVE_CMUInstr<(outs VRF512:$dst), (ins VRF512:$src), ".ROT4R $dst $src", []>;

//
// 1.4.4.26 - CMU.VILV
//   Vector interleave/deinterleave.
//

class CMU_VLIV_Raw<RegisterClass srcRC, string asmstr> :
  SHAVE_CMUInstr<(outs srcRC:$dst1, srcRC:$dst2), (ins srcRC:$src1, srcRC:$src2),
                 !strconcat(asmstr, " $dst1 $dst2 $src1 $src2"), []>;

let Itinerary = II_CMU_VILV_VRF, hasSideEffects = 0, Predicates = [HasVRF128_Predicate] in
{
  def CMU_VILV_x32 : CMU_VLIV_Raw<VRF128, ".VILV.x32">;
  def CMU_VILV_x16 : CMU_VLIV_Raw<VRF128, ".VILV.x16">;
  def CMU_VILV_x8 : CMU_VLIV_Raw<VRF128, ".VILV.x8">;

  def CMU_VILV_x32_v2i32 : CMU_VLIV_Raw<VRF64_l, ".VILV.x32">;
  def CMU_VILV_x16_v4i16 : CMU_VLIV_Raw<VRF64_l, ".VILV.x16">;
  def CMU_VILV_x8_v8i8 : CMU_VLIV_Raw<VRF64_l, ".VILV.x8">;

  def CMU_VDILV_x32 : CMU_VLIV_Raw<VRF128, ".VDILV.x32">;
  def CMU_VDILV_x16 : CMU_VLIV_Raw<VRF128, ".VDILV.x16">;
  def CMU_VDILV_x8 : CMU_VLIV_Raw<VRF128, ".VDILV.x8">;
}

//
// 1.4.4.27 - CMU.VNZ
//   Generate IRF mask from VRF contents.
//

class CMU_VNZ_Raw<string variant> :
  SHAVE_CMUInstr<(outs IRF32:$dst), (ins VRF128:$src, IRF32:$vsrc, i8imm:$offset), 
                 !strconcat(".VNZ", variant, " $dst $src $offset"),  []> {
  let Constraints = "$vsrc = $dst";                 
}

let Itinerary = II_CMU_VNZ, Predicates = [HasVRF128_Predicate] in
{
  def CMU_VNZ_x32_imm : CMU_VNZ_Raw<".x32">;
  def CMU_VNZ_x16_imm : CMU_VNZ_Raw<".x16">;
  def CMU_VNZ_x8_imm : CMU_VNZ_Raw<".x8">;

  // VNZD needs a 2xVRF super-register
//  def CMU_VNZD_x32_imm : CMU_VNZ_Raw<"D.x32">;
//  def CMU_VNZD_x16_imm : CMU_VNZ_Raw<"D.x16">;
//  def CMU_VNZD_x8_imm : CMU_VNZ_Raw<"D.x8">;
}

//
// 1.4.4.28 - CMU.VSIGN
//   Floating-point sign set/change.
//

class CMU_VSIGNF<RegisterClass RC, ValueType ty, string asmstr> :
  SHAVE_CMUInstr<(outs RC:$dst), (ins RC:$src), 
                 asmstr, [(set (ty RC:$dst), (fneg (ty RC:$src)))]>;

// IRF variants                 
let Itinerary = II_CMU_VSIGNF_IRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_VSIGNF_f32   : CMU_VSIGNF<IRF32,   f32,   ".VSIGNF.f32 $dst $src [n]">;
  def CMU_VSIGNF_f16   : CMU_VSIGNF<IRF16_l, f16,   ".VSIGNF.f16 $dst $src [pn]">;
  def CMU_VSIGNF_v2f16 : CMU_VSIGNF<IRF32,   v2f16, ".VSIGNF.f16 $dst $src [nn]">;
}

// VRF variants
let Itinerary = II_CMU_VSIGNF_VRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_VSIGNF_v4f32    : CMU_VSIGNF<VRF128,  v4f32, ".VSIGNF.f32 $dst $src [nnnn]">;
  def CMU_VSIGNF_v8f16    : CMU_VSIGNF<VRF128,  v8f16, ".VSIGNF.f16 $dst $src [nnnnnnnn]">;
  def CMU_VSIGNF_v2f32    : CMU_VSIGNF<VRF64_l, v2f32, ".VSIGNF.f32 $dst $src [ppnn]">;
  def CMU_VSIGNF_v4f16    : CMU_VSIGNF<VRF64_l, v4f16, ".VSIGNF.f16 $dst $src [ppppnnnn]">;
}

//
// 1.4.4.29 - CMU.VSZM
//   Byte/Word swizzle with optional write disable or zeroing.
//

// Class definitions for VSZM variants
// FIXME: Movidius - could rewrite these along the lines of the LSU patterns
class CMU_VSZM_Raw<RegisterClass RC, string asmstr, list<SHAVEConflictName> conflicts = []> :
  SHAVE_CMUInstr<(outs RC:$dst), (ins RC:$src, shave_vszm_opt:$en0, shave_vszm_opt:$en1, shave_vszm_opt:$en2, shave_vszm_opt:$en3),
                 !strconcat(".VSZM", asmstr, " $dst $src [$en3$en2$en1$en0]"), [], conflicts>;

class CMU_VSZM_VECTOR_EXTRACT<RegisterClass dstRC, RegisterClass srcRC, Operand posTy, ValueType dstVT, ValueType srcVT, string asmstr> :
  SHAVE_CMUInstr<(outs dstRC:$dst), (ins srcRC:$src, posTy:$pos),
                 !strconcat(asmstr, " $dst $src [$pos]"),
                 [(set (dstVT dstRC:$dst), (vector_extract (srcVT srcRC:$src), imm:$pos))]>;

class CMU_VSZM_VECTOR_INSERT<RegisterClass destRC, RegisterClass srcRC, Operand posTy, ValueType vty, ValueType sty, string asmstr>:
  SHAVE_CMUInstr<(outs destRC:$dst), (ins destRC:$vsrc, srcRC:$src, posTy:$pos),
                 !strconcat(asmstr, " $dst $src [$pos]"),
                 [(set (vty destRC:$dst), (vector_insert (vty destRC:$vsrc), (sty srcRC:$src), imm:$pos))]>
{
  let Constraints = "$vsrc = $dst";
}

class CMU_VSZM_SPLAT<RegisterClass dstRC, RegisterClass srcRC, ValueType dstVT, ValueType srcVT, string asmstr> : 
  SHAVE_CMUInstr<(outs dstRC:$dst), (ins srcRC:$src),
                 !strconcat(".VSZM", asmstr),
                 [(set (dstVT dstRC:$dst), (dstVT (splat (srcVT srcRC:$src))))]>;     

// Instruction definitions
let Itinerary = II_CMU_VSZM_VRF, Predicates = [HasVRF128_Predicate] in
{
  def CMU_VSZMWORD_imm_vrf     : CMU_VSZM_Raw<VRF128,  ".WORD">;
  def CMU_VSZMWORD_imm_vrf64   : CMU_VSZM_Raw<VRF64_l, ".WORD">;
  def CMU_VSZMBYTE_imm_irf     : CMU_VSZM_Raw<IRF32,   ".BYTE">;  // FIXME: Movidius - why is this using the VRF itinerary?

  let Predicates = [HasVRF128_Predicate] in {
    def CMU_VSZMBYTE_imm_vrf   : CMU_VSZM_Raw<VRF128,  ".BYTE">;
    def CMU_VSZMBYTE_imm_vrf64 : CMU_VSZM_Raw<VRF64_l, ".BYTE">;
  }
}

let Itinerary = II_CMU_VSZM_IRF, Predicates = [HasVSZM_Predicate] in
{
	//	Zero extended
  def CMU_CPII_u16_u32    : CMU_CP<(outs IRF32:$dst),   (ins IRF16_l:$src), ".VSZM.BYTE $dst $src [ZZ10] ${:comment} Zero-extend", [(set (i32   IRF32:$dst),   (zext (i16  IRF16_l:$src)))]>;

  def CMU_CPII_u8_u16     : CMU_CP<(outs IRF16_l:$dst), (ins IRF8_q0:$src), ".VSZM.BYTE $dst $src [ZZZ0] ${:comment} Zero-extend", [(set (i16   IRF16_l:$dst), (zext (i8   IRF8_q0:$src)))]>;
  
  def CMU_CPII_v2u8_v2u16 : CMU_CP<(outs IRF32:$dst),   (ins IRF16_l:$src), ".VSZM.BYTE $dst $src [Z1Z0] ${:comment} Zero-extend", [(set (v2i16 IRF32:$dst),   (zext (v2i8 IRF16_l:$src)))]>;
  def CMU_CPII_v2x8_v2x16 : CMU_CP<(outs IRF32:$dst),   (ins IRF16_l:$src), ".VSZM.BYTE $dst $src [Z1Z0] ${:comment} Any-extend",  [(set (v2i16 IRF32:$dst),   (anyext (v2i8 IRF16_l:$src)))]>;
}

let Itinerary = II_CMU_VSZM_IRF, Predicates = [HasVSZM_Predicate] in
{
  // Small vector extract  
  def CMU_VSZM_v2i16_i16 : CMU_VSZM_VECTOR_EXTRACT<IRF16_l, IRF32,   shave_vszm_opt_extract16, i16,   v2i16, ".VSZM.BYTE">; 
  def CMU_VSZM_v2f16_f16 : CMU_VSZM_VECTOR_EXTRACT<IRF16_l, IRF32,   shave_vszm_opt_extract16, f16,   v2f16, ".VSZM.BYTE">;

  def CMU_VSZM_v4i8_i8   : CMU_VSZM_VECTOR_EXTRACT<IRF8_q0, IRF32,   shave_vszm_opt_extract8,  i8,    v4i8,  ".VSZM.BYTE">;

  def CMU_VSZM_v2i8_i8   : CMU_VSZM_VECTOR_EXTRACT<IRF8_q0, IRF16_l, shave_vszm_opt_extract8,  i8,    v2i8,  ".VSZM.BYTE">;

  // Small vector insert
  def CMU_VSZM_i16_v2i16 : CMU_VSZM_VECTOR_INSERT< IRF32,   IRF16_l, shave_vszm_opt_insert16,  v2i16, i16,   ".VSZM.BYTE">; 
  def CMU_VSZM_f16_v2f16 : CMU_VSZM_VECTOR_INSERT< IRF32,   IRF16_l, shave_vszm_opt_insert16,  v2f16, f16,   ".VSZM.BYTE">;

  def CMU_VSZM_i8_v4i8   : CMU_VSZM_VECTOR_INSERT< IRF32,   IRF8_q0, shave_vszm_opt_insert8,   v4i8,  i8,    ".VSZM.BYTE">;

  def CMU_VSZM_i8_v2i8   : CMU_VSZM_VECTOR_INSERT< IRF16_l, IRF8_q0, shave_vszm_opt_insert8,   v2i8,  i8,    ".VSZM.BYTE">;
}

// splat pattern between IRF-to-IRF registers, alternative to above ones CPIVR.x8 and CPIVR.x16
// seems more efficient, avoids register copies.
let Itinerary = II_CMU_VSZM_IRF, Predicates = [HasVSZM_Predicate] in
{
def CMU_VSZM_SPLAT_i8_v4i8   : CMU_VSZM_SPLAT<IRF32,   IRF8_q0, v4i8,  i8,  ".BYTE $dst $src [0000]">;
def CMU_VSZM_SPLAT_i16_v2i16 : CMU_VSZM_SPLAT<IRF32,   IRF16_l, v2i16, i16, ".BYTE $dst $src [1010]">;
def CMU_VSZM_SPLAT_i8_v2i8   : CMU_VSZM_SPLAT<IRF16_l, IRF8_q0, v2i8,  i8,  ".BYTE $dst $src [ZZ00]">;
def CMU_VSZM_SPLAT_f16_v2f16 : CMU_VSZM_SPLAT<IRF32,   IRF16_l, v2f16, f16, ".BYTE $dst $src [1010]">;
}

class CMU_VSZM_SUBVECTOR_INSERT<RegisterClass dstRC, RegisterClass srcRC, string asmstr> : 
  SHAVE_CMUInstr<(outs dstRC:$dst), 
                 (ins dstRC:$vector1, srcRC:$vector2, shave_vszm_opt:$en0, shave_vszm_opt:$en1, shave_vszm_opt:$en2, shave_vszm_opt:$en3),
                 !strconcat(asmstr, " $dst $vector2 [$en3$en2$en1$en0]"), []>;

let Itinerary = II_CMU_VSZM_VRF, Constraints = "$dst = $vector1", Predicates = [HasVSZM_Predicate, HasVRF128_Predicate] in
{
  def CMU_VSZM_WORD_SUBVECTOR_INSERT : CMU_VSZM_SUBVECTOR_INSERT<VRF128, VRF64_l, ".VSZM.WORD">;
  def CMU_VSZM_BYTE_SUBVECTOR_INSERT : CMU_VSZM_SUBVECTOR_INSERT<IRF32,  IRF16_l, ".VSZM.BYTE">;
  def CMU_VSZM_BYTE_IRF32            : CMU_VSZM_SUBVECTOR_INSERT<IRF32,  IRF32,   ".VSZM.BYTE">;
}

//
// CMU.BIT instructions (available on NPU4 and later only), used by preemption support
//

let Itinerary = II_CMU_BIT in {
  def CMU_INVB : SHAVE_CMUInstr<(outs IRF32:$dst), (ins IRF32:$src, i8imm:$bit), ".INVB $dst $src $bit", []>;
}

//===----------------------------------------------------------------------===//
// CMU pattern definitions
//===----------------------------------------------------------------------===//

// Narrow to smaller integer ==> simple truncation (ignore upper bits) by extracting the sub register.
def : Pat<(i32 (trunc IRF64:$src)),
          (EXTRACT_SUBREG $src, vsub32_0)>;
def : Pat<(i16 (trunc IRF64:$src)),
          (EXTRACT_SUBREG (i32 (EXTRACT_SUBREG $src, vsub32_0)), hsub_0)>;
def : Pat<(i8 (trunc IRF64:$src)),
          (EXTRACT_SUBREG (i32 (EXTRACT_SUBREG $src, vsub32_0)), qsub_0)>;
def : Pat<(i16 (trunc IRF32:$src)),
          (EXTRACT_SUBREG $src, hsub_0)>;
def : Pat<(i8 (trunc IRF32:$src)),
          (EXTRACT_SUBREG $src, qsub_0)>;
def : Pat<(i8(trunc IRF16_l:$src)),
          (EXTRACT_SUBREG $src, qsub_0)>;

//  Complex conversions not supported by the ISA
def shave_llvm_f32_to_f16   : Pat<(fp_to_f16 (f32 IRF32:$src)), (i16 (CMU_CPII_16_Raw (CMU_CPII_f32_f16 IRF32:$src)))>;
def shave_llvm_f16_to_f32   : Pat<(f16_to_fp (i16 IRF16_l:$src)), (f32(CMU_CPII_f16_f32 (f16(CMU_CPII_16_Raw (i16 IRF16_l:$src)))))>;

// pattern for v8f16 -> v8u8 conversion
def CMU_CPVV_v8f16_v8u8_conv : Pat<(v8i8 (fp_to_uint (v8f16 VRF128:$src))),
        (CMU_CPVV_v8u16_v8u8_conv (CMU_CPVV_v8f16_v8i16_conv VRF128:$src))>, Requires<[HasVRF128_Predicate]>;

// patterns for v4i8 <---> v4f32 conversion
def CMU_CPVV_v4f32_v4i8 : Pat<(v4i8 (fp_to_sint (v4f32 VRF128:$src))),
        (CMU_CPVV_v4i32_v4i8_conv (CMU_CPVV_v4f32_v4i32_conv VRF128:$src))>, Requires<[HasVRF128_Predicate]>;
def CMU_CPVV_v4i8_v4f32 : Pat<(v4f32 (sint_to_fp (v4i8 VRF32_q0:$src))),
        (CMU_CPVV_v4i32_v4f32_conv (CMU_CPVV_v4i8_v4i32_conv VRF32_q0:$src))>, Requires<[HasVRF128_Predicate]>;

// v2i8 <---> v2f32
def CMU_CPVV_v2f32_v2i8 : Pat<(v2i8 (fp_to_sint (v2f32 VRF64_l:$src))),
        (CMU_CPVV_v2i32_v2i8_conv (CMU_CPVV_v2f32_v2i32_conv VRF64_l:$src))>, Requires<[HasVRF128_Predicate]>;
def CMU_CPVV_v2i8_v2f32 : Pat<(v2f32 (sint_to_fp (v2i8 VRF16_e0:$src))),
        (CMU_CPVV_v2i32_v2f32_conv (CMU_CPVV_v2i8_v2i32_conv VRF16_e0:$src))>, Requires<[HasVRF128_Predicate]>;

// v4u8 -> v4u32
def CMU_CPVV_v4u8_v4u32: Pat<(v4i32 (zext (v4i8 VRF32_q0:$src))),
        (CMU_CPVV_v4i16_v4i32_conv (CMU_CPVV_v4u8_v4u16_conv VRF32_q0:$src))>, Requires<[HasVRF128_Predicate]>;

// v2u8 -> v2u32
def CMU_CPVV_v2u8_v2u32: Pat<(v2i32 (zext (v2i8 VRF16_e0:$src))),
        (CMU_CPVV_v2i16_v2i32_conv (CMU_CPVV_v2u8_v2u16_conv VRF16_e0:$src))>, Requires<[HasVRF128_Predicate]>;

// v2u16 -> v2u32 (Myriad 2 except 2.1)
def CMU_CPVV_v2u16_v2u32_Myr2v3: Pat<(v2i32 (zext (v2i16 VRF32_q0:$src))),
        (CMU_VSZMBYTE_imm_vrf64 (CMU_CPVV_v2i16_v2i32_conv VRF32_q0:$src), 4, 5, 0, 0)>, Requires<[HasVSZM_Predicate, HasVRF128_Predicate]>;

def: Pat<(v4f32 (uint_to_fp (v4i8 VRF32_q0:$src))),
        (CMU_CPVV_v4i32_v4f32_conv (CMU_CPVV_v4i8_v4i32_conv VRF32_q0:$src))>, Requires<[HasVRF128_Predicate]>;

// v2u8 <---> v2f32
def: Pat<(v2i8 (fp_to_uint (v2f32 VRF64_l:$src))),
        (CMU_CPVV_v2u16_v2u8_conv (CMU_CPVV_v2f32_v2i16_conv VRF64_l:$src))>, Requires<[HasVRF128_Predicate]>;
def: Pat<(v2f32 (uint_to_fp (v2i8 VRF16_e0:$src))),
        (CMU_CPVV_v2i16_v2f32_conv (CMU_CPVV_v2i8_v2i16_conv VRF16_e0:$src))>, Requires<[HasVRF128_Predicate]>;

// pattern v4i8 <---> v4f16 conversion (check these patterns, if really necessary!!!)
// there is bitconvert pattern above supposed to do fp_to_uint and vice versa
def: Pat<(v4i8 (fp_to_uint (v4f16 VRF64_l:$src))),
        (CMU_CPVV_v4u16_v4u8_conv (CMU_CPVV_v4f16_v4i16_conv VRF64_l:$src))>, Requires<[HasVRF128_Predicate]>;
def: Pat<(v4f16 (uint_to_fp (v4i8 VRF32_q0:$src))),
        (CMU_CPVV_v4i16_v4f16_conv (CMU_CPVV_v4i8_v4i16_conv VRF32_q0:$src))>, Requires<[HasVRF128_Predicate]>;

// v2u8 <---> v2f16
def: Pat<(v2i8 (fp_to_uint (v2f16 VRF32_q0:$src))),
        (CMU_CPVV_v2u16_v2u8_conv (CMU_CPVV_v2f16_v2i16_conv VRF32_q0:$src))>, Requires<[HasVRF128_Predicate]>;
def: Pat<(v2f16 (uint_to_fp (v2i8 VRF16_e0:$src))),
        (CMU_CPVV_v2i16_v2f16_conv (CMU_CPVV_v2i8_v2i16_conv VRF16_e0:$src))>, Requires<[HasVRF128_Predicate]>;

def : Pat<(i16 (vector_extract (v16i8 VRF128:$src), imm:$pos)),
          (CMU_CPVI_x16_l VRF128:$src, imm:$pos)>, Requires<[HasVRF128_Predicate]>;

include "SHAVEv3/SHAVEv3InstrInfo_CMU.td"
