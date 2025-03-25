//===-- SHAVEv3InstrInfo_LSU.td - LSU Instructions ---------*- tablegen -*-===//
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

//
// Pattern Fragments
//

// Floating-point extending loads
def extloadv16f16 : PatFrag<(ops node:$ptr), (extload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v16f16;
}]>;

// Integer any-extending loads
def extloadv16i16 : PatFrag<(ops node:$ptr), (extload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v16i16;
}]>;

def extloadv32i8 : PatFrag<(ops node:$ptr), (extload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v32i8;
}]>;

def extloadv16i8 : PatFrag<(ops node:$ptr), (extload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v16i8;
}]>;

// Integer sign-extending loads
def sextloadv16i16 : PatFrag<(ops node:$ptr), (sextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v16i16;
}]>;

def sextloadv32i8 : PatFrag<(ops node:$ptr), (sextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v32i8;
}]>;

def sextloadv16i8 : PatFrag<(ops node:$ptr), (sextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v16i8;
}]>;

// Integer zero-extending loads
def zextloadv16i16 : PatFrag<(ops node:$ptr), (zextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v16i16;
}]>;

def zextloadv32i8 : PatFrag<(ops node:$ptr), (zextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v32i8;
}]>;

def zextloadv16i8 : PatFrag<(ops node:$ptr), (zextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v16i8;
}]>;

let Predicates = [HasVRF512_Predicate] in {

//
// LSU.LDED
//

class T_LSU_LD_WVRF<RegisterClass dstRC, ValueType dstVT, string asmstr,
                    Operand MEMtype, ComplexPattern ADDRtype,
                    string lane = "0", list<SHAVEConflictName> confs = []> :
  SHAVE_LSUInstr<(outs dstRC:$dst), (ins MEMtype:$addr),
                 !strconcat(".LD", asmstr, " $dst.", lane, " $addr"),
                 [(set (dstVT dstRC:$dst), (load ADDRtype:$addr))], confs>;

class T_LSU_LD_WVRF256<ValueType dstVT, string asmstr, Operand MEMtype, ComplexPattern ADDRtype, list<SHAVEConflictName> confs = []> : 
  T_LSU_LD_WVRF<WVRF256_0, dstVT, !strconcat(asmstr, ".256"), MEMtype, ADDRtype, "0", confs>;

class T_LSU_LD_WVRF128<ValueType dstVT, string asmstr,  Operand MEMtype, ComplexPattern ADDRtype, list<SHAVEConflictName> confs = []> :
  T_LSU_LD_WVRF<WVRF128_0, dstVT, !strconcat(asmstr, ".128"), MEMtype, ADDRtype, "0", confs>;

class T_LSU_LD_WVRF64< ValueType dstVT, string asmstr,  Operand MEMtype, ComplexPattern ADDRtype, list<SHAVEConflictName> confs = []> :
  T_LSU_LD_WVRF<WVRF64_0,  dstVT, !strconcat(asmstr, ".64"),  MEMtype, ADDRtype, "0", confs>;

class T_LSU_LD_WVRF32< ValueType dstVT, string asmstr,  Operand MEMtype, ComplexPattern ADDRtype, list<SHAVEConflictName> confs = []> :
  T_LSU_LD_WVRF<WVRF32_0,  dstVT, !strconcat(asmstr, ".32"),  MEMtype, ADDRtype, "0", confs>;

// 256-bit vector loads
multiclass T_LSU_LD_WVRF256x<Operand MEMtype, ComplexPattern ADDRtype, string asmstr = "", list<SHAVEConflictName> confs = []> {
  def 256_v8i32  : T_LSU_LD_WVRF256<v8i32,  asmstr, MEMtype, ADDRtype, confs>;
  def 256_v16i16 : T_LSU_LD_WVRF256<v16i16, asmstr, MEMtype, ADDRtype, confs>;
  def 256_v32i8  : T_LSU_LD_WVRF256<v32i8,  asmstr, MEMtype, ADDRtype, confs>;
  def 256_v8f32  : T_LSU_LD_WVRF256<v8f32,  asmstr, MEMtype, ADDRtype, confs>;
  def 256_v16f16 : T_LSU_LD_WVRF256<v16f16, asmstr, MEMtype, ADDRtype, confs>;
}

// 128-bit vector loads
multiclass T_LSU_LD_WVRF128x<Operand MEMtype, ComplexPattern ADDRtype, string asmstr = "", list<SHAVEConflictName> confs = []> {
  def 128_v4i32  : T_LSU_LD_WVRF128<v4i32, asmstr, MEMtype, ADDRtype, confs>;
  def 128_v8i16  : T_LSU_LD_WVRF128<v8i16, asmstr, MEMtype, ADDRtype, confs>;
  def 128_v16i8  : T_LSU_LD_WVRF128<v16i8, asmstr, MEMtype, ADDRtype, confs>;
  def 128_v4f32  : T_LSU_LD_WVRF128<v4f32, asmstr, MEMtype, ADDRtype, confs>;
  def 128_v8f16  : T_LSU_LD_WVRF128<v8f16, asmstr, MEMtype, ADDRtype, confs>;
}

// 64-bit vector loads
multiclass T_LSU_LD_WVRF64x<Operand MEMtype, ComplexPattern ADDRtype, string asmstr = "", list<SHAVEConflictName> confs = []> {
  def 64_v2i32   : T_LSU_LD_WVRF64<v2i32,  asmstr, MEMtype, ADDRtype, confs>;
  def 64_v4i16   : T_LSU_LD_WVRF64<v4i16,  asmstr, MEMtype, ADDRtype, confs>;
  def 64_v8i8    : T_LSU_LD_WVRF64<v8i8,   asmstr, MEMtype, ADDRtype, confs>;
  def 64_v2f32   : T_LSU_LD_WVRF64<v2f32,  asmstr, MEMtype, ADDRtype, confs>;
  def 64_v4f16   : T_LSU_LD_WVRF64<v4f16,  asmstr, MEMtype, ADDRtype, confs>;
}

// 32-bit vector loads
multiclass T_LSU_LD_WVRF32x<Operand MEMtype, ComplexPattern ADDRtype, string asmstr = "", list<SHAVEConflictName> confs = []> {
  def 32_v2i16   : T_LSU_LD_WVRF32<v2i16,  asmstr, MEMtype, ADDRtype, confs>;
  def 32_v4i8    : T_LSU_LD_WVRF32<v4i8,   asmstr, MEMtype, ADDRtype, confs>;
  def 32_v2f16   : T_LSU_LD_WVRF32<v2f16,  asmstr, MEMtype, ADDRtype, confs>;
}

multiclass T_LSU_LD_WVRFx<Operand MEMtype, ComplexPattern ADDRtype, string asmstr = "", list<SHAVEConflictName> confs = []> {
  defm _256 : T_LSU_LD_WVRF256x<MEMtype, ADDRtype, asmstr, confs>;
  defm _128 : T_LSU_LD_WVRF128x<MEMtype, ADDRtype, asmstr, confs>;
  defm _64  : T_LSU_LD_WVRF64x< MEMtype, ADDRtype, asmstr, confs>;
  defm _32  : T_LSU_LD_WVRF32x< MEMtype, ADDRtype, asmstr, confs>;
}

let Itinerary = II_LSU_LD_VRF, mayLoad = 1 in {
  // 512-bit vector loads
  // These instructions [LSU_LDV512_l, LSU_LDV512_h] must be emited together
  // fragment to load the low part
  def LSU_LDV512_l : SHAVE_LSUInstr<(outs WVRF512:$dst), (ins MEMr:$src),
                              ".LD.256 $dst.0 $src", [], [isLD512Low]>;

  def LSU_LDOV512_l : SHAVE_LSUInstr<(outs WVRF512:$dst), (ins MEMri:$src),
                                  ".LDO.256 $dst.0 $src", [], [isLD512Low]>;

  // fragment to load the high part
  let Constraints = "$src1 = $dst", Itinerary = II_LSU_LD128H_VRF in {
    def LSU_LDV512_h : SHAVE_LSUInstr<(outs WVRF512:$dst), (ins WVRF512:$src1, MEMr:$src),
                                      ".LD.256 $dst.1 $src", [], [isLD512High]>;

    def LSU_LDOV512_h : SHAVE_LSUInstr<(outs WVRF512:$dst), (ins WVRF512:$src1, MEMri:$src),
                                       ".LDO.256 $dst.1 $src", [], [isLD512High]>;
  }

  defm LSU_LDV : T_LSU_LD_WVRFx<MEMr, ADDRr>;
}

//
// LSU.LDO
//

let Itinerary = II_LSU_LDO_VRF, mayLoad = 1 in {
  defm LSU_LDOV_ : T_LSU_LD_WVRF256x<MEMri, ADDRri5, "O">;
  defm LSU_LDOV_ : T_LSU_LD_WVRF128x<MEMri, ADDRri4, "O">;
  defm LSU_LDOV_ : T_LSU_LD_WVRF64x< MEMri, ADDRri3, "O">;
  defm LSU_LDOV_ : T_LSU_LD_WVRF32x< MEMri, ADDRri2, "O">;
}

//
// LSU.LDXED
//

let Itinerary = II_LSU_LDX_VRF, mayLoad = 1 in {
  // 512-bit vector loads
  // These instructions [LSU_LDV512_l, LSU_LDV512_h] must be emited together
  // fragment to load the low part
  def LSU_LDXV512_l : SHAVE_LSUInstr<(outs WVRF512:$dst), (ins MEMr32r32:$src),
                                     ".LDX.256 $dst.0 $src", [], [isLD512Low, isLDXVorSTXV]>;

  // fragment to load the high part
  let Constraints = "$src1 = $dst" in {
    def LSU_LDXV512_h : SHAVE_LSUInstr<(outs WVRF512:$dst), (ins WVRF512:$src1, MEMr32r32:$src),
                                       ".LDX.256 $dst.1 $src", [], [isLD512High, isLDXVorSTXV]>;
  }

  defm LSU_LDXV : T_LSU_LD_WVRFx<MEMr32r32, ADDRr32r32, "X", [isLDXVorSTXV]>;
}

//
// LSU.LDWC
//

class T_LSU_LD_WVRFconv<RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode, string lane = ""> :
      T_LSU_LD_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (convMode (srcVT (load ADDRr:$addr))))], lane>  {
  let Itinerary = II_LSU_LD_VRF;
}

class T_LSU_LD_WVRFconv_EXTLOAD<RegisterClass dstRC, ValueType dstVT, string asmstr, PatFrag convLoad> :
      T_LSU_LD_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (convLoad ADDRr:$addr))]>  {
  let Itinerary = II_LSU_LD_VRF;
}

class T_LSU_LD_WVRFconv_Raw<RegisterClass dstRC, string asmstr, string lane = ""> :
      T_LSU_LD_Base<dstRC, asmstr, [], lane>  {
  let Itinerary = II_LSU_LD_VRF;
}

// 512-bit converting loads
def LSU_LDV512_u8_f16  : T_LSU_LD_WVRFconv<WVRF512, v32i8, v32f16, "512.u8.f16", uint_to_fp>;
def LSU_LDV512_f16_f32 : T_LSU_LD_WVRFconv<WVRF512, v16f16, v16f32, "512.f16.f32", fpextend>;

def LSU_LDV512_i16_i32 : T_LSU_LD_WVRFconv<WVRF512, v16i16, v16i32, "512.i16.i32", sext>;
def LSU_LDV512_u16_u32 : T_LSU_LD_WVRFconv<WVRF512, v16i16, v16i32, "512.u16.u32", zext>;
def LSU_LDV512_a16_a32 : T_LSU_LD_WVRFconv<WVRF512, v16i16, v16i32, "512.i16.i32", anyext>;

def LSU_LDV512_i8_i32 : T_LSU_LD_WVRFconv<WVRF512, v16i8, v16i32, "512.i8.i32", sext>;
def LSU_LDV512_u8_u32 : T_LSU_LD_WVRFconv<WVRF512, v16i8, v16i32, "512.u8.u32", zext>;
def LSU_LDV512_a8_a32 : T_LSU_LD_WVRFconv<WVRF512, v16i8, v16i32, "512.i8.i32", anyext>;

def LSU_LDV512_i8_i16 : T_LSU_LD_WVRFconv<WVRF512, v32i8, v32i16, "512.i8.i16", sext>;
def LSU_LDV512_u8_u16 : T_LSU_LD_WVRFconv<WVRF512, v32i8, v32i16, "512.u8.u16", zext>;
def LSU_LDV512_a8_a16 : T_LSU_LD_WVRFconv<WVRF512, v32i8, v32i16, "512.i8.i16", anyext>;

// 512-bit converting loads using ISD::EXTLOAD
def LSU_LDV512_f16_f32_extload : T_LSU_LD_WVRFconv_EXTLOAD<WVRF512, v16f32, "512.f16.f32", extloadv16f16>;

def LSU_LDV512_a16_a32_extload : T_LSU_LD_WVRFconv_EXTLOAD<WVRF512, v16i32, "512.i16.i32", extloadv16i16>;
def LSU_LDV512_u16_u32_extload : T_LSU_LD_WVRFconv_EXTLOAD<WVRF512, v16i32, "512.u16.u32", zextloadv16i16>;
def LSU_LDV512_i16_i32_extload : T_LSU_LD_WVRFconv_EXTLOAD<WVRF512, v16i32, "512.i16.i32", sextloadv16i16>;

def LSU_LDV512_a8_a32_extload : T_LSU_LD_WVRFconv_EXTLOAD<WVRF512, v16i32, "512.i8.i32", extloadv16i8>;
def LSU_LDV512_u8_u32_extload : T_LSU_LD_WVRFconv_EXTLOAD<WVRF512, v16i32, "512.u8.u32", zextloadv16i8>;
def LSU_LDV512_i8_i32_extload : T_LSU_LD_WVRFconv_EXTLOAD<WVRF512, v16i32, "512.i8.i32", sextloadv16i8>;

def LSU_LDV512_a8_a16_extload : T_LSU_LD_WVRFconv_EXTLOAD<WVRF512, v32i16, "512.i8.i16", extloadv32i8>;
def LSU_LDV512_u8_u16_extload : T_LSU_LD_WVRFconv_EXTLOAD<WVRF512, v32i16, "512.u8.u16", zextloadv32i8>;
def LSU_LDV512_i8_i16_extload : T_LSU_LD_WVRFconv_EXTLOAD<WVRF512, v32i16, "512.i8.i16", sextloadv32i8>;

// FIXME: Movidius_v3 - What should we do with these?
def LSU_LDV512_u8f_f16         : T_LSU_LD_WVRFconv_Raw<WVRF512, "512.u8f.f16">;
def LSU_LD512_b16_f32_Raw_VRF512 : T_LSU_LD_WVRFconv_Raw<WVRF512,   "512.b16.f32">;
def LSU_LD512_b8_f16_Raw_VRF512  : T_LSU_LD_WVRFconv_Raw<WVRF512,   "512.b8.f16">, Requires<[HasF8Float_Predicate]>;
def LSU_LD512_h8_f16_Raw_VRF512  : T_LSU_LD_WVRFconv_Raw<WVRF512,   "512.h8.f16">, Requires<[HasF8Float_Predicate]>;

def LSU_LD32_b8_f16_Raw_VRF512 : T_LSU_LD_IRF_Raw_Base<IRF32,    "32.b8.f16",  "">, Requires<[HasF8Float_Predicate]>;
def LSU_LD32_h8_f16_Raw_VRF512 : T_LSU_LD_IRF_Raw_Base<IRF32,    "32.h8.f16",  "">, Requires<[HasF8Float_Predicate]>;

// Sub-byte converting loads
def LSU_LD512_i4_i8_Raw_VRF512 : T_LSU_LD_WVRFconv_Raw<WVRF512, "512.i4.i8">;
def LSU_LD512_u4_u8_Raw_VRF512 : T_LSU_LD_WVRFconv_Raw<WVRF512, "512.u4.u8">;
//
// LSU.STED
//


class T_LSU_ST_WVRF<RegisterClass srcRC, ValueType srcVT, string asmstr,
                    Operand MEMtype, ComplexPattern ADDRtype,
                    string lane = "0", list<SHAVEConflictName> confs = []> :
  SHAVE_LSUInstr<(outs), (ins srcRC:$src, MEMtype:$addr),
                 !strconcat(".ST", asmstr, " $src.", lane, " $addr"),
                 [(store (srcVT srcRC:$src), ADDRtype:$addr)], confs>;

class T_LSU_ST_WVRF_NoPat<RegisterClass srcRC, string asmstr, Operand MEMtype, string lane = "0", list<SHAVEConflictName> confs = []> :
  SHAVE_LSUInstr<(outs), (ins srcRC:$src, MEMtype:$addr),
                 !strconcat(".ST", asmstr, " $src.", lane, " $addr"), [], confs>;

class T_LSU_ST_WVRF256<ValueType srcVT, string asmstr, Operand MEMtype, ComplexPattern ADDRtype, list<SHAVEConflictName> confs = []> :
  T_LSU_ST_WVRF<WVRF256_0, srcVT, !strconcat(asmstr, ".256"), MEMtype, ADDRtype, "0", confs>;

class T_LSU_ST_WVRF128<ValueType srcVT, string asmstr, Operand MEMtype, ComplexPattern ADDRtype, list<SHAVEConflictName> confs = []> :
  T_LSU_ST_WVRF<WVRF128_0, srcVT, !strconcat(asmstr, ".128"), MEMtype, ADDRtype, "0", confs>;

class T_LSU_ST_WVRF64<ValueType srcVT, string asmstr, Operand MEMtype, ComplexPattern ADDRtype, list<SHAVEConflictName> confs = []> :
  T_LSU_ST_WVRF<WVRF64_0,  srcVT, !strconcat(asmstr, ".64"),  MEMtype, ADDRtype, "0", confs>;

class T_LSU_ST_WVRF32<string asmstr, Operand MEMtype, list<SHAVEConflictName> confs = []> :
  T_LSU_ST_WVRF_NoPat<WVRF32_0, !strconcat(asmstr, ".32"),  MEMtype, "0", confs>;

// 256-bit vector stores
multiclass T_LSU_ST_WVRF256x<Operand MEMtype, ComplexPattern ADDRtype, string asmstr = "", list<SHAVEConflictName> confs = []> {
  def 256_v8i32  : T_LSU_ST_WVRF256<v8i32,  asmstr, MEMtype, ADDRtype, confs>;
  def 256_v16i16 : T_LSU_ST_WVRF256<v16i16, asmstr, MEMtype, ADDRtype, confs>;
  def 256_v32i8  : T_LSU_ST_WVRF256<v32i8,  asmstr, MEMtype, ADDRtype, confs>;
  def 256_v8f32  : T_LSU_ST_WVRF256<v8f32,  asmstr, MEMtype, ADDRtype, confs>;
  def 256_v16f16 : T_LSU_ST_WVRF256<v16f16, asmstr, MEMtype, ADDRtype, confs>;
}

// 128-bit vector stores
multiclass T_LSU_ST_WVRF128x<Operand MEMtype, ComplexPattern ADDRtype, string asmstr = "", list<SHAVEConflictName> confs = []> {
  def 128_v4i32  : T_LSU_ST_WVRF128<v4i32, asmstr, MEMtype, ADDRtype, confs>;
  def 128_v8i16  : T_LSU_ST_WVRF128<v8i16, asmstr, MEMtype, ADDRtype, confs>;
  def 128_v16i8  : T_LSU_ST_WVRF128<v16i8, asmstr, MEMtype, ADDRtype, confs>;
  def 128_v4f32  : T_LSU_ST_WVRF128<v4f32, asmstr, MEMtype, ADDRtype, confs>;
  def 128_v8f16  : T_LSU_ST_WVRF128<v8f16, asmstr, MEMtype, ADDRtype, confs>;
}

// 64-bit vector stores
multiclass T_LSU_ST_WVRF64x<Operand MEMtype, ComplexPattern ADDRtype, string asmstr = "", list<SHAVEConflictName> confs = []> {
  def 64_v2i32   : T_LSU_ST_WVRF64<v2i32, asmstr, MEMtype, ADDRtype, confs>;
  def 64_v4i16   : T_LSU_ST_WVRF64<v4i16, asmstr, MEMtype, ADDRtype, confs>;
  def 64_v8i8    : T_LSU_ST_WVRF64<v8i8,  asmstr, MEMtype, ADDRtype, confs>;
  def 64_v2f32   : T_LSU_ST_WVRF64<v2f32, asmstr, MEMtype, ADDRtype, confs>;
  def 64_v4f16   : T_LSU_ST_WVRF64<v4f16, asmstr, MEMtype, ADDRtype, confs>;
}

// 32-bit vector stores
multiclass T_LSU_ST_WVRF32x<Operand MEMtype, string asmstr = "", list<SHAVEConflictName> confs = []> {
  def 32_v2i16   : T_LSU_ST_WVRF32<asmstr, MEMtype, confs>;
  def 32_v4i8    : T_LSU_ST_WVRF32<asmstr, MEMtype, confs>;
  def 32_v2f16   : T_LSU_ST_WVRF32<asmstr, MEMtype, confs>;
}

multiclass T_LSU_ST_WVRFx<Operand MEMtype, ComplexPattern ADDRtype, string asmstr = "", list<SHAVEConflictName> confs = []> {
  defm _ : T_LSU_ST_WVRF256x<MEMtype, ADDRtype, asmstr, confs>;
  defm _ : T_LSU_ST_WVRF128x<MEMtype, ADDRtype, asmstr, confs>;
  defm _ : T_LSU_ST_WVRF64x< MEMtype, ADDRtype, asmstr, confs>;
  defm _ : T_LSU_ST_WVRF32x< MEMtype,           asmstr, confs>;
}

let Itinerary = II_LSU_ST_VRF, mayStore = 1 in {
  // 512-bit vector stores
  // fragment to store the low part
  let Itinerary = II_LSU_ST128L_VRF in {
    def LSU_STV512_l : SHAVE_LSUInstr<(outs), (ins WVRF512:$src, MEMr:$addr),
                                      ".ST.256 $src.0 $addr", [], [isST512Low]>;

    def LSU_STOV512_l : SHAVE_LSUInstr<(outs), (ins WVRF512:$src, MEMri:$addr),
                                       ".STO.256 $src.0 $addr", [], [isST512Low]>;
  }

  //fragment to store the high part
  def LSU_STV512_h : SHAVE_LSUInstr<(outs), (ins WVRF512:$src, MEMr:$addr),
                                    ".ST.256 $src.1 $addr", [], [isST512High]>;

  def LSU_STOV512_h : SHAVE_LSUInstr<(outs), (ins WVRF512:$src, MEMri:$addr),
                                     ".STO.256 $src.1 $addr", [], [isST512High]>;

  defm LSU_STV : T_LSU_ST_WVRFx<MEMr, ADDRr>;
}

//
// LSU.STO
//

let Itinerary = II_LSU_ST_VRF, mayStore = 1 in {
  defm LSU_STOV_ : T_LSU_ST_WVRF256x<MEMri, ADDRri5, "O">;
  defm LSU_STOV_ : T_LSU_ST_WVRF128x<MEMri, ADDRri4, "O">;
  defm LSU_STOV_ : T_LSU_ST_WVRF64x< MEMri, ADDRri3, "O">;
  defm LSU_STOV_ : T_LSU_ST_WVRF32x< MEMri, "O">; // No patterns on 32-bit variants, IRF instructions preferred
}

//
// LSU.STXED
//

let Itinerary = II_LSU_STX_VRF, mayStore = 1 in {
  // 512-bit vector stores
  def LSU_STXV512_l : SHAVE_LSUInstr<(outs), (ins WVRF512:$src, MEMr32r32:$addr),
                                     ".STX.256 $src.0 $addr", [], [isST512Low, isLDXVorSTXV]>;

  // fragment to load the high part
  def LSU_STXV512_h : SHAVE_LSUInstr<(outs), (ins WVRF512:$src, MEMr32r32:$addr),
                                     ".STX.256 $src.1 $addr", [], [isST512High, isLDXVorSTXV]>;

  defm LSU_STXV : T_LSU_ST_WVRFx<MEMr32r32, ADDRr32r32, "X", [isLDXVorSTXV]>;
}

//
// LSU.STWC
//

class T_LSU_ST_WVRFconv<RegisterClass srcRC, ValueType srcVT, string asmstr, PatFrag storeOp, string lane = ""> :
      T_LSU_ST_Base<srcRC, asmstr, lane, [(storeOp (srcVT srcRC:$src), (i32 ADDRr:$addr))]> {
  let Itinerary = II_LSU_ST_VRF;
}

class T_LSU_ST_WVRFconv_Raw<RegisterClass srcRC, string asmstr, string lane = ""> :
      T_LSU_ST_Base<srcRC, asmstr, lane, []> {
  let Itinerary = II_LSU_ST_VRF;
}

// 256-bit converting stores

let Itinerary = II_LSU_ST_VRF in {
  def LSU_STV256_f16_u8 : T_LSU_ST_Base<WVRF512, "256.f16.u8s", "", [(store (v32i8 (fp_to_uint (v32f16 WVRF512:$src))), (i32 ADDRr:$addr))]>;
}

def LSU_STV256_f32_f16 : T_LSU_ST_WVRFconv<WVRF512, v16f32, "256.f32.f16", shave_truncstore_vNf16>;
def LSU_STV256_i32_i16 : T_LSU_ST_WVRFconv<WVRF512, v16i32, "256.i32.i16", shave_truncstore_vNi16>;
def LSU_STV256_i16_i8  : T_LSU_ST_WVRFconv<WVRF512, v32i16, "256.i16.i8", shave_truncstore_vNi8>;

def LSU_STV256_u32_u16 : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.u32.u16">;
def LSU_STV256_u16_u8  : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.u16.u8">;

// Saturating variants
def LSU_STV256_i32_i16s : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.i32.i16s">;
def LSU_STV256_u32_u16s : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.u32.u16s">;
def LSU_STV256_i16_i8s  : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.i16.i8s">;
def LSU_STV256_u16_u8s  : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.u16.u8s">;
def LSU_STV256_f16_u8fs : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.f16.u8fs">;
def LSU_STV256_f16_u8s  : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.f16.u8s">;

// Sub-byte variants
def LSU_ST256_i8_i4_Raw_VRF512  : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.i8.i4">;
def LSU_ST256_u8_u4_Raw_VRF512  : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.u8.u4">;
def LSU_ST256_i8_i4s_Raw_VRF512 : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.i8.i4s">;
def LSU_ST256_u8_u4s_Raw_VRF512 : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.u8.u4s">;

def LSU_ST256_f32_b16_Raw_VRF512  : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.f32.b16">;
// Small floats
def LSU_ST256_f32_f16c_Raw_VRF512 : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.f32.f16c">;
def LSU_ST256_f16_b8_Raw_VRF512   : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.f16.b8">, Requires<[HasF8Float_Predicate]>;
def LSU_ST256_f16_b8c_Raw_VRF512  : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.f16.b8c">, Requires<[HasF8Float_Predicate]>;
def LSU_ST256_f16_h8_Raw_VRF512   : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.f16.h8">, Requires<[HasF8Float_Predicate]>;
def LSU_ST256_f16_h8c_Raw_VRF512  : T_LSU_ST_WVRFconv_Raw<WVRF512, "256.f16.h8c">, Requires<[HasF8Float_Predicate]>;

// 128-bit converting stores

def LSU_STV128_i32_i8 : T_LSU_ST_WVRFconv<WVRF512, v16i32, "128.i32.i8", shave_truncstore_vNi8>;
def LSU_STV128_u32_u8 : T_LSU_ST_WVRFconv_Raw<WVRF512, "128.u32.u8">;

// Saturating
def LSU_STV128_i32_i8s : T_LSU_ST_WVRFconv_Raw<WVRF512, "128.i32.i8s">;
def LSU_STV128_u32_u8s : T_LSU_ST_WVRFconv_Raw<WVRF512, "128.u32.u8s">;


// 16-bit converting stores
// Small floats
def LSU_ST16_f32_f16c_Raw_VRF512 : T_LSU_ST_IRFconv_Raw<IRF32, "16.f32.f16c">;
def LSU_ST16_f16_b8_Raw_VRF512   : T_LSU_ST_IRFconv_Raw<IRF32, "16.f16.b8">, Requires<[HasF8Float_Predicate]>;
def LSU_ST16_f16_b8c_Raw_VRF512  : T_LSU_ST_IRFconv_Raw<IRF32, "16.f16.b8c">, Requires<[HasF8Float_Predicate]>;
def LSU_ST16_f16_h8_Raw_VRF512   : T_LSU_ST_IRFconv_Raw<IRF32, "16.f16.h8">, Requires<[HasF8Float_Predicate]>;
def LSU_ST16_f16_h8c_Raw_VRF512  : T_LSU_ST_IRFconv_Raw<IRF32, "16.f16.h8c">, Requires<[HasF8Float_Predicate]>;

} // Predicates = [HasVRF512_Predicate]
