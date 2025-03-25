//===-- SHAVEv3InstrInfo_CMU.td - CMU Instructions ---------*- tablegen -*-===//
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

let Predicates = [HasVRF512_Predicate] in {

//
// CMU.ALIGNVEC
//

class CMU_ALIGNVEC_Myr4<RegisterClass regClass, ValueType type> :
  SHAVE_CMUInstr<(outs regClass:$dst), (ins regClass:$src0, regClass:$src1, i8imm:$boff),
                 ".ALIGNVEC $dst $src0 $src1 $boff",
                 [(set (type regClass:$dst), (alignvec (type regClass:$src0), (type regClass:$src1), (i8 imm:$boff)))]>;

let Itinerary = II_CMU_ALIGN_VRF in {
  foreach type = [v16i32, v32i16, v64i8, v16f32, v32f16] in {
    def CMU_ALIGNVEC_#!cast<string>(type) : CMU_ALIGNVEC_Myr4<SHAVE_TypeInfo<type>.regClass, type>;
  }

  // Instruction definition for C builtin
  def CMU_ALIGNVEC_imm_VRF512 : SHAVE_CMUInstr<(outs WVRF512:$dst), (ins WVRF512:$src0, WVRF512:$src1, i8imm:$boff),
                                               ".ALIGNVEC $dst $src0 $src1 $boff", []>;
}

//
// CMU.ALIGNVEC for subvector extract with a constant offset
//

class CMU_ALIGNVEC_Extract_Myr4<ValueType inputType, ValueType outputType> :
  SHAVE_CMUInstr<(outs SHAVE_TypeInfo<outputType>.regClass:$dst),
                 (ins SHAVE_TypeInfo<inputType>.regClass:$src, i32imm:$offset),
                 // Offset for extraction is (scalarSizeInBytes * offset), get moviAsm to do the calculation for us
                 !strconcat(".ALIGNVEC $dst $src $src (", !cast<string>(!srl(SHAVE_TypeInfo<inputType>.scalarType.Size, 3)), " * $offset)"),
                 [(set (outputType SHAVE_TypeInfo<outputType>.regClass:$dst),
                       (shave_extract_subvector (inputType SHAVE_TypeInfo<inputType>.regClass:$src),
                                                (i32 imm:$offset))
                  )],
                 [isAlignvecExtract]>;

let Itinerary = II_CMU_ALIGN_VRF in {
  // Pairs are defined as [inputType, [outputTypes]]
  foreach pair = [ [[v16i32], [v8i32, v4i32, v2i32]], [[v32i16], [v16i16, v8i16, v4i16, v2i16]], [[v64i8], [v32i8, v16i8, v8i8, v4i8, v2i8]], // 512-bit integer vectors
                   [[v8i32],  [v4i32, v2i32]],        [[v16i16], [v8i16, v4i16, v2i16]],         [[v32i8], [v16i8, v8i8, v4i8, v2i8]],        // 256-bit integer vectors
                   [[v4i32],  [v2i32]],               [[v8i16],  [v4i16, v2i16]],                [[v16i8], [v8i8, v4i8, v2i8]],               // 128-bit integer vectors
                                                      [[v4i16],  [v2i16]],                       [[v8i8],  [v4i8, v2i8]],                     // 64-bit integer vectors
                                                                                                 [[v4i8],  [v2i8]],                           // 32-bit integer vectors
                   [[v16f32], [v8f32, v4f32, v2f32]], [[v32f16], [v16f16, v8f16, v4f16, v2f16]],                                              // 512-bit floating-point vectors
                   [[v8f32],  [v4f32, v2f32]],        [[v16f16], [v8f16, v4f16, v2f16]],                                                      // 256-bit floating-point vectors
                   [[v4f32],  [v2f32]],               [[v8f16],  [v4f16, v2f16]],                                                             // 128-bit floating-point vectors
                                                      [[v4f16],  [v2f16]]                                                                     // 64-bit floating-point vectors
                 ] in {
    foreach outputType = pair[1] in {
      def CMU_ALIGNVEC_extract_#!cast<string>(pair[0][0])#_#!cast<string>(outputType) : CMU_ALIGNVEC_Extract_Myr4<pair[0][0], outputType>;
    }
  }
}

//
// CMU.CLAMPAB
//

let Itinerary = II_CMU_CLAMPAB_VRF in {
  // Integer variants
  foreach type = [v16i32, v32i16, v64i8, // 512-bit vectors
                  v8i32,  v16i16, v32i8, // 256-bit vectors
                  v4i32,  v8i16,  v16i8, // 128-bit vectors
                  v2i32,  v4i16,  v8i8   // 64-bit vectors
                 ] in {
    // Construct the machine instruction names, e.g. "CMU_CLAMPAB_i32_v16i32_VRF512" for v16i32 and "CMU_CLAMPAB_u32_v16u32_VRF512" for v16u32
    def CMU_CLAMPAB_#!cast<string>(SHAVE_TypeInfo<type>.scalarType)#_#!cast<string>(type)#_VRF512 :
      CMU_CLAMPAB<SHAVE_TypeInfo<type>.regClass, type, 0, !strconcat(".", SHAVE_TypeInfo<type, 0>.asmstr)>;
    def CMU_CLAMPAB_#!subst("i", "u", !cast<string>(SHAVE_TypeInfo<type>.scalarType))#_#!subst("i", "u", !cast<string>(type))#_VRF512 :
      CMU_CLAMPAB<SHAVE_TypeInfo<type>.regClass, type, 1, !strconcat(".", SHAVE_TypeInfo<type, 1>.asmstr)>;
  }

  // Floating-point variants
  foreach type = [v16f32, v32f16, // 512-bit vectors
                  v8f32,  v16f16, // 256-bit vectors
                  v4f32,  v8f16,  // 128-bit vectors
                  v2f32,  v4f16   // 64-bit vectors
                 ] in {
    // Construct the machine instruction names, e.g. "CMU_CLAMPAB_f32_v16f32_VRF512" for v16f32
    def CMU_CLAMPAB_#!cast<string>(SHAVE_TypeInfo<type>.scalarType)#_#!cast<string>(type)#_VRF512 :
      CMU_CLAMPAB<SHAVE_TypeInfo<type>.regClass, type, 0, !strconcat(".", SHAVE_TypeInfo<type, 0>.asmstr)>;
  }
}

//
// CMU.CLAMP
//

let Itinerary = II_CMU_CLAMP_VRF in {
  // Instruction definitions for C builtins
  foreach type = [v16i32, v32i16, v64i8, v16f32, v32f16] in {
    // Construct the machine instruction names, e.g. "CMU_CLAMP0_i32_v16i32_VRF512" for v16i32
    def CMU_CLAMP0_#!cast<string>(SHAVE_TypeInfo<type>.scalarType)#_#!cast<string>(type)#_VRF512 :
      CMU_CLAMP_Raw<".CLAMP0", WVRF512, !strconcat(".", SHAVE_TypeInfo<type>.asmstr)>;

    def CMU_CLAMP_#!cast<string>(SHAVE_TypeInfo<type>.scalarType)#_#!cast<string>(type)#_VRF512 :
      CMU_CLAMP_Raw<".CLAMP", WVRF512, !strconcat(".", SHAVE_TypeInfo<type>.asmstr)>;
  }
}

//
// CMU.CPZ
//   These instructions have been intentionally removed as they are not supported on NPU6
//   Do not add support for them, use a "VAU.XOR dreg dreg dreg" instead
//

//
// CMU.MIN/MAX
//

multiclass CMU_MinMax_Base_Myr4<RegisterClass RC, string asmstr, ValueType ty, SDNode op, int isUint = 0> {
  def _VRF512 : CMU_MinMax_Base<RC, asmstr, ty, op, isUint, [CC_CMU0, CC_CMU1,  CC_CMU2,  CC_CMU3,  CC_CMU4,  CC_CMU5,  CC_CMU6,  CC_CMU7,
                                                             CC_CMU8, CC_CMU9, CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15]>;
}

multiclass CMU_MinMax_Myr4<string asmstr, SDNode op> {
  // Integer variants
  foreach type = [v16i32, v32i16, v64i8, // 512-bit vectors
                  v8i32,  v16i16, v32i8, // 256-bit vectors
                  v4i32,  v8i16,  v16i8, // 128-bit vectors
                  v2i32,  v4i16,  v8i8   // 64-bit vectors
                 ] in {
    // Construct the machine instruction name suffixes, e.g. "_i32_v16i32" for v16i32 and "_u32_v16u32" for v16u32
    defm _#!cast<string>(SHAVE_TypeInfo<type>.scalarType)#_#!cast<string>(type) :
      CMU_MinMax_Base_Myr4<SHAVE_TypeInfo<type, 0>.regClass, !strconcat(asmstr, SHAVE_TypeInfo<type, 0>.asmstr), type, op, 0>;
    defm _#!subst("i", "u", !cast<string>(SHAVE_TypeInfo<type>.scalarType))#_#!subst("i", "u", !cast<string>(type)) :
      CMU_MinMax_Base_Myr4<SHAVE_TypeInfo<type, 1>.regClass, !strconcat(asmstr, SHAVE_TypeInfo<type, 1>.asmstr), type, op, 1>;
  }

  // Floating-point variants
  foreach type = [v16f32, v32f16, // 512-bit vectors
                  v8f32,  v16f16, // 256-bit vectors
                  v4f32,  v8f16,  // 128-bit vectors
                  v2f32,  v4f16   // 64-bit vectors
                 ] in {
    // Construct the machine instruction name suffixes, e.g. "_f32_v16f32_Myr4" for v16f32
    defm _#!cast<string>(SHAVE_TypeInfo<type>.scalarType)#_#!cast<string>(type) :
      CMU_MinMax_Base_Myr4<SHAVE_TypeInfo<type>.regClass, !strconcat(asmstr, SHAVE_TypeInfo<type>.asmstr), type, op>;
  }
}

let Itinerary = II_CMU_RRR_VRF_MIN_MAX_Myr2v3 in {
  defm CMU_MAX : CMU_MinMax_Myr4<".MAX.", shave_max>;
  defm CMU_MIN : CMU_MinMax_Myr4<".MIN.", shave_min>;
}

//
// CMU.CMVV
//

// FIXME: Movidius_v3 - These CC flags need to be updated

class CMU_CMVV_WVRFx<RegisterClass rc, ValueType vt, string asmstr> : 
  CMU_CompareC<rc, vt, !strconcat(".CMVV.", asmstr),
               [CC_CMU0,  CC_CMU1,  CC_CMU2,  CC_CMU3,  CC_CMU4,  CC_CMU5,  CC_CMU6,  CC_CMU7,
                CC_CMU8,  CC_CMU9,  CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15]>;

let Itinerary = II_CMU_CMVV_COMPARISON_WITH_WRITE_16_CONDITION in {
  // Integer variants
  foreach type = [v16i32, v32i16, v64i8, v8i32, v16i16, v32i8, v4i32, v8i16, v16i8, v2i32, v4i16, v8i8] in {
    // Construct the machine instruction names, e.g. "CMU_CMVV_v16i32_signed_Myr4" for v16i32 and "CMU_CMVV_v16i32_unsigned_Myr4" for v16u32
    def CMU_CMVV_#!cast<string>(type)#_signed_Myr4   : CMU_CMVV_WVRFx<SHAVE_TypeInfo<type, 0>.regClass, type, SHAVE_TypeInfo<type, 0>.asmstr>;
    def CMU_CMVV_#!cast<string>(type)#_unsigned_Myr4 : CMU_CMVV_WVRFx<SHAVE_TypeInfo<type, 1>.regClass, type, SHAVE_TypeInfo<type, 1>.asmstr>;
  }

  // Floating point variants
  foreach type = [v16f32, v32f16, v8f32, v16f16, v4f32, v8f16, v2f32, v4f16] in {
    // Construct the machine instruction names, e.g. "CMU_CMVV_v16f32_Myr4" for v16f32
    def CMU_CMVV_#!cast<string>(type)#_Myr4 : CMU_CMVV_WVRFx<SHAVE_TypeInfo<type>.regClass, type, SHAVE_TypeInfo<type>.asmstr>;
  }
}

//
// CMU.CMZ
//

let Itinerary = II_CMU_CMZ_COMPARISON_VRF_WITH_WRITE_16_CONDITION in
{
  defvar defs = [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7,
                 CC_CMU8,  CC_CMU9,  CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15];

  def CMU_CMZ_v16i32  : CMU_CompareZeroC_Raw<WVRF512, ".CMZ.i32", defs>;
  def CMU_CMZ_v16f32  : CMU_CompareZeroC_Raw<WVRF512, ".CMZ.f32", defs>;
  def CMU_CMZ_v32i16  : CMU_CompareZeroC_Raw<WVRF512, ".CMZ.i16", defs>;
  def CMU_CMZ_v32f16  : CMU_CompareZeroC_Raw<WVRF512, ".CMZ.f16", defs>;
  def CMU_CMZ_v64i8   : CMU_CompareZeroC_Raw<WVRF512, ".CMZ.i8", defs>;
}

//
// CMU.COMPRESS
//

class CMU_COMPRESS<RegisterClass destinationRC, RegisterClass sourceRC, RegisterClass maskRC, string asmstr> :
  SHAVE_CMUInstr<(outs destinationRC:$dst), (ins sourceRC:$src, maskRC:$mask),
                 !strconcat(".COMPRESS.", asmstr, " $dst $src $mask"),
                 []>;

foreach destinationRC = [ WVRF512, WVRF256_0, WVRF128_0, WVRF64_0, WVRF32_0, WVRF16_0 ] in {
  foreach sourceRC = [ WVRF512, WVRF256_0, WVRF128_0, WVRF64_0, WVRF32_0, WVRF16_0 ] in {
    let Itinerary = II_CMU_COMPRESS in {
      def CMU_COMPRESS_x32_#destinationRC#_#sourceRC : CMU_COMPRESS<destinationRC, sourceRC, IRF32, "x32">;
      def CMU_COMPRESS_x16_#destinationRC#_#sourceRC : CMU_COMPRESS<destinationRC, sourceRC, IRF32, "x16">;
    }
    let Itinerary = II_CMU_COMPRESS_X8 in {
      def CMU_COMPRESS_x8_#destinationRC#_#sourceRC  : CMU_COMPRESS<destinationRC, sourceRC, IRF64, "x8">;
    }
  }
}

//
// CMU.CPR
//

class CMU_CPR_LaneSplat<ValueType dstType, ValueType srcType> :
  CMU_CP<(outs SHAVE_TypeInfo<dstType>.regClass:$dst),
         (ins SHAVE_TypeInfo<srcType>.regClass:$src, i32imm:$lane),
         !strconcat(".CP.512.",                                             // Assembly string
                    !cast<string>(SHAVE_TypeInfo<dstType>.scalarType.Size), //   e.g. ".CP.512.32.R" for v16i32
                    ".R $dst $src.$lane"),                                  // We always use 512, regardless of the subreg class we are using
         [(set (dstType SHAVE_TypeInfo<dstType>.regClass:$dst),
               (dstType (lanesplat (srcType SHAVE_TypeInfo<srcType>.regClass:$src),
                                   (i32 simm16:$lane)
                        )
               )
          )
         ]>;

let Itinerary = II_CMU_CPIV_VRF_IRF in {
  foreach type = [v16i32, v32i16, v64i8, v16f32, v32f16, // 512-bit vectors
                  v8i32,  v16i16, v32i8, v8f32,  v16f16, // 256-bit vectors
                  v4i32,  v8i16,  v16i8, v4f32,  v8f16,  // 128-bit vectors
                  v2i32,  v4i16,  v8i8,  v2f32,  v4f16,  // 64-bit vectors
                          v2i16,  v4i8,          v2f16,  // 32-bit vectors
                                  v2i8                   // 16-bit vectors
                 ] in {
    def CMU_CPR_#!cast<string>(type)#_splat :
      CMU_CPR<SHAVE_TypeInfo<type>.regClass, SHAVE_TypeInfo<type>.scalarRegClass,    // Register classes
              type, SHAVE_TypeInfo<type>.scalarType,                                 // Types
              !strconcat(".CP.512.",                                                 // Assembly string,
                         !cast<string>(SHAVE_TypeInfo<type>.scalarType.Size), ".R"), //   e.g. ".CP.512.32.R" for v16i32
              !if(!eq(SHAVE_TypeInfo<type>.scalarType.Size, 32), "", ".0")>;         // Lane string
  }

  // For each set of scalar types (i32, 16, i8, f32, f16), define lane splats from/to each corresponding vector type
  foreach types = [[ v16i32, v8i32, v4i32, v2i32 ],
                   [ v32i16, v16i16, v8i16, v4i16, v2i16 ],
                   [ v64i8, v32i8, v16i8, v8i8, v4i8, v2i8 ],
                   [ v16f32, v8f32, v4f32, v2f32 ],
                   [ v32f16, v16f16, v8f16, v4f16, v2f16 ],
                  ] in {
    foreach dstType = types in {
      foreach srcType = types in {
        def CMU_CPR_#!cast<string>(dstType)#_lanesplat_from_#!cast<string>(srcType) : CMU_CPR_LaneSplat<dstType, srcType>;
      }
    }
  }
}

let Itinerary = II_CMU_CPII_IRF in {
  def CMU_CPR_v4i8_IRF  : CMU_CPR<IRF32,   IRF8_q0, v4i8,  i8,  ".CP.32.8.R",  ".0">;
  def CMU_CPR_v2i16_IRF : CMU_CPR<IRF32,   IRF16_l, v2i16, i16, ".CP.32.16.R", ".0">;
  def CMU_CPR_v2f16_IRF : CMU_CPR<IRF32,   IRF16_l, v2f16, f16, ".CP.32.16.R", ".0">;
  def CMU_CPR_v2i8_IRF  : CMU_CPR<IRF16_l, IRF8_q0, v2i8,  i8,  ".CP.32.8.R",  ".0">;
}

//
// CMU.CPII
//

let Itinerary = II_CMU_CPII_IRF_FLOAT_INT in {
  def CMU_CPII_u8_f32_Myr4 :  CMU_CP<(outs IRF32:$dst),   (ins IRF8_q0:$src), ".CP.32.u8.f32 $dst $src.0",  [(set (f32 IRF32:$dst), (uint_to_fp (i8 IRF8_q0:$src)))]>;
  def CMU_CPII_u8_f16_Myr4 :  CMU_CP<(outs IRF16_l:$dst), (ins IRF8_q0:$src), ".CP.32.u8.f16 $dst $src.0",  [(set (f16 IRF16_l:$dst), (uint_to_fp (i8 IRF8_q0:$src)))]>;
}

//
// CMU.CPII for small vector element extract/insert
//

class CMU_CPII_vector_extract_Myr4<RegisterClass dstRC, RegisterClass srcRC, ValueType dstVT, ValueType srcVT, string sizeStr> : 
    CMU_CP<(outs dstRC:$dst), (ins srcRC:$src, i8imm:$srcPosition), !strconcat(".CP.", sizeStr, " $dst.0 $src.$srcPosition"),
           [(set (dstVT dstRC:$dst), (vector_extract (srcVT srcRC:$src), imm:$srcPosition))]>;

let Itinerary = II_CMU_CPII_IRF in {
  def CMU_CPII_v2i16_extract_i16_Myr4 : CMU_CPII_vector_extract_Myr4<IRF16_l, IRF32,   i16, v2i16, "16">;
  def CMU_CPII_v2f16_extract_f16_Myr4 : CMU_CPII_vector_extract_Myr4<IRF16_l, IRF32,   f16, v2f16, "16">;

  def CMU_CPII_v4i8_extract_i8_Myr4   : CMU_CPII_vector_extract_Myr4<IRF8_q0, IRF32,   i8,  v4i8,  "8">;
  def CMU_CPII_v2i8_extract_i8_Myr4   : CMU_CPII_vector_extract_Myr4<IRF8_q0, IRF16_l, i8,  v2i8,  "8">;
}

class CMU_CPII_vector_insert_Myr4<RegisterClass vectorRC, RegisterClass scalarRC, ValueType vectorVT, ValueType scalarVT, string sizeStr> : 
    CMU_CP<(outs vectorRC:$dst), (ins vectorRC:$src, scalarRC:$value, i8imm:$dstPosition), !strconcat(".CP.", sizeStr, " $dst.$dstPosition $value.0"),
           [(set (vectorVT vectorRC:$dst), (vector_insert (vectorVT vectorRC:$src), (scalarVT scalarRC:$value), imm:$dstPosition))]> {
  let Constraints = "$src = $dst";
}

let Itinerary = II_CMU_CPII_IRF in {
  def CMU_CPII_v2i16_insert_i16_Myr4 : CMU_CPII_vector_insert_Myr4<IRF32,   IRF16_l, v2i16, i16, "16">;
  def CMU_CPII_v2f16_insert_f16_Myr4 : CMU_CPII_vector_insert_Myr4<IRF32,   IRF16_l, v2f16, f16, "16">;

  def CMU_CPII_v4i8_insert_i8_Myr4   : CMU_CPII_vector_insert_Myr4<IRF32,   IRF8_q0, v4i8,  i8,  "8">;
  def CMU_CPII_v2i8_insert_i8_Myr4   : CMU_CPII_vector_insert_Myr4<IRF16_l, IRF8_q0, v2i8,  i8,  "8">;
}

//
// CMU.CPIV
//

// IRFXX -> WVRFXX_0 plain copy
let Itinerary = II_CMU_CPIV_VRF_IRF64, isAsCheapAsAMove = 1 in {
  def CMU_CPIV_64_Myr4 : CMU_CP<(outs WVRF64_0:$dst), (ins IRF64:$src), ".CP.64 $dst.0 $src">;
}
let Itinerary = II_CMU_CPIV_VRF_IRF, isAsCheapAsAMove = 1 in {
  def CMU_CPIV_32_Myr4 : CMU_CP<(outs WVRF32_0:$dst), (ins IRF32:$src), ".CP.32 $dst.0 $src">;
  def CMU_CPIV_16_Myr4 : CMU_CP<(outs WVRF16_0:$dst), (ins IRF16_l:$src), ".CP.16 $dst.0 $src.0">;
}

//
// CMU.CPVI
//

let isAsCheapAsAMove = 1 in {
  // WVRFXX_0 -> IRFXX plain copy
  let Itinerary = II_CMU_CPVI_IRF64_VRF in {
    def CMU_CPVI_64_Myr4 : CMU_CP<(outs IRF64:$dst), (ins WVRF64_0:$src), ".CP.64 $dst $src.0">;
  }
  let Itinerary = II_CMU_CPVI_IRF_VRF in {
    def CMU_CPVI_32_Myr4 : CMU_CP<(outs IRF32:$dst), (ins WVRF32_0:$src), ".CP.32 $dst $src.0">;
    def CMU_CPVI_16_Myr4 : CMU_CP<(outs IRF16_l:$dst), (ins WVRF16_0:$src), ".CP.16 $dst.0 $src.0">;
  }
}

//
// CMU.CPVV
//

// Element-wise copies
class CMU_CPVV_element<RegisterClass RC, ValueType vectorType, ValueType scalarType, string asmstr> :
   SHAVE_CMUInstr<(outs RC:$dst), (ins RC:$vsrc, RC:$src, i8imm:$dstElement, i8imm:$srcElement),
                  !strconcat(asmstr," $dst.$dstElement $src.$srcElement"),
                  [(set (vectorType RC:$dst), (vector_insert (vectorType RC:$vsrc),
                                                             (scalarType (vector_extract (vectorType RC:$src), imm:$srcElement)),
                                                             imm:$dstElement))],
                  [!if(!eq(scalarType.Size, 32), isElementCopy32,
                   !if(!eq(scalarType.Size, 16), isElementCopy16,
                                                 isElementCopy8))]> {
  let Constraints = "$vsrc = $dst";
}

// Subvector-insert copies
class CMU_CPVV_subvector_Myr4<RegisterClass outputRC, RegisterClass inputRC, string asmstr> :
   SHAVE_CMUInstr<(outs outputRC:$dst),
                  (ins outputRC:$vsrc, inputRC:$src, i32imm:$idx, i32imm:$copySize),
                  // Let moviAsm to compute adjusted index
                  !strconcat(".CP.", asmstr, " $dst.($idx/$copySize) $src.0"),
                  []> {
  let Constraints = "$vsrc = $dst";
}

// Direct subvector to subvector copies
class CMU_CPVV_subvector_copy_Myr4<RegisterClass RC, string asmstr> :
   SHAVE_CMUInstr<(outs RC:$dst),
                  (ins RC:$vsrc, RC:$src, i32imm:$dstIdx, i32imm:$srcIdx, i32imm:$copySize),
                  // Let moviAsm to compute adjusted index
                  !strconcat(".CP.", asmstr, " $dst.($dstIdx/$copySize) $src.($srcIdx/$copySize)"),
                  []> {
  let Constraints = "$vsrc = $dst";
}


let Itinerary = II_CMU_CPVV_VRF, isAsCheapAsAMove = 1 in {
  // Full register to register copies
  def CMU_CPVV_512_Myr4 : CMU_CP<(outs WVRF512:$dst),   (ins WVRF512:$src),   ".CP.512 $dst $src",     []>;
  def CMU_CPVV_256_Myr4 : CMU_CP<(outs WVRF256_0:$dst), (ins WVRF256_0:$src), ".CP.256 $dst.0 $src.0", []>;
  def CMU_CPVV_128_Myr4 : CMU_CP<(outs WVRF128_0:$dst), (ins WVRF128_0:$src), ".CP.128 $dst.0 $src.0", []>;
  def CMU_CPVV_64_Myr4  : CMU_CP<(outs WVRF64_0:$dst),  (ins WVRF64_0:$src),  ".CP.64 $dst.0 $src.0",  []>;
  def CMU_CPVV_32_Myr4  : CMU_CP<(outs WVRF32_0:$dst),  (ins WVRF32_0:$src),  ".CP.32 $dst.0 $src.0",  []>;
  def CMU_CPVV_16_Myr4  : CMU_CP<(outs WVRF16_0:$dst),  (ins WVRF16_0:$src),  ".CP.16 $dst.0 $src.0",  []>;

  // Raw 256-bit extract from 512-bit vector
  def CMU_CPVV_256_extract_512_Myr4 : CMU_CP<(outs WVRF512:$dst),
                                             (ins WVRF512:$src, i32imm:$didx, i32imm:$sidx),
                                             ".CP.256 $dst.$didx $src.$sidx",
                                             []>;

  // Subvector insert copies
  foreach pair = [ [[v64i8], [v32i8, v16i8, v8i8, v4i8, v2i8]],
                   [[v32i8],        [v16i8, v8i8, v4i8, v2i8]],
                   [[v16i8],               [v8i8, v4i8, v2i8]],
                   [[v8i8],                      [v4i8, v2i8]],
                   [[v4i8],                            [v2i8]]
                 ] in {
    foreach inputType = pair[1] in {
      // Subvector inserts
      def CMU_CPVV_subvector_insert_#SHAVE_TypeInfo<pair[0][0]>.regClass#_#SHAVE_TypeInfo<inputType>.regClass :
        CMU_CPVV_subvector_Myr4<SHAVE_TypeInfo<pair[0][0]>.regClass, SHAVE_TypeInfo<inputType>.regClass, !cast<string>(inputType.Size)>;

      // Direct subvector copies
      def CMU_CPVV_subvector_copy_#SHAVE_TypeInfo<pair[0][0]>.regClass#_#inputType.Size :
        CMU_CPVV_subvector_copy_Myr4<SHAVE_TypeInfo<pair[0][0]>.regClass, !cast<string>(inputType.Size)>;
    }
  }
}

foreach pair = [ [[v16i32], [v8i32, v4i32, v2i32]], [[v32i16], [v16i16, v8i16, v4i16, v2i16]], [[v64i8], [v32i8, v16i8, v8i8, v4i8, v2i8]], // 512-bit integer vectors
                 [[v8i32],  [v4i32, v2i32]],        [[v16i16], [v8i16, v4i16, v2i16]],         [[v32i8], [v16i8, v8i8, v4i8, v2i8]],        // 256-bit integer vectors
                 [[v4i32],  [v2i32]],               [[v8i16],  [v4i16, v2i16]],                [[v16i8], [v8i8, v4i8, v2i8]],               // 128-bit integer vectors
                                                    [[v4i16],  [v2i16]],                       [[v8i8],  [v4i8, v2i8]],                     // 64-bit integer vectors
                                                                                               [[v4i8],  [v2i8]],                           // 32-bit integer vectors
                 [[v16f32], [v8f32, v4f32, v2f32]], [[v32f16], [v16f16, v8f16, v4f16, v2f16]],                                              // 512-bit floating-point vectors
                 [[v8f32],  [v4f32, v2f32]],        [[v16f16], [v8f16, v4f16, v2f16]],                                                      // 256-bit floating-point vectors
                 [[v4f32],  [v2f32]],               [[v8f16],  [v4f16, v2f16]],                                                             // 128-bit floating-point vectors
                                                    [[v4f16],  [v2f16]]                                                                     // 64-bit floating-point vectors
               ] in {
    foreach inputType = pair[1] in {
        // Subvector insert patterns
        def : Pat<(pair[0][0] (insert_subvector (pair[0][0] SHAVE_TypeInfo<pair[0][0]>.regClass:$vsrc), (inputType SHAVE_TypeInfo<inputType>.regClass:$src), (i32 imm:$idx))),
            (!cast<Instruction>("CMU_CPVV_subvector_insert_"#SHAVE_TypeInfo<pair[0][0]>.regClass#_#SHAVE_TypeInfo<inputType>.regClass)
              SHAVE_TypeInfo<pair[0][0]>.regClass:$vsrc, SHAVE_TypeInfo<inputType>.regClass:$src, imm:$idx, SHAVE_TypeInfo<inputType>.vectorElementCount)>;

        // Subvector insert/extract patterns
        def : Pat<(pair[0][0] (insert_subvector
                                        (pair[0][0] SHAVE_TypeInfo<pair[0][0]>.regClass:$vsrc),
                                        (inputType (shave_extract_subvector
                                                        (pair[0][0] SHAVE_TypeInfo<pair[0][0]>.regClass:$src),
                                                        (i32 imm:$srcElement))),
                                        (i32 imm:$dstElement))),
            (!cast<Instruction>("CMU_CPVV_subvector_copy_"#SHAVE_TypeInfo<pair[0][0]>.regClass#"_"#inputType.Size)
              SHAVE_TypeInfo<pair[0][0]>.regClass:$vsrc, SHAVE_TypeInfo<pair[0][0]>.regClass:$src,
              imm:$dstElement, imm:$srcElement, SHAVE_TypeInfo<inputType>.vectorElementCount)>;
    }
}

//
// CMU.ITOF/UTOF/FTOI/FTOU VRF->VRF
//

class CMU_ConvertingCP_Base<RegisterClass srcRC, ValueType srcType,
                            RegisterClass dstRC, ValueType dstType,
                            SDNode op, string asmstr, string srcLane = "", string dstLane = ""> :
  SHAVE_CMUInstr<(outs dstRC:$dst), (ins srcRC:$src),
                 !strconcat(asmstr, " $dst", dstLane, " $src", srcLane),
                 [(set (dstType dstRC:$dst), (op (srcType srcRC:$src)))]>;

class CMU_ConvertingCP<ValueType srcType, ValueType dstType,
                       SDNode op, bit srcUnsigned, bit dstUnsigned, string saturating = ""> :
  CMU_ConvertingCP_Base<SHAVE_TypeInfo<srcType, srcUnsigned>.regClass, srcType, SHAVE_TypeInfo<dstType, srcUnsigned>.regClass, dstType, op, 
                        !strconcat(".CP.", !cast<string>(dstType.Size),
                                   ".", SHAVE_TypeInfo<srcType, srcUnsigned>.asmstr, ".", SHAVE_TypeInfo<dstType, dstUnsigned>.asmstr,
                                   saturating),
                        !if(!lt(srcType.Size, 512), ".0", ""), 
                        !if(!lt(dstType.Size, 512), ".0", "")>;

multiclass CMU_ConvertingCP_int_fp<ValueType typeA, ValueType typeB> {
  def _#!cast<string>(typeB.Size)#_#!cast<string>(typeA)#_#!cast<string>(typeB)#_signed   : CMU_ConvertingCP<typeA, typeB, sint_to_fp, 0, 0>;
  def _#!cast<string>(typeB.Size)#_#!cast<string>(typeA)#_#!cast<string>(typeB)#_unsigned : CMU_ConvertingCP<typeA, typeB, uint_to_fp, 1, 0>;

  def _#!cast<string>(typeA.Size)#_#!cast<string>(typeB)#_#!cast<string>(typeA)#_signed   : CMU_ConvertingCP<typeB, typeA, fp_to_sint, 0, 0, "s">;
  def _#!cast<string>(typeA.Size)#_#!cast<string>(typeB)#_#!cast<string>(typeA)#_unsigned : CMU_ConvertingCP<typeB, typeA, fp_to_uint, 0, 1, "s">;
}

let Itinerary = II_CMU_CPVV_VRF_CONV in {
  // 32-element int to fp and fp to int converting copies
  foreach typeA = [v32i16, v32i8] in {
    defm CMU_CPVV : CMU_ConvertingCP_int_fp<typeA, v32f16>;
  }

  // 16-element int to fp and fp to int converting copies
  foreach typeA = [v16i32, v16i16, v16i8] in {
    foreach typeB = [v16f32, v16f16] in {
      defm CMU_CPVV : CMU_ConvertingCP_int_fp<typeA, typeB>;
    }
  }

  // 8-element int to fp and fp to int converting copies
  foreach typeA = [v8i32, v8i16, v8i8] in {
    foreach typeB = [v8f32, v8f16] in {
      defm CMU_CPVV : CMU_ConvertingCP_int_fp<typeA, typeB>;
    }
  }

  // 4-element int to fp and fp to int converting copies
  foreach typeA = [v4i32, v4i16] in {
    foreach typeB = [v4f32, v4f16] in {
      defm CMU_CPVV : CMU_ConvertingCP_int_fp<typeA, typeB>;
    }
  }

  // 2-element int to fp and fp to int converting copies
  foreach typeA = [v2i32] in {
    foreach typeB = [v2f32] in {
      defm CMU_CPVV : CMU_ConvertingCP_int_fp<typeA, typeB>;
    }
  }
}

//
// CMU.ITOF/UTOF/FTOI/FTOU IRF->IRF
//

class CMU_ConvertingCP_IRF<ValueType srcType, ValueType dstType,
                           SDNode op, bit srcUnsigned, bit dstUnsigned, string saturating = ""> :
  CMU_ConvertingCP_Base<SHAVE_TypeInfo<srcType, srcUnsigned>.mixedVectorRegClass, srcType, SHAVE_TypeInfo<dstType, srcUnsigned>.mixedVectorRegClass, dstType, op, 
                        !strconcat(".CP.", !cast<string>(dstType.Size),
                                   ".", SHAVE_TypeInfo<srcType, srcUnsigned>.asmstr, ".", SHAVE_TypeInfo<dstType, dstUnsigned>.asmstr,
                                   saturating),
                        !if(!lt(srcType.Size, 32), ".0", ""), 
                        !if(!lt(dstType.Size, 32), ".0", "")>;

multiclass CMU_ConvertingCP_IRF_int_fp<ValueType typeA, ValueType typeB> {
  def _#!cast<string>(typeB.Size)#_#!cast<string>(typeA)#_#!cast<string>(typeB)#_signed   : CMU_ConvertingCP_IRF<typeA, typeB, sint_to_fp, 0, 0>;
  def _#!cast<string>(typeB.Size)#_#!cast<string>(typeA)#_#!cast<string>(typeB)#_unsigned : CMU_ConvertingCP_IRF<typeA, typeB, uint_to_fp, 1, 0>;

  def _#!cast<string>(typeA.Size)#_#!cast<string>(typeB)#_#!cast<string>(typeA)#_signed   : CMU_ConvertingCP_IRF<typeB, typeA, fp_to_sint, 0, 0, "s">;
  def _#!cast<string>(typeA.Size)#_#!cast<string>(typeB)#_#!cast<string>(typeA)#_unsigned : CMU_ConvertingCP_IRF<typeB, typeA, fp_to_uint, 0, 1, "s">;
}

let Itinerary = II_CMU_CPII_IRF_FLOAT_INT in {
  defm CMU_CPII : CMU_ConvertingCP_IRF_int_fp<v2i16, v2f16>;
  defm CMU_CPII : CMU_ConvertingCP_IRF_int_fp<v2i8, v2f16>;
}

//
// CMU.ITOF/UTOF/FTOI/FTOU IRF->VRF and VRF->IRF
//

class CMU_ConvertingCPIV_CPVI_Myr4<SHAVE_TypeInfo srcType, SHAVE_TypeInfo dstType, SDNode op, string saturating = ""> :
  CMU_ConvertingCP_Base<srcType.mixedVectorRegClass, srcType.valueType, dstType.mixedVectorRegClass, dstType.valueType, op, 
                        !strconcat(".CP.", !cast<string>(dstType.valueType.Size),
                                   ".", srcType.asmstr, ".", dstType.asmstr,
                                   saturating),
                        !if(!ne(srcType.valueType.Size, 32), ".0", ""), 
                        !if(!ne(dstType.valueType.Size, 32), ".0", "")>;

multiclass CMU_CPIV_CPVI_int_fp_Myr4<ValueType intType, ValueType fpType> {
  let Itinerary = II_CMU_CPIV_VRF_IRF_FCONV in {
    def IV_#!cast<string>(intType)#_#!cast<string>(fpType)#_signed_Myr4   : CMU_ConvertingCPIV_CPVI_Myr4<SHAVE_TypeInfo<intType, 0>, SHAVE_TypeInfo<fpType>, sint_to_fp>;
    def IV_#!cast<string>(intType)#_#!cast<string>(fpType)#_unsigned_Myr4 : CMU_ConvertingCPIV_CPVI_Myr4<SHAVE_TypeInfo<intType, 1>, SHAVE_TypeInfo<fpType>, uint_to_fp>;
  }
  let Itinerary = II_CMU_CPVI_IRF_VRF_FCONV in {
    def VI_#!cast<string>(fpType)#_#!cast<string>(intType)#_signed_Myr4   : CMU_ConvertingCPIV_CPVI_Myr4<SHAVE_TypeInfo<fpType>, SHAVE_TypeInfo<intType, 0>, fp_to_sint, "s">;
    def VI_#!cast<string>(fpType)#_#!cast<string>(intType)#_unsigned_Myr4 : CMU_ConvertingCPIV_CPVI_Myr4<SHAVE_TypeInfo<fpType>, SHAVE_TypeInfo<intType, 1>, fp_to_uint, "s">;
  }
}

defm CMU_CP : CMU_CPIV_CPVI_int_fp_Myr4<v2i8,  v2f32>;
defm CMU_CP : CMU_CPIV_CPVI_int_fp_Myr4<v2i16, v2f32>;
defm CMU_CP : CMU_CPIV_CPVI_int_fp_Myr4<v4i8,  v4f32>;
defm CMU_CP : CMU_CPIV_CPVI_int_fp_Myr4<v4i8,  v4f16>;

let Itinerary = II_CMU_CPIV_VRF_IRF_FCONV in {
  def CMU_CPIV_v2f16_v2i32_signed_Myr4   : CMU_ConvertingCPIV_CPVI_Myr4<SHAVE_TypeInfo<v2f16>, SHAVE_TypeInfo<v2i32, 0>, fp_to_sint, "s">;
  def CMU_CPIV_v2f16_v2i32_unsigned_Myr4 : CMU_ConvertingCPIV_CPVI_Myr4<SHAVE_TypeInfo<v2f16>, SHAVE_TypeInfo<v2i32, 1>, fp_to_uint, "s">;
}
let Itinerary = II_CMU_CPVI_IRF_VRF_FCONV in {
  def CMU_CPVI_v2i32_v2f16_signed_Myr4   : CMU_ConvertingCPIV_CPVI_Myr4<SHAVE_TypeInfo<v2i32, 0>, SHAVE_TypeInfo<v2f16>, sint_to_fp>;
  def CMU_CPVI_v2i32_v2f16_unsigned_Myr4 : CMU_ConvertingCPIV_CPVI_Myr4<SHAVE_TypeInfo<v2i32, 1>, SHAVE_TypeInfo<v2f16>, uint_to_fp>;
}

//
// CMU.ITOI/UTOU
//

multiclass CMU_ITOI<ValueType srcVT, ValueType dstVT, RegisterClass srcRC,
                    RegisterClass dstRC, bit srcNeedsIdx, bit dstNeedsIdx,
                    list<SDNode> ops, bit isUnsigned, InstrItinClass instrItin> {
  defvar srcType = SHAVE_TypeInfo<srcVT, isUnsigned>;
  defvar dstType = SHAVE_TypeInfo<dstVT, isUnsigned>;
  defvar srcSuffix = !if(srcNeedsIdx, ".0", "");
  defvar dstSuffix = !if(dstNeedsIdx, ".0", "");
  defvar asmstr = ".CP."#dstVT.Size#"."#srcType.asmstr#"."#dstType.asmstr#
                  " $dst"#dstSuffix#" $src"#srcSuffix;

  let isAsCheapAsAMove = 1, Itinerary = instrItin in
  def _#dstVT.Size#_#srcType.asmstr#_#dstType.asmstr#"_Myr4" :
    CMU_CP<(outs dstRC:$dst), (ins srcRC:$src), asmstr,
           [(set (dstVT dstRC:$dst), (ops[0] (srcVT srcRC:$src)))]>;

  // Additional patterns.
  foreach op = !tail(ops) in
  def : Pat<(dstVT (op (srcVT srcRC:$src))),
            (!cast<Instruction>(
               NAME#_#dstVT.Size#_#srcType.asmstr#_#dstType.asmstr#"_Myr4")
             srcRC:$src)>;
}

multiclass CMU_ITOI_II<ValueType srcVT, ValueType dstVT, list<SDNode> ops,
                       bit isUnsigned = 0> {
  defvar srcRC = SHAVE_TypeInfo<srcVT, isUnsigned>.mixedVectorRegClass;
  defvar dstRC = SHAVE_TypeInfo<dstVT, isUnsigned>.mixedVectorRegClass;
  defvar srcNeedsIdx = !lt(srcVT.Size, 32);
  defvar dstNeedsIdx = !lt(dstVT.Size, 32);

  defm : CMU_ITOI<srcVT, dstVT, srcRC, dstRC, srcNeedsIdx, dstNeedsIdx, ops,
                  isUnsigned, II_CMU_CPII_IRF>;
}

multiclass CMU_ITOI_IV<ValueType srcVT, ValueType dstVT, list<SDNode> ops,
                       bit isUnsigned = 0> {
  defvar srcRC = SHAVE_TypeInfo<srcVT, isUnsigned>.mixedVectorRegClass;
  defvar dstRC = SHAVE_TypeInfo<dstVT, isUnsigned>.regClass;
  defvar srcNeedsIdx = !lt(srcVT.Size, 32);
  defvar dstNeedsIdx = !lt(dstVT.Size, 512);

  defm : CMU_ITOI<srcVT, dstVT, srcRC, dstRC, srcNeedsIdx, dstNeedsIdx, ops,
                  isUnsigned, II_CMU_CPIV_VRF_IRF>;
}

multiclass CMU_ITOI_VI<ValueType srcVT, ValueType dstVT, list<SDNode> ops,
                       bit isUnsigned = 0> {
  defvar srcRC = SHAVE_TypeInfo<srcVT, isUnsigned>.regClass;
  defvar dstRC = SHAVE_TypeInfo<dstVT, isUnsigned>.mixedVectorRegClass;
  defvar srcNeedsIdx = !lt(srcVT.Size, 512);
  defvar dstNeedsIdx = !lt(dstVT.Size, 32);

  defm : CMU_ITOI<srcVT, dstVT, srcRC, dstRC, srcNeedsIdx, dstNeedsIdx, ops,
                  isUnsigned, II_CMU_CPVI_IRF_VRF>;
}

multiclass CMU_ITOI_VV<ValueType srcVT, ValueType dstVT, list<SDNode> ops,
                       bit isUnsigned = 0> {
  defvar srcRC = SHAVE_TypeInfo<srcVT, isUnsigned>.regClass;
  defvar dstRC = SHAVE_TypeInfo<dstVT, isUnsigned>.regClass;
  defvar srcNeedsIdx = !lt(srcVT.Size, 512);
  defvar dstNeedsIdx = !lt(dstVT.Size, 512);

  defm : CMU_ITOI<srcVT, dstVT, srcRC, dstRC, srcNeedsIdx, dstNeedsIdx, ops,
                  isUnsigned, II_CMU_CPVV_VRF>;
}

// IRF <-> IRF
foreach types = [
    // FIXME: IRF>32 supperreg types disabled since these are not defined in
    // SHAVERegisterInfo.td yet.
    [i8, i16],  [v2i8, v2i16],  /*[v4i8, v4i16], [v8i8, v8i16],*/
    [i8, i32],  /*[v2i8, v2i32],  [v4i8, v4i32],*/
    [i16, i32], /*[v2i16, v2i32], [v4i16, v4i32]*/
] in {
  defm CMU_ITOI_II : CMU_ITOI_II<types[0], types[1], [sext]>;
  defm CMU_ITOI_II : CMU_ITOI_II<types[0], types[1], [zext, anyext], /*isUnsigned*/ 1>;
  defm CMU_ITOI_II : CMU_ITOI_II<types[1], types[0], [trunc]>;
}

// IRF <-> VRF
foreach types = [
    // FIXME: IRF>32 supperreg types disabled since these are not defined in
    // SHAVERegisterInfo.td yet.
    [v2i8, v2i16],  [v4i8, v4i16],  /*[v8i8, v8i16], [v16i8, v16i16],*/
    [v2i8, v2i32],  [v4i8, v4i32],  /*[v8i8, v8i32], [v16i8, v16i32],*/
    [v2i16, v2i32], /*[v4i16, v4i32], [v8i16, v8i32]*/
] in {
  defm CMU_ITOI_IV : CMU_ITOI_IV<types[0], types[1], [sext]>;
  defm CMU_ITOI_IV : CMU_ITOI_IV<types[0], types[1], [zext, anyext], /*isUnsigned*/ 1>;
  defm CMU_ITOI_VI : CMU_ITOI_VI<types[1], types[0], [trunc]>;
}
// FIXME: IRF>32 supperreg types disabled since these are not defined in
// SHAVERegisterInfo.td yet.
//foreach types = [
//    [v4i8, v4i16],  [v8i8, v8i16],
//    [v4i8, v4i32],  [v8i8, v8i32],
//    [v4i16, v4i32], [v8i16, v8i32]
//] in {
//  defm CMU_ITOI_VI : CMU_ITOI_VI<types[0], types[1], sext>;
//  defm CMU_ITOI_VI : CMU_ITOI_VI<types[0], types[1], zext, /*isUnsigned*/ 1>;
//  defm CMU_ITOI_IV : CMU_ITOI_IV<types[1], types[0], trunc>;
//}

// VRF <-> VRF
foreach types = [
                    [v4i8, v4i16],  [v8i8, v8i16],  [v16i8, v16i16], [v32i8, v32i16],
                    [v4i8, v4i32],  [v8i8, v8i32],  [v16i8, v16i32],
    [v2i16, v2i32], [v4i16, v4i32], [v8i16, v8i32], [v16i16, v16i32]
] in {
  defm CMU_ITOI_VV : CMU_ITOI_VV<types[0], types[1], [sext]>;
  defm CMU_ITOI_VV : CMU_ITOI_VV<types[0], types[1], [zext, anyext], /*isUnsigned*/ 1>;
  defm CMU_ITOI_VV : CMU_ITOI_VV<types[1], types[0], [trunc]>;
}

//
// CMU.FTOF
//

multiclass CMU_ConvertingCP_fp_fp<ValueType typeA, ValueType typeB> {
  def _#!cast<string>(typeB.Size)#_#!cast<string>(typeA)#_#!cast<string>(typeB) : CMU_ConvertingCP<typeA, typeB, fpextend, 0, 0>;

  def _#!cast<string>(typeA.Size)#_#!cast<string>(typeB)#_#!cast<string>(typeA) : CMU_ConvertingCP<typeB, typeA, fpround,  0, 0>;
}

let Itinerary = II_CMU_CPVV_VRF in {
  // 16-element fp to fp converting copies
  defm CMU_CPVV : CMU_ConvertingCP_fp_fp<v16f16, v16f32>;

  // 8-element fp to fp converting copies
  defm CMU_CPVV : CMU_ConvertingCP_fp_fp<v8f16,  v8f32>;

  // 4-element fp to fp converting copies
  defm CMU_CPVV : CMU_ConvertingCP_fp_fp<v4f16,  v4f32>;

  // 2-element fp to fp converting copies
  defm CMU_CPVV : CMU_ConvertingCP_fp_fp<v2f16,  v2f32>;
}

//
// CMU.SVEC
//

class CMU_SVEC<RegisterClass regClass, SDNode op, ValueType type, string asmstr> :
  SHAVE_CMUInstr<(outs regClass:$dst), (ins regClass:$src, i8imm:$boff),
                 !strconcat(asmstr," $dst $src $boff"),
                 [(set (type regClass:$dst), (op (type regClass:$src), (i8 imm:$boff)))]>;

let Itinerary = II_CMU_ALIGN_VRF in {
  foreach type = [v16i32, v32i16, v64i8, v16f32, v32f16] in {
    def CMU_ROTVEC_#!cast<string>(type) : CMU_SVEC<SHAVE_TypeInfo<type>.regClass, rotate_vector, type, ".ROTVEC">;
  }
}

//
// CMU.VILV/VDILV
//

class CMU_VILV_Base<SDNode opcode, RegisterClass regClass, ValueType type, string asmstr> :
  SHAVE_CMUInstr<(outs regClass:$dst1, regClass:$dst2), (ins regClass:$src1, regClass:$src2),
                 !strconcat(asmstr, " $dst1 $dst2 $src1 $src2"),
                 [(set (type regClass:$dst1), (type regClass:$dst2), (opcode (type regClass:$src1), (type regClass:$src2)))]>;

multiclass CMU_VILV<RegisterClass regClass, ValueType type, string asmstr> {
  def CMU_VILV_#!cast<string>(type)  : CMU_VILV_Base<interleave_vectors,   regClass, type, !strconcat(".VILV.", asmstr)>;
  def CMU_VDILV_#!cast<string>(type) : CMU_VILV_Base<deinterleave_vectors, regClass, type, !strconcat(".VDILV.", asmstr)>;
}

let Itinerary = II_CMU_VILV_VRF, hasSideEffects = 0 in {
  foreach type = [v16i32, v32i16, v64i8, v16f32, v32f16, // 512-bit vectors
                  v8i32,  v16i16, v32i8, v8f32,  v16f16, // 256-bit vectors
                  v4i32,  v8i16,  v16i8, v4f32,  v8f16,  // 128-bit vectors
                  v2i32,  v4i16,  v8i8,  v2f32,  v4f16,  // 64-bit vectors
                          v2i16,  v4i8,          v2f16,  // 32-bit vectors
                                  v2i8                   // 16-bit vectors
                 ] in {
    defm : CMU_VILV<SHAVE_TypeInfo<type>.regClass, type, SHAVE_TypeInfo<type, 0, 1>.asmstr>;
  }
}

class CMU_VILV_Combine_Base<SHAVE_TypeInfo dstType, SHAVE_TypeInfo srcType> :
  SHAVE_CMUInstr<(outs dstType.regClass:$dst1, dstType.regClass:$dst2), (ins srcType.regClass:$src1, srcType.regClass:$src2),
                 !strconcat(".VILV.", dstType.asmstr, " $dst1 $dst2 $src1 $src2"),
                 [(set (dstType.valueType dstType.regClass:$dst1),
                       (dstType.valueType dstType.regClass:$dst2),
                       (interleave_vectors_combine (srcType.valueType srcType.regClass:$src1),
                                                   (srcType.valueType srcType.regClass:$src2)))]>;

multiclass CMU_VILV_Combine<ValueType dst, ValueType src> {
  def _#!cast<string>(src)#_#!cast<string>(dst) : CMU_VILV_Combine_Base<SHAVE_TypeInfo<dst, 0, 1>, SHAVE_TypeInfo<src, 0, 1>>;
}

let Itinerary = II_CMU_VILV_VRF, hasSideEffects = 0 in {
  foreach pair = [ [v16i32, v8i32], [v32i16, v16i16], [v64i8, v32i8], [v16f32, v8f32], [v32f16, v16f16], // 512-bit vectors
                   [v8i32,  v4i32], [v16i16, v8i16],  [v32i8, v16i8], [v8f32,  v4f32], [v16f16, v8f16],  // 256-bit vectors
                   [v4i32,  v2i32], [v8i16,  v4i16],  [v16i8, v8i8],  [v4f32,  v2f32], [v8f16,  v4f16],  // 128-bit vectors
                                    [v4i16,  v2i16],  [v8i8,  v4i8],                   [v4f16,  v2f16],  // 64-bit vectors
                                                      [v4i8,  v2i8]                                      // 32-bit vectors
                 ] in {
    defm CMU_VILV : CMU_VILV_Combine<pair[0], pair[1]>;
  }
}

//
// CMU.CMB
//

class CMU_CMB<RegisterClass regClass, ValueType type> :
  SHAVE_CMUInstr<(outs regClass:$dst),
                 (ins regClass:$vsrc, regClass:$src, cmbSize:$size, ben:$en0, ben:$en1, ben:$en2, ben:$en3),
                 ".CMB.$size $dst $src [$en3$en2$en1$en0]",
                 [(set (type regClass:$dst), (combine_vectors (type regClass:$vsrc), (type regClass:$src), (i32 imm:$size),
                                                              (i8 imm:$en0), (i8 imm:$en1), (i8 imm:$en2), (i8 imm:$en3)))]> {
  let Constraints = "$dst = $vsrc";                 
}
    
let Itinerary = II_CMU_ALIGN_VRF in {
  foreach type = [v16i32, v32i16, v64i8, v16f32, v32f16, // 512-bit vectors
                  v8i32,  v16i16, v32i8, v8f32,  v16f16, // 256-bit vectors
                  v4i32,  v8i16,  v16i8, v4f32,  v8f16,  // 128-bit vectors
                  v2i32,  v4i16,  v8i8,  v2f32,  v4f16,  // 64-bit vectors
                          v2i16,  v4i8,          v2f16,  // 32-bit vectors
                                  v2i8                   // 16-bit vectors
                 ] in {
    def CMU_CMB_#!cast<string>(type) : CMU_CMB<SHAVE_TypeInfo<type>.regClass, type>;
  }
}       

//
// CMU.SHIV
//

class CMU_SHIV<RegisterClass vectorRegClass, RegisterClass scalarRegClass, string asmstr> :
  SHAVE_CMUInstr<(outs vectorRegClass:$dst), (ins vectorRegClass:$src1, scalarRegClass:$src2),
                 !strconcat(asmstr, " $dst $src1 $src2"), []> {
  let Constraints = "$src1 = $dst";
}

let Itinerary = II_CMU_SHLIV in {
  foreach type = [v16i32, v32i16, v64i8] in {
    def CMU_SHLIV_#SHAVE_TypeInfo<type, 0, 1>.asmstr#_VRF512 : CMU_SHIV<WVRF512, SHAVE_TypeInfo<type>.scalarRegClass,
                                                                        !strconcat(".SHLIV.", SHAVE_TypeInfo<type, 0, 1>.asmstr)>;

    def CMU_SHRIV_#SHAVE_TypeInfo<type, 0, 1>.asmstr#_VRF512 : CMU_SHIV<WVRF512, SHAVE_TypeInfo<type>.scalarRegClass,
                                                                        !strconcat(".SHRIV.", SHAVE_TypeInfo<type, 0, 1>.asmstr)>;
  }
}

//
// CMU.LUTR
//

let Itinerary = II_CMU_LUT_IRF in {
  foreach type = [ v16i32, v32i16, v64i8,  // 512-bit vectors
                   v8i32,  v16i16, v32i8,  // 256-bit vectors
                   v4i32,  v8i16,  v16i8,  // 128-bit vectors
                   v2i32,  v4i16,  v8i8,   // 64-bit vectors
		           v2i16,  v4i8    // 32-bit vectors
                 ] in {
    def CMU_LUTR_#!cast<string>(type)#_Myr4 :
      CMU_LUT_Raw<SHAVE_TypeInfo<type>.scalarRegClass, SHAVE_TypeInfo<type>.regClass, !strconcat("R.", SHAVE_TypeInfo<type>.asmstr)>;
    def CMU_LUTR_#!subst("i", "u", !cast<string>(type))#_Myr4 :
      CMU_LUT_Raw<SHAVE_TypeInfo<type, 1>.scalarRegClass, SHAVE_TypeInfo<type, 1>.regClass, !strconcat("R.", SHAVE_TypeInfo<type, 1>.asmstr)>;
  }
}

//
// CMU.LUTW
//

let Itinerary = II_CMU_LUTW_IRF in {
  foreach type = [ v16i32, v32i16, v64i8, // 512-bit vectors
                   v8i32,  v16i16, v32i8, // 256-bit vectors
                   v4i32,  v8i16,  v16i8, // 128-bit vectors
                   v2i32,  v4i16,  v8i8,  // 64-bit vectors
		           v2i16,  v4i8   // 32-bit vectors
                 ] in {
    def CMU_LUTW_#!cast<string>(type)#_Myr4 :
      CMU_LUTw_Raw<SHAVE_TypeInfo<type>.regClass, SHAVE_TypeInfo<type>.scalarRegClass, !strconcat(".", SHAVE_TypeInfo<type, 0, 1>.asmstr)>;
  }
}

//
// CMU.EXPAND/NDEXPAND
//

class CMU_EXPAND_Base<SDNode opcode, RegisterClass regClass, ValueType type, string asmstr> :
  SHAVE_CMUInstr<(outs regClass:$dst), (ins regClass:$src1, regClass:$src2),
                 !strconcat(asmstr, " $dst1 $src1 $src2"),
                 [(set (type regClass:$dst), (opcode (type regClass:$src1), (type regClass:$src2)))]>;

let Itinerary = II_CMU_EXPAND in {
  def CMU_EXPAND_x16_VRF512        : SHAVE_CMUInstr<(outs WVRF512:$dst), (ins WVRF512:$src1, IRF32:$src2), ".EXPAND.x16 $dst $src1 $src2">;
  def CMU_NDEXPAND_x16_VRF512      : SHAVE_CMUInstr<(outs WVRF512:$dst), (ins WVRF512:$src1, IRF32:$src2), ".NDEXPAND.x16 $dst $src1 $src2">;
  def CMU_EXPAND_x32_VRF512        : SHAVE_CMUInstr<(outs WVRF512:$dst), (ins WVRF512:$src1, IRF32:$src2), ".EXPAND.x32 $dst $src1 $src2">;
  def CMU_NDEXPAND_x32_VRF512      : SHAVE_CMUInstr<(outs WVRF512:$dst), (ins WVRF512:$src1, IRF32:$src2), ".NDEXPAND.x32 $dst $src1 $src2">;
}

let Itinerary = II_CMU_EXPAND_X8 in {
  def CMU_EXPAND_x8_VRF512        : SHAVE_CMUInstr<(outs WVRF512:$dst), (ins WVRF512:$src1, IRF64:$src2), ".EXPAND.x8 $dst $src1 $src2">;
  def CMU_NDEXPAND_x8_VRF512      : SHAVE_CMUInstr<(outs WVRF512:$dst), (ins WVRF512:$src1, IRF64:$src2), ".NDEXPAND.x8 $dst $src1 $src2">;
}

// FIXME: replace the Myr4 suffix with a more suitable one
// and add more accurate feature predicates for NPU6
foreach type = ["x32", "x16", "x8"] in {
  def CMU_PERM_#type:
    SHAVE_CMUInstr<(outs WVRF512:$dst), (ins WVRF512:$src1, WVRF512:$src2), !strconcat(".PERM.", type, " $dst $src1 $src2")> {
      let Itinerary = II_CMU_PERM;
  }

  def CMU_PERMB_#type:
    SHAVE_CMUInstr<(outs WVRF512:$dst), (ins WVRF512:$vsrc, WVRF512:$src1, WVRF512:$src2), !strconcat(".PERMB.", type, " $dst $src1 $src2")> {
      let Constraints = "$dst = $vsrc";
      let Itinerary = II_CMU_PERMB;
  }
}

// Separate definitions for shuffle lowering to the builtins above
class CMU_PERM_Base<RegisterClass dstRegClass, RegisterClass srcRegClass,
                    SDNode permuteOp, string asmstr,
                    ValueType dstType, ValueType srcType, ValueType maskType> :
  SHAVE_CMUInstr<(outs dstRegClass:$dst),
                 (ins srcRegClass:$source, dstRegClass:$mask),
                 !strconcat(asmstr, " $dst $source $mask"),
                 [(set (dstType dstRegClass:$dst),
                       (permuteOp (srcType srcRegClass:$source), (maskType dstRegClass:$mask)))]> {
  let Itinerary = II_CMU_PERM;
}

multiclass CMU_PERM<SHAVE_TypeInfo dstInfo, SHAVE_TypeInfo srcInfo> {
  defvar maskType = !cond(
                      !eq(dstInfo.valueType, v16f32) : v16i32,
                      !eq(dstInfo.valueType, v32f16) : v32i16,
                      true                           : dstInfo.valueType); // Mask type is the same as destination for integers

  def : CMU_PERM_Base<dstInfo.regClass, srcInfo.regClass,
                      permute_vector, !strconcat(".PERM.", dstInfo.asmstr),
                      dstInfo.valueType, srcInfo.valueType, maskType>;

  def _blend : CMU_PERM_Base<dstInfo.regClass, srcInfo.regClass,
                             permute_blend_vectors, !strconcat(".PERMB.", dstInfo.asmstr),
                             dstInfo.valueType, srcInfo.valueType, maskType>;
}

foreach types = [ [ [v16i32], [v16i32, v8i32]  ],
                  [ [v32i16], [v32i16, v16i16] ],
                  [ [v64i8],  [v64i8, v32i8]   ],
                  [ [v16f32], [v16f32, v8f32]  ],
                  [ [v32f16], [v32f16, v16f16] ] ] in {
  foreach srcType = types[1] in {
    defm CMU_PERM_#types[0][0]#_#srcType : CMU_PERM<SHAVE_TypeInfo<types[0][0], 0, 1>, SHAVE_TypeInfo<srcType>>;
  }
}

} // Predicates = [HasVRF512_Predicate]
