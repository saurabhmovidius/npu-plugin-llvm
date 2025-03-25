// ***************************************************************************
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
// ---------------------------------------------------------------------------
// File       : SHAVEv3InstrInfo.td
// Description: Definitions of pseudoinstructions and selection patterns
// ---------------------------------------------------------------------------

// Pseudo 512-bit vector load and store. These are used when spilling vectors to
// the stack as the spiller expects only one instruction to be emitted.
let mayLoad = 1 in {
  def LDV512 : SHAVE_Pseudo<(outs WVRF512:$val), (ins MEMr:$addr),
                             "LDV128_Myr4", [], [usesLSU0, usesLSU1]>;

  def LDOV512 : SHAVE_Pseudo<(outs WVRF512:$val), (ins MEMri:$addr),
                              "LDOV512_Myr4", [], [usesLSU0, usesLSU1]>;

  def LDO_I64_Myr4 : SHAVE_Pseudo<(outs IRF64:$val), (ins MEMri:$addr),
                              "LDOI64_Myr4", [], [usesLSU0, usesLSU1]>;

}

let mayStore = 1 in {
  def STV512 : SHAVE_Pseudo<(outs), (ins WVRF512:$val, MEMr:$addr),
                            "STV512", [], [usesLSU0, usesLSU1]>;

  def STOV512 : SHAVE_Pseudo<(outs), (ins WVRF512:$val, MEMri:$addr),
                             "STOV512", [], [usesLSU0, usesLSU1]>;

  def STO_I64_Myr4 : SHAVE_Pseudo<(outs), (ins IRF64:$val, MEMri:$addr),
                             "STOI64_Myr4", [], [usesLSU0, usesLSU1]>;

}

//
// Select pseudo instructions for WVRFx vectors
//

// 512-bit instruction
def SELECT_WVRF512 : SELECT<WVRF512, v16i32, CC_CMU0>;

// 512-bit patterns
def : Pat<(v32i16 (shave_select WVRF512:$trueVal, WVRF512:$falseVal, timm:$cc)),
          (SELECT_WVRF512 WVRF512:$trueVal, WVRF512:$falseVal, i32imm:$cc)>;
def : Pat<(v64i8 (shave_select WVRF512:$trueVal, WVRF512:$falseVal, timm:$cc)),
          (SELECT_WVRF512 WVRF512:$trueVal, WVRF512:$falseVal, i32imm:$cc)>;

def : Pat<(v16f32 (shave_select WVRF512:$trueVal, WVRF512:$falseVal, timm:$cc)),
          (SELECT_WVRF512 WVRF512:$trueVal, WVRF512:$falseVal, i32imm:$cc)>;
def : Pat<(v32f16 (shave_select WVRF512:$trueVal, WVRF512:$falseVal, timm:$cc)),
          (SELECT_WVRF512 WVRF512:$trueVal, WVRF512:$falseVal, i32imm:$cc)>;

// 256-bit instruction
def SELECT_WVRF256_0 : SELECT<WVRF256_0, v8i32, CC_CMU0>;

// 256-bit patterns
def : Pat<(v16i16 (shave_select WVRF256_0:$trueVal, WVRF256_0:$falseVal, timm:$cc)),
          (SELECT_WVRF256_0 WVRF256_0:$trueVal, WVRF256_0:$falseVal, i32imm:$cc)>;
def : Pat<(v32i8 (shave_select WVRF256_0:$trueVal, WVRF256_0:$falseVal, timm:$cc)),
          (SELECT_WVRF256_0 WVRF256_0:$trueVal, WVRF256_0:$falseVal, i32imm:$cc)>;

def : Pat<(v8f32 (shave_select WVRF256_0:$trueVal, WVRF256_0:$falseVal, timm:$cc)),
          (SELECT_WVRF256_0 WVRF256_0:$trueVal, WVRF256_0:$falseVal, i32imm:$cc)>;
def : Pat<(v16f16 (shave_select WVRF256_0:$trueVal, WVRF256_0:$falseVal, timm:$cc)),
          (SELECT_WVRF256_0 WVRF256_0:$trueVal, WVRF256_0:$falseVal, i32imm:$cc)>;

// 128-bit instruction
def SELECT_WVRF128_0 : SELECT<WVRF128_0, v4i32, CC_CMU0>;

// 128-bit patterns
def : Pat<(v8i16 (shave_select WVRF128_0:$trueVal, WVRF128_0:$falseVal, timm:$cc)),
          (SELECT_WVRF128_0 WVRF128_0:$trueVal, WVRF128_0:$falseVal, i32imm:$cc)>;
def : Pat<(v16i8 (shave_select WVRF128_0:$trueVal, WVRF128_0:$falseVal, timm:$cc)),
          (SELECT_WVRF128_0 WVRF128_0:$trueVal, WVRF128_0:$falseVal, i32imm:$cc)>;

def : Pat<(v4f32 (shave_select WVRF128_0:$trueVal, WVRF128_0:$falseVal, timm:$cc)),
          (SELECT_WVRF128_0 WVRF128_0:$trueVal, WVRF128_0:$falseVal, i32imm:$cc)>;
def : Pat<(v8f16 (shave_select WVRF128_0:$trueVal, WVRF128_0:$falseVal, timm:$cc)),
          (SELECT_WVRF128_0 WVRF128_0:$trueVal, WVRF128_0:$falseVal, i32imm:$cc)>;

// 64-bit instruction
def SELECT_WVRF64_0 : SELECT<WVRF64_0, v2i32, CC_CMU0>;

// 64-bit patterns
def : Pat<(v4i16 (shave_select WVRF64_0:$trueVal, WVRF64_0:$falseVal, timm:$cc)),
          (SELECT_WVRF64_0 WVRF64_0:$trueVal, WVRF64_0:$falseVal, i32imm:$cc)>;
def : Pat<(v8i8 (shave_select WVRF64_0:$trueVal, WVRF64_0:$falseVal, timm:$cc)),
          (SELECT_WVRF64_0 WVRF64_0:$trueVal, WVRF64_0:$falseVal, i32imm:$cc)>;

def : Pat<(v2f32 (shave_select WVRF64_0:$trueVal, WVRF64_0:$falseVal, timm:$cc)),
          (SELECT_WVRF64_0 WVRF64_0:$trueVal, WVRF64_0:$falseVal, i32imm:$cc)>;
def : Pat<(v4f16 (shave_select WVRF64_0:$trueVal, WVRF64_0:$falseVal, timm:$cc)),
          (SELECT_WVRF64_0 WVRF64_0:$trueVal, WVRF64_0:$falseVal, i32imm:$cc)>;

//
// Vselect pseudo instructions for WVRFx vectors
//

let Predicates = [HasVRF512_Predicate] in {
  // 512-bit vectors
  def VSELECT_WVRF512_v16   : VSELECT<WVRF512,   v16i32, C_CMU_0_3>;
  def VSELECT_WVRF512_v32   : VSELECT<WVRF512,   v32i16, C_CMU0>;
  def VSELECT_WVRF512_v64   : VSELECT<WVRF512,   v64i8,  C_CMU_0_15>;

  // 256-bit vectors
  def VSELECT_WVRF256_0_v8  : VSELECT<WVRF256_0, v8i32,  C_CMU_0_3>;
  def VSELECT_WVRF256_0_v16 : VSELECT<WVRF256_0, v16i16, C_CMU0>;
  def VSELECT_WVRF256_0_v32 : VSELECT<WVRF256_0, v32i8,  C_CMU_0_15>;

  // 128-bit vectors
  def VSELECT_WVRF128_0_v4  : VSELECT<WVRF128_0, v4i32,  C_CMU_0_3>;
  def VSELECT_WVRF128_0_v8  : VSELECT<WVRF128_0, v8i16,  C_CMU0>;
  def VSELECT_WVRF128_0_v16 : VSELECT<WVRF128_0, v16i8,  C_CMU_0_15>;

  // 64-bit vectors
  def VSELECT_WVRF64_0_v2   : VSELECT<WVRF64_0,  v2i32,  C_CMU_0_3>;
  def VSELECT_WVRF64_0_v4   : VSELECT<WVRF64_0,  v4i16,  C_CMU0>;
  def VSELECT_WVRF64_0_v8   : VSELECT<WVRF64_0,  v8i8,   C_CMU_0_15>;
}

let Predicates = [HasVRF512_Predicate] in {
  // 512-bit floating-point vector patterns
  def : Pat<(v16f32 (shave_vselect WVRF512:$trueVal, WVRF512:$falseVal, timm:$cc)),
            (VSELECT_WVRF512_v16 WVRF512:$trueVal, WVRF512:$falseVal, imm:$cc)>;
  def : Pat<(v32f16 (shave_vselect WVRF512:$trueVal, WVRF512:$falseVal, timm:$cc)),
            (VSELECT_WVRF512_v32 WVRF512:$trueVal, WVRF512:$falseVal, imm:$cc)>;

  // 256-bit floating-point vector patterns
  def : Pat<(v8f32 (shave_vselect WVRF256_0:$trueVal, WVRF256_0:$falseVal, timm:$cc)),
            (VSELECT_WVRF256_0_v8 WVRF256_0:$trueVal, WVRF256_0:$falseVal, imm:$cc)>;
  def : Pat<(v16f16 (shave_vselect WVRF256_0:$trueVal, WVRF256_0:$falseVal, timm:$cc)),
            (VSELECT_WVRF256_0_v16 WVRF256_0:$trueVal, WVRF256_0:$falseVal, imm:$cc)>;

  // 128-bit floating-point vector patterns
  def : Pat<(v4f32 (shave_vselect WVRF128_0:$trueVal, WVRF128_0:$falseVal, timm:$cc)),
            (VSELECT_WVRF128_0_v4 WVRF128_0:$trueVal, WVRF128_0:$falseVal, imm:$cc)>;
  def : Pat<(v8f16 (shave_vselect WVRF128_0:$trueVal, WVRF128_0:$falseVal, timm:$cc)),
            (VSELECT_WVRF128_0_v8 WVRF128_0:$trueVal, WVRF128_0:$falseVal, imm:$cc)>;

  // 64-bit floating-point vector patterns
  def : Pat<(v2f32 (shave_vselect WVRF64_0:$trueVal, WVRF64_0:$falseVal, timm:$cc)),
            (VSELECT_WVRF64_0_v2 WVRF64_0:$trueVal, WVRF64_0:$falseVal, imm:$cc)>;
  def : Pat<(v4f16 (shave_vselect WVRF64_0:$trueVal, WVRF64_0:$falseVal, timm:$cc)),
            (VSELECT_WVRF64_0_v4 WVRF64_0:$trueVal, WVRF64_0:$falseVal, imm:$cc)>;
}

let Predicates = [HasVRF512_Predicate] in {
  def : Pat<(i16 (vecreduce_add (v32i16 WVRF512:$source))),
            (CMU_CPII_32_16_trunc (SAU_SUMX_i16_VRF512 WVRF512:$source))>;
  def : Pat<(i8 (vecreduce_add (v64i8 WVRF512:$source))),
            (CMU_CPII_32_8_trunc (SAU_SUMX_i8_VRF512 WVRF512:$source))>;

  def : Pat<(f32 (vecreduce_fadd (v16f32 WVRF512:$source))),
            (SAU_ADD_f32 (SAU_SUMFX_v16f32_Pat_Myr4 WVRF512:$source, 0),
                         (SAU_ADD_f32 (SAU_SUMFX_v16f32_Pat_Myr4 WVRF512:$source, 1),
                                      (SAU_ADD_f32 (SAU_SUMFX_v16f32_Pat_Myr4 WVRF512:$source, 2),
                                                   (SAU_SUMFX_v16f32_Pat_Myr4 WVRF512:$source, 3))))>;

  def : Pat<(f32 (vecreduce_fadd (v8f32 WVRF256_0:$source))),
            (SAU_ADD_f32 (SAU_SUMFX_v8f32_Pat_Myr4 WVRF256_0:$source, 0),
                         (SAU_SUMFX_v8f32_Pat_Myr4 WVRF256_0:$source, 1))>;

  def : Pat<(f32 (vecreduce_fadd (v4f32 WVRF128_0:$source))),
            (SAU_SUMFX_v4f32_Pat_Myr4 WVRF128_0:$source, 0)>;

  def : Pat<(f16 (vecreduce_fadd (v32f16 WVRF512:$source))),
            (SAU_ADD_f16 (SAU_SUMFX_v32f16_Pat_Myr4 WVRF512:$source, 0),
                         (SAU_ADD_f16 (SAU_SUMFX_v32f16_Pat_Myr4 WVRF512:$source, 1),
                                      (SAU_ADD_f16 (SAU_SUMFX_v32f16_Pat_Myr4 WVRF512:$source, 2),
                                                   (SAU_SUMFX_v32f16_Pat_Myr4 WVRF512:$source, 3))))>;

  def : Pat<(f16 (vecreduce_fadd (v16f16 WVRF256_0:$source))),
            (SAU_ADD_f16 (SAU_SUMFX_v16f16_Pat_Myr4 WVRF256_0:$source, 0),
                         (SAU_SUMFX_v16f16_Pat_Myr4 WVRF256_0:$source, 1))>;

  def : Pat<(f16 (vecreduce_fadd (v8f16 WVRF128_0:$source))),
            (SAU_SUMFX_v8f16_Pat_Myr4 WVRF128_0:$source, 0)>;
}

let Predicates = [HasSUMFX512_Predicate] in {
  def : Pat<(f16 (vecreduce_fadd (v32f16 WVRF512:$source))),
            (CMU_CPII_f32_f16 (SAU_SUMFX_f16_f32_Pat_Myr4 WVRF512:$source))>;
}
