//===- IntrinsicsShaveLSU.td - Defines Shave intrinsics ----*- tablegen -*-===//
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

multiclass LSU_Load_Intrinsic<string opcode, LLVMType type> {
  def _#opcode : SHAVE_Intrinsic<opcode, [ type ], [ llvm_ptr_ty ], [IntrReadMem]>;
}

multiclass LSU_Store_Intrinsic<string opcode, LLVMType type> {
  def _#opcode : SHAVE_Intrinsic<opcode, [], [ type, llvm_ptr_ty ], [IntrWriteMem]>;
}

//
//  LSU.LD
//

// U8[F] float converting loads
defm int_shave : LSU_Load_Intrinsic<"lsu_ld128_u8_f16_r",  llvm_v8f16_ty>;
defm int_shave : LSU_Load_Intrinsic<"lsu_ld128_u8f_f16_r", llvm_v8f16_ty>;

// B16 float converting loads
defm int_shave : LSU_Load_Intrinsic<"lsu_ld512_b16_f32_r", llvm_v16f32_ty>;
defm int_shave : LSU_Load_Intrinsic<"lsu_ld128_b16_f32_r", llvm_v4f32_ty>;
defm int_shave : LSU_Load_Intrinsic<"lsu_ld32_b16_f32_r",  llvm_float_ty>;

// Register load - these are "exclusive" loads that do not permit a simultaneous load on the other LSU in the same cycle
defm int_shave : LSU_Load_Intrinsic<"lsu_ld64_r_exclusive", llvm_i64_ty>;
defm int_shave : LSU_Load_Intrinsic<"lsu_ld32_r_exclusive", llvm_i32_ty>;
defm int_shave : LSU_Load_Intrinsic<"lsu_ld16_r_exclusive", llvm_i16_ty>;
defm int_shave : LSU_Load_Intrinsic<"lsu_ld8_r_exclusive",  llvm_i8_ty>;

// Sub-byte converting loads
defm int_shave : LSU_Load_Intrinsic<"lsu_ld32_i4_i8_r", llvm_v4i8_ty>;
defm int_shave : LSU_Load_Intrinsic<"lsu_ld32_u4_u8_r", llvm_v4i8_ty>;

// Small floats
defm int_shave : LSU_Load_Intrinsic<"lsu_ld32_b8_f16_r",  llvm_v2f16_ty>;
defm int_shave : LSU_Load_Intrinsic<"lsu_ld32_h8_f16_r",  llvm_v2f16_ty>;
defm int_shave : LSU_Load_Intrinsic<"lsu_ld512_b8_f16_r", llvm_v32f16_ty>;
defm int_shave : LSU_Load_Intrinsic<"lsu_ld512_h8_f16_r", llvm_v32f16_ty>;

// Sub-byte converting loads
defm int_shave : LSU_Load_Intrinsic<"lsu_ld512_i4_i8_r", llvm_v64i8_ty>;
defm int_shave : LSU_Load_Intrinsic<"lsu_ld512_u4_u8_r", llvm_v64i8_ty>;

//
//  LSU.ST
//

// B16 float converting stores
defm int_shave : LSU_Store_Intrinsic<"lsu_st256_f32_b16_rr", llvm_v16f32_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st64_f32_b16_rr",  llvm_v4f32_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st16_f32_b16_rr",  llvm_float_ty>;

// Register indirect store of specified type to memory.
defm int_shave : LSU_Store_Intrinsic<"lsu_st128_f16_u8_rr",   llvm_v8f16_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st_v8f16_v8u8s_rr", llvm_v8f16_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st128_f16_u8f_rr",  llvm_v8f16_ty>;

// Sub-byte converting stores
defm int_shave : LSU_Store_Intrinsic<"lsu_st16_i8_i4_rr",  llvm_v4i8_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st16_i8_i4s_rr", llvm_v4i8_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st16_u8_u4_rr",  llvm_v4i8_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st16_u8_u4s_rr", llvm_v4i8_ty>;

// Small floats
defm int_shave : LSU_Store_Intrinsic<"lsu_st16_f32_f16c_rr",  llvm_float_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st256_f32_f16c_rr", llvm_v16f32_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st16_f16_b8_rr",    llvm_v2f16_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st16_f16_b8c_rr",   llvm_v2f16_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st16_f16_h8_rr",    llvm_v2f16_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st16_f16_h8c_rr",   llvm_v2f16_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st256_f16_b8_rr",   llvm_v32f16_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st256_f16_b8c_rr",  llvm_v32f16_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st256_f16_h8_rr",   llvm_v32f16_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st256_f16_h8c_rr",  llvm_v32f16_ty>;

// Sub-byte converting stores
defm int_shave : LSU_Store_Intrinsic<"lsu_st256_i8_i4_rr",  llvm_v64i8_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st256_i8_i4s_rr", llvm_v64i8_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st256_u8_u4_rr",  llvm_v64i8_ty>;
defm int_shave : LSU_Store_Intrinsic<"lsu_st256_u8_u4s_rr", llvm_v64i8_ty>;
