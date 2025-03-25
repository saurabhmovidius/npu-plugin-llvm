//===-- SHAVEv3RegisterInfo.td - VAU Instructions -----------*- tablegen -*-===//
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
// This file describes the register set for the SHAVEv3 Vector
// Arithmetic Unit (VAU)
//
//===----------------------------------------------------------------------===//

let Namespace = "SHAVE" in {
  def wsub_256_0 : SubRegIndex<256>;
  def wsub_256_1 : SubRegIndex<256, 256>;

  foreach subN = 0 - 3 in
    def wsub_128_#subN : SubRegIndex<128, !mul(128, subN)>;

  foreach subN = 0 - 7 in
    def wsub_64_#subN : SubRegIndex<64, !mul(64, subN)>;

  foreach subN = 0 - 15 in
    def wsub_32_#subN : SubRegIndex<32, !mul(32, subN)>;

  foreach subN = 0 - 31 in
    def wsub_16_#subN : SubRegIndex<16, !mul(16, subN)>;
}

// Dwarf registers are '64' thru '95'
foreach nVRF = 0 - 31 in {
  // WVRF16.
  foreach idx = 0 - 31 in
    def W#nVRF#_16_#idx : SHAVEReg<"v"#nVRF>,
                          DwarfRegNum<[!add(64, nVRF)]>;

  // WVRF32.
  foreach idx = 0 - 15 in {
    let SubRegIndices = [!cast<SubRegIndex>("wsub_16_"#!mul(idx, 2)), !cast<SubRegIndex>("wsub_16_"#!add(1, !mul(idx, 2)))], CoveredBySubRegs = 1 in
      def W#nVRF#_32_#idx : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("W"#nVRF#"_16_"#!mul(idx, 2)), !cast<Register>("W"#nVRF#"_16_"#!add(!mul(idx, 2), 1))]>,
                            DwarfRegAlias<!cast<Register>("W"#nVRF#"_16_"#idx)>;
  }
  // WVRF64.
  foreach idx = 0 - 7 in
    let SubRegIndices = [!cast<SubRegIndex>("wsub_32_"#!mul(idx, 2)), !cast<SubRegIndex>("wsub_32_"#!add(1, !mul(idx, 2)))], CoveredBySubRegs = 1 in
      def W#nVRF#_64_#idx : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("W"#nVRF#"_32_"#!mul(idx, 2)), !cast<Register>("W"#nVRF#"_32_"#!add(!mul(idx, 2), 1))]>,
                            DwarfRegAlias<!cast<Register>("W"#nVRF#"_16_"#idx)>;

  // WVRF128.
  foreach idx = 0 - 3 in
    let SubRegIndices = [!cast<SubRegIndex>("wsub_64_"#!mul(idx, 2)), !cast<SubRegIndex>("wsub_64_"#!add(1, !mul(idx, 2)))], CoveredBySubRegs = 1 in
      def W#nVRF#_128_#idx : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("W"#nVRF#"_64_"#!mul(idx, 2)), !cast<Register>("W"#nVRF#"_64_"#!add(!mul(idx, 2), 1))]>,
                             DwarfRegAlias<!cast<Register>("W"#nVRF#"_16_"#idx)>;

  // WVRF256.
  let SubRegIndices = [wsub_128_0, wsub_128_1], CoveredBySubRegs = 1 in
    def W#nVRF#_256_0 : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("W"#nVRF#"_128_0"), !cast<Register>("W"#nVRF#"_128_1")]>,
                        DwarfRegAlias<!cast<Register>("W"#nVRF#"_16_0")>;
  let SubRegIndices = [wsub_128_2, wsub_128_3], CoveredBySubRegs = 1 in
    def W#nVRF#_256_1 : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("W"#nVRF#"_128_2"), !cast<Register>("W"#nVRF#"_128_3")]>,
                        DwarfRegAlias<!cast<Register>("W"#nVRF#"_16_1")>;

  // WVRF512.
  let SubRegIndices = [wsub_256_0, wsub_256_1], CoveredBySubRegs = 1 in
      def W#nVRF : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("W"#nVRF#"_256_0"), !cast<Register>("W"#nVRF#"_256_1")]>,
                   DwarfRegAlias<!cast<Register>("W"#nVRF#"_16_0")>;
}

foreach nClass = 0 - 31 in {
  def WVRF16_#nClass : RegisterClass<"SHAVE", [v2i8], 16,
                                     (add (sequence "W%u_16_"#nClass, 15, 0),  // scratch registers
                                          (sequence "W%u_16_"#nClass, 16, 23), // scratch + calling convention registers
                                          (sequence "W%u_16_"#nClass, 24, 31)  // preserved registers
                                     )
                                    >;
}

foreach nClass = 0 - 15 in {
  def WVRF32_#nClass : RegisterClass<"SHAVE", [v4i8, v2i16, v2f16, i32, f32], 32,
                                     (add (sequence "W%u_32_"#nClass, 15, 0),  // scratch registers
                                          (sequence "W%u_32_"#nClass, 16, 23), // scratch + calling convention registers
                                          (sequence "W%u_32_"#nClass, 24, 31)  // preserved registers
                                     )
                                    >;
}

foreach nClass = 0 - 7 in {
  def WVRF64_#nClass : RegisterClass<"SHAVE", [v8i8, v4i16, v4f16, v2i32, v2f32], 64,
                                     (add (sequence "W%u_64_"#nClass, 15, 0),  // scratch registers
                                          (sequence "W%u_64_"#nClass, 16, 23), // scratch + calling convention registers
                                          (sequence "W%u_64_"#nClass, 24, 31)  // preserved registers
                                     )
                                    >;
}

foreach nClass = 0 - 3 in {
  def WVRF128_#nClass : RegisterClass<"SHAVE", [v16i8, v8i16, v8f16, v4i32, v4f32], 128,
                                      (add (sequence "W%u_128_"#nClass, 15, 0),  // scratch registers
                                           (sequence "W%u_128_"#nClass, 16, 23), // scratch + calling convention registers
                                           (sequence "W%u_128_"#nClass, 24, 31)  // preserved registers
                                      )
                                     >;
}

foreach nClass = 0 - 1 in {
  def WVRF256_#nClass : RegisterClass<"SHAVE", [v32i8, v16i16, v16f16, v8i32, v8f32], 256,
                                      (add (sequence "W%u_256_"#nClass, 15, 0),  // scratch registers
                                           (sequence "W%u_256_"#nClass, 16, 23), // scratch + calling convention registers
                                           (sequence "W%u_256_"#nClass, 24, 31)  // preserved registers
                                      )
                                     >;
}

def WVRF512 : RegisterClass<"SHAVE", [v64i8, v32i16, v16i32, v16f32, v32f16], 512,
            (add
             // scratch registers
             W15, W14, W13, W12, W11, W10, W9, W8, W7, W6, W5, W4, W3, W2, W1, W0, 
             // scratch + calling convention registers
             W16, W17, W18, W19, W20, W21, W22, W23,
             // preserved registers
             W24, W25, W26, W27, W28, W29, W30, W31
             )>;
