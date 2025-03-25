//===-- SHAVEInstrInfo_IAU.td - IAU Instructions -----------*- tablegen -*-===//
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
// This file describes the instructions available in the SHAVE Integer
// Arithmetic Unit (IAU).
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// IAU generic class declarations
//===----------------------------------------------------------------------===//

// base class for defining Reg <-- (Reg op Reg) type ops
class IAU_RRROpC_Base<RegisterClass regVT, string asmstr, list<dag> pattern> :
  SHAVE_IAUInstr<(outs regVT:$dst), (ins regVT:$src1, regVT:$src2),
                 !strconcat(asmstr, " $dst $src1 $src2"), pattern>;
class IAU_RRROpC_Base_NoPred<RegisterClass regVT, string asmstr, list<dag> pattern> :
  SHAVE_IAUInstr_NoPred<(outs regVT:$dst), (ins regVT:$src1, regVT:$src2),
                 !strconcat(asmstr, " $dst $src1 $src2"), pattern>;

// class for defining Reg <-- (Reg op Reg) type ops without a pattern
class IAU_RRROpC_NoPat<RegisterClass regVT, string asmstr> :
  IAU_RRROpC_Base<regVT, asmstr, []>;
class IAU_RRROpC_NoPat_NoPred<RegisterClass regVT, string asmstr> :
  IAU_RRROpC_Base_NoPred<regVT, asmstr, []>;

// class for defining Reg <-- (Reg op Reg) type ops
class IAU_RRROpC<SDNode opc, RegisterClass regVT, ValueType vt, string asmstr> :
  IAU_RRROpC_Base<regVT, asmstr, [(set (vt regVT:$dst), (opc (vt regVT:$src1), (vt regVT:$src2)))]>;
class IAU_RRROpC_NoPred<SDNode opc, RegisterClass regVT, ValueType vt, string asmstr> :
  IAU_RRROpC_Base_NoPred<regVT, asmstr, [(set (vt regVT:$dst), (opc (vt regVT:$src1), (vt regVT:$src2)))]>;

multiclass IAU_RRROp<SDNode opc, string asmstr>
{
  def _8  : IAU_RRROpC<opc, IRF8_q0,  i8,  asmstr>;
  def _16 : IAU_RRROpC<opc, IRF16_l,  i16, asmstr>;
  def _32 : IAU_RRROpC<opc, IRF32,    i32, asmstr>;
}
multiclass IAU_RRROp_NoPred<SDNode opc, string asmstr>
{
  def _8  : IAU_RRROpC_NoPred<opc, IRF8_q0,  i8,  asmstr>;
  def _16 : IAU_RRROpC_NoPred<opc, IRF16_l,  i16, asmstr>;
  def _32 : IAU_RRROpC_NoPred<opc, IRF32,    i32, asmstr>;
}

// base class for defining Reg <-- (Reg op Imm) type ops
class IAU_RRIOpC_Base<RegisterClass regVT, Operand immOp, string asmstr, list<dag> pattern> :
  SHAVE_IAUInstr<(outs regVT:$dst), (ins regVT:$src, immOp:$imm),
                  !strconcat(asmstr, " $dst $src $imm"), pattern>;
class IAU_RRIOpC_Base_NoPred<RegisterClass regVT, Operand immOp, string asmstr, list<dag> pattern> :
  SHAVE_IAUInstr_NoPred<(outs regVT:$dst), (ins regVT:$src, immOp:$imm),
                  !strconcat(asmstr, " $dst $src $imm"), pattern>;

// class for defining Reg <-- (Reg op Imm) type ops without a pattern
class IAU_RRIOpC_NoPat<RegisterClass regVT, Operand immOp, string asmstr> :
  IAU_RRIOpC_Base<regVT, immOp, asmstr, []>;
class IAU_RRIOpC_NoPat_NoPred<RegisterClass regVT, Operand immOp, string asmstr> :
  IAU_RRIOpC_Base_NoPred<regVT, immOp, asmstr, []>;

// class for defining Reg <-- (Reg op Imm) type ops
class IAU_RRIOpC<SDNode opc, RegisterClass regVT, ValueType vt, Operand immOp, string asmstr, PatLeaf immPat> :
  IAU_RRIOpC_Base<regVT, immOp, asmstr, [(set (vt regVT:$dst), (opc (vt regVT:$src), (vt immPat:$imm)))]>;
class IAU_RRIOpC_NoPred<SDNode opc, RegisterClass regVT, ValueType vt, Operand immOp, string asmstr, PatLeaf immPat> :
  IAU_RRIOpC_Base_NoPred<regVT, immOp, asmstr, [(set (vt regVT:$dst), (opc (vt regVT:$src), (vt immPat:$imm)))]>;
                             
multiclass IAU_RRIOp<SDNode opc, string asmstr, PatLeaf immPat>
{
  def _8_imm  : IAU_RRIOpC<opc, IRF8_q0,  i8,  i8imm, asmstr, immPat>;
  def _16_imm : IAU_RRIOpC<opc, IRF16_l, i16, i16imm, asmstr, immPat>;
  def _32_imm : IAU_RRIOpC<opc, IRF32,   i32, i32imm, asmstr, immPat>;
}
multiclass IAU_RRIOp_NoPred<SDNode opc, string asmstr, PatLeaf immPat>
{
  def _8_imm  : IAU_RRIOpC_NoPred<opc, IRF8_q0,  i8,  i8imm, asmstr, immPat>;
  def _16_imm : IAU_RRIOpC_NoPred<opc, IRF16_l, i16, i16imm, asmstr, immPat>;
  def _32_imm : IAU_RRIOpC_NoPred<opc, IRF32,   i32, i32imm, asmstr, immPat>;
}

// base class for defining Reg <-- (op Reg) type ops
class IAU_RROpC_Base<RegisterClass regVT, string asmstr, list<dag> pattern> :
  SHAVE_IAUInstr<(outs regVT:$dst), (ins regVT:$src), !strconcat(asmstr, " $dst $src"), pattern>;

// class for defining Reg <-- (op Reg) type ops without a pattern
class IAU_RROpC_NoPat<RegisterClass regVT, string asmstr> :
  IAU_RROpC_Base<regVT, asmstr, []>;

// class for defining Reg <-- (op Reg) type ops
class IAU_RROpC<SDNode opc, RegisterClass regVT, ValueType vt, string asmstr> :
  IAU_RROpC_Base<regVT, asmstr, [(set (vt regVT:$dst), (opc (vt regVT:$src)))]>;

multiclass IAU_RROp<SDNode opc, string asmstr>
{
  def _8  : IAU_RROpC<opc, IRF8_q0,  i8, asmstr>;
  def _16 : IAU_RROpC<opc, IRF16_l, i16, asmstr>;
  def _32 : IAU_RROpC<opc, IRF32,   i32, asmstr>;
}

//===----------------------------------------------------------------------===//
// IAU instruction definitions (Section 4.5 of Fragrak SHAVE Reference Guide)
//===----------------------------------------------------------------------===//

//
// 4.5.1 - IAU.ABS
//   32-bit signed integer absolute value.
//
// IAU.ABS
let Itinerary = II_IAU_RR_WCC_WI, Defs = [CC_IAU0, I_STATE] in
{
  def IAU_ABS_32 : SHAVE_IAUInstr<(outs IRF32:$dst), (ins IRF32:$src),
                                  ".ABS $dst $src", [(set (i32 IRF32:$dst), (xor (add (i32 IRF32:$src), (sra (i32 IRF32:$src), (i32 +31))), (sra IRF32:$src, (i32 +31))))]>;
}

// IAU.ABSCI
// ABSCI has been removed from Myriad4.0
let Itinerary = II_IAU_RR, Predicates = [HasVRF128_Predicate] in
{
  def IAU_ABSCI : IAU_RROpC_NoPat<IRF32, ".ABSCI">;
}

//
// 4.5.2 - IAU.ADD
//   32-bit integer addition with optional unsigned immediate operand.
//
let Itinerary = II_IAU_RRR_WI, Defs = [I_STATE] in
{
  defm IAU_ADD : IAU_RRROp<add, ".ADD">;
}

let Itinerary = II_IAU_RRI_WCC_WI, Defs = [CC_IAU0, I_STATE] in
{
  defm IAU_ADD : IAU_RRIOp<add, ".ADD", uimm8>;
}

//
// 4.5.3 - IAU.ADDCI
//   32-bit integer addition with carry-in.
//
let Itinerary = II_IAU_RRR_RWI, Uses = [I_STATE], Defs = [I_STATE] in
{
  def IAU_ADDCI : IAU_RRROpC<adde, IRF32, i32, ".ADDCI">;
}

//
// 4.5.4 - IAU.ADDSI
//   32-bit signed integer addition with saturation. Optional unsigned immediate operand.
//
let Itinerary = II_IAU_RRR_RWI in
{
  def IAU_ADDSI     : IAU_RRROpC_NoPat<IRF32, ".ADDSI">;
  def IAU_ADDSI_imm : IAU_RRIOpC_NoPat<IRF32, i8imm, ".ADDSI">;
}

//
// 4.5.5 - IAU.ADDSU
//   32-bit unsigned integer addition with saturation. Optional immediate operand.
//
let Itinerary = II_IAU_RRR_RWI in
{
  def IAU_ADDSU     : IAU_RRROpC_NoPat<IRF32, ".ADDSU">;
  def IAU_ADDSU_imm : IAU_RRIOpC_NoPat<IRF32, i8imm, ".ADDSU">;
}

//
// 4.5.6 - IAU.ALIGN
//   Align to byte boundary.
//
let Itinerary = II_IAU_RRRI in
{
  def IAU_ALIGN_imm : SHAVE_IAUInstr<(outs IRF32:$dst), (ins IRF32:$src1, IRF32:$src2, i8imm:$boff),
                                     ".ALIGN $dst $src1 $src2 $boff", []>;
}

//
// 4.5.7 - IAU.AND
//   Bitwise AND.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0] in
{
  defm IAU_AND : IAU_RRROp<and, ".AND">;
}

//
// 4.5.8 - IAU.AVG
//   32-bit unsigned integer two operand average.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0], isCommutable = 1 in
{
  def IAU_AVG : SHAVE_IAUInstr<(outs IRF32:$dst), (ins IRF32:$src1, IRF32:$src2),
                               ".AVG $dst $src1 $src2",
                               [(set (i32 IRF32:$dst), (srl (add (add (i32 IRF32:$src1), (i32 IRF32:$src2)), (i32 1)), (i32 1)))]>;
}

//
// 4.5.9 - IAU.BSF
//   Find bit position of least significant bit set to 1/0.
//
let Itinerary = II_IAU_RR in
{
  def IAU_BSF_32    : IAU_RROpC<cttz_zero_undef, IRF32,   i32, ".BSF">;
  def IAU_BSFINV_32 : IAU_RROpC_NoPat<IRF32, ".BSFINV">; 
}

//
// 4.5.10 - IAU.BSR
//   Find bit position of most significant bit set to 1/0.
//
let Itinerary = II_IAU_RR in
{
  def IAU_BSR_32    : IAU_RROpC<ctlz_zero_undef, IRF32, i32, ".BSR">;
  def IAU_BSRINV_32 : IAU_RROpC_NoPat<IRF32, ".BSRINV">; 
}

//
// 4.5.11 - IAU.CLAMP
//   32-bit signed integer clamp.
//
let Itinerary = II_IAU_RRR_WCC in
{
  def IAU_CLAMP0_32 : IAU_RRROpC_NoPat<IRF32, ".CLAMP0">;
  def IAU_CLAMP_32  : IAU_RRROpC_NoPat<IRF32, ".CLAMP">;
}

//
// 4.5.12 - IAU.DECCMP
//   Unsigned integer compare with optional post-decrement.
//
let Itinerary = II_IAU_RI_WCC, Defs = [CC_IAU0] in
{
  def IAU_CMPI : SHAVE_IAUInstr<(outs), (ins IRF32:$src, i8imm:$imm), ".CMPI $src $imm", []>;
}

let Itinerary = II_IAU_RRI_WCC, Constraints = "$dst = $src" in
{
  def IAU_DECCMP : SHAVE_IAUInstr<(outs IRF32:$dst), (ins IRF32:$src, i8imm:$imm), ".DECCMP $dst $imm", []>;
}

//
// 4.5.13 - IAU.FEXTI
//   Signed integer field extract with immediate specifiers.
//
let Itinerary = II_IAU_RRI_WCC, Defs = [CC_IAU0] in
{
  def IAU_FEXTI_imm    : SHAVE_IAUInstr<(outs IRF32 : $dst), (ins IRF32 : $src, i8imm : $lsbpos, i8imm : $len),
                                        ".FEXTI $dst $src $lsbpos $len", []>;
}

//
// 4.5.14 - IAU.FEXTJI
//   Signed integer field extract with indirect specifiers.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0] in
{
  def IAU_FEXTJI : IAU_RRROpC_NoPat<IRF32, ".FEXTJI">;
}

//
// 4.5.15 - IAU.FEXTJU
//   Unsigned integer field extract with indirect specifiers.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0] in
{
  def IAU_FEXTJU : IAU_RRROpC_NoPat<IRF32, ".FEXTJU">;
}

//
// 4.5.16 - IAU.FEXTU
//   Unsigned integer field extract with immediate specifiers.
//
let Itinerary = II_IAU_RRI_WCC, Defs = [CC_IAU0] in
{
  def IAU_FEXTU_imm : SHAVE_IAUInstr<(outs IRF32 : $dst), (ins IRF32 : $src, i32imm : $lsbpos, i32imm : $len),
                                     ".FEXTU $dst $src $lsbpos $len", []>;
}

//
// 4.5.17 - IAU.FINS
//   Field insert with immediate specifiers.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0], Constraints = "$src1 = $dst" in
{
  def IAU_FINS_imm : SHAVE_IAUInstr<(outs IRF32:$dst), (ins IRF32:$src1, IRF32:$src2, i32imm:$lsbpos, i32imm:$len),
                                 ".FINS $dst $src2 $lsbpos $len", []>;
}

//
// 4.5.18 - IAU.FINSJ
//   Field insert with indirect specifiers.
//

class IAU_FINSJ<RegisterClass dstRegVT, RegisterClass srcRegVT, string asmstr> : 
  SHAVE_IAUInstr<(outs dstRegVT:$dst), (ins dstRegVT:$src1, srcRegVT:$src2, IRF32:$src3),
                 !strconcat(asmstr, " $dst $src2 $src3"), []>
{
  let Constraints = "$src1 = $dst";
}

let Itinerary = II_IAU_RRRR_WCC, Defs = [CC_IAU0] in
{
  def IAU_FINSJ           : IAU_FINSJ<IRF32,   IRF32,   ".FINSJ">;
  def IAU_FINSJ_i16_v2i16 : IAU_FINSJ<IRF32,   IRF16_l, ".FINSJ">;
  def IAU_FINSJ_i8_v4i8   : IAU_FINSJ<IRF32,   IRF8_q0, ".FINSJ">;
  def IAU_FINSJ_i8_v2i8   : IAU_FINSJ<IRF16_l, IRF8_q0, ".FINSJ">;
}

//
// 4.5.19 - IAU.INCS
//   Signed integer increment with immediate.
//
class IAU_INCS<RegisterClass regVT, ValueType vt, Operand immTy> :
     SHAVE_IAUInstr<(outs regVT:$dst), (ins regVT:$src, immTy:$imm),
                    ".INCS $dst $imm", [(set (vt regVT:$dst), (add (vt regVT:$src), simm14:$imm))]>;

let Itinerary = II_IAU_RRI, Constraints = "$src = $dst", isReMaterializable = 1 in
{
  def IAU_INCS_i8  : IAU_INCS<IRF8_q0, i8,  i8imm>;
  def IAU_INCS_i16 : IAU_INCS<IRF16_l, i16, i16imm>;
  def IAU_INCS_i32 : IAU_INCS<IRF32,   i32, i32imm>;
}

//
// 4.5.20 - IAU.LMULH
//   32-bit signed integer multiply returning high-order 32bit result.
//
let Itinerary = II_IAU_LMUL_MULS, Defs = [CC_IAU0] in
{
  def IAU_LMULH : IAU_RRROpC<mulhs, IRF32, i32, ".LMULH">;
}

//
// 4.5.21 - IAU.LMULL
//   32-bit signed integer multiply returning low-order 32bit result.
//
let Itinerary = II_IAU_LMUL_MULS, Defs = [CC_IAU0] in
{
  def IAU_LMULL : IAU_RRROpC<mul, IRF32, i32, ".LMULL">;
}

//
// 4.5.22 - IAU.MAXI
//   32-bit signed integer maximum.
//
class IAU_MAX_MIN<SDNode op, string asmstr, int isUint> :
  SHAVE_IAUInstr<(outs IRF32:$dst), (ins IRF32:$src1, IRF32:$src2), 
                 !strconcat(asmstr, " $dst $src1 $src2"), 
                 [(set (i32 IRF32:$dst), (op (i32 IRF32:$src1), (i32 IRF32:$src2), (i32 isUint)))]>;

let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0] in
{
  def IAU_MAXI : IAU_MAX_MIN<shave_max, ".MAXI", 0>;
}

//
// 4.5.23 - IAU.MAXU
//   32-bit unsigned integer maximum.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0] in
{
  def IAU_MAXU : IAU_MAX_MIN<shave_max, ".MAXU", 1>;
}

//
// 4.5.24 - IAU.MINI
//   32-bit signed integer minimum.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0] in
{
  def IAU_MINI : IAU_MAX_MIN<shave_min, ".MINI", 0>;
}

//
// 4.5.25 - IAU.MINU
//   32-bit unsigned integer minimum.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0] in
{
  def IAU_MINU : IAU_MAX_MIN<shave_min, ".MINU", 1>;
}

//
// 4.5.26 - IAU.MUL
//   32-bit signed integer multiplication with optional signed immediate operand.
//
let Itinerary = II_IAU_MUL_RRR, Defs = [I_AE, CC_IAU0] in
{
  defm IAU_MUL : IAU_RRROp<mul, ".MUL">;
}

let Itinerary = II_IAU_MUL_RRI in
{
  defm IAU_MUL : IAU_RRIOp<mul, ".MUL", simm8>;
}

//
// 4.5.27 - IAU.MULSI
//   32-bit signed integer multiplication with saturation. Optional signed immediate operand.
//
let Itinerary = II_IAU_LMUL_MULS, Defs = [CC_IAU0] in
{
  def IAU_MULSI     : IAU_RRROpC_NoPat<IRF32,        ".MULSI">;
  def IAU_MULSI_imm : IAU_RRIOpC_NoPat<IRF32, i8imm, ".MULSI">;
}

//
// 4.5.28 - IAU.MULSU
//   32-bit unsigned integer multiplication with saturation. Optional immediate operand.
//
let Itinerary = II_IAU_LMUL_MULS, Defs = [CC_IAU0] in
{
  def IAU_MULSU     : IAU_RRROpC_NoPat<IRF32,        ".MULSU">;
  def IAU_MULSU_imm : IAU_RRIOpC_NoPat<IRF32, i8imm, ".MULSU">;
}

//
// 4.5.29 - IAU.NOT
//   Bitwise NOT.
//
// We can't use IAU_RROp for IAU_NOT since 'not' is a pattern fragment and not a SDNode
class IAU_NOT<RegisterClass regVT, ValueType vt> : 
  SHAVE_IAUInstr<(outs regVT:$dst), (ins regVT:$src1),
                 ".NOT $dst $src1", [(set (vt regVT:$dst), (not (vt regVT:$src1)))]>;

let Itinerary = II_IAU_RR_WCC, Defs = [CC_IAU0] in
{
  def IAU_NOT_32 : IAU_NOT<IRF32,   i32>;
  def IAU_NOT_16 : IAU_NOT<IRF16_l, i16>;
  def IAU_NOT_8  : IAU_NOT<IRF8_q0, i8>;
}

//
// 4.5.30 - IAU.ONES
//   Count number of bits set to 1/0.
//
let Itinerary = II_IAU_RR in
{
  def IAU_ONES_32 : IAU_RROpC<ctpop, IRF32, i32, ".ONES">;

  def IAU_ZEROS_32 : SHAVE_IAUInstr<(outs IRF32:$dst), (ins IRF32:$src), ".ZEROS $dst $src",
                                    [(set (i32 IRF32:$dst), (sub (i32 32), (ctpop (i32 IRF32:$src))))]>;
}

//
// 4.5.31 - IAU.OR
//   Bitwise OR.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0] in
{
  defm IAU_OR  : IAU_RRROp<or,  ".OR">;
}

//
// 4.5.32 - IAU.ROL
//   32-bit rotate left with optional immediate rotate amount.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0] in
{
  def IAU_ROL : IAU_RRROpC<rotl, IRF32, i32, ".ROL">;
}

let Itinerary = II_IAU_RRI in
{
  def IAU_ROL_imm : IAU_RRIOpC<rotl, IRF32, i32, i32imm, ".ROL",  uimm5>;
}

//
// 4.5.33 - IAU.RVRS
//   Reverse bit order.
//
let Itinerary = II_IAU_RR_WCC in
{
  def IAU_RVRS : IAU_RROpC<bitreverse, IRF32, i32, ".RVRS">;
}

//
// 4.5.34 - IAU.SHL
//   32-bit logical shift left with optional immediate shift amount.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0, I_STATE] in
{
  def IAU_SHL_i32 : IAU_RRROpC<shl, IRF32, i32, ".SHL">;

  // Special definition for i16 and i8 leftshifts, where the shift amount must be a full 32 bit value in the IRF
  def IAU_SHL_i16 : SHAVE_IAUInstr<(outs IRF16_l:$dst), (ins IRF16_l:$src, IRF32:$shamt), ".SHL $dst $src $shamt",
                                   [(set (i16 IRF16_l:$dst), (shl (i16 IRF16_l:$src), (i32 IRF32:$shamt)))]>;

  def IAU_SHL_i8 : SHAVE_IAUInstr<(outs IRF8_q0:$dst), (ins IRF8_q0:$src, IRF32:$shamt), ".SHL $dst $src $shamt",
                                  [(set (i8 IRF8_q0:$dst), (shl (i8 IRF8_q0:$src), (i32 IRF32:$shamt)))]>;
}

let Itinerary = II_IAU_RRI_WCC, Defs = [CC_IAU0, I_STATE] in
{
  defm IAU_SHL : IAU_RRIOp<shl, ".SHL", uimm5>;
}

//
// 4.5.35 - IAU.SHR
//   32-bit logical/arithmetic shift right with optional immediate shift amount. Optional round towards zero
//   for negative result >=-0.5
//
let Itinerary = II_IAU_RRR_WCC_WI, Defs = [CC_IAU0, I_STATE] in
{
  def IAU_SHR_u32 : IAU_RRROpC<srl, IRF32, i32, ".SHR.u32">;
  def IAU_SHR_i32 : IAU_RRROpC<sra, IRF32, i32, ".SHR.i32">;
}

let Itinerary = II_IAU_RRI_WCC_WI, Defs = [CC_IAU0, I_STATE] in
{
  def IAU_SHR_u32_imm      : IAU_RRIOpC<srl, IRF32, i32, i32imm, ".SHR.u32", uimm5>;
  def IAU_SHR_i32_imm      : IAU_RRIOpC<sra, IRF32, i32, i32imm, ".SHR.i32", uimm5>;

  // Special version of this instructions which is used for extracting an IRF16_l subvector
  // from an IRF32 vector
  def IAU_SHR_u16_u32 : SHAVE_IAUInstr<(outs IRF16_l:$dst), (ins IRF32:$src, i32imm:$imm),
                                       ".SHR.u32 $dst $src $imm", []>;
}

let Itinerary = II_IAU_RRR_WCC_WI, Defs = [CC_IAU0, I_STATE] in
{
  def IAU_SHD_i32 : IAU_RRROpC<shd, IRF32, i32, ".SHD">;
}

let Itinerary = II_IAU_RRI_WCC_WI, Defs = [CC_IAU0, I_STATE] in
{
  def IAU_SHD_i32_imm : IAU_RRIOpC<shd, IRF32, i32, i32imm, ".SHD",     uimm5>;
}

//
// 4.5.36 - IAU.SUB
//   32-bit integer subtraction with optional unsigned immediate operand.
//
let Itinerary = II_IAU_RRR_WI, Defs = [I_STATE] in
{
  defm IAU_SUB : IAU_RRROp<sub, ".SUB">;
}

let Itinerary = II_IAU_RRI_WCC_WI, Defs = [CC_IAU0, I_STATE] in
{
  defm IAU_SUB : IAU_RRIOp<sub, ".SUB", uimm8>;
}

//
// 4.5.37 - IAU.SUBCI
//   32-bit integer subtraction with carry-in.
//
let Itinerary = II_IAU_RRR_RWI, Uses = [I_STATE], Defs = [I_STATE] in
{
  def IAU_SUBCI : SHAVE_IAUInstr<(outs IRF32:$dst), (ins IRF32:$src1, IRF32:$src2),
                  ".SUBCI $dst $src1 $src2",
                  [(set (i32 IRF32:$dst), (sube (i32 IRF32:$src1), (i32 IRF32:$src2)))]>;
}

//
// 4.5.38 - IAU.SUBSI
//   32-bit signed integer subtraction with saturation. Optional unsigned immediate operand.
//
let Itinerary = II_IAU_RRR_RWI in
{
  def IAU_SUBSI     : IAU_RRROpC_NoPat<IRF32, ".SUBSI">;
  def IAU_SUBSI_imm : IAU_RRIOpC_NoPat<IRF32, i8imm, ".SUBSI">;
}

//
// 4.5.39 - IAU.SUBSU
//   32-bit unsigned integer subtraction with saturation. Optional immediate operand.
//
let Itinerary = II_IAU_RRR_RWI in
{
  def IAU_SUBSU     : IAU_RRROpC_NoPat<IRF32, ".SUBSU">;
  def IAU_SUBSU_imm : IAU_RRIOpC_NoPat<IRF32, i8imm, ".SUBSU">;
}

//
// 4.5.40 - IAU.XOR
//   Bitwise XOR.
//
let Itinerary = II_IAU_RRR_WCC, Defs = [CC_IAU0] in
{
  defm IAU_XOR : IAU_RRROp<xor, ".XOR">;
}

//
// 4.5.41 - IAU.XORT
//   Reduction XOR.
//
let Itinerary = II_IAU_RR_WCC_WI, Defs = [CC_IAU0, I_STATE] in
{
  def IAU_XORT : IAU_RROpC_NoPat<IRF32, ".XORT">;
}

//===----------------------------------------------------------------------===//
// IAU pattern definitions
//===----------------------------------------------------------------------===//

def IAU_ADDCO : Pat<(addc (i32 IRF32:$src1), (i32 IRF32:$src2)), (IAU_ADD_32 (i32 IRF32:$src1), (i32 IRF32:$src2))>;
def IAU_SUBCO : Pat<(subc (i32 IRF32:$src1), (i32 IRF32:$src2)), (IAU_SUB_32 (i32 IRF32:$src1), (i32 IRF32:$src2))>;

// fcopysign f32 magnitude, f32 sign
def IAU_COPYSIGN_F32 : Pat<(fcopysign (f32 IRF32:$src1), (f32 IRF32:$src2)),
                           (IAU_FINS_imm (f32 IRF32:$src1),
                                         (IAU_FEXTU_imm (f32 IRF32:$src2), (i32 31), (i32 1)),
                                         (i32 31),
                                         (i32 1))>;

// fcopysign f16 magnitude, f16 sign
def IAU_COPYSIGN_F16 : Pat<(fcopysign (f16 IRF16_l:$src1), (f16 IRF16_l:$src2)),
                           (EXTRACT_SUBREG
                               (IAU_FINS_imm (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF16_l:$src1, hsub_0),
                                             (IAU_FEXTU_imm (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF16_l:$src2, hsub_0), (i32 15), (i32 1)),
                                             (i32 15),
                                             (i32 1)),
                                hsub_0)>;

// ISD::FCOPYSIGN supports a mismatch in types between the magnitude and sign input operands, so we must account for that here
// fcopysign f32 magnitude, f16 sign
def IAU_COPYSIGN_F32_16 : Pat<(fcopysign (f32 IRF32:$src1), (f16 IRF16_l:$src2)),
                              (IAU_FINS_imm (f32 IRF32:$src1),
                                            (IAU_FEXTU_imm (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF16_l:$src2, hsub_0), (i32 15), (i32 1)),
                                            (i32 31),
                                            (i32 1))>;

// fcopysign f16 magnitude, f32 sign
def IAU_COPYSIGN_F16_32 : Pat<(fcopysign (f16 IRF16_l:$src1), (f32 IRF32:$src2)),
                              (EXTRACT_SUBREG
                                  (IAU_FINS_imm (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF16_l:$src1, hsub_0),
                                                (IAU_FEXTU_imm (f32 IRF32:$src2), (i32 31), (i32 1)),
                                                (i32 15),
                                                (i32 1)),
                                   hsub_0)>;
