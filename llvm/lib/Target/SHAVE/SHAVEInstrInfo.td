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
// File       : SHAVEInstrInfo.td
// Description: Definitions of pseudoinstructions and selection patterns
// ---------------------------------------------------------------------------

def SDT_SHAVE_Combine : SDTypeProfile<1, 6, [
  SDTCisSameAs<0, 1>, SDTCisSameAs<0, 2>, SDTCisPtrTy<3>, SDTCisPtrTy<4>, SDTCisPtrTy<5>, SDTCisPtrTy<6>
]>;

def SDT_SHAVE_UITOFP : SDTypeProfile<1, 1, [
  SDTCisVec<0>, SDTCisFP<0>, SDTCisVec<1>, SDTCisInt<1>
]>;

def SDT_SHAVE_FPTOUI : SDTypeProfile<1, 1, [
  SDTCisVec<1>, SDTCisFP<1>, SDTCisVec<0>, SDTCisInt<0>
]>;

def splat : SDNode<"SHAVEISD::SPLAT", SDTypeProfile<1, -1, []>, []>;
def lanesplat : SDNode<"SHAVEISD::LANESPLAT", SDTypeProfile<1, 2, [SDTCisVec<0>, SDTCisVec<1>, SDTCisInt<2>]>, []>;
def rotate_vector : SDNode<"SHAVEISD::ROTATE_VECTOR", SDTypeProfile<1, 2, [SDTCisVec<0>, SDTCisVec<1>, SDTCisInt<2>]>, []>;
def alignvec : SDNode<"SHAVEISD::ALIGNVEC", SDTypeProfile<1, 3, [SDTCisVec<0>, SDTCisSameAs<0, 1>, SDTCisSameAs<0, 2>, SDTCisInt<3>]>, []>;
def interleave_vectors : SDNode<"SHAVEISD::INTERLEAVE_VECTORS", SDTypeProfile<2, 2, [SDTCisVec<0>, SDTCisSameAs<0, 1>, SDTCisSameAs<0, 2>, SDTCisSameAs<0, 3>]>, []>;
def interleave_vectors_combine : SDNode<"SHAVEISD::INTERLEAVE_VECTORS_COMBINE", SDTypeProfile<2, 2, [SDTCisVec<0>, SDTCisSameAs<0, 1>, SDTCisVec<2>, SDTCisSameAs<2, 3>]>, []>;
def deinterleave_vectors : SDNode<"SHAVEISD::DEINTERLEAVE_VECTORS", SDTypeProfile<2, 2, [SDTCisVec<0>, SDTCisSameAs<0, 1>, SDTCisSameAs<0, 2>, SDTCisSameAs<0, 3>]>, []>;
def combine_vectors : SDNode<"SHAVEISD::COMBINE_VECTORS", SDTypeProfile<1, 7, [SDTCisVec<0>, SDTCisSameAs<0, 1>, SDTCisSameAs<0, 2>, SDTCisInt<3>, 
                                                                               SDTCisInt<4>, SDTCisInt<5>, SDTCisInt<6>, SDTCisInt<7>]>, []>;
def compress_vector : SDNode<"SHAVEISD::COMPRESS_VECTOR", SDTypeProfile<1, 2, [SDTCisVec<0>, SDTCisSameAs<0, 1>, SDTCisInt<2>]>, []>;
def permute_vector : SDNode<"SHAVEISD::PERMUTE_VECTOR", SDTypeProfile<1, 2, [SDTCisVec<0>, SDTCisVec<1>, SDTCisVec<2>]>, []>;
def permute_blend_vectors : SDNode<"SHAVEISD::PERMUTE_BLEND_VECTORS", SDTypeProfile<1, 2, [SDTCisVec<0>, SDTCisVec<1>, SDTCisVec<2>]>, []>;
def unpack : SDNode<"SHAVEISD::UNPACK", SDTypeProfile<1, -1, []>, []>;
def combine_word : SDNode<"SHAVEISD::COMBINE_WORD", SDT_SHAVE_Combine, []>;
def combine_byte : SDNode<"SHAVEISD::COMBINE_BYTE", SDT_SHAVE_Combine, []>;
def uitofp_inreg : SDNode<"SHAVEISD::UITOFP_INREG", SDT_SHAVE_UITOFP, []>;
def fptoui_inreg : SDNode<"SHAVEISD::FPTOUI_INREG", SDT_SHAVE_FPTOUI, []>;
def shd : SDNode<"SHAVEISD::SHD", SDTIntShiftOp, []>;
def flog : SDNode<"ISD::FLOG"      , SDTFPUnaryOp>;
def fexp : SDNode<"ISD::FEXP"      , SDTFPUnaryOp>;

// These definitions are missing in llvm\Target\TargetSelectionDAG.td
// but required for capturing and selection bitwise op patterns
def vecreduce_and  : SDNode<"ISD::VECREDUCE_AND", SDTypeProfile<1, 1, [SDTCisInt<0>, SDTCisVec<1>]>>;
def vecreduce_or   : SDNode<"ISD::VECREDUCE_OR",  SDTypeProfile<1, 1, [SDTCisInt<0>, SDTCisVec<1>]>>;
def vecreduce_xor  : SDNode<"ISD::VECREDUCE_XOR", SDTypeProfile<1, 1, [SDTCisInt<0>, SDTCisVec<1>]>>;

// Byte enable (writeback mask) for CMB.WORD/BYTE
let PrintMethod = "printByteEnableOperand" in
{
  def ben : Operand<i8>;
}

// Size specifier for CMU.CMB on Myriad4v0
let PrintMethod = "printCMBSize" in {
  def cmbSize : Operand<i32>;
}

let Predicates = [HasVRF128_Predicate] in {
  def ZeroHi_v2i8 : SHAVE_Pseudo<(outs VRF128:$dst), (ins VRF128:$src), "ZeroHi_v2i8", []>;
  def ZeroHi_v4i8 : SHAVE_Pseudo<(outs VRF128:$dst), (ins VRF128:$src), "ZeroHi_v4i8", []>;
  def ZeroHi_v4i16 : SHAVE_Pseudo<(outs VRF128:$dst), (ins VRF128:$src), "ZeroHi_v4i16", []>;
  def ZeroHi_v8i8 : SHAVE_Pseudo<(outs VRF128:$dst), (ins VRF128:$src), "ZeroHi_v8i8", []>;

  def ZeroHi_v2i8_VRF64_l : SHAVE_Pseudo<(outs VRF64_l:$dst), (ins VRF64_l:$src), "ZeroHi_v2i8_VRF64_l", []>;
  def ZeroHi_v4i8_VRF64_l : SHAVE_Pseudo<(outs VRF64_l:$dst), (ins VRF64_l:$src), "ZeroHi_v4i8_VRF64_l", []>;
  def ZeroHi_v2i16_VRF64_l: SHAVE_Pseudo<(outs VRF64_l:$dst), (ins VRF64_l:$src), "ZeroHi_v2i16_VRF64_l", []>;
  def ZeroHi_v8i8_VRF64_l : SHAVE_Pseudo<(outs VRF64_l:$dst), (ins VRF64_l:$src), "ZeroHi_v8i8_VRF64_l", []>;
}

def SDTSHAVEVECTOR_EXTEND : SDTypeProfile<1, 1, []>;

def shave_vector_extend  : SDNode<"SHAVEISD::VECTOR_EXTEND", SDTSHAVEVECTOR_EXTEND>;

class VECTOR_EXTEND<RegisterClass inReg, RegisterClass outReg, ValueType inVT, ValueType outVT> : 
  SHAVE_Pseudo<(outs outReg:$dst), (ins inReg:$src), "VECTOR_EXTEND", [(set (outVT outReg:$dst), (shave_vector_extend (inVT inReg:$src)))]>;

def VECTOR_EXTEND_v8i8_v16i8 : VECTOR_EXTEND<VRF64_l, VRF128, v8i8, v16i8>;
def VECTOR_EXTEND_v4i16_v8i16 : VECTOR_EXTEND<VRF64_l, VRF128, v4i16, v8i16>;
def VECTOR_EXTEND_v2i32_v4i32 : VECTOR_EXTEND<VRF64_l, VRF128, v2i32, v4i32>;

def VECTOR_EXTEND_v4f16_v8f16 : VECTOR_EXTEND<VRF64_l, VRF128, v4f16, v8f16>;
def VECTOR_EXTEND_v2f32_v4f32 : VECTOR_EXTEND<VRF64_l, VRF128, v2f32, v4f32>;

def VECTOR_EXTEND_v2i16_v8i16 : VECTOR_EXTEND<VRF32_q0, VRF128, v2i16, v8i16>;
def VECTOR_EXTEND_v2f16_v8f16 : VECTOR_EXTEND<VRF32_q0, VRF128, v2f16, v8f16>;
def VECTOR_EXTEND_v4i8_v16i8  : VECTOR_EXTEND<VRF32_q0, VRF128, v4i8,  v16i8>;
def VECTOR_EXTEND_v2i8_v16i8  : VECTOR_EXTEND<VRF16_e0, VRF128, v2i8,  v16i8>;

//
// SHAVE_INSERT_SUBVECTOR pseudo instructions
// These pseudo instructions are used for vector subvector insertion
//

def SDT_SHAVE_INSERT_SUBVECTOR : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>, SDTCisVec<0>, SDTCisVec<1>, SDTCisVec<2>, SDTCisInt<3>]>;

def shave_insert_subvector : SDNode<"SHAVEISD::INSERT_SUBVECTOR", SDT_SHAVE_INSERT_SUBVECTOR>;

// FIXME: Movidius - We currently don't have an implementation for vector subvector insert
//                   with a variable index. When there is one, this block can be re-enabled
/*
class INSERT_SUBVECTOR<RegisterClass dstRC, RegisterClass srcRC, ValueType dstVT, ValueType srcVT> :
  SHAVE_Pseudo<(outs dstRC:$dst), (ins dstRC:$vector1, srcRC:$vector2, IRF32:$index), "SHAVE_INSERT_SUBVECTOR $dst $vector2 $index", 
               [(set (dstVT dstRC:$dst), (shave_insert_subvector (dstVT dstRC:$vector1), (srcVT srcRC:$vector2), (i32 IRF32:$index)))]>
{
  let Constraints = "$dst = $vector1";
}
*/

class INSERT_SUBVECTOR_imm<RegisterClass dstRC, RegisterClass srcRC, ValueType dstVT, ValueType srcVT> :
  SHAVE_Pseudo<(outs dstRC:$dst), (ins dstRC:$vector1, srcRC:$vector2, i32imm:$index), "SHAVE_INSERT_SUBVECTOR $dst $vector2 $index", 
               [(set (dstVT dstRC:$dst), (shave_insert_subvector (dstVT dstRC:$vector1), (srcVT srcRC:$vector2), (i32 imm:$index)))]>
{
  let Constraints = "$dst = $vector1";
}

let usesCustomInserter = 1 in {

// FIXME: Movidius - We currently don't have an implementation for vector subvector insert
//                   with a variable index. When there is one, this block can be re-enabled
/*
  def INSERT_SUBVECTOR_v2i32_v4i32 : INSERT_SUBVECTOR<VRF128,  VRF64_l, v4i32, v2i32>;
  def INSERT_SUBVECTOR_v4i16_v8i16 : INSERT_SUBVECTOR<VRF128,  VRF64_l, v8i16, v4i16>;
  def INSERT_SUBVECTOR_v2i16_v8i16 : INSERT_SUBVECTOR<VRF128,  IRF32,   v8i16, v2i16>;
  def INSERT_SUBVECTOR_v2i16_v4i16 : INSERT_SUBVECTOR<VRF64_l, IRF32,   v4i16, v2i16>;
  def INSERT_SUBVECTOR_v8i8_v16i8  : INSERT_SUBVECTOR<VRF128,  VRF64_l, v16i8, v8i8>;
  def INSERT_SUBVECTOR_v4i8_v16i8  : INSERT_SUBVECTOR<VRF128,  IRF32,   v16i8, v4i8>;
  def INSERT_SUBVECTOR_v2i8_v16i8  : INSERT_SUBVECTOR<VRF128,  IRF16_l, v16i8, v2i8>;
  def INSERT_SUBVECTOR_v4i8_v8i8   : INSERT_SUBVECTOR<VRF64_l, IRF32,   v8i8,  v4i8>;
  def INSERT_SUBVECTOR_v2i8_v8i8   : INSERT_SUBVECTOR<VRF64_l, IRF16_l, v8i8,  v2i8>;
  def INSERT_SUBVECTOR_v2i8_v4i8   : INSERT_SUBVECTOR<IRF32,   IRF16_l, v4i8,  v2i8>;
*/

  // Integer operations
  def INSERT_SUBVECTOR_imm_v2i32_v4i32 : INSERT_SUBVECTOR_imm<VRF128,  VRF64_l, v4i32, v2i32>;
  def INSERT_SUBVECTOR_imm_v4i16_v8i16 : INSERT_SUBVECTOR_imm<VRF128,  VRF64_l, v8i16, v4i16>;
  def INSERT_SUBVECTOR_imm_v2i16_v8i16 : INSERT_SUBVECTOR_imm<VRF128,  IRF32,   v8i16, v2i16>;
  def INSERT_SUBVECTOR_imm_v2i16_v4i16 : INSERT_SUBVECTOR_imm<VRF64_l, IRF32,   v4i16, v2i16>;
  def INSERT_SUBVECTOR_imm_v8i8_v16i8  : INSERT_SUBVECTOR_imm<VRF128,  VRF64_l, v16i8, v8i8>;
  def INSERT_SUBVECTOR_imm_v4i8_v16i8  : INSERT_SUBVECTOR_imm<VRF128,  IRF32,   v16i8, v4i8>;
  def INSERT_SUBVECTOR_imm_v2i8_v16i8  : INSERT_SUBVECTOR_imm<VRF128,  IRF16_l, v16i8, v2i8>;
  def INSERT_SUBVECTOR_imm_v4i8_v8i8   : INSERT_SUBVECTOR_imm<VRF64_l, IRF32,   v8i8,  v4i8>;
  def INSERT_SUBVECTOR_imm_v2i8_v8i8   : INSERT_SUBVECTOR_imm<VRF64_l, IRF16_l, v8i8,  v2i8>;
  def INSERT_SUBVECTOR_imm_v2i8_v4i8   : INSERT_SUBVECTOR_imm<IRF32,   IRF16_l, v4i8,  v2i8>;

  // Floating point operations
  def INSERT_SUBVECTOR_imm_v2f32_v4f32 : INSERT_SUBVECTOR_imm<VRF128,  VRF64_l, v4f32, v2f32>;
  def INSERT_SUBVECTOR_imm_v4f16_v8f16 : INSERT_SUBVECTOR_imm<VRF128,  VRF64_l, v8f16, v4f16>;
  def INSERT_SUBVECTOR_imm_v2f16_v8f16 : INSERT_SUBVECTOR_imm<VRF128,  IRF32,   v8f16, v2f16>;
  def INSERT_SUBVECTOR_imm_v2f16_v4f16 : INSERT_SUBVECTOR_imm<VRF64_l, IRF32,   v4f16, v2f16>;
}

let Predicates = [HasVRF128_Predicate] in {
def : Pat<(v4i32 (insert_subvector (v4i32 VRF128:$vector1), (v2i32 VRF64_l:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v2i32_v4i32 VRF128:$vector1, VRF64_l:$vector2, imm:$index)>;
def : Pat<(v8i16 (insert_subvector (v8i16 VRF128:$vector1), (v4i16 VRF64_l:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v4i16_v8i16 VRF128:$vector1, VRF64_l:$vector2, imm:$index)>;
def : Pat<(v8i16 (insert_subvector (v8i16 VRF128:$vector1), (v2i16 IRF32:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v2i16_v8i16 VRF128:$vector1, IRF32:$vector2, imm:$index)>;
def : Pat<(v4i16 (insert_subvector (v4i16 VRF64_l:$vector1), (v2i16 IRF32:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v2i16_v4i16 VRF64_l:$vector1, IRF32:$vector2, imm:$index)>;
def : Pat<(v16i8 (insert_subvector (v16i8 VRF128:$vector1), (v8i8 VRF64_l:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v8i8_v16i8 VRF128:$vector1, VRF64_l:$vector2, imm:$index)>;
def : Pat<(v16i8 (insert_subvector (v16i8 VRF128:$vector1), (v4i8 IRF32:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v4i8_v16i8 VRF128:$vector1, IRF32:$vector2, imm:$index)>;
def : Pat<(v16i8 (insert_subvector (v16i8 VRF128:$vector1), (v2i8 IRF16_l:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v2i8_v16i8 VRF128:$vector1, IRF16_l:$vector2, imm:$index)>;
def : Pat<(v8i8 (insert_subvector (v8i8 VRF64_l:$vector1), (v4i8 IRF32:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v4i8_v8i8 VRF64_l:$vector1, IRF32:$vector2, imm:$index)>;
def : Pat<(v8i8 (insert_subvector (v8i8 VRF64_l:$vector1), (v2i8 IRF16_l:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v2i8_v8i8 VRF64_l:$vector1, IRF16_l:$vector2, imm:$index)>;
def : Pat<(v4i8 (insert_subvector (v4i8 IRF32:$vector1), (v2i8 IRF16_l:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v2i8_v4i8 IRF32:$vector1, IRF16_l:$vector2, imm:$index)>;

def : Pat<(v4f32 (insert_subvector (v4f32 VRF128:$vector1), (v2f32 VRF64_l:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v2f32_v4f32 VRF128:$vector1, VRF64_l:$vector2, imm:$index)>;
def : Pat<(v8f16 (insert_subvector (v8f16 VRF128:$vector1), (v4f16 VRF64_l:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v4f16_v8f16 VRF128:$vector1, VRF64_l:$vector2, imm:$index)>;
def : Pat<(v8f16 (insert_subvector (v8f16 VRF128:$vector1), (v2f16 IRF32:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v2f16_v8f16 VRF128:$vector1, IRF32:$vector2, imm:$index)>;
def : Pat<(v4f16 (insert_subvector (v4f16 VRF64_l:$vector1), (v2f16 IRF32:$vector2), (i32 imm:$index))),
          (INSERT_SUBVECTOR_imm_v2f16_v4f16 VRF64_l:$vector1, IRF32:$vector2, imm:$index)>;
} // HasVRF128_Predicate

//
// SHAVE_EXTRACT_SUBVECTOR pseudo instructions
// These pseudo instructions are only used for subvector extraction with a variable index
// Subvector extraction with a constant index are handled in SHAVEISelDAGtoDAG
//

def SDT_SHAVE_EXTRACT_SUBVECTOR : SDTypeProfile<1, 2, [SDTCisVec<0>, SDTCisVec<1>, SDTCisInt<2>]>;

def shave_extract_subvector : SDNode<"SHAVEISD::EXTRACT_SUBVECTOR", SDT_SHAVE_EXTRACT_SUBVECTOR>;

class EXTRACT_SUBVECTOR<RegisterClass dstRC, RegisterClass srcRC, ValueType dstVT, ValueType srcVT> :
  SHAVE_Pseudo<(outs dstRC:$dst), (ins srcRC:$src, IRF32:$index), "SHAVE_EXTRACT_SUBVECTOR", 
               [(set (dstVT dstRC:$dst), (shave_extract_subvector (srcVT srcRC:$src), (i32 IRF32:$index)))]>, Requires<[HasVRF128_Predicate]>;

let usesCustomInserter = 1 in {
  // Integer operations
  def EXTRACT_SUBVECTOR_v2i32_v4i32 : EXTRACT_SUBVECTOR<VRF64_l, VRF128,  v2i32, v4i32>;
  def EXTRACT_SUBVECTOR_v4i16_v8i16 : EXTRACT_SUBVECTOR<VRF64_l, VRF128,  v4i16, v8i16>;
  def EXTRACT_SUBVECTOR_v2i16_v8i16 : EXTRACT_SUBVECTOR<IRF32,   VRF128,  v2i16, v8i16>;
  def EXTRACT_SUBVECTOR_v2i16_v4i16 : EXTRACT_SUBVECTOR<IRF32,   VRF64_l, v2i16, v4i16>;
  def EXTRACT_SUBVECTOR_v8i8_v16i8  : EXTRACT_SUBVECTOR<VRF64_l, VRF128,  v8i8,  v16i8>;
  def EXTRACT_SUBVECTOR_v4i8_v16i8  : EXTRACT_SUBVECTOR<IRF32,   VRF128,  v4i8,  v16i8>;
  def EXTRACT_SUBVECTOR_v2i8_v16i8  : EXTRACT_SUBVECTOR<IRF16_l, VRF128,  v2i8,  v16i8>;
  def EXTRACT_SUBVECTOR_v4i8_v8i8   : EXTRACT_SUBVECTOR<IRF32,   VRF64_l, v4i8,  v8i8>;
  def EXTRACT_SUBVECTOR_v2i8_v8i8   : EXTRACT_SUBVECTOR<IRF16_l, VRF64_l, v2i8,  v8i8>;
  def EXTRACT_SUBVECTOR_v2i8_v4i8   : EXTRACT_SUBVECTOR<IRF16_l, IRF32,   v2i8,  v4i8>;

  // Floating point operations
  def EXTRACT_SUBVECTOR_v2f32_v4f32 : EXTRACT_SUBVECTOR<VRF64_l, VRF128, v2f32, v4f32>;
  def EXTRACT_SUBVECTOR_v4f16_v8f16 : EXTRACT_SUBVECTOR<VRF64_l, VRF128, v4f16, v8f16>;
  def EXTRACT_SUBVECTOR_v2f16_v8f16 : EXTRACT_SUBVECTOR<IRF32,   VRF128, v2f16, v8f16>;
  def EXTRACT_SUBVECTOR_v2f16_v4f16 : EXTRACT_SUBVECTOR<IRF32,  VRF64_l, v2f16, v4f16>;
}

//
// Masked store pseudo instructions
//

def SDT_SHAVE_MASKED_STORE : SDTypeProfile<0, 2, [SDTCisVec<0>, SDTCisInt<1>]>;
def SDT_SHAVE_MASKED_STORE_L : SDTypeProfile<0, 2, [SDTCisVec<0>, SDTCisInt<1>]>;
def SDT_SHAVE_MASKED_STORE_H : SDTypeProfile<0, 2, [SDTCisVec<0>, SDTCisInt<1>]>;
def shave_masked_store : SDNode<"SHAVEISD::MASKED_STORE" , SDT_SHAVE_MASKED_STORE, [SDNPMayStore, SDNPHasChain]>;
def shave_masked_store_l : SDNode<"SHAVEISD::MASKED_STORE_L" , SDT_SHAVE_MASKED_STORE_L, [SDNPMayStore, SDNPHasChain]>;
def shave_masked_store_h : SDNode<"SHAVEISD::MASKED_STORE_H" , SDT_SHAVE_MASKED_STORE_H, [SDNPMayStore, SDNPHasChain]>;

let mayStore = 1, isPredicable = 1 in {
  def MASKED_STORE_WVRF_l : SHAVE_Pseudo<(outs), (ins WVRF512:$value, MEMr:$pointer, pred:$predicate), "MASKED_STORE", []>;
  def MASKED_STORE_WVRF_h : SHAVE_Pseudo<(outs), (ins WVRF512:$value, MEMr:$pointer, pred:$predicate), "MASKED_STORE", []>;
  def MASKED_STORE_VRF : SHAVE_Pseudo<(outs), (ins VRF128:$value, MEMr:$pointer, pred:$predicate), "MASKED_STORE", []>;
  def MASKED_STORE_IRF : SHAVE_Pseudo<(outs), (ins IRF32:$value, MEMr:$pointer, pred:$predicate), "MASKED_STORE", []>;
  def MASKED_STORE_WVRF : SHAVE_Pseudo<(outs), (ins WVRF256_0:$value, MEMr:$pointer, pred:$predicate), "MASKED_STORE", []>;
}

// Horizontal vector operation support
def SDT_SHAVE_HORIZONTAL_ADD : SDTypeProfile<1, 1, []>;
def horizontal_add : SDNode<"SHAVEISD::HORIZONTAL_ADD", SDT_SHAVE_HORIZONTAL_ADD, []>;

def SDT_SHAVE_HORIZONTAL_FADD : SDTypeProfile<1, 1, []>;
def horizontal_fadd : SDNode<"SHAVEISD::HORIZONTAL_FADD", SDT_SHAVE_HORIZONTAL_FADD, []>;

def SDT_SHAVE_HORIZONTAL_FADD_SUBVECTOR : SDTypeProfile<1, 2, []>;
def horizontal_fadd_subvector : SDNode<"SHAVEISD::HORIZONTAL_FADD_SUBVECTOR", SDT_SHAVE_HORIZONTAL_FADD_SUBVECTOR, []>;

def SDT_SHAVE_HORIZONTAL_AND : SDTypeProfile<1, 1, []>;
def horizontal_and : SDNode<"SHAVEISD::HORIZONTAL_AND", SDT_SHAVE_HORIZONTAL_AND, []>;

def SDT_SHAVE_HORIZONTAL_OR : SDTypeProfile<1, 1, []>;
def horizontal_or : SDNode<"SHAVEISD::HORIZONTAL_OR", SDT_SHAVE_HORIZONTAL_OR, []>;

def SDT_SHAVE_HORIZONTAL_XOR : SDTypeProfile<1, 1, []>;
def horizontal_xor : SDNode<"SHAVEISD::HORIZONTAL_XOR", SDT_SHAVE_HORIZONTAL_XOR, []>;

//
// MIN/MAX pseudo instructions
//

def SDTSHAVE_MINMAX : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>, SDTCisSameAs<0, 2>, SDTCisInt<3>]>;
def shave_min       : SDNode<"SHAVEISD::MIN", SDTSHAVE_MINMAX>;
def shave_max       : SDNode<"SHAVEISD::MAX", SDTSHAVE_MINMAX>;

//
// 128-bit load support with 64-bit halves from different addresses
//

let mayLoad = 1 in {
  def LOAD64_LOW     : SHAVE_Pseudo<(outs VRF128:$dst), (ins MEMr:$srcAddr), "LOAD64_LOW", []>;
  def LOAD64_LOW_Imm : SHAVE_Pseudo<(outs VRF128:$dst), (ins MEMri:$srcAddr), "LOAD64_LOW_Imm", []>;

  let Constraints = "$dst = $insertReg" in {
    def LOAD64_HIGH     : SHAVE_Pseudo<(outs VRF128:$dst), (ins VRF128:$insertReg, MEMr:$srcAddr), "LOAD64_HIGH", []>;
    def LOAD64_HIGH_Imm : SHAVE_Pseudo<(outs VRF128:$dst), (ins VRF128:$insertReg, MEMri:$srcAddr), "LOAD64_HIGH_Imm", []>;
  }
}

//
// Helper class for converting ValueType into an assembly string for Myriad4v0
//

class SHAVE_TypeInfo<ValueType vt, bit isUnsigned = 0, bit isBitwise = 0> {
  ValueType valueType = vt;

  ValueType scalarType = !cond(
    !and(!ge(vt.Value, v2i32.Value), !le(vt.Value, v16i32.Value)) : i32,
    !and(!ge(vt.Value, v2i16.Value), !le(vt.Value, v32i16.Value)) : i16,
    !and(!ge(vt.Value, v2i8.Value),  !le(vt.Value, v64i8.Value))  : i8,
    !and(!ge(vt.Value, v2f32.Value), !le(vt.Value, v16f32.Value)) : f32,
    !and(!ge(vt.Value, v2f16.Value), !le(vt.Value, v32f16.Value)) : f16,
    !eq(vt, i32)                                                  : i32,
    !eq(vt, i16)                                                  : i16,
    !eq(vt, i8)                                                   : i8,
    !eq(vt, f32)                                                  : f32,
    !eq(vt, f16)                                                  : f16,
    true                                                          : isVoid);

  // FIXME: Movidius - drop it when !div support is merged from upstream
  int log2OfScalarSize = !cond(
    !eq(scalarType.Size, 32) : 5,
    !eq(scalarType.Size, 16) : 4,
    !eq(scalarType.Size, 8)  : 3,
    true                     : 0);

  // FIXME: Movidius - replace with !div(vt.Size, scalarType.size)
  int vectorElementCount = !srl(vt.Size, log2OfScalarSize);

  string asmstr = !cond(
    !eq(scalarType, i32) : !if(isBitwise, "x32", !if(isUnsigned, "u32", "i32")),
    !eq(scalarType, i16) : !if(isBitwise, "x16", !if(isUnsigned, "u16", "i16")),
    !eq(scalarType, i8)  : !if(isBitwise, "x8",  !if(isUnsigned, "u8", "i8")),
    !eq(scalarType, f32) : !if(isBitwise, "x32", "f32"),
    !eq(scalarType, f16) : !if(isBitwise, "x16", "f16"),
    true                 : "");

  RegisterClass regClass = !cond(
    !eq(vt.Size, 512) : WVRF512,
    !eq(vt.Size, 256) : WVRF256_0,
    !eq(vt.Size, 128) : WVRF128_0,
    !eq(vt.Size, 64)  : WVRF64_0,
    !eq(vt.Size, 32)  : WVRF32_0,
    true              : WVRF16_0);

  RegisterClass regClassVRF = !cond(
    !eq(vt.Size, 128) : VRF128,
    !eq(vt.Size, 64)  : VRF64_l,
    !eq(vt.Size, 32)  : VRF32_q0,
    true              : VRF16_e0);

  RegisterClass mixedVectorRegClass = !cond(
    !eq(vt.Size, 512) : WVRF512,
    !eq(vt.Size, 256) : WVRF256_0,
    !eq(vt.Size, 128) : WVRF128_0,
    !eq(vt.Size, 64)  : WVRF64_0,
    !eq(vt.Size, 32)  : IRF32,
    !eq(vt.Size, 16)  : IRF16_l,
    true              : IRF8_q0);

  RegisterClass scalarRegClass = !cond(
    !eq(scalarType, i32) : IRF32,
    !eq(scalarType, i16) : IRF16_l,
    !eq(scalarType, i8)  : IRF8_q0,
    !eq(scalarType, f32) : IRF32,
    !eq(scalarType, f16) : IRF16_l,
    true                 : IRF32);

  Operand immediateOperand = !cond(
    !eq(scalarType, i32) : i32imm,
    !eq(scalarType, i16) : i16imm,
    !eq(scalarType, i8)  : i8imm,
    !eq(scalarType, f32) : constf32,
    !eq(scalarType, f16) : constf16,
    true                 : Operand<isVoid>);
}

// ***************************************************************************
// The Functional Unit specific instruction patterns
// ***************************************************************************
include "SHAVEInstrInfo_IAU.td"
include "SHAVEInstrInfo_BRU.td"
include "SHAVEInstrInfo_SAU.td"
include "SHAVEInstrInfo_VAU.td"
include "SHAVEInstrInfo_PEU.td"
include "SHAVEInstrInfo_CMU.td"
include "SHAVEInstrInfo_LSU.td"


// ***************************************************************************
// NOP Instructions
// ***************************************************************************
// NOP <-- probably the best instruction in the world
def NOP : SHAVEInstr<(outs), (ins), "NOP", PseudoUnit>
{
  let hasSideEffects = 1;
  let Itinerary = II_NOP;
}

// This fools tablegen into thinking all read/write ports are used, even the
// ones that actually aren't (e.g. IRF_R5).
def DUMMY : SHAVEInstr<(outs), (ins), "DUMMY", PseudoUnit>
{
  let Itinerary = II_DUMMY;
}

// ***************************************************************************
// Pseudo Instructions
// ***************************************************************************

let isTerminator = 1, Defs = [B_IP_0] in {
  // Unconditional branch to label (BB).
  def JMP_TO_LABEL : SHAVE_Predicable_Pseudo<(outs), (ins Label:$target),
                                             ".JMP_TO_LABEL $target", []>
  {
    let isBranch = 1;
    let isBarrier = 1;
    let hasCtrlDep = 1;
  }
  
  // Conditional branch to label (BB).
  def JMPcc_TO_LABEL : SHAVE_Predicable_Pseudo<(outs), (ins Label:$target),
                                               ".JMPcc_TO_LABEL $target", []>
  {
    let isBranch = 1;
    let hasCtrlDep = 1;
  }

  // Conditional branch to label (BB) with interrupt handling
  def JMPcc_TO_LABEL_PREEMPTION : SHAVE_Predicable_Pseudo<(outs IRF32:$internalRegOut), (ins Label:$target, IRF32:$internalReg),
                                                          ".JMPcc_TO_LABEL_PREEMPTION $target $internalReg", []>
  {
    let Constraints = "@earlyclobber $internalRegOut, $internalReg = $internalRegOut";
    let isBranch = 1;
    let hasCtrlDep = 1;
  }
}

/*
 *  Condition code handling
 */
def SHAVECondCode : Operand<i32>;

def SDTSHAVESetCC : SDTypeProfile<1, 3, [        // setcc
  SDTCisInt<0>, SDTCisSameAs<1, 2>, SDTCisVT<3, i32>
]>;

def SDTSHAVESelect : SDTypeProfile<1, 3, [     // select
  SDTCisSameAs<1, 2>, SDTCisSameAs<0, 1>, SDTCisVT<3, i32>]>;

def shave_setcc   : SDNode<"SHAVEISD::SETCC"     , SDTSHAVESetCC>;
def shave_select  : SDNode<"SHAVEISD::SELECT"    , SDTSHAVESelect, [SDNPInGlue]>;
def shave_vselect : SDNode<"SHAVEISD::VSELECT"   , SDTSHAVESelect, [SDNPInGlue]>;

/*
 *  Define the SELECT group
 */
class SELECT<RegisterClass RC, ValueType vt, Register ccIn>
    : SHAVE_Pseudo<(outs RC:$dst), (ins RC:$trueVal, RC:$falseVal, i32imm:$cc),
                    "SELECT", [(set (vt RC:$dst),
                    (shave_select (vt RC:$trueVal), (vt RC:$falseVal), timm:$cc))]> {
    let Uses = [ccIn];
    // dst and trueVal can only be equal if the predicate can be reversed which is not
    // guaranteed with the existence of ordered FP comparisons. If we allowed them to be
    // equal, then we would need a scratch register to store trueVal in if the predicate
    // couldn't be reversed
    let Constraints = "@earlyclobber $dst";
}

def SELECT_IRF64   : SELECT<IRF64,   i64, CC_CMU0>;
def SELECT_IRF32   : SELECT<IRF32,   i32, CC_CMU0>;
def SELECT_IRF16   : SELECT<IRF16_l, i16, CC_CMU0>;
def SELECT_IRF8    : SELECT<IRF8_q0, i8,  CC_CMU0>;

let Predicates = [HasVRF128_Predicate] in {
  def SELECT_VRF128  : SELECT<VRF128,  v4i32, CC_CMU0>;

  def SELECT_VRF64_v2i32 : SELECT<VRF64_l, v2i32, CC_CMU0>;
  def SELECT_VRF64_v2f32 : SELECT<VRF64_l, v2f32, CC_CMU0>;
  def SELECT_VRF64_v4i16 : SELECT<VRF64_l, v4i16, CC_CMU0>;
  def SELECT_VRF64_v4f16 : SELECT<VRF64_l, v4f16, CC_CMU0>;
  def SELECT_VRF64_v8i8  : SELECT<VRF64_l, v8i8,  CC_CMU0>;
}

def SELECT_IRF32_v2f16 : SELECT<IRF32,   v2f16, CC_CMU0>;
def SELECT_IRF32_v2i16 : SELECT<IRF32,   v2i16, CC_CMU0>;

def : Pat<(f32 (shave_select IRF32:$trueVal, IRF32:$falseVal, timm:$cc)),
          (SELECT_IRF32 IRF32:$trueVal, IRF32:$falseVal, i32imm:$cc)>;
def : Pat<(f16 (shave_select IRF16_l:$trueVal, IRF16_l:$falseVal, timm:$cc)),
          (SELECT_IRF16 IRF16_l:$trueVal, IRF16_l:$falseVal, i32imm:$cc)>;

def : Pat<(v2f16 (shave_select IRF32:$trueVal, IRF32:$falseVal, timm:$cc)),
          (SELECT_IRF32_v2f16 IRF32:$trueVal, IRF32:$falseVal, i32imm:$cc)>;
def : Pat<(v2i16 (shave_select IRF32:$trueVal, IRF32:$falseVal, timm:$cc)),
          (SELECT_IRF32_v2i16 IRF32:$trueVal, IRF32:$falseVal, i32imm:$cc)>;

let Predicates = [HasVRF128_Predicate] in {
  def : Pat<(v2f32 (shave_select VRF64_l:$trueVal, VRF64_l:$falseVal, timm:$cc)),
            (SELECT_VRF64_v2f32 VRF64_l:$trueVal, VRF64_l:$falseVal, i32imm:$cc)>;
  def : Pat<(v4f32 (shave_select VRF128:$trueVal, VRF128:$falseVal, timm:$cc)),
            (SELECT_VRF128 VRF128:$trueVal, VRF128:$falseVal, i32imm:$cc)>;
  def : Pat<(v8i16 (shave_select VRF128:$trueVal, VRF128:$falseVal, timm:$cc)),
            (SELECT_VRF128 VRF128:$trueVal, VRF128:$falseVal, i32imm:$cc)>;
  def : Pat<(v8f16 (shave_select VRF128:$trueVal, VRF128:$falseVal, timm:$cc)),
            (SELECT_VRF128 VRF128:$trueVal, VRF128:$falseVal, i32imm:$cc)>;
  def : Pat<(v16i8 (shave_select VRF128:$trueVal, VRF128:$falseVal, timm:$cc)),
            (SELECT_VRF128 VRF128:$trueVal, VRF128:$falseVal, i32imm:$cc)>;
}
/*
 *  Define the VSELECT group
 */
class VSELECT<RegisterClass RC, ValueType vt, Register ccIn>
    : SHAVE_Pseudo<(outs RC:$dst), (ins RC:$trueVal, RC:$falseVal, i32imm:$cc),
                    "VSELECT", [(set (vt RC:$dst),
                    (shave_vselect (vt RC:$trueVal), (vt RC:$falseVal), timm:$cc))]> {
    let Uses = [ccIn];
    let Constraints = "$dst = $falseVal";
}

// VRF vselect

let Predicates = [HasVRF128_Predicate] in {
  def VSELECT_VRF128_v4 : VSELECT<VRF128, v4i32, C_CMU_0_3>;
  def VSELECT_VRF128_v8 : VSELECT<VRF128, v8i16, C_CMU_0_7>;
  def VSELECT_VRF128_v16 : VSELECT<VRF128, v16i8, C_CMU_0_15>;

  def VSELECT_VRF64_v2 : VSELECT<VRF64_l, v2i32, C_CMU_0_3>;
  // In practice, only C_CMU_0_3 are used for v4x16 instructions, but the full
  // C_CMU_0_7 range is actually defined. When the select is expanded, the PEU
  // instruction will implicitly use the entire CMU condition code register for
  // a v8x16, so we need to define the entire register here. Otherwise, there
  // will be an implicit use of the register with no corresponding def.
  def VSELECT_VRF64_v4 : VSELECT<VRF64_l, v4i16, C_CMU_0_7>;
  def VSELECT_VRF64_v8 : VSELECT<VRF64_l, v8i8,  C_CMU_0_15>;  
}

def VSELECT_IRF32_v4 : VSELECT<IRF32, v4i8, C_CMU_0_3>;
def VSELECT_IRF32_v2 : VSELECT<IRF32, v2i16, C_CMU_0_1>;
def VSELECT_IRF16_v2 : VSELECT<IRF16_l, v2i8, C_CMU_0_1>;

let Predicates = [HasVRF128_Predicate] in {
def : Pat<(v4f32 (shave_vselect VRF128:$trueVal, VRF128:$falseVal, timm:$cc)),
          (VSELECT_VRF128_v4 VRF128:$trueVal, VRF128:$falseVal, imm:$cc)>;
def : Pat<(v2f32 (shave_vselect VRF64_l:$trueVal, VRF64_l:$falseVal, timm:$cc)),
          (VSELECT_VRF64_v2 VRF64_l:$trueVal, VRF64_l:$falseVal, imm:$cc)>;
def : Pat<(v8f16 (shave_vselect VRF128:$trueVal, VRF128:$falseVal, timm:$cc)),
          (VSELECT_VRF128_v8 VRF128:$trueVal, VRF128:$falseVal, imm:$cc)>;
def : Pat<(v4f16 (shave_vselect VRF64_l:$trueVal, VRF64_l:$falseVal, timm:$cc)),
          (VSELECT_VRF64_v4 VRF64_l:$trueVal, VRF64_l:$falseVal, imm:$cc)>;
def : Pat<(v2f16 (shave_vselect IRF32:$trueVal, IRF32:$falseVal, timm:$cc)),
          (VSELECT_IRF32_v2 IRF32:$trueVal, IRF32:$falseVal, imm:$cc)>;
}
/*
 *  Define the SHAVE_SETCC group
 */
class SHAVE_SETCC_Class<RegisterClass cmpRC, RegisterClass resRC, ValueType cmpVT, ValueType resVT, list<Register> ccDefs, list<SHAVEConflictName> confs = []> :
  SHAVE_Pseudo<(outs resRC:$dst), (ins cmpRC:$op1, cmpRC:$op2, SHAVECondCode:$cmp),
               "SHAVE_SETCC",
               [(set resRC:$dst, (resVT (shave_setcc (cmpVT cmpRC:$op1), (cmpVT cmpRC:$op2), timm:$cmp)))],
               !listconcat(confs, [usesCMU])>
{
  let Defs = ccDefs;
}

class SHAVE_SETCC_Scalar<RegisterClass cmpRC,
                         ValueType cmpVT,
                         list<Register> ccDefs,
                         list<SHAVEConflictName> confs = []> :
  SHAVE_SETCC_Class<cmpRC, IRF32, cmpVT, i32, ccDefs, !listconcat(confs, [usesIAU])>;

// The common Integer to Integer SETCC group
def SHAVE_SETCC_i32 : SHAVE_SETCC_Scalar<IRF32,   i32, [CC_CMU0]>;
def SHAVE_SETCC_i16 : SHAVE_SETCC_Scalar<IRF16_l, i16, [CC_CMU0, CC_CMU1]>;
def SHAVE_SETCC_i8  : SHAVE_SETCC_Scalar<IRF8_q0, i8,  [C_CMU_0_3]>;

// The FP to Integer SETCC group
def SHAVE_SETCC_f32 : SHAVE_SETCC_Scalar<IRF32,   f32, [CC_CMU0],          [isFloatingPointSETCC]>;
def SHAVE_SETCC_f16 : SHAVE_SETCC_Scalar<IRF16_l, f16, [CC_CMU0, CC_CMU1], [isFloatingPointSETCC]>;

multiclass SHAVE_SETCC_Myr4<RegisterClass cmpRC,
                            RegisterClass resRC,
                            ValueType cmpVT,
                            ValueType resVT,
                            list<Register> ccDefs,
                            list<SHAVEConflictName> confs = []> {
  def _Myr4 : SHAVE_SETCC_Class<cmpRC, resRC, cmpVT, resVT, ccDefs, !listconcat(confs, [usesVAU])>;
}

// The common Vector to Vector SETCC group
let Predicates = [HasVRF512_Predicate] in {
  // Integer variants
  // 512-bit vectors
  defm SHAVE_SETCC_v16i32_v16i32 : SHAVE_SETCC_Myr4<WVRF512,   WVRF512,   v16i32, v16i32, [C_CMU_0_3]>;
  defm SHAVE_SETCC_v32i16_v32i16 : SHAVE_SETCC_Myr4<WVRF512,   WVRF512,   v32i16, v32i16, [C_CMU0]>;
  defm SHAVE_SETCC_v64i8_v64i8   : SHAVE_SETCC_Myr4<WVRF512,   WVRF512,   v64i8,  v64i8,  [C_CMU_0_15]>;

  // 256-bit vectors
  defm SHAVE_SETCC_v8i32_v8i32   : SHAVE_SETCC_Myr4<WVRF256_0, WVRF256_0, v8i32,  v8i32,  [C_CMU_0_3]>;
  defm SHAVE_SETCC_v16i16_v16i16 : SHAVE_SETCC_Myr4<WVRF256_0, WVRF256_0, v16i16, v16i16, [C_CMU0]>;
  defm SHAVE_SETCC_v32i8_v32i8   : SHAVE_SETCC_Myr4<WVRF256_0, WVRF256_0, v32i8,  v32i8,  [C_CMU_0_15]>;

  // 128-bit vectors
  defm SHAVE_SETCC_v4i32_v4i32   : SHAVE_SETCC_Myr4<WVRF128_0, WVRF128_0, v4i32,  v4i32,  [C_CMU_0_3]>;
  defm SHAVE_SETCC_v8i16_v8i16   : SHAVE_SETCC_Myr4<WVRF128_0, WVRF128_0, v8i16,  v8i16,  [C_CMU0]>;
  defm SHAVE_SETCC_v16i8_v16i8   : SHAVE_SETCC_Myr4<WVRF128_0, WVRF128_0, v16i8,  v16i8,  [C_CMU_0_15]>;

  // 64-bit vectors
  defm SHAVE_SETCC_v2i32_v2i32   : SHAVE_SETCC_Myr4<WVRF64_0,  WVRF64_0,  v2i32,  v2i32,  [C_CMU_0_3]>;
  defm SHAVE_SETCC_v4i16_v4i16   : SHAVE_SETCC_Myr4<WVRF64_0,  WVRF64_0,  v4i16,  v4i16,  [C_CMU0]>;
  defm SHAVE_SETCC_v8i8_v8i8     : SHAVE_SETCC_Myr4<WVRF64_0,  WVRF64_0,  v8i8,   v8i8,   [C_CMU_0_15]>;

  // Floating-point variants
  // 512-bit vectors
  defm SHAVE_SETCC_v16f32_v16i32 : SHAVE_SETCC_Myr4<WVRF512,   WVRF512,   v16f32, v16i32, [C_CMU_0_3], [isFloatingPointSETCC]>;
  defm SHAVE_SETCC_v32f16_v32i16 : SHAVE_SETCC_Myr4<WVRF512,   WVRF512,   v32f16, v32i16, [C_CMU0],    [isFloatingPointSETCC]>;

  // 256-bit vectors
  defm SHAVE_SETCC_v8f32_v8i32   : SHAVE_SETCC_Myr4<WVRF256_0, WVRF256_0, v8f32,  v8i32,  [C_CMU_0_3], [isFloatingPointSETCC]>;
  defm SHAVE_SETCC_v16f16_v16i16 : SHAVE_SETCC_Myr4<WVRF256_0, WVRF256_0, v16f16, v16i16, [C_CMU0],    [isFloatingPointSETCC]>;

  // 128-bit vectors
  defm SHAVE_SETCC_v4f32_v4i32   : SHAVE_SETCC_Myr4<WVRF128_0, WVRF128_0, v4f32,  v4i32,  [C_CMU_0_3], [isFloatingPointSETCC]>;
  defm SHAVE_SETCC_v8f16_v8i16   : SHAVE_SETCC_Myr4<WVRF128_0, WVRF128_0, v8f16,  v8i16,  [C_CMU0],    [isFloatingPointSETCC]>;

  // 64-bit vectors
  defm SHAVE_SETCC_v2f32_v2i32   : SHAVE_SETCC_Myr4<WVRF64_0,  WVRF64_0,  v2f32,  v2i32,  [C_CMU_0_3], [isFloatingPointSETCC]>;
  defm SHAVE_SETCC_v4f16_v4i16   : SHAVE_SETCC_Myr4<WVRF64_0,  WVRF64_0,  v4f16,  v4i16,  [C_CMU0],    [isFloatingPointSETCC]>;
}

let Predicates = [HasVRF128_Predicate] in {
  def SHAVE_SETCC_v4i32_v4i32 : SHAVE_SETCC_Class<VRF128,  VRF128,  v4i32, v4i32, [C_CMU_0_3]>;
  def SHAVE_SETCC_v8i16_v8i16 : SHAVE_SETCC_Class<VRF128,  VRF128,  v8i16, v8i16, [C_CMU0]>;
  def SHAVE_SETCC_v16i8_v16i8 : SHAVE_SETCC_Class<VRF128,  VRF128,  v16i8, v16i8, [C_CMU_0_15]>;
  def SHAVE_SETCC_v2i32_v2i32 : SHAVE_SETCC_Class<VRF64_l, VRF64_l, v2i32, v2i32, [C_CMU_0_3]>;
  def SHAVE_SETCC_v4i16_v4i16 : SHAVE_SETCC_Class<VRF64_l, VRF64_l, v4i16, v4i16, [C_CMU0]>;
  def SHAVE_SETCC_v8i8_v8i8   : SHAVE_SETCC_Class<VRF64_l, VRF64_l, v8i8,  v8i8,  [C_CMU_0_15]>;

  def SHAVE_SETCC_v4f32_v4i32 : SHAVE_SETCC_Class<VRF128,  VRF128,  v4f32, v4i32, [C_CMU_0_3], [isFloatingPointSETCC]>;
  def SHAVE_SETCC_v2f32_v2i32 : SHAVE_SETCC_Class<VRF64_l, VRF64_l, v2f32, v2i32, [C_CMU_0_3], [isFloatingPointSETCC]>;
  def SHAVE_SETCC_v8f16_v8i16 : SHAVE_SETCC_Class<VRF128,  VRF128,  v8f16, v8i16, [C_CMU0],    [isFloatingPointSETCC]>;
  def SHAVE_SETCC_v4f16_v4i16 : SHAVE_SETCC_Class<VRF64_l, VRF64_l, v4f16, v4i16, [C_CMU0],    [isFloatingPointSETCC]>;
}

def SHAVE_SETCC_v4i8_v4i8   : SHAVE_SETCC_Class<IRF32,   IRF32,   v4i8,  v4i8,  [C_CMU_0_3]>;
def SHAVE_SETCC_v2i16_v2i16 : SHAVE_SETCC_Class<IRF32,   IRF32,   v2i16, v2i16, [CC_CMU0, CC_CMU1]>;
def SHAVE_SETCC_v2i8_v2i8   : SHAVE_SETCC_Class<IRF16_l, IRF16_l, v2i8,  v2i8,  [CC_CMU0, CC_CMU1]>;
def SHAVE_SETCC_v2f16_v2i16 : SHAVE_SETCC_Class<IRF32,   IRF32,   v2f16, v2i16, [CC_CMU0, CC_CMU1], [isFloatingPointSETCC]>;

//	End of SHAVE_SETCC group



// ***************************************************************************
// Various patterns
// - these are multiple SHAVE instructions that match a portion of the DAG
// ***************************************************************************

// FIXME: Movidius - Should separate these into a new post-Instruction TD file

// conversion
def CMU_CPII_u8_f32 : Pat<(f32(uint_to_fp IRF8_q0:$src)),
                          (CMU_CPII_i16f32(CMU_CPII_u8_u16 IRF8_q0:$src))>, Requires<[HasVRF128_Predicate]>;
def CMU_CPII_u16_f32 : Pat<(f32 (uint_to_fp (i16 IRF16_l:$src))),
                           (CMU_CPII_i32f32 (LSU_LDIH (INSERT_SUBREG (IMPLICIT_DEF), IRF16_l:$src, hsub_0), 0))>;
def CMU_CPII_u8_f16  : Pat<(f16(uint_to_fp IRF8_q0:$src)),
                           (CMU_CPII_i16f16 (CMU_CPII_u8_u16 IRF8_q0:$src))>, Requires<[HasVRF128_Predicate]>;
def CMU_CPII_u16_f16 : Pat<(f16 (uint_to_fp (i16 IRF16_l:$src))),
                           (CMU_CPII_i32f16 (LSU_LDIH (INSERT_SUBREG (IMPLICIT_DEF), IRF16_l:$src, hsub_0), 0))>;

// shifting
def CMU_SHR_u16 : Pat<(srl (i16 IRF16_l:$src), (i32 IRF32:$shamt)),
    (EXTRACT_SUBREG (IAU_SHR_u32 (LSU_LDIH (CMU_CPII_i16_i32_anyext IRF16_l:$src), 0), (i32 IRF32:$shamt)), hsub_0)>;

def CMU_SHR_u8 : Pat<(srl IRF8_q0:$src, (i32 IRF32:$shamt)),
    (EXTRACT_SUBREG (IAU_SHR_u32 (IAU_AND_32 (CMU_CPII_i8_i32_anyext IRF8_q0:$src), (LSU_LDIL 0x000000FF)), IRF32:$shamt), hsub_0)>;

def CMU_SHR_u16_imm : Pat<(srl (i16 IRF16_l:$src), (i32 uimm5:$shamt)),
    (EXTRACT_SUBREG (IAU_SHR_u32_imm (LSU_LDIH (CMU_CPII_i16_i32_anyext IRF16_l:$src), 0), uimm5:$shamt), hsub_0)>;

def CMU_SHR_u8_imm : Pat<(srl IRF8_q0:$src, (i32 uimm5:$shamt)),
    (EXTRACT_SUBREG (IAU_SHR_u32_imm (IAU_AND_32 (CMU_CPII_i8_i32_anyext IRF8_q0:$src), (LSU_LDIL 0x000000FF)), uimm5:$shamt), hsub_0)>;

// indirect branch
def : Pat<(br bb:$target),
          (BRU_JMP (LSU_LDIH_Label (LSU_LDIL_Label bb:$target), bb:$target))>;

// truncstore where MemVT = f16 and RegVT = IRF32
def : Pat<(truncstore (f32 IRF32:$src), ADDRr:$dst),
          (LSU_ST_f16 (CMU_CPII_f32_f16 IRF32:$src), ADDRr:$dst)>;

def CMU_CPVI_x32_l : SHAVE_CMUInstr<(outs IRF32:$dst), (ins VRF32_q0:$src, i8imm:$pos),
                     ".CP.32 $dst $src.$pos", []>
{
  let Itinerary = II_CMU_CPVI_IRF_VRF;
}

// Patterns for in-register sign extension.
def : Pat<(v4i32 (sext_inreg VRF128:$src, v4i8)),
          (CMU_CPVV_v4i8_v4i32_conv (CMU_CPVV_v4u32_v4u8_conv (ZeroHi_v4i8 VRF128:$src)))>, Requires<[HasVRF128_Predicate]>;

def : Pat<(v4i32 (sext_inreg VRF128:$src, v4i16)),
          (CMU_CPVV_v4i16_v4i32_conv (CMU_CPVV_v4u32_v4u16_conv (ZeroHi_v4i16 VRF128:$src)))>, Requires<[HasVRF128_Predicate]>;

def : Pat<(v8i16 (sext_inreg VRF128:$src, v8i8)),
          (CMU_CPVV_v8i8_v8i16_conv (CMU_CPVV_v8u16_v8u8_conv (ZeroHi_v8i8 VRF128:$src)))>, Requires<[HasVRF128_Predicate]>;

def : Pat<(v4i16 (sext_inreg VRF64_l:$src, v4i8)),
          (CMU_CPVV_v4i8_v4i16_conv (CMU_CPVV_v4u16_v4u8_conv (ZeroHi_v4i8_VRF64_l VRF64_l:$src)))>, Requires<[HasVRF128_Predicate]>;

// When sign-extending from v*i1, only the LSB should be taken into account.
// Clear all bits but the LSB before doing the extension.
def : Pat<(v4i32 (sext_inreg VRF128:$src, v4i1)),
          (VAU_ADD_i32_imm (VAU_NOT_i32 (VAU_AND_32 VRF128:$src, (CMU_CPIVR_i32 (LSU_LDIL 1)))), 1)>, Requires<[HasVRF128_Predicate]>;

def : Pat<(v8i16 (sext_inreg VRF128:$src, v8i1)),
          (VAU_ADD_i16_imm (VAU_NOT_i16 (VAU_AND_16 VRF128:$src, (CMU_CPIVR_i16 (EXTRACT_SUBREG (LSU_LDIL 1), hsub_0)))), 1)>, Requires<[HasVRF128_Predicate]>;

def : Pat<(v16i8 (sext_inreg VRF128:$src, v16i1)),
          (VAU_ADD_i8_imm (VAU_NOT_i8 (VAU_AND_8 VRF128:$src, (CMU_CPIVR_i8 (EXTRACT_SUBREG (LSU_LDIL 1), qsub_0)))), 1)>, Requires<[HasVRF128_Predicate]>;

def : Pat<(v4i16 (sext_inreg VRF64_l:$src, v4i1)),
          (VAU_ADD_i16_imm_v4i16 (VAU_NOT_v4i16 (VAU_AND_v4i16 VRF64_l:$src, (CMU_CPIVR_v4i16 (EXTRACT_SUBREG (LSU_LDIL 1), hsub_0)))), 1)>, Requires<[HasVRF128_Predicate]>;

def : Pat<(v8i8 (sext_inreg VRF64_l:$src, v8i1)),
          (VAU_ADD_i8_imm_v8i8 (VAU_NOT_v8i8 (VAU_AND_v8i8 VRF64_l:$src, (CMU_CPIVR_v8i8 (EXTRACT_SUBREG (LSU_LDIL 1), qsub_0)))), 1)>, Requires<[HasVRF128_Predicate]>;

// Vector in-register truncate.
def SDT_SHAVE_TruncateInreg : SDTypeProfile<1, 2, [SDTCisSameAs<0, 1>]>;
def truncate_inreg : SDNode<"SHAVEISD::TRUNCATE_INREG", SDT_SHAVE_TruncateInreg, []>;

def : Pat<(v16i8 (truncate_inreg VRF128:$src, v4i8)),
          (INSERT_SUBREG (v16i8 (IMPLICIT_DEF)), (CMU_CPVV_v4u32_v4u8_conv (ZeroHi_v4i8 VRF128:$src)), qsub_0)>, Requires<[HasVRF128_Predicate]>;
def : Pat<(v16i8 (truncate_inreg VRF128:$src, v8i8)),
          (INSERT_SUBREG (v16i8 (IMPLICIT_DEF)), (CMU_CPVV_v8u16_v8u8_conv (ZeroHi_v8i8 VRF128:$src)), hsub_0)>, Requires<[HasVRF128_Predicate]>;
def : Pat<(v8i16 (truncate_inreg VRF128:$src, v4i16)),
          (INSERT_SUBREG (v8i16 (IMPLICIT_DEF)), (CMU_CPVV_v4u32_v4u16_conv (ZeroHi_v4i16 VRF128:$src)), hsub_0)>, Requires<[HasVRF128_Predicate]>;

// Vector zero high bits.
def : Pat<(v4i32 (and VRF128:$src, (splat (i32 65535)))),
          (ZeroHi_v4i16 VRF128:$src)>, Requires<[HasVRF128_Predicate]>;
def : Pat<(v4i32 (and VRF128:$src, (splat (i32 255)))),
          (ZeroHi_v4i8 VRF128:$src)>, Requires<[HasVRF128_Predicate]>;
def : Pat<(v8i16 (and VRF128:$src, (splat (i32 255)))),
          (ZeroHi_v8i8 VRF128:$src)>, Requires<[HasVRF128_Predicate]>;

// Pseudo 128-bit vector load and store. These are used when spilling vectors to
// the stack as the spiller expects only one instruction to be emitted.
let mayLoad = 1 in {
  def LDV128 : SHAVE_Pseudo<(outs VRF128:$val), (ins MEMr:$addr),
                             "LDV128", [], [usesLSU0, usesLSU1]>;

  def LDOV128 : SHAVE_Pseudo<(outs VRF128:$val), (ins MEMri:$addr),
                             "LDOV128", [], [usesLSU0, usesLSU1]>;
}

let mayStore = 1 in {
  def STV128 : SHAVE_Pseudo<(outs), (ins VRF128:$val, MEMr:$addr),
                            "STV128", [], [usesLSU0, usesLSU1]>;

  def STOV128 : SHAVE_Pseudo<(outs), (ins VRF128:$val, MEMri:$addr),
                             "STOV128", [], [usesLSU0, usesLSU1]>;
}

let mayStore = 1 in {
  def STO32_VRF : SHAVE_Pseudo<(outs), (ins VRF32_q0:$val, MEMri:$addr),
                            "STO32_VRF", [], [usesLSU1]>;

  def STO16_VRF : SHAVE_Pseudo<(outs), (ins VRF16_e0:$val, MEMri:$addr),
                             "STO16_VRF", [], [usesLSU1]>;
}

// Move immediate instructions.
let hasSideEffects = 0, hasSideEffects = 0, isReMaterializable = 1 in {
  def LDImm32 : SHAVE_Pseudo<(outs IRF32:$dst), (ins i32imm:$src1),
                             "LDImm32", [], [usesLSU0, usesLSU1]>;
}

let hasSideEffects = 0, hasSideEffects = 0 in {
  def Unpack1 : SHAVE_Pseudo<(outs VRF128:$dst), (ins i32imm:$ty, i8imm:$positve, IRF32:$src),
                              "Unpack32", []>;
  def Unpack1_32bit : SHAVE_Pseudo<(outs VRF32_q0:$dst), (ins i32imm:$ty, i8imm:$positve, IRF32:$src),
                                    "Unpack32", []>;

  def Unpack1_16bit : SHAVE_Pseudo<(outs VRF16_e0:$dst), (ins i32imm:$ty, i8imm:$positve, IRF16_l:$src),
                                    "Unpack32", []>;

  def Unpack2 : SHAVE_Pseudo<(outs VRF128:$dst), (ins i32imm:$ty, i8imm:$positve, IRF32:$src1, IRF32:$src2),
                              "Unpack32", []>;

  def Unpack4 : SHAVE_Pseudo<(outs VRF128:$dst), (ins i32imm:$ty, i8imm:$positve, IRF32:$src1, IRF32:$src2, IRF32:$src3, IRF32:$src4),
                              "Unpack32", []>;
}

class INPUT_SWIZZLE<RegisterClass regVT> : 
  SHAVE_Pseudo<(outs regVT:$dst), 
               (ins regVT:$src, i8imm:$sel0, i8imm:$sel1, 
                i8imm:$sel2, i8imm:$sel3, i8imm:$sel4, 
                i8imm:$sel5, i8imm:$sel6, i8imm:$sel7),
               "INPUT_SWIZZLE8", []>
{
  let Constraints = "$dst = $src";
}

def INPUT_SWIZZLE8       : INPUT_SWIZZLE<VRF128>;
def INPUT_SWIZZLE8_vrf64 : INPUT_SWIZZLE<VRF64_l>;

/*
FIXME: Movidius - TODO: implementation of Memset.  See 'SHAVESelectionDAGInfo.cpp' for full details
def SDTSHAVEMemsetBlock : SDTypeProfile<0, 4, []>;

def shave_memset_block : SDNode<"SHAVEISD::MEMSET_BLOCK", SDTSHAVEMemsetBlock, [SDNPHasChain], "shave_memset_block">;

def SHAVE_MEMSET_BLOCK : SHAVE_Pseudo<(outs), (ins IRF32:$dst, IRF32:$count, IRF32:$value, IRF32:$inc),
                         "SHAVE_MEMSET_BLOCK", [(shave_memset_block (i32 IRF32:$dst), (i32 IRF32:$count), (i32 IRF32:$value), (i32 IRF32:$inc))]>
{
  let hasSideEffects = 1;
}
*/

/*
  CMU_LUT_Pseudo and CMU_LUTW_Pseudo instructions are pseudos used by SHAVEISelDAGtoDAG::SelectEXTRACT_VECTOR_ELT and 
  are later expanded to actual CMU.LUT instructions in SHAVEFixLUTOperands::fixLUTOperands(MachineBasicBlock::iterator I).
  They take an extra operand temp which is used after the pseudo expansion to setup the LUT index operand.
  This extra operand is also an output of the pseudo since its value is being overwritten internally.
  This operand is marked as earlyclobber to prevent the register allocator from assigning it the same register
  as $pos or $src, it must be unique.
*/
class CMU_LUT_Pseudo<RegisterClass dstRC, RegisterClass srcRC> :
  SHAVE_Pseudo<(outs dstRC:$dst, IRF32:$dtemp), (ins srcRC:$src, IRF32:$pos, IRF32:$temp), "CMU_LUT_Pseudo", [], [usesCMU]>
{
  let Constraints = "@earlyclobber $dtemp, $dtemp = $temp";
}

def CMU_LUT_i32_v4i32_pseudo      : CMU_LUT_Pseudo<IRF32,   VRF128>;
def CMU_LUT_i32_v2i32_pseudo      : CMU_LUT_Pseudo<IRF32,   VRF64_l>;
def CMU_LUT_i16_v8i16_pseudo      : CMU_LUT_Pseudo<IRF16_l, VRF128>;
def CMU_LUT_i16_v4i16_pseudo      : CMU_LUT_Pseudo<IRF16_l, VRF64_l>;
def CMU_LUT_i16_v2i16_pseudo      : CMU_LUT_Pseudo<IRF16_l, VRF32_q0>;
def CMU_LUT_i8_v16i8_pseudo       : CMU_LUT_Pseudo<IRF8_q0, VRF128>;
def CMU_LUT_i8_v8i8_pseudo        : CMU_LUT_Pseudo<IRF8_q0, VRF64_l>;
def CMU_LUT_i8_v4i8_pseudo        : CMU_LUT_Pseudo<IRF8_q0, VRF32_q0>;
def CMU_LUT_i8_v2i8_pseudo        : CMU_LUT_Pseudo<IRF8_q0, VRF16_e0>;

def CMU_LUT_f32_v4f32_pseudo : CMU_LUT_Pseudo<IRF32,   VRF128>;
def CMU_LUT_f32_v2f32_pseudo : CMU_LUT_Pseudo<IRF32,   VRF64_l>;
def CMU_LUT_f16_v8f16_pseudo : CMU_LUT_Pseudo<IRF16_l, VRF128>;
def CMU_LUT_f16_v4f16_pseudo : CMU_LUT_Pseudo<IRF16_l, VRF64_l>;
def CMU_LUT_f16_v2f16_pseudo : CMU_LUT_Pseudo<IRF16_l, VRF32_q0>;

// Myriad4.0 LUT pseudos
foreach type = [ v16i32, v32i16, v64i8, v16f32, v32f16, // 512-bit vectors
                 v8i32,  v16i16, v32i8, v8f32,  v16f16, // 256-bit vectors
                 v4i32,  v8i16,  v16i8, v4f32,  v8f16,  // 128-bit vectors
                 v2i32,  v4i16,  v8i8,  v2f32,  v4f16,  // 64-bit vectors
                         v2i16,  v4i8,          v2f16   // 32-bit vectors
               ] in {
  // Construct the pseudo instruction name, e.g. "CMU_LUT_i32_v16i32_Myr4_pseudo" for v16i32
  def CMU_LUT_#!cast<string>(SHAVE_TypeInfo<type>.scalarType)#_#!cast<string>(type)#_Myr4_pseudo :
    CMU_LUT_Pseudo<SHAVE_TypeInfo<type>.scalarRegClass, SHAVE_TypeInfo<type>.regClass>;
}

class CMU_LUTW_Pseudo<RegisterClass dstRC, RegisterClass srcRC> :
  SHAVE_Pseudo<(outs dstRC:$dst, IRF32:$dtemp), (ins dstRC:$src, srcRC:$val, IRF32:$pos, IRF32:$temp), "CMU_LUTW_Pseudo", [], [usesCMU]>
{
  let Constraints = "@earlyclobber $dtemp, $dtemp = $temp, $dst = $src";
}

def CMU_LUTW_i32_v4i32_pseudo      : CMU_LUTW_Pseudo<VRF128,  IRF32>;
def CMU_LUTW_i32_v2i32_pseudo      : CMU_LUTW_Pseudo<VRF64_l, IRF32>;
def CMU_LUTW_i16_v8i16_pseudo      : CMU_LUTW_Pseudo<VRF128,  IRF16_l>;
def CMU_LUTW_i16_v4i16_pseudo      : CMU_LUTW_Pseudo<VRF64_l, IRF16_l>;
def CMU_LUTW_i8_v16i8_pseudo       : CMU_LUTW_Pseudo<VRF128,  IRF8_q0>;
def CMU_LUTW_i8_v8i8_pseudo        : CMU_LUTW_Pseudo<VRF64_l, IRF8_q0>;

def CMU_LUTW_f32_v4f32_pseudo : CMU_LUTW_Pseudo<VRF128,  IRF32>;
def CMU_LUTW_f32_v2f32_pseudo : CMU_LUTW_Pseudo<VRF64_l, IRF32>;
def CMU_LUTW_f16_v8f16_pseudo : CMU_LUTW_Pseudo<VRF128,  IRF16_l>;
def CMU_LUTW_f16_v4f16_pseudo : CMU_LUTW_Pseudo<VRF64_l, IRF16_l>;

// Myriad4.0 LUTW pseudos
foreach type = [ v16i32, v32i16, v64i8, v16f32, v32f16, // 512-bit vectors
                 v8i32,  v16i16, v32i8, v8f32,  v16f16, // 256-bit vectors
                 v4i32,  v8i16,  v16i8, v4f32,  v8f16,  // 128-bit vectors
                 v2i32,  v4i16,  v8i8,  v2f32,  v4f16,  // 64-bit vectors
                         v2i16,  v4i8,          v2f16,  // 32-bit vectors
               ] in {
  // Construct the pseudo instruction name, e.g. "CMU_LUTW_i32_v16i32_Myr4_pseudo" for v16i32
  def CMU_LUTW_#!cast<string>(SHAVE_TypeInfo<type>.scalarType)#_#!cast<string>(type)#_Myr4_pseudo :
    CMU_LUTW_Pseudo<SHAVE_TypeInfo<type>.regClass, SHAVE_TypeInfo<type>.scalarRegClass>;
}

// i64 support helpers
def SDT_SHAVE_I64_EXTRACT_LOW : SDTypeProfile<1, 1, []>;
def i64_extract_low : SDNode<"SHAVEISD::I64_EXTRACT_LOW", SDT_SHAVE_I64_EXTRACT_LOW, []>;

def : Pat<(i32 (i64_extract_low IRF64:$src)),
          (EXTRACT_SUBREG IRF64:$src, (i32 vsub32_0))>;

def SDT_SHAVE_I64_EXTRACT_HIGH : SDTypeProfile<1, 1, []>;
def i64_extract_high : SDNode<"SHAVEISD::I64_EXTRACT_HIGH", SDT_SHAVE_I64_EXTRACT_HIGH, []>;

def : Pat<(i32 (i64_extract_high IRF64:$src)),
          (EXTRACT_SUBREG IRF64:$src, (i32 vsub32_1))>;

def SDT_SHAVE_I64_CONCAT : SDTypeProfile<1, 2, []>;
def i64_concat : SDNode<"SHAVEISD::I64_CONCAT", SDT_SHAVE_I64_CONCAT, []>;

// Concat i32 and i32 into i64
def : Pat<(i64 (i64_concat (i32 IRF32:$src_h), (i32 IRF32:$src_l))),
          (INSERT_SUBREG (INSERT_SUBREG (i64 (IMPLICIT_DEF)), IRF32:$src_l, vsub32_0), IRF32:$src_h, vsub32_1)>;

// Concat i32 and i16 into i64
def : Pat<(i64 (i64_concat (i32 IRF32:$src_h), (i16 IRF16_l:$src_l))),
          (INSERT_SUBREG 
            (INSERT_SUBREG (i64 (IMPLICIT_DEF)), 
              (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF16_l:$src_l, hsub_0), vsub32_0), 
            IRF32:$src_h, vsub32_1)>;

def : Pat<(i64 (i64_concat (i16 IRF16_l:$src_h), (i32 IRF32:$src_l))),
          (INSERT_SUBREG 
            (INSERT_SUBREG (i64 (IMPLICIT_DEF)), IRF32:$src_l, vsub32_0), 
            (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF16_l:$src_h, hsub_0), vsub32_1)>;

// Concat i32 and i8 into i64 
def : Pat<(i64 (i64_concat (i32 IRF32:$src_h), (i8 IRF8_q0:$src_l))),
          (INSERT_SUBREG 
            (INSERT_SUBREG (i64 (IMPLICIT_DEF)), 
              (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF8_q0:$src_l, qsub_0), vsub32_0), 
            IRF32:$src_h, vsub32_1)>;

def : Pat<(i64 (i64_concat (i8 IRF8_q0:$src_h), (i32 IRF32:$src_l))),
          (INSERT_SUBREG 
            (INSERT_SUBREG (i64 (IMPLICIT_DEF)), IRF32:$src_l, vsub32_0), 
            (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF8_q0:$src_h, qsub_0), vsub32_1)>;

// Concat i16 and i8 into i64
def : Pat<(i64 (i64_concat (i16 IRF16_l:$src_h), (i8 IRF8_q0:$src_l))),
          (INSERT_SUBREG 
            (INSERT_SUBREG (i64 (IMPLICIT_DEF)), 
              (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF8_q0:$src_l, qsub_0), vsub32_0), 
            (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF16_l:$src_h, hsub_0), vsub32_1)>;

def : Pat<(i64 (i64_concat (i8 IRF8_q0:$src_h), (i16 IRF16_l:$src_l))),
          (INSERT_SUBREG 
            (INSERT_SUBREG (i64 (IMPLICIT_DEF)), 
              (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF16_l:$src_l, hsub_0), vsub32_0), 
            (INSERT_SUBREG (i32 (IMPLICIT_DEF)), IRF8_q0:$src_h, qsub_0), vsub32_1)>;

let Predicates = [HasVRF128_Predicate] in {
  def : Pat<(i16 (vecreduce_add (v8i16 VRF128:$source))),
            (CMU_CPII_32_16_trunc (SAU_SUMX_i16 VRF128:$source))>;
  def : Pat<(i8 (vecreduce_add (v16i8 VRF128:$source))),
            (CMU_CPII_32_8_trunc (SAU_SUMX_i8 VRF128:$source))>;
}

include "SHAVEv3/SHAVEv3InstrInfo.td"

include "SHAVEInstrInfo_Mutating.td"
