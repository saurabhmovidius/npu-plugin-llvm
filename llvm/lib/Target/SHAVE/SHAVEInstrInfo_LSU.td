//===-- SHAVEInstrInfo_LSU.td - LSU Instructions -----------*- tablegen -*-===//
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
// This file describes the instructions available in the SHAVE Load/Store
// Units (LSU0 and LSU1).
//
//===----------------------------------------------------------------------===//


//  32-bit patterns
def extloadv4i8 : PatFrag<(ops node:$ptr), (extload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v4i8;
}]>;

def zextloadv4i8 : PatFrag<(ops node:$ptr), (zextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v4i8;
}]>;

def sextloadv4i8 : PatFrag<(ops node:$ptr), (sextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v4i8;
}]>;

//  64-bit patterns
def extloadv2f16 : PatFrag<(ops node:$ptr), (extload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v2f16;
}]>;

//  128-bit patterns
def extloadv4f16 : PatFrag<(ops node:$ptr), (extload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v4f16;
}]>;

def sextloadv8i8 : PatFrag<(ops node:$ptr), (sextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v8i8;
}]>;

def sextloadv2i8 : PatFrag<(ops node:$ptr), (sextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v2i8;
}]>;

def zextloadv2i8 : PatFrag<(ops node:$ptr), (zextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v2i8;
}]>;

def zextloadv8i8 : PatFrag<(ops node:$ptr), (zextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v8i8;
}]>;

def extloadv8i8 : PatFrag<(ops node:$ptr), (extload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v8i8;
}]>;

def sextloadv4i16 : PatFrag<(ops node:$ptr), (sextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v4i16;
}]>;

def zextloadv4i16 : PatFrag<(ops node:$ptr), (zextload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v4i16;
}]>;

def extloadv4i16 : PatFrag<(ops node:$ptr), (extload node:$ptr), [{
  return cast<LoadSDNode>(N)->getMemoryVT() == MVT::v4i16;
}]>;

// pattern for stores with post increment
def shave_istore : PatFrag<(ops node:$val, node:$ptr, node:$offset),
  (istore node:$val, node:$ptr, node:$offset),
  [{
    ISD::MemIndexedMode AM = cast<StoreSDNode>(N)->getAddressingMode();
    return (AM == ISD::POST_INC);
  }]
>;

// pattern for stores with post decrement
def shave_dstore : PatFrag<(ops node:$val, node:$ptr, node:$offset),
  (istore node:$val, node:$ptr, node:$offset),
  [{
    ISD::MemIndexedMode AM = cast<StoreSDNode>(N)->getAddressingMode();
    return (AM == ISD::POST_DEC);
  }]
>;

// pattern for truncating stores with post-increment
def shave_itruncstore : PatFrag<(ops node:$val, node:$ptr, node:$offset),
  (itruncstore node:$val, node:$ptr, node:$offset),
  [{
    ISD::MemIndexedMode AM = cast<StoreSDNode>(N)->getAddressingMode();
    return (AM == ISD::POST_INC);
  }]
>;

// pattern for truncating store with post-decrement
def shave_dtruncstore : PatFrag<(ops node:$val, node:$ptr, node:$offset),
  (itruncstore node:$val, node:$ptr, node:$offset),
  [{
    ISD::MemIndexedMode AM = cast<StoreSDNode>(N)->getAddressingMode();
    return (AM == ISD::POST_DEC);
  }]
>;

def dtruncsti8 : PatFrag<(ops node:$val, node:$base, node:$offset),
  (shave_dtruncstore node:$val, node:$base, node:$offset),
  [{
    return cast<StoreSDNode>(N)->getMemoryVT() == MVT::i8;
  }]
>;
def dtruncsti16 : PatFrag<(ops node:$val, node:$base, node:$offset),
  (shave_dtruncstore node:$val, node:$base, node:$offset),
  [{
    return cast<StoreSDNode>(N)->getMemoryVT() == MVT::i16;
  }]
>;
def dtruncsti32 : PatFrag<(ops node:$val, node:$base, node:$offset),
(shave_dtruncstore node:$val, node:$base, node:$offset),
  [{
    return cast<StoreSDNode>(N)->getMemoryVT() == MVT::i32;
  }]
>;

def itruncsti8 : PatFrag<(ops node:$val, node:$base, node:$offset),
  (shave_itruncstore node:$val, node:$base, node:$offset),
  [{
    return cast<StoreSDNode>(N)->getMemoryVT() == MVT::i8;
  }]
>;
def itruncsti16 : PatFrag<(ops node:$val, node:$base, node:$offset),
  (shave_itruncstore node:$val, node:$base, node:$offset),
  [{
    return cast<StoreSDNode>(N)->getMemoryVT() == MVT::i16;
  }]
>;
def itruncsti32 : PatFrag<(ops node:$val, node:$base, node:$offset),
  (shave_itruncstore node:$val, node:$base, node:$offset),
  [{
    return cast<StoreSDNode>(N)->getMemoryVT() == MVT::i32;
  }]
>;

def shave_itruncstore_vector : PatFrag<(ops node:$val, node:$addr, node:$inc),
  (itruncstore node:$val, node:$addr, node:$inc),
  [{
    ISD::MemIndexedMode AM = cast<StoreSDNode>(N)->getAddressingMode();
    EVT scalarTy = cast<StoreSDNode>(N)->getMemoryVT().getScalarType();
    return ((AM == ISD::POST_INC) &&
            ((scalarTy == MVT::f16) ||
             (scalarTy == MVT::i8)  ||
             (scalarTy == MVT::i16)));
  }]
>;

def shave_truncstore_vNf16 : PatFrag<(ops node:$val, node:$addr),
  (truncstore node:$val, node:$addr),
  [{
   return cast<StoreSDNode>(N)->getMemoryVT().getScalarType() == MVT::f16;
  }]
>;

def shave_truncstore_vNi8 : PatFrag<(ops node:$val, node:$addr),
  (truncstore node:$val, node:$addr),
  [{
   return cast<StoreSDNode>(N)->getMemoryVT().getScalarType() == MVT::i8;
  }]
>;

def shave_truncstore_vNi16 : PatFrag<(ops node:$val, node:$addr),
  (truncstore node:$val, node:$addr),
  [{
    return cast<StoreSDNode>(N)->getMemoryVT().getScalarType() == MVT::i16;
  }]
>;

//===----------------------------------------------------------------------===//
// 1.5.6 LSU Instructions
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
// 1.5.6.1 - LSU.CP
//    Copy value from one RF location to another.
//
//    LSU.CP - these have no patterns and will be selected (mutated from CMU.CP*)
//    by the scheduler.
//

class LSU_CP_irf32_irf32<RegisterClass dstRegVT, RegisterClass srcRegVT> :
  SHAVE_LSUInstr<(outs dstRegVT:$dst), (ins srcRegVT:$src), ".CP.32 $dst $src">;
class LSU_CP<RegisterClass dstRegVT, RegisterClass srcRegVT> :
  SHAVE_LSUInstr<(outs dstRegVT:$dst), (ins srcRegVT:$src), ".CP.128 $dst $src">;

class LSU_CPED_vrf_vrf<string asmstr> :
  SHAVE_LSUInstr<(outs VRF128:$dst), (ins VRF128:$dsrc, VRF128:$src, i8imm:$srcPos, i8imm:$dstPos), !strconcat(asmstr, " $dst.$dstPos $src.$srcPos")>;

// Class matching the arguments for CMU_CPVV_element for the definitions in SHAVEInstrInfo_Mutating.td
class LSU_CPVV_element<RegisterClass RC, string asmstr> :
   SHAVE_LSUInstr<(outs RC:$dst), (ins RC:$vsrc, RC:$src, i8imm:$dstElement, i8imm:$srcElement),
                  !strconcat(asmstr," $dst.$dstElement $src.$srcElement"), []> {
  let Constraints = "$vsrc = $dst";
}

// IRF -> IRF
let Itinerary = II_LSU_CP_IRF_IRF in {
  def LSU_CP_irf_irf     : LSU_CP_irf32_irf32<IRF32, IRF32>;
  def LSU_CP_irf16_irf16 : LSU_CP_irf32_irf32<IRF16_l, IRF16_l>;
  def LSU_CP_irf8_irf8   : LSU_CP_irf32_irf32<IRF8_q0, IRF8_q0>;
}

// VRF -> VRF
let Itinerary = II_LSU_CP_VRF_VRF in {
  def LSU_CP_vrf_vrf     : LSU_CP<VRF128, VRF128>;
  def LSU_CP_vrf64_vrf64 : LSU_CP<VRF64_l, VRF64_l>;

  // Vector extending copies (equivalent to CMU_CPVV variants used by SHAVEInstrInfo::ExpandVECTOR_EXTEND)
  def LSU_CP_vrf_vrf64   : LSU_CP<VRF128, VRF64_l>;
  def LSU_CP_vrf_vrf32   : LSU_CP<VRF128, VRF32_q0>;
  def LSU_CP_vrf_vrf16   : LSU_CP<VRF128, VRF16_e0>;

  def LSU_CPED_vrf_vrf_x32 : LSU_CPED_vrf_vrf<".CP.32">;
  def LSU_CPED_vrf_vrf_x16 : LSU_CPED_vrf_vrf<".CP.16">;
  def LSU_CPED_vrf_vrf_x8  : LSU_CPED_vrf_vrf<".CP.8">;
}

// VRF -> IRF
class LSU_CP_irf_vrf<RegisterClass dstRegVT, RegisterClass srcRegVT, string cpStr=".CP.32", string lane=""> :
  SHAVE_LSUInstr<(outs dstRegVT:$dst), (ins srcRegVT:$src, i8imm:$pos),
                  !strconcat(cpStr," $dst", lane, " $src.$pos")>;

let Itinerary = II_LSU_CP_IRF_VRF in {
  def LSU_CP_irf_vrf   : LSU_CP_irf_vrf<IRF32, VRF128>;
  def LSU_CP_irf_vrf64 : LSU_CP_irf_vrf<IRF32, VRF64_l>;
  def LSU_CP_irf_vrf32 : LSU_CP_irf_vrf<IRF32, VRF32_q0>;

  def LSU_CP16_irf0_vrf   : LSU_CP_irf_vrf<IRF16_l, VRF128, ".CP.16", ".0">;
}

// IRF -> VRF
class LSU_CP_vrf_irf<RegisterClass dstRegVT, RegisterClass srcRegVT, string cpStr=".CP.32", string lane=""> :
  SHAVE_LSUInstr<(outs dstRegVT:$dst), (ins dstRegVT:$vsrc, srcRegVT:$src, i8imm:$pos),
                  !strconcat(cpStr," $dst.$pos $src",lane)>;

// Interface for LSU_CP_vrf_irf which matches the arguments expected by CMU_CP_insert for the definitions in
// SHAVEInstrInfo_Mutating.td
class LSU_CP_insert<RegisterClass dstRC, RegisterClass srcRC, string cpStr, string lane> :
  LSU_CP_vrf_irf<dstRC, srcRC, !strconcat(".CP", cpStr), lane>
{
  let Constraints = "$vsrc = $dst";
}

let Itinerary = II_LSU_CP_VRF_IRF, Constraints = "$vsrc = $dst" in {
  def LSU_CP_vrf_irf   : LSU_CP_vrf_irf<VRF128, IRF32>;
  def LSU_CP_vrf64_irf : LSU_CP_vrf_irf<VRF64_l, IRF32>;
  def LSU_CP_vrf32_irf : LSU_CP_vrf_irf<VRF32_q0, IRF32>;

  def LSU_CP16_vrf_irf0   : LSU_CP_vrf_irf<VRF128, IRF32, ".CP.16", ".0">;
}

//
//===----------------------------------------------------------------------===//
//
// 1.5.6.2 - LSU.LD
//    Register indirect load of specified type from memory.
//
class T_LSU_LD_Base<RegisterClass dstRC, string asmstr, list<dag> pattern, string lane = "", list<SHAVEConflictName> conflicts = []> :
      SHAVE_LSUInstr<(outs dstRC:$dst),
                     (ins MEMr:$addr),
                     !strconcat(".LD.", asmstr, " $dst", lane, " $addr"),
                     pattern, conflicts> {
  let mayLoad = 1;
}

//    Specialise for VRF itineraries
class T_LSU_LD_VRF_Raw_Base<RegisterClass dstRC, string asmstr, string lane> :
      T_LSU_LD_Base<dstRC, asmstr, [], lane> {
  let Itinerary = II_LSU_LD_VRF;
}
multiclass T_LSU_LD_VRF_Raw<RegisterClass dstRC, string asmstr, string lane = ""> {
  def  _Raw   : T_LSU_LD_VRF_Raw_Base<dstRC, asmstr, lane>;
}

class T_LSU_LD_VRF<RegisterClass dstRC, ValueType dstVT, string asmstr, PatFrag loadfrag = load, string lane = "", list<Predicate> requires = []> :
      T_LSU_LD_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (loadfrag ADDRr:$addr))], lane>, Requires<!listconcat(requires, [HasVRF128_Predicate])> {
  let Itinerary = II_LSU_LD_VRF;
}

class T_LSU_LD_VRFconv<RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode, string lane = "", list<Predicate> requires = []> :
      T_LSU_LD_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (convMode (srcVT (load ADDRr:$addr))))], lane>, Requires<!listconcat(requires, [HasVRF128_Predicate])>  {
  let Itinerary = II_LSU_LD_VRF;
}

//    Specialise for 32-bit IRF itineraries
class T_LSU_LD_IRF_Raw_Base<RegisterClass dstRC, string asmstr, string lane = "", list<SHAVEConflictName> conflicts = []> :
      T_LSU_LD_Base<dstRC, asmstr, [], lane, conflicts> {
  let Itinerary = II_LSU_LD_IRF;
}
multiclass T_LSU_LD_IRF_Raw<RegisterClass dstRC, string asmstr, string lane = "", list<SHAVEConflictName> conflicts = []> {
  def _Raw : T_LSU_LD_IRF_Raw_Base<dstRC, asmstr, lane, conflicts>;
}

class T_LSU_LD_IRF<RegisterClass dstRC, ValueType dstVT, string asmstr, PatFrag loadfrag = load, string lane = ""> :
      T_LSU_LD_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (loadfrag ADDRr:$addr))], lane> {
  let Itinerary = II_LSU_LD_IRF;
}

class T_LSU_LD_IRFconv<RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode, string lane = ""> :
      T_LSU_LD_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (convMode (srcVT (load ADDRr:$addr))))], lane> {
  let Itinerary = II_LSU_LD_IRF;
}

//    Specialise for 64-bit IRF itineraries
class T_LSU_LD_IRF64_Raw_Base<RegisterClass dstRC, string asmstr, list<SHAVEConflictName> conflicts = []> :
      T_LSU_LD_Base<dstRC, asmstr, [], "", conflicts> {
  let Itinerary = II_LSU_LD_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}
multiclass T_LSU_LD_IRF64_Raw<RegisterClass dstRC, string asmstr, list<SHAVEConflictName> conflicts = []> {
  def  _Raw : T_LSU_LD_IRF64_Raw_Base<dstRC, asmstr, conflicts>;
}

class T_LSU_LD_IRF64<RegisterClass dstRC, ValueType dstVT, string asmstr> :
      T_LSU_LD_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (load ADDRr:$addr))], ""> {
  let Itinerary = II_LSU_LD_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}

class T_LSU_LD_IRF64conv<RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode> :
      T_LSU_LD_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (convMode (srcVT (load ADDRr:$addr))))], ""> {
  let Itinerary = II_LSU_LD_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}


// Vector loads
// ======
// 64-bit LD.64.l and LD.64.h loads
defm LSU_LD64_h            : T_LSU_LD_VRF_Raw<   VRF64_h,                "64",          ".1">;

def  LSU_LD_v2i32_h        : T_LSU_LD_VRF<       VRF64_h,         v2i32, "64",    load, ".1">;
def  LSU_LD_v4i16_h        : T_LSU_LD_VRF<       VRF64_h,         v4i16, "64",    load, ".1">;
def  LSU_LD_v8i8_h         : T_LSU_LD_VRF<       VRF64_h,         v8i8,  "64",    load, ".1">;

def  LSU_LD_v2f32_h        : T_LSU_LD_VRF<       VRF64_h,         v2f32, "64",    load, ".1">;
def  LSU_LD_v4f16_h        : T_LSU_LD_VRF<       VRF64_h,         v4f16, "64",    load, ".1">;

defm LSU_LD64_l            : T_LSU_LD_VRF_Raw<   VRF64_l,                "64",          ".0">;

def  LSU_LD_v2i32          : T_LSU_LD_VRF<       VRF64_l,         v2i32, "64",    load, ".0">;
def  LSU_LD_v4i16          : T_LSU_LD_VRF<       VRF64_l,         v4i16, "64",    load, ".0">;
def  LSU_LD_v8i8           : T_LSU_LD_VRF<       VRF64_l,         v8i8,  "64",    load, ".0">;

def  LSU_LD_v2f32          : T_LSU_LD_VRF<       VRF64_l,         v2f32, "64",    load, ".0">;
def  LSU_LD_v4f16          : T_LSU_LD_VRF<       VRF64_l,         v4f16, "64",    load, ".0">;

// ======
// Unsigned 8-bit to 16-bit FP converting LD.128.u8.f16 loads
defm LSU_LD128_u8_f16      : T_LSU_LD_VRF_Raw<   VRF128,                 "128.u8.f16">;

def  LSU_LD_v8u8_v8f16     : T_LSU_LD_VRFconv<   VRF128,   v8i8,  v8f16, "128.u8.f16",  uint_to_fp>;

// ======
// U8F to 16-bit FP converting LD.128.u8f.f16 loads
defm LSU_LD128_u8f_f16     : T_LSU_LD_VRF_Raw<   VRF128,                 "128.u8f.f16">;

let Itinerary = II_LSU_LD_VRF in {
def LSU_LD128_u8f_f16      : T_LSU_LD_Base<VRF128, "128.u8f.f16", [(set (v8f16 VRF128:$dst), (fdiv (uint_to_fp (v8i8 (load ADDRr:$addr))), (v8f16 (splat (f16 u8fConst)))))]>;
}

// ======
// F16 to F32 extending 128-bit loads
defm LSU_LD128_f16_f32     : T_LSU_LD_VRF_Raw<   VRF128,                 "128.f16.f32">;

def  LSU_LD_v4f16_v4f32    : T_LSU_LD_VRF<       VRF128,          v4f32, "128.f16.f32", extload>;

// ======
// NPU2.6 Bfloat16 loads
def LSU_LD128_b16_f32_Raw             : T_LSU_LD_VRF_Raw_Base<   VRF128,  "128.b16.f32", "">, Requires<[HasB16Float_Predicate, HasVRF128_Predicate]>;
def LSU_LD32_b16_f32_Raw              : T_LSU_LD_IRF_Raw_Base<   IRF32,    "32.b16.f32",  "">, Requires<[HasB16Float_Predicate]>;

// Integer extending loads
defm LSU_LD128_i8_i16      : T_LSU_LD_VRF_Raw<   VRF128,                 "128.i8.i16">;
defm LSU_LD128_u8_u16      : T_LSU_LD_VRF_Raw<   VRF128,                 "128.u8.u16">;
defm LSU_LD128_i16_i32     : T_LSU_LD_VRF_Raw<   VRF128,                 "128.i16.i32">;
defm LSU_LD128_u16_u32     : T_LSU_LD_VRF_Raw<   VRF128,                 "128.u16.u32">;

def  LSU_LD_v8a8_v8a16     : T_LSU_LD_VRFconv<   VRF128,   v8i8,  v8i16, "128.i8.i16",  anyext>;
def  LSU_LD_v8s8_v8s16     : T_LSU_LD_VRFconv<   VRF128,   v8i8,  v8i16, "128.i8.i16",  sext>;
def  LSU_LD_v8u8_v8u16     : T_LSU_LD_VRFconv<   VRF128,   v8i8,  v8i16, "128.u8.u16",  zext>;

def  LSU_LD_v4a16_v4a32    : T_LSU_LD_VRFconv<   VRF128,   v4i16, v4i32, "128.i16.i32", anyext>;
def  LSU_LD_v4s16_v4s32    : T_LSU_LD_VRFconv<   VRF128,   v4i16, v4i32, "128.i16.i32", sext>;
def  LSU_LD_v4u16_v4u32    : T_LSU_LD_VRFconv<   VRF128,   v4i16, v4i32, "128.u16.u32", zext>;

//  For Myriad2.3, we generate extLoad
def  LSU_ExtLD_v2s8_v2s16_Myr2v3     : T_LSU_LD_IRF<   IRF32,   v2i16, "32.i8.i16",  sextloadv2i8>;
def  LSU_ExtLD_v2u8_v2u16_Myr2v3     : T_LSU_LD_IRF<   IRF32,   v2i16, "32.i8.i16",  zextloadv2i8>;

def  LSU_ExtLD_v8a8_v8a16_Myr2v3     : T_LSU_LD_VRF<   VRF128,   v8i16, "128.i8.i16",  extloadv8i8>;
def  LSU_ExtLD_v8s8_v8s16_Myr2v3     : T_LSU_LD_VRF<   VRF128,   v8i16, "128.i8.i16",  sextloadv8i8>;
def  LSU_ExtLD_v8u8_v8u16_Myr2v3     : T_LSU_LD_VRF<   VRF128,   v8i16, "128.u8.u16",  zextloadv8i8>;

def  LSU_ExtLD_v4a8_v4a32_Myr2v3     : T_LSU_LD_VRF<   VRF128,   v4i32, "128.i8.i32",  extloadv4i8,  "", [HasVRF128_Predicate]>;
def  LSU_ExtLD_v4s8_v4s32_Myr2v3     : T_LSU_LD_VRF<   VRF128,   v4i32, "128.i8.i32",  sextloadv4i8, "", [HasVRF128_Predicate]>;
def  LSU_ExtLD_v4u8_v4u32_Myr2v3     : T_LSU_LD_VRF<   VRF128,   v4i32, "128.u8.u32",  zextloadv4i8, "", [HasVRF128_Predicate]>;

def  LSU_ExtLD_v4a16_v4a32_Myr2v3    : T_LSU_LD_VRF<   VRF128,   v4i32, "128.i16.i32", extloadv4i16>;
def  LSU_ExtLD_v4s16_v4s32_Myr2v3    : T_LSU_LD_VRF<   VRF128,   v4i32, "128.i16.i32", sextloadv4i16>;
def  LSU_ExtLD_v4u16_v4u32_Myr2v3    : T_LSU_LD_VRF<   VRF128,   v4i32, "128.u16.u32", zextloadv4i16>;

// ======
// Splat loads
defm LSU_LD32r             : T_LSU_LD_VRF_Raw<   VRF128,                 "128.32.r">;
defm LSU_LD16r             : T_LSU_LD_VRF_Raw<   VRF128,                 "128.16.r">;
defm LSU_LD8r              : T_LSU_LD_VRF_Raw<   VRF128,                 "128.8.r">;

def  LSU_LD_i32_v4i32      : T_LSU_LD_VRFconv<   VRF128,   i32,   v4i32, "128.32.r",        splat>;
def  LSU_LD_i16_v8i16      : T_LSU_LD_VRFconv<   VRF128,   i16,   v8i16, "128.16.r",        splat>;
def  LSU_LD_i8_v16i8       : T_LSU_LD_VRFconv<   VRF128,   i8,    v16i8, "128.8.r",         splat>;

def  LSU_LD_f32_v4f32      : T_LSU_LD_VRFconv<   VRF128,   f32,   v4f32, "128.32.r",        splat>;
def  LSU_LD_f16_v8f16      : T_LSU_LD_VRFconv<   VRF128,   f16,   v8f16, "128.16.r",        splat>;

//    These actually write 128-bits, but we aren't concerned about the upper bits
def  LSU_LD_i32_v2i32      : T_LSU_LD_VRFconv<   VRF64_l,  i32,   v2i32, "128.32.r",        splat>;
def  LSU_LD_i16_v4i16      : T_LSU_LD_VRFconv<   VRF64_l,  i16,   v4i16, "128.16.r",        splat>;
def  LSU_LD_i8_v8i8        : T_LSU_LD_VRFconv<   VRF64_l,  i8,    v8i8,  "128.8.r",         splat>;

def  LSU_LD_f32_v2f32      : T_LSU_LD_VRFconv<   VRF64_l,  f32,   v2f32, "128.32.r",        splat>;
def  LSU_LD_f16_v4f16      : T_LSU_LD_VRFconv<   VRF64_l,  f16,   v4f16, "128.16.r",        splat>;

//    These actually write 128-bits, but we aren't concerned about the upper bits
def  LSU_LD_i16_v2i16_VRF  : T_LSU_LD_VRFconv<   VRF32_q0, i16,   v2i16, "128.16.r",        splat>;
def  LSU_LD_i8_v4i8_VRF    : T_LSU_LD_VRFconv<   VRF32_q0, i8,    v4i8,  "128.8.r",         splat>;

def  LSU_LD_f16_v2f16_VRF  : T_LSU_LD_VRFconv<   VRF32_q0, f16,   v2f16, "128.16.r",        splat>;

//    These actually write 128-bits, but we aren't concerned about the upper bits
def  LSU_LD_v2i16_VRF      : T_LSU_LD_VRF<       VRF32_q0,        v2i16, "128.32.r",        load>;
def  LSU_LD_v4i8_VRF       : T_LSU_LD_VRF<       VRF32_q0,        v4i8,  "128.32.r",        load>;

def  LSU_LD_v2f16_VRF      : T_LSU_LD_VRF<       VRF32_q0,        v2f16, "128.32.r",        load>;

// Scalar loads
// ======
// 64-bit scalar loads
defm LSU_LD64              : T_LSU_LD_IRF64_Raw< IRF64,                  "64">;

def  LSU_LD_i64            : T_LSU_LD_IRF64<     IRF64,           i64,   "64">;

// ======
// Non-converting loads
//    8, 16 and 32-bit scalar
defm LSU_LD8               : T_LSU_LD_IRF_Raw<   IRF8_q0,                "8",  ".0">;
defm LSU_LD16              : T_LSU_LD_IRF_Raw<   IRF16_l,                "16", ".0">;
defm LSU_LD32              : T_LSU_LD_IRF_Raw<   IRF32,                  "32">;

def  LSU_LD_i8             : T_LSU_LD_IRF<       IRF8_q0,         i8,    "8",           load,       ".0">;
def  LSU_LD_i16            : T_LSU_LD_IRF<       IRF16_l,         i16,   "16",          load,       ".0">;
def  LSU_LD_i32            : T_LSU_LD_IRF<       IRF32,           i32,   "32">;

def  LSU_LD_f16            : T_LSU_LD_IRF<       IRF16_l,         f16,   "16",          load,       ".0">;
def  LSU_LD_f32            : T_LSU_LD_IRF<       IRF32,           f32,   "32">;
//    32-bit small vector
def  LSU_LD_v4i8           : T_LSU_LD_IRF<       IRF32,           v4i8,  "32">;
def  LSU_LD_v2i16          : T_LSU_LD_IRF<       IRF32,           v2i16, "32">;

def  LSU_LD_v2f16          : T_LSU_LD_IRF<       IRF32,           v2f16, "32">;
//    16-bit small vector
def  LSU_LD_v2i8           : T_LSU_LD_IRF<       IRF16_l,         v2i8,  "16",          load,       ".0">;

// Converting loads
//    8, 16 and 32-bit scalar
def  LSU_LD_a16_a32        : T_LSU_LD_IRFconv<   IRF32,    i16,   i32,   "16",          anyext,     ".0">;
def  LSU_LD_a8_a32         : T_LSU_LD_IRFconv<   IRF32,    i8,    i32,   "8",           anyext,     ".0">;
def  LSU_LD_a8_a16         : T_LSU_LD_IRFconv<   IRF16_l,  i8,    i16,   "8",           anyext,     ".0">;

defm LSU_LD32_i16_i32      : T_LSU_LD_IRF_Raw<   IRF32,                  "32.i16.i32">;
defm LSU_LD32_i8_i32       : T_LSU_LD_IRF_Raw<   IRF32,                  "32.i8.i32">;

def  LSU_LD_s16_s32        : T_LSU_LD_IRFconv<   IRF32,    i16,   i32,   "32.i16.i32",  sext>;
def  LSU_LD_s8_s32         : T_LSU_LD_IRFconv<   IRF32,    i8,    i32,   "32.i8.i32",   sext>;
def  LSU_LD_s8_s16         : T_LSU_LD_IRFconv<   IRF16_l,  i8,    i16,   "32.i8.i32",   sext>;   // This overwrites the upper 16-bits, but we don't care about them

def  LSU_ExtLD_s16_s32_Myr2v3        : T_LSU_LD_IRF<   IRF32,    i32,   "32.i16.i32",  sextloadi16>;
def  LSU_ExtLD_s8_s32_Myr2v3         : T_LSU_LD_IRF<   IRF32,    i32,   "32.i8.i32",   sextloadi8>;

defm LSU_LD32_u16_u32      : T_LSU_LD_IRF_Raw<   IRF32,                  "32.u16.u32">;
defm LSU_LD32_u8_u32       : T_LSU_LD_IRF_Raw<   IRF32,                  "32.u8.u32">;

def  LSU_LD_u16_u32        : T_LSU_LD_IRFconv<   IRF32,    i16,   i32,   "32.u16.u32",  zext>;
def  LSU_LD_u8_u32         : T_LSU_LD_IRFconv<   IRF32,    i8,    i32,   "32.u8.u32",   zext>;
def  LSU_LD_u8_u16         : T_LSU_LD_IRFconv<   IRF16_l,  i8,    i16,   "32.u8.u32",   zext>;   // This overwrites the upper 16-bits, but we don't care about them

def  LSU_ExtLD_u16_u32_Myr2v3: T_LSU_LD_IRF<   IRF32,    i32,   "32.u16.u32",  zextloadi16>;
def  LSU_ExtLD_u8_u32_Myr2v3 : T_LSU_LD_IRF<   IRF32,    i32,   "32.u8.u32",   zextloadi8>;

// Myriad2.3 new converting loads
def  LSU_LD_f16_f32        : T_LSU_LD_IRF<   IRF32,    f32,   "32.f16.f32",  extload>;

class T_LSU_LD_IRFconv_Raw<RegisterClass dstRC, string asmstr, string lane = ""> :
      T_LSU_LD_Base<dstRC, asmstr, [], lane>  {
  let Itinerary = II_LSU_LD_IRF;
}

// Sub-byte converting loads
def LSU_LD32_i4_i8_Raw : T_LSU_LD_IRFconv_Raw<IRF32, "32.i4.i8">;
def LSU_LD32_u4_u8_Raw : T_LSU_LD_IRFconv_Raw<IRF32, "32.u4.u8">;

// Reinterpretting loads
// ======
//    64-bit reinterpret loads
def  LSU_LD_v2i32_as_v2f32 : T_LSU_LD_VRFconv<   VRF64_l,  v2i32, v2f32, "64",    bitconvert, ".0">;
def  LSU_LD_v2i32_as_v4i16 : T_LSU_LD_VRFconv<   VRF64_l,  v2i32, v4i16, "64",    bitconvert, ".0">;
def  LSU_LD_v2i32_as_v4f16 : T_LSU_LD_VRFconv<   VRF64_l,  v2i32, v4f16, "64",    bitconvert, ".0">;
def  LSU_LD_v2i32_as_v8i8  : T_LSU_LD_VRFconv<   VRF64_l,  v2i32, v8i8,  "64",    bitconvert, ".0">;

def  LSU_LD_v2f32_as_v2i32 : T_LSU_LD_VRFconv<   VRF64_l,  v2f32, v2i32, "64",    bitconvert, ".0">;
def  LSU_LD_v2f32_as_v4i16 : T_LSU_LD_VRFconv<   VRF64_l,  v2f32, v4i16, "64",    bitconvert, ".0">;
def  LSU_LD_v2f32_as_v4f16 : T_LSU_LD_VRFconv<   VRF64_l,  v2f32, v4f16, "64",    bitconvert, ".0">;
def  LSU_LD_v2f32_as_v8i8  : T_LSU_LD_VRFconv<   VRF64_l,  v2f32, v8i8,  "64",    bitconvert, ".0">;

def  LSU_LD_v4i16_as_v2i32 : T_LSU_LD_VRFconv<   VRF64_l,  v4i16, v2i32, "64",    bitconvert, ".0">;
def  LSU_LD_v4i16_as_v2f32 : T_LSU_LD_VRFconv<   VRF64_l,  v4i16, v2f32, "64",    bitconvert, ".0">;
def  LSU_LD_v4i16_as_v4f16 : T_LSU_LD_VRFconv<   VRF64_l,  v4i16, v4f16, "64",    bitconvert, ".0">;
def  LSU_LD_v4i16_as_v8i8  : T_LSU_LD_VRFconv<   VRF64_l,  v4i16, v8i8,  "64",    bitconvert, ".0">;

def  LSU_LD_v4f16_as_v2i32 : T_LSU_LD_VRFconv<   VRF64_l,  v4f16, v2i32, "64",    bitconvert, ".0">;
def  LSU_LD_v4f16_as_v2f32 : T_LSU_LD_VRFconv<   VRF64_l,  v4f16, v2f32, "64",    bitconvert, ".0">;
def  LSU_LD_v4f16_as_v4i16 : T_LSU_LD_VRFconv<   VRF64_l,  v4f16, v4i16, "64",    bitconvert, ".0">;
def  LSU_LD_v4f16_as_v8i8  : T_LSU_LD_VRFconv<   VRF64_l,  v4f16, v8i8,  "64",    bitconvert, ".0">;

def  LSU_LD_v8i8_as_v2i32  : T_LSU_LD_VRFconv<   VRF64_l,  v8i8,  v2i32, "64",    bitconvert, ".0">;
def  LSU_LD_v8i8_as_v2f32  : T_LSU_LD_VRFconv<   VRF64_l,  v8i8,  v2f32, "64",    bitconvert, ".0">;
def  LSU_LD_v8i8_as_v4i16  : T_LSU_LD_VRFconv<   VRF64_l,  v8i8,  v4i16, "64",    bitconvert, ".0">;
def  LSU_LD_v8i8_as_v4f16  : T_LSU_LD_VRFconv<   VRF64_l,  v8i8,  v4f16, "64",    bitconvert, ".0">;

// ======
//    32-bit reinterpret loads
def  LSU_LD_i32_as_f32     : T_LSU_LD_IRFconv<   IRF32,    i32,   f32,   "32",          bitconvert>;
def  LSU_LD_i32_as_v2i16   : T_LSU_LD_IRFconv<   IRF32,    i32,   v2i16, "32",          bitconvert>;
def  LSU_LD_i32_as_v2f16   : T_LSU_LD_IRFconv<   IRF32,    i32,   v2f16, "32",          bitconvert>;
def  LSU_LD_i32_as_v4i8    : T_LSU_LD_IRFconv<   IRF32,    i32,   v4i8,  "32",          bitconvert>;

def  LSU_LD_f32_as_i32     : T_LSU_LD_IRFconv<   IRF32,    f32,   i32,   "32",          bitconvert>;
def  LSU_LD_f32_as_v2i16   : T_LSU_LD_IRFconv<   IRF32,    f32,   v2i16, "32",          bitconvert>;
def  LSU_LD_f32_as_v2f16   : T_LSU_LD_IRFconv<   IRF32,    f32,   v2f16, "32",          bitconvert>;
def  LSU_LD_f32_as_v4i8    : T_LSU_LD_IRFconv<   IRF32,    f32,   v4i8,  "32",          bitconvert>;

def  LSU_LD_v2i16_as_i32   : T_LSU_LD_IRFconv<   IRF32,    v2i16, i32,   "32",          bitconvert>;
def  LSU_LD_v2i16_as_f32   : T_LSU_LD_IRFconv<   IRF32,    v2i16, f32,   "32",          bitconvert>;
def  LSU_LD_v2i16_as_v2f16 : T_LSU_LD_IRFconv<   IRF32,    v2i16, v2f16, "32",          bitconvert>;
def  LSU_LD_v2i16_as_v4i8  : T_LSU_LD_IRFconv<   IRF32,    v2i16, v4i8,  "32",          bitconvert>;

def  LSU_LD_v2f16_as_i32   : T_LSU_LD_IRFconv<   IRF32,    v2f16, i32,   "32",          bitconvert>;
def  LSU_LD_v2f16_as_f32   : T_LSU_LD_IRFconv<   IRF32,    v2f16, f32,   "32",          bitconvert>;
def  LSU_LD_v2f16_as_v2i16 : T_LSU_LD_IRFconv<   IRF32,    v2f16, v2i16, "32",          bitconvert>;
def  LSU_LD_v2f16_as_v4i8  : T_LSU_LD_IRFconv<   IRF32,    v2f16, v4i8,  "32",          bitconvert>;

def  LSU_LD_v4i8_as_i32    : T_LSU_LD_IRFconv<   IRF32,    v4i8,  i32,   "32",          bitconvert>;
def  LSU_LD_v4i8_as_f32    : T_LSU_LD_IRFconv<   IRF32,    v4i8,  f32,   "32",          bitconvert>;
def  LSU_LD_v4i8_as_v2i16  : T_LSU_LD_IRFconv<   IRF32,    v4i8,  v2i16, "32",          bitconvert>;
def  LSU_LD_v4i8_as_v2f16  : T_LSU_LD_IRFconv<   IRF32,    v4i8,  v2f16, "32",          bitconvert>;

// ======
//    16-bit reinterpret loads
def  LSU_LD_i16_as_f16     : T_LSU_LD_IRFconv<   IRF16_l,  i16,   f16,   "16",          bitconvert, ".0">;
def  LSU_LD_i16_as_v2i8    : T_LSU_LD_IRFconv<   IRF16_l,  i16,   v2i8,  "16",          bitconvert, ".0">;

def  LSU_LD_f16_as_i16     : T_LSU_LD_IRFconv<   IRF16_l,  f16,   i16,   "16",          bitconvert, ".0">;
def  LSU_LD_f16_as_v2i8    : T_LSU_LD_IRFconv<   IRF16_l,  f16,   v2i8,  "16",          bitconvert, ".0">;

def  LSU_LD_v2i8_as_i16    : T_LSU_LD_IRFconv<   IRF16_l,  v2i8,  i16,   "16",          bitconvert, ".0">;
def  LSU_LD_v2i8_as_f16    : T_LSU_LD_IRFconv<   IRF16_l,  v2i8,  f16,   "16",          bitconvert, ".0">;

// The following are special cases intended for use in intrinsics that limit to one LSU load accesses.
// They are essentially the same as the RAW versions, but have special names to allow the hazard checker
// to distinguish them.
defm LSU_LD64_ex           : T_LSU_LD_IRF64_Raw< IRF64,                  "64",           [isExclusiveLoad]>;

defm LSU_LD8_ex            : T_LSU_LD_IRF_Raw<   IRF8_q0,                "8",  ".0", [isExclusiveLoad]>;
defm LSU_LD16_ex           : T_LSU_LD_IRF_Raw<   IRF16_l,                "16", ".0", [isExclusiveLoad]>;
defm LSU_LD32_ex           : T_LSU_LD_IRF_Raw<   IRF32,                  "32", "",       [isExclusiveLoad]>;

//
// Direct vector element loads
//

// Pointer in IRF
class T_LSU_LD_VRF_IRF_Element<ValueType dstVT> :
  SHAVE_LSUInstr<(outs SHAVE_TypeInfo<dstVT>.regClassVRF:$dst),
                 (ins SHAVE_TypeInfo<dstVT>.regClassVRF:$dsrc, MEMr:$addr, i32imm:$element),
                 !strconcat(".LD.", !cast<string>(SHAVE_TypeInfo<dstVT>.scalarType.Size), " $dst.$element $addr"),
                 [(set (dstVT SHAVE_TypeInfo<dstVT>.regClassVRF:$dst),
                       (vector_insert (dstVT SHAVE_TypeInfo<dstVT>.regClassVRF:$dsrc),
                                      (SHAVE_TypeInfo<dstVT>.scalarType (load ADDRr:$addr)),
                                      imm:$element))],
                 [isLoadElement]>, Requires<[HasVRF128_Predicate]> {
  let mayLoad = 1;
  let Constraints = "$dsrc = $dst";
  let Itinerary = II_LSU_LD_VRF_Element;
}

// Pointer in VRF element
class T_LSU_LD_VRF_VRF_Element<ValueType dstVT, ValueType ptrVT> :
  SHAVE_LSUInstr<(outs SHAVE_TypeInfo<dstVT>.regClassVRF:$dst),
                 (ins SHAVE_TypeInfo<dstVT>.regClassVRF:$dsrc, SHAVE_TypeInfo<ptrVT>.regClassVRF:$addr, i32imm:$dstElement, i32imm:$srcElement),
                 !strconcat(".LD.", !cast<string>(SHAVE_TypeInfo<dstVT>.scalarType.Size), " $dst.$dstElement $addr.$srcElement"),
                 [(set (dstVT SHAVE_TypeInfo<dstVT>.regClassVRF:$dst),
                       (vector_insert (dstVT SHAVE_TypeInfo<dstVT>.regClassVRF:$dsrc),
                                      (SHAVE_TypeInfo<dstVT>.scalarType
                                        (load
                                          (i32
                                            (vector_extract (ptrVT SHAVE_TypeInfo<ptrVT>.regClassVRF:$addr), imm:$srcElement)
                                          )
                                        )
                                      ),
                                      imm:$dstElement
                       )
                  )],
                 [isLoadElement]>, Requires<[HasVRF128_Predicate]> {
  let mayLoad = 1;
  let Constraints = "$dsrc = $dst";
  let Itinerary = II_LSU_LD_VRF_VRF_Element;
}

foreach type = [ v4i32, v4f32, v8i16, v8f16, v16i8,
                 v2i32, v2f32, v4i16, v4f16, v8i8,
                               v2i16, v2f16, v4i8,
                                             v2i8 ] in {
  // Loads with pointer in VRF must be defined first or the scalar versions will take precedence and these will never be selected
  def LSU_LD#SHAVE_TypeInfo<type>.scalarType.Size#_#type#_insert_v4i32_ptr : T_LSU_LD_VRF_VRF_Element<type, v4i32>;
  def LSU_LD#SHAVE_TypeInfo<type>.scalarType.Size#_#type#_insert_v2i32_ptr : T_LSU_LD_VRF_VRF_Element<type, v2i32>;

  def LSU_LD#SHAVE_TypeInfo<type>.scalarType.Size#_#type#_insert : T_LSU_LD_VRF_IRF_Element<type>;
}

//
//===----------------------------------------------------------------------===//
//
// 1.5.6.2 - LSU.LDI
//    Register indirect load of specified type from memory with post-increment
//    of address by implicit value.
//
class T_LSU_LDI_Base<RegisterClass dstRC, string asmstr, list<dag> pattern, string lane = ""> :
      SHAVE_LSUInstr<(outs dstRC:$dst, IRF32:$ptr),
                     (ins MEMr:$addr),
                     !strconcat(".LDI.", asmstr, " $dst", lane, " $addr"),
                     pattern> {
  let Constraints = "$addr = $ptr";
  let mayLoad = 1;
}

//    Specialise for VRF itineraries
class T_LSU_LDI_VRF_Raw_Base<RegisterClass dstRC, string asmstr, string lane> :
      T_LSU_LDI_Base<dstRC, asmstr, [], lane> {
  let Itinerary = II_LSU_LDI_VRF;
}
multiclass T_LSU_LDI_VRF_Raw<RegisterClass dstRC, string asmstr, string lane = ""> {
  def  _Raw   : T_LSU_LDI_VRF_Raw_Base<dstRC, asmstr, lane>;
}

//    Specialise for 32-bit IRF itineraries
class T_LSU_LDI_IRF_Raw_Base<RegisterClass dstRC, string asmstr, string lane = ""> :
      T_LSU_LDI_Base<dstRC, asmstr, [], lane> {
  let Itinerary = II_LSU_LDI_IRF;
}
multiclass T_LSU_LDI_IRF_Raw<RegisterClass dstRC, string asmstr, string lane = ""> {
  def  _Raw : T_LSU_LDI_IRF_Raw_Base<dstRC, asmstr, lane>;
}

//    Specialise for 64-bit IRF itineraries
class T_LSU_LDI_IRF64_Raw_Base<RegisterClass dstRC, string asmstr> :
      T_LSU_LDI_Base<dstRC, asmstr, []> {
  let Itinerary = II_LSU_LDI_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}
multiclass T_LSU_LDI_IRF64_Raw<RegisterClass dstRC, string asmstr> {
  def  _Raw : T_LSU_LDI_IRF64_Raw_Base<dstRC, asmstr>;
}

// Vector loads
// ======
// 64-bit LDI.64.l and LDI.64.h loads
defm LSU_LDI64_h            : T_LSU_LDI_VRF_Raw<VRF64_h, "64",   ".1">;
defm LSU_LDI64_l            : T_LSU_LDI_VRF_Raw<VRF64_l, "64",   ".0">;

// ======
// Unsigned 8-bit to 16-bit FP converting LDI.128.u8.f16 loads
defm LSU_LDI128_u8_f16      : T_LSU_LDI_VRF_Raw<VRF128,  "128.u8.f16"          >;

// ======
// U8F to 16-bit FP converting LDI.128.u8f.f16 loads
defm LSU_LDI_128_u8f_f16    : T_LSU_LDI_VRF_Raw<VRF128,  "128.u8f.f16"         >;

// ======
// F16 to F32 extending 128-bit loads
defm LSU_LDI128_f16_f32     : T_LSU_LDI_VRF_Raw<VRF128,  "128.f16.f32"         >;

// ======
// Integer extending loads
defm LSU_LDI128_i8_i16      : T_LSU_LDI_VRF_Raw<VRF128,  "128.i8.i16"          >;
defm LSU_LDI128_u8_u16      : T_LSU_LDI_VRF_Raw<VRF128,  "128.u8.u16"          >;
defm LSU_LDI128_i16_i32     : T_LSU_LDI_VRF_Raw<VRF128,  "128.i16.i32"         >;
defm LSU_LDI128_u16_u32     : T_LSU_LDI_VRF_Raw<VRF128,  "128.u16.u32"         >;
defm LSU_LDI128_i8_i32      : T_LSU_LDI_VRF_Raw<VRF128,  "128.i8.i32"          >;
defm LSU_LDI128_u8_u32      : T_LSU_LDI_VRF_Raw<VRF128,  "128.u8.u32"          >;

// ======
// Splat loads
defm LSU_LDI_v4x32r         : T_LSU_LDI_VRF_Raw<VRF128,  "128.32.r"                >;
defm LSU_LDI_v8x16r         : T_LSU_LDI_VRF_Raw<VRF128,  "128.16.r"                >;
defm LSU_LDI_v16x8r         : T_LSU_LDI_VRF_Raw<VRF128,  "128.8.r"                 >;

// These actually write 128-bits, but we aren't concerned about the upper bits
defm LSU_LDI_v2x32r        : T_LSU_LDI_VRF_Raw<VRF64_l,  "128.32.r"                >;
defm LSU_LDI_v4x16r        : T_LSU_LDI_VRF_Raw<VRF64_l,  "128.16.r"                >;
defm LSU_LDI_v8x8r         : T_LSU_LDI_VRF_Raw<VRF64_l,  "128.8.r"                 >;

// These actually write 128-bits, but we aren't concerned about the upper bits
defm LSU_LDI_x32r_VRF      : T_LSU_LDI_VRF_Raw<VRF32_q0, "128.32.r"                >;
defm LSU_LDI_v2x16r_VRF    : T_LSU_LDI_VRF_Raw<VRF32_q0, "128.16.r"                >;
defm LSU_LDI_v4x8r_VRF     : T_LSU_LDI_VRF_Raw<VRF32_q0, "128.8.r"                 >;


// Scalar loads
// ======
// 64-bit scalar loads
defm LSU_LDI_i64           : T_LSU_LDI_IRF64_Raw<IRF64, "64">;

// ======
// Non-converting loads
//    8, 16 and 32-bit scalar
defm LSU_LDI_x8             : T_LSU_LDI_IRF_Raw<IRF8_q0, "8",         ".0">;
defm LSU_LDI_x16            : T_LSU_LDI_IRF_Raw<IRF16_l, "16",        ".0">;
defm LSU_LDI_x32            : T_LSU_LDI_IRF_Raw<IRF32,   "32"                 >;

// Converting loads
//    8, 16 and 32-bit scalar
defm LSU_LDI32_i16_i32      : T_LSU_LDI_IRF_Raw<IRF32,   "32.i16.i32"         >;
defm LSU_LDI32_i8_i32       : T_LSU_LDI_IRF_Raw<IRF32,   "32.i8.i32"          >;
defm LSU_LDI32_i8_i16       : T_LSU_LDI_IRF_Raw<IRF32,   "32.i8.i16"          >; // This overwrites the upper 16-bits, but we don't care about them

defm LSU_LDI32_u16_u32      : T_LSU_LDI_IRF_Raw<IRF32,   "32.u16.u32"         >;
defm LSU_LDI32_u8_u32       : T_LSU_LDI_IRF_Raw<IRF32,   "32.u8.u32"          >;
defm LSU_LDI32_u8_u16       : T_LSU_LDI_IRF_Raw<IRF32,   "32.u8.u16"          >; // This overwrites the upper 16-bits, but we don't care about them

// Myriad2.3 new ldi patterns
defm LSU_LDI32_f16_f32      : T_LSU_LDI_IRF_Raw<IRF32,   "32.f16.f32">;

//
//
//===----------------------------------------------------------------------===//
//
// 1.5.6.3 - LSU.LDA
//    Direct load of specified type from memory
//
//    TODO
//
//===----------------------------------------------------------------------===//
//
// 1.5.6.4 - LSU.LDIH
//    Load 16-bit immedage to IRF (high-order).
//
//    No patterns are used, these are selected during lowering
//
let hasSideEffects = 0,
    isReMaterializable = 1,
    Constraints = "$src = $dst",
    Itinerary = II_LSU_LDIH_IRF in {
    def LSU_LDIH       : SHAVE_LSUInstr<(outs IRF32:$dst),
                                        (ins IRF32:$src, i32imm:$src1),
                                        ".LDIH $dst $src1",
                                        [], [isLoadImmediateHigh]>;

    def LSU_LDIHSym    : SHAVE_LSUInstr<(outs IRF32:$dst),
                                        (ins IRF32:$src, SymBB:$src1),
                                        ".LDIH $dst $src1",
                                        [], [isLoadImmediateHigh]>;

    def LSU_LDIH_Label : SHAVE_LSUInstr<(outs IRF32:$dst),
                                        (ins IRF32:$src, Label:$l),
                                        ".LDIH $dst $l",
                                        [], [isLoadImmediateHigh]>;
}

//
//===----------------------------------------------------------------------===//
//
// 1.5.6.5 - LSU.LDIL
//    Load 16-bit immedage to IRF (low-order).
//
//    No patterns are used, these are selected during lowering
//
let hasSideEffects = 0,
    isReMaterializable = 1,
    Itinerary = II_LSU_LDIL_IRF in {
  def LSU_LDIL       : SHAVE_LSUInstr<(outs IRF32:$dst),
                                      (ins i32imm:$src1),
                                      ".LDIL $dst $src1",
                                      [], [isLoadImmediateLow]>;

  def LSU_LDILSym    : SHAVE_LSUInstr<(outs IRF32:$dst),
                                      (ins SymBB:$src1),
                                      ".LDIL $dst $src1",
                                      [], [isLoadImmediateLow]>;

  def LSU_LDIL_Label : SHAVE_LSUInstr<(outs IRF32:$dst),
                                      (ins Label:$l),
                                      ".LDIL $dst $l",
                                      [], [isLoadImmediateLow]>;
}

//
//===----------------------------------------------------------------------===//
//
// 1.5.6.6 - LSU.LDINC
//    Register indirect load of specified type from memory with post-increment of
//    address by explicit mode and value.
//
class T_LSU_LDINC_Base<RegisterClass dstRC, string asmstr, list<dag> pattern, string lane = "", list<SHAVEConflictName> conflicts = []> :
      SHAVE_LSUInstr<(outs dstRC:$dst, IRF32:$ptr),
                     (ins MEMr:$addr, IRF16_l:$incr, IRF16_h:$mode),
                     !strconcat(".LDI.", asmstr, " $dst", lane, " $addr"),
                     pattern, conflicts> {
//  let Constraints = "$addr = $ptr, $incr = $mode";
// FIXME-SAURABH: $incr = $mode does not work with llvm9 any more
  let Constraints = "$addr = $ptr";
  let mayLoad = 1;
}

//    Specialise for VRF itineraries
class T_LSU_LDINC_VRF_Raw_Base<RegisterClass dstRC, string asmstr, string lane> :
      T_LSU_LDINC_Base<dstRC, asmstr, [], lane> {
  let Itinerary = II_LSU_LDINC_VRF;
}
multiclass T_LSU_LDINC_VRF_Raw<RegisterClass dstRC, string asmstr, string lane = ""> {
  def  _Raw   : T_LSU_LDINC_VRF_Raw_Base<dstRC, asmstr, lane>;
}

//    Specialise for 32-bit IRF itineraries
class T_LSU_LDINC_IRF_Raw_Base<RegisterClass dstRC, string asmstr, string lane = ""> :
      T_LSU_LDINC_Base<dstRC, asmstr, [], lane> {
  let Itinerary = II_LSU_LDINC_IRF;
}
multiclass T_LSU_LDINC_IRF_Raw<RegisterClass dstRC, string asmstr, string lane = ""> {
  def  _Raw : T_LSU_LDINC_IRF_Raw_Base<dstRC, asmstr, lane>;
}

//    Specialise for 64-bit IRF itineraries
class T_LSU_LDINC_IRF64_Raw_Base<RegisterClass dstRC, string asmstr, list<SHAVEConflictName> conflicts = []> :
      T_LSU_LDINC_Base<dstRC, asmstr, [], "", conflicts> {
  let Itinerary = II_LSU_LDINC_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}
multiclass T_LSU_LDINC_IRF64_Raw<RegisterClass dstRC, string asmstr, list<SHAVEConflictName> conflicts = []> {
  def  _Raw : T_LSU_LDINC_IRF64_Raw_Base<dstRC, asmstr, conflicts>;
}

// Vector loads
// ======
// 64-bit LDI.64.l and LDI.64.h loads
defm LSU_LDINC64_h            : T_LSU_LDINC_VRF_Raw<VRF64_h, "64", ".1">;
defm LSU_LDINC64_l            : T_LSU_LDINC_VRF_Raw<VRF64_l, "64", ".0">;

// ======
// Unsigned 8-bit to 16-bit FP converting LDI.128.u8.f16 loads
defm LSU_LDINC_v8u8_v8f16     : T_LSU_LDINC_VRF_Raw<VRF128, "128.u8.f16">;

// ======
// U8F to 16-bit FP converting LDI.128.u8f.f16 loads
defm LSU_LDINC_v8u8f_v8f16    : T_LSU_LDINC_VRF_Raw<VRF128, "128.u8f.f16">;

// ======
// F16 to F32 extending 128-bit loads
defm LSU_LDINC_v4f16_v4f32    : T_LSU_LDINC_VRF_Raw<VRF128, "128.f16.f32">;

// ======
// Integer extending loads
defm LSU_LDINC128_i8_i16      : T_LSU_LDINC_VRF_Raw<VRF128, "128.i8.i16">;
defm LSU_LDINC128_u8_u16      : T_LSU_LDINC_VRF_Raw<VRF128, "128.u8.u16">;
defm LSU_LDINC128_i16_i32     : T_LSU_LDINC_VRF_Raw<VRF128, "128.i16.i32">;
defm LSU_LDINC128_u16_u32     : T_LSU_LDINC_VRF_Raw<VRF128, "128.u16.u32">;

// ======
// Splat loads
defm LSU_LDINC_v4x32r         : T_LSU_LDINC_VRF_Raw<VRF128, "128.32.r">;
defm LSU_LDINC_v8x16r         : T_LSU_LDINC_VRF_Raw<VRF128, "128.16.r">;
defm LSU_LDINC_v16x8r         : T_LSU_LDINC_VRF_Raw<VRF128, "128.8.r">;

// These actually write 128-bits, but we aren't concerned about the upper bits
defm LSU_LDINC_v2x32r         : T_LSU_LDINC_VRF_Raw<VRF64_l, "128.32.r">;
defm LSU_LDINC_v4x16r         : T_LSU_LDINC_VRF_Raw<VRF64_l, "128.16.r">;
defm LSU_LDINC_v8x8r          : T_LSU_LDINC_VRF_Raw<VRF64_l, "128.8.r">;

// These actually write 128-bits, but we aren't concerned about the upper bits
defm LSU_LDINC_v2x16r_VRF     : T_LSU_LDINC_VRF_Raw<VRF32_q0, "128.16.r">;
defm LSU_LDINC_v4x8r_VRF      : T_LSU_LDINC_VRF_Raw<VRF32_q0, "128.8.r">;


// Scalar loads
// ======
// 64-bit scalar loads
defm LSU_LDINC_i64            : T_LSU_LDINC_IRF64_Raw<IRF64, "64">;

// ======
// Non-converting loads
//    8, 16 and 32-bit scalar and vector
defm LSU_LDINC_x8             : T_LSU_LDINC_IRF_Raw<IRF8_q0, "8",  ".0">;
defm LSU_LDINC_x16            : T_LSU_LDINC_IRF_Raw<IRF16_l, "16", ".0">;
defm LSU_LDINC_x32            : T_LSU_LDINC_IRF_Raw<IRF32,   "32"          >;

// Converting loads
//    8, 16 and 32-bit scalar
defm LSU_LDINC_i16_i32        : T_LSU_LDINC_IRF_Raw<IRF32,   "32.i16.i32">;
defm LSU_LDINC_i8_i32         : T_LSU_LDINC_IRF_Raw<IRF32,   "32.i8.i32 ">;
defm LSU_LDINC_i8_i16         : T_LSU_LDINC_IRF_Raw<IRF16_l, "32.i8.i32" >; // This overwrites the upper 16-bits, but we don't care about them

defm LSU_LDINC_u16_u32        : T_LSU_LDINC_IRF_Raw<IRF32,   "32.u16.u32">;
defm LSU_LDINC_u8_u32         : T_LSU_LDINC_IRF_Raw<IRF32,   "32.u8.u32" >;
defm LSU_LDINC_u8_u16         : T_LSU_LDINC_IRF_Raw<IRF16_l, "32.u8.u32" >;   // This overwrites the upper 16-bits, but we don't care about them

//
//===----------------------------------------------------------------------===//
//
// 1.5.6.7 - LSU.LDO
//    Displacement load of specified type from memory.
//
class T_LSU_LDO_Base<RegisterClass dstRC, string asmstr, list<dag> pattern, string lane = "", list<SHAVEConflictName> conflicts = []> :
      SHAVE_LSUInstr<(outs dstRC:$dst),
                     (ins MEMri:$addr),
                     !strconcat(".LDO.", asmstr, " $dst", lane, " $addr"),
                     pattern, conflicts> {
  let mayLoad = 1;
}

//    Specialise for VRF itineraries
class T_LSU_LDO_VRF_Raw_Base<RegisterClass dstRC, string asmstr, string lane> :
      T_LSU_LDO_Base<dstRC, asmstr, [], lane> {
  let Itinerary = II_LSU_LDO_VRF;
}
multiclass T_LSU_LDO_VRF_Raw<RegisterClass dstRC, string asmstr, string lane = ""> {
  def  _Raw : T_LSU_LDO_VRF_Raw_Base<dstRC, asmstr, lane>;
}

class T_LSU_LDO_VRF<RegisterClass dstRC, ValueType dstVT, string asmstr, PatFrag loadfrag = load, string lane = ""> :
      T_LSU_LDO_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (loadfrag ADDRri:$addr))], lane> {
  let Itinerary = II_LSU_LDO_VRF;
}

class T_LSU_LDO_VRFconv<RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode, string lane = ""> :
      T_LSU_LDO_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (convMode (srcVT (load ADDRri:$addr))))], lane> {
  let Itinerary = II_LSU_LDO_VRF;
}

class T_LSU_LDO_IRF<RegisterClass dstRC, ValueType dstVT, string asmstr, PatFrag loadfrag = load, string lane = ""> :
      T_LSU_LDO_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (loadfrag ADDRri:$addr))], lane> {
  let Itinerary = II_LSU_LDO_IRF;
}

class T_LSU_LDO_IRFconv<RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode, string lane = ""> :
      T_LSU_LDO_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (convMode (srcVT (load ADDRri:$addr))))], lane> {
  let Itinerary = II_LSU_LDO_IRF;
}

//    Specialise for 64-bit IRF itineraries
class T_LSU_LDO_IRF64_Raw_Base<RegisterClass dstRC, string asmstr, list<SHAVEConflictName> conflicts = []> :
      T_LSU_LDO_Base<dstRC, asmstr, [], "", conflicts> {
  let Itinerary = II_LSU_LDO_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}
multiclass T_LSU_LDO_IRF64_Raw<RegisterClass dstRC, string asmstr, list<SHAVEConflictName> conflicts = []> {
  def  _Raw : T_LSU_LDO_IRF64_Raw_Base<dstRC, asmstr, conflicts>;
}

class T_LSU_LDO_IRF64<RegisterClass dstRC, ValueType dstVT, string asmstr> :
      T_LSU_LDO_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (load ADDRri:$addr))], ""> {
  let Itinerary = II_LSU_LDO_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}

class T_LSU_LDO_IRF64conv<RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode> :
      T_LSU_LDO_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (convMode (srcVT (load ADDRri:$addr))))], ""> {
  let Itinerary = II_LSU_LDO_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}

let Predicates = [HasVRF128_Predicate] in {
  // Vector loads
  // ======
  // 64-bit LDO.64.l and LDO.64.h loads
  defm LSU_LDO64_h            : T_LSU_LDO_VRF_Raw<   VRF64_h,                "64",          ".1">;

  def  LSU_LDO_v2i32_h        : T_LSU_LDO_VRF<       VRF64_h,         v2i32, "64",    load, ".1">;
  def  LSU_LDO_v4i16_h        : T_LSU_LDO_VRF<       VRF64_h,         v4i16, "64",    load, ".1">;
  def  LSU_LDO_v8i8_h         : T_LSU_LDO_VRF<       VRF64_h,         v8i8,  "64",    load, ".1">;

  def  LSU_LDO_v2f32_h        : T_LSU_LDO_VRF<       VRF64_h,         v2f32, "64",    load, ".1">;
  def  LSU_LDO_v4f16_h        : T_LSU_LDO_VRF<       VRF64_h,         v4f16, "64",    load, ".1">;

  defm LSU_LDO64_l            : T_LSU_LDO_VRF_Raw<   VRF64_l,                "64",          ".0">;

  def  LSU_LDO_v2i32          : T_LSU_LDO_VRF<       VRF64_l,         v2i32, "64",    load, ".0">;
  def  LSU_LDO_v4i16          : T_LSU_LDO_VRF<       VRF64_l,         v4i16, "64",    load, ".0">;
  def  LSU_LDO_v8i8           : T_LSU_LDO_VRF<       VRF64_l,         v8i8,  "64",    load, ".0">;

  def  LSU_LDO_v2f32          : T_LSU_LDO_VRF<       VRF64_l,         v2f32, "64",    load, ".0">;
  def  LSU_LDO_v4f16          : T_LSU_LDO_VRF<       VRF64_l,         v4f16, "64",    load, ".0">;

  //    32-bit small vector
  def  LSU_LDO_v4i8           : T_LSU_LDO_IRF<       IRF32,           v4i8,  "32">;
  def  LSU_LDO_v2i16          : T_LSU_LDO_IRF<       IRF32,           v2i16, "32">;

  def  LSU_LDO_v2f16          : T_LSU_LDO_IRF<       IRF32,           v2f16, "32">;
  //    16-bit small vector
  def  LSU_LDO_v2i8           : T_LSU_LDO_IRF<       IRF16_l,         v2i8,  "16",          load, ".0">;

  // Reinterpretting loads
  // ======
  //    64-bit reinterpret loads
  def  LSU_LDO_v2i32_as_v2f32 : T_LSU_LDO_VRFconv<   VRF64_l,  v2i32, v2f32, "64",    bitconvert, ".0">;
  def  LSU_LDO_v2i32_as_v4i16 : T_LSU_LDO_VRFconv<   VRF64_l,  v2i32, v4i16, "64",    bitconvert, ".0">;
  def  LSU_LDO_v2i32_as_v4f16 : T_LSU_LDO_VRFconv<   VRF64_l,  v2i32, v4f16, "64",    bitconvert, ".0">;
  def  LSU_LDO_v2i32_as_v8i8  : T_LSU_LDO_VRFconv<   VRF64_l,  v2i32, v8i8,  "64",    bitconvert, ".0">;

  def  LSU_LDO_v2f32_as_v2i32 : T_LSU_LDO_VRFconv<   VRF64_l,  v2f32, v2i32, "64",    bitconvert, ".0">;
  def  LSU_LDO_v2f32_as_v4i16 : T_LSU_LDO_VRFconv<   VRF64_l,  v2f32, v4i16, "64",    bitconvert, ".0">;
  def  LSU_LDO_v2f32_as_v4f16 : T_LSU_LDO_VRFconv<   VRF64_l,  v2f32, v4f16, "64",    bitconvert, ".0">;
  def  LSU_LDO_v2f32_as_v8i8  : T_LSU_LDO_VRFconv<   VRF64_l,  v2f32, v8i8,  "64",    bitconvert, ".0">;

  def  LSU_LDO_v4i16_as_v2i32 : T_LSU_LDO_VRFconv<   VRF64_l,  v4i16, v2i32, "64",    bitconvert, ".0">;
  def  LSU_LDO_v4i16_as_v2f32 : T_LSU_LDO_VRFconv<   VRF64_l,  v4i16, v2f32, "64",    bitconvert, ".0">;
  def  LSU_LDO_v4i16_as_v4f16 : T_LSU_LDO_VRFconv<   VRF64_l,  v4i16, v4f16, "64",    bitconvert, ".0">;
  def  LSU_LDO_v4i16_as_v8i8  : T_LSU_LDO_VRFconv<   VRF64_l,  v4i16, v8i8,  "64",    bitconvert, ".0">;

  def  LSU_LDO_v4f16_as_v2i32 : T_LSU_LDO_VRFconv<   VRF64_l,  v4f16, v2i32, "64",    bitconvert, ".0">;
  def  LSU_LDO_v4f16_as_v2f32 : T_LSU_LDO_VRFconv<   VRF64_l,  v4f16, v2f32, "64",    bitconvert, ".0">;
  def  LSU_LDO_v4f16_as_v4i16 : T_LSU_LDO_VRFconv<   VRF64_l,  v4f16, v4i16, "64",    bitconvert, ".0">;
  def  LSU_LDO_v4f16_as_v8i8  : T_LSU_LDO_VRFconv<   VRF64_l,  v4f16, v8i8,  "64",    bitconvert, ".0">;

  def  LSU_LDO_v8i8_as_v2i32  : T_LSU_LDO_VRFconv<   VRF64_l,  v8i8,  v2i32, "64",    bitconvert, ".0">;
  def  LSU_LDO_v8i8_as_v2f32  : T_LSU_LDO_VRFconv<   VRF64_l,  v8i8,  v2f32, "64",    bitconvert, ".0">;
  def  LSU_LDO_v8i8_as_v4i16  : T_LSU_LDO_VRFconv<   VRF64_l,  v8i8,  v4i16, "64",    bitconvert, ".0">;
  def  LSU_LDO_v8i8_as_v4f16  : T_LSU_LDO_VRFconv<   VRF64_l,  v8i8,  v4f16, "64",    bitconvert, ".0">;

  // ======
  //    32-bit reinterpret loads
  def  LSU_LDO_i32_as_v2i16   : T_LSU_LDO_IRFconv<   IRF32,    i32,   v2i16, "32",          bitconvert>;
  def  LSU_LDO_i32_as_v2f16   : T_LSU_LDO_IRFconv<   IRF32,    i32,   v2f16, "32",          bitconvert>;
  def  LSU_LDO_i32_as_v4i8    : T_LSU_LDO_IRFconv<   IRF32,    i32,   v4i8,  "32",          bitconvert>;

  def  LSU_LDO_f32_as_v2i16   : T_LSU_LDO_IRFconv<   IRF32,    f32,   v2i16, "32",          bitconvert>;
  def  LSU_LDO_f32_as_v2f16   : T_LSU_LDO_IRFconv<   IRF32,     f32,   v2f16, "32",          bitconvert>;
  def  LSU_LDO_f32_as_v4i8    : T_LSU_LDO_IRFconv<   IRF32,    f32,   v4i8,  "32",          bitconvert>;

  def  LSU_LDO_v2i16_as_i32   : T_LSU_LDO_IRFconv<   IRF32,    v2i16, i32,   "32",          bitconvert>;
  def  LSU_LDO_v2i16_as_f32   : T_LSU_LDO_IRFconv<   IRF32,    v2i16, f32,   "32",          bitconvert>;
  def  LSU_LDO_v2i16_as_v2f16 : T_LSU_LDO_IRFconv<   IRF32,    v2i16, v2f16, "32",          bitconvert>;
  def  LSU_LDO_v2i16_as_v4i8  : T_LSU_LDO_IRFconv<   IRF32,    v2i16, v4i8,  "32",          bitconvert>;

  def  LSU_LDO_v2f16_as_i32   : T_LSU_LDO_IRFconv<   IRF32,    v2f16, i32,   "32",          bitconvert>;
  def  LSU_LDO_v2f16_as_f32   : T_LSU_LDO_IRFconv<   IRF32,    v2f16, f32,   "32",          bitconvert>;
  def  LSU_LDO_v2f16_as_v2i16 : T_LSU_LDO_IRFconv<   IRF32,    v2f16, v2i16, "32",          bitconvert>;
  def  LSU_LDO_v2f16_as_v4i8  : T_LSU_LDO_IRFconv<   IRF32,    v2f16, v4i8,  "32",          bitconvert>;

  def  LSU_LDO_v4i8_as_i32    : T_LSU_LDO_IRFconv<   IRF32,    v4i8,  i32,   "32",          bitconvert>;
  def  LSU_LDO_v4i8_as_f32    : T_LSU_LDO_IRFconv<   IRF32,    v4i8,  f32,   "32",          bitconvert>;
  def  LSU_LDO_v4i8_as_v2i16  : T_LSU_LDO_IRFconv<   IRF32,    v4i8,  v2i16, "32",          bitconvert>;
  def  LSU_LDO_v4i8_as_v2f16  : T_LSU_LDO_IRFconv<   IRF32,    v4i8,  v2f16, "32",          bitconvert>;

  // ======
  //    16-bit reinterpret loads
  def  LSU_LDO_i16_as_v2i8    : T_LSU_LDO_IRFconv<   IRF16_l,  i16,   v2i8,  "16",          bitconvert, ".0">;
  def  LSU_LDO_f16_as_v2i8    : T_LSU_LDO_IRFconv<   IRF16_l,  f16,   v2i8,  "16",          bitconvert, ".0">;
  def  LSU_LDO_v2i8_as_i16    : T_LSU_LDO_IRFconv<   IRF16_l,  v2i8,  i16,   "16",          bitconvert, ".0">;
  def  LSU_LDO_v2i8_as_f16    : T_LSU_LDO_IRFconv<   IRF16_l,  v2i8,  f16,   "16",          bitconvert, ".0">;
}

let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate] in {
  // Scalar loads
  // ======
  // 64-bit scalar loads
  defm LSU_LDO64              : T_LSU_LDO_IRF64_Raw< IRF64,                  "64">;

  def  LSU_LDO_i64            : T_LSU_LDO_IRF64<     IRF64,           i64,   "64">;
}

// ======
// Non-converting loads
//    8, 16 and 32-bit scalar
def  LSU_LDO_i8             : T_LSU_LDO_IRF<       IRF8_q0,         i8,    "8",           load, ".0">;
def  LSU_LDO_i16            : T_LSU_LDO_IRF<       IRF16_l,         i16,   "16",          load, ".0">;
def  LSU_LDO_i32            : T_LSU_LDO_IRF<       IRF32,           i32,   "32">;

def  LSU_LDO_f16            : T_LSU_LDO_IRF<       IRF16_l,         f16,   "16",          load, ".0">;
def  LSU_LDO_f32            : T_LSU_LDO_IRF<       IRF32,           f32,   "32">;

// Converting loads
//    8, 16 and 32-bit scalar
def  LSU_LDO_a16_a32        : T_LSU_LDO_IRFconv<   IRF32,    i16,   i32,   "16",          anyext, ".0">;
def  LSU_LDO_a8_a32         : T_LSU_LDO_IRFconv<   IRF32,    i8,    i32,   "8",           anyext, ".0">;
def  LSU_LDO_a8_a16         : T_LSU_LDO_IRFconv<   IRF16_l,  i8,    i16,   "8",           anyext, ".0">;

// ======
//    32-bit reinterpret loads
def  LSU_LDO_i32_as_f32     : T_LSU_LDO_IRFconv<   IRF32,    i32,   f32,   "32",          bitconvert>;
def  LSU_LDO_f32_as_i32     : T_LSU_LDO_IRFconv<   IRF32,    f32,   i32,   "32",          bitconvert>;

// ======
//    16-bit reinterpret loads
def  LSU_LDO_i16_as_f16     : T_LSU_LDO_IRFconv<   IRF16_l,  i16,   f16,   "16",          bitconvert, ".0">;
def  LSU_LDO_f16_as_i16     : T_LSU_LDO_IRFconv<   IRF16_l,  f16,   i16,   "16",          bitconvert, ".0">;

//
//===----------------------------------------------------------------------===//
//
// 1.5.6.8 - LSU.LDX
//    Indexed load of specified type from memory.
//
class T_LSU_LDX_Base<RegisterClass dstRC, string asmstr, list<dag> pattern, string lane = "", list<SHAVEConflictName> conflicts = [], list<Predicate> predicates = []>:
      SHAVE_LSUInstr<(outs dstRC:$dst),
                     (ins MEMr32r32:$addr),
                     !strconcat(".LDX.", asmstr, " $dst", lane, " $addr"),
                     pattern, !listconcat(conflicts, [isLDXorSTX])>,
      Requires<!listconcat(predicates, [EnableLDXSTX])> {
  let mayLoad = 1;
}

//    Specialise for VRF itineraries
class T_LSU_LDX_VRF_Raw_Base<RegisterClass dstRC, string asmstr, string lane> :
      T_LSU_LDX_Base<dstRC, asmstr, [], lane, [], [HasVRF128_Predicate]> {
  let Itinerary = II_LSU_LDX_VRF;
}
multiclass T_LSU_LDX_VRF_Raw<RegisterClass dstRC, string asmstr, string lane = ""> {
  def  _Raw : T_LSU_LDX_VRF_Raw_Base<dstRC, asmstr, lane>;
}

class T_LSU_LDX_VRF<RegisterClass dstRC, ValueType dstVT, string asmstr, PatFrag loadfrag = load, string lane = ""> :
      T_LSU_LDX_Base<dstRC, asmstr, [(set(dstVT dstRC : $dst), (loadfrag ADDRrr : $addr))], lane, [], [HasVRF128_Predicate]> {
  let Itinerary = II_LSU_LDX_VRF;
}

class T_LSU_LDX_VRFconv<RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode, string lane = ""> :
      T_LSU_LDX_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (convMode (srcVT (load ADDRrr:$addr))))], lane, [], [HasVRF128_Predicate]> {
  let Itinerary = II_LSU_LDX_VRF;
}

//    Specialise for 32-bit IRF itineraries
class T_LSU_LDX_IRF_Raw_Base<RegisterClass dstRC, string asmstr, string lane = "", list<SHAVEConflictName> conflicts = []> :
      T_LSU_LDX_Base<dstRC, asmstr, [], lane, conflicts> {
  let Itinerary = II_LSU_LDX_IRF;
}
multiclass T_LSU_LDX_IRF_Raw<RegisterClass dstRC, string asmstr, string lane = "", list<SHAVEConflictName> conflicts = []> {
  def  _Raw : T_LSU_LDX_IRF_Raw_Base<dstRC, asmstr, lane, conflicts>;
}

class T_LSU_LDX_IRF<RegisterClass dstRC, ValueType dstVT, string asmstr, PatFrag loadfrag = load, string lane = ""> :
      T_LSU_LDX_Base<dstRC, asmstr, [(set(dstVT dstRC : $dst), (loadfrag ADDRr32r32 : $addr))], lane> {
  let Itinerary = II_LSU_LDX_IRF;
}

class T_LSU_LDX_IRFconv<RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode, string lane = ""> :
      T_LSU_LDX_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (convMode (srcVT (load ADDRrr:$addr))))], lane> {
  let Itinerary = II_LSU_LDX_IRF;
}

//    Specialise for 64-bit IRF itineraries
class T_LSU_LDX_IRF64_Raw_Base<RegisterClass dstRC, string asmstr, list<SHAVEConflictName> conflicts = []> :
      T_LSU_LDX_Base<dstRC, asmstr, [], "", conflicts> {
  let Itinerary = II_LSU_LDX_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}
multiclass T_LSU_LDX_IRF64_Raw<RegisterClass dstRC, string asmstr, list<SHAVEConflictName> conflicts = []> {
  def  _Raw : T_LSU_LDX_IRF64_Raw_Base<dstRC, asmstr, conflicts>;
}

class T_LSU_LDX_IRF64<RegisterClass dstRC, ValueType dstVT, string asmstr> :
      T_LSU_LDX_Base<dstRC, asmstr, [(set(dstVT dstRC : $dst), (load ADDRrr:$addr))], ""> {
  let Itinerary = II_LSU_LDX_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}

class T_LSU_LDX_IRF64conv<RegisterClass dstRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode> :
      T_LSU_LDX_Base<dstRC, asmstr, [(set (dstVT dstRC:$dst), (convMode (srcVT (load ADDRrr:$addr))))], ""> {
  let Itinerary = II_LSU_LDX_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}


  // Vector loads
  // ======
  let Itinerary = II_LSU_LDX128_VRF in {
    def LSU_LDX128_Raw : T_LSU_LDX_Base<VRF128, "128", [], "", [isLDXVorSTXV], [HasVRF128_Predicate]>;
  }

  // ======
  // 64-bit LDX.64.l and LDX.64.h loads
  defm LSU_LDX64_h            : T_LSU_LDX_VRF_Raw<   VRF64_h,                "64",          ".1">;

  def  LSU_LDX_v2i32_h        : T_LSU_LDX_VRF<       VRF64_h,         v2i32, "64",    load, ".1">;
  def  LSU_LDX_v4i16_h        : T_LSU_LDX_VRF<       VRF64_h,         v4i16, "64",    load, ".1">;
  def  LSU_LDX_v8i8_h         : T_LSU_LDX_VRF<       VRF64_h,         v8i8,  "64",    load, ".1">;

  def  LSU_LDX_v2f32_h        : T_LSU_LDX_VRF<       VRF64_h,         v2f32, "64",    load, ".1">;
  def  LSU_LDX_v4f16_h        : T_LSU_LDX_VRF<       VRF64_h,         v4f16, "64",    load, ".1">;

  defm LSU_LDX64_l            : T_LSU_LDX_VRF_Raw<   VRF64_l,                "64",          ".0">;

  def  LSU_LDX_v2i32          : T_LSU_LDX_VRF<       VRF64_l,         v2i32, "64",    load, ".0">;
  def  LSU_LDX_v4i16          : T_LSU_LDX_VRF<       VRF64_l,         v4i16, "64",    load, ".0">;
  def  LSU_LDX_v8i8           : T_LSU_LDX_VRF<       VRF64_l,         v8i8,  "64",    load, ".0">;

  def  LSU_LDX_v2f32          : T_LSU_LDX_VRF<       VRF64_l,         v2f32, "64",    load, ".0">;
  def  LSU_LDX_v4f16          : T_LSU_LDX_VRF<       VRF64_l,         v4f16, "64",    load, ".0">;

  // ======
  // Unsigned 8-bit to 16-bit FP converting LDX.128.u8.f16 loads
  defm LSU_LDX128_u8_f16      : T_LSU_LDX_VRF_Raw<   VRF128,                 "128.u8.f16">;

  def  LSU_LDX_v8u8_v8f16     : T_LSU_LDX_VRFconv<   VRF128,   v8i8,  v8f16, "128.u8.f16",  uint_to_fp>;

  // ======
  // U8F to 16-bit FP converting LDX.128.u8f.f16 loads
  defm LSU_LDX128_u8f_f16     : T_LSU_LDX_VRF_Raw<   VRF128,                 "128.u8f.f16">;

  let Itinerary = II_LSU_LDX_VRF in {
    def LSU_LDX128_u8f_f16 : T_LSU_LDX_Base<VRF128, "128.u8f.f16",
                                            [(set (v8f16 VRF128:$dst), (fdiv (uint_to_fp (v8i8 (load ADDRr32r32:$addr))), (v8f16 (splat (f16 u8fConst)))))],
                                            "", [], [HasVRF128_Predicate]>;
  }

  // ======
  // F16 to F32 extending 128-bit loads
  defm LSU_LDX128_f16_f32     : T_LSU_LDX_VRF_Raw<   VRF128,                 "128.f16.f32">;

  def  LSU_LDX_v4f16_v4f32    : T_LSU_LDX_VRF<       VRF128,          v4f32, "128.f16.f32", extload>;

  // ======
  // Integer extending loads
  defm LSU_LDX128_i8_i16      : T_LSU_LDX_VRF_Raw<   VRF128,                 "128.i8.i16">;
  defm LSU_LDX128_u8_u16      : T_LSU_LDX_VRF_Raw<   VRF128,                 "128.u8.u16">;
  defm LSU_LDX128_i16_i32     : T_LSU_LDX_VRF_Raw<   VRF128,                 "128.i16.i32">;
  defm LSU_LDX128_u16_u32     : T_LSU_LDX_VRF_Raw<   VRF128,                 "128.u16.u32">;

  def  LSU_LDX_v8a8_v8a16     : T_LSU_LDX_VRF<   VRF128,   v8i16, "128.i8.i16",  extloadv8i8>;
  def  LSU_LDX_v8s8_v8s16     : T_LSU_LDX_VRF<   VRF128,   v8i16, "128.i8.i16",  sextloadv8i8>;
  def  LSU_LDX_v8u8_v8u16     : T_LSU_LDX_VRF<   VRF128,   v8i16, "128.u8.u16",  zextloadv8i8>;
  def  LSU_LDX_v4a16_v4a32    : T_LSU_LDX_VRF<   VRF128,   v4i32, "128.i16.i32", extloadv4i16>;
  def  LSU_LDX_v4s16_v4s32    : T_LSU_LDX_VRF<   VRF128,   v4i32, "128.i16.i32", sextloadv4i16>;
  def  LSU_LDX_v4u16_v4u32    : T_LSU_LDX_VRF<   VRF128,   v4i32, "128.u16.u32", zextloadv4i16>;

  def  LSU_LDX_v4s8_v4s32    : T_LSU_LDX_VRF<   VRF128,   v4i32, "128.i8.i32", sextloadv4i8>;
  def  LSU_LDX_v4u8_v4u32    : T_LSU_LDX_VRF<   VRF128,   v4i32, "128.u8.u32", zextloadv4i8>;

  // ======
  // Splat loads
  defm LSU_LDX32r             : T_LSU_LDX_VRF_Raw<   VRF128,                 "128.32.r">;
  defm LSU_LDX16r             : T_LSU_LDX_VRF_Raw<   VRF128,                 "128.16.r">;
  defm LSU_LDX8r              : T_LSU_LDX_VRF_Raw<   VRF128,                 "128.8.r">;

  def  LSU_LDX_i32_v4i32      : T_LSU_LDX_VRFconv<   VRF128,   i32,   v4i32, "128.32.r",        splat>;
  def  LSU_LDX_i16_v8i16      : T_LSU_LDX_VRFconv<   VRF128,   i16,   v8i16, "128.16.r",        splat>;
  def  LSU_LDX_i8_v16i8       : T_LSU_LDX_VRFconv<   VRF128,   i8,    v16i8, "128.8.r",         splat>;

  def  LSU_LDX_f32_v4f32      : T_LSU_LDX_VRFconv<   VRF128,   f32,   v4f32, "128.32.r",        splat>;
  def  LSU_LDX_f16_v8f16      : T_LSU_LDX_VRFconv<   VRF128,   f16,   v8f16, "128.16.r",        splat>;

  //    These actually write 128-bits, but we aren't concerned about the upper bits
  def  LSU_LDX_i32_v2i32      : T_LSU_LDX_VRFconv<   VRF64_l,  i32,   v2i32, "128.32.r",        splat>;
  def  LSU_LDX_i16_v4i16      : T_LSU_LDX_VRFconv<   VRF64_l,  i16,   v4i16, "128.16.r",        splat>;
  def  LSU_LDX_i8_v8i8        : T_LSU_LDX_VRFconv<   VRF64_l,  i8,    v8i8,  "128.8.r",         splat>;

  def  LSU_LDX_f32_v2f32      : T_LSU_LDX_VRFconv<   VRF64_l,  f32,   v2f32, "128.32.r",        splat>;
  def  LSU_LDX_f16_v4f16      : T_LSU_LDX_VRFconv<   VRF64_l,  f16,   v4f16, "128.16.r",        splat>;

  //    These actually write 128-bits, but we aren't concerned about the upper bits
  def  LSU_LDX_i16_v2i16_VRF  : T_LSU_LDX_VRFconv<   VRF32_q0, i16,   v2i16, "128.16.r",        splat>;
  def  LSU_LDX_i8_v4i8_VRF    : T_LSU_LDX_VRFconv<   VRF32_q0, i8,    v4i8,  "128.8.r",         splat>;

  def  LSU_LDX_f16_v2f16_VRF  : T_LSU_LDX_VRFconv<   VRF32_q0, f16,   v2f16, "128.16.r",        splat>;

  //    These actually write 128-bits, but we aren't concerned about the upper bits
  def  LSU_LDX_v2i16_VRF      : T_LSU_LDX_VRF<       VRF32_q0,        v2i16, "128.32.r",        load>;
  def  LSU_LDX_v4i8_VRF       : T_LSU_LDX_VRF<       VRF32_q0,        v4i8,  "128.32.r",        load>;

  def  LSU_LDX_v2f16_VRF      : T_LSU_LDX_VRF<       VRF32_q0,        v2f16, "128.32.r",        load>;


  // Scalar loads
  // ======
  // 64-bit scalar loads
  defm LSU_LDX64              : T_LSU_LDX_IRF64_Raw< IRF64,                  "64">;

  def  LSU_LDX_i64            : T_LSU_LDX_IRF64<     IRF64,           i64,   "64">;

  // ======
  // Non-converting loads
  //    8, 16 and 32-bit scalar
  defm LSU_LDX8               : T_LSU_LDX_IRF_Raw<   IRF8_q0,                "8",                       ".0">;
  defm LSU_LDX16              : T_LSU_LDX_IRF_Raw<   IRF16_l,                "16",                      ".0">;
  defm LSU_LDX32              : T_LSU_LDX_IRF_Raw<   IRF32,                  "32">;

  def  LSU_LDX_i8             : T_LSU_LDX_IRF<       IRF8_q0,         i8,    "8",           load,       ".0">;
  def  LSU_LDX_i16            : T_LSU_LDX_IRF<       IRF16_l,         i16,   "16",          load,       ".0">;
  def  LSU_LDX_i32            : T_LSU_LDX_IRF<       IRF32,           i32,   "32"                               >;               
                                                                                                  
  def  LSU_LDX_f16            : T_LSU_LDX_IRF<       IRF16_l,         f16,   "16",          load,       ".0">;
  def  LSU_LDX_f32            : T_LSU_LDX_IRF<       IRF32,           f32,   "32"                               >;               
  //    32-bit small vector                                                                       
  def  LSU_LDX_v4i8           : T_LSU_LDX_IRF<       IRF32,           v4i8,  "32"                               >;               
  def  LSU_LDX_v2i16          : T_LSU_LDX_IRF<       IRF32,           v2i16, "32"                               >;               
                                                                                                  
  def  LSU_LDX_v2f16          : T_LSU_LDX_IRF<       IRF32,           v2f16, "32"                               >;               
  //    16-bit small vector                                                                       
  def  LSU_LDX_v2i8           : T_LSU_LDX_IRF<       IRF16_l,         v2i8,  "16",          load,       ".0">;

  // Converting loads
  //    8, 16 and 32-bit scalar
  def  LSU_LDX_a16_a32        : T_LSU_LDX_IRF<       IRF32,    i32,          "16",          extloadi16, ".0">;
  def  LSU_LDX_a8_a32         : T_LSU_LDX_IRFconv<   IRF32,    i8,    i32,   "8",           anyext,     ".0">;
  def  LSU_LDX_a8_a16         : T_LSU_LDX_IRFconv<   IRF16_l,  i8,    i16,   "8",           anyext,     ".0">;

  defm LSU_LDX32_i16_i32      : T_LSU_LDX_IRF_Raw<   IRF32,                  "32.i16.i32"                       >;
  defm LSU_LDX32_i8_i32       : T_LSU_LDX_IRF_Raw<   IRF32,                  "32.i8.i32"                        >;

  def  LSU_LDX_s16_s32        : T_LSU_LDX_IRF<       IRF32,           i32,   "32.i16.i32",  sextloadi16         >;
  def  LSU_LDX_s8_s32         : T_LSU_LDX_IRF<       IRF32,           i32,   "32.i8.i32",   sextloadi8          >;

  defm LSU_LDX32_u16_u32      : T_LSU_LDX_IRF_Raw<   IRF32,                  "32.u16.u32"                       >;
  defm LSU_LDX32_u8_u32       : T_LSU_LDX_IRF_Raw<   IRF32,                  "32.u8.u32"                        >;

  def  LSU_LDX_u16_u32        : T_LSU_LDX_IRF<       IRF32,           i32,   "32.u16.u32",  zextloadi16         >;
  def  LSU_LDX_u8_u32         : T_LSU_LDX_IRF<       IRF32,           i32,   "32.u8.u32",   zextloadi8          >;

  // Myriad2.3 new converting load
  def  LSU_LDX_f16_f32        : T_LSU_LDX_IRF<       IRF32,           f32,   "32.f16.f32",  extload             >;

  // Reinterpretting loads
  // ======
  //    64-bit reinterpret loads
  def  LSU_LDX_v2i32_as_v2f32 : T_LSU_LDX_VRFconv<   VRF64_l,  v2i32, v2f32, "64",    bitconvert, ".0">;
  def  LSU_LDX_v2i32_as_v4i16 : T_LSU_LDX_VRFconv<   VRF64_l,  v2i32, v4i16, "64",    bitconvert, ".0">;
  def  LSU_LDX_v2i32_as_v4f16 : T_LSU_LDX_VRFconv<   VRF64_l,  v2i32, v4f16, "64",    bitconvert, ".0">;
  def  LSU_LDX_v2i32_as_v8i8  : T_LSU_LDX_VRFconv<   VRF64_l,  v2i32, v8i8,  "64",    bitconvert, ".0">;
                                                                                                        
  def  LSU_LDX_v2f32_as_v2i32 : T_LSU_LDX_VRFconv<   VRF64_l,  v2f32, v2i32, "64",    bitconvert, ".0">;
  def  LSU_LDX_v2f32_as_v4i16 : T_LSU_LDX_VRFconv<   VRF64_l,  v2f32, v4i16, "64",    bitconvert, ".0">;
  def  LSU_LDX_v2f32_as_v4f16 : T_LSU_LDX_VRFconv<   VRF64_l,  v2f32, v4f16, "64",    bitconvert, ".0">;
  def  LSU_LDX_v2f32_as_v8i8  : T_LSU_LDX_VRFconv<   VRF64_l,  v2f32, v8i8,  "64",    bitconvert, ".0">;
                                                                                                        
  def  LSU_LDX_v4i16_as_v2i32 : T_LSU_LDX_VRFconv<   VRF64_l,  v4i16, v2i32, "64",    bitconvert, ".0">;
  def  LSU_LDX_v4i16_as_v2f32 : T_LSU_LDX_VRFconv<   VRF64_l,  v4i16, v2f32, "64",    bitconvert, ".0">;
  def  LSU_LDX_v4i16_as_v4f16 : T_LSU_LDX_VRFconv<   VRF64_l,  v4i16, v4f16, "64",    bitconvert, ".0">;
  def  LSU_LDX_v4i16_as_v8i8  : T_LSU_LDX_VRFconv<   VRF64_l,  v4i16, v8i8,  "64",    bitconvert, ".0">;
                                                                                                        
  def  LSU_LDX_v4f16_as_v2i32 : T_LSU_LDX_VRFconv<   VRF64_l,  v4f16, v2i32, "64",    bitconvert, ".0">;
  def  LSU_LDX_v4f16_as_v2f32 : T_LSU_LDX_VRFconv<   VRF64_l,  v4f16, v2f32, "64",    bitconvert, ".0">;
  def  LSU_LDX_v4f16_as_v4i16 : T_LSU_LDX_VRFconv<   VRF64_l,  v4f16, v4i16, "64",    bitconvert, ".0">;
  def  LSU_LDX_v4f16_as_v8i8  : T_LSU_LDX_VRFconv<   VRF64_l,  v4f16, v8i8,  "64",    bitconvert, ".0">;
                                                                                                        
  def  LSU_LDX_v8i8_as_v2i32  : T_LSU_LDX_VRFconv<   VRF64_l,  v8i8,  v2i32, "64",    bitconvert, ".0">;
  def  LSU_LDX_v8i8_as_v2f32  : T_LSU_LDX_VRFconv<   VRF64_l,  v8i8,  v2f32, "64",    bitconvert, ".0">;
  def  LSU_LDX_v8i8_as_v4i16  : T_LSU_LDX_VRFconv<   VRF64_l,  v8i8,  v4i16, "64",    bitconvert, ".0">;
  def  LSU_LDX_v8i8_as_v4f16  : T_LSU_LDX_VRFconv<   VRF64_l,  v8i8,  v4f16, "64",    bitconvert, ".0">;

  // ======
  //    32-bit reinterpret loads
  def  LSU_LDX_i32_as_f32     : T_LSU_LDX_IRFconv<   IRF32,    i32,   f32,   "32",          bitconvert          >;
  def  LSU_LDX_i32_as_v2i16   : T_LSU_LDX_IRFconv<   IRF32,    i32,   v2i16, "32",          bitconvert          >;
  def  LSU_LDX_i32_as_v2f16   : T_LSU_LDX_IRFconv<   IRF32,    i32,   v2f16, "32",          bitconvert          >;
  def  LSU_LDX_i32_as_v4i8    : T_LSU_LDX_IRFconv<   IRF32,    i32,   v4i8,  "32",          bitconvert          >;

  def  LSU_LDX_f32_as_i32     : T_LSU_LDX_IRFconv<   IRF32,    f32,   i32,   "32",          bitconvert          >;
  def  LSU_LDX_f32_as_v2i16   : T_LSU_LDX_IRFconv<   IRF32,    f32,   v2i16, "32",          bitconvert          >;
  def  LSU_LDX_f32_as_v2f16   : T_LSU_LDX_IRFconv<   IRF32,    f32,   v2f16, "32",          bitconvert          >;
  def  LSU_LDX_f32_as_v4i8    : T_LSU_LDX_IRFconv<   IRF32,    f32,   v4i8,  "32",          bitconvert          >;

  def  LSU_LDX_v2i16_as_i32   : T_LSU_LDX_IRFconv<   IRF32,    v2i16, i32,   "32",          bitconvert          >;
  def  LSU_LDX_v2i16_as_f32   : T_LSU_LDX_IRFconv<   IRF32,    v2i16, f32,   "32",          bitconvert          >;
  def  LSU_LDX_v2i16_as_v2f16 : T_LSU_LDX_IRFconv<   IRF32,    v2i16, v2f16, "32",          bitconvert          >;
  def  LSU_LDX_v2i16_as_v4i8  : T_LSU_LDX_IRFconv<   IRF32,    v2i16, v4i8,  "32",          bitconvert          >;

  def  LSU_LDX_v2f16_as_i32   : T_LSU_LDX_IRFconv<   IRF32,    v2f16, i32,   "32",          bitconvert          >;
  def  LSU_LDX_v2f16_as_f32   : T_LSU_LDX_IRFconv<   IRF32,    v2f16, f32,   "32",          bitconvert          >;
  def  LSU_LDX_v2f16_as_v2i16 : T_LSU_LDX_IRFconv<   IRF32,    v2f16, v2i16, "32",          bitconvert          >;
  def  LSU_LDX_v2f16_as_v4i8  : T_LSU_LDX_IRFconv<   IRF32,    v2f16, v4i8,  "32",          bitconvert          >;

  def  LSU_LDX_v4i8_as_i32    : T_LSU_LDX_IRFconv<   IRF32,    v4i8,  i32,   "32",          bitconvert          >;
  def  LSU_LDX_v4i8_as_f32    : T_LSU_LDX_IRFconv<   IRF32,    v4i8,  f32,   "32",          bitconvert          >;
  def  LSU_LDX_v4i8_as_v2i16  : T_LSU_LDX_IRFconv<   IRF32,    v4i8,  v2i16, "32",          bitconvert          >;
  def  LSU_LDX_v4i8_as_v2f16  : T_LSU_LDX_IRFconv<   IRF32,    v4i8,  v2f16, "32",          bitconvert          >;

  // ======
  //    16-bit reinterpret loads
  def  LSU_LDX_i16_as_f16     : T_LSU_LDX_IRFconv<   IRF16_l,  i16,   f16,   "16",          bitconvert, ".0">;
  def  LSU_LDX_i16_as_v2i8    : T_LSU_LDX_IRFconv<   IRF16_l,  i16,   v2i8,  "16",          bitconvert, ".0">;

  def  LSU_LDX_f16_as_i16     : T_LSU_LDX_IRFconv<   IRF16_l,  f16,   i16,   "16",          bitconvert, ".0">;
  def  LSU_LDX_f16_as_v2i8    : T_LSU_LDX_IRFconv<   IRF16_l,  f16,   v2i8,  "16",          bitconvert, ".0">;

  def  LSU_LDX_v2i8_as_i16    : T_LSU_LDX_IRFconv<   IRF16_l,  v2i8,  i16,   "16",          bitconvert, ".0">;
  def  LSU_LDX_v2i8_as_f16    : T_LSU_LDX_IRFconv<   IRF16_l,  v2i8,  f16,   "16",          bitconvert, ".0">;

//
//===----------------------------------------------------------------------===//
//
// 1.5.6.9 - LSU.LDXV
//    Register indirect load of 128-bits from memory with optional post-increment
//    of address by explicit mode and value.
//
//    TODO
//
//===----------------------------------------------------------------------===//
//
// 1.5.6.10 - LSU.PFA
//    Data L1 Cache prefetch from direct address.
//
//    TODO
//
//===----------------------------------------------------------------------===//
//

//
// 1.5.6.11 - LSU.PFO
//    Data L1 Cache prefetch from displacement address and Data L1 Cache control.
//

let mayLoad = 1, mayStore = 1, Itinerary = II_LSU_PFO_IRF in {
  def LSU_CPFL1 : SHAVE_LSUInstr<(outs), (ins MEMr:$addr, i32imm:$readwrite, i32imm:$locality, i32imm:$isData),
                                 ".CPFL1 $addr 0",
                                 [(prefetch ADDRr:$addr, (i32 timm:$readwrite), (i32 timm:$locality), (i32 timm:$isData))],
                                 [isPrefetch]>;
}

//
// 1.5.6.12 - LSU.ST
//    Register indirect store of specified type to memory.
//
class T_LSU_ST_Base<RegisterClass srcRC, string asmstr, string lane, list<dag> pattern> :
      SHAVE_LSUInstr<(outs),
                     (ins srcRC:$src, MEMr:$addr),
                     !strconcat(".ST.", asmstr, " $src", lane, " $addr"),
                     pattern> {
  let mayStore = 1;
}

//    Specialise for VRF itineraries
class T_LSU_ST_VRF_Raw_Base<RegisterClass srcRC, string asmstr, string lane> :
      T_LSU_ST_Base<srcRC, asmstr, lane, []> {
  let Itinerary = II_LSU_ST_VRF;
}
multiclass T_LSU_ST_VRF_Raw<RegisterClass srcRC, string asmstr, string lane = ""> {
  def  _Raw   : T_LSU_ST_VRF_Raw_Base<srcRC, asmstr, lane>;
}

class T_LSU_ST_VRF<RegisterClass srcRC, ValueType srcVT, string asmstr, PatFrag storefrag = store, string lane = "", list<Predicate> requires = []> :
      T_LSU_ST_Base<srcRC, asmstr, lane, [(storefrag (srcVT srcRC:$src), ADDRr:$addr)]>, Requires<!listconcat(requires, [HasVRF128_Predicate])> {
  let Itinerary = II_LSU_ST_VRF;
}

class T_LSU_STX_Myr2v3_Base<RegisterClass srcRC, string asmstr, string lane, list<dag> pattern, list<Predicate> predicates = []> :
      SHAVE_LSUInstr<(outs),
                     (ins srcRC:$src, MEMr32r32:$addr),
                     !strconcat(".STX.", asmstr, " $src", lane, " $addr"),
                     pattern, [isLDXorSTX]>,
      Requires<!listconcat(predicates, [EnableLDXSTX])> {
  let mayStore = 1;
}

class T_LSU_STX_VRFelem_Myr2v3_Base<RegisterClass srcRC, string asmstr, list<dag> pattern> :
      SHAVE_LSUInstr<(outs),
                     (ins srcRC:$src, i8imm:$pos, MEMr32r32:$addr),
                     !strconcat(".STX.", asmstr, " $src.$pos", " $addr"),
                     pattern, [isLDXorSTX]>,
      Requires<[HasVRF128_Predicate, EnableLDXSTX]> {
  let mayStore = 1;
}

//  Myriad2.3 LSU.ST.xx Vsa.col Isb variants
class T_LSU_ST_VRFelem_Myr2v3<ValueType srcTy, RegisterClass srcRC, string asmstr,ValueType elemTy>:
  SHAVE_LSUInstr<(outs), (ins srcRC:$src, i8imm:$pos, MEMr:$addr),
                 !strconcat(".ST", asmstr, " $src.$pos $addr"),
                 [(store (elemTy (vector_extract (srcTy srcRC:$src), imm:$pos)), ADDRr:$addr)]>,
    Requires<[HasVRF128_Predicate]> {
  let Itinerary = II_LSU_ST_VRF;
  let mayStore = 1;
}

def T_LSU_ST_VRFelem_Myr2v3_v4f32: T_LSU_ST_VRFelem_Myr2v3 <v4f32,  VRF128, ".32", f32>;
def T_LSU_ST_VRFelem_Myr2v3_v4i32: T_LSU_ST_VRFelem_Myr2v3 <v4i32,  VRF128, ".32", i32>;
def T_LSU_ST_VRFelem_Myr2v3_v2f32: T_LSU_ST_VRFelem_Myr2v3 <v2f32, VRF64_l, ".32", f32>;
def T_LSU_ST_VRFelem_Myr2v3_v2i32: T_LSU_ST_VRFelem_Myr2v3 <v2i32, VRF64_l, ".32", i32>;

def T_LSU_ST_VRFelem_Myr2v3_v8f16: T_LSU_ST_VRFelem_Myr2v3 <v8f16,  VRF128, ".16", f16>;
def T_LSU_ST_VRFelem_Myr2v3_v8i16: T_LSU_ST_VRFelem_Myr2v3 <v8i16,  VRF128, ".16", i16>;
def T_LSU_ST_VRFelem_Myr2v3_v4f16: T_LSU_ST_VRFelem_Myr2v3 <v4f16, VRF64_l, ".16", f16>;
def T_LSU_ST_VRFelem_Myr2v3_v4i16: T_LSU_ST_VRFelem_Myr2v3 <v4i16, VRF64_l, ".16", i16>;
def T_LSU_ST_VRFelem_Myr2v3_v16i8: T_LSU_ST_VRFelem_Myr2v3 <v16i8,  VRF128,  ".8",  i8>;
def T_LSU_ST_VRFelem_Myr2v3_v8i8 : T_LSU_ST_VRFelem_Myr2v3 < v8i8, VRF64_l,  ".8",  i8>;

class T_LSU_ST_VRFconv<RegisterClass srcRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode, string lane = ""> :
      T_LSU_ST_Base<srcRC, asmstr, lane,
                    [(store (dstVT (convMode (srcVT srcRC:$src))), (i32 ADDRr:$addr))]>,
      Requires<[HasVRF128_Predicate]> {
  let Itinerary = II_LSU_ST_VRF;
}
class T_LSU_STX_VRFconv_Myr2v3<RegisterClass srcRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode, string lane = ""> :
      T_LSU_STX_Myr2v3_Base<srcRC, asmstr, lane, [(store (dstVT (convMode (srcVT srcRC:$src))), (i32 ADDRrr:$addr))], [HasVRF128_Predicate]> {
  let Itinerary = II_LSU_STX_VRF;
}

//    Specialise for 32-bit IRF itineraries
class T_LSU_ST_IRF_Raw_Base<RegisterClass srcRC, string asmstr, string lane> :
      T_LSU_ST_Base<srcRC, asmstr, lane, []> {
  let Itinerary = II_LSU_ST_IRF;
}
multiclass T_LSU_ST_IRF_Raw<RegisterClass srcRC, string asmstr, string lane = ".0"> {
  def  _Raw : T_LSU_ST_IRF_Raw_Base<srcRC, asmstr, lane>;
}

class T_LSU_ST_IRF<RegisterClass srcRC, ValueType srcVT, string asmstr, PatFrag storefrag = store, string lane = ""> :
      T_LSU_ST_Base<srcRC, asmstr, lane, [(storefrag (srcVT srcRC:$src), ADDRr:$addr)]> {
  let Itinerary = II_LSU_ST_IRF;
}

class T_LSU_STX_IRF_Myr2v3<RegisterClass srcRC, ValueType srcVT, string asmstr, PatFrag storefrag = store, string lane = ""> :
       T_LSU_STX_Myr2v3_Base<srcRC, asmstr, lane, [(storefrag (srcVT srcRC:$src), ADDRr32r32:$addr)]> {
  let Itinerary = II_LSU_STX_IRF;
}

class T_LSU_STX_VRFelem_Myr2v3<RegisterClass srcRC, ValueType srcVT, ValueType elemTy, string asmstr, PatFrag storefrag = store> :
        T_LSU_STX_VRFelem_Myr2v3_Base<srcRC, asmstr, [(storefrag (elemTy (vector_extract (srcVT srcRC:$src), imm:$pos)), ADDRr32r32:$addr)]> {
  let Itinerary = II_LSU_STX_VRF;
}

// FIXME: Movidius - instead of the 'truncstoreN' fragments, see if it can be generalised to a converting pattern
class T_LSU_ST_IRFtrunc<RegisterClass srcRC, ValueType srcVT, string asmstr, PatFrag storeFrag, string lane = ""> :
      T_LSU_ST_Base<srcRC, asmstr, lane, [(storeFrag (srcVT srcRC:$src), ADDRr:$addr)]> {
  let Itinerary = II_LSU_ST_IRF;
}

class T_LSU_STX_IRFtrunc_Myr2v3<RegisterClass srcRC, ValueType srcVT, string asmstr, PatFrag storeFrag, string lane = ""> :
      T_LSU_STX_Myr2v3_Base<srcRC, asmstr, lane, [(storeFrag (srcVT srcRC:$src), ADDRrr:$addr)]> {
  let Itinerary = II_LSU_STX_IRF;
}

class T_LSU_ST_IRFconv_Myr2v3<RegisterClass srcRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode, string lane = ".0"> :
      T_LSU_ST_Base<srcRC, asmstr, lane,
                    [(store (dstVT (convMode (srcVT srcRC:$src))), (i32 ADDRr:$addr))]>{
  let Itinerary = II_LSU_ST_IRF;
}
  def  LSU_ST_f32_f16        : T_LSU_ST_IRFconv_Myr2v3<IRF32,    f32, f16,         "16.f32.f16",          fpround, "">;

let Constraints = "$dst = $ptr", mayStore = 1, Itinerary = II_LSU_STID_IRF in {
  class LSU_STI_IRFconv_Myr2v3<RegisterClass srcRC, ValueType srcVT, ValueType dstVT, SDNode convMode, string asmstr, dag inc> :
      SHAVE_LSUInstr<(outs IRF32:$ptr), (ins srcRC:$val, MEMr:$dst),
                 !strconcat(".STI", asmstr," $val $dst"),
  [(set IRF32:$ptr, (shave_istore (dstVT (convMode (srcVT srcRC:$val))), ADDRr:$dst, inc))]>;
}
  def  LSU_STI_f32_f16       : LSU_STI_IRFconv_Myr2v3<IRF32,    f32, f16,       fpround,  ".16.f32.f16", (i32 2)>;

class T_LSU_STX_IRFconv_Myr2v3<RegisterClass srcRC, ValueType srcVT, ValueType dstVT, string asmstr, SDNode convMode, string lane = ".0"> :
      T_LSU_STX_Myr2v3_Base<srcRC, asmstr, lane, [(store (dstVT (convMode (srcVT srcRC:$src))), (i32 ADDRrr:$addr))]> {
  let Itinerary = II_LSU_STX_IRF;
}

def  LSU_STX_f32_f16        : T_LSU_STX_IRFconv_Myr2v3<  IRF32,    f32, f16,         "16.f32.f16",          fpround, "">;

//    Specialise for 64-bit IRF itineraries
class T_LSU_ST_IRF64_Raw_Base<RegisterClass srcRC, string asmstr> :
      T_LSU_ST_Base<srcRC, asmstr, "", []> {
  let Itinerary = II_LSU_ST_IRF64;

  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}
multiclass T_LSU_ST_IRF64_Raw<RegisterClass srcRC, string asmstr> {
  def  _Raw : T_LSU_ST_IRF64_Raw_Base<srcRC, asmstr>;
}

class T_LSU_ST_IRF64<RegisterClass srcRC, ValueType srcVT, string asmstr> :
      T_LSU_ST_Base<srcRC, asmstr, "", [(store (srcVT srcRC:$src), ADDRr:$addr)]> {
  let Itinerary = II_LSU_ST_IRF64;
  let Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate];
}

// store instructions with post increment
let Constraints = "$dst = $ptr" in {
  class LSU_STI_32_Class<RegisterClass RC, ValueType ty, string opcode, dag inc> :
      SHAVE_LSUInstr<(outs IRF32:$ptr), (ins RC:$val, MEMr:$dst),
                      !strconcat(opcode, " $val $dst"),
                      [(set IRF32:$ptr, (shave_istore (ty RC:$val), ADDRr:$dst, inc))]>;
  class LSU_STI_XX_Class<RegisterClass RC, ValueType ty, string opcode, dag inc> :
      SHAVE_LSUInstr<(outs IRF32:$ptr), (ins RC:$val, MEMr:$dst),
                      !strconcat(opcode, " $val.0 $dst"),
                      [(set IRF32:$ptr, (shave_istore (ty RC:$val), ADDRr:$dst, inc))]>;

  class LSU_STI_32_Myriad<RegisterClass RC, ValueType ty, string asmstr, dag inc> :
      SHAVE_LSUInstr<(outs IRF32:$ptr), (ins RC:$val, MEMr:$dst),
                     !strconcat(".STI.", asmstr, " $val $dst"),
                     [(set IRF32:$ptr, (shave_istore (ty RC:$val), ADDRr:$dst, inc))]>;
  class LSU_STI_XX_Myriad<RegisterClass RC, ValueType ty, string asmstr, dag inc> :
      SHAVE_LSUInstr<(outs IRF32:$ptr), (ins RC:$val, MEMr:$dst),
                     !strconcat(".STI.", asmstr, " $val.0 $dst"),
                     [(set IRF32:$ptr, (shave_istore (ty RC:$val), ADDRr:$dst, inc))]>;

  class LSU_STI_Myr2v3_implicit<RegisterClass RC, ValueType ty, PatFrag frag, string asmstr, dag inc> :
                      SHAVE_LSUInstr<(outs IRF32:$ptr), (ins RC:$val, MEMr:$dst),
                      !strconcat(asmstr, " $val $dst"),
                      [(set IRF32:$ptr, (frag (ty RC:$val), ADDRr:$dst, inc))]>;

  class LSU_STI_Myr2v3_explicit<RegisterClass RC, ValueType ty, PatFrag frag, string asmstr> :
                      SHAVE_LSUInstr<(outs IRF32:$ptr), (ins RC:$val, MEMr:$dst, IRF32:$inc),
                      !strconcat(asmstr, " $val $dst $inc"),
                      [(set IRF32:$ptr, (frag (ty RC:$val), ADDRr:$dst, IRF32:$inc))]>;

  class LSU_STI_Trunc_Myriad<RegisterClass RC, ValueType ty, PatFrag frag, string asmstr> :
      SHAVE_LSUInstr<(outs IRF32:$ptr), (ins RC:$val, MEMr:$dst, i32imm:$inc),
                     !strconcat(".STI.", asmstr, " $val.0 $dst"),
                     [(set IRF32:$ptr, (frag (ty RC:$val), ADDRr:$dst, imm:$inc))]>;
}


  // Vector stores
  // ======
  // 64-bit ST.64.l and ST.64.h stores
  defm LSU_ST64_h            : T_LSU_ST_VRF_Raw<   VRF64_h,                "64",              ".1">;

  def  LSU_ST_v2i32_h        : T_LSU_ST_VRF<       VRF64_h,         v2i32, "64",       store, ".1">;
  def  LSU_ST_v4i16_h        : T_LSU_ST_VRF<       VRF64_h,         v4i16, "64",       store, ".1">;
  def  LSU_ST_v8i8_h         : T_LSU_ST_VRF<       VRF64_h,         v8i8,  "64",       store, ".1">;

  def  LSU_ST_v4f16_h        : T_LSU_ST_VRF<       VRF64_h,         v4f16, "64",       store, ".1">;

  defm LSU_ST64_l            : T_LSU_ST_VRF_Raw<   VRF64_l,                "64",              ".0">;

  def  LSU_ST_v2i32          : T_LSU_ST_VRF<       VRF64_l,         v2i32, "64",       store, ".0">;
  def  LSU_ST_v4i16          : T_LSU_ST_VRF<       VRF64_l,         v4i16, "64",       store, ".0">;
  def  LSU_ST_v8i8           : T_LSU_ST_VRF<       VRF64_l,         v8i8,  "64",       store, ".0">;

  def  LSU_ST_v4f16          : T_LSU_ST_VRF<       VRF64_l,         v4f16, "64",       store, ".0">;

  // ======
  // 16-bit to unsigned 8-bit converting ST.128.f16.u8 stores
  defm LSU_ST128_f16_u8      : T_LSU_ST_VRF_Raw<   VRF128,                 "64.f16.u8">;

  def  LSU_ST_v8f16_v8u8     : T_LSU_ST_VRFconv<   VRF128,   v8f16, v8i8,  "64.f16.u8",  fp_to_uint>;
  def  LSU_ST_v8f16_v8u8s     : T_LSU_ST_VRFconv<   VRF128,   v8f16, v8i8,  "64.f16.u8s",  fp_to_uint>;

  def  LSU_STX_v8f16_v8u8     : T_LSU_STX_VRFconv_Myr2v3<   VRF128,   v8f16, v8i8,  "64.f16.u8",  fp_to_uint>;


let Itinerary = II_LSU_STX_VRF in {
  def LSU_STX_v4f32_v4f16     : T_LSU_STX_Myr2v3_Base<VRF128, "64.f32.f16", "", [(shave_truncstore_vNf16 (v4f32 VRF128:$src), (i32 ADDRr32r32:$addr))], [HasVRF128_Predicate]>;
  def LSU_STX_v4i32_v4i8      : T_LSU_STX_Myr2v3_Base<VRF128, "32.i32.i8", "", [(shave_truncstore_vNi8 (v4i32 VRF128:$src), (i32 ADDRrr:$addr))], [HasVRF128_Predicate]>;
  def LSU_STX_v8i16_v8i8      : T_LSU_STX_Myr2v3_Base<VRF128, "64.i16.i8", "", [(shave_truncstore_vNi8 (v8i16 VRF128:$src), (i32 ADDRrr:$addr))], [HasVRF128_Predicate]>;
  def LSU_STX_v4i32_v4i16     : T_LSU_STX_Myr2v3_Base<VRF128, "64.i32.i16", "", [(shave_truncstore_vNi16 (v4i32 VRF128:$src), (i32 ADDRrr:$addr))], [HasVRF128_Predicate]>;
}

  let Itinerary = II_LSU_STID_VRF, mayStore = 1 in {
    def LSU_STI_v4f32_v4f16    : LSU_STI_Myr2v3_implicit <VRF128, v4f32, shave_itruncstore_vector, ".STI.64.f32.f16", (i32 8)>;
    def LSU_STI_v4i32_v4i16    : LSU_STI_Myr2v3_implicit <VRF128, v4i32, shave_itruncstore_vector, ".STI.64.i32.i16", (i32 8)>;
    def LSU_STI_v4i32_v4i8     : LSU_STI_Myr2v3_implicit <VRF128, v4i32, shave_itruncstore_vector, ".STI.32.i32.i8", (i32 4)>;
    def LSU_STI_v8i16_v8i8     : LSU_STI_Myr2v3_implicit <VRF128, v8i16, shave_itruncstore_vector, ".STI.64.i16.i8", (i32 8)>;
    //    def LSU_STI_v4f32_v4f16r   : LSU_STI_Myr2v3_explicit <VRF128, v4f32, shave_itruncstore_vector, ".STI.64.f32.f16">;
}

let Itinerary = II_LSU_ST_VRF, mayStore = 1 in {
  def LSU_ST_v4f32_v4f16     : T_LSU_ST_Base<VRF128, "64.f32.f16", "", [(shave_truncstore_vNf16 (v4f32 VRF128:$src), (i32 ADDRr:$addr))]>, Requires<[HasVRF128_Predicate]>;
  def LSU_ST_v4i32_v4i8      : T_LSU_ST_Base<VRF128, "32.i32.i8", "", [(shave_truncstore_vNi8 (v4i32 VRF128:$src), (i32 ADDRr:$addr))]>, Requires<[HasVRF128_Predicate]>;
  def LSU_ST_v8i16_v8i8      : T_LSU_ST_Base<VRF128, "64.i16.i8", "", [(shave_truncstore_vNi8 (v8i16 VRF128:$src), (i32 ADDRr:$addr))]>, Requires<[HasVRF128_Predicate]>;
  def LSU_ST_v4i32_v4i16     : T_LSU_ST_Base<VRF128, "64.i32.i16", "", [(shave_truncstore_vNi16 (v4i32 VRF128:$src), (i32 ADDRr:$addr))]>, Requires<[HasVRF128_Predicate]>;
}

let Itinerary = II_LSU_STX_IRF in {
  def LSU_STX_v2i16_v2i8      : T_LSU_STX_Myr2v3_Base<IRF32, "16.i16.i8", "", [(shave_truncstore_vNi8 (v2i16 IRF32:$src), (i32 ADDRrr:$addr))]>;
}

let Itinerary = II_LSU_ST_IRF in {
  def LSU_ST_v2i16_v2i8      : T_LSU_ST_Base<IRF32, "16.i16.i8", "", [(shave_truncstore_vNi8 (v2i16 IRF32:$src), (i32 ADDRr:$addr))]>;
}

  // ======
  // 16-bit FP to U8F converting ST.128.f16.u8f stores
  defm LSU_ST128_f16_u8f     : T_LSU_ST_VRF_Raw<   VRF128,                 "64.f16.u8fs">;

  let Itinerary = II_LSU_ST_VRF in {
    def LSU_ST128_f16_u8f      : T_LSU_ST_Base<VRF128, "64.f16.u8fs", "",
                                               [(store (v8i8 (fp_to_uint (v8f16 (fmul (v8f16 VRF128:$src), (v8f16 (splat (f16 u8fConst))))))), (i32 ADDRr:$addr))]>;
  }

  let Itinerary = II_LSU_STX_VRF in {
    def LSU_STX128_f16_u8f      : T_LSU_STX_Myr2v3_Base<VRF128, "64.f16.u8fs", "",
                                               [(store (v8i8 (fp_to_uint (v8f16 (fmul (v8f16 VRF128:$src), (v8f16 (splat (f16 u8fConst))))))), (i32 ADDRrr:$addr))], [HasVRF128_Predicate]>;
  }

  // ======
  // F32 to F16 rounding 128-bit stores
  defm LSU_ST128_f32_f16     : T_LSU_ST_VRF_Raw<   VRF128,                 "64.f32.f16">;

//  def  LSU_ST_v4f32_v4f16    : T_LSU_ST_VRFconv<   VRF128,   v4f32, v4f16, "64.f16.f32", fround>;

  // ======
  // Integer converting stores
  defm LSU_ST128_u32_u16     : T_LSU_ST_VRF_Raw<   VRF128,                 "64.u32.u16">;
  defm LSU_ST128_i32_i16     : T_LSU_ST_VRF_Raw<   VRF128,                 "64.i32.i16">;
  defm LSU_ST128_u16_u8      : T_LSU_ST_VRF_Raw<   VRF128,                 "64.u16.u8">;
  defm LSU_ST128_i16_i8      : T_LSU_ST_VRF_Raw<   VRF128,                 "64.i16.i8">;

//  def  LSU_ST_v4u32_v4u16    : T_LSU_ST_VRFconv<   VRF128,   v4i32, v4i16, "64.u32.u16", trunc/*saturate*/>;
//  def  LSU_ST_v4s32_v4s16    : T_LSU_ST_VRFconv<   VRF128,   v4i32, v4i16, "64.i32.i16", trunc/*saturate*/>;
//  def  LSU_ST_v8u16_v8u8     : T_LSU_ST_VRFconv<   VRF128,   v8i16, v8i8,  "64.u16.u8",  trunc/*saturate*/>;
//  def  LSU_ST_v8s16_v8s8     : T_LSU_ST_VRFconv<   VRF128,   v8i16, v8i8,  "64.i16.i8",  trunc/*saturate*/>;


  // Scalar stores
  // ======
  // 64-bit scalar stores
  defm LSU_ST64              : T_LSU_ST_IRF64_Raw< IRF64,                  "64">;

  def  LSU_ST_i64            : T_LSU_ST_IRF64<     IRF64,           i64,   "64">;

  // ======
  // Non-converting stores
  //    8, 16 and 32-bit scalar
  defm LSU_ST32              : T_LSU_ST_IRF_Raw<   IRF32,                  "32", "">;
  defm LSU_ST16              : T_LSU_ST_IRF_Raw<   IRF16_l,                "16">;
  defm LSU_ST8               : T_LSU_ST_IRF_Raw<   IRF8_q0,                "8">;

  def  LSU_ST_i32            : T_LSU_ST_IRF<       IRF32,           i32,   "32">;
  def  LSU_ST_i16            : T_LSU_ST_IRF<       IRF16_l,         i16,   "16",          store, ".0">;
  def  LSU_ST_i8             : T_LSU_ST_IRF<       IRF8_q0,         i8,    "8",           store, ".0">;

  def  LSU_ST_f32            : T_LSU_ST_IRF<       IRF32,           f32,   "32">;
  def  LSU_ST_f16            : T_LSU_ST_IRF<       IRF16_l,         f16,   "16",          store, ".0">;
  //    32-bit small vector
  def  LSU_ST_v4i8           : T_LSU_ST_IRF<       IRF32,           v4i8,  "32">;
  def  LSU_ST_v2i16          : T_LSU_ST_IRF<       IRF32,           v2i16, "32">;

  def  LSU_ST_v2f16          : T_LSU_ST_IRF<       IRF32,           v2f16, "32">;
  //    16-bit small vector
  def  LSU_ST_v2i8           : T_LSU_ST_IRF<       IRF16_l,         v2i8,  "16",          store, ".0">;

  //    Indexed scalar stores (Myriad2.3 only)
  def  LSU_STX_i32           : T_LSU_STX_IRF_Myr2v3<       IRF32,           i32,   "32">;
  def  LSU_STX_f32           : T_LSU_STX_IRF_Myr2v3<       IRF32,           f32,   "32">;
  def  LSU_STX_i16           : T_LSU_STX_IRF_Myr2v3<       IRF16_l,         i16,   "16",          store, ".0">;
  def  LSU_STX_f16           : T_LSU_STX_IRF_Myr2v3<       IRF16_l,         f16,   "16",          store, ".0">;
  def  LSU_STX_i8            : T_LSU_STX_IRF_Myr2v3<       IRF8_q0,         i8,    "8",           store, ".0">;

  //    Indexed vector stores (Myriad2.3 only)
  def  LSU_STX_v4i32         : T_LSU_STX_VRFelem_Myr2v3<   VRF128,          v4i32, i32,   "32">;
  def  LSU_STX_v4f32         : T_LSU_STX_VRFelem_Myr2v3<   VRF128,          v4f32, f32,   "32">;
  def  LSU_STX_v2i32         : T_LSU_STX_VRFelem_Myr2v3<   VRF64_l,          v2i32, i32,   "32">;
  def  LSU_STX_v2f32         : T_LSU_STX_VRFelem_Myr2v3<   VRF64_l,          v2f32, f32,   "32">;
  def  LSU_STX_v8i16         : T_LSU_STX_VRFelem_Myr2v3<   VRF128,          v8i16, i16,   "16">;
  def  LSU_STX_v8f16         : T_LSU_STX_VRFelem_Myr2v3<   VRF128,          v8f16, f16,   "16">;
  def  LSU_STX_v4i16         : T_LSU_STX_VRFelem_Myr2v3<   VRF64_l,         v4i16, i16,   "16">;
  def  LSU_STX_v4f16         : T_LSU_STX_VRFelem_Myr2v3<   VRF64_l,         v4f16, f16,   "16">;
  def  LSU_STX_v16i8         : T_LSU_STX_VRFelem_Myr2v3<    VRF128,         v16i8, i8,   "8">;
  def  LSU_STX_v8i8          : T_LSU_STX_VRFelem_Myr2v3<   VRF64_l,          v8i8, i8,   "8">;

  def  LSU_STX64_Raw     : SHAVE_LSUInstr<(outs), (ins VRF64_l:$src, MEMr32r32:$addr),
			       ".STX.64 $src.0 $addr">, Requires<[EnableLDXSTX, HasVRF128_Predicate]> {
  let mayStore = 1;
  let Itinerary = II_LSU_STX_VRF;
}

  // Truncating stores
  def  LSU_ST_i32_i16        : T_LSU_ST_IRFtrunc        <  IRF32,    i32,          "16",          truncstorei16,".0">;
  def  LSU_STX_i32_i16       : T_LSU_STX_IRFtrunc_Myr2v3<  IRF32,    i32,          "16",          truncstorei16,".0">;

  def  LSU_ST_i32_i8         : T_LSU_ST_IRFtrunc        <  IRF32,    i32,          "8",           truncstorei8, ".0">;
  def  LSU_STX_i32_i8        : T_LSU_STX_IRFtrunc_Myr2v3<  IRF32,    i32,          "8",           truncstorei8, ".0">;

  def  LSU_ST_i16_i8         : T_LSU_ST_IRFtrunc        <  IRF16_l,  i16,          "8",           truncstorei8, ".0">;

  // Converting stores
  //    8, 16 and 32-bit scalar
  defm LSU_ST32_i32_i16      : T_LSU_ST_IRF_Raw<   IRF32,                  "32.i32.i16">;
  defm LSU_ST32_i32_i8       : T_LSU_ST_IRF_Raw<   IRF32,                  "32.i32.i8">;

//  def  LSU_ST_s32_s16s       : T_LSU_ST_IRFconv<   IRF32,    i32,   i16,   "32.i32.i16",  trunc/*saturate*/>;
//  def  LSU_ST_s32_s8s        : T_LSU_ST_IRFconv<   IRF32,    i32,   i8,    "32.i32.i8",   trunc/*saturate*/>;

  defm LSU_ST32_u32_u16      : T_LSU_ST_IRF_Raw<   IRF32,                  "32.u32.u16">;
  defm LSU_ST32_u32_u8       : T_LSU_ST_IRF_Raw<   IRF32,                  "32.u32.u8">;

//  def  LSU_ST_u32_u16s       : T_LSU_ST_IRFconv<   IRF32,    i16,   i32,   "32.u32.u16",  trunc/*saturate*/>;
//  def  LSU_ST_u32_u8s        : T_LSU_ST_IRFconv<   IRF32,    i8,    i32,   "32.u32.u8",   trunc/*saturate*/>;

class T_LSU_ST_IRFconv_Raw<RegisterClass srcRC, string asmstr, string lane = ""> :
      T_LSU_ST_Base<srcRC, asmstr, lane, []> {
  let Itinerary = II_LSU_ST_IRF;
}

// Sub-byte converting stores
def LSU_ST16_i8_i4_Raw     : T_LSU_ST_IRFconv_Raw<IRF32, "16.i8.i4">;
def LSU_ST16_u8_u4_Raw     : T_LSU_ST_IRFconv_Raw<IRF32, "16.u8.u4">;
def LSU_ST16_i8_i4s_Raw    : T_LSU_ST_IRFconv_Raw<IRF32, "16.i8.i4s">;
def LSU_ST16_u8_u4s_Raw    : T_LSU_ST_IRFconv_Raw<IRF32, "16.u8.u4s">;

  // Reinterpretting stores
  // ======
  //    64-bit reinterpret stores
//  def  LSU_ST_v2i32_as_v2f32 : T_LSU_ST_VRFconv<   VRF64_l,  v2i32, v2f32, "64.l",        bitconvert>;
//  def  LSU_ST_v2i32_as_v4i16 : T_LSU_ST_VRFconv<   VRF64_l,  v2i32, v4i16, "64.l",        bitconvert>;
//  def  LSU_ST_v2i32_as_v4f16 : T_LSU_ST_VRFconv<   VRF64_l,  v2i32, v4f16, "64.l",        bitconvert>;
//  def  LSU_ST_v2i32_as_v8i8  : T_LSU_ST_VRFconv<   VRF64_l,  v2i32, v8i8,  "64.l",        bitconvert>;

//  def  LSU_ST_v2f32_as_v2i32 : T_LSU_ST_VRFconv<   VRF64_l,  v2f32, v2i32, "64.l",        bitconvert>;
//  def  LSU_ST_v2f32_as_v4i16 : T_LSU_ST_VRFconv<   VRF64_l,  v2f32, v4i16, "64.l",        bitconvert>;
//  def  LSU_ST_v2f32_as_v4f16 : T_LSU_ST_VRFconv<   VRF64_l,  v2f32, v4f16, "64.l",        bitconvert>;
//  def  LSU_ST_v2f32_as_v8i8  : T_LSU_ST_VRFconv<   VRF64_l,  v2f32, v8i8,  "64.l",        bitconvert>;

//  def  LSU_ST_v4i16_as_v2i32 : T_LSU_ST_VRFconv<   VRF64_l,  v4i16, v2i32, "64.l",        bitconvert>;
//  def  LSU_ST_v4i16_as_v2f32 : T_LSU_ST_VRFconv<   VRF64_l,  v4i16, v2f32, "64.l",        bitconvert>;
//  def  LSU_ST_v4i16_as_v4f16 : T_LSU_ST_VRFconv<   VRF64_l,  v4i16, v4f16, "64.l",        bitconvert>;
//  def  LSU_ST_v4i16_as_v8i8  : T_LSU_ST_VRFconv<   VRF64_l,  v4i16, v8i8,  "64.l",        bitconvert>;

//  def  LSU_ST_v4f16_as_v2i32 : T_LSU_ST_VRFconv<   VRF64_l,  v4f16, v2i32, "64.l",        bitconvert>;
//  def  LSU_ST_v4f16_as_v2f32 : T_LSU_ST_VRFconv<   VRF64_l,  v4f16, v2f32, "64.l",        bitconvert>;
//  def  LSU_ST_v4f16_as_v4i16 : T_LSU_ST_VRFconv<   VRF64_l,  v4f16, v4i16, "64.l",        bitconvert>;
//  def  LSU_ST_v4f16_as_v8i8  : T_LSU_ST_VRFconv<   VRF64_l,  v4f16, v8i8,  "64.l",        bitconvert>;

//  def  LSU_ST_v8i8_as_v2i32  : T_LSU_ST_VRFconv<   VRF64_l,  v8i8,  v2i32, "64.l",        bitconvert>;
//  def  LSU_ST_v8i8_as_v2f32  : T_LSU_ST_VRFconv<   VRF64_l,  v8i8,  v2f32, "64.l",        bitconvert>;
//  def  LSU_ST_v8i8_as_v4i16  : T_LSU_ST_VRFconv<   VRF64_l,  v8i8,  v4i16, "64.l",        bitconvert>;
//  def  LSU_ST_v8i8_as_v4f16  : T_LSU_ST_VRFconv<   VRF64_l,  v8i8,  v4f16, "64.l",        bitconvert>;

  // ======
  //    32-bit reinterpret stores
//  def  LSU_ST_i32_as_f32     : T_LSU_ST_IRFconv<   IRF32,    i32,   f32,   "32",          bitconvert>;
//  def  LSU_ST_i32_as_v2i16   : T_LSU_ST_IRFconv<   IRF32,    i32,   v2i16, "32",          bitconvert>;
//  def  LSU_ST_i32_as_v2f16   : T_LSU_ST_IRFconv<   IRF32,    i32,   v2f16, "32",          bitconvert>;
//  def  LSU_ST_i32_as_v4i8    : T_LSU_ST_IRFconv<   IRF32,    i32,   v4i8,  "32",          bitconvert>;

//  def  LSU_ST_f32_as_i32     : T_LSU_ST_IRFconv<   IRF32,    f32,   i32,   "32",          bitconvert>;
//  def  LSU_ST_f32_as_v2i16   : T_LSU_ST_IRFconv<   IRF32,    f32,   v2i16, "32",          bitconvert>;
//  def  LSU_ST_f32_as_v2f16   : T_LSU_ST_IRFconv<   IRF32,    f32,   v2f16, "32",          bitconvert>;
//  def  LSU_ST_f32_as_v4i8    : T_LSU_ST_IRFconv<   IRF32,    f32,   v4i8,  "32",          bitconvert>;

//  def  LSU_ST_v2i16_as_i32   : T_LSU_ST_IRFconv<   IRF32,    v2i16, i32,   "32",          bitconvert>;
//  def  LSU_ST_v2i16_as_f32   : T_LSU_ST_IRFconv<   IRF32,    v2i16, f32,   "32",          bitconvert>;
//  def  LSU_ST_v2i16_as_v2f16 : T_LSU_ST_IRFconv<   IRF32,    v2i16, v2f16, "32",          bitconvert>;
//  def  LSU_ST_v2i16_as_v4i8  : T_LSU_ST_IRFconv<   IRF32,    v2i16, v4i8,  "32",          bitconvert>;

//  def  LSU_ST_v2f16_as_i32   : T_LSU_ST_IRFconv<   IRF32,    v2f16, i32,   "32",          bitconvert>;
//  def  LSU_ST_v2f16_as_f32   : T_LSU_ST_IRFconv<   IRF32,    v2f16, f32,   "32",          bitconvert>;
//  def  LSU_ST_v2f16_as_v2i16 : T_LSU_ST_IRFconv<   IRF32,    v2f16, v2i16, "32",          bitconvert>;
//  def  LSU_ST_v2f16_as_v4i8  : T_LSU_ST_IRFconv<   IRF32,    v2f16, v4i8,  "32",          bitconvert>;

//  def  LSU_ST_v4i8_as_i32    : T_LSU_ST_IRFconv<   IRF32,    v4i8,  i32,   "32",          bitconvert>;
//  def  LSU_ST_v4i8_as_f32    : T_LSU_ST_IRFconv<   IRF32,    v4i8,  f32,   "32",          bitconvert>;
//  def  LSU_ST_v4i8_as_v2i16  : T_LSU_ST_IRFconv<   IRF32,    v4i8,  v2i16, "32",          bitconvert>;
//  def  LSU_ST_v4i8_as_v2f16  : T_LSU_ST_IRFconv<   IRF32,    v4i8,  v2f16, "32",          bitconvert>;

  // ======
  //    16-bit reinterpret stores
//  def  LSU_ST_i16_as_f16     : T_LSU_ST_IRFconv<   IRF16_l,  i16,   f16,   "16",          bitconvert>;
//  def  LSU_ST_i16_as_v2i8    : T_LSU_ST_IRFconv<   IRF16_l,  i16,   v2i8,  "16",          bitconvert>;

//  def  LSU_ST_f16_as_i16     : T_LSU_ST_IRFconv<   IRF16_l,  f16,   i16,   "16",          bitconvert>;
//  def  LSU_ST_f16_as_v2i8    : T_LSU_ST_IRFconv<   IRF16_l,  f16,   v2i8,  "16",          bitconvert>;

//  def  LSU_ST_v2i8_as_i16    : T_LSU_ST_IRFconv<   IRF16_l,  v2i8,  i16,   "16",          bitconvert>;
//  def  LSU_ST_v2i8_as_f16    : T_LSU_ST_IRFconv<   IRF16_l,  v2i8,  f16,   "16",          bitconvert>;

// NPU2.6 Bfloat16 stores
def  LSU_ST64_f32_b16_Raw           : T_LSU_ST_VRF_Raw_Base<   VRF128,   "64.f32.b16", "">, Requires<[HasB16Float_Predicate]>;
def  LSU_ST16_f32_b16_Raw           : T_LSU_ST_IRF_Raw_Base<   IRF32,   "16.f32.b16", "">, Requires<[HasB16Float_Predicate]>;

//
//===----------------------------------------------------------------------===//
//
// 1.5.6.12 - LSU.STI
//    Register indirect store of specified type to memory with post-increment
//    of address by implicit value.
//
//    TODO
//
//===----------------------------------------------------------------------===//
//
// 1.5.6.13 - LSU.STA
//    Direct store of specified type to memory.
//
//    TODO
//
//===----------------------------------------------------------------------===//
//
// 1.5.6.14 - LSU.STINC
//    Register indirect store of specified type to memory with post-increment of
//    address by explicit mode and value.
//
//    TODO
//
//===----------------------------------------------------------------------===//
//
// 1.5.6.15 - LSU.STO
//    Displacement store of specified type to memory.
//
//    TODO
//
//===----------------------------------------------------------------------===//
//
// 1.5.6.16 - LSU.STX
//    Indexed store of specified type to memory.
//
//    TODO
//
//===----------------------------------------------------------------------===//
//
// 1.5.6.17 - LSU.STXV
//    Register indirect store of 128-bits to memory with optional post-increment
//    of address by explicit mode and value.
//
//    TODO
//
//===----------------------------------------------------------------------===//
//
// 1.5.6.18 - LSU.SWZ8
//    Apply 8 element vector swizzle to VAU, CMU, or SAU VRF input A.
//
class LSU_SWZ8<string opcode> :
      SHAVE_LSUInstr<(outs),
                     (ins i8imm:$sel0, i8imm:$sel1, i8imm:$sel2, i8imm:$sel3,
                          i8imm:$sel4, i8imm:$sel5, i8imm:$sel6, i8imm:$sel7),
                     !strconcat(opcode, " [$sel7$sel6$sel5$sel4$sel3$sel2$sel1$sel0]"),
                     []> {
  let hasSideEffects = 1;
  let Itinerary = II_LSU_SWZ8;
}

def LSU_SWZC8 : LSU_SWZ8<".SWZC8">;
def LSU_SWZS8 : LSU_SWZ8<".SWZS8">;
def LSU_SWZV8 : LSU_SWZ8<".SWZV8">;

//
//===----------------------------------------------------------------------===//
//
// 1.5.6.19 - LSU.SWZM4
//    Apply 4 element vector swizzle or byte-within-word swizzle to VAU, CMU, or
//    SAU VRF input A with optional zeroing, setting to one, or inversion (bitwise
//    of sign inversion).
//
def shave_swzm4_opt : Operand<i8> { let PrintMethod = "printSWZM4Operand"; }

class LSU_SWZM4<string opcode> :
      SHAVE_LSUInstr<(outs),
                     (ins shave_swzm4_opt:$mod0, shave_swzm4_opt:$mod1, shave_swzm4_opt:$mod2, shave_swzm4_opt:$mod3,
                          i8imm:$sel0, i8imm:$sel1, i8imm:$sel2, i8imm:$sel3),
                     !strconcat(opcode, " [$sel3$sel2$sel1$sel0],[$mod3$mod2$mod1$mod0]"),
                     []> {
  let hasSideEffects = 1;
  let Itinerary = II_LSU_SWZM4;
}

def LSU_SWZMC4_BYTE : LSU_SWZM4<".SWZMC4.BYTE">;
def LSU_SWZMS4_BYTE : LSU_SWZM4<".SWZMS4.BYTE">;
def LSU_SWZMV4_BYTE : LSU_SWZM4<".SWZMV4.BYTE">;

def LSU_SWZMC4_WORD : LSU_SWZM4<".SWZMC4.WORD">;
def LSU_SWZMS4_WORD : LSU_SWZM4<".SWZMS4.WORD">;
def LSU_SWZMV4_WORD : LSU_SWZM4<".SWZMV4.WORD">;

//
//===----------------------------------------------------------------------===//
//
// 1.5.6.20 - LSU.SWZM8
//    Apply 8 element vector swizzle to VAU, CMU, or  SAU VRF input A with optional
//    zeroing, setting to one, or inversion (bitwise or sign inversion).
//
def shave_swzm8_opt : Operand<i8> { let PrintMethod = "printSWZM8Operand"; }

class LSU_SWZM8<string opcode> :
      SHAVE_LSUInstr<(outs),
                     (ins shave_swzm8_opt:$mod0, shave_swzm8_opt:$mod1, shave_swzm8_opt:$mod2, shave_swzm8_opt:$mod3,
                          i8imm:$sel0, i8imm:$sel1, i8imm:$sel2, i8imm:$sel3),
                     !strconcat(opcode, " [$sel3$sel2$sel1$sel0],[$mod3$mod2$mod1$mod0]"),
                     []> {
  let hasSideEffects = 1;
  let Itinerary = II_LSU_SWZM8;
}

def LSU_SWZMC8_L : LSU_SWZM8<".SWZMC8.L">;
def LSU_SWZMS8_L : LSU_SWZM8<".SWZMS8.L">;
def LSU_SWZMV8_L : LSU_SWZM8<".SWZMV8.L">;

def LSU_SWZMC8_H : LSU_SWZM8<".SWZMC8.H">;
def LSU_SWZMS8_H : LSU_SWZM8<".SWZMS8.H">;
def LSU_SWZMV8_H : LSU_SWZM8<".SWZMV8.H">;

//
//===----------------------------------------------------------------------===//
//



//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Describe the LSU.LD, LSU.LDD, LSU.LDI, LSU.LDINC, LSU.LDO and LSU.LDX
// instructions using a high-level meta-pattern
//
// Unfortunately TableGen cannot handle this form of definition yet

/* ====
foreach lsuOP = [ "LD", "LDD", "LDI", "LDINC", "LDX" ] in {
  // Vector loads
  // ======
  // Unsigned 8-bit to 16-bit FP converting LD.128.u8.f16 loads
  defm LSU_#lsuOp#128_u8_f16      : T_LSU_#lsuOp#_VRF_Raw<   VRF128,                 "128.u8.f16">;

  def  LSU_#lsuOp#_v8u8_v8f16     : T_LSU_#lsuOp#_VRFconv<   VRF128,   v8i8,  v8f16, "128.u8.f16",  uint_to_fp>;

  // ======
  // U8F to 16-bit FP converting LD.128.u8f.f16 loads
  defm LSU_#lsuOp#128_u8f_f16     : T_LSU_#lsuOp#_VRF_Raw<   VRF128,                 "128.u8f.f16">;

  // ======
  // F16 to F32 extending 128-bit loads
  defm LSU_#lsuOp#128_f16_f32     : T_LSU_#lsuOp#_VRF_Raw<   VRF128,                 "128.f16.f32">;

  def  LSU_#lsuOp#_v4f16_v4f32    : T_LSU_#lsuOp#_VRF<       VRF128,          v4f32, "128.f16.f32", extload>;

  // ======
  // Integer extending loads
  defm LSU_#lsuOp#128_i8_i16      : T_LSU_#lsuOp#_VRF_Raw<   VRF128,                 "128.i8.i16">;
  defm LSU_#lsuOp#128_u8_u16      : T_LSU_#lsuOp#_VRF_Raw<   VRF128,                 "128.u8.u16">;
  defm LSU_#lsuOp#128_i16_i32     : T_LSU_#lsuOp#_VRF_Raw<   VRF128,                 "128.i16.i32">;
  defm LSU_#lsuOp#128_u16_u32     : T_LSU_#lsuOp#_VRF_Raw<   VRF128,                 "128.u16.u32">;

  def  LSU_#lsuOp#_v8a8_v8a16     : T_LSU_#lsuOp#_VRFconv<   VRF128,   v8i8,  v8i16, "128.i8.i16",  anyext>;
  def  LSU_#lsuOp#_v8s8_v8s16     : T_LSU_#lsuOp#_VRFconv<   VRF128,   v8i8,  v8i16, "128.i8.i16",  sext>;
  def  LSU_#lsuOp#_v8u8_v8u16     : T_LSU_#lsuOp#_VRFconv<   VRF128,   v8i8,  v8i16, "128.u8.u16",  zext>;
  def  LSU_#lsuOp#_v4a16_v4a32    : T_LSU_#lsuOp#_VRFconv<   VRF128,   v4i16, v4i32, "128.i16.i32", anyext>;
  def  LSU_#lsuOp#_v4s16_v4s32    : T_LSU_#lsuOp#_VRFconv<   VRF128,   v4i16, v4i32, "128.i16.i32", sext>;
  def  LSU_#lsuOp#_v4u16_v4u32    : T_LSU_#lsuOp#_VRFconv<   VRF128,   v4i16, v4i32, "128.u16.u32", zext>;

  // ======
  // Splat loads
  defm LSU_#lsuOp#32r             : T_LSU_#lsuOp#_VRF_Raw<   VRF128,                 "32r">;
  defm LSU_#lsuOp#16r             : T_LSU_#lsuOp#_VRF_Raw<   VRF128,                 "16r">;
  defm LSU_#lsuOp#8r              : T_LSU_#lsuOp#_VRF_Raw<   VRF128,                 "8r">;

  def  LSU_#lsuOp#_i32_v4i32      : T_LSU_#lsuOp#_VRFconv<   VRF128,   i32,   v4i32, "32r",         splat,       4>;
  def  LSU_#lsuOp#_i16_v8i16      : T_LSU_#lsuOp#_VRFconv<   VRF128,   i16,   v8i16, "16r",         splat,       2>;
  def  LSU_#lsuOp#_i8_v16i8       : T_LSU_#lsuOp#_VRFconv<   VRF128,   i8,    v16i8, "8r",          splat,       1>;

  def  LSU_#lsuOp#_f32_v4f32      : T_LSU_#lsuOp#_VRFconv<   VRF128,   f32,   v4f32, "32r",         splat,       4>;
  def  LSU_#lsuOp#_f16_v8f16      : T_LSU_#lsuOp#_VRFconv<   VRF128,   f16,   v8f16, "16r",         splat,       2>;

  //    These actually write 128-bits, but we aren't concerned about the upper bits
  def  LSU_#lsuOp#_i32_v2i32      : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  i32,   v2i32, "32r",         splat,       4>;
  def  LSU_#lsuOp#_i16_v4i16      : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  i16,   v4i16, "16r",         splat,       2>;
  def  LSU_#lsuOp#_i8_v8i8        : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  i8,    v8i8,  "8r",          splat,       1>;

  def  LSU_#lsuOp#_f32_v2f32      : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  f32,   v2f32, "32r",         splat,       4>;
  def  LSU_#lsuOp#_f16_v4f16      : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  f16,   v4f16, "16r",         splat,       2>;

  //    These actually write 128-bits, but we aren't concerned about the upper bits
  def  LSU_#lsuOp#_i16_v2i16_VRF  : T_LSU_#lsuOp#_VRFconv<   VRF32_q0, i16,   v2i16, "16r",         splat,       2>;
  def  LSU_#lsuOp#_i8_v4i8_VRF    : T_LSU_#lsuOp#_VRFconv<   VRF32_q0, i8,    v4i8,  "8r",          splat,       1>;

  def  LSU_#lsuOp#_f16_v2f16_VRF  : T_LSU_#lsuOp#_VRFconv<   VRF32_q0, f16,   v2f16, "16r",         splat,       2>;

  //    These actually write 128-bits, but we aren't concerned about the upper bits
  def  LSU_#lsuOp#_v2i16_VRF      : T_LSU_#lsuOp#_VRF<       VRF32_q0,        v2i16, "32r",         load,        4>;
  def  LSU_#lsuOp#_v4i8_VRF       : T_LSU_#lsuOp#_VRF<       VRF32_q0,        v4i8,  "32r",         load,        4>;

  def  LSU_#lsuOp#_v2f16_VRF      : T_LSU_#lsuOp#_VRF<       VRF32_q0,        v2f16, "32r",         load,        4>;


  // Scalar loads
  // ======
  // Converting loads
  //    8, 16 and 32-bit scalar
  def  LSU_#lsuOp#32_i16_i32      : T_LSU_#lsuOp#_IRF_Raw<   IRF32,                  "32.i16.i32">;
  def  LSU_#lsuOp#32_i8_i32       : T_LSU_#lsuOp#_IRF_Raw<   IRF32,                  "32.i8.i32">;

  def  LSU_#lsuOp#_s16_s32        : T_LSU_#lsuOp#_IRFconv<   IRF32,    i16,   i32,   "32.i16.i32",  sext,        2>;
  def  LSU_#lsuOp#_s8_s32         : T_LSU_#lsuOp#_IRFconv<   IRF32,    i8,    i32,   "32.i8.i32",   sext,        1>;
  def  LSU_#lsuOp#_s8_s16         : T_LSU_#lsuOp#_IRFconv<   IRF16_l,  i8,    i16,   "32.i8.i32",   sext,        1>;   // This overwrites the upper 16-bits, but we don't care about them

  def  LSU_#lsuOp#32_u16_u32      : T_LSU_#lsuOp#_IRF_Raw<   IRF32,                  "32.u16.u32">;
  def  LSU_#lsuOp#32_u8_u32       : T_LSU_#lsuOp#_IRF_Raw<   IRF32,                  "32.u8.u32">;

  def  LSU_#lsuOp#_u16_u32        : T_LSU_#lsuOp#_IRFconv<   IRF32,    i16,   i32,   "32.u16.u32",  zext,        2>;
  def  LSU_#lsuOp#_u8_u32         : T_LSU_#lsuOp#_IRFconv<   IRF32,    i8,    i32,   "32.u8.u32",   zext,        1>;
  def  LSU_#lsuOp#_u8_u16         : T_LSU_#lsuOp#_IRFconv<   IRF16_l,  i8,    i16,   "32.u8.u32",   zext,        1>;   // This overwrites the upper 16-bits, but we don't care about them
}

foreach lsuOP = [ "LD", "LDD", "LDI", "LDINC", "LDO", "LDX" ] in {
  // Vector loads
  // ======
  // 64-bit LD.64.l and LD.64.h loads
  defm LSU_#lsuOp#64_h            : T_LSU_#lsuOp#_VRF_Raw<  VRF64_h,                "64.h">;

  def  LSU_#lsuOp#_v2i32_h        : T_LSU_#lsuOp#_VRF<      VRF64_h,         v2i32, "64.h">;
  def  LSU_#lsuOp#_v4i16_h        : T_LSU_#lsuOp#_VRF<      VRF64_h,         v4i16, "64.h">;
  def  LSU_#lsuOp#_v8i8_h         : T_LSU_#lsuOp#_VRF<      VRF64_h,         v8i8,  "64.h">;

  def  LSU_#lsuOp#_v2f32_h        : T_LSU_#lsuOp#_VRF<      VRF64_h,         v2f32, "64.h">;
  def  LSU_#lsuOp#_v4f16_h        : T_LSU_#lsuOp#_VRF<      VRF64_h,         v4f16, "64.h">;

  defm LSU_#lsuOp#64_l            : T_LSU_#lsuOp#_VRF_Raw<  VRF64_l,                "64.l">;

  def  LSU_#lsuOp#_v2i32          : T_LSU_#lsuOp#_VRF<      VRF64_l,         v2i32, "64.l">;
  def  LSU_#lsuOp#_v4i16          : T_LSU_#lsuOp#_VRF<      VRF64_l,         v4i16, "64.l">;
  def  LSU_#lsuOp#_v8i8           : T_LSU_#lsuOp#_VRF<      VRF64_l,         v8i8,  "64.l">;

  def  LSU_#lsuOp#_v2f32          : T_LSU_#lsuOp#_VRF<      VRF64_l,         v2f32, "64.l">;
  def  LSU_#lsuOp#_v4f16          : T_LSU_#lsuOp#_VRF<      VRF64_l,         v4f16, "64.l">;

  // Scalar loads
  // ======
  // 64-bit scalar loads
  defm LSU_#lsuOp#64              : T_LSU_#lsuOp#_IRF64_Raw<IRF64,                  "64">;

  def  LSU_#lsuOp#_i64            : T_LSU_#lsuOp#_IRF64<    IRF64,           i64,   "64">;

  // ======
  // Non-converting loads
  //    8, 16 and 32-bit scalar
  defm LSU_#lsuOp#8               : T_LSU_#lsuOp#_IRF_Raw<  IRF8_q0,                "8">;
  defm LSU_#lsuOp#16              : T_LSU_#lsuOp#_IRF_Raw<  IRF16_l,                "16">;
  defm LSU_#lsuOp#32              : T_LSU_#lsuOp#_IRF_Raw<  IRF32,                  "32">;

  def  LSU_#lsuOp#_i8             : T_LSU_#lsuOp#_IRF<      IRF8_q0,         i8,    "8",           load,        1>;
  def  LSU_#lsuOp#_i16            : T_LSU_#lsuOp#_IRF<      IRF16_l,         i16,   "16",          load,        2>;
  def  LSU_#lsuOp#_i32            : T_LSU_#lsuOp#_IRF<      IRF32,           i32,   "32">;

  def  LSU_#lsuOp#_f16            : T_LSU_#lsuOp#_IRF<      IRF16_l,         f16,   "16",          load,        2>;
  def  LSU_#lsuOp#_f32            : T_LSU_#lsuOp#_IRF<      IRF32,           f32,   "32">;
  //    32-bit small vector
  def  LSU_#lsuOp#_v4i8           : T_LSU_#lsuOp#_IRF<      IRF32,           v4i8,  "32">;
  def  LSU_#lsuOp#_v2i16          : T_LSU_#lsuOp#_IRF<      IRF32,           v2i16, "32">;

  def  LSU_#lsuOp#_v2f16          : T_LSU_#lsuOp#_IRF<      IRF32,           v2f16, "32">;
  //    16-bit small vector
  def  LSU_#lsuOp#_v2i8           : T_LSU_#lsuOp#_IRF<      IRF16_l,         v2i8,  "16",          load,        2>;

  // Converting loads
  //    8, 16 and 32-bit scalar
  def  LSU_#lsuOp#_a16_a32        : T_LSU_#lsuOp#_IRFconv<   IRF32,    i16,   i32,   "16",          anyext,      2>;
  def  LSU_#lsuOp#_a8_a32         : T_LSU_#lsuOp#_IRFconv<   IRF32,    i8,    i32,   "8",           anyext,      1>;
  def  LSU_#lsuOp#_a8_a16         : T_LSU_#lsuOp#_IRFconv<   IRF16_l,  i8,    i16,   "8",           anyext,      1>;

  // Reinterpretting loads
  // ======
  //    64-bit reinterpret loads
  def  LSU_#lsuOp#_v2i32_as_v2f32 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v2i32, v2f32, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v2i32_as_v4i16 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v2i32, v4i16, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v2i32_as_v4f16 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v2i32, v4f16, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v2i32_as_v8i8  : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v2i32, v8i8,  "64.l",        bitconvert>;

  def  LSU_#lsuOp#_v2f32_as_v2i32 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v2f32, v2i32, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v2f32_as_v4i16 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v2f32, v4i16, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v2f32_as_v4f16 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v2f32, v4f16, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v2f32_as_v8i8  : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v2f32, v8i8,  "64.l",        bitconvert>;

  def  LSU_#lsuOp#_v4i16_as_v2i32 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v4i16, v2i32, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v4i16_as_v2f32 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v4i16, v2f32, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v4i16_as_v4f16 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v4i16, v4f16, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v4i16_as_v8i8  : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v4i16, v8i8,  "64.l",        bitconvert>;

  def  LSU_#lsuOp#_v4f16_as_v2i32 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v4f16, v2i32, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v4f16_as_v2f32 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v4f16, v2f32, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v4f16_as_v4i16 : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v4f16, v4i16, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v4f16_as_v8i8  : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v4f16, v8i8,  "64.l",        bitconvert>;

  def  LSU_#lsuOp#_v8i8_as_v2i32  : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v8i8,  v2i32, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v8i8_as_v2f32  : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v8i8,  v2f32, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v8i8_as_v4i16  : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v8i8,  v4i16, "64.l",        bitconvert>;
  def  LSU_#lsuOp#_v8i8_as_v4f16  : T_LSU_#lsuOp#_VRFconv<   VRF64_l,  v8i8,  v4f16, "64.l",        bitconvert>;

  // ======
  //    32-bit reinterpret loads
  def  LSU_#lsuOp#_i32_as_f32     : T_LSU_#lsuOp#_IRFconv<IRF32,       i32,   f32,    "32",          bitconvert>;
  def  LSU_#lsuOp#_i32_as_v2i16   : T_LSU_#lsuOp#_IRFconv<IRF32,       i32,   v2i16,  "32",          bitconvert>;
  def  LSU_#lsuOp#_i32_as_v2f16   : T_LSU_#lsuOp#_IRFconv<IRF32,       i32,   v2f16,  "32",          bitconvert>;
  def  LSU_#lsuOp#_i32_as_v4i8    : T_LSU_#lsuOp#_IRFconv<IRF32,       i32,   v4i8,   "32",          bitconvert>;

  def  LSU_#lsuOp#_f32_as_i32     : T_LSU_#lsuOp#_IRFconv<IRF32,       f32,   i32,    "32",          bitconvert>;
  def  LSU_#lsuOp#_f32_as_v2i16   : T_LSU_#lsuOp#_IRFconv<IRF32,       f32,   v2i16,  "32",          bitconvert>;
  def  LSU_#lsuOp#_f32_as_v2f16   : T_LSU_#lsuOp#_IRFconv<IRF32,       f32,   v2f16,  "32",          bitconvert>;
  def  LSU_#lsuOp#_f32_as_v4i8    : T_LSU_#lsuOp#_IRFconv<IRF32,       f32,   v4i8,   "32",          bitconvert>;

  def  LSU_#lsuOp#_v2i16_as_i32   : T_LSU_#lsuOp#_IRFconv<IRF32,       v2i16, i32,    "32",          bitconvert>;
  def  LSU_#lsuOp#_v2i16_as_f32   : T_LSU_#lsuOp#_IRFconv<IRF32,       v2i16, f32,    "32",          bitconvert>;
  def  LSU_#lsuOp#_v2i16_as_v2f16 : T_LSU_#lsuOp#_IRFconv<IRF32,       v2i16, v2f16,  "32",          bitconvert>;
  def  LSU_#lsuOp#_v2i16_as_v4i8  : T_LSU_#lsuOp#_IRFconv<IRF32,       v2i16, v4i8,   "32",          bitconvert>;

  def  LSU_#lsuOp#_v2f16_as_i32   : T_LSU_#lsuOp#_IRFconv<IRF32,       v2f16, i32,    "32",          bitconvert>;
  def  LSU_#lsuOp#_v2f16_as_f32   : T_LSU_#lsuOp#_IRFconv<IRF32,       v2f16, f32,    "32",          bitconvert>;
  def  LSU_#lsuOp#_v2f16_as_v2i16 : T_LSU_#lsuOp#_IRFconv<IRF32,       v2f16, v2i16,  "32",          bitconvert>;
  def  LSU_#lsuOp#_v2f16_as_v4i8  : T_LSU_#lsuOp#_IRFconv<IRF32,       v2f16, v4i8,   "32",          bitconvert>;

  def  LSU_#lsuOp#_v4i8_as_i32    : T_LSU_#lsuOp#_IRFconv<IRF32,       v4i8,  i32,    "32",          bitconvert>;
  def  LSU_#lsuOp#_v4i8_as_f32    : T_LSU_#lsuOp#_IRFconv<IRF32,       v4i8,  f32,    "32",          bitconvert>;
  def  LSU_#lsuOp#_v4i8_as_v2i16  : T_LSU_#lsuOp#_IRFconv<IRF32,       v4i8,  v2i16,  "32",          bitconvert>;
  def  LSU_#lsuOp#_v4i8_as_v2f16  : T_LSU_#lsuOp#_IRFconv<IRF32,       v4i8,  v2f16,  "32",          bitconvert>;

  // ======
  //    16-bit reinterpret loads
  def  LSU_#lsuOp#_i16_as_f16     : T_LSU_#lsuOp#_IRFconv<IRF16_l,     i16,   f16,    "16",          bitconvert, 2>;
  def  LSU_#lsuOp#_i16_as_v2i8    : T_LSU_#lsuOp#_IRFconv<IRF16_l,     i16,   v2i8,   "16",          bitconvert, 2>;

  def  LSU_#lsuOp#_f16_as_i16     : T_LSU_#lsuOp#_IRFconv<IRF16_l,     f16,   i16,    "16",          bitconvert, 2>;
  def  LSU_#lsuOp#_f16_as_v2i8    : T_LSU_#lsuOp#_IRFconv<IRF16_l,     f16,   v2i8,   "16",          bitconvert, 2>;

  def  LSU_#lsuOp#_v2i8_as_i16    : T_LSU_#lsuOp#_IRFconv<IRF16_l,     v2i8,  i16,    "16",          bitconvert, 2>;
  def  LSU_#lsuOp#_v2i8_as_f16    : T_LSU_#lsuOp#_IRFconv<IRF16_l,     v2i8,  f16,    "16",          bitconvert, 2>;
}
==== */

//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//


// Supplementary
// FIXME: Movidius - need a smarter implementation of a memory barrier/flush
def FLUSH_LSU0 : SHAVEInstr<(outs VRF128:$dst),
                           (ins VRF128:$src, MEMr:$addr, pred:$p, lsu_id:$funit),
                           "LSU0.LDO.64  $dst.1, $addr, 8  ${:comment} Flush Pending Stores on LSU0",
                           LSU0,
                           [usesLSU0]> {
  let Itinerary = II_LSU_LDO_VRF;
  let Constraints = "$dst = $src";  // This is a fake use, and is present to make TableGen think that the destination register is both read and written
//  let isBarrier = 1;
//  let hasSideEffects = 1;
  let mayLoad = 1;
  let mayStore = 1;
  let Pattern = [];
}
def FLUSH_LSU1 : SHAVEInstr<(outs VRF128:$dst),
                           (ins VRF128:$src, MEMr:$addr, pred:$p, lsu_id:$funit),
                           "LSU1.LD.64  $dst.0, $addr  ${:comment} Flush Pending Stores on LSU1",
                           LSU1,
                           [usesLSU1]> {
  let Itinerary = II_LSU_LD_VRF;
  let Constraints = "$dst = $src";  // This is a fake use, and is present to make TableGen think that the destination register is both read and written
//  let isBarrier = 1;
//  let hasSideEffects = 1;
  let mayLoad = 1;
  let mayStore = 1;
  let Pattern = [];
}


// FIXME: Movidius - is there not a better way of doing this that does not require this special
//        companion implementation?
let Itinerary = II_LSU_LD_VRF, mayLoad = 1 in {
  // These instructions [LSU_LDV128_l, LSU_LDV128_h] must be emited together
  // fragment to load the low part

  def LSU_LDV128_l : SHAVE_LSUInstr<(outs VRF128:$dst), (ins MEMr:$src),
                              ".LD.64 $dst.0 $src", [], [isLD128Low]>;

  def LSU_LDOV128_l : SHAVE_LSUInstr<(outs VRF128:$dst), (ins MEMri:$src),
                                  ".LDO.64 $dst.0 $src", [], [isLD128Low]>;

  // fragment to load the high part
  let Constraints = "$src1 = $dst", Itinerary = II_LSU_LD128H_VRF in {
    def LSU_LDV128_h : SHAVE_LSUInstr<(outs VRF128:$dst), (ins VRF128:$src1, MEMr:$src),
                                  ".LD.64 $dst.1 $src", [], [isLD128High]>;

    def LSU_LDOV128_h : SHAVE_LSUInstr<(outs VRF128:$dst), (ins VRF128:$src1, MEMri:$src),
                                    ".LDO.64 $dst.1 $src", [], [isLD128High]>;
  }
}

// FIXME - Movidius - The LSU.LDXV instructions have no scheduling information
def LSU_LDXV : SHAVE_LSUInstr_NoPredicate<(outs VRF128:$dst), (ins MEMr:$src),
                              ".LDXV $dst $src", [], [isLDXVorSTXV]>
{
  let mayLoad = 1;
}



let mayStore = 1 in {
// LSU.STO Store instructions
class LSU_STO<PatFrag storefrag, RegisterClass srcRC, ValueType srcVT, string asmstr, string lane = ""> :
    SHAVE_LSUInstr<(outs), (ins srcRC:$src1, MEMri:$src),
                    !strconcat(asmstr, " $src1", lane, " $src"),
                    [(storefrag (srcVT srcRC:$src1), ADDRri:$src)]>;

let Itinerary = II_LSU_ST_IRF in {
  def LSU_STO_i8    : LSU_STO<store, IRF8_q0, i8,    ".STO.8",  ".0">;
  def LSU_STO_i16   : LSU_STO<store, IRF16_l, i16,   ".STO.16", ".0">;
  def LSU_STO_i32   : LSU_STO<store, IRF32,   i32,   ".STO.32">;
  def LSU_STO_f16   : LSU_STO<store, IRF16_l, f16,   ".STO.16", ".0">;
  def LSU_STO_f32   : LSU_STO<store, IRF32,   f32,   ".STO.32">;
  def LSU_STO_v2f16 : LSU_STO<store, IRF32,   v2f16, ".STO.32">;
  def LSU_STO_v4i8  : LSU_STO<store, IRF32,   v4i8,  ".STO.32">;
  def LSU_STO_v2i16 : LSU_STO<store, IRF32,   v2i16, ".STO.32">;

  def LSU_STO_v2i8  : LSU_STO<store, IRF16_l, v2i8,  ".STO.16", ".0">;
}

let Itinerary = II_LSU_ST_IRF64, Predicates = [HasIRF64_LSU_CMU_Instrs_Predicate] in {
  def LSU_STO_i64   : LSU_STO<store, IRF64, i64, ".STO.64">;
}

// LSU.ST to VRF
let Itinerary = II_LSU_ST_VRF, mayStore = 1, Predicates = [HasVRF128_Predicate] in {
//  def LSU_STO_v4i16    : LSU_STO<store, VRF64_l, v4i16, "64">;
//  def LSU_STO_v4f16    : LSU_STO<store, VRF64_l, v4f16, "64">;
  def LSU_STO_v4i16    : LSU_STO<store, VRF64_l, v4i16, ".STO.64.L">;
  def LSU_STO_v4f16    : LSU_STO<store, VRF64_l, v4f16, ".STO.64.L">;

  def LSU_STOV64_l : SHAVE_LSUInstr<(outs), (ins VRF64_l:$src1, MEMri:$src),
              ".STO.64 $src1.0 $src", []>;

  def LSU_STOV64_h : SHAVE_LSUInstr<(outs), (ins VRF64_h:$src1, MEMri:$src),
              ".STO.64 $src1.1 $src", []>;

  let Itinerary = II_LSU_ST128L_VRF in {
    // fragment to store the low part
    def LSU_STV128_l : SHAVE_LSUInstr<(outs), (ins VRF128:$src1, MEMr:$src),
                                  ".ST.64 $src1.0 $src", [], [isST128Low]>;

    def LSU_STOV128_l : SHAVE_LSUInstr<(outs), (ins  VRF128:$src1, MEMri:$src),
                                   ".STO.64 $src1.0 $src", [], [isST128Low]>;
  }

  def LSU_STOV128_h : SHAVE_LSUInstr<(outs), (ins VRF128:$src1, MEMri:$src),
                                 ".STO.64 $src1.1 $src", [], [isST128High]>;

  def LSU_STXV : SHAVE_LSUInstr_NoPredicate<(outs), (ins VRF128:$src, MEMr:$addr),
                       ".STXV $src $addr", [], [isLDXVorSTXV]>;

  def LSU_STXV_masked_Myr2v2 : SHAVE_LSUInstr<(outs), (ins VRF128:$src, MEMr:$addr),
                                              ".STXV $src $addr", [], [isLDXVorSTXV]>;

  def LSU_STXV_masked_Myr2v3 : SHAVE_LSUInstr<(outs), (ins VRF128:$src, MEMr:$addr),
                                              ".ST.128 $src $addr", [], [isLDXVorSTXV]>;
}


let Itinerary = II_LSU_STID_VRF, Predicates = [HasVRF128_Predicate] in {
  def LSU_STI_v4f16         : LSU_STI_XX_Myriad<VRF64_l,    v4f16, "64",    (i32 8)>;
  def LSU_STI_v4i16         : LSU_STI_XX_Myriad<VRF64_l,    v4i16, "64",    (i32 8)>;
  def LSU_STI_v8i8          : LSU_STI_XX_Myriad<VRF64_l,     v8i8, "64",    (i32 8)>;
  def LSU_STI_v4i8          : LSU_STI_XX_Myriad<VRF32_q0,    v4i8, "32",    (i32 4)>;
}

let Itinerary = II_LSU_STID_IRF in {
  def LSU_STI_i32           : LSU_STI_32_Myriad<IRF32,      i32, "32",      (i32 4)>;
  def LSU_STI_i16           : LSU_STI_XX_Myriad<IRF16_l,    i16, "16",      (i32 2)>;
  def LSU_STI_i8            : LSU_STI_XX_Myriad<IRF8_q0,    i8,  "8",       (i32 1)>;

  def LSU_STI_f32           : LSU_STI_32_Class<IRF32,       f32, ".STI.32", (i32 4)>;
  def LSU_STI_f16           : LSU_STI_XX_Class<IRF16_l,     f16, ".STI.16", (i32 2)>;

  def LSU_STI_i32_trunc_i16 : LSU_STI_Trunc_Myriad<IRF32,   i32, itruncsti16, "16">;
  def LSU_STI_i32_trunc_i8  : LSU_STI_Trunc_Myriad<IRF32,   i32, itruncsti8,  "8">;
  def LSU_STI_i16_trunc_i8  : LSU_STI_Trunc_Myriad<IRF16_l, i16, itruncsti8,  "8">;
}
}

//  Myriad2.3 LSU.STI.xx Vsa.col Isb variants
class T_LSU_STI_VRFelem<ValueType srcTy, RegisterClass srcRC, string asmstr, ValueType elemTy>:
  SHAVE_LSUInstr<(outs  IRF32:$ptr), (ins srcRC:$src, i8imm:$pos, i32imm:$inc, MEMr:$dst),
                 !strconcat(".STI",asmstr," $src.$pos $dst"),
                 [(set IRF32:$ptr, (shave_istore (elemTy (vector_extract (srcTy srcRC:$src), imm:$pos)), ADDRr:$dst, imm:$inc))]>,
                   Requires<[HasVRF128_Predicate]> {
  let Itinerary = II_LSU_STID_VRF;
  let mayStore = 1;
}
def T_LSU_STI_32_VRFelem_v4f32: T_LSU_STI_VRFelem<v4f32,  VRF128, ".32", f32>;
def T_LSU_STI_32_VRFelem_v4i32: T_LSU_STI_VRFelem<v4i32,  VRF128, ".32", i32>;
def T_LSU_STI_32_VRFelem_v2f32: T_LSU_STI_VRFelem<v2f32, VRF64_l, ".32", f32>;
def T_LSU_STI_32_VRFelem_v2i32: T_LSU_STI_VRFelem<v2i32, VRF64_l, ".32", i32>;

def T_LSU_STI_32_VRFelem_v8f16: T_LSU_STI_VRFelem<v8f16,  VRF128, ".16", f16>;
def T_LSU_STI_32_VRFelem_v8i16: T_LSU_STI_VRFelem<v8i16,  VRF128, ".16", i16>;
def T_LSU_STI_32_VRFelem_v4f16: T_LSU_STI_VRFelem<v4f16, VRF64_l, ".16", f16>;
def T_LSU_STI_32_VRFelem_v4i16: T_LSU_STI_VRFelem<v4i16, VRF64_l, ".16", i16>;

def T_LSU_STI_32_VRFelem_v16i8: T_LSU_STI_VRFelem<v16i8,  VRF128,  ".8",  i8>;
def T_LSU_STI_32_VRFelem_v8i8 : T_LSU_STI_VRFelem<v8i8,  VRF64_l,  ".8",  i8>;


//   Extending LSU.LD loads from v2f16.
def : Pat<(v2f32 (extloadv2f16 ADDRr:$src)),
          (v2f32 (CMU_CPVV_v2f16_v2f32_conv (LSU_LD_v2f16_VRF ADDRr:$src)))>, Requires<[HasVRF128_Predicate]>;

//   Extending LSU.LDX loads from v2f16.
def : Pat<(v2f32 (extloadv2f16 ADDRrr:$src)),
          (v2f32 (CMU_CPVV_v2f16_v2f32_conv (LSU_LDX_v2f16_VRF ADDRrr:$src)))>, Requires<[EnableLDXSTX, HasVRF128_Predicate]>;

//   Extending LSU.LD loads from i8 to i16
def : Pat<(i16 (extloadi8 ADDRr:$src)),
          (i16 (LSU_LD_a8_a16 ADDRr:$src))>;

def : Pat<(i16 (sextloadi8 ADDRr:$src)),
          (i16 (LSU_LD_s8_s16 ADDRr:$src))>;

def : Pat<(i16 (zextloadi8 ADDRr:$src)),
          (i16 (LSU_LD_u8_u16 ADDRr:$src))>;

// Post-increment store for truncating v4i16 to v4i8
def : Pat<(i32 (shave_itruncstore_vector (v4i16 VRF64_l:$val), ADDRr:$dst, (i32 4))),
          (i32 (LSU_STI_v4i32_v4i8 (v4i32 (CMU_CPVV_v4i16_v4i32_conv VRF64_l:$val)), ADDRr:$dst))>, Requires<[HasVRF128_Predicate]>;

// Store for truncating v4i16 to v4i8
def : Pat<(shave_truncstore_vNi8 (v4i16 VRF64_l:$val), ADDRr:$dst),
          (LSU_ST_v4i32_v4i8 (v4i32 (CMU_CPVV_v4i16_v4i32_conv VRF64_l:$val)), ADDRr:$dst)>, Requires<[HasVRF128_Predicate]>;

include "SHAVEv3/SHAVEv3InstrInfo_LSU.td"
