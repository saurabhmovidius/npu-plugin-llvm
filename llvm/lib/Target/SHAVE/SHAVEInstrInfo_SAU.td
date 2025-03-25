//===-- SHAVEInstrInfo_SAU.td - SAU Instructions -----------*- tablegen -*-===//
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

//===----------------------------------------------------------------------===//
// SAU pseudo instructions
//===----------------------------------------------------------------------===//

//
// Pseudo instructions for SAU.ACC and SAU.ACCW variants
//
class SAU_ACC_SEQ_BASE<RegisterClass RC, list<SHAVEConflictName> confs = []> :
    SHAVE_Pseudo<(outs RC:$dst), (ins variable_ops), "", [], confs>{
    // TODO: make dtemp an ins argument to get the temporary reg instead of I-1
    // let Constraints = "@earlyclobber $dtemp";
}

// TODO: Need to free the I0 here and get a reg from regalloc
let Defs = [S_ACC, S_ACC0, S_ACC1] in {
  def SAU_ACCP_SEQ_i32 : SAU_ACC_SEQ_BASE<IRF32,   [isACCPSeq]>;
  def SAU_ACCP_SEQ_u32 : SAU_ACC_SEQ_BASE<IRF32,   [isACCPSeq]>;
  def SAU_ACCP_SEQ_i16 : SAU_ACC_SEQ_BASE<IRF16_l, [isACCPSeq]>;
  def SAU_ACCP_SEQ_u16 : SAU_ACC_SEQ_BASE<IRF16_l, [isACCPSeq]>;
  def SAU_ACCP_SEQ_i8  : SAU_ACC_SEQ_BASE<IRF8_q0, [isACCPSeq]>;
  def SAU_ACCP_SEQ_u8  : SAU_ACC_SEQ_BASE<IRF8_q0, [isACCPSeq]>;

  def SAU_ACCN_SEQ_i32 : SAU_ACC_SEQ_BASE<IRF32>;
  def SAU_ACCN_SEQ_u32 : SAU_ACC_SEQ_BASE<IRF32>;
  def SAU_ACCN_SEQ_i16 : SAU_ACC_SEQ_BASE<IRF16_l>;
  def SAU_ACCN_SEQ_u16 : SAU_ACC_SEQ_BASE<IRF16_l>;
  def SAU_ACCN_SEQ_i8  : SAU_ACC_SEQ_BASE<IRF8_q0>;
  def SAU_ACCN_SEQ_u8  : SAU_ACC_SEQ_BASE<IRF8_q0>;
}

let Defs = [S_ACC, S_ACC0, S_ACC1, I0] in {
  def SAU_ACCP_SEQ_f32 : SAU_ACC_SEQ_BASE<IRF32,   [isACCPSeq]>;
  def SAU_ACCP_SEQ_f16 : SAU_ACC_SEQ_BASE<IRF16_l, [isACCPSeq]>;

  def SAU_ACCN_SEQ_f32 : SAU_ACC_SEQ_BASE<IRF32>;
  def SAU_ACCN_SEQ_f16 : SAU_ACC_SEQ_BASE<IRF16_l>;
}

//
// Pseudo instructions for SAU.MAC and SAU.MACPW variants
//
class SAU_MAC_SEQ_BASE<RegisterClass RC, list<SHAVEConflictName> confs = []> :
    SHAVE_Pseudo<(outs RC:$dst), (ins RC:$src, variable_ops), "", [], confs>{
    // TODO: make dtemp an ins argument to get the temporary reg instead of I-1
    // let Constraints = "@earlyclobber $dtemp";
}

let Defs = [S_ACC, S_ACC0, S_ACC1] in {
  def SAU_MACP_SEQ_i32 : SAU_MAC_SEQ_BASE<IRF32,   [isMACPSeq]>;
  def SAU_MACP_SEQ_u32 : SAU_MAC_SEQ_BASE<IRF32,   [isMACPSeq]>;
  def SAU_MACP_SEQ_i16 : SAU_MAC_SEQ_BASE<IRF16_l, [isMACPSeq]>;
  def SAU_MACP_SEQ_u16 : SAU_MAC_SEQ_BASE<IRF16_l, [isMACPSeq]>;
  def SAU_MACP_SEQ_i8  : SAU_MAC_SEQ_BASE<IRF8_q0, [isMACPSeq]>;
  def SAU_MACP_SEQ_u8  : SAU_MAC_SEQ_BASE<IRF8_q0, [isMACPSeq]>;

  def SAU_MACN_SEQ_i32 : SAU_MAC_SEQ_BASE<IRF32>;
  def SAU_MACN_SEQ_u32 : SAU_MAC_SEQ_BASE<IRF32>;
  def SAU_MACN_SEQ_i16 : SAU_MAC_SEQ_BASE<IRF16_l>;
  def SAU_MACN_SEQ_u16 : SAU_MAC_SEQ_BASE<IRF16_l>;
  def SAU_MACN_SEQ_i8  : SAU_MAC_SEQ_BASE<IRF8_q0>;
  def SAU_MACN_SEQ_u8  : SAU_MAC_SEQ_BASE<IRF8_q0>;
}

let Defs = [S_ACC, S_ACC0, S_ACC1, I0] in {
  def SAU_MACP_SEQ_f32 : SAU_MAC_SEQ_BASE<IRF32,   [isMACPSeq]>;
  def SAU_MACP_SEQ_f16 : SAU_MAC_SEQ_BASE<IRF16_l, [isMACPSeq]>;

  def SAU_MACN_SEQ_f32 : SAU_MAC_SEQ_BASE<IRF32>;
  def SAU_MACN_SEQ_f16 : SAU_MAC_SEQ_BASE<IRF16_l>;
}

//===----------------------------------------------------------------------===//
// SAU generic class declarations
//===----------------------------------------------------------------------===//

//
// Reg <-- (Reg op Reg) classes
//

// base class for defining Reg <-- (Reg op Reg) type ops
class SAU_RRROpC_Base<RegisterClass regVT, string asmstr, list<dag> pattern, list<SHAVEConflictName> conflicts = []> :
  SHAVE_SAUInstr<(outs regVT:$dst), (ins regVT:$src1, regVT:$src2),
                 !strconcat(asmstr, " $dst $src1 $src2"), pattern, conflicts>;

// class for defining Reg <-- (Reg op Reg) type ops without a pattern
class SAU_RRROpC_Raw<RegisterClass regVT, string asmstr, list<SHAVEConflictName> conflicts = []> :
  SAU_RRROpC_Base<regVT, asmstr, [], conflicts>;

// class for defining Reg <-- (Reg op Reg) type ops
class SAU_RRROpC<SDNode opc, RegisterClass regVT, ValueType vt, string asmstr, list<SHAVEConflictName> conflicts = []> :
  SAU_RRROpC_Base<regVT, asmstr, [(set (vt regVT:$dst), (opc (vt regVT:$src1), (vt regVT:$src2)))], conflicts>;

//
// Reg <-- (Reg op Imm) classes
//

// base class for defining Reg <-- (Reg op Imm) type ops
class SAU_RRIOpC_Base<RegisterClass regVT, Operand immOp, string asmstr, list<dag> pattern, list<SHAVEConflictName> conflicts = []> :
  SHAVE_SAUInstr<(outs regVT:$dst), (ins regVT:$src, immOp:$imm),
                  !strconcat(asmstr, " $dst $src $imm"), pattern, conflicts>;

// class for defining Reg <-- (Reg op Imm) type ops without a pattern
class SAU_RRIOpC_Raw<RegisterClass regVT, Operand immOp, string asmstr, list<SHAVEConflictName> conflicts = []> :
  SAU_RRIOpC_Base<regVT, immOp, asmstr, [], conflicts>;

// class for defining Reg <-- (Reg op Imm) type scalar ops
class SAU_RRIOpC<SDNode opc, RegisterClass regVT, ValueType vt, Operand immOp, string asmstr, PatLeaf immPat, list<SHAVEConflictName> conflicts = []> :
  SAU_RRIOpC_Base<regVT, immOp, asmstr, [(set (vt regVT:$dst), (opc (vt regVT:$src), (vt immPat:$imm)))], conflicts>;

// class for defining Reg <-- (Reg op Imm) type vector ops
class SAU_RRIOpC_VOp<SDNode opc, RegisterClass regVT, ValueType vt, ValueType st, Operand immOp, string asmstr, PatLeaf immPat, list<SHAVEConflictName> conflicts = []> :
  SAU_RRIOpC_Base<regVT, immOp, asmstr, [(set (vt regVT:$dst), (opc regVT:$src, (vt (splat (st immPat:$imm)))))], conflicts>;

//
// Reg <-- (op Reg) classes
//

// base class for defining Reg <-- (op Reg) type ops
class SAU_RROpC_Base<RegisterClass srcRegVT, RegisterClass dstRegVT, string asmstr, list<dag> pattern, list<SHAVEConflictName> conflicts = []> :
  SHAVE_SAUInstr<(outs dstRegVT:$dst), (ins srcRegVT:$src),
                 !strconcat(asmstr, " $dst $src"), pattern, conflicts>;

// class for defining Reg <-- (op Reg) type ops without a pattern
class SAU_RROpC_Raw<RegisterClass srcRegVT, RegisterClass dstRegVT, string asmstr, list<SHAVEConflictName> conflicts = []> :
  SAU_RROpC_Base<srcRegVT, dstRegVT, asmstr, [], conflicts>;

// class for defining Reg <-- (op Reg) type ops
class SAU_RROpC<SDNode opc, RegisterClass regVT, ValueType vt, string asmstr, list<SHAVEConflictName> conflicts = []> :
  SAU_RROpC_Base<regVT, regVT, asmstr, [(set (vt regVT:$dst), (opc (vt regVT:$src)))], conflicts>;

//
// Reg <-- (op Reg) classes with a suffix on the src register
//

class SAU_RROpC_Suffix<SDNode opc, RegisterClass dstVT, RegisterClass srcVT, ValueType vt, string asmstr, string suffixstr, list<SHAVEConflictName> conflicts = []> :
   SHAVE_SAUInstr<(outs dstVT:$dst), (ins srcVT:$src),
                   !strconcat(asmstr, " $dst $src", suffixstr),
                   [(set (vt dstVT:$dst), (opc srcVT:$src))], conflicts>;

class SAU_RROpC_Suffix_NoPat<RegisterClass dstVT, RegisterClass srcVT, string asmstr, string suffixstr, list<SHAVEConflictName> conflicts = []> :
   SHAVE_SAUInstr<(outs dstVT:$dst), (ins srcVT:$src),
                  !strconcat(asmstr, " $dst $src", suffixstr),
                  [], conflicts>;

//
// Reg <-- (op Reg) classes for "horizontal" vector instructions
//

class SAU_XOp<SDNode opc, RegisterClass dstRC, RegisterClass srcRC, ValueType rt, ValueType vt, string asmstr> :
    SHAVE_SAUInstr<(outs dstRC:$dst), (ins srcRC:$src),
                   !strconcat(asmstr, " $dst $src"),
                   [(set (rt dstRC:$dst), (opc (vt srcRC:$src)))]>;

//
// IRF <-- (op VRF.column) class for math operations
//

class SAU_Math_Op_VRF<string asmstr> :
    SHAVE_SAUInstr<(outs IRF16_l:$dst), (ins VRF128:$src, i8imm:$col),
                   !strconcat(asmstr, " $dst $src.$col"), []>;

multiclass SAU_RRROp<SDNode opc, string asmstr, list<SHAVEConflictName> conflicts = []>
{
  // Scalar types
  def _i8  : SAU_RRROpC<opc, IRF8_q0, i8,  !strconcat(asmstr, ".i8"),  conflicts>;
  def _i16 : SAU_RRROpC<opc, IRF16_l, i16, !strconcat(asmstr, ".i16"), conflicts>;
  def _i32 : SAU_RRROpC<opc, IRF32,   i32, !strconcat(asmstr, ".i32"), conflicts>;

  // Vector types
  def _v4i8  : SAU_RRROpC<opc, IRF32,   v4i8,  !strconcat(asmstr, ".i8"),  conflicts>;
  def _v2i16 : SAU_RRROpC<opc, IRF32,   v2i16, !strconcat(asmstr, ".i16"), conflicts>;
  def _v2i8  : SAU_RRROpC<opc, IRF16_l, v2i8,  !strconcat(asmstr, ".i8"),  conflicts>;
}

multiclass SAU_RRIOp<SDNode opc, string asmstr, PatLeaf immPat, list<SHAVEConflictName> conflicts = []>
{
  // Scalar types
  // FIXME: Movidius - This should really use 'uimm5' or something along those lines
  def _i8_imm      : SAU_RRIOpC<opc, IRF8_q0, i8,  i8imm,  !strconcat(asmstr, ".i8"),  immPat, conflicts>;
  def _i16_imm     : SAU_RRIOpC<opc, IRF16_l, i16, i16imm, !strconcat(asmstr, ".i16"), immPat, conflicts>;
  def _i32_imm     : SAU_RRIOpC<opc, IRF32,   i32, i32imm, !strconcat(asmstr, ".i32"), immPat, conflicts>;

  // Vector types
  def _v4i8_imm    : SAU_RRIOpC_VOp<opc, IRF32,   v4i8,  i8,  i8imm,  !strconcat(asmstr, ".i8"),  immPat, conflicts>;
  def _v2i16_imm   : SAU_RRIOpC_VOp<opc, IRF32,   v2i16, i16, i16imm, !strconcat(asmstr, ".i16"), immPat, conflicts>;
  def _v2i8_imm    : SAU_RRIOpC_VOp<opc, IRF16_l, v2i8,  i8,  i8imm,  !strconcat(asmstr, ".i8"),  immPat, conflicts>;
}

multiclass SAU_IRF_RRROpf<SDNode opc, string asmstr, list<SHAVEConflictName> conflicts = []>
{
  def _f16   : SAU_RRROpC<opc,  IRF16_l, f16,   !strconcat(asmstr, ".f16"), conflicts>;
  def _f32   : SAU_RRROpC<opc,  IRF32,   f32,   !strconcat(asmstr, ".f32"), conflicts>;
  def _v2f16 : SAU_RRROpC<opc,  IRF32,   v2f16, !strconcat(asmstr, ".f16"), conflicts>;
}

// expand general SAU operations to type specific
multiclass SAU_XOp_8_16_32<SDNode opc, string asmstr>
{
  def _x8    : SAU_XOp<opc, IRF8_q0, VRF128, i8,  v16i8, !strconcat(asmstr, ".x8")>;
  def _x16   : SAU_XOp<opc, IRF16_l, VRF128, i16, v8i16, !strconcat(asmstr, ".x16")>;
  def _x32   : SAU_XOp<opc, IRF32,   VRF128, i32, v4i32, !strconcat(asmstr, ".x32")>;

  def _v4i8  : SAU_XOp<opc, IRF8_q0, IRF32,  i8,  v4i8,  !strconcat(asmstr, ".x8")>;
  def _v2i16 : SAU_XOp<opc, IRF16_l, IRF32,  i16, v2i16, !strconcat(asmstr, ".x16")>;
}

// Logic_x
multiclass SAU_Logic_x<SDNode op, string asmstr, list<SHAVEConflictName> conflicts = []>
{
  // Scalar types
  def _x32 : SAU_RRROpC<op, IRF32,   i32, !strconcat(asmstr, "32"), conflicts>;
  def _x16 : SAU_RRROpC<op, IRF16_l, i16, !strconcat(asmstr, "16"), conflicts>;
  def _x8  : SAU_RRROpC<op, IRF8_q0, i8,  !strconcat(asmstr, "8"),  conflicts>;

  // Vector types
  def _v4x8   : SAU_RRROpC<op, IRF32,   v4i8,  !strconcat(asmstr, "8"),  conflicts>;
  def _v2x16 : SAU_RRROpC<op, IRF32,   v2i16, !strconcat(asmstr, "16"), conflicts>;
  def _v2x8   : SAU_RRROpC<op, IRF16_l, v2i8,  !strconcat(asmstr, "8"),  conflicts>;
}

multiclass SAU_Logic_Imm_x<SDNode op, string asmstr, PatLeaf immPat, list<SHAVEConflictName> conflicts = []>
{
  // Scalar types
  def _x32_imm : SAU_RRIOpC<op, IRF32,   i32, i32imm, !strconcat(asmstr, "32"), immPat, conflicts>;
  def _x16_imm : SAU_RRIOpC<op, IRF16_l, i16, i16imm, !strconcat(asmstr, "16"), immPat, conflicts>;
  def _x8_imm  : SAU_RRIOpC<op, IRF8_q0, i8,  i8imm,  !strconcat(asmstr, "8"),  immPat, conflicts>;

  // Vector types
  def _v4x8_imm   : SAU_RRIOpC_VOp<op, IRF32,   v4i8,  i8,  i8imm,  !strconcat(asmstr, "8"),  immPat, conflicts>;
  def _v2x16_imm : SAU_RRIOpC_VOp<op, IRF32,   v2i16, i16, i16imm, !strconcat(asmstr, "16"), immPat, conflicts>;
  def _v2x8_imm   : SAU_RRIOpC_VOp<op, IRF16_l, v2i8,  i8,  i8imm,  !strconcat(asmstr, "8"),  immPat, conflicts>;
}

// Logic_u
multiclass SAU_Logic_u<SDNode op, string asmstr, list<SHAVEConflictName> conflicts>
{
  // Scalar types
  def _u32 : SAU_RRROpC<op, IRF32,   i32, !strconcat(asmstr, "32"), conflicts>;
  def _u16 : SAU_RRROpC<op, IRF16_l, i16, !strconcat(asmstr, "16"), conflicts>;
  def _u8  : SAU_RRROpC<op, IRF8_q0, i8,  !strconcat(asmstr, "8"),  conflicts>;

  // Vector types
  def _v4u8   : SAU_RRROpC<op, IRF32,   v4i8,  !strconcat(asmstr, "8"),  conflicts>;
  def _v2u16 : SAU_RRROpC<op, IRF32,   v2i16, !strconcat(asmstr, "16"), conflicts>;
  def _v2u8   : SAU_RRROpC<op, IRF16_l, v2i8,  !strconcat(asmstr, "8"),  conflicts>;
}

multiclass SAU_Logic_Imm_u<SDNode op, string asmstr, PatLeaf immPat, list<SHAVEConflictName> conflicts>
{
  // Scalar types
  def _u32_imm : SAU_RRIOpC<op, IRF32,   i32, i32imm, !strconcat(asmstr, "32"), immPat, conflicts>;
  def _u16_imm : SAU_RRIOpC<op, IRF16_l, i16, i16imm, !strconcat(asmstr, "16"), immPat, conflicts>;
  def _u8_imm  : SAU_RRIOpC<op, IRF8_q0, i8,  i8imm,  !strconcat(asmstr, "8"),  immPat, conflicts>;

  // Vector types
  def _v4u8_imm   : SAU_RRIOpC_VOp<op, IRF32,   v4i8,  i8,  i8imm,  !strconcat(asmstr, "8"),  immPat, conflicts>;
  def _v2u16_imm  : SAU_RRIOpC_VOp<op, IRF32,   v2i16, i16, i16imm, !strconcat(asmstr, "16"), immPat, conflicts>;
  def _v2u8_imm   : SAU_RRIOpC_VOp<op, IRF16_l, v2i8,  i8,  i8imm,  !strconcat(asmstr, "8"),  immPat, conflicts>;
}

// Logic_i
multiclass SAU_Logic_i<SDNode op, string asmstr, list<SHAVEConflictName> conflicts>
{
  // Scalar types
  def _i32 : SAU_RRROpC<op, IRF32,   i32, !strconcat(asmstr, "32"), conflicts>;
  def _i16 : SAU_RRROpC<op, IRF16_l, i16, !strconcat(asmstr, "16"), conflicts>;
  def _i8  : SAU_RRROpC<op, IRF8_q0, i8,  !strconcat(asmstr, "8"),  conflicts>;

  // Vector types
  def _v4i8  : SAU_RRROpC<op, IRF32,   v4i8,  !strconcat(asmstr, "8"),  conflicts>;
  def _v2i16 : SAU_RRROpC<op, IRF32,   v2i16, !strconcat(asmstr, "16"), conflicts>;
  def _v2i8  : SAU_RRROpC<op, IRF16_l, v2i8,  !strconcat(asmstr, "8"),  conflicts>;
}

multiclass SAU_Logic_Imm_i<SDNode op, string asmstr, PatLeaf immPat, list<SHAVEConflictName> conflicts = []>
{
  // Scalar types
  def _i32_imm : SAU_RRIOpC<op, IRF32,   i32, i32imm, !strconcat(asmstr, "32"), immPat, conflicts>;
  def _i16_imm : SAU_RRIOpC<op, IRF16_l, i16, i16imm, !strconcat(asmstr, "16"), immPat, conflicts>;
  def _i8_imm  : SAU_RRIOpC<op, IRF8_q0, i8,  i8imm,  !strconcat(asmstr, "8"),  immPat, conflicts>;

  // Vector types
  def _v4i8_imm   : SAU_RRIOpC_VOp<op, IRF32,   v4i8,  i8,  i8imm,  !strconcat(asmstr, "8"),  immPat, conflicts>;
  def _v2i16_imm  : SAU_RRIOpC_VOp<op, IRF32,   v2i16, i16, i16imm, !strconcat(asmstr, "16"), immPat, conflicts>;
  def _v2i8_imm   : SAU_RRIOpC_VOp<op, IRF16_l, v2i8,  i8,  i8imm,  !strconcat(asmstr, "8"),  immPat, conflicts>;
}

multiclass SAU_BitwiseLogic<SDNode op, string asmstr, list<SHAVEConflictName> conflicts = []>
{
  def _i32   : SAU_RRROpC<op, IRF32,   i32,   asmstr, conflicts>;
  def _i16   : SAU_RRROpC<op, IRF16_l, i16,   asmstr, conflicts>;
  def _i8    : SAU_RRROpC<op, IRF8_q0, i8,    asmstr, conflicts>;

  def _v2i16 : SAU_RRROpC<op, IRF32,   v2i16, asmstr, conflicts>;
  def _v4i8  : SAU_RRROpC<op, IRF32,   v4i8,  asmstr, conflicts>;
  def _v2i8  : SAU_RRROpC<op, IRF16_l, v2i8,  asmstr, conflicts>;
}

//===----------------------------------------------------------------------===//
// SAU instruction definitions (Section 1.4.8 of Fragrak SHAVE Reference Guide)
//===----------------------------------------------------------------------===//

//
// 1.4.8.1 - SAU.ABS
//   Short vector/scalar absolute value.
//

class SAU_ABS_Int<RegisterClass regVT, ValueType vt, string asmstr, int sraVal>:
   SHAVE_SAUInstr<(outs regVT:$dst), (ins regVT:$src), !strconcat(asmstr, " $dst $src"),
                    [(set (vt regVT:$dst), (xor (add regVT:$src, (sra regVT:$src, (i32 sraVal))), (sra regVT:$src, (i32 sraVal))))],
                    [usesVAU]>;

let Itinerary = II_SAU_ABS_IRF in
{ // FIXME: Movidius - LLVM does not appear to have an 'abs' pattern for integers!
  def SAU_ABS_i32        : SAU_ABS_Int<IRF32,     i32,   ".ABS.i32", +31>;
  def SAU_ABS_i16        : SAU_ABS_Int<IRF16_l,   i16,   ".ABS.i16", +15>;
  def SAU_ABS_i8         : SAU_ABS_Int<IRF8_q0,    i8,   ".ABS.i8",   +7>;

  def SAU_ABS_f32   : SAU_RROpC<fabs, IRF32,   f32,   ".ABS.f32", [usesVAU]>;
  def SAU_ABS_f16   : SAU_RROpC<fabs, IRF16_l, f16,   ".ABS.f16", [usesVAU]>;
  def SAU_ABS_v2f16 : SAU_RROpC<fabs, IRF32,   v2f16, ".ABS.f16", [usesVAU]>;
}

//
//===----------------------------------------------------------------------===//
//
// 1.4.8.2 - SAU.ACC
//    Short vector/scalar add/subtract accumulate with optional zeroing of the accumulator.
//
class SAU_ACC_BASE<RegisterClass accRC, RegisterClass RC, string asmstr, list<SHAVEConflictName> conflicts = []> :
    SHAVE_SAUInstr<(outs accRC:$dst_acc_reg), (ins RC:$src1, accRC:$src_acc_reg),
               !strconcat(asmstr, " $src1 $src_acc_reg"), [], !listconcat(conflicts, [usesVAU, isACC])> {
let Constraints = "$dst_acc_reg = $src_acc_reg";
}

// Integer accumulator patterns
multiclass SAU_ACC_TYPE<string asmstr> {
  def _i32    : SAU_ACC_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".i32"), [isACC_MAC_Int, usesSAUIntegerAdder]>;
  def _u32    : SAU_ACC_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".u32"), [isACC_MAC_Int, usesSAUIntegerAdder]>;
  def _i16    : SAU_ACC_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".i16"), [isACC_MAC_Int, usesSAUIntegerAdder]>;
  def _u16    : SAU_ACC_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".u16"), [isACC_MAC_Int, usesSAUIntegerAdder]>;
  def _i8     : SAU_ACC_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".i8"),  [isACC_MAC_Int, usesSAUIntegerAdder]>;
  def _u8     : SAU_ACC_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".u8"),  [isACC_MAC_Int, usesSAUIntegerAdder]>;
  let IsPipelinedACCBit = 1 in {
     def _i32_pl : SAU_ACC_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".i32"), [isPipelinedACCorMAC, isACC_MAC_Int, usesSAUIntegerAdder]>;
     def _u32_pl : SAU_ACC_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".u32"), [isPipelinedACCorMAC, isACC_MAC_Int, usesSAUIntegerAdder]>;
     def _i16_pl : SAU_ACC_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".i16"), [isPipelinedACCorMAC, isACC_MAC_Int, usesSAUIntegerAdder]>;
     def _u16_pl : SAU_ACC_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".u16"), [isPipelinedACCorMAC, isACC_MAC_Int, usesSAUIntegerAdder]>;
     def _i8_pl  : SAU_ACC_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".i8"),  [isPipelinedACCorMAC, isACC_MAC_Int, usesSAUIntegerAdder]>;
     def _u8_pl  : SAU_ACC_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".u8"),  [isPipelinedACCorMAC, isACC_MAC_Int, usesSAUIntegerAdder]>;
  }
}

let Itinerary = II_SAU_ACC_INT, ACC_MAC_InsnType = ACC_MAC_Z in {
  defm SAU_ACCPZ : SAU_ACC_TYPE<".ACCPZ">;
  defm SAU_ACCNZ : SAU_ACC_TYPE<".ACCNZ">;

}

let Itinerary = II_SAU_ACC_INT, ACC_MAC_InsnType = ACC_MAC_PN in {
  defm SAU_ACCP  : SAU_ACC_TYPE<".ACCP">;
  defm SAU_ACCN  : SAU_ACC_TYPE<".ACCN">;
}

// Floating-point accumulator patterns
multiclass SAU_ACC_TYPE_FLOAT<string asmstr> {
  let Itinerary = II_SAU_ACC_FP_Pipe, IsPipelinedACCBit = 1  in {
    def _f32_pl : SAU_ACC_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".f32"), [isPipelinedACCorMAC, isACC_MAC_Float, usesSAUFloatingPointAdder]>;
    def _f16_pl : SAU_ACC_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".f16"), [isPipelinedACCorMAC, isACC_MAC_Float, usesSAUFloatingPointAdder]>;
  }
}
  let ACC_MAC_InsnType = ACC_MAC_Z in {
     defm  SAU_ACCPZ : SAU_ACC_TYPE_FLOAT<".ACCPZ">;
     defm  SAU_ACCNZ : SAU_ACC_TYPE_FLOAT<".ACCNZ">;
   }

  let ACC_MAC_InsnType = ACC_MAC_PN in {
     defm  SAU_ACCP : SAU_ACC_TYPE_FLOAT<".ACCP">;
     defm  SAU_ACCN : SAU_ACC_TYPE_FLOAT<".ACCN">;
   }
//
// 1.4.8.3 - SAU.ACCW
//   Short vector/scalar add/subtract accumulate with write to IRF.
//

class SAU_ACCW_BASE<RegisterClass accRC, RegisterClass RC, string asmstr, list<SHAVEConflictName> conflicts = []> :
    SHAVE_SAUInstr<(outs RC:$dst, accRC:$dst_acc_reg), (ins RC:$src1, accRC:$src_acc_reg),
                 !strconcat(asmstr, " $dst $src1 $src_acc_reg"), [], !listconcat(conflicts, [usesVAU, isACC, isACC_MAC_W])>{
    let Constraints = "$dst_acc_reg = $src_acc_reg";
    let ACC_MAC_InsnType = ACC_MAC_W;
}


multiclass SAU_ACCW_TYPE<string asmstr> {
  def _i32    : SAU_ACCW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".i32"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;
  def _i16    : SAU_ACCW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".i16"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;
  def _u16    : SAU_ACCW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".u16"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;
  def _i8     : SAU_ACCW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".i8"),  [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;
  def _u8     : SAU_ACCW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".u8"),  [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;
  def _u32    : SAU_ACCW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".u32"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;

  let IsPipelinedACCBit = 1 in {
     def _i32_pl : SAU_ACCW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".i32"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;
     def _u32_pl : SAU_ACCW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".u32"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;
     def _i16_pl : SAU_ACCW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".i16"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;
     def _u16_pl : SAU_ACCW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".u16"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;
     def _i8_pl  : SAU_ACCW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".i8"),  [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;
     def _u8_pl  : SAU_ACCW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".u8"),  [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesSAUIntegerAdder]>;
  }
}

multiclass SAU_ACCW_TYPE_FLOAT<string asmstr> {
  let Itinerary = II_SAU_ACCW_FP_Pipe, IsPipelinedACCBit = 1 in {
   def _f32_pl : SAU_ACCW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".f32"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Float, usesSAUFloatingPointAdder]>;
   def _f16_pl : SAU_ACCW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".f16"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Float, usesSAUFloatingPointAdder]>;
 }
}

let Itinerary = II_SAU_ACCW_INT in {
  defm SAU_ACCPW  : SAU_ACCW_TYPE<".ACCPW">;
  defm SAU_ACCNW  : SAU_ACCW_TYPE<".ACCNW">;
}

  defm SAU_ACCPW : SAU_ACCW_TYPE_FLOAT<".ACCPW">;
  defm SAU_ACCNW : SAU_ACCW_TYPE_FLOAT<".ACCNW">;

//
// 1.4.8.4 - SAU.ADD
//   Short vector/scalar addition with optional immediate operand.
//

// IRF variants
let Itinerary = II_SAU_ADD_SUB_IRF_WITH_WRITE_2_CONDITION, Defs = [S_STATE, C_CSI] in
{
  defm SAU_ADD : SAU_RRROp<add, ".ADD", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  defm SAU_ADD : SAU_RRIOp<add, ".ADD", uimm5, [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
}

let Itinerary = II_SAU_ADD_SUB_MUL_IRF_f16_f32_WITH_WRITE_2_CONDITION, Defs = [S_STATE, C_CSI] in
{
  defm SAU_ADD : SAU_IRF_RRROpf<fadd, ".ADD", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUFloatingPointAdder]>;
}

let Itinerary = II_SAU_ADD_SUB_MUL_IRF_f16_f32_IMM_WITH_WRITE_2_CONDITION, Defs = [S_STATE, C_CSI] in
{
  // Scalar types
  def SAU_ADD_f32_imm : SAU_RRIOpC<fadd, IRF32, f32, constf32, ".ADD.f32", FloatConst, [usesVAU, isSAUorVAU_ADDorSUB, usesSAUFloatingPointAdder]>;
  def SAU_ADD_f16_imm : SAU_RRIOpC<fadd, IRF16_l, f16, constf16, ".ADD.f16", HalfFloatConst, [usesVAU, isSAUorVAU_ADDorSUB, usesSAUFloatingPointAdder]>;
  def SAU_ADD_v2f16_imm : SAU_RRIOpC_VOp<fadd, IRF32, v2f16, f16, constf16, ".ADD.f16", HalfFloatConst, [usesVAU, isSAUorVAU_ADDorSUB, usesSAUFloatingPointAdder]>;
}

//
// 1.4.8.5 - SAU.ADDCI
//   32-bit integer scalar addition with carry-in.
//

let Itinerary = II_SAU_ADDCI_SUBCI_IRF, Defs = [S_STATE, C_CSI], Uses = [S_STATE] in
{
  def SAU_ADDCI_i32 : SAU_RRROpC_Raw<IRF32, ".ADDCI", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
}

//
// 1.4.8.6 - SAU.ADIFF
//   Short vector/scalar absolute difference.
//

class SAU_ADIFF_Class<RegisterClass RC, ValueType ty, string typestr, list<SHAVEConflictName> conflicts = []> :
                SHAVE_SAUInstr<(outs RC:$dst), (ins RC:$src1, RC:$src2),
                               !strconcat(".ADIFF.", typestr, " $dst $src1 $src2"),
                               [(set RC:$dst, (ty (fabs (ty (fsub RC:$src1, RC:$src2)))))], conflicts>;

class SAU_ADIFF_NoPat<RegisterClass RC, string typestr> :
                SHAVE_SAUInstr<(outs RC:$dst), (ins RC:$src1, RC:$src2),
                               !strconcat(".ADIFF.", typestr, " $dst $src1 $src2"),
                               [], [usesVAU, usesSAUIntegerAdder]>;

let Itinerary = II_SAU_ADIFF_IRF, Defs = [C_CSI] in
{
  def SAU_ADIFF_f16 : SAU_ADIFF_Class<IRF16_l, f16, "f16", [usesVAU, usesSAUFloatingPointAdder]>;
  def SAU_ADIFF_f32 : SAU_ADIFF_Class<IRF32,   f32, "f32", [usesVAU, usesSAUFloatingPointAdder]>;

  def SAU_ADIFF_i32 : SAU_ADIFF_NoPat<IRF32,   "i32">;
  def SAU_ADIFF_i16 : SAU_ADIFF_NoPat<IRF16_l, "i16">;
  def SAU_ADIFF_i8  : SAU_ADIFF_NoPat<IRF8_q0, "i8">;

  def SAU_ADIFF_u32 : SAU_ADIFF_NoPat<IRF32,   "u32">;
  def SAU_ADIFF_u16 : SAU_ADIFF_NoPat<IRF16_l, "u16">;
  def SAU_ADIFF_u8  : SAU_ADIFF_NoPat<IRF8_q0, "u8">;
}

//
// 1.4.8.7 - SAU.ALIGNBYTE
//   Align bytes within 32-bit word.
//

let Itinerary = II_SAU_ALIGNBYTE_IRF in
{
  def SAU_ALIGNBYTE_i32 : SHAVE_SAUInstr<(outs IRF32:$dst), (ins IRF32:$src1, IRF32:$src2, i8imm:$boff),
                                         ".ALIGNBYTE $dst $src1 $src2 $boff", [], [usesVAU]>;
}

//
// 1.4.8.8 - SAU.AND
//   Bitwise AND.
//

let Itinerary = II_SAU_AND_OR_XOR_IRF in
{
  defm SAU_AND : SAU_BitwiseLogic<and, ".AND", [usesVAU]>;
}

//
// 1.4.8.9 - SAU.ANDX
//   Element-wise AND vector elements.
//

let Itinerary = II_SAU_OR_AND_IRF_VRF in
{
  defm SAU_ANDX  : SAU_XOp_8_16_32<horizontal_and, ".ANDX" >;
}

//
// 1.4.8.10 - SAU.ATN
//   FP16 arc tangent.
//

// IRF variant
let Itinerary = II_SAU_MATH_IRF in
{
  def SAU_ATN_f16_l  : SAU_RROpC_Suffix_NoPat<IRF16_l, IRF16_l, ".ATN", ".0">;
}

//
// 1.4.8.11 - SAU.AVG
//   Short vector/scalar integer average.
//

class SAU_AVG<RegisterClass regVT, ValueType vt, string asmstr> :
  SHAVE_SAUInstr<(outs regVT:$dst), (ins regVT:$src1, regVT:$src2),
                 !strconcat(".AVG.", asmstr, " $dst $src1 $src2"),
                 [(set (vt regVT:$dst), (srl (add (add (vt regVT:$src1), (vt regVT:$src2)), (vt 1)), (vt 1)))],
                 [usesVAU]>;

let Itinerary = II_SAU_AVG_IRF in
{
  // Scalar types
  def SAU_AVG_x32 : SAU_AVG<IRF32,   i32, "x32">;
  def SAU_AVG_x16 : SAU_AVG<IRF16_l, i16, "x16">;
  def SAU_AVG_x8  : SAU_AVG<IRF8_q0, i8,  "x8">;

  // Vector types
  def SAU_AVG_x16_v2i16 : SAU_AVG<IRF32,   v2i16, "x16">;
  def SAU_AVG_x8_v4i8   : SAU_AVG<IRF32,   v4i8,  "x8">;
  def SAU_AVG_x8_v2i8   : SAU_AVG<IRF16_l, v2i8,  "x8">;
}

//
// 1.4.8.12 - SAU.CMB
//   32-bit word byte-wise combine.
//

// Immediate operands appear in the reverse order (left-to-right) compared to
// the generated ASM (right-to-left).
class SAU_Combine<RegisterClass regVT, string asmstr> :
    SHAVE_SAUInstr<(outs regVT:$dst), (ins regVT:$src1, regVT:$src2, ben:$en0, ben:$en1, ben:$en2, ben:$en3),
                   !strconcat(asmstr, " $dst $src2 [$en3$en2$en1$en0]"), [], [usesVAU]>
{
  let Constraints = "$src1 = $dst";
}

let Itinerary = II_SAU_CMB_IRF in
{
  def SAU_CMBBYTE_imm_IRF : SAU_Combine<IRF32, ".CMB">;
}

//
// 1.4.8.13 - SAU.COS
//   FP16 cosine.
//

// IRF variants
let Itinerary = II_SAU_MATH_IRF in
{
  def SAU_COS_f16_l : SAU_RROpC_Suffix<fcos, IRF16_l, IRF16_l, f16, ".COS", ".0">;
}

//
// 1.4.8.14 - SAU.DIV
//   Short vector/scalar division.
//

// IRF variants
let Itinerary = II_SAU_DIV16_IRF_WITH_WRITE_2_CONDITION, Defs = [I_AE, C_CSI] in
{
  def SAU_DIV_u16 : SAU_RRROpC<udiv, IRF16_l, i16, ".DIV.u16">;
  def SAU_DIV_i16 : SAU_RRROpC<sdiv, IRF16_l, i16, ".DIV.i16">;

  def SAU_DIV_u16_v2i16 : SAU_RRROpC<udiv, IRF32, v2i16, ".DIV.u16">;
  def SAU_DIV_i16_v2i16 : SAU_RRROpC<sdiv, IRF32, v2i16, ".DIV.i16">;
}

let Itinerary = II_SAU_DIV32_IRF_WITH_WRITE_2_CONDITION, Defs = [I_AE, C_CSI] in
{
  def SAU_DIV_u32 : SAU_RRROpC<udiv, IRF32,   i32, ".DIV.u32">;
  def SAU_DIV_i32 : SAU_RRROpC<sdiv, IRF32,   i32, ".DIV.i32", [isSAU_DIV_i32]>;
}

let Itinerary = II_SAU_DIV_f16, Defs = [C_CSI] in
{
  def SAU_DIV_f16 : SAU_RRROpC<fdiv, IRF16_l, f16, ".DIV.f16">;
  def SAU_DIV_v2f16 : SAU_RRROpC<fdiv, IRF32, v2f16, ".DIV.f16">;
}

let Itinerary = II_SAU_DIV_f32, Defs = [C_CSI] in
{
  def SAU_DIV_f32 : SAU_RRROpC<fdiv, IRF32, f32, ".DIV.f32", [isSAU_DIV_f32]>;
}

//
// 1.4.8.15 - SAU.EXP2
//   FP16 base-2 exponentiation.
//

// IRF variants
let Itinerary = II_SAU_MATH_IRF in
{
  def SAU_EXP2_f16_l : SAU_RROpC_Suffix<fexp2, IRF16_l, IRF16_l, f16, ".EXP2", ".0">;
}

// VRF variant
let Itinerary = II_SAU_MATH_VRF in
{
  def SAU_EXP2_f16_imm_vrf : SAU_Math_Op_VRF<".EXP2">;
}

//
// 1.4.8.16 - SAU.FRAC
//   Fractional part of floating-point number.
//

class SAU_FRAC<RegisterClass regVT, ValueType vt, string asmstr> :
   SHAVE_SAUInstr<(outs regVT:$dst), (ins regVT:$src),
                   !strconcat(asmstr, " $dst $src"),
                   [(set (vt regVT:$dst), (fsub regVT:$src, (ffloor regVT:$src)))]>;

// IRF variants
let Itinerary = II_SAU_FRAC_IRF in
{
  def SAU_FRAC_f32    : SAU_FRAC<IRF32,   f32,   ".FRAC.f32">;
  def SAU_FRAC_f16    : SAU_FRAC<IRF16_l, f16,   ".FRAC.f16">;
  def SAU_FRAC_v2f16  : SAU_FRAC<IRF32,   v2f16, ".FRAC.f16">;
}

//
// 1.4.8.17 - SAU.IADDS
//   Short vector/scalar integer addition with saturation. Optional unsigned immediate operand.
//

let Itinerary = II_SAU_ADD_RRR_IRF_WITH_1_CONDITION, Defs = [C_CSI] in
{
  def SAU_IADDS_i32 : SAU_RRROpC<add, IRF32,   i32, ".IADDS.i32", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  def SAU_IADDS_i16 : SAU_RRROpC<add, IRF16_l, i16, ".IADDS.i16", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  def SAU_IADDS_i8  : SAU_RRROpC<add, IRF8_q0, i8,  ".IADDS.i8",  [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  def SAU_IADDS_u32 : SAU_RRROpC<add, IRF32,   i32, ".IADDS.u32", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  def SAU_IADDS_u16 : SAU_RRROpC<add, IRF16_l, i16, ".IADDS.u16", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  def SAU_IADDS_u8  : SAU_RRROpC<add, IRF8_q0, i8,  ".IADDS.u8",  [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
}

//
// 1.4.8.18 - SAU.ILMULH
//   Short vector/scalar multiply returning higher-order result.
//

let Itinerary = II_SAU_ILMULH_RRR_IRF_WITH_1_CONDITION, Defs = [C_CSI] in
{
  def SAU_ILMULH_i32 : SAU_RRROpC_Raw<IRF32,   ".ILMULH.i32", [usesVAU]>;
  def SAU_ILMULH_i16 : SAU_RRROpC_Raw<IRF16_l, ".ILMULH.i16", [usesVAU]>;
  def SAU_ILMULH_i8  : SAU_RRROpC_Raw<IRF8_q0, ".ILMULH.i8",  [usesVAU]>;
  def SAU_ILMULH_u32 : SAU_RRROpC_Raw<IRF32,   ".ILMULH.u32", [usesVAU]>;
  def SAU_ILMULH_u16 : SAU_RRROpC_Raw<IRF16_l, ".ILMULH.u16", [usesVAU]>;
  def SAU_ILMULH_u8  : SAU_RRROpC_Raw<IRF8_q0, ".ILMULH.u8",  [usesVAU]>;
}

//
// 1.4.8.19 - SAU.IMULS
//   Short vector/scalar integer multiplication with saturation. Optional immediate operand.
//

let Itinerary = II_SAU_MUL_RRR_IRF_WITH_1_CONDITION, Defs = [C_CSI] in
{
  def SAU_IMULS_i32 : SAU_RRROpC<mul, IRF32,   i32, ".IMULS.i32", [usesVAU]>;
  def SAU_IMULS_i16 : SAU_RRROpC<mul, IRF16_l, i16, ".IMULS.i16", [usesVAU]>;
  def SAU_IMULS_i8  : SAU_RRROpC<mul, IRF8_q0, i8,  ".IMULS.i8",  [usesVAU]>;
  def SAU_IMULS_u32 : SAU_RRROpC<mul, IRF32,   i32, ".IMULS.u32", [usesVAU]>;
  def SAU_IMULS_u16 : SAU_RRROpC<mul, IRF16_l, i16, ".IMULS.u16", [usesVAU]>;
  def SAU_IMULS_u8  : SAU_RRROpC<mul, IRF8_q0, i8,  ".IMULS.u8",  [usesVAU]>;
}

//
// 1.4.8.20 - SAU.ISUBS
//   Short vector/scalar integer subtraction with saturation. Optional unsigned immediate operand.
//

let Itinerary = II_SAU_ADD_RRR_IRF_WITH_1_CONDITION, Defs = [C_CSI] in
{
  def SAU_ISUBS_i32 : SAU_RRROpC<sub, IRF32,   i32, ".ISUBS.i32", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  def SAU_ISUBS_i16 : SAU_RRROpC<sub, IRF16_l, i16, ".ISUBS.i16", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  def SAU_ISUBS_i8  : SAU_RRROpC<sub, IRF8_q0, i8,  ".ISUBS.i8",  [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  def SAU_ISUBS_u32 : SAU_RRROpC<sub, IRF32,   i32, ".ISUBS.u32", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  def SAU_ISUBS_u16 : SAU_RRROpC<sub, IRF16_l, i16, ".ISUBS.u16", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  def SAU_ISUBS_u8  : SAU_RRROpC<sub, IRF8_q0, i8,  ".ISUBS.u8",  [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
}

//
// 1.4.8.21 - SAU.LOG2
//   FP16 base-2 logarithm.
//

// IRF variants
let Itinerary = II_SAU_MATH_IRF in
{
  def SAU_LOG2_f16_l : SAU_RROpC_Suffix<flog2, IRF16_l, IRF16_l, f16, ".LOG2", ".0">;
}

//
// 1.4.8.22 - SAU.MAC
//   Scalar multiply-add/subtract accumulate with optional zeroing of the accumulator.
//

class SAU_MAC_BASE<RegisterClass accRC, RegisterClass RC, string asmstr, list<SHAVEConflictName> conflicts = []> :
    SHAVE_SAUInstr<(outs accRC:$dst_acc_reg), (ins RC:$src1, RC:$src2, accRC:$src_acc_reg),
               !strconcat(asmstr, " $src1 $src2 $src_acc_reg"), [], !listconcat(conflicts, [usesVAU, isMAC])> {
  let Constraints = "$dst_acc_reg = $src_acc_reg";
  let IsMACInsnBit = 1;
}

multiclass SAU_MAC_TYPE<string asmstr>{
  def _i32    : SAU_MAC_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".i32"), [isACC_MAC_Int]>;
  def _u32    : SAU_MAC_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".u32"), [isACC_MAC_Int]>;
  def _i16    : SAU_MAC_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".i16"), [isACC_MAC_Int]>;
  def _u16    : SAU_MAC_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".u16"), [isACC_MAC_Int]>;
  def _i8     : SAU_MAC_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".i8"),  [isACC_MAC_Int]>;
  def _u8     : SAU_MAC_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".u8"),  [isACC_MAC_Int]>;

  let IsPipelinedACCBit = 1 in {
       def _i32_pl : SAU_MAC_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".i32"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _u32_pl : SAU_MAC_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".u32"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _i16_pl : SAU_MAC_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".i16"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _u16_pl : SAU_MAC_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".u16"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _i8_pl  : SAU_MAC_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".i8"),  [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _u8_pl  : SAU_MAC_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".u8"),  [ isPipelinedACCorMAC, isACC_MAC_Int]>;
   }
}

let Itinerary = II_SAU_MAC_INT, ACC_MAC_InsnType = ACC_MAC_Z in {
  defm SAU_MACPZ : SAU_MAC_TYPE<".MACPZ">;
  defm SAU_MACNZ : SAU_MAC_TYPE<".MACNZ">;
}

let Itinerary = II_SAU_MAC_INT, ACC_MAC_InsnType = ACC_MAC_PN in {
  defm SAU_MACP  : SAU_MAC_TYPE<".MACP">;
  defm SAU_MACN  : SAU_MAC_TYPE<".MACN">;
}

// Floating-point MAC accumulator patterns
multiclass SAU_MAC_TYPE_FLOAT<string asmstr> {
  let Itinerary = II_SAU_MAC_FP_Pipe, IsPipelinedACCBit = 1 in {
    def _f32_pl : SAU_MAC_BASE<TRF_ACC32, IRF32, !strconcat(asmstr, ".f32"), [ isPipelinedACCorMAC, isACC_MAC_Float]>;
    def _f16_pl : SAU_MAC_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".f16"), [ isPipelinedACCorMAC, isACC_MAC_Float]>;
  }
}

let ACC_MAC_InsnType = ACC_MAC_Z in {
  defm SAU_MACPZ : SAU_MAC_TYPE_FLOAT<".MACPZ">;
  defm SAU_MACNZ : SAU_MAC_TYPE_FLOAT<".MACNZ">;
}

let ACC_MAC_InsnType = ACC_MAC_PN in {
  defm SAU_MACP : SAU_MAC_TYPE_FLOAT<".MACP">;
  defm SAU_MACN : SAU_MAC_TYPE_FLOAT<".MACN">;
}

//
// 1.4.8.23 - SAU.MACNW
//   Scalar multiply-subtract accumulate with write to IRF.
//

class SAU_MACW_BASE<RegisterClass accRC, RegisterClass RC, string asmstr, list<SHAVEConflictName> conflicts = []> :
    SHAVE_SAUInstr<(outs RC:$dst, accRC:$dst_acc_reg), (ins RC:$src1, RC:$src2, accRC:$src_acc_reg),
                !strconcat(asmstr, " $dst $src1 $src2 $src_acc_reg "), [], !listconcat(conflicts, [usesVAU, isMAC, isACC_MAC_W])> {
     let Constraints = "$dst_acc_reg = $src_acc_reg";
     let ACC_MAC_InsnType = ACC_MAC_W;
     let IsMACInsnBit = 1;
}

multiclass SAU_MACW_TYPE<string asmstr>
{
  def _i32    : SAU_MACW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".i32"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _u32    : SAU_MACW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".u32"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _i16    : SAU_MACW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".i16"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _u16    : SAU_MACW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".u16"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _i8     : SAU_MACW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".i8"),  [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _u8     : SAU_MACW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".u8"),  [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;

  let IsPipelinedACCBit = 1 in {
       def _i32_pl : SAU_MACW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".i32"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _u32_pl : SAU_MACW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".u32"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _i16_pl : SAU_MACW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".i16"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _u16_pl : SAU_MACW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".u16"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _i8_pl  : SAU_MACW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".i8"),  [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _u8_pl  : SAU_MACW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".u8"),  [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
  }
}



multiclass SAU_MACW_TYPE_FLOAT<string asmstr> {
  let Itinerary = II_SAU_MACW_FP_Pipe, IsPipelinedACCBit = 1 in {
    def _f32_pl : SAU_MACW_BASE<TRF_ACC32, IRF32, !strconcat(asmstr, ".f32"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Float]>;
    def _f16_pl : SAU_MACW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".f16"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Float]>;
  }
}

let Itinerary = II_SAU_MACW_INT in {
  defm SAU_MACNW  : SAU_MACW_TYPE<".MACNW">;
}

  defm SAU_MACNW : SAU_MACW_TYPE_FLOAT<".MACNW">;

//
// 1.4.8.24 - SAU.MACPW
//   Scalar multiply-add accumulate with write to IRF.
//

let Itinerary = II_SAU_MACW_INT in {
  defm SAU_MACPW  : SAU_MACW_TYPE<".MACPW">;
}

  defm SAU_MACPW : SAU_MACW_TYPE_FLOAT<".MACPW">;

//
// 1.4.8.25 - SAU.MOD
//   Short vector/scalar integer modulo. Optional unsigned immediate operand.
//

let Itinerary = II_SAU_MOD16_IRF_WITH_WRITE_2_CONDITION, Defs = [I_AE, C_CSI] in
{
  def SAU_MOD_u16 : SAU_RRROpC<urem, IRF16_l, i16, ".MOD.u16">;
  def SAU_MOD_i16 : SAU_RRROpC<srem, IRF16_l, i16, ".MOD.i16">;

  def SAU_MOD_v2u16 : SAU_RRROpC<urem, IRF32, v2i16, ".MOD.u16">;
  def SAU_MOD_v2i16 : SAU_RRROpC<srem, IRF32, v2i16, ".MOD.i16">;
}

let Itinerary = II_SAU_MOD32_IRF_WITH_WRITE_2_CONDITION, Defs = [I_AE, C_CSI] in
{
  def SAU_MOD_u32 : SAU_RRROpC<urem, IRF32,   i32, ".MOD.u32">;
  def SAU_MOD_i32 : SAU_RRROpC<srem, IRF32,   i32, ".MOD.i32", [isSAU_MOD]>;
}

//
// 1.4.8.26 - SAU.MUL
//   Short vector/scalar multiplication with optional immediate operand.
//

// IRF variants
let Itinerary = II_SAU_MUL_IRF_WITH_WRITE_2_CONDITION, Defs = [S_STATE, C_CSI] in
{
  defm SAU_MUL : SAU_RRROp<mul, ".MUL", [usesVAU, isSAU_MULorVAU_MUL]>;
  defm SAU_MUL : SAU_RRIOp<mul, ".MUL", uimm5, [usesVAU, isSAU_MULorVAU_MUL]>;
}

let Itinerary = II_SAU_ADD_SUB_MUL_IRF_f16_f32_WITH_WRITE_2_CONDITION, Defs = [S_STATE, C_CSI] in
{
  defm SAU_MUL : SAU_IRF_RRROpf<fmul, ".MUL", [usesVAU, isSAU_MULorVAU_MUL]>;
}

let Itinerary = II_SAU_ADD_SUB_MUL_IRF_f16_f32_IMM_WITH_WRITE_2_CONDITION, Defs = [S_STATE, C_CSI] in
{
  def SAU_MUL_f32_imm : SAU_RRIOpC<fmul, IRF32, f32, constf32, ".MUL.f32", FloatConst, [usesVAU, isSAU_MULorVAU_MUL]>;
  def SAU_MUL_f16_imm : SAU_RRIOpC<fmul, IRF16_l, f16, constf16, ".MUL.f16", HalfFloatConst, [usesVAU, isSAU_MULorVAU_MUL]>;

  def SAU_MUL_v2f16_imm : SAU_RRIOpC_VOp<fmul, IRF32, v2f16, f16, constf16, ".MUL.f16", HalfFloatConst, [usesVAU, isSAU_MULorVAU_MUL]>;
}

//
// 1.4.8.27 - SAU.NOT
//   Bitwise NOT.
//

class SAU_NOT<RegisterClass regVT, ValueType vt> :
  SHAVE_SAUInstr<(outs regVT:$dst), (ins regVT:$src),
                 ".NOT $dst $src",
                 [(set (vt regVT:$dst), (xor (vt regVT:$src), (vt -1)))],
                 [usesVAU]>;

let Itinerary = II_SAU_NOT_IRF in
{
  def SAU_NOT_i32 : SAU_NOT<IRF32,   i32>;
  def SAU_NOT_i16 : SAU_NOT<IRF16_l, i16>;
  def SAU_NOT_i8  : SAU_NOT<IRF8_q0, i8>;

  def SAU_NOT_v2i16 : SAU_NOT<IRF32,   v2i16>;
  def SAU_NOT_v4i8  : SAU_NOT<IRF32,   v4i8>;
  def SAU_NOT_v2i8  : SAU_NOT<IRF16_l, v2i8>;
}

//
// 1.4.8.28 - SAU.ONESX
//   Long vector ones or zeros count reduction.
//

let Itinerary = II_SAU_ONES_ZEROS_VRF, Predicates = [HasVRF128_Predicate] in
{
  def SAU_ONESX_x32   : SAU_RROpC_Raw<VRF128,  IRF32, ".ONESX.x32">;
  def SAU_ONESX_x64   : SAU_RROpC_Raw<VRF128,  IRF32, ".ONESX.x64">;
  def SAU_ONESX_x128  : SAU_RROpC_Raw<VRF128,  IRF32, ".ONESX.x128">;

  def SAU_ZEROSX_x32  : SAU_RROpC_Raw<VRF128,  IRF32, ".ZEROSX.x32">;
  def SAU_ZEROSX_x64  : SAU_RROpC_Raw<VRF128,  IRF32, ".ZEROSX.x64">;
  def SAU_ZEROSX_x128 : SAU_RROpC_Raw<VRF128,  IRF32, ".ZEROSX.x128">;
}

//
// 1.4.8.29 - SAU.OR
//   Bitwise OR.
//

let Itinerary = II_SAU_AND_OR_XOR_IRF in
{
  defm SAU_OR  : SAU_BitwiseLogic<or,  ".OR", [usesVAU]>;
}

//
// 1.4.8.30 - SAU.ORX
//   Element-wise OR of vector.
//

let Itinerary = II_SAU_OR_AND_IRF_VRF in
{
  defm SAU_ORX   : SAU_XOp_8_16_32<horizontal_or,  ".ORX" >;
}

//
// 1.4.8.31 - SAU.RCP
//   FP16 reciprocal.
//

// IRF variants
let Itinerary = II_SAU_MATH_IRF in
{
  def SAU_RCP_f16_l : SHAVE_SAUInstr<(outs IRF16_l:$dst), (ins IRF16_l:$src),
                               ".RCP $dst $src.0",
                               [(set IRF16_l:$dst, (fdiv One_half, IRF16_l:$src))]>;
}

//
// 1.4.8.32 - SAU.ROL
//   Scalar rotate left with optional immediate rotate amount.
//

let Itinerary = II_SAU_SHL_RRR_IRF, Defs = [C_CSI] in
{
  defm SAU_ROL : SAU_Logic_x<rotl, ".ROL.x", [usesVAU]>;
}

let Itinerary = II_SAU_SHL_RRI_IRF, Defs = [C_CSI] in
{
  defm SAU_ROL : SAU_Logic_Imm_x<rotl, ".ROL.x", uimm5, [usesVAU]>;
}

//
// 1.4.8.33 - SAU.RQT
//   FP16 reciprocal square root.
//

// IRF variants
let Itinerary = II_SAU_MATH_IRF in
{
  def SAU_RQT_f16_l : SHAVE_SAUInstr<(outs IRF16_l:$dst), (ins IRF16_l:$src), ".RQT $dst $src.0",
                                          [(set IRF16_l:$dst, (fdiv One_half, (fsqrt IRF16_l:$src)))]>;
}

//
// 1.4.8.34 - SAU.SHD
//   Short vector/scalar arithmetic shift right with set to zero on underflow. Optional immediate shift amount.
//

let Itinerary = II_SAU_SHR_RRR_IRF, Defs = [S_STATE] in
{
  defm SAU_SHD  : SAU_Logic_u<shd,  ".SHD.i", [usesVAU]>;
}

let Itinerary = II_SAU_SHR_RRI_IRF, Defs = [S_STATE] in
{
  defm SAU_SHD : SAU_Logic_Imm_i<shd, ".SHD.i", uimm5, [usesVAU]>;
}

//
// 1.4.8.35 - SAU.SHL
//   Short vector/scalar logical shift left with optional immediate shift amount.
//

let Itinerary = II_SAU_SHL_RRR_IRF, Defs = [C_CSI] in
{
  defm SAU_SHL : SAU_Logic_x<shl,  ".SHL.x", [usesVAU]>;
}

let Itinerary = II_SAU_SHL_RRI_IRF, Defs = [C_CSI] in
{
  defm SAU_SHL : SAU_Logic_Imm_x<shl,  ".SHL.x", uimm5, [usesVAU]>;
}

//
// 1.4.8.36 - SAU.SHR
//   Short vector/scalar logical/arithmetic shift right with optional immediate shift amount.
//

let Itinerary = II_SAU_SHR_RRR_IRF, Defs = [S_STATE] in
{
  defm SAU_SHR  : SAU_Logic_i<sra,  ".SHR.i", [usesVAU]>;
  defm SAU_SHR  : SAU_Logic_u<srl,  ".SHR.u", [usesVAU]>;
}

let Itinerary = II_SAU_SHR_RRI_IRF, Defs = [S_STATE] in
{
  defm SAU_SHR : SAU_Logic_Imm_i<sra, ".SHR.i", uimm5, [usesVAU]>;
  defm SAU_SHR : SAU_Logic_Imm_u<srl, ".SHR.u", uimm5, [usesVAU]>;
}

//
// 1.4.8.37 - SAU.SIN
//   FP16 sine.
//

// IRF variants
let Itinerary = II_SAU_MATH_IRF in
{
  def SAU_SIN_f16_l  : SAU_RROpC_Suffix<fsin, IRF16_l, IRF16_l, f16, ".SIN", ".0">;
}

//
// 1.4.8.38 - SAU.SQT
//   FP16 square root.
//

// IRF variants
let Itinerary = II_SAU_MATH_IRF in
{
  def SAU_SQT_f16_l  : SAU_RROpC_Suffix<fsqrt, IRF16_l, IRF16_l, f16, ".SQT", ".0">;
}

let Itinerary = II_SAU_MATH_VRF in
{
  def SAU_SQT_f16_imm_vrf : SAU_Math_Op_VRF<".SQT">;
}

//
// 1.4.8.39 - SAU.SUB
//   Short vector/scalar subtraction with optional immediate operand.
//

// IRF variants
let Itinerary = II_SAU_ADD_SUB_IRF_WITH_WRITE_2_CONDITION, Defs = [S_STATE, C_CSI] in
{
  defm SAU_SUB : SAU_RRROp<sub, ".SUB", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
  defm SAU_SUB : SAU_RRIOp<sub, ".SUB", uimm5, [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
}

let Itinerary = II_SAU_ADD_SUB_MUL_IRF_f16_f32_WITH_WRITE_2_CONDITION, Defs = [S_STATE, C_CSI] in
{
  defm SAU_SUB : SAU_IRF_RRROpf<fsub, ".SUB", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUFloatingPointAdder]>;
}

let Itinerary = II_SAU_ADD_SUB_MUL_IRF_f16_f32_IMM_WITH_WRITE_2_CONDITION, Defs = [S_STATE, C_CSI] in
{
  def SAU_SUB_f32_imm : SAU_RRIOpC<fsub, IRF32, f32, constf32, ".SUB.f32", FloatConst, [usesVAU, isSAUorVAU_ADDorSUB, usesSAUFloatingPointAdder]>;
  def SAU_SUB_f16_imm : SAU_RRIOpC<fsub, IRF16_l, f16, constf16, ".SUB.f16", HalfFloatConst, [usesVAU, isSAUorVAU_ADDorSUB, usesSAUFloatingPointAdder]>;

  def SAU_SUB_v2f16_imm : SAU_RRIOpC_VOp<fsub, IRF32, v2f16, f16, constf16, ".SUB.f16", HalfFloatConst, [usesVAU, isSAUorVAU_ADDorSUB, usesSAUFloatingPointAdder]>;
}

//
// 1.4.8.40 - SAU.SUBCI
//   32-bit signed integer scalar subtraction with carry-in.
//

let Itinerary = II_SAU_ADDCI_SUBCI_IRF, Defs = [S_STATE, C_CSI], Uses = [S_STATE] in
{
  def SAU_SUBCI_i32 : SAU_RRROpC_Raw<IRF32, ".SUBCI", [usesVAU, isSAUorVAU_ADDorSUB, usesSAUIntegerAdder]>;
}

//
// 1.4.8.41 - SAU.SUMC
//   Sum bytes within 32-bit words.
//

let Itinerary = II_SAU_SUMC_IRF_VRF in
{
  def SAU_SUMC : SHAVE_SAUInstr<(outs IRF32:$dst), (ins VRF128:$src),
                                    ".SUMC $dst $src", []>;
}

//
// 1.4.8.42 - SAU.SUMX
//   Sum vector elements to a scalar.
//

let Predicates = [HasVRF128_Predicate] in {

  class SAU_SUMX<RegisterClass srcRC, ValueType srcVT, string asmstr> :
    SHAVE_SAUInstr<(outs IRF32:$dst), (ins srcRC:$src),
                   !strconcat(".SUMX.", asmstr, " $dst $src"),
                   [(set (i32 IRF32:$dst), (horizontal_add (srcVT srcRC:$src)))]>;

  // Integer variants
  let Itinerary = II_SAU_SUM_IRF_VRF_INT in
  {
    def SAU_SUMX_i8    : SAU_SUMX<VRF128, v16i8, "i8">;
    def SAU_SUMX_i16   : SAU_SUMX<VRF128, v8i16, "i16">;
    def SAU_SUMX_i32   : SAU_SUMX<VRF128, v4i32, "i32">;

    def SAU_SUMX_u8    : SHAVE_SAUInstr<(outs IRF32:$dst), (ins VRF128:$src), ".SUMX.u8  $dst $src", []>;
    def SAU_SUMX_u16   : SHAVE_SAUInstr<(outs IRF32:$dst), (ins VRF128:$src), ".SUMX.u16 $dst $src", []>;
    def SAU_SUMX_u32   : SHAVE_SAUInstr<(outs IRF32:$dst), (ins VRF128:$src), ".SUMX.u32 $dst $src", []>;
  }

  let Itinerary = II_SAU_SUM_IRF_IRF_INT in
  {
    def SAU_SUMX_v2i16 : SAU_SUMX<IRF32, v2i16, "i16">;
    def SAU_SUMX_v4i8  : SAU_SUMX<IRF32, v4i8,  "i8">;

    def SAU_SUMX_v4u8  : SHAVE_SAUInstr<(outs IRF32:$dst), (ins IRF32:$src),  ".SUMX.u8  $dst $src", []>;
  }

  // Floating-point variants

  let Itinerary = II_SAU_SUM_IRF_VRF_FP in
  {
    def SAU_SUMX_f16   : SAU_XOp<horizontal_fadd, IRF16_l, VRF128, f16,  v8f16, ".SUMX.f16">;
    def SAU_SUMX_f32   : SAU_XOp<horizontal_fadd, IRF32,   VRF128, f32,  v4f32, ".SUMX.f32">;
  }

  let Itinerary = II_SAU_SUM_IRF_IRF_FP in
  {
    def SAU_SUMX_v2f16 : SAU_XOp<horizontal_fadd, IRF16_l, IRF32, f16, v2f16, ".SUMX.f16">;
  }

  def : Pat<(f16 (vecreduce_fadd (v8f16 VRF128:$source))),
            (SAU_SUMX_f16 VRF128:$source)>;

  def : Pat<(f32 (vecreduce_fadd (v4f32 VRF128:$source))),
            (SAU_SUMX_f32 VRF128:$source)>;
}

//
// 1.4.8.43 - SAU.SWZ
//   Byte swizzle.
//

let Itinerary = II_SAU_SWZ_IRF in
{
  def SAU_SWZ     : SHAVE_SAUInstr<(outs IRF32:$dst), (ins IRF32:$src, i8imm:$o1, i8imm:$o2, i8imm:$o3, i8imm:$o4),
                                        ".SWZ $dst $src [$o1$o2$o3$o4]", [], [usesVAU]>;

  def SAU_SWZ_i16     : SHAVE_SAUInstr<(outs IRF16_l:$dst), (ins IRF16_l:$src, i8imm:$o1, i8imm:$o2, i8imm:$o3, i8imm:$o4),
                                        ".SWZ $dst $src [$o1$o2$o3$o4]", [], [usesVAU]>;
}

//
//   SAU.TANH
//   FP16 hyperbolic tangent.
//

// IRF variant
let Itinerary = II_SAU_MATH_IRF in
{
  def SAU_TANH_f16_l  : SAU_RROpC_Suffix_NoPat<IRF16_l, IRF16_l, ".TANH", ".0">;
}

// VRF variant
let Itinerary = II_SAU_MATH_VRF in
{
  def SAU_TANH_f16_imm_vrf : SAU_Math_Op_VRF<".TANH">;
}


//
// 1.4.8.44 - SAU.XOR
//   Bitwise XOR.
//

let Itinerary = II_SAU_AND_OR_XOR_IRF in
{
  defm SAU_XOR : SAU_BitwiseLogic<xor, ".XOR", [usesVAU]>;

  // Special definition for the tail_writeback intrinsic with one register for all three operands
  def SAU_XOR_tied : SHAVE_SAUInstr<(outs IRF32:$dst), (ins IRF32:$src), ".XOR $dst $src $src", [], [usesVAU]> {
    let Constraints = "$src = $dst";
  }
}

//
// 1.4.8.45 - SAU.XORX
//   Element-wise XOR of vector.
//

let Itinerary = II_SAU_OR_AND_IRF_VRF in
{
  defm SAU_XORX  : SAU_XOp_8_16_32<horizontal_xor, ".XORX" >;
}

let Predicates = [HasVRF128_Predicate] in {
  foreach type = [ v4i32,  v8i16,  v16i8 ] in {
    def : Pat<(SHAVE_TypeInfo<type>.scalarType (vecreduce_and (type VRF128:$source))),
              (!cast<Instruction>("SAU_ANDX_x"#SHAVE_TypeInfo<type>.scalarType.Size)
              VRF128:$source)>;
    def : Pat<(SHAVE_TypeInfo<type>.scalarType (vecreduce_or (type VRF128:$source))),
              (!cast<Instruction>("SAU_ORX_x"#SHAVE_TypeInfo<type>.scalarType.Size)
              VRF128:$source)>;
    def : Pat<(SHAVE_TypeInfo<type>.scalarType (vecreduce_xor (type VRF128:$source))),
              (!cast<Instruction>("SAU_XORX_x"#SHAVE_TypeInfo<type>.scalarType.Size)
              VRF128:$source)>;
  }
}

// Class for DP4 variants with register writeback
class SAU_DP4_Base_Myr2v6<string asmstr, list <Register>useList, list <Register>defList, string accStr="", list <SHAVEConflictName>conflicts=[]>:
  SHAVE_SAUInstr<(outs IRF32:$dst), (ins IRF32:$src1, IRF32:$src2),
                 !strconcat(asmstr, " $dst $src1 $src2 ", accStr), [], !listconcat([usesSAUIntegerAdder], conflicts)>
{
  let Uses = !listconcat([S_STATE], useList);
  let Defs = !listconcat([S_STATE], defList);
}

let Itinerary = II_SAU_DP4_NONACC_Myr2v6 in
{
  def SAU_DP4_v4i8 : SAU_DP4_Base_Myr2v6<".DP4", [], []>;
  def SAU_DP4M_v4i8 : SAU_DP4_Base_Myr2v6<".DP4M", [], []>;
}

// Class for DP4 variants with only S_ACC writes (no register writeback)
class SAU_DP4_Base_NoDest_Myr2v6<string asmstr, list <Register>useList, list <Register>defList, string accStr="">:
  SHAVE_SAUInstr<(outs), (ins IRF32:$src1, IRF32:$src2),
                 !strconcat(asmstr, " $src1 $src2 ", accStr), [], [usesSAUIntegerAdder, isDP4A]>
{
  let Uses = !listconcat([S_STATE], useList);
  let Defs = !listconcat([S_STATE], defList);
}

multiclass SAU_DP4_TYPE<string asmstr, list <Register>useList, list <Register>defList, string accStr = "">
{
  def ""    :  SAU_DP4_Base_Myr2v6<asmstr, useList, defList, accStr, [isDP4A]>;
  def _void    : SAU_DP4_Base_NoDest_Myr2v6<asmstr, useList, defList, accStr>;
}

let Itinerary = II_SAU_DP4_ACC_Myr2v6 in
{
  defm SAU_DP4A_v4i8_sacc0 : SAU_DP4_TYPE<".DP4A", [S_ACC0], [S_ACC0], "S_ACC0">;
  defm SAU_DP4AZ_v4i8_sacc0 : SAU_DP4_TYPE<".DP4AZ", [], [S_ACC0], "S_ACC0">;

  defm SAU_DP4AM_v4i8_sacc0 : SAU_DP4_TYPE<".DP4AM", [S_ACC0], [S_ACC0], "S_ACC0">;
  defm SAU_DP4AMZ_v4i8_sacc0 : SAU_DP4_TYPE<".DP4AMZ", [], [S_ACC0], "S_ACC0">;

  defm SAU_DP4A_v4i8_sacc1 : SAU_DP4_TYPE<".DP4A", [S_ACC1], [S_ACC1], "S_ACC1">;
  defm SAU_DP4AZ_v4i8_sacc1 : SAU_DP4_TYPE<".DP4AZ", [], [S_ACC1], "S_ACC1">;

  defm SAU_DP4AM_v4i8_sacc1 : SAU_DP4_TYPE<".DP4AM", [S_ACC1], [S_ACC1], "S_ACC1">;
  defm SAU_DP4AMZ_v4i8_sacc1 : SAU_DP4_TYPE<".DP4AMZ", [], [S_ACC1], "S_ACC1">;

}

// NPU2.6 SAU LOG/EXP
let Itinerary = II_SAU_MATH_IRF in {
    def SAU_LOG_f16 : SAU_RROpC_Suffix<flog, IRF16_l, IRF16_l, f16, ".LOG", ".0">;
    def SAU_EXP_f16 : SAU_RROpC_Suffix<fexp, IRF16_l, IRF16_l, f16, ".EXP", ".0">;
    // Duplicate definitions for duplicate built-ins __builtin_shave_sau_*_f16_l_r to maintain legacy support
    def SAU_LOG_f16_l : SAU_RROpC_Suffix_NoPat<IRF16_l, IRF16_l, ".LOG", ".0">;
    def SAU_EXP_f16_l : SAU_RROpC_Suffix_NoPat<IRF16_l, IRF16_l, ".EXP", ".0">;
}

include "SHAVEv3/SHAVEv3InstrInfo_SAU.td"
