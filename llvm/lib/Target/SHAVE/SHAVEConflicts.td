//===- SHAVEConflicts.td - Scheduler conflict descriptions -*- tablegen -*-===//
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
// This file describes all hardware conflicts that can occur during
// instruction scheduling
//
//===----------------------------------------------------------------------===//

class SHAVEConflictType<string n> {
  string name = n;
}

def functionalUnitConflict    : SHAVEConflictType<"functionalUnit">;
def registerPortConflict      : SHAVEConflictType<"registerPort">;
def registerWritebackConflict : SHAVEConflictType<"registerWriteback">;
def predicationConflict       : SHAVEConflictType<"predication">;
def hardwareBug               : SHAVEConflictType<"hardwareBug">;
def invalidCycle              : SHAVEConflictType<"invalidCycle">;
def resourceConflict          : SHAVEConflictType<"resourceConflict">;
def tileClash                 : SHAVEConflictType<"tileClash">;

class SHAVEConflictCondition<string n> {
  string name = n;
}

def allSet  : SHAVEConflictCondition<"allSet">;
def anySet  : SHAVEConflictCondition<"anySet">;
def noneSet : SHAVEConflictCondition<"noneSet">;

class SHAVEConflictName<string n> {
  string name = n;
  bit hasValue = 0;
  int value;
}

class SHAVEConflictValue<string n, int v> : SHAVEConflictName<n> {
  let value = v;
  let hasValue = 1;
}

// Functional unit conflict names
def usesIAU : SHAVEConflictName<"usesIAU">;
def usesSAU : SHAVEConflictName<"usesSAU">;
def usesVAU : SHAVEConflictName<"usesVAU">;
def usesPEU : SHAVEConflictName<"usesPEU">;
def usesCMU : SHAVEConflictName<"usesCMU">;
def usesBRU : SHAVEConflictName<"usesBRU">;
def usesLSU0 : SHAVEConflictName<"usesLSU0">;
def usesLSU1 : SHAVEConflictName<"usesLSU1">;
def usesAllFUs : SHAVEConflictName<"usesAllFUs">;

// Specific resource usage conflict names
def usesVAUAdder : SHAVEConflictName<"usesVAUAdder">;
def usesSAUFloatingPointAdder : SHAVEConflictName<"usesSAUFloatingPointAdder">;
def usesSAUIntegerAdder : SHAVEConflictName<"usesSAUIntegerAdder">;
def usesVAUFloatingPointAdder : SHAVEConflictName<"usesVAUFloatingPointAdder">;
def usesVAUIntegerAdder : SHAVEConflictName<"usesVAUIntegerAdder">;

// Accumulator conflict names
def isMACZNonPipe : SHAVEConflictName<"isMACZNonPipe">;
def isMACWNonPipe : SHAVEConflictName<"isMACWNonPipe">;
def isACCWNonPipe : SHAVEConflictName<"isACCWNonPipe">;
def isFPMACNonPipe : SHAVEConflictName<"isFPMACNonPipe">;
def isFPACCNonPipe : SHAVEConflictName<"isFPACCNonPipe">;
def isMACWPipe : SHAVEConflictName<"isMACWPipe">;
def isACCWPipe : SHAVEConflictName<"isACCWPipe">;
def isPipelinedVAU_ACCorVAU_MAC : SHAVEConflictName<"isPipelinedVAU_ACCorVAU_MAC">;
def isPipelinedSAU_ACCorSAU_MAC : SHAVEConflictName<"isPipelinedSAU_ACCorSAU_MAC">;
def isPipelinedACCorMAC : SHAVEConflictName<"isPipelinedACCorMAC">;

def isDP4A : SHAVEConflictName<"isDP4A">;
def isACC : SHAVEConflictName<"isACC">;
def isMAC : SHAVEConflictName<"isMAC">;
def isACC_MAC_W : SHAVEConflictName<"isACC_MAC_W">;
def isACC_MAC_Int : SHAVEConflictName<"isACC_MAC_Int">;
def isACC_MAC_Float : SHAVEConflictName<"isACC_MAC_Float">;


// Instruction specific conflict names
def isBRU_SWIH : SHAVEConflictName<"isBRU_SWIH">;
def isSAU_DIV_i32 : SHAVEConflictName<"isSAU_DIV_i32">;
def isSAU_DIV_f32 : SHAVEConflictName<"isSAU_DIV_f32">;
def isSAU_MOD : SHAVEConflictName<"isSAU_MOD">;
def isSAU_MULorVAU_MUL : SHAVEConflictName<"isSAU_MULorVAU_MUL">;
def isSAUorVAU_ADDorSUB : SHAVEConflictName<"isSAUorVAU_ADDorSUB">;
def isCMU_CP : SHAVEConflictName<"isCMU_CP">;
def isCMU_CPIV : SHAVEConflictName<"isCMU_CPIV">;
def isCMU_MIN_MAX : SHAVEConflictName<"isCMU_MIN_MAX">;
def isCMU_Compare : SHAVEConflictName<"isCMU_Compare">;
def isCMU_LUTR : SHAVEConflictName<"isCMU_LUTR">;
def isCMU_LUT : SHAVEConflictName<"isCMU_LUT">; // Used by SHAVEAntiDependencyBreaker
def isPredicating : SHAVEConflictName<"isPredicating">; // Used by SHAVEPostSchedulingPreemption
def isWritebackPredicating : SHAVEConflictName<"isWritebackPredicating">; // Used by Post-RA scheduler

// LSU Load/Store conflict names
def isLoad : SHAVEConflictName<"isLoad">;
def isStore : SHAVEConflictName<"isStore">;
def isPseudoInstr : SHAVEConflictName<"isPseudoInstr">;
def isLDXVorSTXV : SHAVEConflictName<"isLDXVorSTXV">;
def isLoadImmediateLow : SHAVEConflictName<"isLoadImmediateLow">;
def isLoadImmediateHigh : SHAVEConflictName<"isLoadImmediateHigh">;
def isLD128Low : SHAVEConflictName<"isLD128Low">;
def isLD128High : SHAVEConflictName<"isLD128High">;
def isST128Low : SHAVEConflictName<"isST128Low">;
def isST128High : SHAVEConflictName<"isST128High">;
def isLD512Low : SHAVEConflictName<"isLD512Low">;
def isLD512High : SHAVEConflictName<"isLD512High">;
def isST512Low : SHAVEConflictName<"isST512Low">;
def isST512High : SHAVEConflictName<"isST512High">;
def isExclusiveLoad : SHAVEConflictName<"isExclusiveLoad">;
// def isExclusiveStore : SHAVEConflictName<"isExclusiveStore">;
def isLDXorSTX : SHAVEConflictName<"isLDXorSTX">;
def isPrefetch : SHAVEConflictName<"isPrefetch">;
def isLoadElement : SHAVEConflictName<"isLoadElement">;

def isACCPSeq : SHAVEConflictName<"isACCPSeq">;
def isMACPSeq : SHAVEConflictName<"isMACPSeq">;

def isAlignvecExtract : SHAVEConflictName<"isAlignvecExtract">;
def isElementCopy8 : SHAVEConflictValue<"isElementCopy", 8>; // 8-bit element copy
def isElementCopy16 : SHAVEConflictValue<"isElementCopy", 16>; // 16-bit element copy
def isElementCopy32 : SHAVEConflictValue<"isElementCopy", 32>; // 32-bit element copy
def isFloatingPointSETCC : SHAVEConflictName<"isFloatingPointSETCC">;

class SHAVEInstrConflict<list<SHAVEConflictName> names> {
  list<SHAVEConflictName> conflicts = names;
}

class SHAVEConflict<list<SHAVEConflictName> pred1, SHAVEConflictCondition pred1C,
                    list<SHAVEConflictName> pred2, SHAVEConflictCondition pred2C,
                    int minD, int maxD,
                    list<SHAVEConflictType> conflictTypes,
                    string oMin = "", string oMax = "">
{
  list<SHAVEConflictName> predicate1Flags = pred1;
  list<SHAVEConflictName> predicate2Flags = pred2;
  SHAVEConflictCondition predicate1Condition = pred1C;
  SHAVEConflictCondition predicate2Condition = pred2C;
  int minimumDelay = minD;
  int maximumDelay = maxD;
  list<SHAVEConflictType> types = conflictTypes;
  string optionMin = oMin;
  string optionMax = oMax;
}

//===----------------------------------------------------------------------===//
// Functional Unit Conflicts
//===----------------------------------------------------------------------===//

def All_FUs_in_use : SHAVEConflict<[usesAllFUs], allSet, 
                                   [usesBRU, usesCMU, usesIAU, usesPEU, usesSAU, usesVAU, usesLSU0, usesLSU1], anySet, 
                                   0, 0, [functionalUnitConflict]>;

//===----------------------------------------------------------------------===//
// Special Conflicts
//===----------------------------------------------------------------------===//

// If the instruction to insert is an LSU store, it is not permitted in the same instruction as a BRU.SWIH
def LSU_Store_BRU_SWIH_Conflict : SHAVEConflict<[isStore], anySet, [isBRU_SWIH], allSet, 0, 0, [resourceConflict]>;

// If the instruction is an LSUx.STXV or LSUx.LDXV instruction (same is true for LSUx.STXVI and
// LSUx.LDXVI, but the compiler currently does not implement these), it takes two cycles to complete,
// and the corresponding LSUx functional unit may not be used again in the next instruction
def LSU0_LDXV_STXV_Conflict : SHAVEConflict<[isLDXVorSTXV, usesLSU0], allSet, [usesLSU0], allSet, 1, 1, [functionalUnitConflict]>;
def LSU1_LDXV_STXV_Conflict : SHAVEConflict<[isLDXVorSTXV, usesLSU1], allSet, [usesLSU1], allSet, 1, 1, [functionalUnitConflict]>;

// Prevent MAC instructions from being pipelined until we can model the accumulator
def MAC_Pipeline_Conflict : SHAVEConflict<[isFPMACNonPipe], allSet, [isFPMACNonPipe], allSet, 1, 2, [resourceConflict]>;

// Prevent ACC instructions from being pipelined until we can model the accumulator
def ACC_Pipeline_Conflict : SHAVEConflict<[isFPACCNonPipe], allSet, [isFPACCNonPipe], allSet, 1, 2, [resourceConflict]>;

// From the ISA: "Uses the VAU adder 3 cycles after issue:
//          VAU.MACPZ.u16 <vrf:srcA><vrf:srcB>
//          NOP
//          NOP
//          NOP ; <== Cannot do VAU.ADD/SUB/ADIFF here"
def VAU_MACPZ_VAU_Conflict : SHAVEConflict<[isMACZNonPipe], allSet, [usesVAUAdder], allSet, 3, 3, [resourceConflict]>;

// From the ISA: "This instruction is blocking on the accumulator for 2 cycles - 
//          so no other accumulator, add, or subtract may be issued in the following two cycles"
def MACW_NonPipeline_Conflict : SHAVEConflict<[isMACWNonPipe], allSet, [usesVAUAdder], allSet, 1, 2, [resourceConflict]>;
def AACW_NonPipeline_Conflict : SHAVEConflict<[isACCWNonPipe], allSet, [usesVAUAdder], allSet, 1, 2, [resourceConflict]>;


// Don't allow SAU divf32 within 11 cycles of an ACCW???
def ACCW_Pipeline_SAU_DIV_Conflict : SHAVEConflict<[isACCWPipe], allSet, [isSAU_DIV_f32], allSet, 1, 11, [resourceConflict]>;
def SAU_DIV_ACCW_Pipeline_Conflict : SHAVEConflict<[isSAU_DIV_f32], allSet, [isACCWPipe], allSet, 1, 11, [resourceConflict]>;

// --------------------------------------------------------------------------------------
// 1.6.10.5
// --------------------------------------------------------------------------------------
// *** Most of these need to be uncommented once the multi-level worklists are 
// *** added to the Post-RA scheduler. These are causing broken acc chains 
// *** otherwise
//
// FP MAC restrictions
// -------------------
// FP MAC + 2 cycles : No adder datapath instruction can be issued by either SAU or VAU
// FP MAC + 3 cycles : No integer adder datapath instruction can be issued on opposite unit if optional register file writeback is used
def FPMAC_cycle2_Conflict : SHAVEConflict<[isMAC, isACC_MAC_Float], allSet, [usesSAUIntegerAdder, usesVAUIntegerAdder, usesSAUFloatingPointAdder, usesVAUFloatingPointAdder], anySet, 2, 2, [resourceConflict]>;
def FPMAC_cycle3_Conflict : SHAVEConflict<[isMAC, isACC_MAC_Float, isACC_MAC_W], allSet, [usesSAUIntegerAdder, usesVAUIntegerAdder], anySet, 3, 3, [resourceConflict]>;

// Integer MAC restrictions
// ------------------------
// Integer MAC + 1 cycles : No floating-point adder datapath instruction can be issued on opposite unit if optional register file writeback is used
// Integer MAC + 2 cycles : No adder datapath instruction can be issued by either SAU or VAU
def IntMAC_cycle1_Conflict : SHAVEConflict<[isMAC, isACC_MAC_Int, isACC_MAC_W], allSet, [usesSAUFloatingPointAdder, usesVAUFloatingPointAdder], anySet, 1, 1, [resourceConflict]>;
def IntMAC_cycle2_Conflict : SHAVEConflict<[isMAC, isACC_MAC_Int], allSet, [usesSAUIntegerAdder, usesVAUIntegerAdder, usesSAUFloatingPointAdder, usesVAUFloatingPointAdder], anySet, 2, 2, [resourceConflict]>;

// FP ACC restrictions
// -------------------
// FP ACC + 1 cycle : No integer adder datapath instruction can be issued on opposite unit if optional register file writeback is used.
def FPACC_cycle1_Conflict : SHAVEConflict<[isACC, isACC_MAC_Float, isACC_MAC_W], allSet, [usesSAUIntegerAdder, usesVAUIntegerAdder], anySet, 1, 1, [resourceConflict]>;

// --------------------------------------------------------------------------------------
// End of 1.6.10.5 restrictions for ACC/MAC
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// 1.8.4.25 and 1.8.4.26
// --------------------------------------------------------------------------------------
// Starting with Myriad2.3, the min and max instructions also write the CC_CMU registers
// on the writeback cycle. Hence we cannot schedule a cmu comparison instruction right
// after a min or max instruction
def CMU_MIN_MAX_CCreg_Conflict : SHAVEConflict<[isCMU_MIN_MAX], allSet, [isCMU_Compare], allSet, 1, 1, [resourceConflict]>;
// --------------------------------------------------------------------------------------
// End of 1.8.4.2X restrictions for CMU Min/Max
// --------------------------------------------------------------------------------------


// It also means that an SAU Floating-Point Adder instruction may not be followed by a
// VAU Integer adder instruction, and a VAU Floating-Point adder instruction may not be
// followed by an SAU Integer adder instruction
//
// See Bugzilla #22046 for details.
def SAU_VAU_Adder_Conflict : SHAVEConflict<[usesSAUFloatingPointAdder], allSet, [usesVAUIntegerAdder], allSet, 1, 1, [resourceConflict]>;
def VAU_SAU_Adder_Conflict : SHAVEConflict<[usesVAUFloatingPointAdder], allSet, [usesSAUIntegerAdder], allSet, 1, 1, [resourceConflict]>;

// The exlusive load instructions may not issue a load on the other LSU in the same cycle
def Exclusive_Load_Conflict : SHAVEConflict<[isLoad], anySet, [isExclusiveLoad], anySet, 0, 0, [resourceConflict]>;
