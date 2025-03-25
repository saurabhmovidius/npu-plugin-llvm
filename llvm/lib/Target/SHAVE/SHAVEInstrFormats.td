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
// File       : SHAVEInstrFormats.td
// Description: Instruction classes and formats
// ---------------------------------------------------------------------------

// This specifies the type of this ACC/MAC instruction
class ACC_MAC_InsnTypeClass<bits<2> val> {
   bits<2> Value = val;
}

def ACC_MAC_Invalid : ACC_MAC_InsnTypeClass <0>;
def ACC_MAC_PN      : ACC_MAC_InsnTypeClass <1>;
def ACC_MAC_W       : ACC_MAC_InsnTypeClass <2>;
def ACC_MAC_Z       : ACC_MAC_InsnTypeClass<3>;

class SHAVEInstr<dag OOL, dag IOL, string asmstr, SHAVEFuncUnit unit, list<SHAVEConflictName> confs = []>  : Instruction, Sched<[]>
{
  field bits<32> Inst;

  // list of execution units that accept this instruction
  field SHAVEFuncUnit Unit = unit;

  // list of conflicts that apply to this instruction
  list<SHAVEConflictName> conflicts = [];

  let Namespace = "SHAVE";
  let OutOperandList = OOL;
  let InOperandList = IOL;
  let AsmString = asmstr;

  bits<4> FunctionalUnit = unit.Id;
 
  // Bits 0-3 are reserved for ACC instruction description as below:

  // {0..1}:
  //         01 - ACCCN/ACCP/MACN/MACP
  //         10 - ACCW/MACW
  //         11 - ACCZ/MACZ
  // {2}: True if this is a pipelined instruction
  // {3}: True if this is a MAC isntruction
  bit IsMACInsnBit = 0;
  bit IsPipelinedACCBit = 0;
  ACC_MAC_InsnTypeClass ACC_MAC_InsnType = ACC_MAC_Invalid;
  bits<2> ACC_MAC_InsnTypeBits = ACC_MAC_InsnType.Value;

  let TSFlags{7-4} = FunctionalUnit;
  let TSFlags{3} = IsMACInsnBit;
  let TSFlags{2} = IsPipelinedACCBit;
  let TSFlags{1-0} = ACC_MAC_InsnTypeBits;
  
  // Set default to 0 so that the !if statements below can be evaluated
  // These are overridden by individual instruction definitions
  let mayLoad = 0;
  let mayStore = 0;

  list<SHAVEConflictName> noConflict = [];
  let conflicts = !listconcat(confs, 
                              !if(mayLoad, [isLoad], noConflict),
                              !if(mayStore, [isStore], noConflict),
							  !if(isPseudo, [isPseudoInstr], noConflict));
}

// Integer instructions
//   With predication
class SHAVE_IAUInstr<dag OOL, dag IOL, string asmstr, list<dag> pattern = [], list<SHAVEConflictName> confs = []> :
	SHAVEInstr<OOL, !con(IOL, (ins pred:$p)), !strconcat(IAU.Name, asmstr), IAU, !listconcat(confs, [usesIAU])>
{
  let isPredicable = 1;
  let Pattern = pattern;
}
//   Without predication (see Bugzilla #22761 and #22764)
class SHAVE_IAUInstr_NoPred<dag OOL, dag IOL, string asmstr, list<dag> pattern, list<SHAVEConflictName> confs = []> :
	SHAVEInstr<OOL, IOL, !strconcat(IAU.Name, asmstr), IAU, !listconcat(confs, [usesIAU])>
{
  let Pattern = pattern;
}

// Load/Store instructions
class SHAVE_LSUInstr<dag OOL, dag IOL, string asmstr, list<dag> pattern = [], list<SHAVEConflictName> confs = []> :
  SHAVEInstr<OOL, !con(IOL, (ins pred:$p, lsu_id:$funit)), !strconcat("$funit", asmstr), LSU1, !listconcat(confs, [usesLSU1])>
{
  let isPredicable = 1;
  let Pattern = pattern;
}

class SHAVE_LSUInstr_NoPredicate<dag OOL, dag IOL, string asmstr, list<dag> pattern = [], list<SHAVEConflictName> confs = []> :
  SHAVEInstr<OOL, !con(IOL, (ins lsu_id:$funit)), !strconcat("$funit", asmstr), LSU1, !listconcat(confs, [usesLSU1])>
{
  let isPredicable = 0;
  let Pattern = pattern;
}

// Vector instructions
class SHAVE_VAUInstr<dag OOL, dag IOL, string asmstr, list<dag> pattern = [], list<SHAVEConflictName> confs = []> :
        SHAVEInstr<OOL, !con(IOL, (ins pred:$p)), !strconcat("VAU", asmstr), VAU, !listconcat(confs, [usesVAU])>
{
  let isPredicable = 1;
  let Pattern = pattern;
}

// Scalar instructions
class SHAVE_SAUInstr<dag OOL, dag IOL, string asmstr, list<dag> pattern = [], list<SHAVEConflictName> confs = []> :
	SHAVEInstr<OOL, !con(IOL, (ins pred:$p)), !strconcat("SAU", asmstr), SAU, !listconcat(confs, [usesSAU])>
{
  let isPredicable = 1;
  let Pattern = pattern;
}

// Predication instructions
class SHAVE_PEUInstr<dag OOL, dag IOL, string asmstr, list<dag> pattern = [], list<SHAVEConflictName> confs = []> :
	SHAVEInstr<OOL, IOL, !strconcat("PEU", asmstr), PEU, !listconcat(confs, [usesPEU])>
{
  let Pattern = pattern;
}

// Compare/Move instructions
// Some of these can be mutated to LSU instruction like LSU.CP.
// The reason we need to 'reserve' the functional unit operand is that the
// anti-dependency breaker can cause crashes when MI operand lists are re-allocated.
class SHAVE_CMUInstr<dag OOL, dag IOL, string asmstr, list<dag> pattern = [], list<SHAVEConflictName> confs = []> :
    SHAVEInstr<OOL, !con(IOL, (ins pred:$p, cmu_id:$funit)), !strconcat("CMU", asmstr), CMU, !listconcat(confs, [usesCMU])>
{
  let isPredicable = 1;
  let Pattern = pattern;
}

// Branch instructions
class SHAVE_BRUInstr<dag OOL, dag IOL, string asmstr, list<dag> pattern = [], list<SHAVEConflictName> confs = []> :
	SHAVEInstr<OOL, !con(IOL, (ins pred:$p)), !strconcat("BRU", asmstr), BRU, !listconcat(confs, [usesBRU])>
{
  let isPredicable = 1;
  let Pattern = pattern;
}

// Pseudo instructions
class SHAVE_Pseudo<dag OOL, dag IOL, string asmstr, list<dag> pattern = [], list<SHAVEConflictName> confs = []>
    : SHAVEInstr<OOL, IOL, asmstr, PseudoUnit, confs>
{
  let Pattern = pattern;
  let isPseudo = 1;
  let hasNoSchedulingInfo = 1;
}

class SHAVE_Phony : SHAVE_Pseudo<(outs), (ins), "PHONY">;   // Used to dummy unwanted instructions that are generated from 'multiclass' templates

class SHAVE_Predicable_Pseudo<dag OOL, dag IOL, string asmstr, list<dag> pattern = []>
    : SHAVEInstr<OOL, !con(IOL, (ins pred:$p)), !strconcat("BRU", asmstr), BRU>
{
  let Pattern = pattern;
  let isPseudo = 1;
  let hasNoSchedulingInfo = 1;
  // let isCodeGenOnly = 1;
}

// Compound instructions
// define this class to tell the scheduler that some operations must be
// issued in the same cycle
class SHAVE_CompoundInstr<dag OOL, dag IOL, list<SHAVEInstr> instructions, list<dag> pattern = []> :
  SHAVEInstr<OOL, IOL, "", PseudoUnit>
{
  let Namespace = "SHAVE";
  let Pattern = pattern;
  field list<SHAVEInstr> InstrList = instructions;
}
