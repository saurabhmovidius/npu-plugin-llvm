//===-- SHAVEv3InstrInfo_SAU.td - VAU Instructions -----------*- tablegen -*-===//
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
// This file describes the instructions available in the SHAVE Scalar
// Arithmetic Unit (SAU)
//
//===----------------------------------------------------------------------===//

let Predicates = [HasVRF512_Predicate] in {

//
// SAU.ILMULH
//

class SAU_MULHS_MULHU_Base_Myr4<SDNode opcode, SHAVE_TypeInfo typeInfo> :
  SAU_RRROpC<opcode, typeInfo.mixedVectorRegClass, typeInfo.valueType, !strconcat(".ILMULH.", typeInfo.asmstr), [usesVAU, isSAU_MULorVAU_MUL]>;

class SAU_MULHS_MULHU_Myr4<SDNode opcode, ValueType valueType> :
  SAU_MULHS_MULHU_Base_Myr4<opcode, SHAVE_TypeInfo<valueType, /*isUnsigned=*/ !if(!eq(opcode, mulhs), 0, 1)>>;

let Itinerary = II_SAU_MUL_IRF_WITH_WRITE_2_CONDITION in {
  def SAU_ILMULH_v4i8_Myr4  : SAU_MULHS_MULHU_Myr4<mulhs, v4i8>;
  def SAU_ILMULH_v2i16_Myr4 : SAU_MULHS_MULHU_Myr4<mulhs, v2i16>;
  def SAU_ILMULH_v2i8_Myr4  : SAU_MULHS_MULHU_Myr4<mulhs, v2i8>;
  
  def SAU_ILMULH_v4u8_Myr4  : SAU_MULHS_MULHU_Myr4<mulhu, v4i8>;
  def SAU_ILMULH_v2u16_Myr4 : SAU_MULHS_MULHU_Myr4<mulhu, v2i16>;
  def SAU_ILMULH_v2u8_Myr4  : SAU_MULHS_MULHU_Myr4<mulhu, v2i8>;
}

class SAU_XOp_Base_Myr4<RegisterClass dstRC, list<dag> pat, string asmstr> :
  SHAVE_SAUInstr<(outs dstRC:$dst), (ins WVRF512:$src),
                  !strconcat(asmstr, " $dst $src"),
                  pat>;

class SAU_XOp_Pattern_Myr4<RegisterClass dstRC, ValueType dstVT, ValueType srcVT, SDNode op, string asmstr, bit match = 1> :
  SAU_XOp_Base_Myr4<dstRC,
                    !if(match, [(set (dstVT dstRC:$dst), (op (srcVT WVRF512:$src)))], []),
                    asmstr>;

multiclass SAU_XOp_Myr4<SDNode op, string asmstr> {
  // FIXME: should these be all IRF32?
  def _x32_VRF512 : SAU_XOp_Pattern_Myr4<IRF32,   i32, v16i32, op, !strconcat(asmstr, ".x32")>;
  def _x16_VRF512 : SAU_XOp_Pattern_Myr4<IRF16_l, i16, v32i16, op, !strconcat(asmstr, ".x16")>;
  def _x8_VRF512  : SAU_XOp_Pattern_Myr4<IRF8_q0, i8,  v64i8,  op, !strconcat(asmstr, ".x8")>;
}

//
// SAU.ANDX
//

let Itinerary = II_SAU_OR_AND_IRF_VRF in {
  defm SAU_ANDX : SAU_XOp_Myr4<horizontal_and, ".ANDX">;
}

//
// SAU.ONESX
//

multiclass SAU_ONESX_ZEROSX_Myr4<string asmstr> {
  def _x32_imm_VRF512  : SHAVE_SAUInstr<(outs IRF32:$dst), (ins WVRF512:$src, i8imm:$idx), !strconcat(asmstr, ".x32 $dst $src.$idx"),  []>;
  def _x64_imm_VRF512  : SHAVE_SAUInstr<(outs IRF32:$dst), (ins WVRF512:$src, i8imm:$idx), !strconcat(asmstr, ".x64 $dst $src.$idx"),  []>;
  def _x128_imm_VRF512 : SHAVE_SAUInstr<(outs IRF32:$dst), (ins WVRF512:$src, i8imm:$idx), !strconcat(asmstr, ".x128 $dst $src.$idx"), []>;
}

let Itinerary = II_SAU_ONES_ZEROS_VRF in {
  defm SAU_ONESX  : SAU_ONESX_ZEROSX_Myr4<".ONESX">;
  defm SAU_ZEROSX : SAU_ONESX_ZEROSX_Myr4<".ZEROSX">;
}

//
// SAU.ORX
//

let Itinerary = II_SAU_OR_AND_IRF_VRF in {
  defm SAU_ORX : SAU_XOp_Myr4<horizontal_or, ".ORX">;
}

//
// SAU.SUMX
//

class SAU_SUMX_Myr4<ValueType srcVT, string asmstr, bit match = 1> :
  SAU_XOp_Pattern_Myr4<IRF32, i32, srcVT, horizontal_add, !strconcat(".SUMX", asmstr), match>;

let Itinerary = II_SAU_SUM_IRF_VRF_INT in {
  def SAU_SUMX_i32_VRF512 : SAU_SUMX_Myr4<v16i32, ".i32">;
  def SAU_SUMX_i16_VRF512 : SAU_SUMX_Myr4<v32i16, ".i16">;
  def SAU_SUMX_i8_VRF512  : SAU_SUMX_Myr4<v64i8,  ".i8">;

  // Don't want to select these
  def SAU_SUMX_u32_VRF512 : SAU_SUMX_Myr4<v16i32, ".u32", 0>;
  def SAU_SUMX_u16_VRF512 : SAU_SUMX_Myr4<v32i16, ".u16", 0>;
  def SAU_SUMX_u8_VRF512  : SAU_SUMX_Myr4<v64i8,  ".u8",  0>;
}

//
// SAU.SUMFX
//

// TODO: figure out how to extract subvectors???
class SAU_SUMFX_Myr4<RegisterClass scalarRegClass, string asmstr, bit hasLane = 1> :
  SHAVE_SAUInstr<(outs scalarRegClass:$dst), !if(hasLane, (ins WVRF512:$src, i16imm:$lane), (ins WVRF512:$src)),
                  !strconcat(".SUMX.", asmstr, " $dst $src", !if(hasLane, ".$lane", "")),
                 []>;

let Itinerary = II_SAU_SUM_IRF_VRF_FP in {
  def SAU_SUMFX_f32_imm_VRF512 : SAU_SUMFX_Myr4<IRF32,   "f32">;
  def SAU_SUMFX_f16_imm_VRF512 : SAU_SUMFX_Myr4<IRF16_l, "f16">;
  def SAU_SUMFX_f16_f32        : SAU_SUMFX_Myr4<IRF32,   "f16.f32", 0>;
}

class SAU_SUMFX_SUBVECTOR_Pattern<RegisterClass dstRC, RegisterClass srcRC, ValueType dstVT, ValueType srcVT, string asmstr> :
   SHAVE_SAUInstr<(outs dstRC:$dst), (ins srcRC:$src, i16imm:$lane),
                   !strconcat(asmstr, " $dst $src", ".$lane"),
                   [(set (dstVT dstRC:$dst), (horizontal_fadd_subvector (srcVT srcRC:$src), (i16 uimm16:$lane)))]>;

let Itinerary = II_SAU_SUM_IRF_VRF_FP in {
    def SAU_SUMFX_v16f32_Pat_Myr4 : SAU_SUMFX_SUBVECTOR_Pattern<IRF32, WVRF512, f32, v16f32, ".SUMX.f32">;
    def SAU_SUMFX_v32f16_Pat_Myr4 : SAU_SUMFX_SUBVECTOR_Pattern<IRF16_l, WVRF512, f16, v32f16, ".SUMX.f16">;
    def SAU_SUMFX_v8f32_Pat_Myr4 : SAU_SUMFX_SUBVECTOR_Pattern<IRF32, WVRF256_0, f32, v8f32, ".SUMX.f32">;
    def SAU_SUMFX_v16f16_Pat_Myr4 : SAU_SUMFX_SUBVECTOR_Pattern<IRF16_l, WVRF256_0, f16, v16f16, ".SUMX.f16">;
    def SAU_SUMFX_v4f32_Pat_Myr4 : SAU_SUMFX_SUBVECTOR_Pattern<IRF32, WVRF128_0, f32, v4f32, ".SUMX.f32">;
    def SAU_SUMFX_v8f16_Pat_Myr4 : SAU_SUMFX_SUBVECTOR_Pattern<IRF16_l, WVRF128_0, f16, v8f16, ".SUMX.f16">;
}

class SAU_SUMFX_Pattern_Myr4<ValueType srcVT, string asmstr, bit match = 1> :
  SAU_XOp_Pattern_Myr4<IRF32, f32, srcVT, horizontal_fadd, !strconcat(".SUMX", asmstr), match>;

let Itinerary = II_SAU_SUM_IRF_VRF_FP, Predicates = [HasSUMFX512_Predicate] in {
  def SAU_SUMFX_f16_f32_Pat_Myr4  : SAU_SUMFX_Pattern_Myr4<v32f16,  ".f16.f32">;
}

//
// SAU.XORX
//

let Itinerary = II_SAU_OR_AND_IRF_VRF in {
  defm SAU_XORX : SAU_XOp_Myr4<horizontal_xor, ".XORX">;
}

foreach type = [ v16i32,  v32i16,  v64i8 ] in {
  def : Pat<(SHAVE_TypeInfo<type>.scalarType (vecreduce_and (type WVRF512:$source))),
            (!cast<Instruction>("SAU_ANDX_x"#SHAVE_TypeInfo<type>.scalarType.Size#"_VRF512")
            WVRF512:$source)>;
  def : Pat<(SHAVE_TypeInfo<type>.scalarType (vecreduce_or (type WVRF512:$source))),
            (!cast<Instruction>("SAU_ORX_x"#SHAVE_TypeInfo<type>.scalarType.Size#"_VRF512")
            WVRF512:$source)>;
  def : Pat<(SHAVE_TypeInfo<type>.scalarType (vecreduce_xor (type WVRF512:$source))),
            (!cast<Instruction>("SAU_XORX_x"#SHAVE_TypeInfo<type>.scalarType.Size#"_VRF512")
            WVRF512:$source)>;
}

class SAU_Math_Op_VRF_Myr4<string asmstr> :
    SHAVE_SAUInstr<(outs IRF16_l:$dst), (ins WVRF512:$src, i8imm:$idx),
                   !strconcat(asmstr, " $dst $src.$idx"), []>;

let Itinerary = II_SAU_MATH_VRF in
{
  def SAU_SQTV_imm_VRF512  : SAU_Math_Op_VRF_Myr4<".SQT">;
  def SAU_LOGV_imm_VRF512  : SAU_Math_Op_VRF_Myr4<".LOG">;
  def SAU_SIGMV_imm_VRF512 : SAU_Math_Op_VRF_Myr4<".SIGM">;
  def SAU_TANHV_imm_VRF512 : SAU_Math_Op_VRF_Myr4<".TANH">;
  def SAU_EXPV_imm_VRF512  : SAU_Math_Op_VRF_Myr4<".EXP">;
  def SAU_EXP2V_imm_VRF512 : SAU_Math_Op_VRF_Myr4<".EXP2">;
}

} // Predicates = [HasVRF512_Predicate]
