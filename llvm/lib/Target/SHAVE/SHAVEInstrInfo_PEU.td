//===-- SHAVEInstrInfo_PEU.td - PEU Instructions -----------*- tablegen -*-===//
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
// This file describes the instructions available in the SHAVE Predicated
// Execution Unit (PEU)
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// PEU instruction definitions (Section 1.4.7 of Fragrak SHAVE Reference Guide)
//===----------------------------------------------------------------------===//

//
// 1.4.7.1 - PEU.ANDACC
//   AND combine CCR update with current CCR value.
//

def PEU_ANDACC : SHAVE_PEUInstr<(outs), (ins), ".ANDACC", []>
{
  let hasCtrlDep = 1;
  let Itinerary = II_PEU_PCC_CMU;
}

//
// 1.4.7.2 - PEU.HOLD
//   Preserve current CCR value.
//

def PEU_HOLD : SHAVE_PEUInstr<(outs), (ins), ".HOLD", []>
{
  let hasCtrlDep = 1;
  let Uses = [C_CMU_0_15, C_CSI];
  let Defs = [C_CMU_0_15, C_CSI];
}

//
// 1.4.7.3 - PEU.ORACC
//   OR combine CCR update with current CCR value.
//

def PEU_ORACC : SHAVE_PEUInstr<(outs), (ins), ".ORACC", []>
{
  let hasCtrlDep = 1;
  let Uses = [C_CMU_0_15, C_CSI];
  let Defs = [C_CMU_0_15, C_CSI];
  let Itinerary = II_PEU_PCC_CMU;
}

//
// 1.4.7.4 - PEU.PC1C
//   Conditionally execute instruction on the first CMU condition code.
//

def PEU_PC1C : SHAVE_PEUInstr<(outs), (ins i8imm:$msk), ".PC1C $msk", [], [isPredicating]>
{
  let hasCtrlDep = 1;
  let Uses = [CC_CMU0];
  let Itinerary = II_PEU_PCC_CMU;
}

//
// 1.4.7.5 - PEU.PC1I
//   Predicate execution of instruction on the IAU condition code.
//

def PEU_PC1I : SHAVE_PEUInstr<(outs), (ins i8imm:$msk), ".PC1I $msk", [], [isPredicating]>
{
  let hasCtrlDep = 1;
  let Uses = [CC_IAU0];
}

//
// 1.4.7.6 - PEU.PC1S
//   Predicate execution of instruction on the SAU condition code.
//

def PEU_PC1S : SHAVE_PEUInstr<(outs), (ins i8imm:$msk), ".PC1S $msk", [], [isPredicating]>
{
  let hasCtrlDep = 1;
  let Uses = [CC_SAU0];
}

//
// 1.4.7.7 - PEU.PCCI
//   Predicate execution of instruction on the IAU and CMU condition codes.
//

multiclass PEU_PCCI<string asmstr>
{
  def _AND : SHAVE_PEUInstr<(outs), (ins i8imm:$cmsk, i8imm:$imsk), !strconcat(asmstr, ".AND $cmsk, $imsk"), [], [isPredicating]>;
  def _OR  : SHAVE_PEUInstr<(outs), (ins i8imm:$cmsk, i8imm:$imsk), !strconcat(asmstr, ".OR $cmsk, $imsk"),  [], [isPredicating]>;
}

let hasCtrlDep = 1, Uses = [CC_CMU0, CC_IAU0] in
{
  defm PEU_PCC0I : PEU_PCCI<".PCC0I">;
}

let hasCtrlDep = 1, Uses = [CC_CMU1, CC_IAU0] in
{
  defm PEU_PCC1I : PEU_PCCI<".PCC1I">;
}

//
// 1.4.7.8 - PEU.PCNC
//   Predicate execution of instruction on the first two, four, eight, or sixteen CMU condition codes.
//

multiclass PEU_PCNC<string asmstr>
{
  def _AND : SHAVE_PEUInstr<(outs), (ins i8imm:$msk), !strconcat(asmstr, ".AND $msk"), [], [isPredicating]>;
  def _OR  : SHAVE_PEUInstr<(outs), (ins i8imm:$msk), !strconcat(asmstr, ".OR $msk"),  [], [isPredicating]>;
}

let hasCtrlDep = 1, Uses = [CC_CMU0, CC_CMU1] in
{
  defm PEU_PC2C : PEU_PCNC<".PC2C">;
}

let hasCtrlDep = 1, Uses = [C_CMU_0_3] in
{
  defm PEU_PC4C : PEU_PCNC<".PC4C">;
}

let hasCtrlDep = 1, Uses = [C_CMU_0_7] in
{
  defm PEU_PC8C : PEU_PCNC<".PC8C">;
}

let hasCtrlDep = 1, Uses = [C_CMU_0_15] in
{
  defm PEU_PC16C : PEU_PCNC<".PC16C">;
}

//
// 1.4.7.9 - PEU.PCNS
//   Predicate execution of instruction on the first two or four SAU condition codes.
//

multiclass PEU_PCNS<string asmstr>
{
  def _AND : SHAVE_PEUInstr<(outs), (ins i8imm:$msk), !strconcat(asmstr, ".AND $msk"), [], [isPredicating]>;
  def _OR  : SHAVE_PEUInstr<(outs), (ins i8imm:$msk), !strconcat(asmstr, ".OR $msk"),  [], [isPredicating]>;
}

let hasCtrlDep = 1, Uses = [CC_SAU0, CC_SAU1] in
{
  defm PEU_PC2S : PEU_PCNS<".PC2S">;
}

let hasCtrlDep = 1, Uses = [CC_SAU0, CC_SAU1, CC_SAU2, CC_SAU3] in
{
  defm PEU_PC4S : PEU_PCNS<".PC4S">;
}

//
// 1.4.7.10 - PEU.PCXC
//   Predicate execution of instruction on a specified CMU condition code.
//

def PEU_PCXC : SHAVE_PEUInstr<(outs), (ins i8imm:$msk, i8imm:$sel), ".PCXC $msk, $sel", [], [isPredicating]>
{
  let hasCtrlDep = 1;
  let Uses = [C_CMU_0_15];
  let Itinerary = II_PEU_PCC_CMU;
  let Predicates = [HasPCXC_Predicate];
}

//
// 1.4.7.11 - PEU.PCXS
//   Predicate execution of instruction on a specified SAU condition code.
//

def PEU_PCXS : SHAVE_PEUInstr<(outs), (ins i8imm:$msk, i8imm:$sel), ".PCXS $msk, $sel", [], [isPredicating]>
{
  let hasCtrlDep = 1;
  let Uses = [CC_SAU0, CC_SAU1, CC_SAU2, CC_SAU3];
}

//
// 1.4.7.12 - PEU.PCXX
//   Predicate execution of individual functional unit instruction(s) on the IAU or the first CMU condition code.
//

multiclass PEU_PCXX<string asmstr>
{
  def _EQ  : SHAVE_PEUInstr<(outs), (ins i8imm:$slots), !strconcat(asmstr, " EQ $slots"),  [], [isPredicating]>;
  def _NEQ : SHAVE_PEUInstr<(outs), (ins i8imm:$slots), !strconcat(asmstr, " NEQ $slots"), [], [isPredicating]>;
}

let hasCtrlDep = 1, Uses = [CC_CMU0], Itinerary = II_PEU_PCC_CMU in
{
  defm PEU_PCCX : PEU_PCXX<".PCCX">;
  def PEU_PCCX : SHAVE_PEUInstr<(outs), (ins i8imm:$msk, i8imm:$slots), ".PCCX $msk $slots", [], [isPredicating]>;
}

let hasCtrlDep = 1, Uses = [CC_IAU0] in
{
  defm PEU_PCIX : PEU_PCXX<".PCIX">;
}

//
// 1.4.7.13 - PEU.PEN4
//   Specify vector element writeback enable for concurrent VAU, SAU, LSU0 store/copy, or LSU1 store/copy
//     instruction with immediate value.
//

multiclass PEU_PEN4<string asmstr>
{
  def _WORD : SHAVE_PEUInstr<(outs), (ins i8imm:$en), !strconcat(asmstr, ".WORD $en"), [], [isWritebackPredicating]>;
  def _BYTE : SHAVE_PEUInstr<(outs), (ins i8imm:$en), !strconcat(asmstr, ".BYTE $en"), [], [isWritebackPredicating]>;
}

let hasCtrlDep = 1, Predicates = [HasVRF128_Predicate] in
{
  defm PEU_PVEN4  : PEU_PEN4<".PVEN4">;
  defm PEU_PSEN4  : PEU_PEN4<".PSEN4">;
  defm PEU_PL0EN4 : PEU_PEN4<".PL0EN4">;
  defm PEU_PL1EN4 : PEU_PEN4<".PL1EN4">;
}

//
// 1.4.7.14 - PEU.PEN8
//   Specify vector element writeback enable for concurrent VAU instruction with immediate value.
//

def PEU_PVEN8 : SHAVE_PEUInstr<(outs), (ins i8imm:$en), ".PVEN8 $en", [], [isWritebackPredicating]>, Requires<[HasVRF128_Predicate]>;

//
// 1.4.7.15 - PEU.PEN8C
//   Specify vector element writeback enable for concurrent CMU instruction with immediate value.
//

let Itinerary = II_PEU_PVEN8C in {
  def PEU_PVEN8C : SHAVE_PEUInstr<(outs), (ins i8imm:$en), ".PVEN8C $en",  [], [isWritebackPredicating]>, Requires<[HasVRF128_Predicate]>;
}

//
// 1.4.7.16 - PEU.PVEC
//   Predicate vector element writeback for concurrent VAU, SAU, LSU0 store, or LSU1 store instruction on the
//     CMU condition codes.
//

multiclass PEU_PVEC<string asmstr>
{
  let Uses = [C_CMU_0_3], Itinerary = II_PEU_PVV_CMU_0_3 in
  {
    def _32 : SHAVE_PEUInstr<(outs), (ins i8imm:$msk), !strconcat(asmstr, "32 $msk"), [], [isWritebackPredicating]>;
  }
  let Uses = [C_CMU_0_7], Itinerary = II_PEU_PVV_CMU_0_7 in
  {
    def _16 : SHAVE_PEUInstr<(outs), (ins i8imm:$msk), !strconcat(asmstr, "16 $msk"), [], [isWritebackPredicating]>;
  }
  let Uses = [C_CMU_0_15], Itinerary = II_PEU_PVV_CMU_0_15 in
  {
    def _8 : SHAVE_PEUInstr<(outs), (ins i8imm:$msk), !strconcat(asmstr, "8 $msk"), [], [isWritebackPredicating]>;
  }
}

let hasCtrlDep = 1 in
{
  defm PEU_PVV  : PEU_PVEC<".PVV">;
  defm PEU_PVS  : PEU_PVEC<".PVS">;
  defm PEU_PVL0 : PEU_PVEC<".PVL0">;
  defm PEU_PVL1 : PEU_PVEC<".PVL1">;
}
