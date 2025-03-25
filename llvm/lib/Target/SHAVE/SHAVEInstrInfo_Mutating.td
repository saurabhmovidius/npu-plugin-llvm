//===-- SHAVEInstrInfo_Mutating.td - Mutating Instructions -*- tablegen -*-===//
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
// This file describes SHAVE instructions which can be mutated between
// functional units by the Post-RA Scheduler. It also includes the mappings
// between the instructions
//
//===----------------------------------------------------------------------===//

// Base class consumed by SHAVEMutationEmitter in TableGen which defines:
//   - the instruction that will be produced by a mutation
//   - the name of the function in SHAVEConflicts that performs the mutation
// The default function "createInstruction" provides a 1:1 mapping of the instructions
// just with different opcodes (plus any implicit operands related to functional unit usage)
class SHAVEMutator<Instruction instr, string func = "createInstruction"> {
  Instruction instruction = instr;
  string mutator = func;
}

// Base class consumed by SHAVEMutationEmitter in TableGen which defines an instruction mutation mapping
class SHAVEMutation<Instruction f, SHAVEMutator t> {
  Instruction from = f; // The instruction which should be mutated
  SHAVEMutator to = t; // The instruction which will be mutated to
}

class SHAVEMutation1to1<Instruction f, Instruction t>
  : SHAVEMutation<f, SHAVEMutator<t>>;

class SHAVESharedTypes<list<ValueType> tys> {
  list<ValueType> types = tys;
}

def allSHAVE512Types : SHAVESharedTypes<[v16i32, v32i16, v64i8, v16f32, v32f16, // 512-bit vectors
                                         v8i32,  v16i16, v32i8, v8f32,  v16f16, // 256-bit vectors
                                         v4i32,  v8i16,  v16i8, v4f32,  v8f16,  // 128-bit vectors
                                         v2i32,  v4i16,  v8i8,  v2f32,  v4f16,  // 64-bit vectors
                                                 v2i16,  v4i8,          v2f16,  // 32-bit vectors
                                                         v2i8                   // 16-bit vectors
                                        ]>;

// ***************************************************************************
// Combined instruction definitions and mutation mappings
// ***************************************************************************

//
// CPIV for both CMU and LSU
//

multiclass MutatingCPIV<RegisterClass destRC, RegisterClass srcRC, ValueType scalarType, ValueType vectorType, string asmstr, string index> {
  defvar name = "_CPIV_"#!cast<string>(scalarType)#"_"#!cast<string>(vectorType)#"_Myr4";

  // Define the CMU and LSU instruction records
  let Predicates = [HasVRF512_Predicate], isAsCheapAsAMove = 1 in {
    let Itinerary = II_CMU_CPIV_VRF_IRF in {
      def CMU#name : CMU_CP_insert<destRC, srcRC, scalarType, vectorType, asmstr, index>;
    }
    let Itinerary = II_LSU_CP_VRF_IRF in {
      def LSU#name : LSU_CP_insert<destRC, srcRC, asmstr, index>;
    }
  }

  // Link the records for mutating
  def : SHAVEMutation1to1<!cast<Instruction>("CMU"#name),
                          !cast<Instruction>("LSU"#name)>;
}

foreach type = allSHAVE512Types.types in {
  defm "" : MutatingCPIV<SHAVE_TypeInfo<type>.regClass, SHAVE_TypeInfo<type>.scalarRegClass,   // Register classes
                         SHAVE_TypeInfo<type>.scalarType, type,                                // Types
                         !strconcat(".", !cast<string>(SHAVE_TypeInfo<type>.scalarType.Size)), // Assembly string, e.g. ".CP.32" for i32 into v16i32
                         !if(!eq(SHAVE_TypeInfo<type>.scalarType.Size, 32), "", ".0")>;        // Lane string
}

//
// CPVI for both CMU and LSU
//

multiclass MutatingCPVI<RegisterClass destRC, RegisterClass srcRC, ValueType scalarType, ValueType vectorType, string asmstr, string index> {
  defvar name = "_CPVI_"#!cast<string>(scalarType)#"_"#!cast<string>(vectorType)#"_Myr4";

  // Define the CMU and LSU instruction records
  let Predicates = [HasVRF512_Predicate], isAsCheapAsAMove = 1 in {
    let Itinerary = II_CMU_CPVI_IRF_VRF in {
      def CMU#name : CMU_CPVI<destRC, srcRC, scalarType, vectorType, asmstr, index>;
    }
    let Itinerary = II_LSU_CP_IRF_VRF in {
      def LSU#name : LSU_CP_irf_vrf<destRC, srcRC, asmstr, index>;
    }
  }

  // Link the records for mutating
  def : SHAVEMutation1to1<!cast<Instruction>("CMU"#name),
                          !cast<Instruction>("LSU"#name)>;
}

foreach type = allSHAVE512Types.types in {
  // Construct the machine instruction name, e.g. "CMU_CPVI_i32_v16i32_Myr4" for v16i32
  defm "" : MutatingCPVI<SHAVE_TypeInfo<type>.scalarRegClass, SHAVE_TypeInfo<type>.regClass,      // Register classes
                         SHAVE_TypeInfo<type>.scalarType, type,                                   // Types
                         !strconcat(".CP.", !cast<string>(SHAVE_TypeInfo<type>.scalarType.Size)), // Assembly string, e.g. ".CP.32" for i32 from v16i32
                         !if(!eq(SHAVE_TypeInfo<type>.scalarType.Size, 32), "", ".0")>;           // Lane string
}

//
// CPVV (element to element) for both CMU and LSU
//

multiclass MutatingCPVV_elt<RegisterClass RC, ValueType vectorType, ValueType scalarType, string asmstr> {
  // Construct the machine instruction name, e.g. "<FU>_CPVV_element_v16i32" for v16i32
  defvar name = "_CPVV_element_"#!cast<string>(vectorType);

  // Define the CMU and LSU instruction records
  let Predicates = [HasVRF512_Predicate], isAsCheapAsAMove = 1 in {
    let Itinerary = II_CMU_CPVV_VRF in {
      def CMU#name : CMU_CPVV_element<RC, vectorType, scalarType, asmstr>;
    }
    let Itinerary = II_LSU_CP_VRF_VRF in {
      def LSU#name : LSU_CPVV_element<RC, asmstr>;
    }
  }

  // Link the records for mutating
  def : SHAVEMutation1to1<!cast<Instruction>("CMU"#name),
                          !cast<Instruction>("LSU"#name)>;
}

// Element to element copies
foreach type = allSHAVE512Types.types in {
  defm "" : MutatingCPVV_elt<SHAVE_TypeInfo<type>.regClass,                                            // Register class
                             type, SHAVE_TypeInfo<type>.scalarType,                                    // Types
                             !strconcat(".CP.", !cast<string>(SHAVE_TypeInfo<type>.scalarType.Size))>; // Assembly string, e.g. ".CP.32" for i32 from/to v16i32
}

// ***************************************************************************
// Mutation mappings for instructions defined previously
// ***************************************************************************

// Common class for all Copy to Bitwise-OR mutations
class SHAVEOrCopyMutation<Instruction cmuCopy, Instruction orInstruction> :
  SHAVEMutation<cmuCopy, SHAVEMutator<orInstruction, "createOrCopyInstruction">>;

// CMU.CPVV -> VAU.OR
// VRF512
def : SHAVEOrCopyMutation<CMU_CPVV_512_Myr4, VAU_OR_bitwise_512_v16i32_Myr4>;
def : SHAVEOrCopyMutation<CMU_CPVV_256_Myr4, VAU_OR_bitwise_256_v8i32_Myr4>;
def : SHAVEOrCopyMutation<CMU_CPVV_128_Myr4, VAU_OR_bitwise_128_v4i32_Myr4>;
def : SHAVEOrCopyMutation<CMU_CPVV_64_Myr4,  VAU_OR_bitwise_64_v2i32_Myr4>;
// VRF128
def : SHAVEOrCopyMutation<CMU_CPVV, VAU_OR_32>;
def : SHAVEOrCopyMutation<CMU_CPVV_64, VAU_OR_v2i32>;

// CMU.CPVV -> LSU.CPVV for VRF128
def : SHAVEMutation1to1<CMU_CPVV,        LSU_CP_vrf_vrf>;
def : SHAVEMutation1to1<CMU_CPVV_64,     LSU_CP_vrf64_vrf64>;
def : SHAVEMutation1to1<CMU_CPVV_64_128, LSU_CP_vrf_vrf64>;
def : SHAVEMutation1to1<CMU_CPVV_32_128, LSU_CP_vrf_vrf32>;
def : SHAVEMutation1to1<CMU_CPVV_16_128, LSU_CP_vrf_vrf16>;

// CMU.CPIV -> LSU.CPIV for VRF128
def : SHAVEMutation1to1<CMU_CPIV_x32,       LSU_CP_vrf_irf>;
def : SHAVEMutation1to1<CMU_CPIV_x32_v4i32, LSU_CP_vrf_irf>;
def : SHAVEMutation1to1<CMU_CPIV_x32_v4f32, LSU_CP_vrf_irf>;
def : SHAVEMutation1to1<CMU_CPIV_x32_v2i32, LSU_CP_vrf64_irf>;
def : SHAVEMutation1to1<CMU_CPIV_x32_v2f32, LSU_CP_vrf64_irf>;
def : SHAVEMutation1to1<CMU_CPIV_x32_v4i8,  LSU_CP_vrf32_irf>;
def : SHAVEMutation1to1<CMU_CPIV_x32_v2i16, LSU_CP_vrf32_irf>;

def : SHAVEMutation1to1<CMU_CPIV_x16_v8f16_l_Myr2v3, LSU_CP16_vrf_irf0>;
def : SHAVEMutation1to1<CMU_CPIV_x16_v8i16_l_Myr2v3, LSU_CP16_vrf_irf0>;

// CMU.CPVI -> LSU.CPVI for VRF128
def : SHAVEMutation1to1<CMU_CPVI_x32,       LSU_CP_irf_vrf>;
def : SHAVEMutation1to1<CMU_CPVI_f32_v2f32, LSU_CP_irf_vrf64>;
def : SHAVEMutation1to1<CMU_CPVI_x32_l,     LSU_CP_irf_vrf32>;

def : SHAVEMutation1to1<CMU_CPVI_x16_l, LSU_CP16_irf0_vrf>;
def : SHAVEMutation1to1<CMU_CPVI_f16_l, LSU_CP16_irf0_vrf>;

// CMU.CPED -> LSU.CPED for VRF128
def : SHAVEMutation1to1<CMU_CPED_32_v4f32, LSU_CPED_vrf_vrf_x32>;
def : SHAVEMutation1to1<CMU_CPED_32_v4i32, LSU_CPED_vrf_vrf_x32>;

def : SHAVEMutation1to1<CMU_CPED_16_v8f16, LSU_CPED_vrf_vrf_x16>;
def : SHAVEMutation1to1<CMU_CPED_16_v8i16, LSU_CPED_vrf_vrf_x16>;

def : SHAVEMutation1to1<CMU_CPED_8, LSU_CPED_vrf_vrf_x8>;

// CMU.CPII -> LSU.CPII, IAU.OR, SAU.OR
multiclass SHAVECPIIMutation<Instruction cmuCopy, Instruction lsuCopy, Instruction iauOr, Instruction sauOr> {
  def : SHAVEMutation1to1<cmuCopy, lsuCopy>;
  def : SHAVEOrCopyMutation<cmuCopy, iauOr>;
  def : SHAVEOrCopyMutation<cmuCopy, sauOr>;
}

defm : SHAVECPIIMutation<CMU_CPII_32_Raw, LSU_CP_irf_irf, IAU_OR_32, SAU_OR_i32>;
defm : SHAVECPIIMutation<CMU_CPII_16_Raw, LSU_CP_irf16_irf16, IAU_OR_16, SAU_OR_i16>;
defm : SHAVECPIIMutation<CMU_CPII_8_Raw, LSU_CP_irf8_irf8, IAU_OR_8, SAU_OR_i8>;

// CMU.CPZI -> IAU.XOR, SAU.XOR, LSU.LDIL 0
def : SHAVEMutation<CMU_CPZI, SHAVEMutator<IAU_XOR_32,  "createCPZIMutation">>;
def : SHAVEMutation<CMU_CPZI, SHAVEMutator<SAU_XOR_i32, "createCPZIMutation">>;
def : SHAVEMutation<CMU_CPZI, SHAVEMutator<LSU_LDIL,    "createCPZIMutation">>;
