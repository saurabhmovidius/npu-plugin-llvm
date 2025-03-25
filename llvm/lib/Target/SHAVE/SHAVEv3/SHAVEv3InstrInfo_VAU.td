//===-- SHAVEv3InstrInfo_VAU.td - VAU Instructions -----------*- tablegen -*-===//
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

let Predicates = [HasVRF512_Predicate] in {

let Itinerary = II_VAU_ADD_SUB_RRR_VRF, Defs = [I_AE] in {
  def WVAU_ADD_i32   : VAU_RRROpC<add, WVRF512,  v16i32, ".ADD.i32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

//
// Reg <-- (Reg op Imm) classes
//

multiclass VAU_RRIOpC_Myr4<SDNode op, ValueType type, string asmstr, PatLeaf immPat, 
                           bit isUnsigned, bit isBitwise = 0, bit modAsmstr = 1,
                           list<SHAVEConflictName> conflicts = []> {
  def _#!cast<string>(type.Size)#_#!cast<string>(type)#_Myr4 : VAU_RRIOpC<op, 
                                                                          SHAVE_TypeInfo<type, isUnsigned>.regClass,
                                                                          type,
                                                                          SHAVE_TypeInfo<type, isUnsigned>.scalarType,
                                                                          SHAVE_TypeInfo<type, isUnsigned>.immediateOperand,
                                                                          !if(modAsmstr, !strconcat(asmstr, ".", SHAVE_TypeInfo<type, isUnsigned, isBitwise>.asmstr), asmstr),
                                                                          immPat, conflicts>;
}

multiclass VAU_RRIOp_Myr4<SDNode op, string asmstr, PatLeaf immPat, list<SHAVEConflictName> conflicts = []> {
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm  _imm : VAU_RRIOpC_Myr4<op, type, asmstr, immPat, 0, 0, 1, conflicts>;
  }
}

//
// Reg <-- (op Reg) classes
//

multiclass VAU_RRROpC_Myr4<SDNode op, ValueType type, string asmstr,
                           bit isUnsigned, bit isBitwise = 0, bit modAsmstr = 1,
                           list<SHAVEConflictName> conflicts = []> {
  def _#!cast<string>(type.Size)#_#!cast<string>(type)#_Myr4 : VAU_RRROpC<op,
                                                                          SHAVE_TypeInfo<type, isUnsigned>.regClass,
                                                                          type,
                                                                          !if(modAsmstr, !strconcat(asmstr, ".", SHAVE_TypeInfo<type, isUnsigned, isBitwise>.asmstr), asmstr),
                                                                          conflicts>;
}

//
// Multiclasses for logic instructions
//

multiclass VAU_LogicX_Myr4<SDNode op, string asmstr> {
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm  _X : VAU_RRROpC_Myr4<op, type, asmstr, 0, 1, 1>;
  }
}

multiclass VAU_LogicIs_Myr4<SDNode op, string asmstr> {
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm  _signed : VAU_RRROpC_Myr4<op, type, asmstr, 0>;
  }
}

multiclass VAU_LogicUs_Myr4<SDNode op, string asmstr> {
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm  _unsigned : VAU_RRROpC_Myr4<op, type, asmstr, 1>;
  }
}

multiclass VAU_LogicX_Imm_Myr4<SDNode op, string asmstr, PatLeaf immPat> {
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm  _X_imm : VAU_RRIOpC_Myr4<op, type, asmstr, immPat, 0, 1>;
  }
}

multiclass VAU_LogicIs_Imm_Myr4<SDNode op, string asmstr, PatLeaf immPat> {
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm  _signed_imm : VAU_RRIOpC_Myr4<op, type, asmstr, immPat, 0>;
  }
}


multiclass VAU_LogicUs_Imm_Myr4<SDNode op, string asmstr, PatLeaf immPat> {
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm  _unsigned_imm : VAU_RRIOpC_Myr4<op, type, asmstr, immPat, 1>;
  }
}

multiclass VAU_BitwiseLogic_Myr4<SDNode op, string asmstr> {
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm  _bitwise : VAU_RRROpC_Myr4<op, type, asmstr, 0, 0, 0>;
  }
}

//
// Multiclasses for saturating arithmetic instructions without patterns
//

multiclass VAU_RRRsOp_Raw_Myr4<string asmstr, list<SHAVEConflictName> conflicts = []> {
  def _i32_VRF512  : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".i32"), conflicts>;
  def _i16_VRF512  : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".i16"), conflicts>;
  def _i8_VRF512   : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".i8"),  conflicts>;
  def _u32_VRF512  : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".u32"), conflicts>;
  def _u16_VRF512  : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".u16"), conflicts>;
  def _u8_VRF512   : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".u8"),  conflicts>;
}

multiclass VAU_RRROp_Raw_Myr4<string asmstr, list<SHAVEConflictName> conflicts = []> {
  def _i32_VRF512  : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".i32"), conflicts>;
  def _i16_VRF512  : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".i16"), conflicts>;
  def _i8_VRF512   : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".i8"),  conflicts>;
  def _u32_VRF512  : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".u32"), conflicts>;
  def _u16_VRF512  : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".u16"), conflicts>;
  def _u8_VRF512   : VAU_RRROpC_Raw<WVRF512, !strconcat(asmstr, ".u8"),  conflicts>;
}

multiclass VAU_RRIsOp_Raw_Myr4<string asmstr, list<SHAVEConflictName> conflicts = []> {
  def _i32_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i32imm, !strconcat(asmstr, ".i32"), conflicts>;
  def _i16_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i16imm, !strconcat(asmstr, ".i16"), conflicts>;
  def _i8_imm_VRF512   : VAU_RRIOpC_Raw<WVRF512, i8imm,  !strconcat(asmstr, ".i8"),  conflicts>;
  def _u32_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i32imm, !strconcat(asmstr, ".u32"), conflicts>;
  def _u16_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i16imm, !strconcat(asmstr, ".u16"), conflicts>;
  def _u8_imm_VRF512   : VAU_RRIOpC_Raw<WVRF512, i8imm,  !strconcat(asmstr, ".u8"),  conflicts>;
}


//===----------------------------------------------------------------------===//
// VAU instruction definitions (Section 1.4.9 of Fragrak SHAVE Reference Guide)
//===----------------------------------------------------------------------===//

//
// 1.4.9.1 - VAU.ABS
//   Long vector absolute value.
//

let Itinerary = II_VAU_ABS_VRF in {
  def VAU_ABS_i32_VRF512 : VAU_RROpC_Raw<WVRF512, ".ABS.i32">;
  def VAU_ABS_i16_VRF512 : VAU_RROpC_Raw<WVRF512, ".ABS.i16">;
  def VAU_ABS_i8_VRF512  : VAU_RROpC_Raw<WVRF512, ".ABS.i8">;
  def VAU_ABS_f32_VRF512 : VAU_RROpC_Raw<WVRF512, ".ABS.f32">;
  def VAU_ABS_f16_VRF512 : VAU_RROpC_Raw<WVRF512, ".ABS.f16">;

  foreach type = [v16f32, v32f16, v8f32, v16f16, v4f32, v8f16, v2f32, v4f16] in {
     def VAU_ABS_#!cast<string>(type)#_Myr4 : VAU_RROpC<fabs, SHAVE_TypeInfo<type, 0>.regClass, type, !strconcat(".ABS.", SHAVE_TypeInfo<type, 0>.asmstr)>;
  }
}

//
// 1.4.9.2 - VAU.ACC
//   Vector add/subtract accumulate with optional zeroing of the accumulator.
//


let Defs = [V_ACC, V_ACC0x, V_ACC1x] in {
  def VAU_ACCP_SEQ_i32_Myr4 : VAU_ACC_SEQ_BASE<WVRF512, [isACCPSeq]>;
  def VAU_ACCP_SEQ_u32_Myr4 : VAU_ACC_SEQ_BASE<WVRF512, [isACCPSeq]>;
  def VAU_ACCP_SEQ_i16_Myr4 : VAU_ACC_SEQ_BASE<WVRF512, [isACCPSeq]>;
  def VAU_ACCP_SEQ_u16_Myr4 : VAU_ACC_SEQ_BASE<WVRF512, [isACCPSeq]>;
  def VAU_ACCP_SEQ_i8_Myr4  : VAU_ACC_SEQ_BASE<WVRF512, [isACCPSeq]>;
  def VAU_ACCP_SEQ_u8_Myr4  : VAU_ACC_SEQ_BASE<WVRF512, [isACCPSeq]>;

  def VAU_ACCN_SEQ_i32_Myr4 : VAU_ACC_SEQ_BASE<WVRF512>;
  def VAU_ACCN_SEQ_u32_Myr4 : VAU_ACC_SEQ_BASE<WVRF512>;
  def VAU_ACCN_SEQ_i16_Myr4 : VAU_ACC_SEQ_BASE<WVRF512>;
  def VAU_ACCN_SEQ_u16_Myr4 : VAU_ACC_SEQ_BASE<WVRF512>;
  def VAU_ACCN_SEQ_i8_Myr4  : VAU_ACC_SEQ_BASE<WVRF512>;
  def VAU_ACCN_SEQ_u8_Myr4  : VAU_ACC_SEQ_BASE<WVRF512>;
}

let Defs = [V_ACC, V_ACC0x, V_ACC1x, V0] in {
  def VAU_ACCP_SEQ_f32_Myr4 : VAU_ACC_SEQ_BASE<WVRF512, [isACCPSeq]>;
  def VAU_ACCP_SEQ_f16_Myr4 : VAU_ACC_SEQ_BASE<WVRF512, [isACCPSeq]>;

  def VAU_ACCN_SEQ_f32_Myr4 : VAU_ACC_SEQ_BASE<WVRF512>;
  def VAU_ACCN_SEQ_f16_Myr4 : VAU_ACC_SEQ_BASE<WVRF512>;
}


// Integer accumulator patterns
multiclass VAU_ACC_TYPE_Myr4<string asmstr> {
  def _i32_Myr4    : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i32"), [isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u32_Myr4    : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u32"), [isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _i16_Myr4    : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i16"), [isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u16_Myr4    : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u16"), [isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _i8_Myr4     : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i8"),  [isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u8_Myr4     : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u8"),  [isACC_MAC_Int, usesVAUIntegerAdder]>;
  let IsPipelinedACCBit = 1 in {
     def _i32_pl_Myr4 : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i32"), [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u32_pl_Myr4 : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u32"), [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _i16_pl_Myr4 : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i16"), [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u16_pl_Myr4 : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u16"), [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _i8_pl_Myr4  : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i8"),  [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u8_pl_Myr4  : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u8"),  [isPipelinedACCorMAC, isACC_MAC_Int, usesVAUIntegerAdder]>;
  }
}

let Itinerary = II_VAU_ACC_INT, ACC_MAC_InsnType = ACC_MAC_Z in {
  defm VAU_ACCPZ : VAU_ACC_TYPE_Myr4<".ACCPZ">;
  defm VAU_ACCNZ : VAU_ACC_TYPE_Myr4<".ACCNZ">;

}

let Itinerary = II_VAU_ACC_INT, ACC_MAC_InsnType = ACC_MAC_PN in {
  defm VAU_ACCP  : VAU_ACC_TYPE_Myr4<".ACCP">;
  defm VAU_ACCN  : VAU_ACC_TYPE_Myr4<".ACCN">;
}

// Floating-point accumulator patterns
multiclass VAU_ACC_TYPE_FLOAT_Myr4<string asmstr> {
  let Itinerary = II_VAU_ACC_FP_Pipe, IsPipelinedACCBit = 1 in {
    def _f32_pl_Myr4 : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".f32"), [isPipelinedACCorMAC, isACC_MAC_Float, usesVAUFloatingPointAdder]>;
    def _f16_pl_Myr4 : VAU_ACC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".f16"), [isPipelinedACCorMAC, isACC_MAC_Float, usesVAUFloatingPointAdder]>;
  }

}
  let ACC_MAC_InsnType = ACC_MAC_Z in {
    defm  VAU_ACCPZ : VAU_ACC_TYPE_FLOAT_Myr4<".ACCPZ">;
    defm  VAU_ACCNZ : VAU_ACC_TYPE_FLOAT_Myr4<".ACCNZ">;
   }

  let ACC_MAC_InsnType = ACC_MAC_PN in {
     defm  VAU_ACCP : VAU_ACC_TYPE_FLOAT_Myr4<".ACCP">;
     defm  VAU_ACCN : VAU_ACC_TYPE_FLOAT_Myr4<".ACCN">;
   }

//
// 1.4.9.3 - VAU.ACCW
//   Vector add/subtract accumulate with write to VRF.
//


multiclass VAU_ACCW_TYPE_Myr4<string asmstr> {
  def _i32_Myr4    : VAU_ACCW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i32"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u32_Myr4    : VAU_ACCW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u32"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _i16_Myr4    : VAU_ACCW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i16"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u16_Myr4    : VAU_ACCW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u16"), [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _i8_Myr4     : VAU_ACCW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i8"),  [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  def _u8_Myr4     : VAU_ACCW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u8"),  [usesVAUAdder, isACCWNonPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;

  let IsPipelinedACCBit = 1 in {
     def _i32_pl_Myr4 : VAU_ACCW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".i32"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u32_pl_Myr4 : VAU_ACCW_BASE<TRF_ACC32, IRF32,   !strconcat(asmstr, ".u32"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _i16_pl_Myr4 : VAU_ACCW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".i16"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u16_pl_Myr4 : VAU_ACCW_BASE<TRF_ACC32, IRF16_l, !strconcat(asmstr, ".u16"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _i8_pl_Myr4  : VAU_ACCW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".i8"),  [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
     def _u8_pl_Myr4  : VAU_ACCW_BASE<TRF_ACC32, IRF8_q0, !strconcat(asmstr, ".u8"),  [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Int, usesVAUIntegerAdder]>;
  }
}

multiclass VAU_ACCW_TYPE_FLOAT_Myr4<string asmstr> {
 let Itinerary = II_VAU_ACCW_FP_Pipe, IsPipelinedACCBit = 1 in {
    def _f32_pl_Myr4 : VAU_ACCW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".f32"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Float, usesVAUFloatingPointAdder]>;
    def _f16_pl_Myr4 : VAU_ACCW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".f16"), [isPipelinedACCorMAC, isACCWPipe, isACC_MAC_Float, usesVAUFloatingPointAdder]>;
 }

}

let Itinerary = II_VAU_ACCW_INT in {
  defm VAU_ACCPW  : VAU_ACCW_TYPE_Myr4<".ACCPW">;
  defm VAU_ACCNW  : VAU_ACCW_TYPE_Myr4<".ACCNW">;
}

  defm VAU_ACCPW : VAU_ACCW_TYPE_FLOAT_Myr4<".ACCPW">;
  defm VAU_ACCNW : VAU_ACCW_TYPE_FLOAT_Myr4<".ACCNW">;

//
// 1.4.9.4 - VAU.ADD
//   Vector addition with optional immediate operand.
//

// Integer variants
let Itinerary = II_VAU_ADD_SUB_RRR_VRF, Defs = [I_AE] in {
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm VAU_ADD : VAU_RRROpC_Myr4<add, type, ".ADD", 0, 0, 1, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  }

  // Instruction definitions for C builtins
  def VAU_ADD_i32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".ADD.i32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_ADD_i16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".ADD.i16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_ADD_i8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".ADD.i8",  [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_RRI_VRF, Defs = [I_AE] in {
  defm VAU_ADD : VAU_RRIOp_Myr4<add, ".ADD", uimm5, [usesVAUIntegerAdder]>;

  // Instruction definitions for C builtins
  def VAU_ADD_i32_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i32imm, ".ADD.i32", [usesVAUIntegerAdder]>;
  def VAU_ADD_i16_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i16imm, ".ADD.i16", [usesVAUIntegerAdder]>;
  def VAU_ADD_i8_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i8imm,  ".ADD.i8",  [usesVAUIntegerAdder]>;
}

// Floating point variants
let Itinerary = II_VAU_ADD_SUB_MUL_RRR_VRF, Defs = [I_AE] in {
  foreach type = [v16f32, v32f16, v8f32, v16f16, v4f32, v8f16, v2f32, v4f16] in {
    defm VAU_ADD : VAU_RRROpC_Myr4<fadd, type, ".ADD", 0, 0, 1, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  }

  // Instruction definitions for C builtins
  def VAU_ADD_f32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".ADD.f32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  def VAU_ADD_f16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".ADD.f16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_MUL_RRI_VRF, Defs = [I_AE] in {
  foreach type = [v16f32, v8f32, v4f32, v2f32] in {
    defm VAU_ADD_imm : VAU_RRIOpC_Myr4<fadd, type, ".ADD", FloatConst, 0, 0, 1, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  }
  foreach type = [v32f16, v16f16, v8f16, v4f16] in {
    defm VAU_ADD_imm : VAU_RRIOpC_Myr4<fadd, type, ".ADD", HalfFloatConst, 0, 0, 1, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  }

  // Instruction definitions for C builtins
  def VAU_ADD_f32_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, constf32, ".ADD.f32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  def VAU_ADD_f16_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, constf16, ".ADD.f16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
}

//
// 1.4.9.5 - VAU.ADDCI
//   Long vector 32-bit integer addition with carry-in.
//

let Itinerary = II_VAU_ADDCI_SUBCI_VRF, Uses = [V_STATE], Defs = [V_STATE] in {
  def VAU_ADDCI_i32_Myr4 : VAU_RRROpC_Raw<WVRF512, ".ADDCI", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

//
// 1.4.9.6 - VAU.ADIFF
//   Long vector absolute difference.
//
let Itinerary = II_VAU_ADIFF_VRF_FLOAT in {
  def VAU_ADIFF_f32_VRF512 : VAU_ADIFF_Class<WVRF512, v16f32, "f32">;
  def VAU_ADIFF_f16_VRF512 : VAU_ADIFF_Class<WVRF512, v32f16, "f16">;
}

let Itinerary = II_VAU_ADIFF_VRF in {
  defm VAU_ADIFF : VAU_RRROp_Raw_Myr4<".ADIFF", [usesVAUIntegerAdder]>;
}

//
// 1.4.9.7 - VAU.ALIGNBYTE
//   Long vector align bytes within words.
//

let Itinerary = II_VAU_ALIGN_VRF in {
  def VAU_ALIGNBYTE_imm_vrf_VRF512 : VAU_Align<WVRF512,  WVRF512,  ".ALIGNBYTE">;
}

//
// 1.4.9.8 - VAU.AND
//   Bitwise AND.
//

let Itinerary = II_VAU_AND_OR_XOR_VRF in {
  defm VAU_AND : VAU_BitwiseLogic_Myr4<and, ".AND">;

  // Instruction definitions for C builtins
  def VAU_AND_32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".AND">;
  def VAU_AND_16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".AND">;
  def VAU_AND_8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".AND">;
}

//
// 1.4.9.9 - VAU.AVG
//   Long vector integer average.
//

class VAU_AVG_Myr4<ValueType vt, string asmstr> :
  SHAVE_VAUInstr<(outs WVRF512:$dst), (ins WVRF512:$src1, WVRF512:$src2),
                 !strconcat(".AVG.", asmstr, " $dst $src1 $src2"),
                 [(set (vt WVRF512:$dst), (srl (add (add (vt WVRF512:$src1), (vt WVRF512:$src2)), (vt 1)), (vt 1)))], [usesVAUIntegerAdder]>;

let Itinerary = II_VAU_AVG_VRF in {
  def VAU_AVG_x32_VRF512 : VAU_AVG_Myr4<v16i32, "x32">;
  def VAU_AVG_x16_VRF512 : VAU_AVG_Myr4<v32i16, "x16">;
  def VAU_AVG_x8_VRF512  : VAU_AVG_Myr4<v64i8, "x8">;
}

//
// 1.4.9.11 - VAU.IADDS
//   Long vector integer addition with saturation. Optional unsigned immediate operand.
//

let Itinerary = II_VAU_ADD_SUB_RRR_VRF_WITH_NO_CONDITION in {
  defm VAU_IADDS : VAU_RRRsOp_Raw_Myr4<".IADDS", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_RRI_VRF_WITH_NO_CONDITION in {
  defm VAU_IADDS : VAU_RRIsOp_Raw_Myr4<".IADDS", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

//
// 1.4.9.12 - VAU.ILMULH
//   Long vector multiply returning higher-order result.
//

let Itinerary = II_VAU_ILMULH_VRF in {
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm VAU_ILMULH_signed   : VAU_RRROpC_Myr4<mulhs, type, ".ILMULH", 0>;
    defm AU_ILMULH_unsigned : VAU_RRROpC_Myr4<mulhu, type, ".ILMULH", 1>;
  }

  // Instruction definitions for C builtins
  def VAU_ILMULH_i32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".ILMULH.i32">;
  def VAU_ILMULH_u32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".ILMULH.u32">;
  def VAU_ILMULH_i16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".ILMULH.i16">;
  def VAU_ILMULH_u16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".ILMULH.u16">;
  def VAU_ILMULH_i8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".ILMULH.i8">;
  def VAU_ILMULH_u8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".ILMULH.u8">;
}

//
// 1.4.9.13 - VAU.IMULS
//   Long vector integer multiplication with saturation. Optional immediate operand
//

let Itinerary = II_VAU_MUL_RRR_VRF_WITH_NO_CONDITION in {
  defm VAU_IMULS : VAU_RRRsOp_Raw_Myr4<".IMULS">;
}

let Itinerary = II_VAU_MUL_RRI_VRF_WITH_NO_CONDITION in {
  defm VAU_IMULS : VAU_RRIsOp_Raw_Myr4<".IMULS">;
}

//
// 1.4.9.14 - VAU.ISUBS
//   Long vector integer subtraction with saturation. Optional unsigned immediate operand.
//

let Itinerary = II_VAU_ADD_SUB_RRR_VRF_WITH_NO_CONDITION in {
  defm VAU_ISUBS : VAU_RRRsOp_Raw_Myr4<".ISUBS", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_RRI_VRF_WITH_NO_CONDITION in {
  defm VAU_ISUBS : VAU_RRIsOp_Raw_Myr4<".ISUBS", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

//
// 1.4.9.15 - VAU.MAC
//   Vector multiply-add/subtract accumulate with optional zeroing of the accumulator.
//
//
// Pseudo instructions for VAU.MAC and VAU.MACPW variants
//
let Defs = [V_ACC, V_ACC0x, V_ACC1x] in {
  def VAU_MACP_SEQ_i32_Myr4 : VAU_MAC_SEQ_BASE<WVRF512, [isMACPSeq]>;
  def VAU_MACP_SEQ_u32_Myr4 : VAU_MAC_SEQ_BASE<WVRF512, [isMACPSeq]>;
  def VAU_MACP_SEQ_i16_Myr4 : VAU_MAC_SEQ_BASE<WVRF512, [isMACPSeq]>;
  def VAU_MACP_SEQ_u16_Myr4 : VAU_MAC_SEQ_BASE<WVRF512, [isMACPSeq]>;
  def VAU_MACP_SEQ_i8_Myr4  : VAU_MAC_SEQ_BASE<WVRF512, [isMACPSeq]>;
  def VAU_MACP_SEQ_u8_Myr4  : VAU_MAC_SEQ_BASE<WVRF512, [isMACPSeq]>;

  def VAU_MACN_SEQ_i32_Myr4 : VAU_MAC_SEQ_BASE<WVRF512>;
  def VAU_MACN_SEQ_u32_Myr4 : VAU_MAC_SEQ_BASE<WVRF512>;
  def VAU_MACN_SEQ_i16_Myr4 : VAU_MAC_SEQ_BASE<WVRF512>;
  def VAU_MACN_SEQ_u16_Myr4 : VAU_MAC_SEQ_BASE<WVRF512>;
  def VAU_MACN_SEQ_i8_Myr4  : VAU_MAC_SEQ_BASE<WVRF512>;
  def VAU_MACN_SEQ_u8_Myr4  : VAU_MAC_SEQ_BASE<WVRF512>;
}

let Defs = [V_ACC, V_ACC0x, V_ACC1x, V0] in {
  def VAU_MACP_SEQ_f32_Myr4 : VAU_MAC_SEQ_BASE<WVRF512, [isMACPSeq]>;
  def VAU_MACP_SEQ_f16_Myr4 : VAU_MAC_SEQ_BASE<WVRF512, [isMACPSeq]>;

  def VAU_MACN_SEQ_f32_Myr4 : VAU_MAC_SEQ_BASE<WVRF512>;
  def VAU_MACN_SEQ_f16_Myr4 : VAU_MAC_SEQ_BASE<WVRF512>;
}

multiclass VAU_MAC_TYPE_Myr4<string asmstr> {
  def _i32_Myr4    : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i32"), [isACC_MAC_Int]>;
  def _u32_Myr4    : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u32"), [isACC_MAC_Int]>;
  def _i16_Myr4    : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i16"), [isACC_MAC_Int]>;
  def _u16_Myr4    : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u16"), [isACC_MAC_Int]>;
  def _i8_Myr4     : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i8"),  [isACC_MAC_Int]>;
  def _u8_Myr4     : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u8"),  [isACC_MAC_Int]>;

  let IsPipelinedACCBit = 1 in {
       def _i32_pl_Myr4 : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i32"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _u32_pl_Myr4 : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u32"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _i16_pl_Myr4 : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i16"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _u16_pl_Myr4 : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u16"), [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _i8_pl_Myr4  : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i8"),  [ isPipelinedACCorMAC, isACC_MAC_Int]>;
       def _u8_pl_Myr4  : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u8"),  [ isPipelinedACCorMAC, isACC_MAC_Int]>;
   }
}

let Itinerary = II_VAU_MAC_INT, ACC_MAC_InsnType = ACC_MAC_Z in {
  defm VAU_MACPZ : VAU_MAC_TYPE_Myr4<".MACPZ">;
  defm VAU_MACNZ : VAU_MAC_TYPE_Myr4<".MACNZ">;
}

let Itinerary = II_VAU_MAC_INT, ACC_MAC_InsnType = ACC_MAC_PN in {
  defm VAU_MACP  : VAU_MAC_TYPE_Myr4<".MACP">;
  defm VAU_MACN  : VAU_MAC_TYPE_Myr4<".MACN">;
}

// Floating-point MAC accumulator patterns
multiclass VAU_MAC_TYPE_FLOAT_Myr4<string asmstr> {
  let Itinerary = II_VAU_MAC_FP_Pipe, IsPipelinedACCBit = 1 in {
    def _f32_pl_Myr4 : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".f32"), [isPipelinedACCorMAC, isACC_MAC_Float]>;
    def _f16_pl_Myr4 : VAU_MAC_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".f16"), [isPipelinedACCorMAC, isACC_MAC_Float]>;
  }
}

let ACC_MAC_InsnType = ACC_MAC_Z in {
  defm VAU_MACPZ : VAU_MAC_TYPE_FLOAT_Myr4<".MACPZ">;
  defm VAU_MACNZ : VAU_MAC_TYPE_FLOAT_Myr4<".MACNZ">;
}

let ACC_MAC_InsnType = ACC_MAC_PN in {
  defm VAU_MACP : VAU_MAC_TYPE_FLOAT_Myr4<".MACP">;
  defm VAU_MACN : VAU_MAC_TYPE_FLOAT_Myr4<".MACN">;
}

//
// 1.4.8.23 - VAU.MACNW
//   Scalar multiply-subtract accumulate with write to IRF.
//

multiclass VAU_MACW_TYPE_Myr4<string asmstr> {
  def _i32_Myr4    : VAU_MACW_BASE<TRF_ACC128, WVRF512,  !strconcat(asmstr, ".i32"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _u32_Myr4    : VAU_MACW_BASE<TRF_ACC128, WVRF512,  !strconcat(asmstr, ".u32"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _i16_Myr4    : VAU_MACW_BASE<TRF_ACC128, WVRF512,  !strconcat(asmstr, ".i16"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _u16_Myr4    : VAU_MACW_BASE<TRF_ACC128, WVRF512,  !strconcat(asmstr, ".u16"), [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _i8_Myr4     : VAU_MACW_BASE<TRF_ACC128, WVRF512,  !strconcat(asmstr, ".i8"),  [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;
  def _u8_Myr4     : VAU_MACW_BASE<TRF_ACC128, WVRF512,  !strconcat(asmstr, ".u8"),  [usesVAUAdder, isMACWNonPipe, isACC_MAC_Int]>;

  let IsPipelinedACCBit = 1 in {
       def _i32_pl_Myr4 : VAU_MACW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i32"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _u32_pl_Myr4 : VAU_MACW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u32"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _i16_pl_Myr4 : VAU_MACW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i16"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _u16_pl_Myr4 : VAU_MACW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u16"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _i8_pl_Myr4  : VAU_MACW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".i8"),  [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
       def _u8_pl_Myr4  : VAU_MACW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".u8"),  [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Int]>;
  }
}

multiclass VAU_MACW_TYPE_FLOAT_Myr4<string asmstr> {
  let Itinerary = II_VAU_MACW_FP_Pipe, IsPipelinedACCBit = 1 in {
    def _f32_pl_Myr4 : VAU_MACW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".f32"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Float]>;
    def _f16_pl_Myr4 : VAU_MACW_BASE<TRF_ACC128, WVRF512, !strconcat(asmstr, ".f16"), [ isPipelinedACCorMAC, isMACWPipe, isACC_MAC_Float]>;
  }

}

let Itinerary = II_VAU_MACW_INT in {
  defm VAU_MACNW  : VAU_MACW_TYPE_Myr4<".MACNW">;
}

defm VAU_MACNW : VAU_MACW_TYPE_FLOAT_Myr4<".MACNW">;

//
// 1.4.8.24 - VAU.MACPW
//   Scalar multiply-add accumulate with write to IRF.
//

let Itinerary = II_VAU_MACW_INT in {
  defm VAU_MACPW  : VAU_MACW_TYPE_Myr4<".MACPW">;
}

defm VAU_MACPW : VAU_MACW_TYPE_FLOAT_Myr4<".MACPW">;


//
// 1.4.9.18 - VAU.MUL
//   Long vector multiplication with optional immediate operand.
//

let Itinerary = II_VAU_ADD_SUB_MUL_RRR_VRF, Defs = [I_AE] in {
  // Integer variants
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm VAU_MUL : VAU_RRROpC_Myr4<mul, type, ".MUL", 0, 0, 1, [isSAU_MULorVAU_MUL]>;
  }

  // Floating point variants
  foreach type = [v16f32, v32f16, v8f32, v16f16, v4f32, v8f16, v2f32, v4f16] in {
    defm VAU_MUL : VAU_RRROpC_Myr4<fmul, type, ".MUL", 0, 0, 1, [isSAU_MULorVAU_MUL]>;
  }

  // Instruction definitions for C builtins
  def VAU_MUL_i32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".MUL.i32", [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_i16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".MUL.i16", [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_i8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".MUL.i8",  [isSAU_MULorVAU_MUL]>;

  def VAU_MUL_f32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".MUL.f32", [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_f16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".MUL.f16", [isSAU_MULorVAU_MUL]>;
}

let Itinerary = II_VAU_ADD_SUB_MUL_RRI_VRF, Defs = [I_AE] in {
  // Integer variants
  defm VAU_MUL : VAU_RRIOp_Myr4<mul, ".MUL", simm5, [isSAU_MULorVAU_MUL]>;

  // Floating point variants
  foreach type = [v16f32, v8f32, v4f32, v2f32] in {
    defm VAU_MUL_imm : VAU_RRIOpC_Myr4<fmul, type, ".MUL", FloatConst, 0, 0, 1, [isSAU_MULorVAU_MUL]>;
  }
  foreach type = [v32f16, v16f16, v8f16, v4f16] in {
    defm VAU_MUL_imm : VAU_RRIOpC_Myr4<fmul, type, ".MUL", HalfFloatConst, 0, 0, 1, [isSAU_MULorVAU_MUL]>;
  }

  // Instruction definitions for C builtins
  def VAU_MUL_i32_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i32imm, ".MUL.i32", [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_i16_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i16imm, ".MUL.i16", [isSAU_MULorVAU_MUL]>;
  def VAU_MUL_i8_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i8imm,  ".MUL.i8",  [isSAU_MULorVAU_MUL]>;
}

//
// 1.4.9.19 - VAU.NOT
//   Bitwise NOT.
//

// VAU.NOT
let Itinerary = II_VAU_NOT_VRF in {
  def VAU_NOT_Myr4 : VAU_RROpC_Raw<WVRF512, ".NOT">;

  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    def VAU_NOT_#!cast<string>(type)#_Myr4 : VAU_NOT<SHAVE_TypeInfo<type, 0>.regClass, type>;
  }

  // Instruction definitions for C builtins
  def VAU_NOT_i32_VRF512 : VAU_RROpC_Raw<WVRF512, ".NOT">;
  def VAU_NOT_i16_VRF512 : VAU_RROpC_Raw<WVRF512, ".NOT">;
  def VAU_NOT_i8_VRF512  : VAU_RROpC_Raw<WVRF512, ".NOT">;
}

//
// 1.4.9.20 - VAU.OR
//   Bitwise OR.
//

let Itinerary = II_VAU_AND_OR_XOR_VRF in {
  defm VAU_OR : VAU_BitwiseLogic_Myr4<or, ".OR">;

  // Instruction definitions for C builtins
  def VAU_OR_32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".OR">;
  def VAU_OR_16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".OR">;
  def VAU_OR_8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".OR">;
}

//
// 1.4.9.21 - VAU.ROL
//   Long vector rotate left with optional immediate rotate amount.
//

let Itinerary = II_VAU_SHL_RRR_VRF, Defs = [V_STATE] in {
  defm VAU_ROL : VAU_LogicX_Myr4<rotl, ".ROL">;

  // Instruction definitions for C builtins
  def VAU_ROL_x32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".ROL.x32">;
  def VAU_ROL_x16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".ROL.x16">;
  def VAU_ROL_x8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".ROL.x8">;
}

let Itinerary = II_VAU_SHL_RRI_VRF, Defs = [V_STATE] in {
  defm VAU_ROL : VAU_LogicX_Imm_Myr4<rotl, ".ROL", uimm5>;

  // Instruction definitions for C builtins
  def VAU_ROL_x32_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i32imm, ".ROL.x32">;
  def VAU_ROL_x16_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i16imm, ".ROL.x16">;
  def VAU_ROL_x8_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i8imm,  ".ROL.x8">;
}

//
// 1.4.9.22 - VAU.SHD
//   Long vector arithmetic shift right with set to zero on underflow. Optional immediate shift amount.
//

let Itinerary = II_VAU_SHR_RRR_VRF, Defs = [V_STATE] in {
  defm VAU_SHD : VAU_LogicIs_Myr4<shd, ".SHD">;

  // Instruction definitions for C builtins
  def VAU_SHD_i32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SHD.i32">;
  def VAU_SHD_i16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SHD.i16">;
  def VAU_SHD_i8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".SHD.i8">;
}

let Itinerary = II_VAU_SHR_RRI_VRF, Defs = [V_STATE] in {
  defm VAU_SHD : VAU_LogicIs_Imm_Myr4<shd, ".SHD", uimm5>;

  // Instruction definitions for C builtins
  def VAU_SHD_i32_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i32imm, ".SHD.i32">;
  def VAU_SHD_i16_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i16imm, ".SHD.i16">;
  def VAU_SHD_i8_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i8imm,  ".SHD.i8">;
}

//
// 1.4.9.23 - VAU.SHL
//   Long vector logical shift left with optional immediate shift amount.
//

let Itinerary = II_VAU_SHL_RRR_VRF, Defs = [V_STATE] in {
  defm VAU_SHL : VAU_LogicX_Myr4<shl,  ".SHL">;

  // Instruction definitions for C builtins
  def VAU_SHL_x32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SHL.x32">;
  def VAU_SHL_x16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SHL.x16">;
  def VAU_SHL_x8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".SHL.x8">;
}

let Itinerary = II_VAU_SHL_RRI_VRF, Defs = [V_STATE] in {
  defm VAU_SHL : VAU_LogicX_Imm_Myr4<shl,  ".SHL", uimm5>;

  // Instruction definitions for C builtins
  def VAU_SHL_x32_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i32imm, ".SHL.x32">;
  def VAU_SHL_x16_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i16imm, ".SHL.x16">;
  def VAU_SHL_x8_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i8imm,  ".SHL.x8">;
}

//
// 1.4.9.24 - VAU.SHR
//   Long vector logical/arithmetic shift right with optional immediate shift amount.
//

let Itinerary = II_VAU_SHR_RRR_VRF, Defs = [V_STATE] in {
  defm VAU_SHR : VAU_LogicIs_Myr4<sra, ".SHR">;
  defm VAU_SHR : VAU_LogicUs_Myr4<srl, ".SHR">;

  // Instruction definitions for C builtins
  def VAU_SHR_i32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SHR.i32">;
  def VAU_SHR_i16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SHR.i16">;
  def VAU_SHR_i8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".SHR.i8">;

  def VAU_SHR_u32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SHR.u32">;
  def VAU_SHR_u16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SHR.u16">;
  def VAU_SHR_u8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".SHR.u8">;
}

let Itinerary = II_VAU_SHR_RRI_VRF, Defs = [V_STATE] in {
  defm VAU_SHR : VAU_LogicIs_Imm_Myr4<sra, ".SHR", uimm5>;
  defm VAU_SHR : VAU_LogicUs_Imm_Myr4<srl, ".SHR", uimm5>;

  // Instruction definitions for C builtins
  def VAU_SHR_i32_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i32imm, ".SHR.i32">;
  def VAU_SHR_i16_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i16imm, ".SHR.i16">;
  def VAU_SHR_i8_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i8imm,  ".SHR.i8">;

  def VAU_SHR_u32_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i32imm, ".SHR.u32">;
  def VAU_SHR_u16_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i16imm, ".SHR.u16">;
  def VAU_SHR_u8_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i8imm,  ".SHR.u8">;
}

//
// 1.4.9.25 - VAU.SUB
//   Long vector subtraction with optional immediate operand.
//

// Integer variants
let Itinerary = II_VAU_ADD_SUB_RRR_VRF, Defs = [I_AE] in {
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    defm VAU_SUB : VAU_RRROpC_Myr4<sub, type, ".SUB", 0, 0, 1, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  }

  // Instruction definitions for C builtins
  def VAU_SUB_i32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SUB.i32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_SUB_i16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SUB.i16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_SUB_i8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".SUB.i8",  [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_RRI_VRF, Defs = [I_AE] in {
  defm VAU_SUB : VAU_RRIOp_Myr4<sub, ".SUB", uimm5, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;

  // Instruction definitions for C builtins
  def VAU_SUB_i32_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i32imm, ".SUB.i32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_SUB_i16_imm_VRF512 : VAU_RRIOpC_Raw<WVRF512, i16imm, ".SUB.i16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
  def VAU_SUB_i8_imm_VRF512  : VAU_RRIOpC_Raw<WVRF512, i8imm,  ".SUB.i8",  [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

// Floating point variants
let Itinerary = II_VAU_ADD_SUB_MUL_RRR_VRF, Defs = [I_AE] in {
  foreach type = [v16f32, v32f16, v8f32, v16f16, v4f32, v8f16, v2f32, v4f16] in {
    defm VAU_SUB : VAU_RRROpC_Myr4<fsub, type, ".SUB", 0, 0, 1, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  }

  // Instruction definitions for C builtins
  def VAU_SUB_f32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SUB.f32", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  def VAU_SUB_f16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".SUB.f16", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
}

let Itinerary = II_VAU_ADD_SUB_MUL_RRI_VRF, Defs = [I_AE] in {
  foreach type = [v16f32, v8f32, v4f32, v2f32] in {
    defm VAU_SUB_imm : VAU_RRIOpC_Myr4<fsub, type, ".SUB", FloatConst, 0, 0, 1, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  }
  foreach type = [v32f16, v16f16, v8f16, v4f16] in {
    defm VAU_SUB_imm : VAU_RRIOpC_Myr4<fsub, type, ".SUB", HalfFloatConst, 0, 0, 1, [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUFloatingPointAdder]>;
  }
}

//
// 1.4.9.26 - VAU.SUBCI
//   Long vector 32-bit signed integer subtraction with carry-in.
//

let Itinerary = II_VAU_ADDCI_SUBCI_VRF, Uses = [V_STATE], Defs = [V_STATE] in {
  def VAU_SUBCI_i32_Myr4 : VAU_RRROpC_Raw<WVRF512, ".SUBCI", [usesVAUAdder, isSAUorVAU_ADDorSUB, usesVAUIntegerAdder]>;
}

//
// 1.4.9.27 - VAU.XOR
//   Bitwise XOR.
//

let Itinerary = II_VAU_AND_OR_XOR_VRF in {
  defm VAU_XOR : VAU_BitwiseLogic_Myr4<xor, ".XOR">;

  // Instruction definitions for C builtins
  def VAU_XOR_32_VRF512 : VAU_RRROpC_Raw<WVRF512, ".XOR">;
  def VAU_XOR_16_VRF512 : VAU_RRROpC_Raw<WVRF512, ".XOR">;
  def VAU_XOR_8_VRF512  : VAU_RRROpC_Raw<WVRF512, ".XOR">;
}

class VAU_TRANSCENDENTAL_Raw_Myr4<RegisterClass regVT, string asmstr> :
  SHAVE_VAUInstr<(outs regVT:$dst), (ins regVT:$src), !strconcat (asmstr, " $dst $src"),
                 []>;

class VAU_TRANSCENDENTAL_Myr4<SDNode op, RegisterClass regVT, ValueType type, string asmstr> :
  SHAVE_VAUInstr<(outs regVT:$dst), (ins regVT:$src), !strconcat (asmstr, " $dst $src"),
                 [(set (type regVT:$dst), (op (type regVT:$src)))]>;

let Itinerary = II_VAU_ISA26_TRANSCENDENTALS_VRF in
{
  // LLVM does not have intrinsics for these operations (as of LLVM 18.1.6) so they are only accessible
  // via the equivalent SHAVE built-ins/intrinsics
  def VAU_TANH_WVRF512 : VAU_TRANSCENDENTAL_Raw_Myr4<WVRF512, ".TANH">;
  def VAU_SIGM_WVRF512 : VAU_TRANSCENDENTAL_Raw_Myr4<WVRF512, ".SIGM">;

  foreach type = [ v32f16, v16f16, v8f16, v4f16, v2f16 ] in {
    defvar typeInfo = SHAVE_TypeInfo<type>;

    // VAU.SQT has a ULP of 0, so we can use it by default
    def VAU_SQT_#typeInfo.regClass  : VAU_TRANSCENDENTAL_Myr4<fsqrt, typeInfo.regClass, type, ".SQT">;

    // These instructions have a ULP of 1, so we cannot select them by default. There is custom lowering
    // support based on presence of "afn" (approximate functions) math flag
    def VAU_LOG_#typeInfo.regClass  : VAU_TRANSCENDENTAL_Myr4<flog, typeInfo.regClass, type, ".LOG">;
    def VAU_EXP_#typeInfo.regClass  : VAU_TRANSCENDENTAL_Myr4<fexp, typeInfo.regClass, type, ".EXP">;
  }
}

// Class for DP4 variants with register writeback
let Itinerary = II_VAU_DP4_NONACC_Myr2v6 in
{
  def VAU_DP4_v64i8_VRF512 : VAU_DP4_Base<WVRF512, ".DP4", [V_STATE], [V_STATE]>;
  def VAU_DP4M_v64i8_VRF512 : VAU_DP4_Base<WVRF512, ".DP4M", [V_STATE], [V_STATE]>;
}

// Class for DP4 variants with only V_ACC writes (no register writeback)
multiclass VAU_DP4_TYPE_Myr4<string asmstr, list <Register>useList, list <Register>defList, string accStr = "">
{
  def _VRF512      :  VAU_DP4_Base<WVRF512, asmstr, useList, defList, accStr, [isDP4A]>;
  def _void_VRF512 : VAU_DP4_Base_NoDest<WVRF512, asmstr, useList, defList, accStr>;
}

let Itinerary = II_VAU_DP4_ACC_Myr2v6 in
{
  defm VAU_DP4A_v64i8_vacc0 : VAU_DP4_TYPE_Myr4<".DP4A", [V_ACC0x], [V_ACC0x], "V_ACC0">;
  defm VAU_DP4AZ_v64i8_vacc0 : VAU_DP4_TYPE_Myr4<".DP4AZ", [], [V_ACC0x], "V_ACC0">;

  defm VAU_DP4AM_v64i8_vacc0 : VAU_DP4_TYPE_Myr4<".DP4AM", [V_ACC0x], [V_ACC0x], "V_ACC0">;
  defm VAU_DP4AMZ_v64i8_vacc0 : VAU_DP4_TYPE_Myr4<".DP4AMZ", [], [V_ACC0x], "V_ACC0">;

  defm VAU_DP4A_v64i8_vacc1 : VAU_DP4_TYPE_Myr4<".DP4A", [V_ACC1x], [V_ACC1x], "V_ACC1">;
  defm VAU_DP4AZ_v64i8_vacc1 : VAU_DP4_TYPE_Myr4<".DP4AZ", [], [V_ACC1x], "V_ACC1">;

  defm VAU_DP4AM_v64i8_vacc1 : VAU_DP4_TYPE_Myr4<".DP4AM", [V_ACC1x], [V_ACC1x], "V_ACC1">;
  defm VAU_DP4AMZ_v64i8_vacc1 : VAU_DP4_TYPE_Myr4<".DP4AMZ", [], [V_ACC1x], "V_ACC1">;

}


} // Predicates = [HasVRF512_Predicate]
