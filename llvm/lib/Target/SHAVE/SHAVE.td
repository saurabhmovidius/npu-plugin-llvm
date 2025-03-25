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
// File       :  SHAVE.td
// Description:  SHAVE target description
// ---------------------------------------------------------------------------


//===----------------------------------------------------------------------===//
// Target-independent interfaces which we are implementing
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// NPU architecture feature definitions
//===----------------------------------------------------------------------===//

multiclass SHAVESubtargetFeature<string name, string description> {
  // Rather than having a long list of boolean member variables, it is much cleaner
  // to use the generated bitset directly and use the generated enum of Features
  // to index it. This means that when we add a new boolean Feature, we only need
  // to add it in one place, and it becomes automatically available through the
  // pre-existing Subtarget API
  //
  // When the Attribute and Value arguments are set to "", the feature will only be accessible as a boolean
  // through subtargetFeatures and the function "hasFeature"
  def _Feature       : SubtargetFeature<name, "", "", description>;

  def _Predicate     : Predicate<!strconcat("Subtarget.hasFeature(SHAVE::", name, "_Feature)")>;
  def _Not_Predicate : Predicate<!strconcat("!Subtarget.hasFeature(SHAVE::", name, "_Feature)")>;
}

defm HasVRF128               : SHAVESubtargetFeature<"HasVRF128",               "Target has 128-bit Vector Register File (VRF)">;
defm HasVRF512               : SHAVESubtargetFeature<"HasVRF512",               "Target has 512-bit Vector Register File (VRF)">;
defm HasIRF64_LSU_CMU_Instrs : SHAVESubtargetFeature<"HasIRF64_LSU_CMU_Instrs", "Target has native 64-bit IRF LSU and CMU instructions">;
defm HasF8Float              : SHAVESubtargetFeature<"HasF8Float",              "Target has BF8 and HF8 Floating-point support">;
defm HasB16Float             : SHAVESubtargetFeature<"HasB16Float",             "Target has B16 Floating-point support">;
defm HasU8FFloat             : SHAVESubtargetFeature<"HasU8FFloat",             "Target has U8F Floating-point support">;
defm HasSharedResources      : SHAVESubtargetFeature<"HasSharedResources",      "Target has shared resources pertinent to instruction scheduling">;
defm HasVSZM                 : SHAVESubtargetFeature<"HasVSZM",                 "Target has CMU.VSZM instruction">;
defm HasPCXC                 : SHAVESubtargetFeature<"HasPCXC",                 "Target has PEU.PCXC instruction">;
defm HasCodeProtection       : SHAVESubtargetFeature<"HasCodeProtection",       "Target has Code Protection for preemption support">;
defm HasSUMFX512             : SHAVESubtargetFeature<"HasSUMFX512",             "Target supports full length SAU.SUMFX">;
defm HasPERM                 : SHAVESubtargetFeature<"HasPERM",                 "Target has CMU.PERM and CMU.PERMB instructions">;
defm HasDP4                  : SHAVESubtargetFeature<"HasDP4",                  "Target has DP4* instructions on the SAU and VAU">;
defm HasLDOSTOMode           : SHAVESubtargetFeature<"HasLDOSTOMode",           "Target LDO/STO instructions have shifted mode operands for offset">;
defm HasLegacyPreemptInstrs  : SHAVESubtargetFeature<"HasLegacyPreemptInstrs",  "Target has CMU.CMTI.C16 and ISR_SA for preemption support">;
defm HasTranscendentals      : SHAVESubtargetFeature<"HasTranscendentals",      "Target has 16-bit floating-point transcendentals instructions in the SAU and VAU">;
defm HasLegacySHAVEIDs       : SHAVESubtargetFeature<"HasLegacySHAVEIDs",       "Target has linear SHAVE IDs stored in TRF.P_SVID">;
defm HasPC1COrdered          : SHAVESubtargetFeature<"HasPC1COrdered",          "Target has PEU.PC1C with O (ordered) mask">;

// If the ITI link arbitration is done in the CMX tile, the new arbitration
// logic requires the stores to be distributed evenly across the LSUs to avoid
// stalls
defm HasITIArbitrationInCMXTile : SHAVESubtargetFeature<"HasITIArbitrationInCMXTile",
          "Additional inter-tile write arbitration logic present in CMX tiles, which necessitates balancing of store instructions across the two LSUs">;

// Range of BRU.BRA can differ between chip versions
def BRU_BRA_Range_8K  : SubtargetFeature<"BRU_BRA_Range_8K",  "maxBRARange", "8191",  "BRU.BRA has an 8KB maximum range">;
def BRU_BRA_Range_32K : SubtargetFeature<"BRU_BRA_Range_32K", "maxBRARange", "32767", "BRU.BRA has an 32KB maximum range">;

// Load lantency increased with NPU4 from 6-cycles to 8-cycles
def Load_Latency_6_Cycles  : SubtargetFeature<"Load_Latency_6_Cycles", "minLoadLatency", "6", "Load latency is at least 6-cycles">;
def Load_Latency_8_Cycles  : SubtargetFeature<"Load_Latency_8_Cycles", "minLoadLatency", "8", "Load latency is at least 8-cycles">;

// CMX SRAM cut size and number of cuts
def CMX_Cut_8_Bytes   : SubtargetFeature<"CMX_Cut_8_Bytes",  "cmxCutSize", "8",  "CMX SRAM Cut Size is 8-bytes">;
def CMX_Cut_16_Bytes  : SubtargetFeature<"CMX_Cut_16_Bytes", "cmxCutSize", "16", "CMX SRAM Cut Size is 16-bytes">;
def CMX_Cut_32_Bytes  : SubtargetFeature<"CMX_Cut_32_Bytes", "cmxCutSize", "32", "CMX SRAM Cut Size is 32-bytes">;

def CMX_2_Cuts  : SubtargetFeature<"CMX_2_Cuts",  "cmxNumberOfCuts", "2",  "CMX has 2 SRAM Cuts">;
def CMX_32_Cuts : SubtargetFeature<"CMX_32_Cuts", "cmxNumberOfCuts", "32", "CMX has 32 SRAM Cuts">;

// Number of bits available to constant offsets in LDO and STO instructions
def LDOSTO_13_bit_offset : SubtargetFeature<"LDOSTO_13_bit_offset", "LDOSTO_OffsetBits", "13", "LDO/STO instructions have 13-bit constant offsets">;
def LDOSTO_15_bit_offset : SubtargetFeature<"LDOSTO_15_bit_offset", "LDOSTO_OffsetBits", "15", "LDO/STO instructions have 15-bit constant offsets">;

// Number of SHAVEs per NPU tile
// This is used for deciding how to extract the SHAVE id from the P_SVID (upto NPU2.7) or P_ID (NPU4 onwards) register
def Has_2_SHAVEs_per_tile : SubtargetFeature<"Has_2_SHAVEs_per_tile", "numShavesPerTile", "2", "NPU version with 2 SHAVEs per tile">;
def Has_4_SHAVEs_per_tile : SubtargetFeature<"Has_4_SHAVEs_per_tile", "numShavesPerTile", "4", "NPU version with 4 SHAVEs per tile">;

// Predicate to inspect the command-line option for enabling/disabling LDX and STX support
def EnableLDXSTX   : Predicate<"Subtarget.useLDXSTX()">;

//===----------------------------------------------------------------------===//
// Scheduler conflict descriptions
//===----------------------------------------------------------------------===//

include "SHAVEConflicts.td"


//===----------------------------------------------------------------------===//
// Register and operand information
//===----------------------------------------------------------------------===//

include "SHAVERegisterInfo.td"
include "SHAVEOperands.td"
include "SHAVESchedule.td"


//===----------------------------------------------------------------------===//
// Calling conventions:
//===----------------------------------------------------------------------===//

include "SHAVECallingConv.td"


//===----------------------------------------------------------------------===//
// Instruction formats, instructions
//===----------------------------------------------------------------------===//

include "SHAVEInstrFormats.td"
include "SHAVEInstrInfo.td"


//===----------------------------------------------------------------------===//
// SHAVE Instruction Info:
//===----------------------------------------------------------------------===//

def SHAVEInstrInfo : InstrInfo {
  let isLittleEndianEncoding = 1;
}

//===----------------------------------------------------------------------===//
// SHAVE processors supported.
//===----------------------------------------------------------------------===//

foreach myriad2v3_soc_variant = [ "ma2080", "ma2085", "ma2480", "ma2485", "ma2x8x", "2xxx", "myriad2.3" ] in {
  def : ProcessorModel<myriad2v3_soc_variant, SHAVEModel, [HasIRF64_LSU_CMU_Instrs_Feature,
                                                           HasVSZM_Feature,
                                                           HasPCXC_Feature,
                                                           HasVRF128_Feature,
                                                           HasLegacySHAVEIDs_Feature,
                                                           HasLegacyPreemptInstrs_Feature,
                                                           HasU8FFloat_Feature,
                                                           LDOSTO_15_bit_offset,
                                                           BRU_BRA_Range_32K,
                                                           Load_Latency_6_Cycles,
                                                           CMX_Cut_8_Bytes,
                                                           CMX_2_Cuts
                                                          ]>;
}

def NPU20Base {
  list<SubtargetFeature> Features = [HasVSZM_Feature,
                                     HasPCXC_Feature,
                                     HasIRF64_LSU_CMU_Instrs_Feature,
                                     HasLegacySHAVEIDs_Feature,
                                     HasU8FFloat_Feature,
                                     LDOSTO_15_bit_offset,
                                     BRU_BRA_Range_32K,
                                     Load_Latency_6_Cycles,
                                     CMX_Cut_8_Bytes,
                                     CMX_2_Cuts
                                    ];
}

// Define the processor models for NPU2.0 (KeemBay) variants
foreach ip_soc_variant = [ "ma2x9x", "3000xx", "3010xx", "3010A0", "3010B0",
                            "ma2090", "ma2095", "ma2490", "ma2495", "vpu20", "npu20",  "3100xx",
                            "myriad2.4"] in {
  def : ProcessorModel<ip_soc_variant,       SHAVEModel, !listconcat(NPU20Base.Features, [HasVRF128_Feature])>;
  def : ProcessorModel<ip_soc_variant#"-nn", SHAVEModel, NPU20Base.Features>;
}

def NPU27Base {
  list<SubtargetFeature> Features = [HasVSZM_Feature,
                                     HasPCXC_Feature,
                                     HasIRF64_LSU_CMU_Instrs_Feature,
                                     HasB16Float_Feature,
                                     HasDP4_Feature,
                                     HasLegacyPreemptInstrs_Feature,
                                     HasTranscendentals_Feature,
                                     HasLegacySHAVEIDs_Feature,
                                     HasPC1COrdered_Feature,
                                     HasU8FFloat_Feature,
                                     LDOSTO_15_bit_offset,
                                     BRU_BRA_Range_32K,
                                     Load_Latency_6_Cycles,
                                     CMX_Cut_16_Bytes,
                                     CMX_32_Cuts,
                                     Has_2_SHAVEs_per_tile
                                    ];
}

// Define the processor models for NPU2.7 variants
foreach ip_soc_variant = [ "3600xx", "3700xx", "3710xx", "3720xx", "3730xx", "3740xx"] in {
  def : ProcessorModel<ip_soc_variant,       SHAVEModel, !listconcat(NPU27Base.Features, [HasVRF128_Feature])>;
  def : ProcessorModel<ip_soc_variant#"-nn", SHAVEModel, NPU27Base.Features>;
}

def SHAVE512Base {
  // List of features common to all SHAVE512 versions (NPU4+)
  list<SubtargetFeature> Features = [HasB16Float_Feature,
                                     HasVRF512_Feature,
                                     HasSharedResources_Feature,
                                     HasLDOSTOMode_Feature,
                                     HasTranscendentals_Feature,
                                     HasDP4_Feature,
                                     HasPC1COrdered_Feature,
                                     LDOSTO_13_bit_offset,
                                     BRU_BRA_Range_32K,
                                     Load_Latency_8_Cycles,
                                     CMX_Cut_32_Bytes,
                                     CMX_32_Cuts
                                    ];
}

foreach npu4_soc_variant = [ "4000xx", "4010xx", "myriad4.0" ] in {
  def : ProcessorModel<npu4_soc_variant, SHAVEModel, !listconcat(SHAVE512Base.Features, [HasU8FFloat_Feature,
                                                                                         Has_2_SHAVEs_per_tile])>;
}

foreach npu5_soc_variant = [ "5000xx", "5010xx" ] in {
  def : ProcessorModel<npu5_soc_variant, SHAVEModel, !listconcat(SHAVE512Base.Features, [HasF8Float_Feature,
                                                                                         HasU8FFloat_Feature,
                                                                                         HasCodeProtection_Feature,
                                                                                         Has_2_SHAVEs_per_tile
                                                                                        ])>;
}

foreach npu6_soc_variant = [ "6000xx", "6010xx" ] in {
  def : ProcessorModel<npu6_soc_variant, SHAVEModel, !listconcat(SHAVE512Base.Features, [HasF8Float_Feature,
                                                                                         HasCodeProtection_Feature,
                                                                                         HasPERM_Feature,
                                                                                         Has_4_SHAVEs_per_tile,
                                                                                         HasSUMFX512_Feature,
                                                                                         HasITIArbitrationInCMXTile_Feature
                                                                                        ])>;
}

foreach npu7_soc_variant = [ "7000xx", "7010xx" ] in {
  def : ProcessorModel<npu7_soc_variant, SHAVEModel, !listconcat(SHAVE512Base.Features, [HasF8Float_Feature,
                                                                                         HasCodeProtection_Feature,
                                                                                         HasPERM_Feature,
                                                                                         Has_4_SHAVEs_per_tile,
                                                                                         HasSUMFX512_Feature,
                                                                                         HasITIArbitrationInCMXTile_Feature
                                                                                        ])>;
}

//===----------------------------------------------------------------------===//
// Declare the target which we are implementing
//===----------------------------------------------------------------------===//

def SHAVE : Target {
  // The Instruction Info
  let InstructionSet = SHAVEInstrInfo;
}
