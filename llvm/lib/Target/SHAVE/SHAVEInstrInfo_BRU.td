//===-- SHAVEInstrInfo_BRU.td - BRU Instructions -----------*- tablegen -*-===//
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
// This file describes the instructions available in the SHAVE Branch and
// Repeat Unit (BRU)
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// BRU instruction definitions (Section 1.4.3 of Fragrak SHAVE Reference Guide)
//===----------------------------------------------------------------------===//

//
// 1.4.3.1 - BRU.BRA
//   Instruction Pointer-relative jump.
//

def BRU_BRA : SHAVE_BRUInstr<(outs), (ins Label:$l),
                             ".BRA $l", []>
{
  let isBranch = 1;
  let isBarrier = 1;
  let hasCtrlDep = 1;
  let isTerminator = 1;

  let Defs = [B_IP_0];
  let Itinerary = II_BRU_BRA;
}

//
// 1.4.3.2 - BRU.INC
//   Increment Instruction Pointer. Default BRU instruction (implicit if no BRU instruction specified).
//

// This is the same as "NOP" available from SHAVEInstrInfo.td

//
// 1.4.3.3 - BRU.JMP
//   Register indirect Instruction Pointer jump.
//

let isTerminator = 1, Defs = [B_IP_0], Itinerary = II_BRU_JMP_IRF in {
  def BRU_JMP : SHAVE_BRUInstr<(outs), (ins IRF32:$target),
                               ".JMP $target", [(brind IRF32:$target)]>
  {
    let isBranch = 1;
    let isBarrier = 1;
    let hasCtrlDep = 1;
  }

  // A flavor of BRU_JMP instruction that is always conditional (not a barrier).
  def BRU_JMPcc : SHAVE_BRUInstr<(outs), (ins IRF32:$target),
                                 ".JMP $target", [/* no pattern */]>
  {
    let isBranch = 1;
    let hasCtrlDep = 1;
  }

  // A flavor of BRU_JMP instruction marked with isReturn flag
  def BRU_JMP_Ret : SHAVE_BRUInstr<(outs), (ins IRF32:$link),
                                   ".JMP $link", [/* no pattern */]>
  {
      let isBranch = 1;
      let isReturn = 1;
      let hasExtraSrcRegAllocReq = 1;
  }
}

//
// 1.4.3.4 - BRU.RFB
//   Set the Register File Base.
//

class BRU_RFB<string asmstr> : SHAVE_BRUInstr<(outs), (ins i8imm:$rfb_inc),
                                              !strconcat(asmstr, " $rfb_inc"), []>
{
  let isBarrier = 1;
  let hasCtrlDep = 1;

  let Itinerary = II_BRU_RFB;
  let Defs = [B_RFB];
}

multiclass BRU_RFB_IZ<string asmstr>
{
  def _I : BRU_RFB<!strconcat(asmstr, ".I")>;
  def _Z : BRU_RFB<!strconcat(asmstr, ".Z")>;
}

defm BRU_RFB32 : BRU_RFB_IZ<"RFB32">;
defm BRU_RFB24 : BRU_RFB_IZ<"RFB24">;
defm BRU_RFB16 : BRU_RFB_IZ<"RFB16">;
defm BRU_RFB8  : BRU_RFB_IZ<"RFB8">;

//
// 1.4.3.5 - BRU.RPI
//   Repeat current instruction a variable number of times with optional update of
//     Register File Base on each iteration.
//

def BRU_RPI : SHAVE_BRUInstr<(outs), (ins Label:$l, IRF32:$reps),
                             ".RPI $reps", [], [usesAllFUs]>
{
  let isBranch = 1;
  let hasCtrlDep = 1;
  let isBarrier = 1;

  let Itinerary = II_BRU_RPI_IRF;
  let Defs = [B_IP_0, B_STATE, B_SREPS];
}

//
// 1.4.3.6 - BRU.RPIM
//   Repeat current instruction a variable number of times with optional update of
//     Register File Base on each iteration.
//

def BRU_RPIM : SHAVE_BRUInstr<(outs), (ins i8imm:$reps),
                              ".RPIM $reps", [], [usesAllFUs]>
{
  let isBranch = 1;
  let isBarrier = 1;
  let hasCtrlDep = 1;

  let Itinerary = II_BRU_RPIM;
  let Defs = [B_IP_0, B_STATE, B_SREPS];
}

//
// 1.4.3.7 - BRU.RPL
//   Repeat (loop) block of instructions a variable number of times.
//

def BRU_RPL : SHAVE_BRUInstr<(outs), (ins IRF32:$target, IRF32:$reps),
                             ".RPL $target $reps", []>
{
  let isBranch = 1;
  let isBarrier = 1;
  let hasCtrlDep = 1;

  let Itinerary = II_BRU_RPL_IRF;
  let Defs = [B_IP_0, B_STATE, B_MREPS, B_LEND, B_LBEG];
}

//
// 1.4.3.8 - BRU.RPS
//   Repeat (loop) block of instructions a constant number of times.
//

def BRU_RPS : SHAVE_BRUInstr<(outs), (ins IRF32:$target, i8imm:$reps),
                             ".RPS $target $reps", []>
{
  let isBranch = 1;
  let isBarrier = 1;
  let hasCtrlDep = 1;

  let Itinerary = II_BRU_RPS_IRF;
  let Defs = [B_IP_0, B_MREPS, B_LEND, B_LBEG];
}

//
// 1.4.3.9 - BRU.SWIC
//   Signal software interrupt and continue.
//

def BRU_SWIC_imm : SHAVE_BRUInstr<(outs), (ins i8imm:$tag),
                             ".SWIC $tag ${:comment} Software Interrupt and Continue", []>
{
  // This is not a branch, it is a software interrupt
  let isBranch = 0;
  let isBarrier = 1;
  let hasCtrlDep = 1;

  let Itinerary = II_BRU_SWIC_SWIH;
  let Defs = [B_IP_0];
}

//
// 1.4.3.10 - BRU.SWIH
//   Signal software interrupt and halt.
//
// FIXME: Movidius - TODO: Need to add a Myriad2.3 version of this which supports a 16-bit tag
def BRU_SWIH_imm : SHAVE_BRUInstr<(outs), (ins i8imm:$tag),
                                  ".SWIH $tag ${:comment} Software Interrupt and Halt", [], [isBRU_SWIH]>
{
  // This is not a branch, it is a software interrupt
  let isBranch = 0;
  let isBarrier = 1;
  let hasCtrlDep = 1;

  let Itinerary = II_BRU_SWIC_SWIH;
  let Defs = [B_IP_0];
}

def BRU_SWIH_Ret : SHAVE_BRUInstr<(outs), (ins),
                                  ".SWIH 0 ${:comment} SHAVEExitCodes_t::SHAVEExitNormal", [], [isBRU_SWIH]>
{
  // this is not a branch, it is a software interrupt
  let isBranch = 0;
  let isBarrier = 1;
  let isReturn = 1;
  let hasCtrlDep = 1;

  let Itinerary = II_BRU_SWIC_SWIH;
  let Defs = [B_IP_0];
}

def BRU_SWIH_imm_preemption : SHAVE_BRUInstr<(outs), (ins i8imm:$tag),
                                            ".SWIH $tag ${:comment} auto-generated for \"norestore\" preemption handler", [], [isBRU_SWIH]>
{
  // This is not a branch, it is a software interrupt
  let isBranch = 0;
  let isBarrier = 0;
  let hasCtrlDep = 1;

  let Itinerary = II_BRU_SWIC_SWIH;
  let Defs = [B_IP_0];
}

//
// 1.4.3.11 - BRU.SWP
//   Register indirect Instruction Pointer jump with store of return instruction address.
//

def BRU_SWP : SHAVE_BRUInstr<(outs IRF32:$oldPC), (ins IRF32:$newPC),
                             ".SWP $newPC ${:comment} Function call", []>
{
  let Constraints = "$newPC = $oldPC";

  let isBarrier = 1;
  let hasCtrlDep = 1;
  let isCall = 1;

  let Itinerary = II_BRU_SWP_IRF;
}

def BRU_SWP_preemption : SHAVE_BRUInstr<(outs), (ins IRF32:$newPC),
                                        ".SWP $newPC ${:comment} auto-generated for preemption handler", []>
{
  let isBarrier = 1;
  let hasCtrlDep = 1;

  let Itinerary = II_BRU_SWP_IRF;
}

//===----------------------------------------------------------------------===//
// Calling convention SDTypeProfiles, SDNodes and Psuedo Instructions
//===----------------------------------------------------------------------===//
def SHAVECallseqStart  : SDNode<"ISD::CALLSEQ_START", SDCallSeqStart<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>, [SDNPHasChain, SDNPOutGlue]>;
def SHAVECallseqEnd    : SDNode<"ISD::CALLSEQ_END",   SDCallSeqEnd<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>,   [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def shavecall          : SDNode<"SHAVEISD::CALL",     SDTypeProfile<0, -1, [SDTCisPtrTy<0>]>, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;
def shaveret           : SDNode<"SHAVEISD::RET_FLAG", SDTNone,                                [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

let hasCtrlDep = 1, hasSideEffects = 1, isCodeGenOnly = 1, Defs = [I19], Uses = [I19] in {
  def ADJCALLSTACKDOWN : SHAVE_Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                                      "${:comment} ADJCALLSTACKDOWN",
                                      [(SHAVECallseqStart timm:$amt1, timm:$amt2)]>;
  def ADJCALLSTACKUP   : SHAVE_Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                                      "${:comment} ADJCALLSTACKUP",
                                      [(SHAVECallseqEnd timm:$amt1, timm:$amt2)]>;
}

// SHAVE_RETURN : placeholder for the SHAVE return sequence
def SHAVE_RETURN : SHAVE_Pseudo<(outs), (ins variable_ops),
                                "SHAVE_RETURN", [(shaveret)]> {
  let isReturn = 1;
  let isTerminator = 1;
  let usesCustomInserter = 1;
}

// SHAVE_CALL : this is a placeholder for the shave call sequence
// it will be custom inserted along whith the appropiate
// sequence of LDIL/LDIHs or pointer copies
def SHAVE_CALL : SHAVE_Pseudo<(outs), (ins IRF32:$target, variable_ops),
                              "SHAVE_CALL $target", [(shavecall IRF32:$target)]> {
  let isCall = 1;
  let usesCustomInserter = 1;
}

/*
  SHAVE_DYNALLOC instruction is a pseudo used by SHAVELowering::SHAVELowerDYNAMIC_STACKALLOC and
  is later expanded to actual stack probes instructions in SHAVELowering::EmitCustomDYNALLOC().
  It takes one operand which is the value of the new
  adjusted SP register.
*/
def SDT_SHAVEDynAlloc : SDTypeProfile<0, 1, [SDTCisVT<0, i32>]>;
def SHAVEDynAlloc     : SDNode<"SHAVEISD::DYNALLOC", SDT_SHAVEDynAlloc, [SDNPHasChain, SDNPInGlue, SDNPOutGlue, SDNPVariadic]>;

def DYNALLOC : SHAVE_Pseudo<(outs), (ins IRF32:$src), "${:comment} DYNALLOC $src", [(SHAVEDynAlloc IRF32:$src)]>
{
   let usesCustomInserter = 1;
   //let hasCtrlDep = 1;
   //let hasSideEffects = 1;
   let Defs = [I19];
   let Uses = [I19];
}
