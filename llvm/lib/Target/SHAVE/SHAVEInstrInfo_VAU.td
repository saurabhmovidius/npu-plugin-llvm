//===-- SHAVEInstrInfo_VAU.td - VAU Instructions -----------*- tablegen -*-===//
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
// This file describes the instructions available in the SHAVE Vector
// Arithmetic Unit (VAU)
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// VAU pseudo instructions
//===----------------------------------------------------------------------===//
let Predicates = [HasVRF128_Predicate] in {

class VAU_ACC_SEQ_BASE<RegisterClass regVT, list<SHAVEConflictName> confs = []> :
  SHAVE_Pseudo<(outs regVT:$dst), (ins variable_ops), "", [], confs> {
  // TODO: make dtemp an ins argument to get the temporary reg instead of I-1
  // let Constraints = "@earlyclobber $dtemp";
}

//===----------------------------------------------------------------------===//
// VAU generic class declarations
//===----------------------------------------------------------------------===//

//
// Reg <-- (Reg op Reg) classes
//

// base class for defining Reg <-- (Reg op Reg) type ops
class VAU_RRROpC_Base<RegisterClass regVT, string asmstr, list<dag> pattern, list<SHAVEConflictName> conflicts = []> :
  SHAVE_VAUInstr<(outs regVT:$dst), (ins regVT:$src1, regVT:$src2),
                 !strconcat(asmstr, " $dst $src1 $src2"), pattern, conflicts>;

// class for defining Reg <-- (Reg op Reg) type ops without a pattern
class VAU_RRROpC_Raw<RegisterClass regVT, string asmstr, list<SHAVEConflictName> conflicts = []> :
  VAU_RRROpC_Base<regVT, asmstr, [], conflicts>;

// class for defining Reg <-- (Reg op Reg) type ops
class VAU_RRROpC<SDNode opc, RegisterClass regVT, ValueType vt, string asmstr, list<SHAVEConflictName> conflicts = []> :
  VAU_RRROpC_Base<regVT, asmstr, [(set (vt regVT:$dst), (opc (vt regVT:$src1), (vt regVT:$src2)))], conflicts>;

//
// Reg <-- (Reg op Imm) classes
//

// base class for defining Reg <-- (Reg op Imm) type ops
class VAU_RRIOpC_Base<RegisterClass regVT, Operand immOp, string asmstr, list<dag> pattern, list<SHAVEConflictName> conflicts = []> :
  SHAVE_VAUInstr<(outs regVT:$dst), (ins regVT:$src, immOp:$imm),
                  !strconcat(asmstr, " $dst $src $imm"), pattern, conflicts>;

// class for defining Reg <-- (Reg op Imm) type ops without a pattern
class VAU_RRIOpC_Raw<RegisterClass regVT, Operand immOp, string asmstr, list<SHAVEConflictName> conflicts = []> :
  VAU_RRIOpC_Base<regVT, immOp, asmstr, [], conflicts>;

// class for defining Reg <-- (Reg op Imm) type ops
class VAU_RRIOpC<SDNode opc, RegisterClass regVT, ValueType vt, ValueType st, Operand immOp, string asmstr, PatLeaf immPat, list<SHAVEConflictName> conflicts = []> :
  VAU_RRIOpC_Base<regVT, immOp, asmstr, [(set (vt regVT:$dst), (opc (vt regVT:$src), (vt (splat (st immPat:$imm)))))], conflicts>;

multiclass VAU_RRIOp<SDNode opc, string asmstr, PatLeaf immPat, list<SHAVEConflictName> conflicts = []>
{
  def _i32_imm       : VAU_RRIOpC<opc, VRF128,  v4i32, i32, i32imm, !strconcat(asmstr, ".i32"), immPat, conflicts>;
  def _i16_imm       : VAU_RRIOpC<opc, VRF128,  v8i16, i16, i16imm, !strconcat(asmstr, ".i16"), immPat, conflicts>;
  def _i8_imm        : VAU_RRIOpC<opc, VRF128,  v16i8, i8,  i8imm,  !strconcat(asmstr, ".i8"),  immPat, conflicts>;

  def _i32_imm_v2i32 : VAU_RRIOpC<opc, VRF64_l, v2i32, i32, i32imm, !strconcat(asmstr, ".i32"), immPat, conflicts>;
  def _i16_imm_v4i16 : VAU_RRIOpC<opc, VRF64_l, v4i16, i16, i16imm, !strconcat(asmstr, ".i16"), immPat, conflicts>;
  def _i8_imm_v8i8   : VAU_RRIOpC<opc, VRF64_l, v8i8,  i8,  i8imm,  !strconcat(asmstr, ".i8"),  immPat, conflicts>;
}

//
// Reg <-- (op Reg) classes
//

// base class for defining Reg <-- (op Reg) type ops
class VAU_RROpC_Base<RegisterClass regVT, string asmstr, list<dag> pattern> :
  SHAVE_VAUInstr<(outs regVT:$dst), (ins regVT:$src),
                 !strconcat(asmstr, " $dst $src"), pattern>;

// class for defining Reg <-- (op Reg) type ops without a pattern
class VAU_RROpC_Raw<RegisterClass regVT, string asmstr> :
  VAU_RROpC_Base<regVT, asmstr, []>;

// class for defining Reg <-- (op Reg) type ops
class VAU_RROpC<SDNode opc, RegisterClass regVT, ValueType vt, string asmstr> :
  VAU_RROpC_Base<regVT, asmstr, [(set (vt regVT:$dst), (opc (vt regVT:$src)))]>;

//
// Multiclasses for logic instructions
//

multiclass VAU_LogicX<SDNode op, string asmstr>
{
  def _x32   : VAU_RRROpC<op, VRF128,  v4i32, !strconcat(asmstr, ".x32")>;
  def _x16   : VAU_RRROpC<op, VRF128,  v8i16, !strconcat(asmstr, ".x16")>;
  def _x8    : VAU_RRROpC<op, VRF128,  v16i8, !strconcat(asmstr, ".x8")>;
  def _v2x32 : VAU_RRROpC<op, VRF64_l, v2i32, !strconcat(asmstr, ".x32")>;
  def _v4x16 : VAU_RRROpC<op, VRF64_l, v4i16, !strconcat(asmstr, ".x16")>;
  def _v8x8  : VAU_RRROpC<op, VRF64_l, v8i8,  !strconcat(asmstr, ".x8")>;
}

multiclass VAU_LogicIs<SDNode op, string asmstr>
{
  def _i32   : VAU_RRROpC<op, VRF128,  v4i32, !strconcat(asmstr, ".i32")>;
  def _i16   : VAU_RRROpC<op, VRF128,  v8i16, !strconcat(asmstr, ".i16")>;
  def _i8    : VAU_RRROpC<op, VRF128,  v16i8, !strconcat(asmstr, ".i8")>;
  def _v2i32 : VAU_RRROpC<op, VRF64_l, v2i32, !strconcat(asmstr, ".i32")>;
  def _v4i16 : VAU_RRROpC<op, VRF64_l, v4i16, !strconcat(asmstr, ".i16")>;
  def _v8i8  : VAU_RRROpC<op, VRF64_l, v8i8,  !strconcat(asmstr, ".i8")>;
}

multiclass VAU_LogicUs<SDNode op, string asmstr>
{
  def _u32   : VAU_RRROpC<op, VRF128,  v4i32, !strconcat(asmstr, ".u32")>;
  def _u16   : VAU_RRROpC<op, VRF128,  v8i16, !strconcat(asmstr, ".u16")>;
  def _u8    : VAU_RRROpC<op, VRF128,  v16i8, !strconcat(asmstr, ".u8")>;
  def _v2u32 : VAU_RRROpC<op, VRF64_l, v2i32, !strconcat(asmstr, ".u32")>;
  def _v4u16 : VAU_RRROpC<op, VRF64_l, v4i16, !strconcat(asmstr, ".u16")>;
  def _v8u8  : VAU_RRROpC<op, VRF64_l, v8i8,  !strconcat(asmstr, ".u8")>;
}

multiclass VAU_LogicX_Imm<SDNode op, string asmstr, PatLeaf immPat>
{
  def _x32_imm       : VAU_RRIOpC<op, VRF128,  v4i32, i32, i32imm, !strconcat(asmstr, ".x32"), immPat>;
  def _x16_imm       : VAU_RRIOpC<op, VRF128,  v8i16, i16, i16imm, !strconcat(asmstr, ".x16"), immPat>;
  def _x8_imm        : VAU_RRIOpC<op, VRF128,  v16i8, i8,  i8imm,  !strconcat(asmstr, ".x8"),  immPat>;
  def _x32_imm_v2x32 : VAU_RRIOpC<op, VRF64_l, v2i32, i32, i32imm, !strconcat(asmstr, ".x32"), immPat>;
  def _x16_imm_v4x16 : VAU_RRIOpC<op, VRF64_l, v4i16, i16, i16imm, !strconcat(asmstr, ".x16"), immPat>;
  def _x8_imm_v8x8   : VAU_RRIOpC<op, VRF64_l, v8i8,  i8,  i8imm,  !strconcat(asmstr, ".x8"),  immPat>;
}

multiclass VAU_LogicIs_Imm<SDNode op, string asmstr, PatLeaf immPat>
{
  def _i32_imm            : VAU_RRIOpC<op, VRF128,  v4i32, i32, i32imm, !strconcat(asmstr, ".i32"), immPat>;
  def _i16_imm            : VAU_RRIOpC<op, VRF128,  v8i16, i16, i16imm, !strconcat(asmstr, ".i16"), immPat>;
  def _i8_imm             : VAU_RRIOpC<op, VRF128,  v16i8, i8,  i8imm,  !strconcat(asmstr, ".i8"),  immPat>;
  def _i32_imm_v2i32      : VAU_RRIOpC<op, VRF64_l, v2i32, i32, i32imm, !strconcat(asmstr, ".i32"), immPat>;
  def _i16_imm_v4i16      : VAU_RRIOpC<op, VRF64_l, v4i16, i16, i16imm, !strconcat(asmstr, ".i16"), immPat>;
  def _i8_imm_v8i8        : VAU_RRIOpC<op, VRF64_l, v8i8,  i8,  i8imm,  !strconcat(asmstr, ".i8"),  immPat>;
}


multiclass VAU_LogicUs_Imm<SDNode op, string asmstr, PatLeaf immPat>
{
  def _u32_imm            : VAU_RRIOpC<op, VRF128,  v4i32, i32, i32imm, !strconcat(asmstr, ".u32"), immPat>;
  def _u16_imm            : VAU_RRIOpC<op, VRF128,  v8i16, i16, i16imm, !strconcat(asmstr, ".u16"), immPat>;
  def _u8_imm             : VAU_RRIOpC<op, VRF128,  v16i8, i8,  i8imm,  !strconcat(asmstr, ".u8"),  immPat>;
  def _u32_imm_v2u32      : VAU_RRIOpC<op, VRF64_l, v2i32, i32, i32imm, !strconcat(asmstr, ".u32"), immPat>;
  def _u16_imm_v4u16      : VAU_RRIOpC<op, VRF64_l, v4i16, i16, i16imm, !strconcat(asmstr, ".u16"), immPat>;
  def _u8_imm_v8u8        : VAU_RRIOpC<op, VRF64_l, v8i8,  i8,  i8imm,  !strconcat(asmstr, ".u8"),  immPat>;
}

multiclass VAU_BitwiseLogic<SDNode op, string asmstr>
{
  def _32    : VAU_RRROpC<op, VRF128,  v4i32, asmstr>;
  def _16    : VAU_RRROpC<op, VRF128,  v8i16, asmstr>;
  def _8     : VAU_RRROpC<op, VRF128,  v16i8, asmstr>;
  def _v2i32 : VAU_RRROpC<op, VRF64_l, v2i32, asmstr>;
  def _v4i16 : VAU_RRROpC<op, VRF64_l, v4i16, asmstr>;
  def _v8i8  : VAU_RRROpC<op, VRF64_l, v8i8,  asmstr>;
}

//
// Multiclasses for saturating arithmetic instructions without patterns
//

multiclass VAU_RRRsOp_Raw<string asmstr, list<SHAVEConflictName> conflicts = []>
{
  def _i32  : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".i32"), conflicts>;
  def _i16  : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".i16"), conflicts>;
  def _i8   : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".i8"),  conflicts>;
  def _u32  : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".u32"), conflicts>;
  def _u16  : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".u16"), conflicts>;
  def _u8   : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".u8"),  conflicts>;
}

multiclass VAU_RRROp_Raw<string asmstr, list<SHAVEConflictName> conflicts = []>
{
  def _i32  : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".i32"), conflicts>;
  def _i16  : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".i16"), conflicts>;
  def _i8   : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".i8"),  conflicts>;
  def _u32  : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".u32"), conflicts>;
  def _u16  : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".u16"), conflicts>;
  def _u8   : VAU_RRROpC_Raw<VRF128, !strconcat(asmstr, ".u8"),  conflicts>;
}

multiclass VAU_RRIsOp_Raw<string asmstr, list<SHAVEConflictName> conflicts = []>
{
  def _i32_imm  : VAU_RRIOpC_Raw<VRF128, i32imm, !strconcat(asmstr, ".i32"), conflicts>;
  def _i16_imm  : VAU_RRIOpC_Raw<VRF128, i16imm, !strconcat(asmstr, ".i16"), conflicts>;
  def _i8_imm   : VAU_RRIOpC_Raw<VRF128, i8imm,  !strconcat(asmstr, ".i8"),  conflicts>;
  def _u32_imm  : VAU_RRIOpC_Raw<VRF128, i32imm, !strconcat(asmstr, ".u32"), conflicts>;
  def _u16_imm  : VAU_RRIOpC_Raw<VRF128, i16imm, !strconcat(asmstr, ".u16"), conflicts>;
  def _u8_imm   : VAU_RRIOpC_Raw<VRF128, i8imm,  !strconcat(asmstr, ".u8"),  conflicts>;
}


//===----------------------------------------------------------------------===//
// VAU instruction definitions (Section 1.4.9 of Fragrak SHAVE Reference Guide)
//===----------------------------------------------------------------------===//

//
// 1.4.9.1 - VAU.ABS
//   Long vector absolute value.
//

let Itinerary = II_VAU_ABS_VRF in
{
  def VAU_ABS_i32   : VAU_RROpC_Raw<  VRF128,         ".ABS.i32">;
  def VAU_ABS_i16   : VAU_RROpC_Raw<  VRF128,         ".ABS.i16">;
  def VAU_ABS_i8    : VAU_RROpC_Raw<  VRF128,         ".ABS.i8">;
  def VAU_ABS_f32   : VAU_RROpC_Raw<  VRF128,         ".ABS.f32">;
  def VAU_ABS_f16   : VAU_RROpC_Raw<  VRF128,         ".ABS.f16">;
  def VAU_ABS_v4f32 : VAU_RROpC<fabs, VRF128,  v4f32, ".ABS.f32">;
  def VAU_ABS_v2f32 : VAU_RROpC<fabs, VRF64_l, v2f32, ".ABS.f32">;
  def VAU_ABS_v8f16 : VAU_RROpC<fabs, VRF128,  v8f16, ".ABS.f16">;
  def VAU_ABS_v4f16 : VAU_RROpC<fabs, VRF64_l, v4f16, ".ABS.f16">;
}

//
// 1.4.9.2 - VAU.ACC
//   Vector add/subtract accumulate with optional zeroing of the accumulator.
//


let Defs = [V_ACC, V_ACC0x, V_ACC1x] in {
  def VAU_ACCP_SEQ_i32 : VAU_ACC_SEQ_BASE<VRF128, [isACCPSeq]>;
  def VAU_ACCP_SEQ_u32 : VAU_ACC_SEQ_BASE<VRF128, [isACCPSeq]>;
  def VAU_ACCP_SEQ_i16 : VAU_ACC_SEQ_BASE<VRF128, [isACCPSeq]>;
  def VAU_ACCP_SEQ_u16 : VAU_ACC_SEQ_BASE<VRF128, [isACCPSeq]>;
  def VAU_ACCP_SEQ_i8  : VAU_ACC_SEQ_BASE<VRF128, [isACCPSeq]>;
  def VAU_ACCP_SEQ_u8  : VAU_ACC_SEQ_BASE<VRF128, [isACCPSeq]>;

  def VAU_ACCN_SEQ_i32 : VAU_ACC_SEQ_BASE<VRF128>;
  def VAU_ACCN_SEQ_u32 : VAU_ACC_SEQ_BASE<VRF128>;
  def VAU_ACCN_SEQ_i16 : VAU_ACC_SEQ_BASE<VRF128>;
  def VAU_ACCN_SEQ_u16 : VAU_ACC_SEQ_BASE<VRF128>;
  def VAU_ACCN_SEQ_i8  : VAU_ACC_SEQ_BASE<VRF128>;
  def VAU_ACCN_SEQ_u8  : VAU_ACC_SEQ_BASE<VRF128>;
}

let Defs = [V_ACC, V_ACC0x, V_ACC1x, V0] in {
  def VAU_ACCP_SEQ_f32 : VAU_ACC_SEQ_BASE<VRF128, [isACCPSeq]>;
  def VAU_ACCP_SEQ_f16 : VAU_ACC_SEQ_BASE<VRF128, [isACCPSeq]>;

  def VAU_ACCN_SEQ_f32 : VAU_ACC_SEQ_BASE<VRF128>;
  def VAU_ACCN_SEQ_f16 : VAU_ACC_SEQ_BASE<VRF128>;
}



class VAU_ACC_BASE<RegisterClass accRC, RegisterClass RC, string asmstr, list<SHAVEConflictName> conflicts = []> :
    SHAVE_VAUInstr<(outs accRC:$dst_acc_reg), (ins RC:$src1, accRC:$src_acc_reg),
               !strconcat(asmstr, " $src1 $src_acc_reg"), [], !listconcat(conflicts, [isACC])> {
  let Constraints = "$dst_acc_reg = $src_acc_reg";
}

// Integer accumulator patterns
multiclass VAU_ACC_TYPE<string asmstr> {
  def _i32    : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i32"), [isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u32    : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u32"), [isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _i16    : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i16"), [isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u16    : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u16"), [isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _i8     : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i8"),  [isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u8     : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u8"),  [isACC_MAC_Int, usesVAUIntegerAdder]>;
  let IsPipelinedACCBit = 1 in {
     def _i32_pl : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i32"), [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u32_pl : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u32"), [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _i16_pl : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i16"), [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u16_pl : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u16"), [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _i8_pl  : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i8"),  [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u8_pl  : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u8"),  [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
  }
}

let Itinerary = II_VAU_ACC_INT, ACC_MAC_InsnType = ACC_MAC_Z in {
  defm VAU_ACCPZ : VAU_ACC_TYPE<".ACCPZ">;
  defm VAU_ACCNZ : VAU_ACC_TYPE<".ACCNZ">;

}

let Itinerary = II_VAU_ACC_INT, ACC_MAC_InsnType = ACC_MAC_PN in {
  let Predicates = [HasVRF128_Predicate] in {
    defm VAU_ACCP  : VAU_ACC_TYPE<".ACCP">;
    defm VAU_ACCN  : VAU_ACC_TYPE<".ACCN">;
  }
}

// Floating-point accumulator patterns
multiclass VAU_ACC_TYPE_FLOAT<string asmstr> {
  let Itinerary = II_VAU_ACC_FP_Pipe, IsPipelinedACCBit = 1, Predicates = [HasVRF128_Predicate] in {
    def _f32_pl : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".f32"), [isPipelinedACCorMAC, isACC_MAC_Float, usesVAUFloatingPointAdder]>;
    def _f16_pl : VAU_ACC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".f16"), [isPipelinedACCorMAC, isACC_MAC_Float, usesVAUFloatingPointAdder]>;
  }
}
  let ACC_MAC_InsnType = ACC_MAC_Z in {
    defm  VAU_ACCPZ : VAU_ACC_TYPE_FLOAT<".ACCPZ">;
    defm  VAU_ACCNZ : VAU_ACC_TYPE_FLOAT<".ACCNZ">;
   }

  let ACC_MAC_InsnType = ACC_MAC_PN in {
     defm  VAU_ACCP : VAU_ACC_TYPE_FLOAT<".ACCP">;
     defm  VAU_ACCN : VAU_ACC_TYPE_FLOAT<".ACCN">;
   }

//
// 1.4.9.3 - VAU.ACCW
//   Vector add/subtract accumulate with write to VRF.
//

class VAU_ACCW_BASE<RegisterClass accRC, RegisterClass RC, string asmstr, list<SHAVEConflictName> conflicts = []> :
    SHAVE_VAUInstr<(outs RC:$dst, accRC:$dst_acc_reg), (ins RC:$src1, accRC:$src_acc_reg),
                 !strconcat(asmstr, " $dst $src1 $src_acc_reg"), [], !listconcat(conflicts, [isACC, isACC_MAC_W])>{
    let Constraints = "$dst_acc_reg = $src_acc_reg";
    let ACC_MAC_InsnType = ACC_MAC_W;
}


multiclass VAU_ACCW_TYPE<string asmstr> {
  def _i32    : VAU_ACCW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i32"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u32    : VAU_ACCW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u32"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _i16    : VAU_ACCW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i16"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u16    : VAU_ACCW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u16"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _i8     : VAU_ACCW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i8"),  [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u8     : VAU_ACCW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u8"),  [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;

  let IsPipelinedACCBit = 1 in {
     def _i32_pl : VAU_ACCW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".i32"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u32_pl : VAU_ACCW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".u32"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _i16_pl : VAU_ACCW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".i16"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u16_pl : VAU_ACCW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".u16"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _i8_pl  : VAU_ACCW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".i8"),  [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u8_pl  : VAU_ACCW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".u8"),  [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  }
}

multiclass VAU_ACCW_TYPE_FLOAT<string asmstr> {
 let Itinerary = II_VAU_ACCW_FP_Pipe, IsPipelinedACCBit = 1, Predicates = [HasVRF128_Predicate] in {
    def _f32_pl : VAU_ACCW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".f32"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Float, usesVAUFloatingPointAdder]>;
    def _f16_pl : VAU_ACCW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".f16"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Float, usesVAUFloatingPointAdder]>;
 }

}

let Itinerary = II_VAU_ACCW_INT in {
  let Predicates = [HasVRF128_Predicate] in {
    defm VAU_ACCPW  : VAU_ACCW_TYPE<".ACCPW">;
    defm VAU_ACCNW  : VAU_ACCW_TYPE<".ACCNW">;
  }
}

  defm VAU_ACCPW : VAU_ACCW_TYPE_FLOAT<".ACCPW">;
  defm VAU_ACCNW : VAU_ACCW_TYPE_FLOAT<".ACCNW">;

//
// 1.4.9.4 - VAU.ADD
//   Vector addition with optional immediate operand.
//

// Integer variants
let Itinerary = II_VAU_ADD_SUB_RRR_VRF, Defs = [I_AE] in
{
  def VAU_ADD_i32   : VAU_RRROpC<add, VRF128,  v4i32, ".ADD.i32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_ADD_i16   : VAU_RRROpC<add, VRF128,  v8i16, ".ADD.i16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_ADD_i8    : VAU_RRROpC<add, VRF128,  v16i8, ".ADD.i8",  [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;

  def VAU_ADD_v2i32 : VAU_RRROpC<add, VRF64_l, v2i32, ".ADD.i32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_ADD_v4i16 : VAU_RRROpC<add, VRF64_l, v4i16, ".ADD.i16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_ADD_v8i8  : VAU_RRROpC<add, VRF64_l, v8i8,  ".ADD.i8",  [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_RRI_VRF, Defs = [I_AE] in
{
  defm VAU_ADD : VAU_RRIOp<add, ".ADD", uimm5, [usesVAUIntegerAdder]>;
}

// Floating point variants
let Itinerary = II_VAU_ADD_SUB_MUL_RRR_VRF, Defs = [I_AE] in
{
  def VAU_ADD_f32     : VAU_RRROpC<fadd, VRF128,  v4f32, ".ADD.f32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  def VAU_ADD_f16     : VAU_RRROpC<fadd, VRF128,  v8f16, ".ADD.f16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;

  def VAU_ADD_v2f32   : VAU_RRROpC<fadd, VRF64_l, v2f32, ".ADD.f32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  def VAU_ADD_v4f16   : VAU_RRROpC<fadd, VRF64_l, v4f16, ".ADD.f16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_MUL_RRI_VRF, Defs = [I_AE] in
{
  def VAU_ADD_f32_imm : VAU_RRIOpC<fadd, VRF128, v4f32, f32, constf32, ".ADD.f32", FloatConst,     [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  def VAU_ADD_f16_imm : VAU_RRIOpC<fadd, VRF128, v8f16, f16, constf16, ".ADD.f16", HalfFloatConst, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
}

//
// 1.4.9.5 - VAU.ADDCI
//   Long vector 32-bit integer addition with carry-in.
//

let Itinerary = II_VAU_ADDCI_SUBCI_VRF, Uses = [V_STATE], Defs = [V_STATE] in
{
  def VAU_ADDCI_i32 : VAU_RRROpC_Raw<VRF128, ".ADDCI", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

//
// 1.4.9.6 - VAU.ADIFF
//   Long vector absolute difference.
//

class VAU_ADIFF_Class<RegisterClass RC, ValueType ty, string typestr> :
  SHAVE_VAUInstr<(outs RC:$dst), (ins RC:$src1, RC:$src2),
                 !strconcat(".ADIFF.", typestr, " $dst $src1 $src2"),
                 [(set RC:$dst, (ty (fabs (ty (fsub RC:$src1, RC:$src2)))))], [usesVAUFloatingPointAdder]>;

let Itinerary = II_VAU_ADIFF_VRF_FLOAT in
{
  def VAU_ADIFF_f32 : VAU_ADIFF_Class<VRF128, v4f32, "f32">;
  def VAU_ADIFF_f16 : VAU_ADIFF_Class<VRF128, v8f16, "f16">;
}

let Itinerary = II_VAU_ADIFF_VRF in
{
  defm VAU_ADIFF : VAU_RRROp_Raw<".ADIFF", [usesVAUIntegerAdder]>;
}

//
// 1.4.9.7 - VAU.ALIGNBYTE
//   Long vector align bytes within words.
//

class VAU_Align<RegisterClass dstRC, RegisterClass srcRC, string asmstr> :
  SHAVE_VAUInstr<(outs dstRC:$dst), (ins srcRC:$src1, srcRC:$src2, i8imm:$align),
                 !strconcat(asmstr, " $dst $src1 $src2 $align"), []>;

let Itinerary = II_VAU_ALIGN_VRF in
{
  def VAU_ALIGNBYTE_imm_vrf       : VAU_Align<VRF128,  VRF128,  ".ALIGNBYTE">;
}

//
// 1.4.9.8 - VAU.AND
//   Bitwise AND.
//

let Itinerary = II_VAU_AND_OR_XOR_VRF in
{
  defm VAU_AND : VAU_BitwiseLogic<and, ".AND">;
}

//
// 1.4.9.9 - VAU.AVG
//   Long vector integer average.
//

class VAU_AVG<ValueType vt, string asmstr> :
  SHAVE_VAUInstr<(outs VRF128:$dst), (ins VRF128:$src1, VRF128:$src2),
                 !strconcat(".AVG.", asmstr, " $dst $src1 $src2"),
                 [(set (vt VRF128:$dst), (srl (add (add (vt VRF128:$src1), (vt VRF128:$src2)), (vt 1)), (vt 1)))], [usesVAUIntegerAdder]>;

let Itinerary = II_VAU_AVG_VRF in
{
  def VAU_AVG_x32 : VAU_AVG<v4i32, "x32">;
  def VAU_AVG_x16 : VAU_AVG<v8i16, "x16">;
  def VAU_AVG_x8  : VAU_AVG<v16i8, "x8">;
}

//
// 1.4.9.10 - VAU.CMB
//   Long vector combine.
//

// Immediate operands appear in the reverse order (left-to-right) compared to
// the generated ASM (right-to-left).
class VAU_Combine<RegisterClass RC, string asmstr, list<SHAVEConflictName> conflicts = []> :
  SHAVE_VAUInstr<(outs RC:$dst), (ins RC:$src1, RC:$src2, ben:$en0, ben:$en1, ben:$en2, ben:$en3),
                 !strconcat(asmstr, " $dst $src2 [$en3$en2$en1$en0]"), [], conflicts>
{
  let Constraints = "$src1 = $dst";
}

let Itinerary = II_VAU_CMB_VRF in
{
  def VAU_CMBWORD_imm_vrf        : VAU_Combine<VRF128, ".CMB.WORD">;

  let Predicates = [HasVRF128_Predicate] in {
    def VAU_CMBBYTE_imm_vrf : VAU_Combine<VRF128, ".CMB.BYTE">;
  }
}

//
// 1.4.9.11 - VAU.IADDS
//   Long vector integer addition with saturation. Optional unsigned immediate operand.
//

let Itinerary = II_VAU_ADD_SUB_RRR_VRF_WITH_NO_CONDITION in
{
  defm VAU_IADDS : VAU_RRRsOp_Raw<".IADDS", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_RRI_VRF_WITH_NO_CONDITION in
{
  defm VAU_IADDS : VAU_RRIsOp_Raw<".IADDS", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

//
// 1.4.9.12 - VAU.ILMULH
//   Long vector multiply returning higher-order result.
//

let Itinerary = II_VAU_ILMULH_VRF in
{
  def VAU_ILMULH_i32   : VAU_RRROpC<mulhs, VRF128,  v4i32, ".ILMULH.i32">;
  def VAU_ILMULH_u32   : VAU_RRROpC<mulhu, VRF128,  v4i32, ".ILMULH.u32">;
  def VAU_ILMULH_i16   : VAU_RRROpC<mulhs, VRF128,  v8i16, ".ILMULH.i16">;
  def VAU_ILMULH_u16   : VAU_RRROpC<mulhu, VRF128,  v8i16, ".ILMULH.u16">;
  def VAU_ILMULH_i8    : VAU_RRROpC<mulhs, VRF128,  v16i8, ".ILMULH.i8">;
  def VAU_ILMULH_u8    : VAU_RRROpC<mulhu, VRF128,  v16i8, ".ILMULH.u8">;

  def VAU_ILMULH_v2i32 : VAU_RRROpC<mulhs, VRF64_l, v2i32, ".ILMULH.i32">;
  def VAU_ILMULH_v2u32 : VAU_RRROpC<mulhu, VRF64_l, v2i32, ".ILMULH.u32">;
  def VAU_ILMULH_v4i16 : VAU_RRROpC<mulhs, VRF64_l, v4i16, ".ILMULH.i16">;
  def VAU_ILMULH_v4u16 : VAU_RRROpC<mulhu, VRF64_l, v4i16, ".ILMULH.u16">;
  def VAU_ILMULH_v8i8  : VAU_RRROpC<mulhs, VRF64_l, v8i8,  ".ILMULH.i8">;
  def VAU_ILMULH_v8u8  : VAU_RRROpC<mulhu, VRF64_l, v8i8,  ".ILMULH.u8">;

  def VAU_ILMULH_v2i16 : VAU_RRROpC<mulhs, VRF32_q0,  v2i16, ".ILMULH.i16">;
  def VAU_ILMULH_v2u16 : VAU_RRROpC<mulhu, VRF32_q0,  v2i16, ".ILMULH.u16">;
  def VAU_ILMULH_v4i8  : VAU_RRROpC<mulhs, VRF32_q0,  v4i8, ".ILMULH.i8">;
  def VAU_ILMULH_v4u8  : VAU_RRROpC<mulhu, VRF32_q0,  v4i8, ".ILMULH.u8">;

  def VAU_ILMULH_v2i8  : VAU_RRROpC<mulhs, VRF16_e0,  v2i8, ".ILMULH.i8">;
  def VAU_ILMULH_v2u8  : VAU_RRROpC<mulhu, VRF16_e0,  v2i8, ".ILMULH.u8">;
}

//
// 1.4.9.13 - VAU.IMULS
//   Long vector integer multiplication with saturation. Optional immediate operand
//

let Itinerary = II_VAU_MUL_RRR_VRF_WITH_NO_CONDITION in
{
  defm VAU_IMULS : VAU_RRRsOp_Raw<".IMULS">;
}

let Itinerary = II_VAU_MUL_RRI_VRF_WITH_NO_CONDITION in
{
  defm VAU_IMULS : VAU_RRIsOp_Raw<".IMULS">;
}

//
// 1.4.9.14 - VAU.ISUBS
//   Long vector integer subtraction with saturation. Optional unsigned immediate operand.
//

let Itinerary = II_VAU_ADD_SUB_RRR_VRF_WITH_NO_CONDITION in
{
  defm VAU_ISUBS : VAU_RRRsOp_Raw<".ISUBS", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_RRI_VRF_WITH_NO_CONDITION in
{
  defm VAU_ISUBS : VAU_RRIsOp_Raw<".ISUBS", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

//
// 1.4.9.15 - VAU.MAC
//   Vector multiply-add/subtract accumulate with optional zeroing of the accumulator.
//
//
// Pseudo instructions for VAU.MAC and VAU.MACPW variants
//
class VAU_MAC_SEQ_BASE<RegisterClass RC, list<SHAVEConflictName> confs = []> :
    SHAVE_Pseudo<(outs RC:$dst), (ins RC:$src, variable_ops), "", [], confs>{
    // TODO: make dtemp an ins argument to get the temporary reg instead of I-1
    // let Constraints = "@earlyclobber $dtemp";
}

let Defs = [V_ACC, V_ACC0x, V_ACC1x] in {
  def VAU_MACP_SEQ_i32 : VAU_MAC_SEQ_BASE<VRF128, [isMACPSeq]>;
  def VAU_MACP_SEQ_u32 : VAU_MAC_SEQ_BASE<VRF128, [isMACPSeq]>;
  def VAU_MACP_SEQ_i16 : VAU_MAC_SEQ_BASE<VRF128, [isMACPSeq]>;
  def VAU_MACP_SEQ_u16 : VAU_MAC_SEQ_BASE<VRF128, [isMACPSeq]>;
  def VAU_MACP_SEQ_i8  : VAU_MAC_SEQ_BASE<VRF128, [isMACPSeq]>;
  def VAU_MACP_SEQ_u8  : VAU_MAC_SEQ_BASE<VRF128, [isMACPSeq]>;

  def VAU_MACN_SEQ_i32 : VAU_MAC_SEQ_BASE<VRF128>;
  def VAU_MACN_SEQ_u32 : VAU_MAC_SEQ_BASE<VRF128>;
  def VAU_MACN_SEQ_i16 : VAU_MAC_SEQ_BASE<VRF128>;
  def VAU_MACN_SEQ_u16 : VAU_MAC_SEQ_BASE<VRF128>;
  def VAU_MACN_SEQ_i8  : VAU_MAC_SEQ_BASE<VRF128>;
  def VAU_MACN_SEQ_u8  : VAU_MAC_SEQ_BASE<VRF128>;
}

let Defs = [V_ACC, V_ACC0x, V_ACC1x, V0] in {
  def VAU_MACP_SEQ_f32 : VAU_MAC_SEQ_BASE<VRF128, [isMACPSeq]>;
  def VAU_MACP_SEQ_f16 : VAU_MAC_SEQ_BASE<VRF128, [isMACPSeq]>;

  def VAU_MACN_SEQ_f32 : VAU_MAC_SEQ_BASE<VRF128>;
  def VAU_MACN_SEQ_f16 : VAU_MAC_SEQ_BASE<VRF128>;
}


let isCommutable = 1 in {
  class VAU_MAC_BASE<RegisterClass accRC, RegisterClass RC, string asmstr, list<SHAVEConflictName> conflicts = []> :
      SHAVE_VAUInstr<(outs accRC:$dst_acc_reg), (ins RC:$src1, RC:$src2, accRC:$src_acc_reg),
                 !strconcat(asmstr, " $src1 $src2 $src_acc_reg"), [], !listconcat(conflicts, [isMAC])> {
    let Constraints = "$dst_acc_reg = $src_acc_reg";
    let IsMACInsnBit = 1;
  }
}

multiclass VAU_MAC_TYPE<string asmstr>{
  def _i32    : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i32"), [isACC_MAC_Int]>;
  def _u32    : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u32"), [isACC_MAC_Int]>;
  def _i16    : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i16"), [isACC_MAC_Int]>;
  def _u16    : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u16"), [isACC_MAC_Int]>;
  def _i8     : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i8"),  [isACC_MAC_Int]>;
  def _u8     : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u8"),  [isACC_MAC_Int]>;

  let IsPipelinedACCBit = 1 in {
       def _i32_pl : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i32"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _u32_pl : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u32"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _i16_pl : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i16"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _u16_pl : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u16"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _i8_pl  : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i8"),  [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _u8_pl  : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u8"),  [ isPipelinedACCorMAC, isACC_MAC_Int]>;
   }
}

let Itinerary = II_VAU_MAC_INT, ACC_MAC_InsnType = ACC_MAC_Z in {
  defm VAU_MACPZ : VAU_MAC_TYPE<".MACPZ">;
  defm VAU_MACNZ : VAU_MAC_TYPE<".MACNZ">;
}

let Itinerary = II_VAU_MAC_INT, ACC_MAC_InsnType = ACC_MAC_PN in {
  defm VAU_MACP  : VAU_MAC_TYPE<".MACP">;
  defm VAU_MACN  : VAU_MAC_TYPE<".MACN">;
}

// Floating-point MAC accumulator patterns
multiclass VAU_MAC_TYPE_FLOAT<string asmstr> {
  let Itinerary = II_VAU_MAC_FP_Pipe, IsPipelinedACCBit = 1, Predicates = [HasVRF128_Predicate] in {
    def _f32_pl : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".f32"), [ isPipelinedACCorMAC, isACC_MAC_Float]>;
    def _f16_pl : VAU_MAC_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".f16"), [ isPipelinedACCorMAC, isACC_MAC_Float]>;
  }
}

let ACC_MAC_InsnType = ACC_MAC_Z in {
  defm VAU_MACPZ : VAU_MAC_TYPE_FLOAT<".MACPZ">;
  defm VAU_MACNZ : VAU_MAC_TYPE_FLOAT<".MACNZ">;
}

let ACC_MAC_InsnType = ACC_MAC_PN in {
  defm VAU_MACP : VAU_MAC_TYPE_FLOAT<".MACP">;
  defm VAU_MACN : VAU_MAC_TYPE_FLOAT<".MACN">;
}

//
// 1.4.8.23 - VAU.MACNW
//   Scalar multiply-subtract accumulate with write to IRF.
//

let isCommutable = 1 in {
  class VAU_MACW_BASE<RegisterClass accRC, RegisterClass RC, string asmstr, list<SHAVEConflictName> conflicts = []> :
      SHAVE_VAUInstr<(outs RC:$dst, accRC:$dst_acc_reg), (ins RC:$src1, RC:$src2, accRC:$src_acc_reg),
                  !strconcat(asmstr, " $dst $src1 $src2 $src_acc_reg "), [], !listconcat(conflicts, [isMAC, isACC_MAC_W])> {
       let Constraints = "$dst_acc_reg = $src_acc_reg";
       let ACC_MAC_InsnType = ACC_MAC_W;
       let IsMACInsnBit = 1;
  }
}

multiclass VAU_MACW_TYPE<string asmstr>
{
  def _i32    : VAU_MACW_BASE<TRF_ACC128, VRF128,  !strconcat(asmstr, ".i32"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _u32    : VAU_MACW_BASE<TRF_ACC128, VRF128,  !strconcat(asmstr, ".u32"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _i16    : VAU_MACW_BASE<TRF_ACC128, VRF128,  !strconcat(asmstr, ".i16"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _u16    : VAU_MACW_BASE<TRF_ACC128, VRF128,  !strconcat(asmstr, ".u16"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _i8     : VAU_MACW_BASE<TRF_ACC128, VRF128,  !strconcat(asmstr, ".i8"),  [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _u8     : VAU_MACW_BASE<TRF_ACC128, VRF128,  !strconcat(asmstr, ".u8"),  [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;

  let IsPipelinedACCBit = 1 in {
       def _i32_pl : VAU_MACW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i32"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _u32_pl : VAU_MACW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u32"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _i16_pl : VAU_MACW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i16"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _u16_pl : VAU_MACW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u16"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _i8_pl  : VAU_MACW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".i8"),  [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _u8_pl  : VAU_MACW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".u8"),  [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
  }
}

multiclass VAU_MACW_TYPE_FLOAT<string asmstr> {
  let Itinerary = II_VAU_MACW_FP_Pipe, IsPipelinedACCBit = 1, Predicates = [HasVRF128_Predicate] in {
    def _f32_pl : VAU_MACW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".f32"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Float]>;
    def _f16_pl : VAU_MACW_BASE<TRF_ACC128, VRF128, !strconcat(asmstr, ".f16"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Float]>;
  }
}

let Itinerary = II_VAU_MACW_INT in {
  let Predicates = [HasVRF128_Predicate] in {
    defm VAU_MACNW  : VAU_MACW_TYPE<".MACNW">;
  }
}

  defm VAU_MACNW : VAU_MACW_TYPE_FLOAT<".MACNW">;

//
// 1.4.8.24 - VAU.MACPW
//   Scalar multiply-add accumulate with write to IRF.
//

let Itinerary = II_VAU_MACW_INT in {
  let Predicates = [HasVRF128_Predicate] in {
    defm VAU_MACPW  : VAU_MACW_TYPE<".MACPW">;
  }
}

  defm VAU_MACPW : VAU_MACW_TYPE_FLOAT<".MACPW">;


//
// 1.4.9.18 - VAU.MUL
//   Long vector multiplication with optional immediate operand.
//

let Itinerary = II_VAU_ADD_SUB_MUL_RRR_VRF, Defs = [I_AE] in
{
  // Integer variants
  def VAU_MUL_i32   : VAU_RRROpC<mul,  VRF128,  v4i32, ".MUL.i32", [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_i16   : VAU_RRROpC<mul,  VRF128,  v8i16, ".MUL.i16", [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_i8    : VAU_RRROpC<mul,  VRF128,  v16i8, ".MUL.i8",  [isSAU_MULorVAU_MUL]>;

  def VAU_MUL_v8i8  : VAU_RRROpC<mul,  VRF64_l, v8i8,  ".MUL.i8",  [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_v4i16 : VAU_RRROpC<mul,  VRF64_l, v4i16, ".MUL.i16", [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_v2i32 : VAU_RRROpC<mul,  VRF64_l, v2i32, ".MUL.i32", [isSAU_MULorVAU_MUL]>;

  // Floating point variants
  def VAU_MUL_f16   : VAU_RRROpC<fmul, VRF128,  v8f16, ".MUL.f16", [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_f32   : VAU_RRROpC<fmul, VRF128,  v4f32, ".MUL.f32", [isSAU_MULorVAU_MUL]>;

  def VAU_MUL_v2f32 : VAU_RRROpC<fmul, VRF64_l, v2f32, ".MUL.f32", [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_v4f16 : VAU_RRROpC<fmul, VRF64_l, v4f16, ".MUL.f16", [isSAU_MULorVAU_MUL]>;
}

let Itinerary = II_VAU_ADD_SUB_MUL_RRI_VRF, Defs = [I_AE] in
{
  // Integer variants
  defm VAU_MUL        : VAU_RRIOp<mul,  ".MUL", simm5, [isSAU_MULorVAU_MUL]>;

  // Floating point variants
  def VAU_MUL_f32_imm : VAU_RRIOpC<fmul, VRF128, v4f32, f32, constf32, ".MUL.f32", FloatConst, [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_f16_imm : VAU_RRIOpC<fmul, VRF128, v8f16, f16, constf16, ".MUL.f16", HalfFloatConst, [isSAU_MULorVAU_MUL]>;
}

//
// 1.4.9.19 - VAU.NOT
//   Bitwise NOT.
//

// VAU.NOT
class VAU_NOT<RegisterClass regVT, ValueType vt> :
  SHAVE_VAUInstr<(outs regVT:$dst), (ins regVT:$src), ".NOT $dst $src",
                 [(set (vt regVT:$dst), (xor (vt regVT:$src), (vt immAllOnesV)))]>;

let Itinerary = II_VAU_NOT_VRF in
{
  def VAU_NOT     : VAU_RROpC_Raw<VRF128, ".NOT">;

  def VAU_NOT_i32 : VAU_NOT<VRF128, v4i32>;
  def VAU_NOT_i16 : VAU_NOT<VRF128, v8i16>;
  def VAU_NOT_i8  : VAU_NOT<VRF128, v16i8>;

  def VAU_NOT_v2i32 : VAU_NOT<VRF64_l, v2i32>;
  def VAU_NOT_v4i16 : VAU_NOT<VRF64_l, v4i16>;
  def VAU_NOT_v8i8  : VAU_NOT<VRF64_l, v8i8>;
}

//
// 1.4.9.20 - VAU.OR
//   Bitwise OR.
//

let Itinerary = II_VAU_AND_OR_XOR_VRF in
{
  defm VAU_OR  : VAU_BitwiseLogic<or,  ".OR">;
}

//
// 1.4.9.21 - VAU.ROL
//   Long vector rotate left with optional immediate rotate amount.
//

let Itinerary = II_VAU_SHL_RRR_VRF, Defs = [V_STATE] in
{
  defm VAU_ROL : VAU_LogicX<rotl, ".ROL">;
}

let Itinerary = II_VAU_SHL_RRI_VRF, Defs = [V_STATE] in
{
  defm VAU_ROL : VAU_LogicX_Imm<rotl, ".ROL", uimm5>;
}

//
// 1.4.9.22 - VAU.SHD
//   Long vector arithmetic shift right with set to zero on underflow. Optional immediate shift amount.
//

let Itinerary = II_VAU_SHR_RRR_VRF, Defs = [V_STATE] in
{
  defm VAU_SHD : VAU_LogicIs<shd, ".SHD">;
}

let Itinerary = II_VAU_SHR_RRI_VRF, Defs = [V_STATE] in
{
  defm VAU_SHD : VAU_LogicIs_Imm<shd, ".SHD", uimm5>;
}

//
// 1.4.9.23 - VAU.SHL
//   Long vector logical shift left with optional immediate shift amount.
//

let Itinerary = II_VAU_SHL_RRR_VRF, Defs = [V_STATE] in
{
  defm VAU_SHL : VAU_LogicX<shl,  ".SHL">;
}

let Itinerary = II_VAU_SHL_RRI_VRF, Defs = [V_STATE] in
{
  defm VAU_SHL : VAU_LogicX_Imm<shl,  ".SHL", uimm5>;
}

//
// 1.4.9.24 - VAU.SHR
//   Long vector logical/arithmetic shift right with optional immediate shift amount.
//

let Itinerary = II_VAU_SHR_RRR_VRF, Defs = [V_STATE] in
{
  defm VAU_SHR : VAU_LogicIs<sra, ".SHR">;
  defm VAU_SHR : VAU_LogicUs<srl, ".SHR">;
}

let Itinerary = II_VAU_SHR_RRI_VRF, Defs = [V_STATE] in
{
  defm VAU_SHR : VAU_LogicIs_Imm<sra, ".SHR", uimm5>;
  defm VAU_SHR : VAU_LogicUs_Imm<srl, ".SHR", uimm5>;
}

//
// 1.4.9.25 - VAU.SUB
//   Long vector subtraction with optional immediate operand.
//

// Integer variants
let Itinerary = II_VAU_ADD_SUB_RRR_VRF, Defs = [I_AE] in
{
  def VAU_SUB_i32   : VAU_RRROpC<sub, VRF128,  v4i32, ".SUB.i32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_SUB_i16   : VAU_RRROpC<sub, VRF128,  v8i16, ".SUB.i16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_SUB_i8    : VAU_RRROpC<sub, VRF128,  v16i8, ".SUB.i8",  [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;

  def VAU_SUB_v4i16 : VAU_RRROpC<sub, VRF64_l, v4i16, ".SUB.i16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_SUB_v8i8  : VAU_RRROpC<sub, VRF64_l, v8i8,  ".SUB.i8",  [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_SUB_v2i32 : VAU_RRROpC<sub, VRF64_l, v2i32, ".SUB.i32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_RRI_VRF, Defs = [I_AE] in
{
  defm VAU_SUB : VAU_RRIOp<sub, ".SUB", uimm5, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

// Floating point variants
let Itinerary = II_VAU_ADD_SUB_MUL_RRR_VRF, Defs = [I_AE] in
{
  def VAU_SUB_f32     : VAU_RRROpC<fsub, VRF128,  v4f32, ".SUB.f32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  def VAU_SUB_f16     : VAU_RRROpC<fsub, VRF128,  v8f16, ".SUB.f16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;

  def VAU_SUB_v2f32   : VAU_RRROpC<fsub, VRF64_l, v2f32, ".SUB.f32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  def VAU_SUB_v4f16   : VAU_RRROpC<fsub, VRF64_l, v4f16, ".SUB.f16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_MUL_RRI_VRF, Defs = [I_AE] in
{
  def VAU_SUB_f32_imm : VAU_RRIOpC<fsub, VRF128, v4f32, f32, constf32, ".SUB.f32", FloatConst,     [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  def VAU_SUB_f16_imm : VAU_RRIOpC<fsub, VRF128, v8f16, f16, constf16, ".SUB.f16", HalfFloatConst, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
}

//
// 1.4.9.26 - VAU.SUBCI
//   Long vector 32-bit signed integer subtraction with carry-in.
//

let Itinerary = II_VAU_ADDCI_SUBCI_VRF, Uses = [V_STATE], Defs = [V_STATE] in
{
  def VAU_SUBCI_i32 : VAU_RRROpC_Raw<VRF128, ".SUBCI", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

// Class for DP4 variants with register writeback
class VAU_DP4_Base<RegisterClass regVT, string asmstr, list <Register>useList, list <Register>defList, string accStr="", list <SHAVEConflictName>conflicts=[]>:
  SHAVE_VAUInstr<(outs regVT:$dst), (ins regVT:$src1, regVT:$src2),
                 !strconcat(asmstr, " $dst $src1 $src2 ", accStr), [], !listconcat([usesVAUAdder, usesVAUIntegerAdder], conflicts)>
{
  let Uses = !listconcat([V_STATE], useList);
  let Defs = !listconcat([V_STATE], defList);
}

let Itinerary = II_VAU_DP4_NONACC_Myr2v6 in
{
  def VAU_DP4_v16i8  : VAU_DP4_Base<VRF128, ".DP4", [V_STATE], [V_STATE]>;
  def VAU_DP4M_v16i8 : VAU_DP4_Base<VRF128, ".DP4M", [V_STATE], [V_STATE]>;
}

// Class for DP4 variants with only V_ACC writes (no register writeback)
class VAU_DP4_Base_NoDest<RegisterClass regVT, string asmstr, list <Register>useList, list <Register>defList, string accStr="">:
  SHAVE_VAUInstr<(outs), (ins regVT:$src1, regVT:$src2),
                 !strconcat(asmstr, " $src1 $src2 ", accStr), [], [usesVAUAdder, usesVAUIntegerAdder, isDP4A]>
{
  let Uses = !listconcat([V_STATE], useList);
  let Defs = !listconcat([V_STATE], defList);
}

multiclass VAU_DP4_TYPE<string asmstr, list <Register>useList, list <Register>defList, string accStr = "">
{
  def ""    : VAU_DP4_Base<VRF128, asmstr, useList, defList, accStr, [isDP4A]>;
  def _void : VAU_DP4_Base_NoDest<VRF128, asmstr, useList, defList, accStr>;
}

let Itinerary = II_VAU_DP4_ACC_Myr2v6 in
{
  defm VAU_DP4A_v16i8_vacc0 : VAU_DP4_TYPE<".DP4A", [V_ACC0x], [V_ACC0x], "V_ACC0">;
  defm VAU_DP4AZ_v16i8_vacc0 : VAU_DP4_TYPE<".DP4AZ", [], [V_ACC0x], "V_ACC0">;

  defm VAU_DP4AM_v16i8_vacc0 : VAU_DP4_TYPE<".DP4AM", [V_ACC0x], [V_ACC0x], "V_ACC0">;
  defm VAU_DP4AMZ_v16i8_vacc0 : VAU_DP4_TYPE<".DP4AMZ", [], [V_ACC0x], "V_ACC0">;

  defm VAU_DP4A_v16i8_vacc1 : VAU_DP4_TYPE<".DP4A", [V_ACC1x], [V_ACC1x], "V_ACC1">;
  defm VAU_DP4AZ_v16i8_vacc1 : VAU_DP4_TYPE<".DP4AZ", [], [V_ACC1x], "V_ACC1">;

  defm VAU_DP4AM_v16i8_vacc1 : VAU_DP4_TYPE<".DP4AM", [V_ACC1x], [V_ACC1x], "V_ACC1">;
  defm VAU_DP4AMZ_v16i8_vacc1 : VAU_DP4_TYPE<".DP4AMZ", [], [V_ACC1x], "V_ACC1">;

}

//
// 1.4.9.27 - VAU.XOR
//   Bitwise XOR.
//

let Itinerary = II_VAU_AND_OR_XOR_VRF in
{
  defm VAU_XOR : VAU_BitwiseLogic<xor, ".XOR">;
}

// SHAVE ISA 2.6+ Transcendental functions
//
// 1.4.9.28 - VAU.TANH/SIGM/LOG/EXP/SQT
//   FP16 transcendental functions added in npu2.6
//

class VAU_ISA26_TRANSCENDENTAL<RegisterClass regVT, string asmstr> :
  SHAVE_VAUInstr<(outs regVT:$dst), (ins regVT:$src), !strconcat (asmstr, " $dst $src"),
                 []>;

let Itinerary = II_VAU_ISA26_TRANSCENDENTALS_VRF, Predicates = [HasVRF128_Predicate] in
{
  def VAU_TANH_v8f16 : VAU_ISA26_TRANSCENDENTAL<VRF128, ".TANH">;
  def VAU_SIGM_v8f16 : VAU_ISA26_TRANSCENDENTAL<VRF128, ".SIGM">;
  def VAU_LOG_v8f16  : VAU_ISA26_TRANSCENDENTAL<VRF128, ".LOG">;
  def VAU_EXP_v8f16  : VAU_ISA26_TRANSCENDENTAL<VRF128, ".EXP">;
  def VAU_SQT_v8f16  : VAU_ISA26_TRANSCENDENTAL<VRF128, ".SQT">;
}
} // HasVRF128_Predicate

include "SHAVEv3/SHAVEv3InstrInfo_VAU.td"
