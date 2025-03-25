//===-- SHAVECallingConv.td - PEU Instructions -------------*- tablegen -*-===//
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
// Description: Describes the SHAVE calling conventions
//
// Special Registers:
//      I0    - Scratch register, effectively 'caller-saved', but not used for register allocation
//      I19   - Stack Pointer or SP
//      I30   - Link Register or LR
//      I31   - Frame Pointer or FP; only as needed, otherwise is is a normal 'callee-saved' register
//
//    The legacy calling convention passes arguments as follows:
//      i8, i16, i32, f16, f32  - I18, I17, I16, I15, I14, I13, I12 and I11
//      v4i32, v8i16, v16i8,
//      v4f32, v8f16            - V23, V22, V21, V20, V19, V18, V17 and V16
//    Return values are passed as follows:
//      i8, i16, i32, f16, f32  - I18
//      v4i32, v8i16, v16i8,
//      v4f32, v8f16            - V23
//
//    The following registers are preserved by the function (callee-saved):
//      I20, I21, I22, I23, I24, I25, I26, I27, I28, I29 and I31
//      V24, V25, V26, V27, V28, V29, V30 and V31
//
//    The following registers will not be preserved (caller-saved):
//      I0, I1, I2, I3, I4, I5, I6, I7, I8, I9, I10, I11, I12, I13, I14, I15, I16, I17, I18 and I30
//      V0, V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15, V16, V17, V18, V19, V20, V21, V22 and V23
//
//===----------------------------------------------------------------------===//


//  The legacy calling convention for Myriad2
def origShave_CallingConv  : CallingConv<[
  //  Promote single-bit integers to the nearest real integer type
  CCIfType<[i1], CCPromoteToType<i8>>,

  //  Pass 8-bit integers in the low quad of the corresponding IRF argument register
  CCIfNotVarArg<CCIfType<[i8], CCAssignToReg<[I18_q0, I17_q0, I16_q0, I15_q0, I14_q0, I13_q0, I12_q0, I11_q0]>>>,

  //  Pass 16-bit integers and floating-point in the low half of the corresponding IRF argument register
  CCIfNotVarArg<CCIfType<[i16, f16], CCAssignToReg<[I18_l, I17_l, I16_l, I15_l, I14_l, I13_l, I12_l, I11_l]>>>,

  //  Pass 32-bit integers and floating-point in the IRF argument registers
  CCIfNotVarArg<CCIfType<[i32, f32], CCAssignToReg<[I18, I17, I16, I15, I14, I13, I12, I11]>>>,

  //  Pass 64-bit integers in the IRF64 argument super-registers
  CCIfNotVarArg<CCIfType<[i64], CCAssignToReg<[I17_18, I16_17, I15_16, I14_15, I13_14, I12_13, I11_12]>>>,

  //  Pass all native 16-bit vectors in the low eighth of the VRF argument registers
  CCIfNotVarArg<CCIfType<[v2i8], CCAssignToReg<[V23_e0, V22_e0, V21_e0, V20_e0, V19_e0, V18_e0, V17_e0, V16_e0]>>>,

  //  Pass all native 32-bit vectors in the low quad of the VRF argument registers
  CCIfNotVarArg<CCIfType<[v4i8, v2i16, v2f16], CCAssignToReg<[V23_q0, V22_q0, V21_q0, V20_q0, V19_q0, V18_q0, V17_q0, V16_q0]>>>,

  //  Pass all native 64-bit vectors in the low half of the VRF argument registers
  CCIfNotVarArg<CCIfType<[v8i8, v4i16, v2i32, v4f16, v2f32], CCAssignToReg<[V23_l, V22_l, V21_l, V20_l, V19_l, V18_l, V17_l, V16_l]>>>,

  //  Pass all native 128-bit vectors in the VRF argument registers
  CCIfNotVarArg<CCIfType<[v16i8, v8i16, v4i32, v8f16, v4f32], CCAssignToReg<[V23, V22, V21, V20, V19, V18, V17, V16]>>>,

  //  Otherwise promote them appropriately, and put them on the stack
  CCIfNotVarArg<CCIfType<[i8], CCAssignToStack<1, 1>>>,                                         // 8-bit scalars
  CCIfNotVarArg<CCIfType<[i16, f16], CCAssignToStack<2, 2>>>,                                   // 16-bit scalars
  CCIfNotVarArg<CCIfType<[i32, f32], CCAssignToStack<4, 4>>>,                                   // 32-bit scalars
  CCIfNotVarArg<CCIfType<[i64], CCAssignToStack<8, 8>>>,                                        // 64-bit scalars
  CCIfNotVarArg<CCIfType<[v2i8], CCAssignToStack<2, 2>>>,                                       // 16-bit vectors
  CCIfNotVarArg<CCIfType<[v4i8, v2i16, v2f16], CCAssignToStack<4, 4>>>,                         // 32-bit vectors
  CCIfNotVarArg<CCIfType<[v8i8, v4i16, v2i32, v4f16, v2f32], CCAssignToStack<8, 8>>>,           // 64-bit vectors
  CCIfNotVarArg<CCIfType<[v16i8, v8i16, v4i32, v2i64, v8f16, v4f32], CCAssignToStack<16, 8>>>,  // 128-bit vectors
  CCIfNotVarArg<CCIfType<[v16i16, v8i32, v4i64, v16f16, v8f32], CCAssignToStack<32, 8>>>,       // 256-bit vectors
  CCIfNotVarArg<CCIfType<[v16i32, v8i64, v16f32], CCAssignToStack<64, 8>>>,                     // 512-bit vectors
  CCIfNotVarArg<CCIfType<[v16i64], CCAssignToStack<128, 8>>>,                                   // 1024-bit vectors

  // Anything else needs to be placed on the stack, and aligned to 8-bytes (legacy ABI requirement)
  CCIfType<[i8, i16], CCPromoteToType<i32>>,                                                    // 8-bit and 16-bit integers are promoted to 32-bit integers and then stacked
  CCIfType<[f16], CCPromoteToType<f32>>,                                                        // 16-bit floating-point scalars are promoted to 32-bit floating-point and then stacked
  CCIfType<[i32, f32], CCAssignToStack<4, 8>>,                                                  // 32-bit scalars
  CCIfType<[i64], CCAssignToStack<8, 8>>,                                                       // 64-bit scalars
  CCIfType<[v2i8], CCAssignToStack<16, 8>>,                                                     // 16-bit vectors
  CCIfType<[v4i8, v2i16, v2f16], CCAssignToStack<16, 8>>,                                       // 32-bit vectors
  CCIfType<[v8i8, v4i16, v2i32, v4f16, v2f32], CCAssignToStack<16, 8>>,                         // 64-bit vectors
  CCIfType<[v16i8, v8i16, v4i32, v2i64, v8f16, v4f32], CCAssignToStack<16, 8>>,                 // 128-bit vectors
  CCIfType<[v16i16, v8i32, v4i64, v16f16, v8f32], CCAssignToStack<32, 8>>,                      // 256-bit vectors
  CCIfType<[v16i32, v8i64, v16f32], CCAssignToStack<64, 8>>,                                    // 512-bit vectors
  CCIfType<[v16i64], CCAssignToStack<128, 8>>                                                   // 1024-bit vectors
]>;

def origShave_ReturnConv : CallingConv<[
  //  Promote single-bit integers to the nearest real integer type
  CCIfType<[i1], CCPromoteToType<i8>>,

  // Return 8-bit integers in the low quad of the IRF registers.  The reason for this is that LLVM can break up
  // irregular sized vectors into a sequence of scalars - e.g. 'v3i8' for which there is no explicit LLVM type
  CCIfType<[i8], CCAssignToReg<[I18_q0, I17_q0, I16_q0, I15_q0, I14_q0, I13_q0, I12_q0, I11_q0]>>,

  //  Return 16-bit integers and floating-point in the low half of the IRF 18 register
  CCIfType<[i16, f16], CCAssignToReg<[I18_l, I17_l, I16_l, I15_l, I14_l, I13_l, I12_l, I11_l]>>,

  //  Return 32-bit integers and floating-point in the IRF 18 register
  CCIfType<[i32, f32], CCAssignToReg<[I18, I17, I16, I15, I14, I13, I12, I11]>>,

  // Return large vectors on the stack
  CCIfType<[v2i64], CCAssignToStack<16, 8>>,                                                    // 128-bit vectors
  CCIfType<[v16i16, v8i32, v4i64, v16f16, v8f32], CCAssignToStack<32, 8>>,                      // 256-bit vectors
  CCIfType<[v16i32, v8i64, v16f32], CCAssignToStack<64, 8>>,                                    // 512-bit vectors
  CCIfType<[v16i64], CCAssignToStack<128, 8>>,                                                  // 1024-bit vectors

  // Return 64-bit integers in the IRF64 super-registers
  CCIfType<[i64], CCAssignToReg<[I17_18, I16_17, I15_16, I14_15, I13_14, I12_13, I11_12]>>,     // Allow overlapping super-registers
//CCIfType<[i64], CCAssignToReg<[I17_18,         I15_16,         I13_14,         I11_12]>>,     // Allow non-overlapping super-registers

  // Return all 16-bit vectors in the low eighth of the VRF registers
  CCIfType<[v2i8], CCAssignToReg<[V23_e0, V22_e0, V21_e0, V20_e0, V19_e0, V18_e0, V17_e0, V16_e0]>>,

  // Return all 32-bit vectors in the low quad of the VRF registers
  CCIfType<[v4i8, v2i16, v2f16], CCAssignToReg<[V23_q0, V22_q0, V21_q0, V20_q0, V19_q0, V18_q0, V17_q0, V16_q0]>>,

  // Return all 64-bit vectors in the low half of the VRF registers
  CCIfType<[v8i8, v4i16, v2i32, v4f16, v2f32], CCAssignToReg<[V23_l, V22_l, V21_l, V20_l, V19_l, V18_l, V17_l, V16_l]>>,

  // Return all native vectors in the VRF registers
  CCIfType<[v16i8, v8i16, v4i32, v8f16, v4f32], CCAssignToReg<[V23, V22, V21, V20, V19, V18, V17, V16]>>
]>;

//  The std calling convention for Myriad2
def stdShave_CallingConv : CallingConv<[
  //  Promote to nearest real integer type
  CCIfType<[i1], CCPromoteToType<i8>>,

  //  Pass 8-bit integers in the low quad of the corresponding IRF argument register
  CCIfNotVarArg<CCIfType<[i8], CCAssignToReg<[I18_q0, I17_q0, I16_q0, I15_q0, I14_q0, I13_q0, I12_q0, I11_q0]>>>,

  //  Pass 16-bit integers and floating-point in the low half of the corresponding IRF argument register
  CCIfNotVarArg<CCIfType<[i16, f16], CCAssignToReg<[I18_l, I17_l, I16_l, I15_l, I14_l, I13_l, I12_l, I11_l]>>>,

  //  Pass 32-bit integers and floating-point in the IRF argument registers
  CCIfNotVarArg<CCIfType<[i32, f32], CCAssignToReg<[I18, I17, I16, I15, I14, I13, I12, I11]>>>,

  //  Pass 64-bit integers in the IRF64 argument super-registers
  CCIfNotVarArg<CCIfType<[i64], CCAssignToReg<[I17_18, I16_17, I15_16, I14_15, I13_14, I12_13, I11_12]>>>,

  // Pass all native 64-bit vectors in the low half of the VRF argument registers
  CCIfNotVarArg<CCIfType<[v2i32, v4i16, v8i8, v4f16], CCAssignToReg<[V23_l, V22_l, V21_l, V20_l, V19_l, V18_l, V17_l, V16_l]>>>,

  //  Pass all native 128-bit vectors in the VRF argument registers
  CCIfNotVarArg<CCIfType<[v4i32, v8i16, v16i8, v4f32, v8f16], CCAssignToReg<[V23, V22, V21, V20, V19, V18, V17, V16]>>>,

  //  Otherwise promote them appropriately, and put them on the stack
  CCIfNotVarArg<CCIfType<[i8], CCAssignToStack<1, 1>>>,
  CCIfNotVarArg<CCIfType<[i16, f16], CCAssignToStack<2, 2>>>,
  CCIfNotVarArg<CCIfType<[i32, f32], CCAssignToStack<4, 4>>>,
  CCIfNotVarArg<CCIfType<[v2i32, v4i16, v8i8, v4f16], CCAssignToStack<16, 8>>>,
  CCIfNotVarArg<CCIfType<[v4i32, v8i16, v16i8, v4f32, v8f16], CCAssignToStack<16, 8>>>,

  CCIfType<[i8, i16], CCPromoteToType<i32>>,
  CCIfType<[f16], CCPromoteToType<f32>>,
  CCIfType<[i32, f32], CCAssignToStack<4, 8>>,
  CCIfType<[v2i32, v4i16, v8i8, v4f16], CCAssignToStack<16, 8>>,
  CCIfType<[v4i32, v8i16, v16i8, v4f32, v8f16], CCAssignToStack<16, 8>>
]>;

def stdShave_ReturnConv  : CallingConv<[
  //  Promote to nearest real integer type
  CCIfType<[i1], CCPromoteToType<i8>>,

  // Return 8-bit integers in the low quad of the IRF registers.  The reason for this is that LLVM can break up
  // irregular sized vectors into a sequence of scalars - e.g. 'v3i8' for which there is no explicit LLVM type
  CCIfType<[i8], CCAssignToReg<[I18_q0, I17_q0, I16_q0, I15_q0, I14_q0, I13_q0, I12_q0, I11_q0]>>,

  //  Return 16-bit integers and floating-point in the low half of the IRF 18 register
  CCIfType<[i16, f16], CCAssignToReg<[I18_l]>>,

  //  Return 32-bit integers and floating-point in the IRF 18 register
  CCIfType<[i32, f32], CCAssignToReg<[I18, I17]>>,

  // Return 64-bit integers in the IRF64 I17_18 super-register
  CCIfType<[i64], CCAssignToReg<[I17_18, I16_17, I15_16, I14_15, I13_14, I12_13, I11_12]>>,

  // Return all 32-bit vectors in the low quad of the VRF registers
  CCIfType<[v4i8, v2i16, v2f16], CCAssignToReg<[V23_q0, V22_q0, V21_q0, V20_q0, V19_q0, V18_q0, V17_q0, V16_q0]>>,

  // Return all 64-bit vectors in the low half of the VRF registers
  CCIfType<[v2i32, v4i16, v8i8, v4f16], CCAssignToReg<[V23_l, V22_l, V21_l, V20_l, V19_l, V18_l, V17_l, V16_l]>>,

  // Return all native vectors in the VRF registers
  CCIfType<[v4i32, v8i16, v16i8, v4f32, v8f16], CCAssignToReg<[V23, V22, V21, V20, V19, V18, V17, V16]>>
]>;

//
// Myriad 3.0 Calling Convention
//

def shaveV3_CallingConv  : CallingConv<[
  //  Promote single-bit integers to the nearest real integer type
  CCIfType<[i1], CCPromoteToType<i8>>,

  //  Pass 8-bit integers in the low quad of the corresponding IRF argument register
  CCIfNotVarArg<CCIfType<[i8], CCAssignToReg<[I18_q0, I17_q0, I16_q0, I15_q0, I14_q0, I13_q0, I12_q0, I11_q0]>>>,

  //  Pass 16-bit integers and floating-point in the low half of the corresponding IRF argument register
  CCIfNotVarArg<CCIfType<[i16, f16], CCAssignToReg<[I18_l, I17_l, I16_l, I15_l, I14_l, I13_l, I12_l, I11_l]>>>,

  //  Pass 32-bit integers and floating-point in the IRF argument registers
  CCIfNotVarArg<CCIfType<[i32, f32], CCAssignToReg<[I18, I17, I16, I15, I14, I13, I12, I11]>>>,

  //  Pass 64-bit integers in the IRF64 argument super-registers
  CCIfNotVarArg<CCIfType<[i64], CCAssignToReg<[I17_18, I16_17, I15_16, I14_15, I13_14, I12_13, I11_12]>>>,

  // FIXME: Movidius_v3 - Can/should we pass 16 and 32-bit vectors in IRF instead of VRF?
  //  Pass all native 16-bit vectors in the low eighth of the VRF argument registers
  CCIfNotVarArg<CCIfType<[v2i8], CCAssignToReg<[W23_16_0, W22_16_0, W21_16_0, W20_16_0, W19_16_0, W18_16_0, W17_16_0, W16_16_0]>>>,

  //  Pass all native 32-bit vectors in the low quad of the VRF argument registers
  CCIfNotVarArg<CCIfType<[v4i8, v2i16, v2f16], CCAssignToReg<[W23_32_0, W22_32_0, W21_32_0, W20_32_0, W19_32_0, W18_32_0, W17_32_0, W16_32_0]>>>,

  //  Pass all native 64-bit vectors in the low half of the VRF argument registers
  CCIfNotVarArg<CCIfType<[v8i8, v4i16, v2i32, v4f16, v2f32], CCAssignToReg<[W23_64_0, W22_64_0, W21_64_0, W20_64_0, W19_64_0, W18_64_0, W17_64_0, W16_64_0]>>>,

  //  Pass all native 128-bit vectors in the VRF argument registers
  CCIfNotVarArg<CCIfType<[v16i8, v8i16, v4i32, v8f16, v4f32], CCAssignToReg<[W23_128_0, W22_128_0, W21_128_0, W20_128_0, W19_128_0, W18_128_0, W17_128_0, W16_128_0]>>>,

  //  Pass all native 256-bit vectors in the VRF argument registers
  CCIfNotVarArg<CCIfType<[v32i8, v16i16, v8i32, v16f16, v8f32], CCAssignToReg<[W23_256_0, W22_256_0, W21_256_0, W20_256_0, W19_256_0, W18_256_0, W17_256_0, W16_256_0]>>>,

  //  Pass all native 512-bit vectors in the WVRF argument registers
  CCIfNotVarArg<CCIfType<[v64i8, v32i16, v16i32, v32f16, v16f32], CCAssignToReg<[W23, W22, W21, W20, W19, W18, W17, W16]>>>,

  //  Otherwise promote them appropriately, and put them on the stack
  CCIfNotVarArg<CCIfType<[i8], CCAssignToStack<1, 1>>>,                                         // 8-bit scalars
  CCIfNotVarArg<CCIfType<[i16, f16], CCAssignToStack<2, 2>>>,                                   // 16-bit scalars
  CCIfNotVarArg<CCIfType<[i32, f32], CCAssignToStack<4, 4>>>,                                   // 32-bit scalars
  CCIfNotVarArg<CCIfType<[i64], CCAssignToStack<8, 8>>>,                                        // 64-bit scalars
  CCIfNotVarArg<CCIfType<[v2i8], CCAssignToStack<2, 2>>>,                                       // 16-bit vectors
  CCIfNotVarArg<CCIfType<[v4i8, v2i16, v2f16], CCAssignToStack<4, 4>>>,                         // 32-bit vectors
  CCIfNotVarArg<CCIfType<[v8i8, v4i16, v2i32, v4f16, v2f32], CCAssignToStack<8, 8>>>,           // 64-bit vectors
  CCIfNotVarArg<CCIfType<[v16i8, v8i16, v4i32, v2i64, v8f16, v4f32], CCAssignToStack<16, 8>>>,  // 128-bit vectors
  CCIfNotVarArg<CCIfType<[v16i16, v8i32, v4i64, v16f16, v8f32], CCAssignToStack<32, 8>>>,       // 256-bit vectors
  CCIfNotVarArg<CCIfType<[v16i32, v8i64, v32f16, v16f32], CCAssignToStack<64, 8>>>,             // 512-bit vectors
  CCIfNotVarArg<CCIfType<[v16i64], CCAssignToStack<128, 8>>>,                                   // 1024-bit vectors

  // Anything else needs to be placed on the stack, and aligned to 8-bytes (legacy ABI requirement)
  CCIfType<[i8, i16], CCPromoteToType<i32>>,                                                    // 8-bit and 16-bit integers are promoted to 32-bit integers and then stacked
  CCIfType<[f16], CCPromoteToType<f32>>,                                                        // 16-bit floating-point scalars are promoted to 32-bit floating-point and then stacked
  CCIfType<[i32, f32], CCAssignToStack<4, 8>>,                                                  // 32-bit scalars
  CCIfType<[i64], CCAssignToStack<8, 8>>,                                                       // 64-bit scalars
  CCIfType<[v2i8], CCAssignToStack<16, 8>>,                                                     // 16-bit vectors
  CCIfType<[v4i8, v2i16, v2f16], CCAssignToStack<16, 8>>,                                       // 32-bit vectors
  CCIfType<[v8i8, v4i16, v2i32, v4f16, v2f32], CCAssignToStack<16, 8>>,                         // 64-bit vectors
  CCIfType<[v16i8, v8i16, v4i32, v2i64, v8f16, v4f32], CCAssignToStack<16, 8>>,                 // 128-bit vectors
  CCIfType<[v16i16, v8i32, v4i64, v16f16, v8f32], CCAssignToStack<32, 8>>,                      // 256-bit vectors
  CCIfType<[v16i32, v8i64, v32f16, v16f32], CCAssignToStack<64, 8>>,                            // 512-bit vectors
  CCIfType<[v16i64], CCAssignToStack<128, 8>>                                                   // 1024-bit vectors
]>;

def shaveV3_ReturnConv : CallingConv<[
  //  Promote single-bit integers to the nearest real integer type
  CCIfType<[i1], CCPromoteToType<i8>>,

  // Return 8-bit integers in the low quad of the IRF registers.  The reason for this is that LLVM can break up
  // irregular sized vectors into a sequence of scalars - e.g. 'v3i8' for which there is no explicit LLVM type
  CCIfType<[i8], CCAssignToReg<[I18_q0, I17_q0, I16_q0, I15_q0, I14_q0, I13_q0, I12_q0, I11_q0]>>,

  //  Return 16-bit integers and floating-point in the low half of the IRF 18 register
  CCIfType<[i16, f16], CCAssignToReg<[I18_l, I17_l, I16_l, I15_l, I14_l, I13_l, I12_l, I11_l]>>,

  //  Return 32-bit integers and floating-point in the IRF 18 register
  CCIfType<[i32, f32], CCAssignToReg<[I18, I17, I16, I15, I14, I13, I12, I11]>>,

  // Return large vectors on the stack
  CCIfType<[v2i64], CCAssignToStack<16, 8>>,                                                    // 128-bit vectors
  CCIfType<[v4i64], CCAssignToStack<32, 8>>,                                                    // 256-bit vectors
  CCIfType<[v8i64], CCAssignToStack<64, 8>>,                                                    // 512-bit vectors
  CCIfType<[v16i64], CCAssignToStack<128, 8>>,                                                  // 1024-bit vectors

  // Return 64-bit integers in the IRF64 super-registers
  CCIfType<[i64], CCAssignToReg<[I17_18, I16_17, I15_16, I14_15, I13_14, I12_13, I11_12]>>,     // Allow overlapping super-registers

  // FIXME: Movidius_v3 - Can/should we pass 16 and 32-bit vectors in IRF instead of VRF?
  //  Pass all native 16-bit vectors in the low eighth of the VRF argument registers
  CCIfType<[v2i8], CCAssignToReg<[W23_16_0, W22_16_0, W21_16_0, W20_16_0, W19_16_0, W18_16_0, W17_16_0, W16_16_0]>>,

  //  Pass all native 32-bit vectors in the low quad of the VRF argument registers
  CCIfType<[v4i8, v2i16, v2f16], CCAssignToReg<[W23_32_0, W22_32_0, W21_32_0, W20_32_0, W19_32_0, W18_32_0, W17_32_0, W16_32_0]>>,

  //  Pass all native 64-bit vectors in the low half of the VRF argument registers
  CCIfType<[v8i8, v4i16, v2i32, v4f16, v2f32], CCAssignToReg<[W23_64_0, W22_64_0, W21_64_0, W20_64_0, W19_64_0, W18_64_0, W17_64_0, W16_64_0]>>,

  //  Pass all native 128-bit vectors in the VRF argument registers
  CCIfType<[v16i8, v8i16, v4i32, v8f16, v4f32], CCAssignToReg<[W23_128_0, W22_128_0, W21_128_0, W20_128_0, W19_128_0, W18_128_0, W17_128_0, W16_128_0]>>,

  //  Pass all native 256-bit vectors in the VRF argument registers
  CCIfType<[v32i8, v16i16, v8i32, v16f16, v8f32], CCAssignToReg<[W23_256_0, W22_256_0, W21_256_0, W20_256_0, W19_256_0, W18_256_0, W17_256_0, W16_256_0]>>,

  //  Pass all native 512-bit vectors in the WVRF argument registers
  CCIfType<[v64i8, v32i16, v16i32, v32f16, v16f32], CCAssignToReg<[W23, W22, W21, W20, W19, W18, W17, W16]>>
]>;

/// CCIfSubtarget - Match if the current subtarget has a feature F.
class CCIfSubtarget<string F, CCAction A>
    : CCIf<!strconcat("static_cast<const X86Subtarget&>(State.getMachineFunction().getSubtarget()).", F),
           A>;

//  To allow multiple calling ceonventions to be selected in the future, use delegation to the actual implementation
def Shave_CallingConv  : CallingConv<[
  CCIf<"static_cast<const SHAVESubtarget&>(State.getMachineFunction().getSubtarget()).hasFeature(SHAVE::HasVRF512_Feature)", CCDelegateTo<shaveV3_CallingConv>>,

  CCIfCC<"CallingConv::X86_StdCall", CCDelegateTo<stdShave_CallingConv>>,

  CCDelegateTo<origShave_CallingConv>
]>;

def Shave_ReturnConv : CallingConv<[
  CCIf<"static_cast<const SHAVESubtarget&>(State.getMachineFunction().getSubtarget()).hasFeature(SHAVE::HasVRF512_Feature)", CCDelegateTo<shaveV3_ReturnConv>>,

  CCIfCC<"CallingConv::X86_StdCall", CCDelegateTo<stdShave_ReturnConv>>,

  CCDelegateTo<origShave_ReturnConv>
]>;

def CSR_EmptySHAVE : CalleeSavedRegs<(add)>;

def CSR_OrigSHAVE : CalleeSavedRegs<(add (sequence "I%u", 20, 29),   // FIXME: Movidius - why not all the way to 31?
                                         (sequence "V%u", 24, 31),
                                         (sequence "W%u", 24, 31))>;

def CSR_StdSHAVE  : CalleeSavedRegs<(add (sequence "I%u", 20, 29),   // FIXME: Movidius - why not all the way to 31?
                                         (sequence "V%u", 24, 31),
                                         (sequence "W%u", 24, 31))>;
