//===- IntrinsicsShave.td - Defines Shave intrinsics -------*- tablegen -*-===//
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
// This file defines all of the Shave-specific intrinsics.
//
//===----------------------------------------------------------------------===//

class SHAVEType<LLVMType type> {
  LLVMType llvmType = type;

  // Extract the scalar type
  ValueType scalarType = !cond(
    !eq(type.VT.isVector, 1) : type.VT.ElementType,
    true                     : type.VT
  );

  // Extract the number of elements if a vector, otherwise empty string
  defvar nElements = !cond(
    !eq(type.VT.isVector, 1) : !cast<string>(type.VT.nElem),
    true                     : ""
  );

  // Scalar type strings
  string scalarString = !cast<string>(scalarType);

  string unsignedScalarString = !cond(
    !eq(scalarType.isInteger, 1) : !strconcat("u", !cast<string>(scalarType.Size)),
    true                         : ""
  );

  string bitwiseString = !strconcat("x", !cast<string>(scalarType.Size));

  // Vector type strings
  string vectorString = !cast<string>(type.VT);

  string unsignedVectorString = !cond(
    !eq(scalarType.isInteger, 1) : !strconcat("v", nElements, "u", !cast<string>(scalarType.Size)),
    true                         : ""
  );

  // Type strings
  string typeString = !cond(
    type.VT.isVector : vectorString,
    true             : scalarString
  );

  string unsignedTypeString = !cond(
    type.VT.isVector : unsignedVectorString,
    true             : unsignedScalarString
  );

  // Map the scalar type to the equivalent C type name
  defvar scalarC = !cond(
    !eq(scalarType.Value, f32.Value) : "float",
    !eq(scalarType.Value, f16.Value) : "half",
    !eq(scalarType.Value, i32.Value) : "int",
    !eq(scalarType.Value, i16.Value) : "short",
    !eq(scalarType.Value, i8.Value)  : "char",
    true                             : ""
  );

  // scalarC must have a value here
  assert !ne(!size(scalarC), 0), "Type not recognised: " # !cast<string>(type); 

  string cName = !strconcat(scalarC, nElements);
}

let TargetPrefix = "shave" in {
  class SHAVE_Intrinsic<string GCCIntSuffix,
                        list<LLVMType> ret_types,
                        list<LLVMType> param_types,
                        list<IntrinsicProperty> properties = [IntrNoMem]>
    : ClangBuiltin<!strconcat("__builtin_shave_", GCCIntSuffix)>,
      Intrinsic<ret_types, param_types, properties>;

  multiclass SHAVE_SuffixedIntrinsic<string GCCIntSuffix,
                                     list<LLVMType> ret_types,
                                     list<LLVMType> param_types,
                                     bit isUnsigned = 0,
                                     LLVMType suffix_type = ret_types[0]> {
    // Build the suffix name containing the C name for the output type (optionally include "u" for unsigned variants)
    defvar suffix = !strconcat(!cond(!eq(isUnsigned, 1) : "u", true : ""),
                               SHAVEType<suffix_type>.cName);

    def _#suffix : SHAVE_Intrinsic<!strconcat(GCCIntSuffix, "_", suffix),
                                   ret_types,
                                   param_types>;
  }

  // Getters/Setters for specific TRFs
  def int_shave_getcpuid : SHAVE_Intrinsic<"getcpuid", [llvm_i32_ty], [], [IntrNoMem]>;
  def int_shave_getP_CFG : SHAVE_Intrinsic<"getP_CFG", [llvm_i32_ty], [], [IntrReadMem]>;
  def int_shave_setP_CFG : SHAVE_Intrinsic<"setP_CFG", [], [llvm_i32_ty], [IntrWriteMem]>;

  // Tail writeback intrinsics
  // Floating-point vector types
  def int_shave_tail_writeback_v16f32 : SHAVE_Intrinsic<"tail_writeback_float16", [], [llvm_v16f32_ty,llvm_ptr_ty,llvm_i32_ty], [IntrWriteMem]>;
  def int_shave_tail_writeback_v32f16 : SHAVE_Intrinsic<"tail_writeback_half32", [], [llvm_v32f16_ty,llvm_ptr_ty,llvm_i32_ty], [IntrWriteMem]>;
  // Signed integer vector types
  def int_shave_tail_writeback_v16i32 : SHAVE_Intrinsic<"tail_writeback_int16", [], [llvm_v16i32_ty,llvm_ptr_ty,llvm_i32_ty], [IntrWriteMem]>;
  def int_shave_tail_writeback_v32i16 : SHAVE_Intrinsic<"tail_writeback_short32", [], [llvm_v32i16_ty,llvm_ptr_ty,llvm_i32_ty], [IntrWriteMem]>;
  def int_shave_tail_writeback_v64i8 : SHAVE_Intrinsic<"tail_writeback_char64", [], [llvm_v64i8_ty,llvm_ptr_ty,llvm_i32_ty], [IntrWriteMem]>;
  // Unsigned integer vector types
  def int_shave_tail_writeback_v16u32 : SHAVE_Intrinsic<"tail_writeback_uint16", [], [llvm_v16i32_ty,llvm_ptr_ty,llvm_i32_ty], [IntrWriteMem]>;
  def int_shave_tail_writeback_v32u16 : SHAVE_Intrinsic<"tail_writeback_ushort32", [], [llvm_v32i16_ty,llvm_ptr_ty,llvm_i32_ty], [IntrWriteMem]>;
  def int_shave_tail_writeback_v64u8 : SHAVE_Intrinsic<"tail_writeback_uchar64", [], [llvm_v64i8_ty,llvm_ptr_ty,llvm_i32_ty], [IntrWriteMem]>;

  // SHAVE ISA intrinsics, providing a 1:1 mapping to hardware instructions
  include "llvm/IR/IntrinsicsShaveBRU.td"
  include "llvm/IR/IntrinsicsShaveCMU.td"
  include "llvm/IR/IntrinsicsShaveIAU.td"
  include "llvm/IR/IntrinsicsShaveLSU.td"
  include "llvm/IR/IntrinsicsShaveSAU.td"
  include "llvm/IR/IntrinsicsShaveVAU.td"
}
