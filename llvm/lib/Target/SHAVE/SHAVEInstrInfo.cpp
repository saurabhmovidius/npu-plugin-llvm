//===-- SHAVEInstInfo.cpp - Instruction Information -------------*- C++ -*-===//
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

#define DEBUG_TYPE "shave-insn"

#include <cstdio>
#include <set>
#include <sstream>

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSchedule.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsShave.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVEInstrInfo.h"
#include "SHAVERegisterInfo.h"
#include "SHAVETargetMachine.h"
#include "SHAVEIntrinsicsInfo.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "SHAVEGenInstrInfo.inc"

using namespace llvm;
using namespace SHAVEOptions;


SHAVEInstrInfo::SHAVEInstrInfo(SHAVESubtarget &ST)
: SHAVEGenInstrInfo(SHAVE::ADJCALLSTACKDOWN, SHAVE::ADJCALLSTACKUP),
  SHAVEST(ST),
  SHAVERegInfo(ST, *this) {
  // Initialise the values for the branch predictor
  AvgILP = 1.4f;
  MinILP = 0.6f;
  AvgILPThreshold = 20;
}

void SHAVEInstrInfo::initialiseLatencies() {
  // Initialise the branch delay slots, and LSU load latency, clamping them to their associated limits
  nDelaySlots = std::min(std::max((int)SHAVEOptions::BranchDelays, minBranchDelaySlots), maxBranchDelaySlots);

  unsigned minLoadLatency = SHAVEST.getMinimumLoadLatency();
  loadLatency = std::min(std::max((unsigned)SHAVEOptions::LoadLatency, minLoadLatency), maxLoadLatency);
}

unsigned SHAVEInstrInfo::isLoadFromStackSlot(const MachineInstr &MI, int &FrameIndex) const {
  if ((MI.getNumOperands() < 2) || !SHAVEConflicts::check_isLoad(MI.getOpcode()))
    return 0;

  const MachineOperand &MOp1 = MI.getOperand(1);
  if (!MOp1.isFI())
    return 0;
  FrameIndex = MOp1.getIndex();

  const MachineOperand &MOp0 = MI.getOperand(0);
  if (!MOp0.isReg())
    return 0;

  // Make sure the load offset (if any) is zero.
  if (MI.getNumOperands() >= 3) {
    const MachineOperand &MOp2 = MI.getOperand(2);
    if (MOp2.isImm() && (MOp2.getImm() != 0))
      return 0;
  }

  return MOp0.getReg();
}

unsigned SHAVEInstrInfo::isStoreToStackSlot(const MachineInstr &MI, int &FrameIndex) const {
  if ((MI.getNumOperands() < 2) || !SHAVEConflicts::check_isStore(MI.getOpcode()))
    return 0;

  const MachineOperand &MOp1 = MI.getOperand(1);
  if (!MOp1.isFI())
    return 0;
  FrameIndex = MOp1.getIndex();

  const MachineOperand &MOp0 = MI.getOperand(0);
  if (!MOp0.isReg())
    return 0;

  // Make sure the load offset (if any) is zero.
  if (MI.getNumOperands() >= 3) {
    const MachineOperand &MOp2 = MI.getOperand(2);
    if (MOp2.isImm() && (MOp2.getImm() != 0))
      return 0;
  }

  return MOp0.getReg();
}

void SHAVEInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MI,
                                         Register SrcReg, bool isKill,
                                         int FrameIndex,
                                         const TargetRegisterClass *RC,
                                         const TargetRegisterInfo *TRI,
                                         Register VReg) const {
  DebugLoc dbgLoc;
  unsigned StoreOpc = 0;
  unsigned StoreSize = 0;
  unsigned StoreFlags = getKillRegState(isKill);
  Register ValReg = SrcReg;

  if (MI != MBB.end())
    dbgLoc = MI->getDebugLoc();

  // FIXME: Movidius - These should be bit-specific, but type agnostic
  // Handle IRFs
  if (RC == &SHAVE::IRF64RegClass) {
    // assert(!isMyriad4v0() && "Can't do a 64 bit scalar store to stack on Myriad 3");
    if (!hasFeature(SHAVE::HasIRF64_LSU_CMU_Instrs_Feature))
      StoreOpc = SHAVE::STO_I64_Myr4;
    else
      StoreOpc = SHAVE::LSU_STO_i64;
    StoreSize = 8;

  }

  else if (RC == &SHAVE::IRF32RegClass) {
    StoreOpc = SHAVE::LSU_STO_i32;
    StoreSize = 4;
  } else if (RC == &SHAVE::IRF16_lRegClass) {
    StoreOpc = SHAVE::LSU_STO_i16;
    StoreSize = 2;
  } else if (RC == &SHAVE::IRF8_q0RegClass) {
    StoreOpc = SHAVE::LSU_STO_i8;
    StoreSize = 1;
  }
  // Handle VRFs
  else if (RC == &SHAVE::VRF128RegClass) {
    StoreOpc = SHAVE::STOV128;
    StoreSize = 16;
  } else if (RC == &SHAVE::VRF64_lRegClass) {
    StoreOpc = SHAVE::LSU_STO_v4i16;
    StoreSize = 8;
  }  else if (RC == &SHAVE::VRF64_hRegClass) {
    StoreOpc = SHAVE::LSU_STOV64_h;
    StoreSize = 8;
  } else if (RC == &SHAVE::VRF32_q0RegClass) {
    StoreOpc = SHAVE::STO32_VRF;
    StoreFlags |= getKillRegState(true);
    StoreSize = 4;
  } else if (RC == &SHAVE::VRF16_e0RegClass) {
    StoreOpc = SHAVE::STO16_VRF;
    StoreFlags |= getKillRegState(true);
    StoreSize = 2;
  }
  // Handle WVRFs
  else if (RC == &SHAVE::WVRF512RegClass) {
    StoreOpc = SHAVE::STOV512;
    StoreSize = 64;
  } else if (RC == &SHAVE::WVRF256_0RegClass) {
    StoreOpc = SHAVE::LSU_STOV_256_v32i8;
    StoreSize = 32;
  } else if (RC == &SHAVE::WVRF128_0RegClass) {
    StoreOpc = SHAVE::LSU_STOV_128_v16i8;
    StoreSize = 16;
  } else if (RC == &SHAVE::WVRF64_0RegClass) {
    StoreOpc = SHAVE::LSU_STOV_64_v8i8;
    StoreSize = 8;
  } else if (RC == &SHAVE::WVRF32_0RegClass) {
    StoreOpc = SHAVE::LSU_STOV_32_v4i8;
    StoreSize = 4;
  }
  // Handle TRFs
  else if (RC == &SHAVE::TRF_ACC128RegClass) {
    // FIXME: Movidius - Need to "spill" the V_ACC
    llvm_unreachable("Can't spill TRF register to the stack");
  } else
    llvm_unreachable("Unknown spill register type");

  MachineFunction *MF = MBB.getParent();
  Align align = MF->getFrameInfo().getObjectAlign(FrameIndex);
  LLVM_ATTRIBUTE_UNUSED unsigned stackOffset =
      MF->getFrameInfo().getObjectOffset(FrameIndex);

  // From Myriad 4.0 onwards the immediate value in LSU.STO must be aligned to
  // the size of the request. Make sure the object offset is a multiple of its
  // size to be able to use the instruction.
  assert((hasFeature(SHAVE::LDOSTO_15_bit_offset) ||
          (stackOffset % StoreSize == 0)) &&
         "Stack offset needs to be a multiple of the store size");

  MachinePointerInfo PtrInfo =
      MachinePointerInfo::getFixedStack(*MF, FrameIndex, 0);
  MachineMemOperand *MMO = MF->getMachineMemOperand(
      PtrInfo, MachineMemOperand::MOStore, StoreSize, align);
  MachineInstrBuilder MIBLo = BuildMI(MBB, MI, dbgLoc, get(StoreOpc));

  MIBLo.addReg(ValReg, StoreFlags);

  // FIXME: Movidius - adapt this code to use a register if the range of the
  // immediate exceeds the range of a signed 15-bit integer:
  // save the immediate to a register and use 'STX'.
  if (!isInt<15u>(FrameIndex))
    llvm_unreachable("Offset to STO exceeds range of a signed 15-bit integer");
  // assert(isInt<15u>(FrameIndex) && "Offset to STO exceeds range of a signed 15-bit
  // integer");
  MIBLo.addFrameIndex(FrameIndex).addImm(0);
  MIBLo.addMemOperand(MMO);
  finaliseMI(MIBLo);
}

void SHAVEInstrInfo::loadRegFromStashReg(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         Register DestReg, unsigned VirtReg, Register StashReg,
                                         const TargetRegisterClass *RC,
                                         const TargetRegisterInfo *TRI) const
{
  DebugLoc DL;

  if (I != MBB.end())
     DL = I->getDebugLoc();

  // Get the lane index from the mapped register entry
  unsigned laneIdx = 0;
  if (SHAVE::VRF32_q3RegClass.contains(StashReg))
    laneIdx = 3;
  else if (SHAVE::VRF32_q2RegClass.contains(StashReg))
    laneIdx = 2;
  else if (SHAVE::VRF32_q1RegClass.contains(StashReg))
    laneIdx = 1;

  MachineInstrBuilder MIB = BuildMI(MBB, I, DL, get(SHAVE::CMU_CPVI_x32), DestReg).addReg(VirtReg).addImm(laneIdx);
  finaliseMI(MIB);
}

void SHAVEInstrInfo::storeRegToStashReg(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator I,
                                        Register SrcReg, bool isKill, unsigned VirtReg, Register StashReg,
                                        const TargetRegisterClass *RC,
                                        const TargetRegisterInfo *TRI) const {
  DebugLoc DL;

  if (I != MBB.end())
     DL = I->getDebugLoc();

  // Get the lane index from the mapped register entry
  unsigned laneIdx = 0;
  if (SHAVE::VRF32_q3RegClass.contains(StashReg))
    laneIdx = 3;
  else if (SHAVE::VRF32_q2RegClass.contains(StashReg))
    laneIdx = 2;
  else if (SHAVE::VRF32_q1RegClass.contains(StashReg))
    laneIdx = 1;

  MachineInstrBuilder MIB = BuildMI(MBB, I, DL, get(SHAVE::CMU_CPIV_x32), VirtReg).addReg(VirtReg).addReg(SrcReg).addImm(laneIdx);
  finaliseMI(MIB);
}

void SHAVEInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MI,
                                          Register DestReg, int FrameIndex,
                                          const TargetRegisterClass *RC,
                                          const TargetRegisterInfo *TRI,
                                          Register VReg) const {
  // FIXME: Movidius - These all explicitly clobber Physical Register I0!
  DebugLoc DL;

  unsigned LoadOpc = 0;
  unsigned LoadSize = 0;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  // Handle IRFs
  if (RC == &SHAVE::IRF64RegClass) {
    if (!hasFeature(SHAVE::HasIRF64_LSU_CMU_Instrs_Feature))
        LoadOpc = SHAVE::LDO_I64_Myr4;
    else
      LoadOpc = SHAVE::LSU_LDO_i64;

    LoadSize = 8;
  }
  else if (RC == &SHAVE::IRF32RegClass) {
    LoadOpc = SHAVE::LSU_LDO_i32;
    LoadSize = 4;
  } else if (RC == &SHAVE::IRF16_lRegClass) {
    LoadOpc = SHAVE::LSU_LDO_i16;
    LoadSize = 2;
  } else if (RC == &SHAVE::IRF8_q0RegClass) {
    LoadOpc = SHAVE::LSU_LDO_i8;
    LoadSize = 1;
  }

  // Handle VRFs
  else if (RC == &SHAVE::VRF128RegClass) {
    LoadOpc = SHAVE::LDOV128;
    LoadSize = 16;
  } else if (RC == &SHAVE::VRF64_lRegClass) {
    LoadOpc = SHAVE::LSU_LDO_v4i16;
    LoadSize = 8;
  } else if (RC == &SHAVE::VRF64_hRegClass) {
    LoadOpc = SHAVE::LSU_LDO_v4i16_h;
    LoadSize = 8;
  } else if (RC == &SHAVE::VRF32_q0RegClass) {
    LoadOpc = SHAVE::LSU_LDO64_l_Raw;
    LoadSize = 4;
  }
  else if (RC == &SHAVE::VRF16_e0RegClass) {
    LoadOpc = SHAVE::LSU_LDO_v2i8;
    LoadSize = 2;
  }
  // Handle WVRFs
  else if (RC == &SHAVE::WVRF512RegClass) {
    LoadOpc = SHAVE::LDOV512;
    LoadSize = 64;
  } else if (RC == &SHAVE::WVRF256_0RegClass) {
    LoadOpc = SHAVE::LSU_LDOV_256_v32i8;
    LoadSize = 32;
  } else if (RC == &SHAVE::WVRF128_0RegClass) {
    LoadOpc = SHAVE::LSU_LDOV_128_v16i8;
    LoadSize = 16;
  } else if (RC == &SHAVE::WVRF64_0RegClass) {
    LoadOpc = SHAVE::LSU_LDOV_64_v8i8;
    LoadSize = 8;
  } else if (RC == &SHAVE::WVRF32_0RegClass) {
    LoadOpc = SHAVE::LSU_LDOV_32_v4i8;
    LoadSize = 4;
  }
  // Handle TRFs
  else if (RC == &SHAVE::TRF_ACC128RegClass) {
    // FIXME: Movidius - Need to "reload" the V_ACC
    llvm_unreachable("Can't reload TRF register from the stack");
  } else {
    llvm_unreachable("Unknown reload register type");
  }

  MachineFunction *MF = MBB.getParent();
  Align align = MF->getFrameInfo().getObjectAlign(FrameIndex);
  LLVM_ATTRIBUTE_UNUSED unsigned stackOffset =
      MF->getFrameInfo().getObjectOffset(FrameIndex);

  // From Myriad 4.0 onwards the immediate value in LSU.LDO must be aligned to
  // the size of the request. Make sure the object offset is a multiple of its
  // size to be able to use the instruction.
  assert((hasFeature(SHAVE::LDOSTO_15_bit_offset) ||
          (stackOffset % LoadSize == 0)) &&
         "Stack offset needs to be a multiple of the load size");

  MachinePointerInfo PtrInfo =
      MachinePointerInfo::getFixedStack(*MF, FrameIndex, 0);
  MachineMemOperand *MMO = MF->getMachineMemOperand(
      PtrInfo, MachineMemOperand::MOLoad, LoadSize, align);
  MachineInstrBuilder MIB = BuildMI(MBB, MI, DL, get(LoadOpc), DestReg);

  // FIXME: Movidius - adapt this code to use a register if the range of the immediate exceeds the range
  //        of a signed 15-bit integer: save the immediate to a register and use 'LDX'.
  if (!isInt<15u>(FrameIndex)) llvm_unreachable("Offset to LDO exceeds range of a signed 15-bit integer");
//assert(isInt<15u>(FrameIndex) && "Offset to LDO exceeds range of a signed 15-bit integer");
  MIB.addFrameIndex(FrameIndex).addImm(0);
  MIB.addMemOperand(MMO);
  finaliseMI(MIB);
}

void SHAVEInstrInfo::finaliseMI(const MachineInstrBuilder &MIB, FrameContext_t frameInstructionType) const {
  // Add default predication operands for predicable instructions
  if (isPredicable(*MIB)) {
    MIB.addImm(SHAVECC::AL); // Always execute.
    MIB.addReg(0);           // No predication register used.
  }

  // Add functional unit ID for LSU and CMU instructions.
  unsigned Opc = MIB->getOpcode();

  if (SHAVEConflicts::check_usesLSU1(Opc) && !SHAVEConflicts::check_isPseudoInstr(Opc))
    MIB.addImm(SHAVE::LSU0);
  else if (SHAVEConflicts::check_usesCMU(Opc) && !SHAVEConflicts::check_isPseudoInstr(Opc))
    MIB.addImm(SHAVE::CMU);

  // Finally, note that this is part of the prologue or epilogue frame setup if appropriate
  if (frameInstructionType == inFunctionPrologue)
    MIB.setMIFlag(MachineInstr::FrameSetup);
  else if (frameInstructionType == inFunctionEpilogue)
    MIB.setMIFlag(MachineInstr::FrameDestroy);
}

void
SHAVEInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
MachineBasicBlock::iterator MI, const DebugLoc &DL,
MCRegister DestReg, MCRegister SrcReg, bool KillSrc) const {
  // Is this trying to copy the same physical register to itself?
  if (DestReg == SrcReg)
    llvm_unreachable("Unnecessary copy of a register to itself");

  // Last two operands are always execute predicate
  if (SHAVE::IRF32RegClass.contains(DestReg, SrcReg)) {
    // IRF32 -> IRF32
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPII_32_Raw), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc)));
  } else if (SHAVE::IRF16_lRegClass.contains(DestReg, SrcReg)) {
    // IRF16_l -> IRF16_l
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPII_16_Raw), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc)));
  } else if (SHAVE::IRF8_q0RegClass.contains(DestReg, SrcReg)) {
    // IRF8_q0 -> IRF8_q0
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPII_8_Raw), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc)));
  } else if(SHAVE::VRF128RegClass.contains(DestReg, SrcReg)) {
    // VRF128 -> VRF128
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVV), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc)));
  } else if(SHAVE::VRF64_lRegClass.contains(DestReg, SrcReg) ||
            SHAVE::VRF64_hRegClass.contains(DestReg, SrcReg)) {
    // VRF64_l -> VRF64_l or VRF64_h -> VRF64_h
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVV_64), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc)));
  } else if(SHAVE::VRF64_lRegClass.contains(DestReg) && SHAVE::VRF64_hRegClass.contains(SrcReg)) {
    // VRF64_h -> VRF64_l
    // FIXME: Use a cp.64 for Myriad2.3
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_VSZMWORD_imm_vrf), DestReg)
               .addReg(SrcReg, getKillRegState(KillSrc))
               .addImm(VSZM_LANE0 + 2)
               .addImm(VSZM_LANE0 + 3)
               .addImm(VSZM_LANE0 + 2)
               .addImm(VSZM_LANE0 + 3));
  } else if(SHAVE::VRF64_hRegClass.contains(DestReg) && SHAVE::VRF64_lRegClass.contains(SrcReg)) {
    // VRF64_l -> VRF64_h
    // FIXME: Use a cp.64 for Myriad2.3
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_VSZMWORD_imm_vrf), DestReg)
               .addReg(SrcReg, getKillRegState(KillSrc))
               .addImm(VSZM_LANE0)
               .addImm(VSZM_LANE0 + 1)
               .addImm(VSZM_LANE0)
               .addImm(VSZM_LANE0 + 1));
  } else if(SHAVE::VRF32_q0RegClass.contains(DestReg, SrcReg)) {
    // VRF32_q0 -> VRF32_q0
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVV), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc)));
  } else if((SHAVE::IRF32RegClass.contains(SrcReg) && SHAVE::VRF32_q0RegClass.contains(DestReg))
              || (SHAVE::IRF32RegClass.contains(SrcReg) && SHAVE::VRF32_q1RegClass.contains(DestReg))
              || (SHAVE::IRF32RegClass.contains(SrcReg) && SHAVE::VRF32_q2RegClass.contains(DestReg))
              || (SHAVE::IRF32RegClass.contains(SrcReg) && SHAVE::VRF32_q3RegClass.contains(DestReg))) {
    // IRF32 -> VRF32_q[0-3]
    unsigned LaneIdx = 0;

    if(SHAVE::VRF32_q3RegClass.contains(DestReg))
      LaneIdx = 3;
    else if(SHAVE::VRF32_q2RegClass.contains(DestReg))
      LaneIdx = 2;
    else if(SHAVE::VRF32_q1RegClass.contains(DestReg))
      LaneIdx = 1;

    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPIV_x32_v4i32), DestReg)
      .addReg(DestReg, getUndefRegState(true))
      .addReg(SrcReg, getKillRegState(KillSrc))
      .addImm(LaneIdx));
  } else if((SHAVE::IRF32RegClass.contains(DestReg)
            && (SHAVE::VRF32_q0RegClass.contains(SrcReg) || SHAVE::VRF32_q1RegClass.contains(SrcReg)
                || SHAVE::VRF32_q2RegClass.contains(SrcReg) || SHAVE::VRF32_q3RegClass.contains(SrcReg)))) {
    // VRF_q[0-3] -> IRF32
    unsigned LaneIdx = 0;

    if(SHAVE::VRF32_q3RegClass.contains(SrcReg))
      LaneIdx = 3;
    else if(SHAVE::VRF32_q2RegClass.contains(SrcReg))
      LaneIdx = 2;
    else if(SHAVE::VRF32_q1RegClass.contains(SrcReg))
      LaneIdx = 1;

    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVI_x32), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc))
      .addImm(LaneIdx));
  } else if (SHAVE::TRFRegClass.contains(SrcReg) && SHAVE::IRF32RegClass.contains(DestReg)) {
    // TRF -> IRF32
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPTI), DestReg)
      .addReg(SrcReg));
  } else if (SHAVE::TRF_ACC128RegClass.contains(SrcReg) && SHAVE::VRF128RegClass.contains(DestReg)) {
    // TRF_ACC128 -> VRF128
    // FIXME: Movidius - we need to wean 'moviCompile' off requiring a physical scratch register
    unsigned TmpReg = SHAVERegisterInfo::getScratchReg();
    const static unsigned SubRegIndices[] = {SHAVE::qsub_0, SHAVE::qsub_1,
                                             SHAVE::qsub_2, SHAVE::qsub_3};
    const static unsigned SrcRegs[] = {SHAVE::V_ACC0, SHAVE::V_ACC1,
                                       SHAVE::V_ACC2, SHAVE::V_ACC3};

    for (unsigned i = 0; i < 4; i++) {
      unsigned DestSubReg = SHAVERegInfo.getSubReg(DestReg, SubRegIndices[i]);

      copyPhysReg(MBB, MI, DL, TmpReg, SrcRegs[i], KillSrc);
      copyPhysReg(MBB, MI, DL, DestSubReg, TmpReg, true);
    }
  } else if (SHAVE::IRF32RegClass.contains(SrcReg) && SHAVE::TRFRegClass.contains(DestReg)) {
    // IRF32 -> TRF
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPIT), DestReg)
      .addReg(SrcReg));
  } else if (SHAVE::VRF128RegClass.contains(SrcReg) && SHAVE::TRF_ACC128RegClass.contains(DestReg)) {
    // VRF128 -> TRF_ACC128
    // FIXME: Movidius - we need to wean 'moviCompile' off requiring a physical scratch register
    unsigned TmpReg = SHAVERegisterInfo::getScratchReg();

    const static unsigned SubRegIndices[] = {SHAVE::qsub_0, SHAVE::qsub_1, SHAVE::qsub_2, SHAVE::qsub_3};
    const static unsigned DestRegs[] = {SHAVE::V_ACC0, SHAVE::V_ACC1, SHAVE::V_ACC2, SHAVE::V_ACC3};

    for (unsigned i = 0; i < 4; i++) {
      unsigned SrcSubReg = SHAVERegInfo.getSubReg(SrcReg, SubRegIndices[i]);

      copyPhysReg(MBB, MI, DL, TmpReg, SrcSubReg, KillSrc);
      copyPhysReg(MBB, MI, DL, DestRegs[i], TmpReg, true);
    }
  } else if (SHAVE::IRF16_hRegClass.contains(SrcReg) && SHAVE::IRF16_lRegClass.contains(DestReg)) {
    // IRF16_h -> IRF16_l
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPII_16_high_to_low), DestReg)
              .addReg(SrcReg, getKillRegState(KillSrc)));
  } else if (SHAVE::IRF16_lRegClass.contains(SrcReg) && SHAVE::IRF16_hRegClass.contains(DestReg)) {
    // IRF16_l -> IRF16_h
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPII_16_low_to_high), DestReg)
              .addReg(SrcReg, getKillRegState(KillSrc)));
  } else if (SHAVE::IRF64RegClass.contains(SrcReg) && SHAVE::IRF64RegClass.contains(DestReg)) {
    // IRF64 -> IRF64
    // 2v1 and 2v2 have support for IRF64 load/store instructions but not IRF64 CMU.CPII. Rather than
    // separate CPII_64 into its own feature, we perform this additional check here
    if (hasFeature(SHAVE::HasIRF64_LSU_CMU_Instrs_Feature)) {
      finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPII_64_Raw), DestReg)
                 .addReg(SrcReg, getKillRegState(KillSrc)));
    } else {
      // Handle ordering of subreg copies to avoid problems with overlapping super-regs
      if (SrcReg > DestReg) {
        finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPII_32_Raw), SHAVERegInfo.getSubReg(DestReg, SHAVE::vsub32_0))
          .addReg(SHAVERegInfo.getSubReg(SrcReg, SHAVE::vsub32_0))
          .addReg(SrcReg, RegState::Implicit).addReg(DestReg, RegState::ImplicitDefine));
        finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPII_32_Raw), SHAVERegInfo.getSubReg(DestReg, SHAVE::vsub32_1))
          .addReg(SHAVERegInfo.getSubReg(SrcReg, SHAVE::vsub32_1), getKillRegState(KillSrc))
          .addReg(DestReg, RegState::ImplicitDefine).addReg(SrcReg, RegState::Implicit)); 
      } else {
        finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPII_32_Raw), SHAVERegInfo.getSubReg(DestReg, SHAVE::vsub32_1))
          .addReg(SHAVERegInfo.getSubReg(SrcReg, SHAVE::vsub32_1), getKillRegState(KillSrc))
          .addReg(DestReg, RegState::ImplicitDefine).addReg(SrcReg, RegState::Implicit)); 
        finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPII_32_Raw), SHAVERegInfo.getSubReg(DestReg, SHAVE::vsub32_0))
          .addReg(SHAVERegInfo.getSubReg(SrcReg, SHAVE::vsub32_0))
          .addReg(SrcReg, RegState::Implicit).addReg(DestReg, RegState::ImplicitDefine));
      }
    }
  } else if (SHAVE::IRF64RegClass.contains(SrcReg) && SHAVE::VRF64_lRegClass.contains(DestReg)) {
    // IRF64 -> VRF64_l
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPIV_x32_v2i32), DestReg)
      .addReg(DestReg).addReg(SHAVERegInfo.getSubReg(SrcReg, SHAVE::vsub32_0)).addImm(0)
      .addReg(SrcReg, RegState::Implicit));
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPIV_x32_v2i32), DestReg)
      .addReg(DestReg).addReg(SHAVERegInfo.getSubReg(SrcReg, SHAVE::vsub32_1)).addImm(1)
      .addReg(SrcReg, RegState::Implicit).addReg(DestReg, RegState::Implicit));
  } else if (SHAVE::VRF64_lRegClass.contains(SrcReg) && SHAVE::IRF64RegClass.contains(DestReg)) {
    // Copies from VRF64_l to IRF64
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVI_x32_v2i32), SHAVERegInfo.getSubReg(DestReg, SHAVE::vsub32_0))
      .addReg(SrcReg).addImm(0).addReg(SrcReg, RegState::ImplicitDefine));
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVI_x32_v2i32), SHAVERegInfo.getSubReg(DestReg, SHAVE::vsub32_1))
      .addReg(SrcReg, getKillRegState(KillSrc)).addImm(1).addReg(SrcReg, RegState::ImplicitDefine));
  } else if ((SHAVE::VRF16_e0RegClass.contains(SrcReg) || SHAVE::VRF16_e1RegClass.contains(SrcReg)
               || SHAVE::VRF16_e2RegClass.contains(SrcReg) || SHAVE::VRF16_e3RegClass.contains(SrcReg)
               || SHAVE::VRF16_e4RegClass.contains(SrcReg) || SHAVE::VRF16_e5RegClass.contains(SrcReg)
               || SHAVE::VRF16_e6RegClass.contains(SrcReg) || SHAVE::VRF16_e7RegClass.contains(SrcReg))
            && SHAVE::IRF16_lRegClass.contains(DestReg)) {
    // VRF16_e[0-7] -> IRF16_l
    unsigned LaneIdx = 0;

    if (SHAVE::VRF16_e1RegClass.contains(SrcReg))
      LaneIdx = 1;
    else if (SHAVE::VRF16_e2RegClass.contains(SrcReg))
      LaneIdx = 2;
    else if (SHAVE::VRF16_e3RegClass.contains(SrcReg))
      LaneIdx = 3;
    else if (SHAVE::VRF16_e4RegClass.contains(SrcReg))
      LaneIdx = 4;
    else if (SHAVE::VRF16_e5RegClass.contains(SrcReg))
      LaneIdx = 5;
    else if (SHAVE::VRF16_e6RegClass.contains(SrcReg))
      LaneIdx = 6;
    else if (SHAVE::VRF16_e7RegClass.contains(SrcReg))
      LaneIdx = 7;

    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVI_x16_l), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc)).addImm(LaneIdx));
  } else if ((SHAVE::VRF16_e0RegClass.contains(DestReg) || SHAVE::VRF16_e1RegClass.contains(DestReg)
              || SHAVE::VRF16_e2RegClass.contains(DestReg) || SHAVE::VRF16_e3RegClass.contains(DestReg)
              || SHAVE::VRF16_e4RegClass.contains(DestReg) || SHAVE::VRF16_e5RegClass.contains(DestReg)
              || SHAVE::VRF16_e6RegClass.contains(DestReg) || SHAVE::VRF16_e7RegClass.contains(DestReg))
            && SHAVE::IRF16_lRegClass.contains(SrcReg)) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPIVR_VRF16), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if (SHAVE::VRF16_e0RegClass.contains(DestReg) && SHAVE::VRF16_e0RegClass.contains(SrcReg)) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVV), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if (SHAVE::WVRF512RegClass.contains(DestReg, SrcReg)) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVV_512_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if (SHAVE::WVRF256_0RegClass.contains(DestReg, SrcReg)) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVV_256_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if (SHAVE::WVRF128_0RegClass.contains(DestReg, SrcReg)) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVV_128_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if (SHAVE::WVRF64_0RegClass.contains(DestReg, SrcReg)) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVV_64_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if (SHAVE::WVRF32_0RegClass.contains(DestReg, SrcReg)) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVV_32_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if (SHAVE::WVRF16_0RegClass.contains(DestReg, SrcReg)) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVV_16_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if ((SHAVE::IRF64RegClass.contains(SrcReg) && SHAVE::WVRF64_0RegClass.contains(DestReg))) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPIV_64_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if ((SHAVE::IRF32RegClass.contains(SrcReg) && SHAVE::WVRF32_0RegClass.contains(DestReg))) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPIV_32_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if ((SHAVE::IRF16_lRegClass.contains(SrcReg) && SHAVE::WVRF16_0RegClass.contains(DestReg))) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPIV_16_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if ((SHAVE::WVRF64_0RegClass.contains(SrcReg) && SHAVE::IRF64RegClass.contains(DestReg))) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVI_64_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if ((SHAVE::WVRF32_0RegClass.contains(SrcReg) && SHAVE::IRF32RegClass.contains(DestReg))) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVI_32_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  } else if ((SHAVE::WVRF16_0RegClass.contains(SrcReg) && SHAVE::IRF16_lRegClass.contains(DestReg))) {
    finaliseMI(BuildMI(MBB, MI, DL, get(SHAVE::CMU_CPVI_16_Myr4), DestReg).addReg(SrcReg, getKillRegState(KillSrc)));
  }
  else {
#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
    MBB.dump();
    errs() << "Culprit copy: "; MI->dump();
#endif
    llvm_unreachable("Illegal register copy detected");
  }
}

SHAVECC::CondCode SHAVEInstrInfo::getPredicateCC(const MachineInstr& MI) const {
  const MCInstrDesc& TID = MI.getDesc();
  if (TID.isPredicable()) {
    for (unsigned i = 0; i < MI.getNumOperands(); ++i) {
      if (TID.operands()[i].isPredicate()) {
        // First predicate operand is a predication mask
        if (MI.getOperand(i).isImm())
          return (SHAVECC::CondCode) MI.getOperand(i).getImm();
        break;
      }
    }
  }
  return SHAVECC::AL;
}

void SHAVEInstrInfo::setPredicateCC(MachineInstr& MI, SHAVECC::CondCode newCC) const {
  const MCInstrDesc& TID = MI.getDesc();
  assert(TID.isPredicable());

  if (TID.isPredicable()) {
    for (unsigned i = 0; i < MI.getNumOperands(); ++i) {
      if (TID.operands()[i].isPredicate()) {
        // First predicate operand is a predication mask
        if (MI.getOperand(i).isImm())
          MI.getOperand(i).setImm(newCC);
        break;
      }
    }
  }
}

bool SHAVEInstrInfo::isPredicated(const MachineInstr &MI) const {
  if (getPredicateCC(MI) != SHAVECC::AL)
    return true;

  // This function is also called later on, when predicate instructions
  // have already been generated and combined with the predicated
  // instructions in a bundle. Check for this type of predication here.
  for (MachineBasicBlock::const_instr_iterator I = MachineBasicBlock::const_instr_iterator(getBundleStart(MI.getIterator())),
        E = getBundleEnd(MI.getIterator()); I != E; ++I)
    // FIXME: Movidius - We should probably add some metadata to PEU instructions that don't predicate execution
    //                   rather than explicitly handling each individual case here
    if (GetFunctionalUnit(&(*I)) == SHAVE::PEU && I->getOpcode() != SHAVE::PEU_PVEN8C)
        return true;

  return false;
}

unsigned SHAVEInstrInfo::getPredicationCost(const MachineInstr &MI) const {
  // BRU_JMP has a latency of 1 cycle as per the tablegen specification, but
  // we need to inform the if converter that it will come with the 6 delay slots
  // as well
  if (MI.isBranch() || MI.isReturn())
    return getDelaySlots();

  return 0;
}

bool SHAVEInstrInfo::PredicateInstruction(MachineInstr &MI,
                                          ArrayRef<MachineOperand> Pred) const {
  // Pred comes from analyzeBranch "Cond" vector. There are two potential sizes, depending on the
  // branch pseudo used. The last two operands are always the CC and CCreg
  unsigned int idx = Pred.size() - 2;
  assert((Pred.size() == 2 || Pred.size() == 4) && Pred[idx].isImm() && Pred[idx+1].isReg() && "Unknown predicates!");

  int pidx = MI.findFirstPredOperandIdx();

  if (pidx != -1) {
    MI.getOperand(pidx).setImm(Pred[idx].getImm()); // set the mask
    MI.getOperand(pidx + 1).setReg(Pred[idx+1].getReg()); // set the CC reg
    return true;
  }

  return false;
}

// SubsumesPredicate - Returns true if the first specified predicate
// subsumes the second, e.g. GE subsumes GT.
bool SHAVEInstrInfo::SubsumesPredicate(ArrayRef<MachineOperand> Pred1,
                                       ArrayRef<MachineOperand> Pred2) const {
  // Pred comes from analyzeBranch "Cond" vector. There are two potential sizes, depending on the
  // branch pseudo used. The last two operands are always the CC and CCreg
  if ((Pred1.size() != 2 && Pred1.size() != 4) || (Pred2.size() != 2 && Pred2.size() != 4))
    return false;

  unsigned int idx1 = Pred1.size() - 2;
  unsigned int idx2 = Pred2.size() - 2;

  assert(Pred1[idx1].isImm() && Pred1[idx1+1].isReg() && Pred2[idx2].isImm() && Pred2[idx2+1].isReg()
          && "Unexpected predicate registers and masks!");

  // If predication is made upon two different registers, we cannot assert anything
  if (Pred1[idx1+1].getReg() == Pred2[idx2+1].getReg()) {
    SHAVECC::CondCode predMask1 = (SHAVECC::CondCode) Pred1[idx1].getImm();
    SHAVECC::CondCode predMask2 = (SHAVECC::CondCode) Pred2[idx2].getImm();

    if (predMask1 == SHAVECC::AL)
      return true; // Always execute subsumes all predicates
    else if ((predMask1 == SHAVECC::GTE) && ((predMask2 == SHAVECC::GT) || (predMask2 == SHAVECC::EQ)))
      return true;
    else if ((predMask1 == SHAVECC::LTE) && ((predMask2 == SHAVECC::LT) || (predMask2 == SHAVECC::EQ)))
      return true;
  }

  return false;
}

/// If the specified instruction defines any predicate
/// or condition code register(s) used for predication, returns true as well
/// as the definition predicate(s) by reference.
/// SkipDead should be set to false at any point that dead
/// predicate instructions should be considered as being defined.
/// A dead predicate instruction is one that is guaranteed to be removed
/// after a call to PredicateInstruction.
/// FIXME-LLVM12: Handle SkipDead
bool SHAVEInstrInfo::ClobbersPredicate(MachineInstr &MI, std::vector<MachineOperand> &Pred, bool SkipDead) const {
  // FIXME: Movidius - predication is currently done only on CC_CMU0, this will be expanded
  for (auto &Reg : get(MI.getOpcode()).implicit_defs())
    if (Reg == SHAVE::CC_CMU0)
      return true;

  return MI.isCall();
}

// isLegalToIfCvt - Return true if if-conversion does not endanger correctness of generated code
// i.e. it is not legal to if-convert call instructions, because the called function could
// (and has the right) to alter predication registers
bool SHAVEInstrInfo::isLegalToIfCvt(MachineBasicBlock &MBB) const {
  // count the number of the call instructions that can be found in the basic block
  unsigned callInstCount = 0;

  for (MachineBasicBlock::iterator instIt = MBB.begin(); instIt != MBB.end(); ++instIt)
    if (instIt->getOpcode() == SHAVE::BRU_SWP)
      ++callInstCount;

  if (callInstCount == 0)
    return true;  // It is safe to if-cvt blocks with no calls
  else if ((callInstCount == 1) && (MBB.getLastNonDebugInstr()->getOpcode() == SHAVE::BRU_SWP))
    return true;  // It is safe to if-cvt blocks with one call as the last instructions

  // Tt is not safe to if-cvt blocks with more than one call or with one call that is not
  // at the end of the basic block
  return false;
}

// isProfitableToIfCvt - Return true if it's profitable to predicate
// instructions with accumulated instruction latency of "NumCycles"
// of the specified basic block, where the probability of the instructions
// being executed is given by Probability, and Confidence is a measure
// of our confidence that it will be properly predicted.
bool SHAVEInstrInfo::isProfitableToIfCvt(MachineBasicBlock &MBB, unsigned NumCycles,
                                         unsigned ExtraPredCycles,
                                         BranchProbability Probability) const {
//  float prob = Probability.getNumerator() / float(Probability.getDenominator());
//  float expectedCyclesWhenPredicated = NumCycles / predictAverageILP(NumCycles);
//  float expectedCyclesWhenNotPredicated = ((NumCycles + 5) / predictAverageILP(NumCycles)) * prob;

    if (NumCycles <= ((unsigned) getDelaySlots() + 1) && Probability >= BranchProbability(1,2))
      return isLegalToIfCvt(MBB); // It is profitable to if cvt, check that it is also legal

    return false;
}

// isProfitableToIfCvt - Second variant of isProfitableToIfCvt, this one
// checks for the case where two basic blocks from true and false path
// of a if-then-else (diamond) are predicated on mutally exclusive
// predicates, where the probability of the true path being taken is given
// by Probability, and Confidence is a measure of our confidence that it
// will be properly predicted.
bool SHAVEInstrInfo::isProfitableToIfCvt(MachineBasicBlock &TMBB,
                                         unsigned NumTCycles, unsigned ExtraTCycles,
                                         MachineBasicBlock &FMBB,
                                         unsigned NumFCycles, unsigned ExtraFCycles,
                                         BranchProbability Probability) const {
  // After predication, the conditional branch of predecessor is removed so
  // add cycles of condition branch and delay slot conservatively.
  unsigned NumCondBrCycles = getDelaySlots() + 1;
  float prob = Probability.getNumerator() / float(Probability.getDenominator());
  float expectedCyclesWhenPredicated =
          (NumTCycles + NumFCycles) / predictAverageILP(NumTCycles+ NumFCycles);
  float expectedCyclesWhenNotPredicated =
          ((NumTCycles + NumCondBrCycles) / predictAverageILP(NumTCycles)) * prob +
          ((NumFCycles + NumCondBrCycles) / predictAverageILP(NumFCycles)) * (1 - prob);

  if (expectedCyclesWhenPredicated <= expectedCyclesWhenNotPredicated)
      return isLegalToIfCvt(TMBB) && isLegalToIfCvt(FMBB);  // It is profitable to if cvt, check that it is also legal

  return false;
}

// isProfitableToDupForIfCvt - Return true if it's profitable for
// if-converter to duplicate instructions of specified accumulated
// instruction latencies in the specified MBB to enable if-conversion.
// The probability of the instructions being executed is given by
// Probability, and Confidence is a measure of our confidence that it
// will be properly predicted.
bool SHAVEInstrInfo::isProfitableToDupForIfCvt(MachineBasicBlock &MBB, unsigned NumCycles,
                                               BranchProbability Probability) const {
  float prob = Probability.getNumerator() / float(Probability.getDenominator());
  float expectedCyclesWhenPredicated = (2 * NumCycles) / predictAverageILP(2 * NumCycles);
  float expectedCyclesWhenNotPredicated =
          (NumCycles + 5) / predictAverageILP(NumCycles) * prob
          + NumCycles / predictAverageILP(NumCycles);

  if (expectedCyclesWhenPredicated < expectedCyclesWhenNotPredicated)
    return isLegalToIfCvt(MBB); // It is profitable to if cvt, check that it is also legal

  return false;
}

bool SHAVEInstrInfo::reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const {
  // Cond comes from analyzeBranch "Cond" vector. There are two potential sizes, depending on the
  // branch pseudo used. The last two operands are always the CC and CCreg
  unsigned int idx = Cond.size() == 4 ? 2 : 0;
  auto condCode = (SHAVECC::CondCode) Cond[idx].getImm();
  if (Cond[idx+1].getReg() == SHAVE::CC_CMU0 && SHAVECC::canReverse(condCode)) {
    Cond[idx].setImm(SHAVECC::getOppositeCondition(condCode));
    return false;
  }

  return true;
}

// AnalyzeBranch - Analyze the branching code at the end of MBB, returning
// true if it cannot be understood (e.g. it's a switch dispatch or isn't
// implemented for a target).  Upon success, this returns false and returns
// with the following information in various cases:
//
// 1. If this block ends with no branches (it just falls through to its successor)
//    just return false, leaving TBB/FBB null.
// 2. If this block ends with only an unconditional branch, it sets TBB to be
//    the destination block.
// 3. If this block ends with a conditional branch and it falls through to a
//    successor block, it sets TBB to be the branch destination block and a
//    list of operands that evaluate the condition. These operands can be
//    passed to other TargetInstrInfo methods to create new branches.
// 4. If this block ends with a conditional branch followed by an
//    unconditional branch, it returns the 'true' destination in TBB, the
//    'false' destination in FBB, and a list of operands that evaluate the
//    condition.  These operands can be passed to other TargetInstrInfo
//    methods to create new branches.
//
// Note that RemoveBranch and InsertBranch must be implemented to support
// cases where this method returns success.
//
// If AllowModify is true, then this routine is allowed to modify the basic
// block (e.g. delete instructions after the unconditional branch).
//
bool SHAVEInstrInfo::analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                                   MachineBasicBlock *&FBB,
                                   SmallVectorImpl<MachineOperand> &Cond,
                                   bool AllowModify) const {
  // MBB.getParent()->viewCFGOnly();
  MachineBasicBlock::instr_iterator it;

  // Rebuild the mapping between BRU.JMP and it's target
  std::map<unsigned, MachineBasicBlock*> addressRegisters;
  std::map<MachineInstr*, MachineBasicBlock*> branchInstructions;

  for (it = MBB.instr_begin(); it != MBB.instr_end(); it++) {
    int opcode = it->getOpcode();
    MachineInstr* MI = &(*it);

    if ((opcode == SHAVE::BRU_RPI) || (opcode == SHAVE::BRU_RPIM))
      return true;
    else if (opcode == SHAVE::LSU_LDIH_Label)
      addressRegisters[MI->getOperand(0).getReg()] = MI->getOperand(2).getMBB();
    else if ((opcode == SHAVE::BRU_JMP) || (opcode == SHAVE::BRU_JMPcc)) {
      unsigned AddressReg = MI->getOperand(0).getReg();

      if (addressRegisters.find(AddressReg) == addressRegisters.end()) {
        // Cannot determine the target basic block for this branch (e.g.
        // for jump tables). Branches cannot be analyzed in this block.
        return true;
      }

      branchInstructions[MI] = addressRegisters[AddressReg];
    } else if ((opcode == SHAVE::BRU_JMP_Ret) || (opcode == SHAVE::BRU_SWIH_imm) || (opcode == SHAVE::BRU_SWIH_Ret)) {
      // Can't analyse predicated branches
      if (isPredicated(*MI)) {
        DEBUG(it->dump());
        return true;
      }
      branchInstructions[MI] = nullptr;
    } else if ((opcode == SHAVE::JMP_TO_LABEL) || (opcode == SHAVE::JMPcc_TO_LABEL) || (opcode == SHAVE::BRU_BRA)) {
      branchInstructions[MI] = it->getOperand(0).getMBB();
    } else if (opcode == SHAVE::JMPcc_TO_LABEL_PREEMPTION) {
      branchInstructions[MI] = it->getOperand(1).getMBB();
    }

    // FIXME: Movidius - this does not take into account the 'BRU.RP*' family of instructions, should it?  And what about 'BRU.SWP' and 'BRU.SWIC'?
  }

  MachineInstr *Branch1 = nullptr;
  MachineInstr *Branch2 = nullptr;
  MachineBasicBlock *Target1 = nullptr;
  MachineBasicBlock *Target2 = nullptr;

  if (branchInstructions.empty()) {
    // 1. This block ends with no branches
    Cond.clear();
    TBB = FBB = nullptr;
    return false;
  } else if (branchInstructions.size() == 1) {
    Branch1 = branchInstructions.begin()->first;
    Target1 = branchInstructions.begin()->second;

    if (!isPredicated(*Branch1) && Target1) {
      // 2. This block ends with an unconditional branch and we really know its target
      Cond.clear();
      TBB = Target1;
      FBB = nullptr;
      return false;
    }

    if (isPredicated(*Branch1) && Target1) {
      // 3. This block ends with a conditional branch and we really know its target
      TBB = Target1;
      FBB = nullptr;
      
      if (Branch1->getOpcode() == SHAVE::JMPcc_TO_LABEL_PREEMPTION) {
        Cond.push_back(Branch1->getOperand(0));
        Cond.push_back(Branch1->getOperand(2));
        Cond.push_back(Branch1->getOperand(3));
        Cond.push_back(Branch1->getOperand(4));
      }
      else {
        Cond.push_back(Branch1->getOperand(1));
        Cond.push_back(Branch1->getOperand(2));
      }

      return false;
    }
  } else if(branchInstructions.size() == 2) {
    Branch1 = branchInstructions.begin()->first;
    Branch2 = branchInstructions.rbegin()->first;
    Target1 = branchInstructions.begin()->second;
    Target2 = branchInstructions.rbegin()->second;

    if (isPredicated(*Branch2) || !isPredicated(*Branch1)) {
      std::swap(Branch1, Branch2);
      std::swap(Target1, Target2);
    }

    if (isPredicated(*Branch1) && !isPredicated(*Branch2)) {
      // 4. This block ends with a conditional branch followed by an unconditional one
      TBB = Target1;
      FBB = Target2;

      // Push the predication mask and the predicate register in the condition list
      if (Branch1->getOpcode() == SHAVE::JMPcc_TO_LABEL_PREEMPTION) {
        Cond.push_back(Branch1->getOperand(0));
        Cond.push_back(Branch1->getOperand(2));
        Cond.push_back(Branch1->getOperand(3));
        Cond.push_back(Branch1->getOperand(4));
      }
      else {
        Cond.push_back(Branch1->getOperand(1));
        Cond.push_back(Branch1->getOperand(2));
      }

      if (TBB != nullptr && FBB != nullptr)
        return false;
      else if (AllowModify && (FBB != nullptr)) {
        // Try to remove 'fallthrough' branches from BBs that cannot
        // be analyzed (e.g. they have a BRU_JMP_Ret instruction).
        // LLVM does not remove fallthrough branches from such blocks.
        MachineFunction::iterator PI = MachineFunction::iterator(MBB);
        MachineFunction::iterator I = std::next(PI);

        // FIXME: Movidius - seeing '&*' makes me nervous; comment or rewrite using an intermediate variable
        if (&*I == FBB)
          Branch2->eraseFromParent();
      }
    }
  } else {
    assert(false && "Expecting a maximum of 2 branches in a MBB!");
  }

  return true;
}

unsigned SHAVEInstrInfo::removeBranch(MachineBasicBlock &MBB, int *BytesRemoved) const {
  assert(!BytesRemoved && "SHAVEInstrInfo::removeBranch - 'BytesRemoved' not handled on SHAVE");

  // Make sure we don't remove any branch that cannot be analyzed like jump table branches.
  SmallVector<MachineOperand, 4> Cond;
#if 0 // Disabled for fixing Bugzilla #26,586
  MachineBasicBlock *TBB = nullptr;
  MachineBasicBlock *FBB = nullptr;

  if (analyzeBranch(MBB, TBB, FBB, Cond, false))
    return 0;
#endif

  // MBB.dump();
  unsigned removalCounter = 0;
  // Rebuild the mapping between BRU.JMP and it's target
  std::map<MachineInstr*, std::pair<MachineInstr*, MachineInstr*> > branchInstructions;
  std::set<unsigned> regForDelete;
  std::vector<MachineInstr*> instructionForDelete;

  for (MachineBasicBlock::reverse_iterator instRIt = MBB.rbegin(); instRIt != MBB.rend(); instRIt++) {
    if ((instRIt->getOpcode() == SHAVE::BRU_JMP) || (instRIt->getOpcode() == SHAVE::BRU_JMPcc)) {
      instructionForDelete.push_back(&(*instRIt));
      regForDelete.insert(instRIt->getOperand(0).getReg());
    } else if ((instRIt->getOpcode() == SHAVE::JMP_TO_LABEL) || (instRIt->getOpcode() == SHAVE::JMPcc_TO_LABEL) || 
               (instRIt->getOpcode() == SHAVE::JMPcc_TO_LABEL_PREEMPTION) || (instRIt->getOpcode() == SHAVE::BRU_JMP_Ret))
      instructionForDelete.push_back(&(*instRIt));
    else if (instRIt->getOpcode() == SHAVE::LSU_LDIH_Label && regForDelete.count(instRIt->getOperand(0).getReg())) {
      instructionForDelete.push_back(&(*instRIt));
      regForDelete.erase(instRIt->getOperand(0).getReg());
      regForDelete.insert(instRIt->getOperand(1).getReg());
    } else if (instRIt->getOpcode() == SHAVE::LSU_LDIL_Label && regForDelete.count(instRIt->getOperand(0).getReg())) {
      instructionForDelete.push_back(&(*instRIt));
      regForDelete.erase(instRIt->getOperand(0).getReg());
    }
  }

  removalCounter = instructionForDelete.size();

  for (unsigned instIdx = 0; instIdx < removalCounter; instIdx++)
    instructionForDelete[instIdx]->eraseFromParent();

  return removalCounter;
}


// InsertBranch - Insert branch code at the end of the specified
// MachineBasicBlock.  The operands to this method are the same as those
// returned by AnalyzeBranch.  This is only invoked in cases where
// AnalyzeBranch returns success.  It returns the number of instructions
// inserted.
//
// It is also invoked by tail merging to add unconditional branches in
// cases where AnalyzeBranch doesn't apply because there was no original
// branch to analyze.  At least this much must be implemented, else tail
// merging needs to be disabled.
unsigned SHAVEInstrInfo::insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                                      MachineBasicBlock *FBB,
                                      ArrayRef<MachineOperand> Cond,
                                      const DebugLoc &DL, int *BytesRemoved) const {
  assert(!BytesRemoved && "SHAVEInstrInfo::insertBranch - 'BytesRemoved' not handled on SHAVE");

  unsigned retVal = 0;

  if(Cond.empty() && TBB && !FBB) {
    assert(TBB->getParent() == MBB.getParent());
    finaliseMI(BuildMI(&MBB, DL, get(SHAVE::JMP_TO_LABEL)).addMBB(TBB));
    retVal = 1;
  } else if(!Cond.empty() && TBB && FBB) {
    assert((TBB->getParent() == MBB.getParent()) &&
            (FBB->getParent() == MBB.getParent()));
    if (Cond.size() == 4) {
      BuildMI(&MBB, DL, get(SHAVE::JMPcc_TO_LABEL_PREEMPTION), Cond[0].getReg()).addMBB(TBB)
          .add(Cond[1]).add(Cond[2]).add(Cond[3]);
    }
    else {
      BuildMI(&MBB, DL, get(SHAVE::JMPcc_TO_LABEL)).addMBB(TBB)
          .add(Cond[0]).add(Cond[1]);
    }
    finaliseMI(BuildMI(&MBB, DL, get(SHAVE::JMP_TO_LABEL)).addMBB(FBB));
    retVal = 2;
  } else if(!Cond.empty() && TBB  && !FBB) {
    assert(TBB->getParent() == MBB.getParent());
    if (Cond.size() == 4) {
      BuildMI(&MBB, DL, get(SHAVE::JMPcc_TO_LABEL_PREEMPTION), Cond[0].getReg()).addMBB(TBB)
          .add(Cond[1]).add(Cond[2]).add(Cond[3]);
    }
    else {
      BuildMI(&MBB, DL, get(SHAVE::JMPcc_TO_LABEL)).addMBB(TBB)
          .add(Cond[0]).add(Cond[1]);
    }
    retVal = 1;
  } else
    assert(false);

  return retVal;
}

bool SHAVEInstrInfo::isLegalToSplitMBBAt(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) const {
  // errs() << "IsLegalToSplitMBBAt\n";
  // assert(false);
  return false;
}

static unsigned getSETCCCompareOpcode(unsigned setCCInstr, unsigned mask) {
  bool Unsigned = SHAVECC::isUnsignedComparison((ISD::CondCode)mask);

  switch (setCCInstr) {
  // scalar comparisons
  case SHAVE::SHAVE_SETCC_i32:
    return Unsigned ? SHAVE::CMU_CMII_u32 : SHAVE::CMU_CMII_i32;
  case SHAVE::SHAVE_SETCC_i16:
    return Unsigned ? SHAVE::CMU_CMII_u16 : SHAVE::CMU_CMII_i16;
  case SHAVE::SHAVE_SETCC_i8:
    return Unsigned ? SHAVE::CMU_CMII_u8 : SHAVE::CMU_CMII_i8;
  case SHAVE::SHAVE_SETCC_f32:
      return SHAVE::CMU_CMII_f32;
  case SHAVE::SHAVE_SETCC_f16:
      return SHAVE::CMU_CMII_f16;

  // vector comparisons
  // Myriad4
  // 512-bit vectors
  case SHAVE::SHAVE_SETCC_v16i32_v16i32_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v16i32_unsigned_Myr4 : SHAVE::CMU_CMVV_v16i32_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v32i16_v32i16_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v32i16_unsigned_Myr4 : SHAVE::CMU_CMVV_v32i16_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v64i8_v64i8_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v64i8_unsigned_Myr4 : SHAVE::CMU_CMVV_v64i8_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v16f32_v16i32_Myr4:
      return SHAVE::CMU_CMVV_v16f32_Myr4;
  case SHAVE::SHAVE_SETCC_v32f16_v32i16_Myr4:
      return SHAVE::CMU_CMVV_v32f16_Myr4;
    // 256-bit vectors
  case SHAVE::SHAVE_SETCC_v8i32_v8i32_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v8i32_unsigned_Myr4 : SHAVE::CMU_CMVV_v8i32_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v16i16_v16i16_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v16i16_unsigned_Myr4 : SHAVE::CMU_CMVV_v16i16_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v32i8_v32i8_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v32i8_unsigned_Myr4 : SHAVE::CMU_CMVV_v32i8_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v8f32_v8i32_Myr4:
      return SHAVE::CMU_CMVV_v8f32_Myr4;
  case SHAVE::SHAVE_SETCC_v16f16_v16i16_Myr4:
      return SHAVE::CMU_CMVV_v16f16_Myr4;
    // 128-bit vectors
  case SHAVE::SHAVE_SETCC_v4i32_v4i32_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v4i32_unsigned_Myr4 : SHAVE::CMU_CMVV_v4i32_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v8i16_v8i16_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v8i16_unsigned_Myr4 : SHAVE::CMU_CMVV_v8i16_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v16i8_v16i8_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v16i8_unsigned_Myr4 : SHAVE::CMU_CMVV_v16i8_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v4f32_v4i32_Myr4:
      return SHAVE::CMU_CMVV_v4f32_Myr4;
  case SHAVE::SHAVE_SETCC_v8f16_v8i16_Myr4:
      return SHAVE::CMU_CMVV_v8f16_Myr4;
    // 64-bit
  case SHAVE::SHAVE_SETCC_v2i32_v2i32_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v2i32_unsigned_Myr4 : SHAVE::CMU_CMVV_v2i32_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v4i16_v4i16_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v4i16_unsigned_Myr4 : SHAVE::CMU_CMVV_v4i16_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v8i8_v8i8_Myr4:
      return Unsigned ? SHAVE::CMU_CMVV_v8i8_unsigned_Myr4 : SHAVE::CMU_CMVV_v8i8_signed_Myr4;
  case SHAVE::SHAVE_SETCC_v2f32_v2i32_Myr4:
      return SHAVE::CMU_CMVV_v2f32_Myr4;
  case SHAVE::SHAVE_SETCC_v4f16_v4i16_Myr4:
      return SHAVE::CMU_CMVV_v4f16_Myr4;
  // Myriad2
  case SHAVE::SHAVE_SETCC_v4i32_v4i32:
  case SHAVE::SHAVE_SETCC_v2i32_v2i32:
      return Unsigned ? SHAVE::CMU_CMVV_u32 : SHAVE::CMU_CMVV_i32;
  case SHAVE::SHAVE_SETCC_v8i16_v8i16:
  case SHAVE::SHAVE_SETCC_v4i16_v4i16:
      return Unsigned ? SHAVE::CMU_CMVV_u16 : SHAVE::CMU_CMVV_i16;
  case SHAVE::SHAVE_SETCC_v16i8_v16i8:
    return Unsigned ? SHAVE::CMU_CMVV_u8 : SHAVE::CMU_CMVV_i8;
  case SHAVE::SHAVE_SETCC_v8i8_v8i8:
    return Unsigned ? SHAVE::CMU_CMVV_v8u8 : SHAVE::CMU_CMVV_v8i8;
  case SHAVE::SHAVE_SETCC_v2i16_v2i16:
    return Unsigned ? SHAVE::CMU_CMII_v2u16 : SHAVE::CMU_CMII_v2i16;
  case SHAVE::SHAVE_SETCC_v4i8_v4i8:
    return Unsigned ? SHAVE::CMU_CMII_v4u8 : SHAVE::CMU_CMII_v4i8;
  case SHAVE::SHAVE_SETCC_v2i8_v2i8:
    return Unsigned ? SHAVE::CMU_CMII_v2u8 : SHAVE::CMU_CMII_v2i8;

  case SHAVE::SHAVE_SETCC_v4f32_v4i32:
    return SHAVE::CMU_CMVV_f32;
  case SHAVE::SHAVE_SETCC_v2f32_v2i32:
    return SHAVE::CMU_CMVV_v2f32;
  case SHAVE::SHAVE_SETCC_v8f16_v8i16:
    return SHAVE::CMU_CMVV_f16;
  case SHAVE::SHAVE_SETCC_v4f16_v4i16:
    return SHAVE::CMU_CMVV_v4f16;
  case SHAVE::SHAVE_SETCC_v2f16_v2i16:
    return SHAVE::CMU_CMII_v2f16;

  default:
    llvm_unreachable("Cannot select comparison instruction");
  }
}

void SHAVEInstrInfo::ExpandMaskedStore(MachineBasicBlock &BB,
                                       MachineBasicBlock::iterator MI) const {
  DebugLoc dl = MI->getDebugLoc();
  MachineOperand value = MI->getOperand(0);
  MachineOperand pointer = MI->getOperand(1);
  MachineOperand predicate = MI->getOperand(2);
  MachineOperand predicateRegister = MI->getOperand(3);
  unsigned int originalOpcode = MI->getOpcode();

  if (originalOpcode == SHAVE::MASKED_STORE_WVRF_l || originalOpcode == SHAVE::MASKED_STORE_WVRF_h || originalOpcode == SHAVE::MASKED_STORE_WVRF) {
    unsigned int opcode = (originalOpcode == SHAVE::MASKED_STORE_WVRF_h) ? SHAVE::LSU_STOV512_h : SHAVE::LSU_STV512_l;

    MachineInstrBuilder store = BuildMI(BB, MI, dl, get(opcode));
    store.add(value);
    store.add(pointer);
    if (opcode == SHAVE::LSU_STOV512_h)
      store.addImm(32);

    store.add(predicate);
    store.add(predicateRegister);
    store.addImm(SHAVE::LSU0);

    return;
  }

  unsigned int opcode = 0;
  
  if (originalOpcode == SHAVE::MASKED_STORE_VRF)
    opcode = SHAVE::LSU_STXV_masked_Myr2v3;
  else if (MI->getOpcode() == SHAVE::MASKED_STORE_IRF)
    opcode = SHAVE::LSU_ST32_Raw;

  MachineInstrBuilder store = BuildMI(BB, MI, dl, get(opcode));

  store.add(value);
  store.add(pointer);

  store.add(predicate);
  store.add(predicateRegister);
  store.addImm(SHAVE::LSU0);
}

bool SHAVEInstrInfo::ExpandSelect(MachineBasicBlock &BB,
                                  MachineBasicBlock::iterator MI) const {
  DebugLoc dl = MI->getDebugLoc();
  MachineOperand dst = MI->getOperand(0);
  MachineOperand tval = MI->getOperand(1);
  MachineOperand fval = MI->getOperand(2);
  MachineOperand mask = MI->getOperand(3);

  unsigned PredReg = 0;
  bool ScalarSelect = true;
  bool IRFSelect = false;

  switch (MI->getOpcode()) {
  case SHAVE::SELECT_IRF64:
  case SHAVE::SELECT_IRF32:
  case SHAVE::SELECT_IRF16:
  case SHAVE::SELECT_IRF8:
  case SHAVE::SELECT_WVRF512:
  case SHAVE::SELECT_WVRF256_0:
  case SHAVE::SELECT_WVRF128_0:
  case SHAVE::SELECT_WVRF64_0:
  case SHAVE::SELECT_VRF128:
  case SHAVE::SELECT_VRF64_v2i32:
  case SHAVE::SELECT_VRF64_v2f32:
  case SHAVE::SELECT_VRF64_v4i16:
  case SHAVE::SELECT_VRF64_v4f16:
  case SHAVE::SELECT_VRF64_v8i8:
  case SHAVE::SELECT_IRF32_v2f16:
  case SHAVE::SELECT_IRF32_v2i16:
    PredReg = SHAVE::CC_CMU0;
    break;
  case SHAVE::VSELECT_WVRF512_v16:
  case SHAVE::VSELECT_WVRF256_0_v8:
  case SHAVE::VSELECT_WVRF128_0_v4:
  case SHAVE::VSELECT_WVRF64_0_v2:
  case SHAVE::VSELECT_VRF128_v4:
  case SHAVE::VSELECT_VRF64_v2:
    PredReg = SHAVE::C_CMU_0_3;
    ScalarSelect = false;
    break;
  case SHAVE::VSELECT_IRF32_v2:
  case SHAVE::VSELECT_IRF16_v2:
    IRFSelect = true;
    LLVM_FALLTHROUGH;
  case SHAVE::VSELECT_WVRF512_v32:
  case SHAVE::VSELECT_WVRF256_0_v16:
  case SHAVE::VSELECT_WVRF128_0_v8:
  case SHAVE::VSELECT_WVRF64_0_v4:
  case SHAVE::VSELECT_VRF128_v8:
  case SHAVE::VSELECT_VRF64_v4:
    PredReg = SHAVE::C_CMU0;
    ScalarSelect = false;
    break;
  case SHAVE::VSELECT_IRF32_v4:
    IRFSelect = true;
    LLVM_FALLTHROUGH;
  case SHAVE::VSELECT_WVRF512_v64:
  case SHAVE::VSELECT_WVRF256_0_v32:
  case SHAVE::VSELECT_WVRF128_0_v16:
  case SHAVE::VSELECT_WVRF64_0_v8:
  case SHAVE::VSELECT_VRF128_v16:
  case SHAVE::VSELECT_VRF64_v8:
    PredReg = SHAVE::C_CMU_0_15;
    ScalarSelect = false;
    break;
  default:
    llvm_unreachable("Unknown SELECT/VSELECT instruction");
    return false;
  }

  MachineBasicBlock::iterator it = MI;
  ++it;

  SHAVECC::CondCode realMask = (SHAVECC::CondCode)mask.getImm();

  if (ScalarSelect) {
    // Copy the false value to dst unconditionally.
    if (dst.getReg() != fval.getReg())
      copyPhysReg(BB, it, dl, dst.getReg(), fval.getReg(), fval.isKill());

    // Copy the true value to dst if the condition is true.
    copyPhysReg(BB, it, dl, dst.getReg(), tval.getReg(), tval.isKill());
  } else {
    // Vector select requires a VAU/SAU instruction because of PEU.PVV.
    if (hasFeature(SHAVE::HasVRF512_Feature) && !IRFSelect) {
      // There is no VAU.CMBWORD on Myriad4v0 so instead we use a VAU.OR of the source register with itself
      MachineInstrBuilder instr = BuildMI(BB, it, dl, get(SHAVE::VAU_OR_bitwise_512_v16i32_Myr4), dst.getReg());

      assert(dst.getReg() == fval.getReg() && "Missing operand constraint?");

      instr.addReg(tval.getReg());
      instr.addReg(tval.getReg(), getKillRegState(tval.isKill()));

      instr.addImm(realMask);
      instr.addReg(PredReg);
    }
    else {
      unsigned SelectOpc = IRFSelect ? SHAVE::SAU_CMBBYTE_imm_IRF : SHAVE::VAU_CMBWORD_imm_vrf;
      MachineInstrBuilder MIB = BuildMI(BB, it, dl, get(SelectOpc), dst.getReg());

      assert(dst.getReg() == fval.getReg() && "Missing operand constraint?");

      MIB.addReg(fval.getReg(), getKillRegState(fval.isKill()));
      MIB.addReg(tval.getReg(), getKillRegState(tval.isKill()));

      for (int i = 0; i < 4; i++)
        MIB.addImm(1);

      MIB.addImm(realMask);
      MIB.addReg(PredReg);
    }
  }

  // Make the second copy predicated on the comparison of LHS and RHS.
  MachineBasicBlock::iterator predCopyIt = it;
  --predCopyIt;

  int predOpIdx = predCopyIt->findFirstPredOperandIdx();

  assert(predOpIdx >= 0 && "Copy instruction not predicable?");

  predCopyIt->getOperand(predOpIdx).setImm(realMask);
  predCopyIt->getOperand(predOpIdx+1).setReg(PredReg);
  return true;
}

void SHAVEInstrInfo::ExpandSetCC(MachineBasicBlock &BB,
                                 MachineBasicBlock::iterator MI) const {
  DebugLoc dbgLoc = MI->getDebugLoc();

  MachineBasicBlock::iterator it = MI;
  ++it;

  MachineOperand dst = MI->getOperand(0);
  MachineOperand lhs = MI->getOperand(1);
  MachineOperand rhs = MI->getOperand(2);
  MachineOperand mask = MI->getOperand(3);
  unsigned DstReg = dst.getReg();

  // Get the compare opcode based on operand types
  ISD::CondCode CompareCC = (ISD::CondCode)mask.getImm();
  unsigned compOpc = getSETCCCompareOpcode(MI->getOpcode(), CompareCC);

  // Update the predicate mask with target mask
  SHAVECC::CondCode CompareTCC = SHAVECC::getSHAVECondCode(CompareCC, SHAVEConflicts::check_isFloatingPointSETCC(MI->getOpcode()));

  mask.setImm(CompareTCC);

  uint64_t falseBC = 1;

  if (SHAVEST.getTargetLowering()->getBooleanContents(false, false) == SHAVEST.getTargetLowering()->ZeroOrOneBooleanContent)
    falseBC = 0;
  else
    assert(false && "Boolean content not supported");

  assert(SHAVE::IRF32RegClass.contains(DstReg) && "Unsupported SETCC type");
  finaliseMI(BuildMI(BB, it, dbgLoc, get(compOpc))
      .add(lhs).add(rhs)
      .addReg(SHAVE::CC_CMU0, RegState::ImplicitDefine));
  finaliseMI(BuildMI(BB, it, dbgLoc, get(SHAVE::LSU_LDIL), DstReg)
      .addImm(falseBC));

  BuildMI(BB, it, dbgLoc, get(SHAVE::IAU_INCS_i32), DstReg)
      .addReg(DstReg).addImm(1)
      .addImm(CompareTCC).addReg(SHAVE::CC_CMU0);
}

void SHAVEInstrInfo::ExpandSetCCVector(MachineBasicBlock &BB,
                                       MachineBasicBlock::iterator MI) const {
  DebugLoc dbgLoc = MI->getDebugLoc();
  int Opc = MI->getOpcode();
  unsigned CCReg = 0;

  switch (Opc) {
  default:
    llvm_unreachable("Unhandled SetCC opcode");
    return;
  // Myriad4
  case SHAVE::SHAVE_SETCC_v16i32_v16i32_Myr4:
  case SHAVE::SHAVE_SETCC_v16f32_v16i32_Myr4:
  case SHAVE::SHAVE_SETCC_v8i32_v8i32_Myr4:
  case SHAVE::SHAVE_SETCC_v8f32_v8i32_Myr4:
  case SHAVE::SHAVE_SETCC_v4i32_v4i32_Myr4:
  case SHAVE::SHAVE_SETCC_v4f32_v4i32_Myr4:
  case SHAVE::SHAVE_SETCC_v2i32_v2i32_Myr4:
  case SHAVE::SHAVE_SETCC_v2f32_v2i32_Myr4:
  // Myriad2
  case SHAVE::SHAVE_SETCC_v4i32_v4i32:
  case SHAVE::SHAVE_SETCC_v4f32_v4i32:
  case SHAVE::SHAVE_SETCC_v2i32_v2i32:
  case SHAVE::SHAVE_SETCC_v2f32_v2i32:
    CCReg = SHAVE::C_CMU_0_3;
    break;
  // Myriad4
  case SHAVE::SHAVE_SETCC_v32i16_v32i16_Myr4:
  case SHAVE::SHAVE_SETCC_v32f16_v32i16_Myr4:
  case SHAVE::SHAVE_SETCC_v16i16_v16i16_Myr4:
  case SHAVE::SHAVE_SETCC_v16f16_v16i16_Myr4:
  case SHAVE::SHAVE_SETCC_v8i16_v8i16_Myr4:
  case SHAVE::SHAVE_SETCC_v8f16_v8i16_Myr4:
  case SHAVE::SHAVE_SETCC_v4i16_v4i16_Myr4:
  case SHAVE::SHAVE_SETCC_v4f16_v4i16_Myr4:
  // Myriad2
  case SHAVE::SHAVE_SETCC_v8i16_v8i16:
  case SHAVE::SHAVE_SETCC_v4i16_v4i16:
  case SHAVE::SHAVE_SETCC_v2i16_v2i16:
  case SHAVE::SHAVE_SETCC_v8f16_v8i16:
  case SHAVE::SHAVE_SETCC_v4f16_v4i16:
  case SHAVE::SHAVE_SETCC_v2f16_v2i16:
    CCReg = SHAVE::C_CMU0;
    break;
  // Myriad4
  case SHAVE::SHAVE_SETCC_v64i8_v64i8_Myr4:
  case SHAVE::SHAVE_SETCC_v32i8_v32i8_Myr4:
  case SHAVE::SHAVE_SETCC_v16i8_v16i8_Myr4:
  case SHAVE::SHAVE_SETCC_v8i8_v8i8_Myr4:
  // Myriad2
  case SHAVE::SHAVE_SETCC_v16i8_v16i8:
  case SHAVE::SHAVE_SETCC_v8i8_v8i8:
  case SHAVE::SHAVE_SETCC_v4i8_v4i8:
  case SHAVE::SHAVE_SETCC_v2i8_v2i8:
    CCReg = SHAVE::C_CMU_0_15;
    break;
  }

  MachineBasicBlock::iterator it = MI;
  ++it;

  MachineOperand Dst = MI->getOperand(0);
  MachineOperand LHS = MI->getOperand(1);
  MachineOperand RHS = MI->getOperand(2);
  MachineOperand Mask = MI->getOperand(3);
  unsigned DstReg = Dst.getReg();

  // Get the compare opcode based on operand types
  unsigned CompareOpc = getSETCCCompareOpcode(MI->getOpcode(), Mask.getImm());
  unsigned bitwisenotOpcode;

  // Update the predicate mask with target mask
  Mask.setImm(SHAVECC::getSHAVECondCode((ISD::CondCode)Mask.getImm(),
                                        SHAVEConflicts::check_isFloatingPointSETCC(MI->getOpcode())));

  finaliseMI(BuildMI(BB, it, dbgLoc, get(CompareOpc))
      .add(LHS).add(RHS)
      .addReg(CCReg, RegState::ImplicitDefine));

  switch (Opc) {
  default:
    llvm_unreachable("Unhandled SetCC opcode");
    return;
  // Myriad4
  // 512-bit vectors
  case SHAVE::SHAVE_SETCC_v16i32_v16i32_Myr4:
  case SHAVE::SHAVE_SETCC_v32i16_v32i16_Myr4:
  case SHAVE::SHAVE_SETCC_v64i8_v64i8_Myr4:
  case SHAVE::SHAVE_SETCC_v16f32_v16i32_Myr4:
  case SHAVE::SHAVE_SETCC_v32f16_v32i16_Myr4:
    finaliseMI(BuildMI(BB, it, dbgLoc, get(SHAVE::VAU_XOR_bitwise_512_v16i32_Myr4), DstReg).addReg(DstReg).addReg(DstReg));
    BuildMI(BB, it, dbgLoc, get(SHAVE::VAU_NOT_v16i32_Myr4), DstReg).addReg(DstReg).add(Mask).addReg(CCReg);
    break;
  // 256-bit vectors
  case SHAVE::SHAVE_SETCC_v8i32_v8i32_Myr4:
  case SHAVE::SHAVE_SETCC_v16i16_v16i16_Myr4:
  case SHAVE::SHAVE_SETCC_v32i8_v32i8_Myr4:
  case SHAVE::SHAVE_SETCC_v8f32_v8i32_Myr4:
  case SHAVE::SHAVE_SETCC_v16f16_v16i16_Myr4:
    finaliseMI(BuildMI(BB, it, dbgLoc, get(SHAVE::VAU_XOR_bitwise_256_v8i32_Myr4), DstReg).addReg(DstReg).addReg(DstReg));
    BuildMI(BB, it, dbgLoc, get(SHAVE::VAU_NOT_v8i32_Myr4), DstReg).addReg(DstReg).add(Mask).addReg(CCReg);
    break;
  // 128-bit vectors
  case SHAVE::SHAVE_SETCC_v4i32_v4i32_Myr4:
  case SHAVE::SHAVE_SETCC_v8i16_v8i16_Myr4:
  case SHAVE::SHAVE_SETCC_v16i8_v16i8_Myr4:
  case SHAVE::SHAVE_SETCC_v4f32_v4i32_Myr4:
  case SHAVE::SHAVE_SETCC_v8f16_v8i16_Myr4:
    finaliseMI(BuildMI(BB, it, dbgLoc, get(SHAVE::VAU_XOR_bitwise_128_v4i32_Myr4), DstReg).addReg(DstReg).addReg(DstReg));
    BuildMI(BB, it, dbgLoc, get(SHAVE::VAU_NOT_v4i32_Myr4), DstReg).addReg(DstReg).add(Mask).addReg(CCReg);
    break;
  // 64-bit
  case SHAVE::SHAVE_SETCC_v2i32_v2i32_Myr4:
  case SHAVE::SHAVE_SETCC_v4i16_v4i16_Myr4:
  case SHAVE::SHAVE_SETCC_v8i8_v8i8_Myr4:
  case SHAVE::SHAVE_SETCC_v2f32_v2i32_Myr4:
  case SHAVE::SHAVE_SETCC_v4f16_v4i16_Myr4:
    finaliseMI(BuildMI(BB, it, dbgLoc, get(SHAVE::VAU_XOR_bitwise_64_v2i32_Myr4), DstReg).addReg(DstReg).addReg(DstReg));
    BuildMI(BB, it, dbgLoc, get(SHAVE::VAU_NOT_v2i32_Myr4), DstReg).addReg(DstReg).add(Mask).addReg(CCReg);
    break;
  // Myriad2
  case SHAVE::SHAVE_SETCC_v4i32_v4i32:
  case SHAVE::SHAVE_SETCC_v4f32_v4i32:
  case SHAVE::SHAVE_SETCC_v8i16_v8i16:
  case SHAVE::SHAVE_SETCC_v8f16_v8i16:
  case SHAVE::SHAVE_SETCC_v16i8_v16i8:
    finaliseMI(BuildMI(BB, it, dbgLoc, get(SHAVE::CMU_CPZV), DstReg).addImm(0));
    BuildMI(BB, it, dbgLoc, get(SHAVE::VAU_NOT), DstReg).addReg(DstReg).add(Mask).addReg(CCReg);
    break;
  case SHAVE::SHAVE_SETCC_v2i32_v2i32:
  case SHAVE::SHAVE_SETCC_v2f32_v2i32:
  case SHAVE::SHAVE_SETCC_v4i16_v4i16:
  case SHAVE::SHAVE_SETCC_v4f16_v4i16:
  case SHAVE::SHAVE_SETCC_v8i8_v8i8:
    finaliseMI(BuildMI(BB, it, dbgLoc, get(SHAVE::CMU_CPZV_VRF64_l), DstReg).addImm(0));
    BuildMI(BB, it, dbgLoc, get(SHAVE::VAU_NOT_v2i32), DstReg).addReg(DstReg).add(Mask).addReg(CCReg);
    break;
  case SHAVE::SHAVE_SETCC_v2i16_v2i16:
  case SHAVE::SHAVE_SETCC_v2f16_v2i16:
  case SHAVE::SHAVE_SETCC_v4i8_v4i8:
    bitwisenotOpcode = SHAVE::SAU_NOT_i32;
    finaliseMI(BuildMI(BB, it, dbgLoc, get(SHAVE::IAU_XOR_32), DstReg).addReg(DstReg).addReg(DstReg));
    BuildMI(BB, it, dbgLoc, get(bitwisenotOpcode), DstReg).addReg(DstReg).add(Mask).addReg(CCReg);
    break;
  case SHAVE::SHAVE_SETCC_v2i8_v2i8:
    bitwisenotOpcode = SHAVE::SAU_NOT_i16;
    finaliseMI(BuildMI(BB, it, dbgLoc, get(SHAVE::IAU_XOR_16), DstReg).addReg(DstReg).addReg(DstReg));
    BuildMI(BB, it, dbgLoc, get(bitwisenotOpcode), DstReg).addReg(DstReg).add(Mask).addReg(CCReg);
    break;
  }
}

bool SHAVEInstrInfo::tryUseRPI(MachineBasicBlock &BB, MachineInstr * branchInstruction) const {
  DebugLoc dbgLoc = branchInstruction->getDebugLoc();
  MachineInstr * CMUInstruction = nullptr;
  MachineInstr * iteratorInstruction = nullptr;

  // Do not attempt to put inline assembly or pseudo instructions into a BRU.RPI bundle
  for (auto &MI : BB)
    if (MI.isPseudo() || MI.isInlineAsm())
      return false;

  // Start by finding the CMU instruction predicating the branch back to the start of this block
  for (auto &MI : BB) {
    unsigned FUnit = GetFunctionalUnit(MI.getOpcode());
    if (FUnit == SHAVE::CMU) {
      if (CMUInstruction == nullptr)
        CMUInstruction = &MI;
      else {
        return false;
      }
    }
  }

  if (!CMUInstruction) {
    return false;
  }

  // Next, find the instruction which updates the iterator for this loop (stored in iteratorRegiser)
  unsigned iteratorRegister = 0;
  if (CMUInstruction->getOpcode() == SHAVE::CMU_CMZ_i32)
    iteratorRegister = CMUInstruction->getOperand(0).getReg();
  else
    return false;

  for (auto &MI : BB)
    if (MI.definesRegister(iteratorRegister))
      iteratorInstruction = &MI;

  if (!iteratorInstruction)
    return false;

  // Check that each functional unit is used only once in the loop
  // Exclude the "CMUInstruction", "iteratorInstruction" and any branch instructions
  // Since we will be packing all instructions in this block (excluding any additional branches) into
  // a single bundle, we must ensure that each functional unit is used only once
  FUnitMask loopFUMask = FmNone;
  for (auto &MI : BB) {
    unsigned FUnit = GetFunctionalUnit(&MI);
    if (FUnit != SHAVE::NONE && &MI != iteratorInstruction && &MI != CMUInstruction
        && MI.getOpcode() != SHAVE::JMP_TO_LABEL && MI.getOpcode() != SHAVE::JMPcc_TO_LABEL && MI.getOpcode() != SHAVE::JMPcc_TO_LABEL_PREEMPTION) {
      FUnitMask currentFUMask = 0;
      // We must maintain the order of Load/Store instructions, so only allow one store/load
      // instruction in a BRU.RPI loop.
      // FIXME: Movidius - Is memory alias information available here? If so, is there a better way to do this?
      if ((FUnit == SHAVE::LSU0 || FUnit == SHAVE::LSU1) && (SHAVEConflicts::check_isStore(MI.getOpcode()) || SHAVEConflicts::check_isLoad(MI.getOpcode())))
        currentFUMask = (1 << (SHAVE::LSU0 - 1)) | (1 << (SHAVE::LSU1 - 1));
      else
        currentFUMask = 1 << (FUnit - 1);

      if (loopFUMask & currentFUMask)
        return false;
      loopFUMask |= currentFUMask;
    }
  }

  bool foundOtherInstrs = false;
  // If any instruction uses the result of any other instruction in this loop
  // then we cannot use BRU.RPI
  for (auto &MI : BB) {
    if (&MI != iteratorInstruction && &MI != CMUInstruction && &MI != branchInstruction) {
      foundOtherInstrs = true;
      for (auto &MO : MI.operands()) {
        if (MO.isReg() && MO.isDef()) {
          for (auto &checkMI : BB) {
            if (&checkMI != &MI && checkMI.readsRegister(MO.getReg()) && &checkMI != iteratorInstruction && &checkMI != CMUInstruction) {
              return false;
            }
          }
        }
      }
    }
  }

  // If there are no other instructions in this loop other than the iterator compare and update
  // there is no point in optimising to use BRU.RPI. This type of loop should be replaced with
  // an immediate load
  if (!foundOtherInstrs)
    return false;

  // Ensure all instructions have a write-back latency of 0 cycles, with the following exceptions:
  //     The "iteratorInstruction" and "CMUInstruction" are being removed so we do not care what their latencies are
  //     Any branch instructions, since they will either be excluded from the bundle or are the original branch being replaced
  //     Any store instructions
  for (auto &MI : BB)
    if (&MI != iteratorInstruction && &MI != CMUInstruction && &MI != branchInstruction && MI.getOpcode() != SHAVE::JMP_TO_LABEL && !SHAVEConflicts::check_isStore(MI.getOpcode()))
      if (GetSchedMaxLatency(&MI) > 0)
        return false;

  if (iteratorInstruction->getOpcode() == SHAVE::IAU_INCS_i32){
    if (iteratorInstruction->getOperand(0).getReg() != iteratorRegister)
      return false;
    if (iteratorInstruction->getOperand(2).getImm() != -1)
      return false;
  }
  else
    return false;

  // If we reach here, then it is safe to transform this loop to use BRU.RPI
  DEBUG(dbgs() << "SHAVEInstrInfo: Transforming loop " << BB.getFullName() << " to use BRU.RPI\n");

  // Start by removing the "iteratorInstruction" and "CMUInstruction"
  DEBUG(dbgs() << "SHAVEInstrInfo: Removing instruction " << *iteratorInstruction
               << "SHAVEInstrInfo: Removing instruction " << *CMUInstruction);
  iteratorInstruction->eraseFromParent();
  CMUInstruction->eraseFromParent();

  // First remove (not erase) all instructions that are not branch instructions
  SmallVector<MachineInstr *, 4> bundleInstructions;
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &MI : BB) {
      if (MI.getOpcode() != SHAVE::JMP_TO_LABEL && MI.getOpcode() != SHAVE::JMPcc_TO_LABEL && MI.getOpcode() != SHAVE::JMPcc_TO_LABEL_PREEMPTION
          && !MI.isDebugValue() && !MI.isLabel() && !MI.isPosition()) {
        MI.removeFromParent();
        bundleInstructions.push_back(&MI);
        changed = true;
        break;
      }
    }
  }

  // Insert the BRU.RPI instruction at the beginning of the block
  finaliseMI(BuildMI(BB, BB.begin(), dbgLoc, get(SHAVE::BRU_RPI)).addMBB(&BB).addReg(iteratorRegister));

  // Then re-insert all other instructions at the beginning of the basic block. This ensures the BRU.RPI is
  // the last instruction in the basic block (which is still required at this point in compilation)
  bool foundUnconditionalBranch = false;
  for (auto MI : bundleInstructions) {
    if (MI->getOpcode() != SHAVE::JMP_TO_LABEL) {
      BB.insert(BB.begin(), MI);
      MI->bundleWithSucc();
    }
    else
      foundUnconditionalBranch = true;
  }

  // Insert a header before the BRU.RPI bundle. This is required for the special handling of 0 iterations
  // with BRU.RPI. In the event that iteratorRegister has an incoming value of 0, the BRU.RPI bundle would
  // loop indefinitely, instead of the expected 2^32 iterations.
  // The header is as follows:
  //     CMU.CMZ iteratorRegister
  //     PEU.PC1C EQ
  //       || IAU.INCS iteratorRegister -1
  //     PEU.PC1C EQ
  //       || bundle instructions
  // The two "PEU.PC1C EQ" predicated bundles may be merged if the bundle of instructions does not contain
  // an IAU instruction

  // Insert a copy of the bundle instructions
  for (auto MI : bundleInstructions) {
    MachineInstrBuilder bundleInstr = BuildMI(BB, BB.begin(), dbgLoc, get(MI->getOpcode()));
    for (unsigned int i = 0; i < MI->getNumOperands(); i++) {
      const MCOperandInfo &MCOI = MI->getDesc().operands()[i];
      MachineOperand MO = MI->getOperand(i);

      // Some instructions implicity define/use certain TRF registers by default
      // We should skip over these registers when copying operands so that they are not included twice
      unsigned regNumber = MO.isReg() ? (unsigned) MO.getReg() : 0;
      if (regNumber && (SHAVE::PEU_TRFRegClass.contains(regNumber) || SHAVE::TRFRegClass.contains(regNumber))) {
        if (MO.isDef() && bundleInstr.getInstr()->definesRegister(regNumber))
          continue;
        if (MO.isUse() && bundleInstr.getInstr()->readsRegister(regNumber))
          continue;
      }

      // Replace the predicate operands (AL, noreg) with (EQ, CC_CMU0)
      if (MCOI.isPredicate()) {
        if (MO.isImm())
          bundleInstr = bundleInstr.addImm(SHAVECC::EQ);
        else
          bundleInstr = bundleInstr.addReg(SHAVE::CC_CMU0);
      }
      else
        bundleInstr = bundleInstr.add(MO);
    }
  }

  // Insert the IAU.INCS of the iteratorRegister
  BuildMI(BB, BB.begin(), dbgLoc, get(SHAVE::IAU_INCS_i32)).addReg(iteratorRegister, RegState::Define).addReg(iteratorRegister)
                                                           .addImm(-1).addImm(SHAVECC::EQ).addReg(SHAVE::CC_CMU0);

  // Insert the CMU.CMZ instruction
  finaliseMI(BuildMI(BB, BB.begin(), dbgLoc, get(SHAVE::CMU_CMZ_i32)).addReg(iteratorRegister));

  // Insert a tail to tidy up after the BRU.RPI loop, at the end of the basic block.
  // This tail is simply:
  //    LSU.LDIL iteratorRegister 0
  // Since BRU.RPI doesn't modify iteratorRegister, we need to load zero to mimic the behaviour of
  // the unmodified loop (which did modify iteratorRegister on each iteration of the loop).
  MachineBasicBlock::iterator insertionPoint = --BB.end();

  // Make sure the immediate load happens before any uncoditional branches at the end of the block
  if (foundUnconditionalBranch)
    insertionPoint--;

  finaliseMI(BuildMI(BB, insertionPoint, dbgLoc, get(SHAVE::LSU_LDIL), iteratorRegister).addImm(0));

  // If this basic block falls through to the next, we need to mark that block as a landing pad
  if (!foundUnconditionalBranch) {
    MachineBasicBlock::succ_iterator succ = BB.succ_begin();
    if (*succ == &BB)
      succ++;
    (*succ)->setIsEHPad();
  }

  return true;
}

void SHAVEInstrInfo::ExpandJmpToLabel(MachineBasicBlock &BB,
                                      MachineBasicBlock::iterator MI) const {
  DebugLoc dbgLoc = MI->getDebugLoc();
  MachineOperand target = MI->getOperand(0);
  MachineOperand predMask = MI->getOperand(1);
  MachineOperand predReg = MI->getOperand(2);
  MachineBasicBlock *targetBlock = target.getMBB();
  bool complete = false;

  if (BB.size() == 1 && MI->getParent() == targetBlock) {
    // Optimise while(1); loops
    finaliseMI(BuildMI(BB, MI, dbgLoc, get(SHAVE::BRU_RPIM)).addImm(0));
    complete = true;
  }
  else if (MI->getParent() == targetBlock) {
    // Optimise small loops
    complete = tryUseRPI(BB, &*MI);
  }

  // FIXME: Movidius - we need to wean 'moviCompile' off requiring a physical scratch register
  //        In this context it should be possible to use a virtual register
  unsigned tmpPhysicalReg = SHAVERegisterInfo::getScratchReg();

  if (!complete) {
    finaliseMI(BuildMI(BB, MI, dbgLoc, get(SHAVE::LSU_LDIL_Label), tmpPhysicalReg)
      .addMBB(targetBlock));
    finaliseMI(BuildMI(BB, MI, dbgLoc, get(SHAVE::LSU_LDIH_Label), tmpPhysicalReg)
      .addReg(tmpPhysicalReg).addMBB(targetBlock));

    unsigned Opc = (predMask.getImm() != SHAVECC::AL) ? SHAVE::BRU_JMPcc : SHAVE::BRU_JMP;

    BuildMI(BB, MI, dbgLoc, get(Opc)).addReg(tmpPhysicalReg).add(predMask).add(predReg);
  }
}

void SHAVEInstrInfo::ExpandLDImm32(MachineBasicBlock &BB,
                                   MachineBasicBlock::iterator MI) const {
  DebugLoc dbgLoc = MI->getDebugLoc();
  unsigned DstReg = MI->getOperand(0).getReg();
  MachineOperand Src = MI->getOperand(1);
  int64_t Imm = Src.getImm();

  if (Imm == 0) {
    // Use CMU.CPZI for zero
    finaliseMI(BuildMI(BB, MI, dbgLoc, get(SHAVE::CMU_CPZI), DstReg)
               .copyImplicitOps(*MI));
    return;
  }

  unsigned ImmLo = ((uint64_t)Imm & 0x0000FFFF);
  unsigned ImmHi = ((uint64_t)Imm & 0xFFFF0000);

  finaliseMI(BuildMI(BB, MI, dbgLoc, get(SHAVE::LSU_LDIL), DstReg)
      .copyImplicitOps(*MI).addImm(ImmLo));

  if (ImmHi)
    finaliseMI(BuildMI(BB, MI, dbgLoc, get(SHAVE::LSU_LDIH), DstReg)
        .copyImplicitOps(*MI).addReg(DstReg).addImm(ImmHi));
}

void SHAVEInstrInfo::ExpandUnpack(MachineBasicBlock &BB,
                                  MachineBasicBlock::iterator MI) const {
  // XXX Kill flags.
  DebugLoc dbgLoc = MI->getDebugLoc();
  unsigned OpIdx = 0;
  unsigned DstReg = MI->getOperand(OpIdx++).getReg();
  unsigned DstTy = MI->getOperand(OpIdx++).getImm();
  unsigned AllSignBitsZero = MI->getOperand(OpIdx++).getImm();
  MVT DstVT = MVT((MVT::SimpleValueType)DstTy);
  unsigned NumSrcWords = MI->getNumOperands() - 3;
  unsigned InsertOpc = SHAVE::CMU_CPIV_x32_v4i32;
  unsigned ExtOpc = 0;
  unsigned ZeroHiOpc = 0;

  switch (NumSrcWords) {
  case 1:
    // v4i8 -> v4i16
    if (DstVT == MVT::v4i16) {
      ExtOpc = SHAVE::CMU_CPVV_v4i8_v4i16_conv;
      ZeroHiOpc = SHAVE::ZeroHi_v8i8_VRF64_l;
    }
    // i16 -> v2i8
    else if (DstVT == MVT::v2i8) {
      ZeroHiOpc = SHAVE::ZeroHi_v4i16;
    }
    // v2i16/v4i8 IRF -> VRF
    else if (DstVT == MVT::v2i16 || DstVT == MVT::v4i8) {
      InsertOpc = SHAVE::CMU_CPIV_x32_v2i16;
    }
    // v4i8 -> v4i32
    else {
      ExtOpc = SHAVE::CMU_CPVV_v4i8_v4i32_conv;
      ZeroHiOpc = SHAVE::ZeroHi_v4i8;
    }
    break;
  case 2:
    if (DstVT == MVT::v8i16) {
      // v8i8 -> v8i16
      ExtOpc = SHAVE::CMU_CPVV_v8u8_v8u16_conv;
    }
    // No operations for v8i8.
    else if (DstVT != MVT::v8i8 && DstVT != MVT::v4i16) {
      // v4i16 -> v4i32
      ExtOpc = SHAVE::CMU_CPVV_v4i16_v4i32_conv;
      ZeroHiOpc = SHAVE::ZeroHi_v4i16;
    }
    break;
  }

  // Insert the source words into the destination vector.
  for (unsigned i = 0; i < NumSrcWords; i++) {
    MachineOperand &MO = MI->getOperand(OpIdx++);

    if (MO.isUndef())
      continue;

    finaliseMI(BuildMI(BB, MI, dbgLoc, get(InsertOpc), DstReg)
        .addReg(DstReg).addReg(MO.getReg()).addImm(i));
  }

  // Perform any required extension.
  if (ExtOpc) {
    finaliseMI(BuildMI(BB, MI, dbgLoc, get(ExtOpc), DstReg).addReg(DstReg));
  }

  // Perform any required clearing of high bits (if sext was done).
  if (ZeroHiOpc && !AllSignBitsZero) {
    finaliseMI(BuildMI(BB, MI, dbgLoc, get(ZeroHiOpc), DstReg).addReg(DstReg));
  }
}

void SHAVEInstrInfo::ExpandZeroHi(MachineBasicBlock &BB,
                                  MachineBasicBlock::iterator MI) const {
  const static unsigned Maskv2i8[] = {VSZM_LANE0+0, VSZM_CLEAR, VSZM_CLEAR, VSZM_CLEAR};
  const static unsigned Maskv4i8[] = {VSZM_LANE0+0, VSZM_CLEAR, VSZM_CLEAR, VSZM_CLEAR};
  const static unsigned Maskv4i16[] = {VSZM_LANE0+0, VSZM_LANE0+1, VSZM_CLEAR, VSZM_CLEAR};
  const static unsigned Maskv8i8[] = {VSZM_LANE0+0, VSZM_CLEAR, VSZM_LANE0+2, VSZM_CLEAR};
  DebugLoc dbgLoc = MI->getDebugLoc();
  unsigned DstReg = MI->getOperand(0).getReg();
  unsigned Opc = SHAVE::CMU_VSZMBYTE_imm_vrf;
  MachineInstrBuilder MIB = BuildMI(BB, MI, dbgLoc, get(Opc), DstReg);

  MIB.add(MI->getOperand(1));

  const unsigned *Mask = nullptr;

  switch (MI->getOpcode()) {
  default:
    llvm_unreachable("Unknown ZeroHi variant");
    return;
  case SHAVE::ZeroHi_v2i8:
  case SHAVE::ZeroHi_v2i8_VRF64_l:
    Mask = Maskv2i8;
    break;
  case SHAVE::ZeroHi_v4i8:
    Mask = Maskv4i8;
    break;
  case SHAVE::ZeroHi_v4i16:
  case SHAVE::ZeroHi_v2i16_VRF64_l:
    Mask = Maskv4i16;
    break;
  case SHAVE::ZeroHi_v8i8:
  case SHAVE::ZeroHi_v4i8_VRF64_l:
    Mask = Maskv8i8;
    break;
  }

  for (unsigned i = 0; i < 4; i++)
    MIB.addImm(Mask[i]);

  finaliseMI(MIB);
}

void SHAVEInstrInfo::ExpandVECTOR_EXTEND(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const {
  MachineOperand destination = MI->getOperand(0);
  MachineOperand source = MI->getOperand(1);
  DebugLoc dl = MI->getDebugLoc();

  assert(destination.isReg() && source.isReg() && "VECTOR_EXTEND pseudo instruction must read/write registers");
  unsigned dstReg = destination.getReg();
  unsigned srcReg = source.getReg();
  const TargetRegisterInfo *TRI = SHAVEST.getRegisterInfo();

  // If destination != source then we need to emit a copy from source to destination
  if (dstReg != srcReg && !TRI->isSuperRegister(srcReg, dstReg)) {
    unsigned opcode = SHAVE::CMU_CPVV;

    if (SHAVE::VRF64_lRegClass.contains(srcReg))
      opcode = SHAVE::CMU_CPVV_64_128;
    else if (SHAVE::VRF32_q0RegClass.contains(srcReg))
      opcode = SHAVE::CMU_CPVV_32_128;
    else if (SHAVE::VRF16_e0RegClass.contains(srcReg))
      opcode = SHAVE::CMU_CPVV_16_128;

    MachineInstrBuilder MIB = BuildMI(BB, MI, dl, get(opcode), destination.getReg());
    MIB.add(source);
    finaliseMI(MIB);
  }
}

// FIXME: We should be able to replace this with an llvm utility function.
MachineBasicBlock::instr_iterator SHAVEInstrInfo::FindFirstUser(MachineBasicBlock &BB,
                                                          MachineBasicBlock::iterator MI,
                                                          MachineOperand *&UseMO) const {
  UseMO = nullptr;

  if (MI->getNumOperands() == 0)
    return BB.end().getInstrIterator();

  MachineOperand &DefMO = MI->getOperand(0);
  const TargetRegisterInfo *TRI = SHAVEST.getRegisterInfo();

  if (!DefMO.isReg() || !DefMO.isDef())
    return BB.end().getInstrIterator();

  unsigned DefReg = DefMO.getReg();

  for (MachineBasicBlock::iterator I = MI, E = BB.end(); I != E; ++I) {
    if (I == MI || I->isDebugValue())
      continue;

    for (unsigned i = 0; i < I->getNumOperands(); i++) {
      MachineOperand &MO = I->getOperand(i);
      if (MO.isReg() && MO.isUse() && MO.getReg().isValid() &&
          ((MO.getReg() == DefReg) ||
           TRI->isSuperRegister(MO.getReg(), DefReg))) {
        UseMO = &MO;
        return I.getInstrIterator();
      }
    }
  }

  for (MachineBasicBlock::succ_iterator B = BB.succ_begin(), BE = BB.succ_end(); B != BE; B++) {
    MachineBasicBlock * currentBB = *B;

    if (!currentBB->isLiveIn(DefReg))
      continue;

    for (MachineBasicBlock::iterator I = currentBB->begin(), E = currentBB->end(); I != E; I++) {
      for (unsigned i = 0; i < I->getNumOperands(); i++) {
        if (I->isDebugValue())
          continue;

        MachineOperand &MO = I->getOperand(i);

        if (MO.isReg() && MO.isUse() && MO.getReg().isValid() &&
            ((MO.getReg() == DefReg) ||
             TRI->isSuperRegister(MO.getReg(), DefReg))) {
          UseMO = &MO;
          return I.getInstrIterator();
        }
      }
    }
  }

  return BB.end().getInstrIterator();
}

bool SHAVEInstrInfo::ExpandInputSwizzle8(MachineBasicBlock &BB,
                                         MachineBasicBlock::iterator MI) const {
  // Find the instruction that should have an input swizzle, and restore the
  // original source register.
  unsigned SrcReg = MI->getOperand(1).getReg();
  MachineOperand *UseMO = nullptr;
  DebugLoc dbgLoc = MI->getDebugLoc();

  MachineBasicBlock::instr_iterator UserMI = FindFirstUser(BB, MI, UseMO);

  // Handle the case where there is a copy between the INPUT_SWIZZLE8 and its
  // user. In this case the modifier should not be used on the 'COPY':
  // %V14<def> = INPUT_SWIZZLE8 %V15, 4, 5, 6, 7, 0, 1, 2, 3
  // %V14_l<def> = COPY %V15<kill>
  // %V14<def> = CMU_CPVV_v4i16_v4f32_conv %V14_l<kill>, pred:1, pred:%noreg
  while ( UserMI != BB.end() && UserMI->getOpcode() == TargetOpcode::COPY) {
    UserMI = FindFirstUser(BB, UserMI, UseMO);
    assert((UserMI != BB.end()) && "Did not find INPUT_SWIZZLE8 user");
  }

  // Determine which instruction to use.
  unsigned SwizzleOpc = 0;
  bool isAlreadySwizzled = false;

  if (UserMI != BB.end()) {
    unsigned FUnit = GetFunctionalUnit(&*UserMI);

    switch (FUnit) {
    default:
      SwizzleOpc = 0;
      break;
    case SHAVE::CMU:
      SwizzleOpc = SHAVE::LSU_SWZC8;
      break;
    case SHAVE::SAU:
      SwizzleOpc = SHAVE::LSU_SWZS8;
      break;
    case SHAVE::VAU:
      SwizzleOpc = SHAVE::LSU_SWZV8;
      break;
    }

    // Check that there's not already a swizzle in the bundle we want to attach to
    auto bundleIter = UserMI;
    while (bundleIter->isBundled()) {
      if (bundleIter->getOpcode() == SwizzleOpc) {
	isAlreadySwizzled = true;
	break;
      }
      bundleIter++;
    }
  }

  // If we cannot find the user of this swizzle then add a CMU.CPVV for the swizzle to be applied to
  // A swizzle can only be applied to a CMU.CP instruction on Myriad2v1 and Myriad2v2. For newer targets,
  // we must use a CMU.ALIGNVEC instead
  if (UserMI == BB.end() || isAlreadySwizzled || SHAVEConflicts::check_isCMU_CP(UserMI->getOpcode())) {
    unsigned opcode = MI->getOpcode() == SHAVE::INPUT_SWIZZLE8 ? SHAVE::CMU_ALIGNVEC_imm_vrf : SHAVE::CMU_ALIGNVEC_imm_vrf64;
    unsigned reg = MI->getOperand(0).getReg();
    MachineInstrBuilder alignvec = BuildMI(BB, MI, dbgLoc, get(opcode), reg).addReg(reg).addReg(reg).addImm(0);
    UserMI = alignvec.getInstr()->getIterator();
    UseMO = &(alignvec.getInstr()->getOperand(1));
    SwizzleOpc = SHAVE::LSU_SWZC8;
    finaliseMI(alignvec);
  }

  // Ensure that we do not end up with swizzle tied to an llvm-generated copy instruction
  assert((UserMI->getOpcode() != TargetOpcode::COPY ) && "INPUT_SWIZZLE8 tied to TargetOpcode::COPY");
  if (UseMO == nullptr)
    llvm_unreachable("Could not find UseMO");

  // ACC/MAC pseudos need to be expanded before INPUT_SWIZZLE8.
  unsigned UserOpc = UserMI->getOpcode();

  if (SHAVEConflicts::check_isACCPSeq(UserOpc) || SHAVEConflicts::check_isMACPSeq(UserOpc))
    return false;

  UseMO->setReg(SrcReg);

  if (UseMO->isImplicit()) {
    // If FindSingleUser returned an impUse register, that's probably because
    // the actual use operand takes a sub-register of SwizzleDstReg.
    const TargetRegisterInfo *TRI = SHAVEST.getRegisterInfo();
    unsigned SwizzleDstReg = MI->getOperand(0).getReg();

    for (unsigned i = 0; i < UserMI->getNumOperands(); i++) {
      MachineOperand &MO = UserMI->getOperand(i);

      if (!MO.isReg() || !MO.isUse())
        continue;

      unsigned UseReg = MO.getReg();

      if (!TRI->isSuperRegister(UseReg, SwizzleDstReg))
        continue;

      unsigned SubRegIdx = TRI->getSubRegIndex(SwizzleDstReg, UseReg);
      unsigned SrcSubReg = TRI->getSubReg(SrcReg, SubRegIdx);

      MO.setReg(SrcSubReg);
    }
  }

  // Create the swizzle 'instruction modifier'.
  MachineBasicBlock::iterator InsertPt = std::next(UserMI);
  MachineBasicBlock * InsertBB = UserMI->getParent();
  MachineInstrBuilder MIB;

  if (UserMI == InsertBB->end())
    MIB = BuildMI(InsertBB, dbgLoc, get(SwizzleOpc));
  else
    MIB = BuildMI(*InsertBB, InsertPt, dbgLoc, get(SwizzleOpc));

  for (unsigned i = 0; i < 8; i++)
    MIB = MIB.addImm(MI->getOperand(2 + i).getImm());

  finaliseMI(MIB);

  // Bundle the input swizzle instruction with the user.
  MIBundleBuilder Bundle(*InsertBB, UserMI, InsertPt);
  return true;
}

void SHAVEInstrInfo::ExpandLOAD64_LOW_HIGH(MachineBasicBlock &BB,
                                           MachineBasicBlock::iterator MI) const {
  DebugLoc dbgLoc = MI->getDebugLoc();
  MachineFunction *MF = BB.getParent();
  MachineOperand * offset = nullptr;

  bool isHigh = MI->getOpcode() == SHAVE::LOAD64_HIGH || MI->getOpcode() == SHAVE::LOAD64_HIGH_Imm;
  bool hasImmediate = MI->getOpcode() == SHAVE::LOAD64_LOW_Imm || MI->getOpcode() == SHAVE::LOAD64_HIGH_Imm;

  unsigned int operandIndex = 0;
  MachineOperand &dstRegister = MI->getOperand(operandIndex++);
  if (isHigh) operandIndex++; // Skip the tied use-def operand
  MachineOperand &baseAddress = MI->getOperand(operandIndex++);
  if (hasImmediate)
    offset = &(MI->getOperand(operandIndex));

  unsigned int opcode = SHAVE::NONE;
  if (isHigh)
    opcode = offset ? SHAVE::LSU_LDOV128_h : SHAVE::LSU_LDV128_h;
  else
    opcode = offset ? SHAVE::LSU_LDOV128_l : SHAVE::LSU_LDV128_l;

  MachineInstrBuilder newLoad = BuildMI(BB, MI, dbgLoc, get(opcode), dstRegister.getReg());
  if (isHigh)
    newLoad.addReg(dstRegister.getReg()); // Tied-def
  newLoad.addReg(baseAddress.getReg());

  if (offset)
    newLoad.addImm(offset->getImm());

  if (MI->hasOneMemOperand())
    newLoad.addMemOperand(MF->getMachineMemOperand(*MI->memoperands_begin(), 0, 8u));

  finaliseMI(newLoad);
}

void SHAVEInstrInfo::ExpandLDV128(MachineBasicBlock &BB,
                                  MachineBasicBlock::iterator MI) const {
  MachineFunction *MF = BB.getParent();
  DebugLoc dbgLoc = MI->getDebugLoc();
  MachineOperand &MOVal = MI->getOperand(0);
  MachineOperand &MOAddr = MI->getOperand(1);

  // Create memory operands.
  unsigned LoadSize = 8;
  MachineMemOperand *MMOLo = nullptr;
  if (MI->hasOneMemOperand())
    MMOLo = MF->getMachineMemOperand(*MI->memoperands_begin(), 0, LoadSize);
  MachineMemOperand *MMOHi = nullptr;
  if (MMOLo)
    MMOHi = MF->getMachineMemOperand(MMOLo, LoadSize, LoadSize);

  // Determine the offsets for the two load instructions.
  int64_t OffsetLo = 0;

  if (MI->getOpcode() == SHAVE::LDOV128) {
    MachineOperand &MOImm = MI->getOperand(2);
    OffsetLo = MOImm.getImm();
  }

  int64_t OffsetHi = OffsetLo + LoadSize;

  // Determine which store instructions to use.
  unsigned LoadLoOpc = 0;
  unsigned LoadHiOpc = 0;

  // FIXME: Movidius - adapt this code to use a register if the range of the immediate exceeds the range
  //        of a signed 15-bit integer: save the immediate to a register and use 'LDX'.
  if (OffsetLo) {
    // FIXME: Movidius - LDO has an offset limit of 15-bits "signed"
    if (!isInt<15u>(OffsetLo)) llvm_unreachable("Offset to LDO exceeds range of a signed 15-bit integer");
//  assert(isInt<15u>(OffsetLo) && "Offset to LDO exceeds range of a signed 15-bit integer");
    LoadLoOpc = SHAVE::LSU_LDOV128_l;
  } else
    LoadLoOpc = SHAVE::LSU_LDV128_l;

  if (OffsetHi) {
    // FIXME: Movidius - LDO has an offset limit of 15-bits "signed"
    if (!isInt<15u>(OffsetHi)) llvm_unreachable("Offset to LDO exceeds range of a signed 15-bit integer");
//  assert(isInt<15u>(OffsetHi) && "Offset to LDO exceeds range of a signed 15-bit integer");
    LoadHiOpc = SHAVE::LSU_LDOV128_h;
  } else
    LoadHiOpc = SHAVE::LSU_LDV128_h;

  // Load the low part of the vector.
  unsigned ValReg = MOVal.getReg();
  MachineInstrBuilder MIBLo = BuildMI(BB, MI, dbgLoc, get(LoadLoOpc), ValReg);

  MIBLo.add(MOAddr);

  if (OffsetLo)
    MIBLo.addImm(OffsetLo);

  if (MMOLo)
    MIBLo.addMemOperand(MMOLo);
  finaliseMI(MIBLo);

  // Load the high part of the vector.
  MachineInstrBuilder MIBHi = BuildMI(BB, MI, dbgLoc, get(LoadHiOpc), ValReg);

  MIBHi.addReg(ValReg, getKillRegState(true)).add(MOAddr);

  if (OffsetHi)
    MIBHi.addImm(OffsetHi);

  if (MMOHi)
    MIBHi.addMemOperand(MMOHi);
  finaliseMI(MIBHi);
}

void SHAVEInstrInfo::ExpandSTV128(MachineBasicBlock &BB,
                                  MachineBasicBlock::iterator MI) const {
  MachineFunction *MF = BB.getParent();
  DebugLoc dl = MI->getDebugLoc();
  MachineOperand &MOVal = MI->getOperand(0);
  MachineOperand &MOAddr = MI->getOperand(1);

  // Create memory operands.
  unsigned StoreSize = 8;
  MachineMemOperand *MMOLo = nullptr;
  if (MI->hasOneMemOperand())
    MMOLo = MF->getMachineMemOperand(*MI->memoperands_begin(), 0, StoreSize);
  MachineMemOperand *MMOHi = nullptr;
  if (MMOLo)
    MMOHi = MF->getMachineMemOperand(MMOLo, StoreSize, StoreSize);

  // Determine the offsets for the two store instructions.
  int64_t OffsetLo = 0;

  if (MI->getOpcode() == SHAVE::STOV128) {
    MachineOperand &MOImm = MI->getOperand(2);
    OffsetLo = MOImm.getImm();
  }

  int64_t OffsetHi = OffsetLo + StoreSize;

  // Determine which store instructions to use.
  unsigned StoreLoOpc = 0;
  unsigned StoreHiOpc = 0;

  // FIXME: Movidius - adapt this code to use a register if the range of the immediate exceeds the range
  //        of a signed 15-bit integer: save the immediate to a register and use 'STX'.
  if (OffsetLo) {
    // FIXME: Movidius - LDO has an offset limit of 15-bits "signed"
    if (!isInt<15u>(OffsetLo)) llvm_unreachable("Offset to STO exceeds range of a signed 15-bit integer");
//  assert(isInt<15u>(OffsetLo) && "Offset to STO exceeds range of a signed 15-bit integer");
    StoreLoOpc = SHAVE::LSU_STOV128_l;
  } else
    StoreLoOpc = SHAVE::LSU_STV128_l;

  if (OffsetHi) {
    // FIXME: Movidius - LDO has an offset limit of 15-bits "signed"
    if (!isInt<15u>(OffsetHi)) llvm_unreachable("Offset to STO exceeds range of a signed 15-bit integer");
//  assert(isInt<15u>(OffsetHi) && "Offset to STO exceeds range of a signed 15-bit integer");
    StoreHiOpc = SHAVE::LSU_STOV128_h;
  } else
    StoreHiOpc = SHAVE::LSU_ST64_h_Raw;

  // Store the low part of the vector.
  unsigned ValReg = MOVal.getReg();
  MachineInstrBuilder MIBLo = BuildMI(BB, MI, dl, get(StoreLoOpc));

  MachineOperand MOAddrCopy(MOAddr);
  MOAddrCopy.setIsKill(false);

  MIBLo.addReg(ValReg, getKillRegState(false)).add(MOAddrCopy);

  if (OffsetLo)
    MIBLo.addImm(OffsetLo);

  if (MMOLo)
    MIBLo.addMemOperand(MMOLo);
  finaliseMI(MIBLo);

  // Store the high part of the vector.
  MachineInstrBuilder MIBHi = BuildMI(BB, MI, dl, get(StoreHiOpc));

  MIBHi.add(MOVal).add(MOAddr);

  if (OffsetHi)
    MIBHi.addImm(OffsetHi);

  if (MMOHi)
    MIBHi.addMemOperand(MMOHi);
  finaliseMI(MIBHi);
}

void SHAVEInstrInfo::ExpandLDV512(MachineBasicBlock &BB,
                                  MachineBasicBlock::iterator MI) const {
  MachineFunction * parentFunction = BB.getParent();
  DebugLoc dbgLoc = MI->getDebugLoc();
  MachineOperand &destinationValue = MI->getOperand(0);
  MachineOperand &addressValue = MI->getOperand(1);

  // Create memory operands
  unsigned loadSize = 32;

  MachineMemOperand * lowMemOperand = nullptr;
  if (MI->hasOneMemOperand())
    lowMemOperand = parentFunction->getMachineMemOperand(*MI->memoperands_begin(), 0, loadSize);

  MachineMemOperand * highMemOperand = nullptr;
  if (lowMemOperand)
    highMemOperand = parentFunction->getMachineMemOperand(lowMemOperand, loadSize, loadSize);

  // Determine the offsets for the two load instructions
  int64_t lowOffset = 0;
  if (MI->getOpcode() == SHAVE::LDOV512) {
    MachineOperand &immediate = MI->getOperand(2);
    lowOffset = immediate.getImm();
  }

  int64_t highOffset = lowOffset + loadSize;

  // Determine which load instructions to use
  unsigned loadLowOpcode = SHAVE::NONE;
  unsigned loadHighOpcode = SHAVE::NONE;

  unsigned destinationReg = destinationValue.getReg();

  // Load the low part of the vector
  if ((lowOffset & 0x1f) != 0 || lowOffset > 2048 || lowOffset < -2048) {
    // If the offset is not a multiple of 32-bytes and doesn't fit into a 12-bit range
    // Then we need to emit an LDIL/LDIH pair with an LDXED instruction
    MachineInstrBuilder loadImmediate = BuildMI(BB, MI, dbgLoc, get(SHAVE::LDImm32), SHAVERegisterInfo::getScratchReg()).addImm(lowOffset);
    finaliseMI(loadImmediate);

    MachineInstrBuilder loadLow = BuildMI(BB, MI, dbgLoc, get(SHAVE::LSU_LDXV512_l), destinationReg);
    loadLow.add(addressValue);
    loadLow.add(loadImmediate->getOperand(0));

    if (lowMemOperand)
      loadLow.addMemOperand(lowMemOperand);

    finaliseMI(loadLow);
  }
  else {
    if (lowOffset)  loadLowOpcode = SHAVE::LSU_LDOV512_l;
    else            loadLowOpcode = SHAVE::LSU_LDV512_l;

    MachineInstrBuilder loadLow = BuildMI(BB, MI, dbgLoc, get(loadLowOpcode), destinationReg);
    loadLow.add(addressValue);

    if (lowOffset)
      loadLow.addImm(lowOffset);

    if (lowMemOperand)
      loadLow.addMemOperand(lowMemOperand);

    finaliseMI(loadLow);
  }

  if ((highOffset & 0x1f) != 0 || highOffset > 2048 || highOffset < -2048) {
    // If the offset is not a multiple of 32-bytes and doesn't fit into a 12-bit range
    // Then we need to emit an LDIL/LDIH pair with an LDXED instruction
    MachineInstrBuilder loadImmediate = BuildMI(BB, MI, dbgLoc, get(SHAVE::LDImm32), SHAVERegisterInfo::getScratchReg()).addImm(highOffset);
    finaliseMI(loadImmediate);

    MachineInstrBuilder loadHigh = BuildMI(BB, MI, dbgLoc, get(SHAVE::LSU_LDXV512_h), destinationReg);
    loadHigh.addReg(destinationReg, getKillRegState(true)).add(addressValue);
    loadHigh.add(loadImmediate->getOperand(0));

    if (highMemOperand)
      loadHigh.addMemOperand(highMemOperand);

    finaliseMI(loadHigh);
  }
  else {
    if (highOffset) loadHighOpcode = SHAVE::LSU_LDOV512_h;
    else            loadHighOpcode = SHAVE::LSU_LDV512_h;

    // Load the high part of the vector
    MachineInstrBuilder loadHigh = BuildMI(BB, MI, dbgLoc, get(loadHighOpcode), destinationReg);
    loadHigh.addReg(destinationReg, getKillRegState(true)).add(addressValue);

    if (highOffset)
      loadHigh.addImm(highOffset);

    if (highMemOperand)
      loadHigh.addMemOperand(highMemOperand);

    finaliseMI(loadHigh);
  }
}

void SHAVEInstrInfo::ExpandLDOI64(MachineBasicBlock &BB,
                                  MachineBasicBlock::iterator MI) const {
  MachineFunction *parentFunction = BB.getParent();
  DebugLoc dbgLoc = MI->getDebugLoc();
  MachineOperand &destValue = MI->getOperand(0);
  MachineOperand &addressValue = MI->getOperand(1);

  unsigned loadSize = 4;

  MachineMemOperand *lowMemOperand = nullptr;
  if (MI->hasOneMemOperand()) {
    lowMemOperand = parentFunction->getMachineMemOperand(
        *MI->memoperands_begin(), 0, loadSize);
  }

  MachineMemOperand *highMemOperand = nullptr;
  if (lowMemOperand) {
    highMemOperand = parentFunction->getMachineMemOperand(lowMemOperand,
                                                          loadSize, loadSize);
  }

  MachineOperand &immediate = MI->getOperand(2);
  int64_t lowOffset = immediate.getImm();

  unsigned loadLowOpcode = SHAVE::LSU_LDO_i32;
 
  unsigned destReg1 = destValue.getReg() - SHAVE::I0_1 + SHAVE::I0;
  unsigned destReg2 = destReg1 + 1;

  MachineInstrBuilder loadLow = BuildMI(BB, MI, dbgLoc, get(loadLowOpcode));

  MachineOperand MOAddrCopy(addressValue);
  MOAddrCopy.setIsKill(false);

  loadLow.addDef(destReg1).add(MOAddrCopy);

  loadLow.addImm(lowOffset);
  if (lowMemOperand)
    loadLow.addMemOperand(lowMemOperand);

  finaliseMI(loadLow);

  int64_t highOffset = lowOffset + loadSize;

  unsigned loadHighOpcode = SHAVE::LSU_LDO_i32;

  MachineInstrBuilder loadHigh = BuildMI(BB, MI, dbgLoc, get(loadHighOpcode));

  // storeHigh.add(sourceValue).add(addressValue);
  loadHigh.addDef(destReg2).add(MOAddrCopy);

  //if (highOffset)
  loadHigh.addImm(highOffset);

  if (highMemOperand)
    loadHigh.addMemOperand(highMemOperand);


  finaliseMI(loadHigh);

}

void SHAVEInstrInfo::ExpandSTOI64(MachineBasicBlock &BB,
                                  MachineBasicBlock::iterator MI) const {
  MachineFunction *parentFunction = BB.getParent();
  DebugLoc dbgLoc = MI->getDebugLoc();
  MachineOperand &sourceValue = MI->getOperand(0);
  MachineOperand &addressValue = MI->getOperand(1);

  unsigned storeSize = 4;

  MachineMemOperand *lowMemOperand = nullptr;
  if (MI->hasOneMemOperand()) {
    lowMemOperand = parentFunction->getMachineMemOperand(
        *MI->memoperands_begin(), 0, storeSize);
  }

  MachineMemOperand *highMemOperand = nullptr;
  if (lowMemOperand) {
    highMemOperand =
      parentFunction->getMachineMemOperand(lowMemOperand, storeSize, storeSize);
  }

  MachineOperand &immediate = MI->getOperand(2);
  int64_t lowOffset = immediate.getImm();

  unsigned storeLowOpcode = SHAVE::LSU_STO_i32;
  unsigned sourceReg1 = sourceValue.getReg() - SHAVE::I0_1 + SHAVE::I0;
  unsigned sourceReg2 = sourceReg1 + 1;

  MachineInstrBuilder storeLow = BuildMI(BB, MI, dbgLoc, get(storeLowOpcode));

  MachineOperand MOAddrCopy(addressValue);
  MOAddrCopy.setIsKill(false);

  storeLow.addReg(sourceReg1, getKillRegState(false)).add(MOAddrCopy);

  storeLow.addImm(lowOffset);
  if (lowMemOperand)
    storeLow.addMemOperand(lowMemOperand);
  finaliseMI(storeLow);


  int64_t highOffset = lowOffset + storeSize;

  unsigned storeHighOpcode = storeLowOpcode;

  MachineInstrBuilder storeHigh = BuildMI(BB, MI, dbgLoc, get(storeHighOpcode));

  // storeHigh.add(sourceValue).add(addressValue);
  storeHigh.addReg(sourceReg2, getKillRegState(false)).add(MOAddrCopy);

  storeHigh.addImm(highOffset);

  if (highMemOperand)
    storeHigh.addMemOperand(highMemOperand);

  finaliseMI(storeHigh);

}

void SHAVEInstrInfo::ExpandSTV512(MachineBasicBlock &BB,
                                  MachineBasicBlock::iterator MI) const {
  MachineFunction * parentFunction = BB.getParent();
  DebugLoc dbgLoc = MI->getDebugLoc();
  MachineOperand &sourceValue = MI->getOperand(0);
  MachineOperand &addressValue = MI->getOperand(1);

  // Create memory operands
  unsigned storeSize = 32;

  MachineMemOperand * lowMemOperand = nullptr;
  if (MI->hasOneMemOperand())
    lowMemOperand = parentFunction->getMachineMemOperand(*MI->memoperands_begin(), 0, storeSize);

  MachineMemOperand * highMemOperand = nullptr;
  if (lowMemOperand)
    highMemOperand = parentFunction->getMachineMemOperand(lowMemOperand, storeSize, storeSize);

  // Determine the offsets for the two store instructions
  int64_t lowOffset = 0;
  if (MI->getOpcode() == SHAVE::STOV512) {
    MachineOperand &immediate = MI->getOperand(2);
    lowOffset = immediate.getImm();
  }

  // Store the low part of the vector.
  if ((lowOffset & 0x1f) != 0 || lowOffset > 2048 || lowOffset < -2048) {
    // If the offset is not a multiple of 32-bytes and doesn't fit into a 13-bit range
    // Then we need to emit an LDIL/LDIH pair with an STXED instruction
    MachineInstrBuilder loadImmediate = BuildMI(BB, MI, dbgLoc, get(SHAVE::LDImm32), SHAVERegisterInfo::getScratchReg()).addImm(lowOffset);
    finaliseMI(loadImmediate);

    unsigned sourceReg = sourceValue.getReg();
    MachineInstrBuilder storeLow = BuildMI(BB, MI, dbgLoc, get(SHAVE::LSU_STXV512_l));

    MachineOperand MOAddrCopy(addressValue);
    MOAddrCopy.setIsKill(false);

    storeLow.addReg(sourceReg, getKillRegState(false)).add(MOAddrCopy);

    storeLow.add(loadImmediate->getOperand(0));

    if (lowMemOperand)
      storeLow.addMemOperand(lowMemOperand);
    finaliseMI(storeLow);
  }
  else {
    unsigned storeLowOpcode = SHAVE::NONE;
    if (lowOffset)  storeLowOpcode = SHAVE::LSU_STOV512_l;
    else            storeLowOpcode = SHAVE::LSU_STV512_l;

    unsigned sourceReg = sourceValue.getReg();
    MachineInstrBuilder storeLow = BuildMI(BB, MI, dbgLoc, get(storeLowOpcode));

    MachineOperand MOAddrCopy(addressValue);
    MOAddrCopy.setIsKill(false);

    storeLow.addReg(sourceReg, getKillRegState(false)).add(MOAddrCopy);

    if (lowOffset)
      storeLow.addImm(lowOffset);

    if (lowMemOperand)
      storeLow.addMemOperand(lowMemOperand);

    finaliseMI(storeLow);
  }

  int64_t highOffset = lowOffset + storeSize;

  if ((highOffset & 0x1f) != 0 || highOffset > 2048 || highOffset < -2048) {
    // If the offset is not a multiple of 32-bytes and doesn't fit into a 13-bit range
    // Then we need to emit an LDIL/LDIH pair with an STXED instruction
    MachineInstrBuilder loadImmediate = BuildMI(BB, MI, dbgLoc, get(SHAVE::LDImm32), SHAVERegisterInfo::getScratchReg()).addImm(highOffset);
    finaliseMI(loadImmediate);

    MachineInstrBuilder storeHigh = BuildMI(BB, MI, dbgLoc, get(SHAVE::LSU_STXV512_h));

    storeHigh.add(sourceValue).add(addressValue);

    storeHigh.add(loadImmediate->getOperand(0));

    if (highMemOperand)
      storeHigh.addMemOperand(highMemOperand);

    finaliseMI(storeHigh);
  }
  else {
    // Store the high part of the vector.
    unsigned storeHighOpcode = SHAVE::NONE;
    if (highOffset) storeHighOpcode = SHAVE::LSU_STOV512_h;
    else            storeHighOpcode = SHAVE::LSU_STV512_h;

    MachineInstrBuilder storeHigh = BuildMI(BB, MI, dbgLoc, get(storeHighOpcode));

    storeHigh.add(sourceValue).add(addressValue);

    if (highOffset)
      storeHigh.addImm(highOffset);

    if (highMemOperand)
      storeHigh.addMemOperand(highMemOperand);

    finaliseMI(storeHigh);
  }
}


void SHAVEInstrInfo::ExpandSTO_VRF(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const {
  DebugLoc dbgLoc = MI->getDebugLoc();
  unsigned StoreOpc = 0;

  // FIXME: Movidius - we need to wean 'moviCompile' off requiring a physical scratch register
  //        In this context it should be possible to use a virtual register
  unsigned tmpPhysicalReg = SHAVERegisterInfo::getScratchReg();

  if (MI->getOpcode() == SHAVE::STO32_VRF) {
    finaliseMI(BuildMI(BB, MI, dbgLoc, get(SHAVE::CMU_CPVI_x32_l), tmpPhysicalReg)
        .addReg(MI->getOperand(0).getReg()).addImm(0));
    StoreOpc = SHAVE::LSU_STO_i32;
  }
  else if (MI->getOpcode() == SHAVE::STO16_VRF) {
    finaliseMI(BuildMI(BB, MI, dbgLoc, get(SHAVE::CMU_CPVI_x16_l), tmpPhysicalReg)
        .addReg(MI->getOperand(0).getReg()).addImm(0));
    StoreOpc = SHAVE::LSU_STO_i16;
  } else
    llvm_unreachable("Unexpected opcode in ExpandSTO_VRF");

  // FIXME: Movidius - see how to check the immediate value for the STO and transform if it is too large
//  assert(isInt<15u>(BaseOffset) && "Offset to STO exceeds range of a signed 15-bit integer");

  finaliseMI(BuildMI(BB, MI, dbgLoc, get(StoreOpc)).copyImplicitOps(*MI)
        .addReg(tmpPhysicalReg, getKillRegState(true))
        .add(MI->getOperand(1)).add(MI->getOperand(2))
        .addMemOperand(*MI->memoperands_begin()));
}

// FIXME: Movidius - Temporarily adding only to test the fp partial ordering issues
//#define LINEAR_CHAIN
#ifdef LINEAR_CHAIN
#define V_ACC1x V_ACC0x
#define S_ACC1 S_ACC0
#endif
void SHAVEInstrInfo::ExpandACCSequence(MachineBasicBlock &BB,
                                       MachineBasicBlock::iterator MI, bool isFloatType, bool isVectorType,
                                       LiveIntervals * LIS) const {
  DebugLoc dbgLoc = MI->getDebugLoc();
  unsigned V_ACCreg[] = { SHAVE::V_ACC0x, SHAVE::V_ACC1x, SHAVE::V_ACC};
  unsigned S_ACCreg[] = { SHAVE::S_ACC0,  SHAVE::S_ACC1,  SHAVE::S_ACC};

  unsigned *ACCreg;
  unsigned FADD32Opc;
  unsigned ACCSequenceOpcode = MI->getOpcode();
  const TargetRegisterClass *TRC;

  if (isVectorType) {
    ACCreg = V_ACCreg;
    if (hasFeature(SHAVE::HasVRF512_Feature)) {
      TRC = &SHAVE::WVRF512RegClass;
      FADD32Opc = ((ACCSequenceOpcode == SHAVE::VAU_ACCP_SEQ_f16) ? SHAVE::VAU_ADD_512_v32f16_Myr4 : SHAVE::VAU_ADD_512_v16f32_Myr4);
    } else {
      TRC = &SHAVE::VRF128RegClass;
      FADD32Opc = ((ACCSequenceOpcode == SHAVE::VAU_ACCP_SEQ_f16) ? SHAVE::VAU_ADD_f16 : SHAVE::VAU_ADD_f32);
    }
  } else {
    ACCreg = S_ACCreg;
    TRC = &SHAVE::IRF64RegClass;
    FADD32Opc = ((ACCSequenceOpcode == SHAVE::SAU_ACCP_SEQ_f16) ? SHAVE::SAU_ADD_f16 : SHAVE::SAU_ADD_f32);
    //        FADD32Opc = SHAVE::SAU_ADD_f16;
  }

  // For myriad2v3 floats, the %2 of this values is used to distribute the chain over ACC0 and ACC1
  unsigned int accRegIdx = 0;

  if (isFloatType /* do for floats only */ ) {
    assert(LIS != nullptr && "Expanding ACC sequence postRA on myriad2v3 and later is not supported");
  }
  if (isFloatType /* do for floats only */) {
    const unsigned FirstOpIdx = 2;              // Index 0 : destreg, Index 1: Number of operands, Index 2: First operand
    const unsigned NumberOfACCArgs = MI->getOperand(1).getImm();

    unsigned totalArgRegs = 0;           // actual number of registers in ACC chain (NumberOfACCArgs - markers)
    unsigned totalArgRegsProcessed = 0;  //  register arguments processed
    unsigned totalArgsProcessed = 0;     // total including the -1 markers for accn
    bool genAccN = false;

    // Get the potentially required nodes at the start
    unsigned ACCPZOpc = GetACCPZOpcode(MI->getOpcode());
    unsigned ACCNZOpc = GetACCNZOpcode(MI->getOpcode());
    unsigned ACCPOpc = GetACCPOpcode(MI->getOpcode());
    unsigned ACCNOpc = GetACCNOpcode(MI->getOpcode());
    unsigned ACCPWOpc = GetACCPWOpcode(MI->getOpcode());
    unsigned ACCNWOpc = GetACCNWOpcode(MI->getOpcode());

    // Find the actual number of arguments, ignoring the negation markers
    for (unsigned i = 0; i < NumberOfACCArgs; i++)
      if (!MI->getOperand(FirstOpIdx + i).isImm())
        totalArgRegs++;

    // Build the ACCPZ nodes with first 2 actual args
    // The first node in the chain is always positive
    // TODO: Modify the combiner so that we can have an accnz at the start of chain1
    MachineOperand ACCPZ1Op1 = MI->getOperand(FirstOpIdx);

    // Handle the negation node
    if (ACCPZ1Op1.isImm() && ACCPZ1Op1.getImm() == SHAVE_ACCN_MACN_MARKER) {
      totalArgsProcessed++;
      genAccN = true;
      ACCPZ1Op1 = MI->getOperand(FirstOpIdx + totalArgsProcessed);
    }

    unsigned ACCZOpc = (genAccN) ? ACCNZOpc : ACCPZOpc;
    MachineInstrBuilder ACCPZ1 = BuildMI(BB, MI, dbgLoc, get(ACCPZOpc));

    ACCPZ1.addReg(ACCreg[accRegIdx % 2], RegState::Define);
    ACCPZ1.add(ACCPZ1Op1);
    ACCPZ1.addReg(ACCreg[accRegIdx % 2]);
    finaliseMI(ACCPZ1);
    genAccN = false;
    accRegIdx++;

    if (LIS)
      LIS->InsertMachineInstrInMaps(*ACCPZ1.getInstr());

    totalArgsProcessed++;
    totalArgRegsProcessed++;

    // Handle the negation node
    MachineOperand ACCPZ2Op1 = MI->getOperand(FirstOpIdx + totalArgsProcessed);

    if (ACCPZ2Op1.isImm() && ACCPZ2Op1.getImm() == SHAVE_ACCN_MACN_MARKER) {
      totalArgsProcessed++;
      genAccN = true;
      ACCPZ2Op1 = MI->getOperand(FirstOpIdx + totalArgsProcessed);
    }

#ifndef LINEAR_CHAIN
    ACCZOpc = (genAccN) ? ACCNZOpc : ACCPZOpc;
#else
    ACCZOpc = (genAccN) ? ACCNOpc : ACCPOpc;
    tmpreg = MI->getOperand(0);
#endif

    MachineInstrBuilder ACCPZ2 = BuildMI(BB, MI, dbgLoc, get(ACCZOpc));

    ACCPZ2.addReg(ACCreg[accRegIdx % 2], RegState::Define);
    ACCPZ2.add(ACCPZ2Op1);
    ACCPZ2.addReg(ACCreg[accRegIdx % 2]);

    accRegIdx++;
    finaliseMI(ACCPZ2);

    totalArgsProcessed++;
    totalArgRegsProcessed++;
    genAccN = false;

    if (LIS)
      LIS->InsertMachineInstrInMaps(*ACCPZ2.getInstr());

    // Build ACCP nodes
    unsigned ACCOpc = ACCPOpc;

#ifndef LINEAR_CHAIN
    for (unsigned i = FirstOpIdx + totalArgsProcessed; totalArgRegsProcessed < totalArgRegs - 2; i++, totalArgsProcessed++) {
#else
    for (unsigned i = FirstOpIdx + totalArgsProcessed; totalArgRegsProcessed < totalArgRegs - 1; i++, totalArgsProcessed++) {
#endif
      MachineOperand &ACCPOp1 = MI->getOperand(i);

      // Detect the negation markers to flag the next instruction as ACCN/MACN
      if (ACCPOp1.isImm() && ACCPOp1.getImm() == SHAVE_ACCN_MACN_MARKER) {
        genAccN = true;
        continue;
      }

      ACCOpc = (genAccN) ? ACCNOpc : ACCPOpc;

      MachineInstrBuilder ACCP = BuildMI(BB, MI, dbgLoc, get(ACCOpc));

      ACCP.addReg(ACCreg[accRegIdx % 2], RegState::Define);
      ACCP.add(ACCPOp1);
      ACCP.addReg(ACCreg[accRegIdx % 2]);
      finaliseMI(ACCP);
      accRegIdx++;
      totalArgRegsProcessed++;
      genAccN = false;

      if (LIS)
        LIS->InsertMachineInstrInMaps(*ACCP.getInstr());
    }

    // Build ACCPW nodes
    unsigned ACCWOpc = ACCPWOpc;
    MachineOperand ACCPW1Op1 = MI->getOperand(FirstOpIdx + totalArgsProcessed);

    // Handle a marker seen at this location
    if (ACCPW1Op1.isImm() && (ACCPW1Op1.getImm() == SHAVE_ACCN_MACN_MARKER)) {
      totalArgsProcessed++;
      ACCWOpc = ACCNWOpc;
      ACCPW1Op1 = MI->getOperand(FirstOpIdx + totalArgsProcessed);
    }

    MachineInstrBuilder ACCPW1 = BuildMI(BB, MI, dbgLoc, get(ACCWOpc));

    unsigned ACCPW1Dst = BB.getParent()->getRegInfo().createVirtualRegister(TRC);
    ACCPW1.addDef(ACCPW1Dst);
    ACCPW1.addReg(ACCreg[accRegIdx % 2], RegState::Define);
    ACCPW1.add(ACCPW1Op1);
    ACCPW1.addReg(ACCreg[accRegIdx % 2]);
    finaliseMI(ACCPW1);

    if (LIS)
      LIS->InsertMachineInstrInMaps(*ACCPW1.getInstr());

#ifndef LINEAR_CHAIN
    totalArgsProcessed++;
    totalArgRegsProcessed++;
    accRegIdx++;

    ACCWOpc = ACCPWOpc;

    MachineOperand ACCPW2Op1 = MI->getOperand(FirstOpIdx + totalArgsProcessed);

    // Handle a marker seen at this location
    if (ACCPW2Op1.isImm() && (ACCPW2Op1.getImm() == SHAVE_ACCN_MACN_MARKER)) {
      totalArgsProcessed++;
      ACCWOpc = ACCNWOpc;
      ACCPW2Op1 = MI->getOperand(FirstOpIdx + totalArgsProcessed);
    }

    unsigned ACCPW2Dst = BB.getParent()->getRegInfo().createVirtualRegister(TRC);
    MachineInstrBuilder ACCPW2 = BuildMI(BB, MI, dbgLoc, get(ACCWOpc));

    ACCPW2.addDef(ACCPW2Dst);
    ACCPW2.addReg(ACCreg[accRegIdx % 2], RegState::Define);
    ACCPW2.add(ACCPW2Op1);
    ACCPW2.addReg(ACCreg[accRegIdx % 2]);
    finaliseMI(ACCPW2);
    accRegIdx++;
    totalArgsProcessed++;
    totalArgRegsProcessed++;

    if (LIS)
      LIS->InsertMachineInstrInMaps(*ACCPW2.getInstr());

    // Tie the two chains into the output operand
    MachineInstrBuilder ACCConsolidationInsn = BuildMI(BB, MI, dbgLoc, get(FADD32Opc));

    ACCConsolidationInsn.addDef(MI->getOperand(0).getReg());
    ACCConsolidationInsn.addUse(ACCPW1Dst);
    ACCConsolidationInsn.addUse(ACCPW2Dst);
    finaliseMI(ACCConsolidationInsn);

    if (LIS)
      LIS->InsertMachineInstrInMaps(*ACCConsolidationInsn.getInstr());
#endif
  } else {
    // Build ACCPZ.
    unsigned ACCPZOpc = GetACCPZOpcode(MI->getOpcode());
    MachineOperand &ACCPZOp1 = MI->getOperand(2);
    MachineInstrBuilder ACCPZ = BuildMI(BB, MI, dbgLoc, get(ACCPZOpc));
    bool genAccN = false;

    ACCPZ.addReg(ACCreg[accRegIdx], RegState::Define);
    ACCPZ.add(ACCPZOp1);
    ACCPZ.addReg(ACCreg[accRegIdx]);
    finaliseMI(ACCPZ);

    if (LIS)
      LIS->InsertMachineInstrInMaps(*ACCPZ.getInstr());

    // Build ACCP.
    unsigned ACCPOpc = GetACCPOpcode(MI->getOpcode());
    unsigned ACCNOpc = GetACCNOpcode(MI->getOpcode());
    unsigned ACCOpc = ACCPOpc;

    const unsigned FirstOpIdx = 2;
    unsigned NumberOfACCArgs = MI->getOperand(1).getImm();

    for (unsigned i = 1; i < NumberOfACCArgs - 1; i++) {
      MachineOperand &ACCPOp1 = MI->getOperand(FirstOpIdx + i);

      // Detect the negation markers to flag the next instruction as ACCN/MACN
      if (ACCPOp1.isImm() && ACCPOp1.getImm() == SHAVE_ACCN_MACN_MARKER) {
        genAccN = true;
        continue;
      }

      ACCOpc = (genAccN) ? ACCNOpc : ACCPOpc;

      MachineInstrBuilder ACCP = BuildMI(BB, MI, dbgLoc, get(ACCOpc));

      ACCP.addReg(ACCreg[accRegIdx], RegState::Define);
      ACCP.add(ACCPOp1);
      ACCP.addReg(ACCreg[accRegIdx]);
      finaliseMI(ACCP);
      genAccN = false;

      if (LIS)
        LIS->InsertMachineInstrInMaps(*ACCP.getInstr());
    }

    // Build ACCPW.
    unsigned ACCPWOpc = GetACCPWOpcode(MI->getOpcode());
    unsigned ACCNWOpc = GetACCNWOpcode(MI->getOpcode());
    unsigned ACCWOpc = genAccN ? ACCNWOpc : ACCPWOpc;
    unsigned ACCPWOpIdx = FirstOpIdx + NumberOfACCArgs - 1;
    MachineOperand &ACCPWOp1 = MI->getOperand(ACCPWOpIdx);
    MachineOperand &ACCPWDst = MI->getOperand(0);
    MachineInstrBuilder ACCPW = BuildMI(BB, MI, dbgLoc, get(ACCWOpc));

    ACCPW.add(ACCPWDst);
    ACCPW.addReg(ACCreg[accRegIdx], RegState::Define);
    ACCPW.add(ACCPWOp1);
    ACCPW.addReg(ACCreg[accRegIdx]);

    finaliseMI(ACCPW);

    if (LIS)
      LIS->InsertMachineInstrInMaps(*ACCPW.getInstr());
  }
}


void SHAVEInstrInfo::ExpandMACPSequence(MachineBasicBlock &BB,
                                        MachineBasicBlock::iterator MI, bool isFloatType, bool isVectorType,
                                        LiveIntervals * LIS) const {
  DebugLoc dbgLoc = MI->getDebugLoc();
  unsigned V_ACCreg[] = { SHAVE::V_ACC0x, SHAVE::V_ACC1x, SHAVE::V_ACC};
  unsigned S_ACCreg[] = { SHAVE::S_ACC0,  SHAVE::S_ACC1,  SHAVE::S_ACC};

  unsigned *ACCreg;
  unsigned FADD32Opc;
  unsigned MACSequenceOpcode = MI->getOpcode();
  const TargetRegisterClass *TRC;

  if (isVectorType) {
    ACCreg = V_ACCreg;
    
    // FIXME: Movidius - this is using 'SHAVE::V0' directly, but this is not a reserved register?
    if (!hasFeature(SHAVE::HasVRF512_Feature)) {
      // tmpreg = SHAVE::V0;
      TRC = &SHAVE::VRF128RegClass;
      FADD32Opc = (MACSequenceOpcode == SHAVE::VAU_MACP_SEQ_f16) ? SHAVE::VAU_ADD_f16 : SHAVE::VAU_ADD_f32;
    }
    else {
      // tmpreg = SHAVE::W0;
      TRC = &SHAVE::WVRF512RegClass;
      FADD32Opc = (MACSequenceOpcode == SHAVE::VAU_MACP_SEQ_f16_Myr4) ? SHAVE::VAU_ADD_512_v32f16_Myr4 : SHAVE::VAU_ADD_512_v16f32_Myr4;
    }
  } else {
    ACCreg = S_ACCreg;
    TRC = &SHAVE::IRF64RegClass;
    FADD32Opc = (MACSequenceOpcode == SHAVE::SAU_MACP_SEQ_f16) ? SHAVE::SAU_ADD_f16 : SHAVE::SAU_ADD_f32;
  }

  // For myriad2v3 floats, the %2 of this values is used to distribute the chain over ACC0 and ACC1
  unsigned int accRegIdx = 0;

  if (isFloatType /* do for floats only */ ) {
    assert(LIS != nullptr && "Expanding MAC sequence postRA on myriad2v3 and later is not supported");
  }

  if (isFloatType /* do for floats only */ ) {
    const unsigned FirstOpIdx = 2;       // Index 0 : destreg, Index 1: Number of operands, Index 2: First operand
    unsigned NumberOfMACArgPairs = MI->getOperand(1).getImm() / 2;

    unsigned totalArgRegPairs = 0;           // actual number of register pairs in MAC chain (NumberOfMACArgPairs - marker pairs)
    unsigned totalArgRegPairsProcessed = 0;  //  register arguments processed
    unsigned totalArgPairsProcessed = 0;     // total including the -1 marker pairs for macn
    bool genMacN = false;

    // Get the potentially required nodes at the start
    unsigned MACPZOpc = GetMACPZOpcode(MI->getOpcode());
    unsigned MACNZOpc = GetMACNZOpcode(MI->getOpcode());
    unsigned MACPOpc = GetMACPOpcode(MI->getOpcode());
    unsigned MACNOpc = GetMACNOpcode(MI->getOpcode());
    unsigned MACPWOpc = GetMACPWOpcode(MI->getOpcode());
    unsigned MACNWOpc = GetMACNWOpcode(MI->getOpcode());

    // Find the actual number of arguments, ignoring the negation markers
    for (unsigned i = 0; i < NumberOfMACArgPairs * 2; i++)
      if (!MI->getOperand(FirstOpIdx + i).isImm())
        totalArgRegPairs++;

    totalArgRegPairs /= 2;

    // Build the ACCPZ nodes with first 2 actual args
    // The first node in the chain should always be positive, but we have seen contradicting cases
    // TODO: Modify the combiner so that we can have an accnz at the start of chain1 more often
    MachineOperand MACPZ1Op1 = MI->getOperand(FirstOpIdx);

    if (MACPZ1Op1.isImm() && MACPZ1Op1.getImm() == SHAVE_ACCN_MACN_MARKER) {
      totalArgPairsProcessed++;
      genMacN = true;
      MACPZ1Op1 = MI->getOperand(FirstOpIdx + totalArgPairsProcessed * 2);
    }

    MachineOperand &MACPZ1Op2 = MI->getOperand(FirstOpIdx + totalArgPairsProcessed * 2 + 1);
    unsigned MACZOpc = (genMacN) ? MACNZOpc : MACPZOpc;
    MachineInstrBuilder MACPZ1 = BuildMI(BB, MI, dbgLoc, get(MACZOpc));

    MACPZ1.addReg(ACCreg[accRegIdx], RegState::Define);
    MACPZ1.add(MACPZ1Op1);
    MACPZ1.add(MACPZ1Op2);
    MACPZ1.addReg(ACCreg[accRegIdx]);
    finaliseMI(MACPZ1);
    accRegIdx++;
    totalArgPairsProcessed++;
    totalArgRegPairsProcessed++;
    genMacN = false;

    if (LIS)
      LIS->InsertMachineInstrInMaps(*MACPZ1.getInstr());

    //This can be a negated node. Handle it thus
    MachineOperand MACPZ2Op1 = MI->getOperand(FirstOpIdx + totalArgPairsProcessed * 2);
    if (MACPZ2Op1.isImm() && MACPZ2Op1.getImm() == SHAVE_ACCN_MACN_MARKER) {
      totalArgPairsProcessed++;
      genMacN = true;
      MACPZ2Op1 = MI->getOperand(FirstOpIdx + totalArgPairsProcessed * 2);
    }
    MachineOperand &MACPZ2Op2 = MI->getOperand(FirstOpIdx + totalArgPairsProcessed * 2 + 1);

#ifndef LINEAR_CHAIN
    MACZOpc = (genMacN) ? MACNZOpc : MACPZOpc;
#else
    MACZOpc = (genMacN) ? MACNOpc : MACPOpc;
    tmpreg = MI->getOperand(0);
#endif

    MachineInstrBuilder MACPZ2 = BuildMI(BB, MI, dbgLoc, get(MACZOpc));

    MACPZ2.addReg(ACCreg[accRegIdx], RegState::Define);
    MACPZ2.add(MACPZ2Op1);
    MACPZ2.add(MACPZ2Op2);
    MACPZ2.addReg(ACCreg[accRegIdx]);
    finaliseMI(MACPZ2);

    if (LIS)
      LIS->InsertMachineInstrInMaps(*MACPZ2.getInstr());

    totalArgPairsProcessed++;
    totalArgRegPairsProcessed++;
    genMacN = false;

    // Build MACP nodes.
#ifndef LINEAR_CHAIN
    for (unsigned i = FirstOpIdx + totalArgPairsProcessed * 2; totalArgRegPairsProcessed < totalArgRegPairs - 2; i += 2, totalArgPairsProcessed++) {
#else
    for (unsigned i = FirstOpIdx + totalArgPairsProcessed * 2; totalArgRegPairsProcessed < totalArgRegPairs - 1; i += 2, totalArgPairsProcessed++) {
#endif
      //        for (unsigned i = 1; i < NumberOfMACArgPairs - 3; i++) {
      MachineOperand &MACPOp1 = MI->getOperand(i);

      // Detect the negation markers to flag the next instruction as ACCN/MACN
      if (MACPOp1.isImm() && MACPOp1.getImm() == SHAVE_ACCN_MACN_MARKER) {
        genMacN = true;
        continue;
      }

      MachineOperand &MACPOp2 = MI->getOperand(i + 1);
      unsigned MACOpc = (genMacN) ? MACNOpc : MACPOpc;
      MachineInstrBuilder MACP = BuildMI(BB, MI, dbgLoc, get(MACOpc));

      MACP.addReg(ACCreg[accRegIdx % 2], RegState::Define);
      MACP.add(MACPOp1);
      MACP.add(MACPOp2);
      MACP.addReg(ACCreg[accRegIdx % 2]);
      finaliseMI(MACP);
      accRegIdx++;
      totalArgRegPairsProcessed++;
      genMacN = false;

      if (LIS)
        LIS->InsertMachineInstrInMaps(*MACP.getInstr());
    }

    // Build the first MACW node
    unsigned MACWOpc = MACPWOpc;

    MachineOperand MACPW1Op1 = MI->getOperand(FirstOpIdx + totalArgPairsProcessed * 2);

    // Handle a marker seen at this location
    if (MACPW1Op1.isImm() && (MACPW1Op1.getImm() == SHAVE_ACCN_MACN_MARKER)) {
      totalArgPairsProcessed++;
      MACWOpc = MACNWOpc;
      MACPW1Op1 = MI->getOperand(FirstOpIdx + totalArgPairsProcessed * 2);
    }

    MachineOperand &MACPW1Op2 = MI->getOperand(FirstOpIdx + totalArgPairsProcessed * 2 + 1);
    MachineInstrBuilder MACPW1 = BuildMI(BB, MI, dbgLoc, get(MACWOpc));

    unsigned MACPW1Dst = BB.getParent()->getRegInfo().createVirtualRegister(TRC);
    MACPW1.addReg(MACPW1Dst, RegState::Define);
    MACPW1.addReg(ACCreg[accRegIdx % 2], RegState::Define);
    MACPW1.add(MACPW1Op1);
    MACPW1.add(MACPW1Op2);
    MACPW1.addReg(ACCreg[accRegIdx % 2]);
    finaliseMI(MACPW1);

    if (LIS)
      LIS->InsertMachineInstrInMaps(*MACPW1.getInstr());

#ifndef LINEAR_CHAIN
    totalArgPairsProcessed++;
    totalArgRegPairsProcessed++;
    accRegIdx++;

    MACWOpc = MACPWOpc;

    // Build the 2nd MACW node
    MachineOperand MACPW2Op1 = MI->getOperand(FirstOpIdx + totalArgPairsProcessed * 2);

    // Handle a marker seen at this location
    if (MACPW2Op1.isImm() && (MACPW2Op1.getImm() == SHAVE_ACCN_MACN_MARKER)) {
      totalArgPairsProcessed++;
      MACWOpc = MACNWOpc;
      MACPW2Op1 = MI->getOperand(FirstOpIdx + totalArgPairsProcessed * 2);
    }

    MachineOperand &MACPW2Op2 = MI->getOperand(FirstOpIdx + totalArgPairsProcessed * 2 + 1);
    unsigned MACPW2Dst = BB.getParent()->getRegInfo().createVirtualRegister(TRC);
    MachineInstrBuilder MACPW2 = BuildMI(BB, MI, dbgLoc, get(MACWOpc));

    MACPW2.addDef(MACPW2Dst);
    MACPW2.addReg(ACCreg[accRegIdx % 2], RegState::Define);
    MACPW2.add(MACPW2Op1);
    MACPW2.add(MACPW2Op2);
    MACPW2.addReg(ACCreg[accRegIdx % 2]);
    finaliseMI(MACPW2);

    totalArgPairsProcessed++;
    totalArgRegPairsProcessed++;
    accRegIdx++;

    if (LIS)
      LIS->InsertMachineInstrInMaps(*MACPW2.getInstr());

    // Tie the two chains into the output operand
    MachineInstrBuilder MACConsolidationInsn = BuildMI(BB, MI, dbgLoc, get(FADD32Opc));

    // Output it into the original SEQ destination
    MACConsolidationInsn.addDef(MI->getOperand(0).getReg());

    MACConsolidationInsn.addUse(MACPW1Dst);
    MACConsolidationInsn.addUse(MACPW2Dst);
    finaliseMI(MACConsolidationInsn);

    if (LIS)
      LIS->InsertMachineInstrInMaps(*MACConsolidationInsn.getInstr());
#endif
  } else {
    // Build MAC[P|N]Z.
    MachineOperand * MACPNZOp1 = &(MI->getOperand(2));
    MachineOperand * MACPNZOp2 = &(MI->getOperand(3));
    bool genMacN = false;
    unsigned MACPNZOpc = GetMACPZOpcode(MI->getOpcode());
    unsigned MACPNStartIdx = 1;

    if (MACPNZOp1->isImm() && MACPNZOp1->getImm() == SHAVE_ACCN_MACN_MARKER) {
      MACPNZOpc = GetMACNZOpcode(MI->getOpcode());
      MACPNZOp1 = &(MI->getOperand(4));
      MACPNZOp2 = &(MI->getOperand(5));
      MACPNStartIdx = 2;
    }

    MachineInstrBuilder MACPNZ = BuildMI(BB, MI, dbgLoc, get(MACPNZOpc));

    MACPNZ.addReg(ACCreg[accRegIdx], RegState::Define);
    MACPNZ.add(*MACPNZOp1);
    MACPNZ.add(*MACPNZOp2);
    MACPNZ.addReg(ACCreg[accRegIdx]);
    finaliseMI(MACPNZ);

    if (LIS)
      LIS->InsertMachineInstrInMaps(*MACPNZ.getInstr());

    // Build MACP.
    unsigned MACPOpc = GetMACPOpcode(MI->getOpcode());
    unsigned MACNOpc = GetMACNOpcode(MI->getOpcode());
    unsigned MACOpc = MACPOpc;

    // "-4" means MACPZ's and MACPW's operands.
    const unsigned FirstOpIdx = 2;
    unsigned NumMACOps = MI->getOperand(1).getImm() / 2;

    for (unsigned i = MACPNStartIdx; i < NumMACOps - 1; i++) {
      unsigned MACPOpIdx = FirstOpIdx + (i * 2);
      MachineOperand &MACPOp1 = MI->getOperand(MACPOpIdx);
      MachineOperand &MACPOp2 = MI->getOperand(MACPOpIdx + 1);

      if (MACPOp1.isImm() && MACPOp1.getImm() == SHAVE_ACCN_MACN_MARKER) {
        genMacN = true;
        continue;
      }

      MACOpc = (genMacN) ? MACNOpc : MACPOpc;

      MachineInstrBuilder MACP = BuildMI(BB, MI, dbgLoc, get(MACOpc));

      MACP.addReg(ACCreg[accRegIdx], RegState::Define);
      MACP.add(MACPOp1);
      MACP.add(MACPOp2);
      MACP.addReg(ACCreg[accRegIdx]);
      finaliseMI(MACP);
      genMacN = false;

      if (LIS)
        LIS->InsertMachineInstrInMaps(*MACP.getInstr());
    }

    // Build MACPW.
    unsigned MACPWOpc = GetMACPWOpcode(MI->getOpcode());
    unsigned MACNWOpc = GetMACNWOpcode(MI->getOpcode());
    unsigned MACWOpc = genMacN ? MACNWOpc : MACPWOpc;
    unsigned MACPWOpIdx = FirstOpIdx + (NumMACOps - 1) * 2;
    MachineOperand &MACPWOp1 = MI->getOperand(MACPWOpIdx);
    MachineOperand &MACPWOp2 = MI->getOperand(MACPWOpIdx + 1);
    MachineOperand &MACPWDst = MI->getOperand(0);
    MachineInstrBuilder MACPW = BuildMI(BB, MI, dbgLoc, get(MACWOpc));

    MACPW.add(MACPWDst);
    MACPW.addReg(ACCreg[accRegIdx], RegState::Define);
    MACPW.add(MACPWOp1);
    MACPW.add(MACPWOp2);
    MACPW.addReg(ACCreg[accRegIdx]);

    finaliseMI(MACPW);

    if (LIS)
      LIS->InsertMachineInstrInMaps(*MACPW.getInstr());
  }
}


#if 0
// FIXME: Movidius - TODO: implementation of Memset.  See 'SHAVESelectionDAGInfo.cpp' for full details
void SHAVEInstrInfo::ExpandMemsetBlock(MachineBasicBlock &BB,
                                       MachineBasicBlock::iterator MI) const {
  DebugLoc dbgLoc = MI->getDebugLoc();

  MachineOperand destination = MI->getOperand(0);
  MachineOperand count = MI->getOperand(1);
  MachineOperand value = MI->getOperand(2);
  MachineOperand inc = MI->getOperand(3);

  MachineBasicBlock::iterator it = MI;
  ++it;

  // Insert the instructions
  //     BRU.RPI count
  //         || LSU.STI32 value destination inc
  // in place of the pseudo instruction SHAVE_MEMSET_BLOCK
  MachineInstrBuilder LoopIns = BuildMI(BB, it, dbgLoc, get(SHAVE::BRU_RPI))
      .add(count);
  finaliseMI(LoopIns);
  finaliseMI(BuildMI(BB, it, dbgLoc, get(SHAVE::LSU0_STI_i32), destination.getReg()).add(value)
      .add(destination).add(inc));

  MIBundleBuilder MIBB(BB, LoopIns, it);
}
#endif

bool SHAVEInstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  MachineBasicBlock &BB = *MI.getParent();

  switch (MI.getDesc().getOpcode()) {
  case SHAVE::SHAVE_SETCC_i32:
  case SHAVE::SHAVE_SETCC_i16:
  case SHAVE::SHAVE_SETCC_i8:
  case SHAVE::SHAVE_SETCC_f32:
  case SHAVE::SHAVE_SETCC_f16:
    ExpandSetCC(BB, MI);
    break;

  // Myriad4
  // 512-bit vectors
  case SHAVE::SHAVE_SETCC_v16i32_v16i32_Myr4:
  case SHAVE::SHAVE_SETCC_v32i16_v32i16_Myr4:
  case SHAVE::SHAVE_SETCC_v64i8_v64i8_Myr4:
  case SHAVE::SHAVE_SETCC_v16f32_v16i32_Myr4:
  case SHAVE::SHAVE_SETCC_v32f16_v32i16_Myr4:
  // 256-bit vectors
  case SHAVE::SHAVE_SETCC_v8i32_v8i32_Myr4:
  case SHAVE::SHAVE_SETCC_v16i16_v16i16_Myr4:
  case SHAVE::SHAVE_SETCC_v32i8_v32i8_Myr4:
  case SHAVE::SHAVE_SETCC_v8f32_v8i32_Myr4:
  case SHAVE::SHAVE_SETCC_v16f16_v16i16_Myr4:
  // 128-bit vectors
  case SHAVE::SHAVE_SETCC_v4i32_v4i32_Myr4:
  case SHAVE::SHAVE_SETCC_v8i16_v8i16_Myr4:
  case SHAVE::SHAVE_SETCC_v16i8_v16i8_Myr4:
  case SHAVE::SHAVE_SETCC_v4f32_v4i32_Myr4:
  case SHAVE::SHAVE_SETCC_v8f16_v8i16_Myr4:
  // 64-bit
  case SHAVE::SHAVE_SETCC_v2i32_v2i32_Myr4:
  case SHAVE::SHAVE_SETCC_v4i16_v4i16_Myr4:
  case SHAVE::SHAVE_SETCC_v8i8_v8i8_Myr4:
  case SHAVE::SHAVE_SETCC_v2f32_v2i32_Myr4:
  case SHAVE::SHAVE_SETCC_v4f16_v4i16_Myr4:
  // Myriad2
  case SHAVE::SHAVE_SETCC_v4i32_v4i32:
  case SHAVE::SHAVE_SETCC_v8i16_v8i16:
  case SHAVE::SHAVE_SETCC_v16i8_v16i8:
  case SHAVE::SHAVE_SETCC_v2i32_v2i32:
  case SHAVE::SHAVE_SETCC_v4i16_v4i16:
  case SHAVE::SHAVE_SETCC_v2i16_v2i16:
  case SHAVE::SHAVE_SETCC_v8i8_v8i8:
  case SHAVE::SHAVE_SETCC_v4i8_v4i8:
  case SHAVE::SHAVE_SETCC_v2i8_v2i8:
  case SHAVE::SHAVE_SETCC_v4f32_v4i32:
  case SHAVE::SHAVE_SETCC_v2f32_v2i32:
  case SHAVE::SHAVE_SETCC_v8f16_v8i16:
  case SHAVE::SHAVE_SETCC_v4f16_v4i16:
  case SHAVE::SHAVE_SETCC_v2f16_v2i16:
    ExpandSetCCVector(BB, MI);
    break;

  case SHAVE::SELECT_IRF64:
  case SHAVE::SELECT_IRF32:
  case SHAVE::SELECT_IRF16:
  case SHAVE::SELECT_IRF8:
  case SHAVE::SELECT_WVRF512:
  case SHAVE::SELECT_WVRF256_0:
  case SHAVE::SELECT_WVRF128_0:
  case SHAVE::SELECT_WVRF64_0:
  case SHAVE::SELECT_VRF128:
  case SHAVE::SELECT_VRF64_v2i32:
  case SHAVE::SELECT_VRF64_v2f32:
  case SHAVE::SELECT_VRF64_v4i16:
  case SHAVE::SELECT_VRF64_v4f16:
  case SHAVE::SELECT_VRF64_v8i8:
  case SHAVE::SELECT_IRF32_v2f16:
  case SHAVE::SELECT_IRF32_v2i16:
  case SHAVE::VSELECT_WVRF512_v16:
  case SHAVE::VSELECT_WVRF256_0_v8:
  case SHAVE::VSELECT_WVRF128_0_v4:
  case SHAVE::VSELECT_WVRF64_0_v2:
  case SHAVE::VSELECT_WVRF512_v32:
  case SHAVE::VSELECT_WVRF256_0_v16:
  case SHAVE::VSELECT_WVRF128_0_v8:
  case SHAVE::VSELECT_WVRF64_0_v4:
  case SHAVE::VSELECT_WVRF512_v64:
  case SHAVE::VSELECT_WVRF256_0_v32:
  case SHAVE::VSELECT_WVRF128_0_v16:
  case SHAVE::VSELECT_WVRF64_0_v8:
  case SHAVE::VSELECT_VRF128_v4:
  case SHAVE::VSELECT_VRF128_v8:
  case SHAVE::VSELECT_VRF128_v16:
  case SHAVE::VSELECT_VRF64_v2:
  case SHAVE::VSELECT_VRF64_v4:
  case SHAVE::VSELECT_VRF64_v8:
  case SHAVE::VSELECT_IRF32_v4:
  case SHAVE::VSELECT_IRF32_v2:
  case SHAVE::VSELECT_IRF16_v2:
    if (!ExpandSelect(BB, MI))
      return false;
    break;

  case SHAVE::MASKED_STORE_VRF:
  case SHAVE::MASKED_STORE_IRF:
  case SHAVE::MASKED_STORE_WVRF:
  case SHAVE::MASKED_STORE_WVRF_l:
  case SHAVE::MASKED_STORE_WVRF_h:
    ExpandMaskedStore(BB, MI);
    break;

  case SHAVE::JMP_TO_LABEL:
  case SHAVE::JMPcc_TO_LABEL:
    ExpandJmpToLabel(BB, MI);
    break;

  case SHAVE::STO_I64_Myr4:
    ExpandSTOI64(BB, MI);
    break;

   case SHAVE::LDO_I64_Myr4:
    ExpandLDOI64(BB, MI);
     break;

  case SHAVE::LOAD64_LOW:
  case SHAVE::LOAD64_LOW_Imm:
  case SHAVE::LOAD64_HIGH:
  case SHAVE::LOAD64_HIGH_Imm:
    ExpandLOAD64_LOW_HIGH(BB, MI);
    break;

  case SHAVE::LDV128:
  case SHAVE::LDOV128:
    ExpandLDV128(BB, MI);
    break;

  case SHAVE::STV128:
  case SHAVE::STOV128:
    ExpandSTV128(BB, MI);
    break;

  case SHAVE::LDV512:
  case SHAVE::LDOV512:
    ExpandLDV512(BB, MI);
    break;

  case SHAVE::STV512:
  case SHAVE::STOV512:
    ExpandSTV512(BB, MI);
    break;

  case SHAVE::STO32_VRF:
  case SHAVE::STO16_VRF:
    ExpandSTO_VRF(BB, MI);
    break;

  case SHAVE::LDImm32:
    ExpandLDImm32(BB, MI);
    break;

  case SHAVE::Unpack1:
  case SHAVE::Unpack1_32bit:
  case SHAVE::Unpack1_16bit:
  case SHAVE::Unpack2:
  case SHAVE::Unpack4:
    ExpandUnpack(BB, MI);
    break;

  case SHAVE::ZeroHi_v2i8:
  case SHAVE::ZeroHi_v4i8:
  case SHAVE::ZeroHi_v4i16:
  case SHAVE::ZeroHi_v8i8:
  case SHAVE::ZeroHi_v2i8_VRF64_l:
  case SHAVE::ZeroHi_v4i8_VRF64_l:
  case SHAVE::ZeroHi_v2i16_VRF64_l:
  case SHAVE::ZeroHi_v8i8_VRF64_l:
    ExpandZeroHi(BB, MI);
    break;

  case SHAVE::VECTOR_EXTEND_v8i8_v16i8:
  case SHAVE::VECTOR_EXTEND_v4i16_v8i16:
  case SHAVE::VECTOR_EXTEND_v2i32_v4i32:
  case SHAVE::VECTOR_EXTEND_v4f16_v8f16:
  case SHAVE::VECTOR_EXTEND_v2f32_v4f32:
  case SHAVE::VECTOR_EXTEND_v2i16_v8i16:
  case SHAVE::VECTOR_EXTEND_v2f16_v8f16:
  case SHAVE::VECTOR_EXTEND_v4i8_v16i8:
  case SHAVE::VECTOR_EXTEND_v2i8_v16i8:
    ExpandVECTOR_EXTEND(BB, MI);
    break;

  case SHAVE::INPUT_SWIZZLE8:
  case SHAVE::INPUT_SWIZZLE8_vrf64:
    if (!ExpandInputSwizzle8(BB, MI))
      return false;
    break;
// SAURABH : check why the _u8/u16/u32 patterns are not used
// Also ACCN nodes not used here?
  case SHAVE::SAU_ACCP_SEQ_i8:
  case SHAVE::SAU_ACCP_SEQ_i16:
  case SHAVE::SAU_ACCP_SEQ_i32:
    ExpandACCSequence(BB, MI, false, false);
    break;
  case SHAVE::VAU_ACCP_SEQ_i8:
  case SHAVE::VAU_ACCP_SEQ_i16:
  case SHAVE::VAU_ACCP_SEQ_i32:
  case SHAVE::VAU_ACCP_SEQ_i8_Myr4:
  case SHAVE::VAU_ACCP_SEQ_i16_Myr4:
  case SHAVE::VAU_ACCP_SEQ_i32_Myr4:
    ExpandACCSequence(BB, MI, false, true);
    break;
  case SHAVE::SAU_ACCP_SEQ_f16:
  case SHAVE::SAU_ACCP_SEQ_f32:
    ExpandACCSequence(BB, MI, true, false);
    break;
// SAURABH : These are not generated at all?
  case SHAVE::VAU_ACCP_SEQ_f16:
  case SHAVE::VAU_ACCP_SEQ_f32:
  case SHAVE::VAU_ACCP_SEQ_f16_Myr4:
  case SHAVE::VAU_ACCP_SEQ_f32_Myr4:
    ExpandACCSequence(BB, MI, true, true);
    break;

  case SHAVE::SAU_MACP_SEQ_i8:
  case SHAVE::SAU_MACP_SEQ_i16:
  case SHAVE::SAU_MACP_SEQ_i32:
    ExpandMACPSequence(BB, MI, false, false);
    break;
  case SHAVE::VAU_MACP_SEQ_i8:
  case SHAVE::VAU_MACP_SEQ_i16:
  case SHAVE::VAU_MACP_SEQ_i32:
  case SHAVE::VAU_MACP_SEQ_i8_Myr4:
  case SHAVE::VAU_MACP_SEQ_i16_Myr4:
  case SHAVE::VAU_MACP_SEQ_i32_Myr4:
    ExpandMACPSequence(BB, MI, false, true);
    break;

  case SHAVE::SAU_MACP_SEQ_f16:
  case SHAVE::SAU_MACP_SEQ_f32:
    ExpandMACPSequence(BB, MI, true, false);
    break;
  case SHAVE::VAU_MACP_SEQ_f16:
  case SHAVE::VAU_MACP_SEQ_f32:
  case SHAVE::VAU_MACP_SEQ_f16_Myr4:
  case SHAVE::VAU_MACP_SEQ_f32_Myr4:
    ExpandMACPSequence(BB, MI, true, true);
    break;

#if 0
  // FIXME: Movidius - TODO: implementation of Memset.  See 'SHAVESelectionDAGInfo.cpp' for full details
  case SHAVE::SHAVE_MEMSET_BLOCK:
    ExpandMemsetBlock(BB, MI);
    break;
#endif

  default:
    return false;
  }

  BB.erase(MI);
  return true;
}

unsigned SHAVEInstrInfo::getISAIntrinsicOpcode(unsigned intrinsicID) const {
  auto lookup = SHAVE::SHAVEIntrinsicsMapping.find(intrinsicID);
  if (lookup != SHAVE::SHAVEIntrinsicsMapping.end()) {
    for (const auto feature : lookup->second.features)
      if (feature != SHAVE::NumSubtargetFeatures && !hasFeature(feature))
        return 0;

    return lookup->second.opcode;
  }

  return 0;
}

int SHAVEInstrInfo::getFUnitOperandIndex(const MachineInstr *MI) const {
  const unsigned Opc = MI->getOpcode();

  if (!SHAVEConflicts::check_usesLSU0(Opc) && !SHAVEConflicts::check_usesLSU1(Opc) && !SHAVEConflicts::check_usesCMU(Opc))
    return -1;

  const MCInstrDesc &Desc = MI->getDesc();

#if 0
  for (int i = (MI->getNumOperands() - 1); i >= 0; --i) {
    const MachineOperand &MO = MI->getOperand(i);

    if (((unsigned)i < Desc.getNumOperands()) && (Desc.operands()[i].isPredicate()))
       return -1;

    if (MO.isImm() && (MO.getImm() >= 0))
      return i;
  }

  return -1;
#else
  int nOperands = MI->getNumOperands();
  int fuOperandIndex = -1;

  // Look for the predicate
  for (int i = 0; i < nOperands; ++i)
    if (Desc.operands()[i].isPredicate()) {
      // Skip the predicate
      while ((i < nOperands) && Desc.operands()[i].isPredicate())
        i++;

      // The next operand should be the FU
      if (i < nOperands) {
        const MachineOperand &MO = MI->getOperand(i);

        if (MO.isImm() && ((MO.getImm() == SHAVE::CMU) || (MO.getImm() == SHAVE::LSU0) || (MO.getImm() == SHAVE::LSU1)))
          fuOperandIndex = i;
        break;
      }
    }

  return fuOperandIndex;
#endif
}

bool SHAVEInstrInfo::findLoadStoreBase(const MachineInstr *MI, unsigned int *Reg, bool isHighPart) const {
  if (MI->mayLoad()) {
    if (isHighPart && MI->getNumOperands() >= 3) {
      const MachineOperand &operand = MI->getOperand(2); // Operand 1 is the tied-def dst register
      if (operand.isReg()) {
        *Reg = operand.getReg();
        return true;
      }
    }
    else if (MI->getNumOperands() >= 2) {
      const MachineOperand &operand = MI->getOperand(1);
      if (operand.isReg()) {
        *Reg = operand.getReg();
        return true;
      }
    }
  }
  else if (MI->mayStore() && MI->getNumOperands() >= 1) {
    const MachineOperand &operand = MI->getOperand(0);
    if (operand.isReg()) {
      *Reg = operand.getReg();
      return true;
    }
  }

  return false;
}

bool SHAVEInstrInfo::findLoadStoreOffset(const MachineInstr *MI, int *Offset) const {
  // Load direct element instructions have the pattern <ptr reg>, <element idx> which
  // will be incorrectly detected by this instruction during frame index elimination
  if (SHAVEConflicts::check_isLoadElement(MI->getOpcode()))
    return false;

  // Find the index of the last 'non-special' operand.
  int OpIdx = MI->findFirstPredOperandIdx();

  if (OpIdx < 0)
    OpIdx = getFUnitOperandIndex(MI);

  if (OpIdx < 0)
    OpIdx = MI->getNumExplicitOperands();

  OpIdx--;

  if (OpIdx < 0)
    return false;

  // Offset operands have to be immediates.
  const MachineOperand &Op = MI->getOperand(OpIdx);

  if (!Op.isImm())
    return false;

  // Found the offset.
  if (Offset)
    *Offset = Op.getImm();

  return true;
}

bool SHAVEInstrInfo::isBranchInstruction(const MachineInstr *MI) const {
  return isBranchInstruction(MI->getDesc());
}

bool SHAVEInstrInfo::isBranchInstruction(const MCInstrDesc &Desc) const {
  // Return true if the instruction is marked as branch, call or return
  // in the target description files.
  return (Desc.isCall() || Desc.isBranch() || Desc.isReturn());
}

const MCSchedClassDesc * SHAVEInstrInfo::GetSchedClass(const MachineInstr *MI) const {
  return SHAVEST.getTargetSchedModel()->resolveSchedClass(MI);
}

unsigned SHAVEInstrInfo::GetSchedMaxLatency(const MachineInstr *MI) const {
  const MCInstrDesc &Desc = MI->getDesc();
  const unsigned opCode = Desc.getOpcode();
  unsigned MaxLatency = 0;

  if (Desc.getOpcode() < SHAVE::ADJCALLSTACKUP)
    return 0;

  // Take into account writeback latencies of instruction operands.
  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i)
    MaxLatency = std::max(MaxLatency, GetSchedLatency(MI, i));

  // Branches have their own writeback latency. The list of case statements
  // are special cases, the normal branch delay slots do not apply.
  if (isBranchInstruction(Desc)) {
    switch (opCode) {
    case SHAVE::BRU_SWIH_Ret:
    case SHAVE::BRU_SWIH_imm:
    case SHAVE::BRU_SWIC_imm:
    case SHAVE::BRU_RPI:
    case SHAVE::BRU_RPIM:
      break;
    default:
      MaxLatency = std::max(MaxLatency, (unsigned)getDelaySlots());
      break;
    }
  }

  // Handle configurable LSU load latencies
  if (SHAVEConflicts::check_isLoad(MI->getOpcode()))
    MaxLatency = std::max(MaxLatency, getLoadLatency());

  // LDXV/STXV instructions use the load/store unit for an additional cycle,
  // increasing latency.
  if (SHAVEConflicts::check_isLDXVorSTXV(MI->getOpcode()))
    MaxLatency = std::max(MaxLatency, 1u);

  return MaxLatency;
}

unsigned SHAVEInstrInfo::GetSchedLatency(const MachineInstr *MI, unsigned OpIdx,
                                         bool *OpFound) const {
  SHAVEOpSchedInfo Op;
  const bool Found = GetSchedOperand(MI, OpIdx, Op);

  if (OpFound)
    *OpFound = Found;

  return Found ? Op.Latency : 0;
}

bool SHAVEInstrInfo::GetSchedOperand(const MachineInstr *MI, unsigned OpIdx,
                                     SHAVEOpSchedInfo &Info) const {
  Info.Clear();

  if (OpIdx >= MI->getNumOperands())
    return false;

  if (MI->getOpcode() <= SHAVE::ADJCALLSTACKUP)
    return false;
  else if (MI->getDesc().isPseudo()) {
    // FIXME: Movidius - it would be useful for load pseudos to have scheduling info.
    return false;
  }

  // We don't know the latency of vararg operands.
  const MCInstrDesc &Desc = MI->getDesc();

  if ((OpIdx >= Desc.getNumOperands()) && (OpIdx < MI->getNumExplicitOperands()))
    return false;

  // Determine the index of the operand in the entry list.
  unsigned EntryIdx = 0;
  const MachineOperand *MO = nullptr;

  for (unsigned i = 0; i <= OpIdx; i++) {
    MO = &MI->getOperand(i);

    if (i < Desc.getNumOperands()) {
      // Skip non-register and predicate (explicit) operands.
      const MCOperandInfo &OpDesc = Desc.operands()[i];

      if (OpDesc.isPredicate()) {
        i++;
        continue;
      } else if (!MO->isReg())
        continue;
    } else if (i < MI->getNumExplicitOperands())
      // Skip vararg operands.
      continue;

    if (i < OpIdx)
      EntryIdx++;
  }

  if (!MO || !MO->isReg())
    return false;

  Info.Reg = MO->getReg();
  Info.IsDef = MO->isDef();
  Info.IsImplicit = MO->isImplicit();

  const MCSchedClassDesc *SCDesc = GetSchedClass(MI);

  if (!SCDesc->isValid()) {
    DEBUG(dbgs() << "Missing scheduling information: " << *MI);
    llvm_unreachable("No scheduling information found for the instruction");
    return false;
  }

  // Validate the entry index.
  if (EntryIdx >= SCDesc->NumWriteLatencyEntries) {
    DEBUG(dbgs() << "Out-of-range latency entry: " << EntryIdx
           << ", entries found: " << SCDesc->NumWriteLatencyEntries
           << ", for operand: " << OpIdx << " (" << *MO << ")\n"
           << *MI);
    return false;
  }

  // Lookup the definition's write latency in SubtargetInfo and validate it,
  // possibly plugging in a special-cased latency.
  const MCWriteLatencyEntry *WLEntry = SHAVEST.getWriteLatencyEntry(SCDesc, EntryIdx);

  Info.Latency = TranslateLatency(MI, WLEntry->Cycles);
  return true;
}

void SHAVEInstrInfo::GetSchedPortRange(const MachineInstr * MI, TargetSchedModel::ProcResIter &begin, TargetSchedModel::ProcResIter &end) const {
  const MCSchedClassDesc *SCDesc = GetSchedClass(MI);

  if (!SCDesc->isValid()) {
    DEBUG(dbgs() << "Missing scheduling information: " << *MI);
    llvm_unreachable("No scheduling information found for instruction");
  }

  begin = SHAVEST.getWriteProcResBegin(SCDesc);
  end = SHAVEST.getWriteProcResEnd(SCDesc);
}

unsigned SHAVEInstrInfo::TranslateLatency(const MachineInstr *MI,
                                          int latency) const {
  if (latency >= 0)
    return (unsigned)latency;

  unsigned FUnit = GetFunctionalUnit(MI);

  switch (FUnit) {
  default:
    llvm_unreachable("Register latency is less than zero");
    return 0;
  case SHAVE::BRU:
#ifndef NDEBUG
    {
      const MCInstrDesc &Desc = MI->getDesc();
      const unsigned opCode = Desc.getOpcode();
      assert((opCode != SHAVE::BRU_SWIH_Ret) && (opCode != SHAVE::BRU_SWIH_imm) && (opCode != SHAVE::BRU_SWIC_imm));
    }
#endif // NDEBUG
    return getDelaySlots();
  case SHAVE::LSU0:
  case SHAVE::LSU1:
    return 6;
  }
}

unsigned SHAVEInstrInfo::getInstrLatency(const InstrItineraryData *ItinData,
                                         const MachineInstr &MI,
                                         unsigned *PredCost) const {
  // Calculate bundle's latency.
  if (MI.isBundle()) {
    unsigned Latency = 0;
    MachineBasicBlock::const_instr_iterator I = MachineBasicBlock::const_instr_iterator(MI);
    MachineBasicBlock::const_instr_iterator E = MI.getParent()->instr_end();

    while (++I != E && I->isInsideBundle())
      Latency += getInstrLatency(ItinData, *I, PredCost);

    return Latency;
  }

  // For the common case, fall back on the itinerary's latency.
  const MCInstrDesc &MCID = MI.getDesc();

  return ItinData->getStageLatency(MCID.getSchedClass());
}

unsigned SHAVEInstrInfo::getInstrLatency(const InstrItineraryData *ItinData,
                                         SDNode *Node) const {
  // FIXME: Movidius - this needs a real implementation
  return TargetInstrInfo::getInstrLatency(ItinData, Node);
}

unsigned SHAVEInstrInfo::GetFunctionalUnit(const MachineInstr *MI) const {
  const unsigned Opc = MI->getOpcode();

  if (SHAVEConflicts::check_usesLSU1(Opc)) {
    const int FUnitOpIdx = getFUnitOperandIndex(MI);

    if (FUnitOpIdx < 0) {
      assert(MI->isPseudo() && "LSU instruction is missing functional unit operand");
      return SHAVE::NONE;
    }

    return (unsigned)MI->getOperand(FUnitOpIdx).getImm();
  }

  return GetFunctionalUnit(Opc);
}

unsigned SHAVEInstrInfo::GetFunctionalUnit(unsigned Opc) const {
  return SHAVEII::getFunctionalUnitFromTSFlags(get(Opc).TSFlags);
}

FUnitMask SHAVEInstrInfo::GetFunctionalUnitMask(const MachineInstr * instr) const {
  unsigned functionalUnit = GetFunctionalUnit(instr);

  switch (functionalUnit) {
  case SHAVE::IAU: return FmIAU;
  case SHAVE::BRU: return FmBRU;
  case SHAVE::SAU: return FmSAU;
  case SHAVE::VAU: return FmVAU;
  case SHAVE::LSU0: return FmLSU0;
  case SHAVE::LSU1: return FmLSU1;
  case SHAVE::CMU: return FmCMU;
  case SHAVE::PEU: return FmPEU;
  default:
    llvm_unreachable("Unknown Functional Unit");
  }
}

FUnitMask SHAVEInstrInfo::GetFunctionalUnitMask(unsigned Opc) const {
  if (SHAVEConflicts::check_usesIAU(Opc))
    return FmIAU;
  else if (SHAVEConflicts::check_usesSAU(Opc))
    return FmSAU;
  else if (SHAVEConflicts::check_usesVAU(Opc))
    return FmVAU;
  else if (SHAVEConflicts::check_usesPEU(Opc))
    return FmPEU;
  else if (SHAVEConflicts::check_usesCMU(Opc))
    return FmCMU;
  else if (SHAVEConflicts::check_usesBRU(Opc))
    return FmBRU;
  else if ((Opc == SHAVE::JMP_TO_LABEL)
           || (Opc == SHAVE::JMPcc_TO_LABEL)
           || (Opc == SHAVE::JMPcc_TO_LABEL_PREEMPTION))
    return FmBRU;
  else
    return FmNone;
}

unsigned SHAVEInstrInfo::GetACCPZOpcode(unsigned Opc) const {
  unsigned ACCPZOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");

  // SAU.ACCPZ
  case SHAVE::SAU_ACCP_SEQ_i8:
    ACCPZOpc = SHAVE::SAU_ACCPZ_i8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i16:
    ACCPZOpc = SHAVE::SAU_ACCPZ_i16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i32:
    ACCPZOpc = SHAVE::SAU_ACCPZ_i32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u8:
    ACCPZOpc = SHAVE::SAU_ACCPZ_u8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u16:
    ACCPZOpc = SHAVE::SAU_ACCPZ_u16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u32:
    ACCPZOpc = SHAVE::SAU_ACCPZ_u32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f16:
    ACCPZOpc = SHAVE::SAU_ACCPZ_f16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f32:
    ACCPZOpc = SHAVE::SAU_ACCPZ_f32_pl;
    break;

  // VAU.ACCPZ
  case SHAVE::VAU_ACCP_SEQ_i8:
    ACCPZOpc = SHAVE::VAU_ACCPZ_i8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16:
    ACCPZOpc = SHAVE::VAU_ACCPZ_i16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i8_Myr4:
    ACCPZOpc = SHAVE::VAU_ACCPZ_i8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16_Myr4:
    ACCPZOpc = SHAVE::VAU_ACCPZ_i16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32:
    ACCPZOpc = SHAVE::VAU_ACCPZ_i32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32_Myr4:
    ACCPZOpc = SHAVE::VAU_ACCPZ_i32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8:
    ACCPZOpc = SHAVE::VAU_ACCPZ_u8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8_Myr4:
    ACCPZOpc = SHAVE::VAU_ACCPZ_u8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16:
    ACCPZOpc = SHAVE::VAU_ACCPZ_u16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16_Myr4:
    ACCPZOpc = SHAVE::VAU_ACCPZ_u16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32:
    ACCPZOpc = SHAVE::VAU_ACCPZ_u32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32_Myr4:
    ACCPZOpc = SHAVE::VAU_ACCPZ_u32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16:
    ACCPZOpc = SHAVE::VAU_ACCPZ_f16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16_Myr4:
    ACCPZOpc = SHAVE::VAU_ACCPZ_f16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32:
    ACCPZOpc = SHAVE::VAU_ACCPZ_f32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32_Myr4:
    ACCPZOpc = SHAVE::VAU_ACCPZ_f32_pl_Myr4;
    break;
  }
  return ACCPZOpc;
}
unsigned SHAVEInstrInfo::GetACCNZOpcode(unsigned Opc) const {
  unsigned ACCNZOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");

  // SAU.ACCNZ
  case SHAVE::SAU_ACCP_SEQ_i8:
    ACCNZOpc = SHAVE::SAU_ACCNZ_i8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i16:
    ACCNZOpc = SHAVE::SAU_ACCNZ_i16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i32:
    ACCNZOpc = SHAVE::SAU_ACCNZ_i32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u8:
    ACCNZOpc = SHAVE::SAU_ACCNZ_u8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u16:
    ACCNZOpc = SHAVE::SAU_ACCNZ_u16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u32:
    ACCNZOpc = SHAVE::SAU_ACCNZ_u32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f16:
    ACCNZOpc = SHAVE::SAU_ACCNZ_f16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f32:
    ACCNZOpc = SHAVE::SAU_ACCNZ_f32_pl;
    break;

  // VAU.ACCNZ
  case SHAVE::VAU_ACCP_SEQ_i8:
    ACCNZOpc = SHAVE::VAU_ACCNZ_i8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i8_Myr4:
    ACCNZOpc = SHAVE::VAU_ACCNZ_i8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16:
    ACCNZOpc = SHAVE::VAU_ACCNZ_i16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16_Myr4:
    ACCNZOpc = SHAVE::VAU_ACCNZ_i16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32:
    ACCNZOpc = SHAVE::VAU_ACCNZ_i32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32_Myr4:
    ACCNZOpc = SHAVE::VAU_ACCNZ_i32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8:
    ACCNZOpc = SHAVE::VAU_ACCNZ_u8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8_Myr4:
    ACCNZOpc = SHAVE::VAU_ACCNZ_u8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16:
    ACCNZOpc = SHAVE::VAU_ACCNZ_u16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16_Myr4:
    ACCNZOpc = SHAVE::VAU_ACCNZ_u16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32:
    ACCNZOpc = SHAVE::VAU_ACCNZ_u32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32_Myr4:
    ACCNZOpc = SHAVE::VAU_ACCNZ_u32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16:
    ACCNZOpc = SHAVE::VAU_ACCNZ_f16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16_Myr4:
    ACCNZOpc = SHAVE::VAU_ACCNZ_f16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32:
    ACCNZOpc = SHAVE::VAU_ACCNZ_f32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32_Myr4:
    ACCNZOpc = SHAVE::VAU_ACCNZ_f32_pl_Myr4;
    break;
  }
  return ACCNZOpc;
}

unsigned SHAVEInstrInfo::GetMACPZOpcode(unsigned Opc) const {
  unsigned MACPZOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");

  // SAU.MACPZ
  case SHAVE::SAU_MACP_SEQ_i8:
    MACPZOpc = SHAVE::SAU_MACPZ_i8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i16:
    MACPZOpc = SHAVE::SAU_MACPZ_i16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i32:
    MACPZOpc = SHAVE::SAU_MACPZ_i32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u8:
    MACPZOpc = SHAVE::SAU_MACPZ_u8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u16:
    MACPZOpc = SHAVE::SAU_MACPZ_u16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u32:
    MACPZOpc = SHAVE::SAU_MACPZ_u32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f16:
    MACPZOpc = SHAVE::SAU_MACPZ_f16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f32:
    MACPZOpc = SHAVE::SAU_MACPZ_f32_pl;
    break;
  
  // VAU.MACPZ
  case SHAVE::VAU_MACP_SEQ_i8:
    MACPZOpc = SHAVE::VAU_MACPZ_i8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i8_Myr4:
    MACPZOpc = SHAVE::VAU_MACPZ_i8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i16:
    MACPZOpc = SHAVE::VAU_MACPZ_i16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i16_Myr4:
    MACPZOpc = SHAVE::VAU_MACPZ_i16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i32:
    MACPZOpc = SHAVE::VAU_MACPZ_i32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i32_Myr4:
    MACPZOpc = SHAVE::VAU_MACPZ_i32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u8:
    MACPZOpc = SHAVE::VAU_MACPZ_u8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u8_Myr4:
    MACPZOpc = SHAVE::VAU_MACPZ_u8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u16:
    MACPZOpc = SHAVE::VAU_MACPZ_u16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u16_Myr4:
    MACPZOpc = SHAVE::VAU_MACPZ_u16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u32:
    MACPZOpc = SHAVE::VAU_MACPZ_u32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u32_Myr4:
    MACPZOpc = SHAVE::VAU_MACPZ_u32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f16:
    MACPZOpc = SHAVE::VAU_MACPZ_f16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f16_Myr4:
    MACPZOpc = SHAVE::VAU_MACPZ_f16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f32:
    MACPZOpc = SHAVE::VAU_MACPZ_f32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f32_Myr4:
    MACPZOpc = SHAVE::VAU_MACPZ_f32_pl_Myr4;
    break;
  }
  return MACPZOpc;
}
unsigned SHAVEInstrInfo::GetMACNZOpcode(unsigned Opc) const {
  unsigned MACNZOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");
  
  // SAU.MACNZ
  case SHAVE::SAU_MACP_SEQ_i8:
    MACNZOpc = SHAVE::SAU_MACNZ_i8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i16:
    MACNZOpc = SHAVE::SAU_MACNZ_i16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i32:
    MACNZOpc = SHAVE::SAU_MACNZ_i32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u8:
    MACNZOpc = SHAVE::SAU_MACNZ_u8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u16:
    MACNZOpc = SHAVE::SAU_MACNZ_u16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u32:
    MACNZOpc = SHAVE::SAU_MACNZ_u32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f16:
    MACNZOpc = SHAVE::SAU_MACNZ_f16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f32:
    MACNZOpc = SHAVE::SAU_MACNZ_f32_pl;
    break;

  // VAU.MACNZ
  case SHAVE::VAU_MACP_SEQ_i8:
    MACNZOpc = SHAVE::VAU_MACNZ_i8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i8_Myr4:
    MACNZOpc = SHAVE::VAU_MACNZ_i8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i16:
    MACNZOpc = SHAVE::VAU_MACNZ_i16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i16_Myr4:
    MACNZOpc = SHAVE::VAU_MACNZ_i16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i32:
    MACNZOpc = SHAVE::VAU_MACNZ_i32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i32_Myr4:
    MACNZOpc = SHAVE::VAU_MACNZ_i32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u8:
    MACNZOpc = SHAVE::VAU_MACNZ_u8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u8_Myr4:
    MACNZOpc = SHAVE::VAU_MACNZ_u8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u16:
    MACNZOpc = SHAVE::VAU_MACNZ_u16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u16_Myr4:
    MACNZOpc = SHAVE::VAU_MACNZ_u16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u32:
    MACNZOpc = SHAVE::VAU_MACNZ_u32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u32_Myr4:
    MACNZOpc = SHAVE::VAU_MACNZ_u32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f16:
    MACNZOpc = SHAVE::VAU_MACNZ_f16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f16_Myr4:
    MACNZOpc = SHAVE::VAU_MACNZ_f16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f32:
    MACNZOpc = SHAVE::VAU_MACNZ_f32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f32_Myr4:
    MACNZOpc = SHAVE::VAU_MACNZ_f32_pl_Myr4;
    break;
  }
  return MACNZOpc;
}

unsigned SHAVEInstrInfo::GetACCNOpcode(unsigned Opc) const {
  unsigned ACCNOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");

  // SAU.ACCN
  case SHAVE::SAU_ACCP_SEQ_i8:
    ACCNOpc = SHAVE::SAU_ACCN_i8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i16:
    ACCNOpc = SHAVE::SAU_ACCN_i16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i32:
    ACCNOpc = SHAVE::SAU_ACCN_i32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u8:
    ACCNOpc = SHAVE::SAU_ACCN_u8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u16:
    ACCNOpc = SHAVE::SAU_ACCN_u16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u32:
    ACCNOpc = SHAVE::SAU_ACCN_u32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f16:
    ACCNOpc = SHAVE::SAU_ACCN_f16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f32:
    ACCNOpc = SHAVE::SAU_ACCN_f32_pl;
    break;

  // VAU.ACCN
  case SHAVE::VAU_ACCP_SEQ_i8:
    ACCNOpc = SHAVE::VAU_ACCN_i8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i8_Myr4:
    ACCNOpc = SHAVE::VAU_ACCN_i8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16:
    ACCNOpc = SHAVE::VAU_ACCN_i16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16_Myr4:
    ACCNOpc = SHAVE::VAU_ACCN_i16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32:
    ACCNOpc = SHAVE::VAU_ACCN_i32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32_Myr4:
    ACCNOpc = SHAVE::VAU_ACCN_i32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8:
    ACCNOpc = SHAVE::VAU_ACCN_u8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8_Myr4:
    ACCNOpc = SHAVE::VAU_ACCN_u8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16:
    ACCNOpc = SHAVE::VAU_ACCN_u16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16_Myr4:
    ACCNOpc = SHAVE::VAU_ACCN_u16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32:
    ACCNOpc = SHAVE::VAU_ACCN_u32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32_Myr4:
    ACCNOpc = SHAVE::VAU_ACCN_u32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16:
    ACCNOpc = SHAVE::VAU_ACCN_f16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16_Myr4:
    ACCNOpc = SHAVE::VAU_ACCN_f16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32:
    ACCNOpc = SHAVE::VAU_ACCN_f32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32_Myr4:
    ACCNOpc = SHAVE::VAU_ACCN_f32_pl_Myr4;
    break;
  }
  return ACCNOpc;
}

unsigned SHAVEInstrInfo::GetACCPOpcode(unsigned Opc) const {
  unsigned ACCPOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");
  
  // SAU.ACCP
  case SHAVE::SAU_ACCP_SEQ_i8:
    ACCPOpc = SHAVE::SAU_ACCP_i8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i16:
    ACCPOpc = SHAVE::SAU_ACCP_i16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i32:
    ACCPOpc = SHAVE::SAU_ACCP_i32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u8:
    ACCPOpc = SHAVE::SAU_ACCP_u8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u16:
    ACCPOpc = SHAVE::SAU_ACCP_u16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u32:
    ACCPOpc = SHAVE::SAU_ACCP_u32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f16:
    ACCPOpc = SHAVE::SAU_ACCP_f16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f32:
    ACCPOpc = SHAVE::SAU_ACCP_f32_pl;
    break;

  // VAU.ACCP
  case SHAVE::VAU_ACCP_SEQ_i8:
    ACCPOpc = SHAVE::VAU_ACCP_i8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i8_Myr4:
    ACCPOpc = SHAVE::VAU_ACCP_i8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16:
    ACCPOpc = SHAVE::VAU_ACCP_i16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16_Myr4:
    ACCPOpc = SHAVE::VAU_ACCP_i16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32:
    ACCPOpc = SHAVE::VAU_ACCP_i32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32_Myr4:
    ACCPOpc = SHAVE::VAU_ACCP_i32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8:
    ACCPOpc = SHAVE::VAU_ACCP_u8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8_Myr4:
    ACCPOpc = SHAVE::VAU_ACCP_u8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16:
    ACCPOpc = SHAVE::VAU_ACCP_u16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16_Myr4:
    ACCPOpc = SHAVE::VAU_ACCP_u16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32:
    ACCPOpc = SHAVE::VAU_ACCP_u32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32_Myr4:
    ACCPOpc = SHAVE::VAU_ACCP_u32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16:
    ACCPOpc = SHAVE::VAU_ACCP_f16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16_Myr4:
    ACCPOpc = SHAVE::VAU_ACCP_f16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32:
    ACCPOpc = SHAVE::VAU_ACCP_f32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32_Myr4:
    ACCPOpc = SHAVE::VAU_ACCP_f32_pl_Myr4;
    break;
  }
  return ACCPOpc;
}

unsigned SHAVEInstrInfo::GetMACPOpcode(unsigned Opc) const {
  unsigned MACPOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");

  // SAU.MACP
  case SHAVE::SAU_MACP_SEQ_i8:
    MACPOpc = SHAVE::SAU_MACP_i8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i16:
    MACPOpc = SHAVE::SAU_MACP_i16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i32:
    MACPOpc = SHAVE::SAU_MACP_i32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u8:
    MACPOpc = SHAVE::SAU_MACP_u8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u16:
    MACPOpc = SHAVE::SAU_MACP_u16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u32:
    MACPOpc = SHAVE::SAU_MACP_u32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f16:
    MACPOpc = SHAVE::SAU_MACP_f16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f32:
    MACPOpc = SHAVE::SAU_MACP_f32_pl;
    break;

  // VAU.MACP
  case SHAVE::VAU_MACP_SEQ_i8:
    MACPOpc = SHAVE::VAU_MACP_i8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i8_Myr4:
    MACPOpc = SHAVE::VAU_MACP_i8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i16:
    MACPOpc = SHAVE::VAU_MACP_i16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i16_Myr4:
    MACPOpc = SHAVE::VAU_MACP_i16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i32:
    MACPOpc = SHAVE::VAU_MACP_i32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i32_Myr4:
    MACPOpc = SHAVE::VAU_MACP_i32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u8:
    MACPOpc = SHAVE::VAU_MACP_u8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u8_Myr4:
    MACPOpc = SHAVE::VAU_MACP_u8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u16:
    MACPOpc = SHAVE::VAU_MACP_u16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u16_Myr4:
    MACPOpc = SHAVE::VAU_MACP_u16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u32:
    MACPOpc = SHAVE::VAU_MACP_u32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u32_Myr4:
    MACPOpc = SHAVE::VAU_MACP_u32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f16:
    MACPOpc = SHAVE::VAU_MACP_f16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f16_Myr4:
    MACPOpc = SHAVE::VAU_MACP_f16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f32:
    MACPOpc = SHAVE::VAU_MACP_f32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f32_Myr4:
    MACPOpc = SHAVE::VAU_MACP_f32_pl_Myr4;
    break;
  }
  return MACPOpc;
}
unsigned SHAVEInstrInfo::GetMACNOpcode(unsigned Opc) const {
  unsigned MACNOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");

  // SAU.MACN
  case SHAVE::SAU_MACP_SEQ_i8:
    MACNOpc = SHAVE::SAU_MACN_i8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i16:
    MACNOpc = SHAVE::SAU_MACN_i16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i32:
    MACNOpc = SHAVE::SAU_MACN_i32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u8:
    MACNOpc = SHAVE::SAU_MACN_u8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u16:
    MACNOpc = SHAVE::SAU_MACN_u16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u32:
    MACNOpc = SHAVE::SAU_MACN_u32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f16:
    MACNOpc = SHAVE::SAU_MACN_f16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f32:
    MACNOpc = SHAVE::SAU_MACN_f32_pl;
    break;
  
  // VAU.MACN
  case SHAVE::VAU_MACP_SEQ_i8:
    MACNOpc = SHAVE::VAU_MACN_i8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i8_Myr4:
    MACNOpc = SHAVE::VAU_MACN_i8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i16:
    MACNOpc = SHAVE::VAU_MACN_i16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i16_Myr4:
    MACNOpc = SHAVE::VAU_MACN_i16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i32:
    MACNOpc = SHAVE::VAU_MACN_i32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i32_Myr4:
    MACNOpc = SHAVE::VAU_MACN_i32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u8:
    MACNOpc = SHAVE::VAU_MACN_u8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u8_Myr4:
    MACNOpc = SHAVE::VAU_MACN_u8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u16:
    MACNOpc = SHAVE::VAU_MACN_u16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u16_Myr4:
    MACNOpc = SHAVE::VAU_MACN_u16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u32:
    MACNOpc = SHAVE::VAU_MACN_u32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u32_Myr4:
    MACNOpc = SHAVE::VAU_MACN_u32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f16:
    MACNOpc = SHAVE::VAU_MACN_f16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f16_Myr4:
    MACNOpc = SHAVE::VAU_MACN_f16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f32:
    MACNOpc = SHAVE::VAU_MACN_f32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f32_Myr4:
    MACNOpc = SHAVE::VAU_MACN_f32_pl_Myr4;
    break;
  }
  return MACNOpc;
}

unsigned SHAVEInstrInfo::GetACCPWOpcode(unsigned Opc) const {
  unsigned ACCPWOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");

  // SAU.ACCPW
  case SHAVE::SAU_ACCP_SEQ_i8:
    ACCPWOpc = SHAVE::SAU_ACCPW_i8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i16:
    ACCPWOpc = SHAVE::SAU_ACCPW_i16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i32:
    ACCPWOpc = SHAVE::SAU_ACCPW_i32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u8:
    ACCPWOpc = SHAVE::SAU_ACCPW_u8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u16:
    ACCPWOpc = SHAVE::SAU_ACCPW_u16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u32:
    ACCPWOpc = SHAVE::SAU_ACCPW_u32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f16:
    ACCPWOpc = SHAVE::SAU_ACCPW_f16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f32:
    ACCPWOpc = SHAVE::SAU_ACCPW_f32_pl;
    break;

  // VAU.ACCPW
  case SHAVE::VAU_ACCP_SEQ_i8:
    ACCPWOpc = SHAVE::VAU_ACCPW_i8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i8_Myr4:
    ACCPWOpc = SHAVE::VAU_ACCPW_i8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16:
    ACCPWOpc = SHAVE::VAU_ACCPW_i16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16_Myr4:
    ACCPWOpc = SHAVE::VAU_ACCPW_i16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32:
    ACCPWOpc = SHAVE::VAU_ACCPW_i32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32_Myr4:
    ACCPWOpc = SHAVE::VAU_ACCPW_i32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8:
    ACCPWOpc = SHAVE::VAU_ACCPW_u8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8_Myr4:
    ACCPWOpc = SHAVE::VAU_ACCPW_u8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16:
    ACCPWOpc = SHAVE::VAU_ACCPW_u16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16_Myr4:
    ACCPWOpc = SHAVE::VAU_ACCPW_u16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32:
    ACCPWOpc = SHAVE::VAU_ACCPW_u32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32_Myr4:
    ACCPWOpc = SHAVE::VAU_ACCPW_u32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16:
    ACCPWOpc = SHAVE::VAU_ACCPW_f16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16_Myr4:
    ACCPWOpc = SHAVE::VAU_ACCPW_f16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32:
    ACCPWOpc = SHAVE::VAU_ACCPW_f32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32_Myr4:
    ACCPWOpc = SHAVE::VAU_ACCPW_f32_pl_Myr4;
    break;
  }
  return ACCPWOpc;
}

unsigned SHAVEInstrInfo::GetACCNWOpcode(unsigned Opc) const {
  unsigned ACCNWOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");

  // SAU.ACCNW
  case SHAVE::SAU_ACCP_SEQ_i8:
    ACCNWOpc = SHAVE::SAU_ACCNW_i8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i16:
    ACCNWOpc = SHAVE::SAU_ACCNW_i16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_i32:
    ACCNWOpc = SHAVE::SAU_ACCNW_i32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u8:
    ACCNWOpc = SHAVE::SAU_ACCNW_u8_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u16:
    ACCNWOpc = SHAVE::SAU_ACCNW_u16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_u32:
    ACCNWOpc = SHAVE::SAU_ACCNW_u32_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f16:
    ACCNWOpc = SHAVE::SAU_ACCNW_f16_pl;
    break;
  case SHAVE::SAU_ACCP_SEQ_f32:
    ACCNWOpc = SHAVE::SAU_ACCNW_f32_pl;
    break;

  // VAU.ACCNW
  case SHAVE::VAU_ACCP_SEQ_i8:
    ACCNWOpc = SHAVE::VAU_ACCNW_i8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i8_Myr4:
    ACCNWOpc = SHAVE::VAU_ACCNW_i8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16:
    ACCNWOpc = SHAVE::VAU_ACCNW_i16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16_Myr4:
    ACCNWOpc = SHAVE::VAU_ACCNW_i16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32:
    ACCNWOpc = SHAVE::VAU_ACCNW_i32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_i32_Myr4:
    ACCNWOpc = SHAVE::VAU_ACCNW_i32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8:
    ACCNWOpc = SHAVE::VAU_ACCNW_u8_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u8_Myr4:
    ACCNWOpc = SHAVE::VAU_ACCNW_u8_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16:
    ACCNWOpc = SHAVE::VAU_ACCNW_u16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u16_Myr4:
    ACCNWOpc = SHAVE::VAU_ACCNW_u16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32:
    ACCNWOpc = SHAVE::VAU_ACCNW_u32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_u32_Myr4:
    ACCNWOpc = SHAVE::VAU_ACCNW_u32_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16:
    ACCNWOpc = SHAVE::VAU_ACCNW_f16_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f16_Myr4:
    ACCNWOpc = SHAVE::VAU_ACCNW_f16_pl_Myr4;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32:
    ACCNWOpc = SHAVE::VAU_ACCNW_f32_pl;
    break;
  case SHAVE::VAU_ACCP_SEQ_f32_Myr4:
    ACCNWOpc = SHAVE::VAU_ACCNW_f32_pl_Myr4;
    break;
  }
  return ACCNWOpc;
}

unsigned SHAVEInstrInfo::GetMACPWOpcode(unsigned Opc) const {
  unsigned MACPWOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");

  // SAU.MACPW
  case SHAVE::SAU_MACP_SEQ_i8:
    MACPWOpc = SHAVE::SAU_MACPW_i8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i16:
    MACPWOpc = SHAVE::SAU_MACPW_i16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i32:
    MACPWOpc = SHAVE::SAU_MACPW_i32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u8:
    MACPWOpc = SHAVE::SAU_MACPW_u8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u16:
    MACPWOpc = SHAVE::SAU_MACPW_u16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u32:
    MACPWOpc = SHAVE::SAU_MACPW_u32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f16:
    MACPWOpc = SHAVE::SAU_MACPW_f16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f32:
    MACPWOpc = SHAVE::SAU_MACPW_f32_pl;
    break;

  // VAU.MACPW
  case SHAVE::VAU_MACP_SEQ_i8:
    MACPWOpc = SHAVE::VAU_MACPW_i8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i8_Myr4:
    MACPWOpc = SHAVE::VAU_MACPW_i8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i16:
    MACPWOpc = SHAVE::VAU_MACPW_i16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i16_Myr4:
    MACPWOpc = SHAVE::VAU_MACPW_i16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i32:
    MACPWOpc = SHAVE::VAU_MACPW_i32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i32_Myr4:
    MACPWOpc = SHAVE::VAU_MACPW_i32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u8:
    MACPWOpc = SHAVE::VAU_MACPW_u8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u8_Myr4:
    MACPWOpc = SHAVE::VAU_MACPW_u8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u16:
    MACPWOpc = SHAVE::VAU_MACPW_u16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u16_Myr4:
    MACPWOpc = SHAVE::VAU_MACPW_u16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u32:
    MACPWOpc = SHAVE::VAU_MACPW_u32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u32_Myr4:
    MACPWOpc = SHAVE::VAU_MACPW_u32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f16:
    MACPWOpc = SHAVE::VAU_MACPW_f16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f16_Myr4:
    MACPWOpc = SHAVE::VAU_MACPW_f16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f32:
    MACPWOpc = SHAVE::VAU_MACPW_f32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f32_Myr4:
    MACPWOpc = SHAVE::VAU_MACPW_f32_pl_Myr4;
    break;
  }
  return MACPWOpc;
}

unsigned SHAVEInstrInfo::GetMACNWOpcode(unsigned Opc) const {
  unsigned MACNWOpc = 0;

  switch (Opc) {
  default:
    assert(0 && "Invalid accumulator opcode.");

  // SAU.MACNW
  case SHAVE::SAU_MACP_SEQ_i8:
    MACNWOpc = SHAVE::SAU_MACNW_i8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i16:
    MACNWOpc = SHAVE::SAU_MACNW_i16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_i32:
    MACNWOpc = SHAVE::SAU_MACNW_i32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u8:
    MACNWOpc = SHAVE::SAU_MACNW_u8_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u16:
    MACNWOpc = SHAVE::SAU_MACNW_u16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_u32:
    MACNWOpc = SHAVE::SAU_MACNW_u32_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f16:
    MACNWOpc = SHAVE::SAU_MACNW_f16_pl;
    break;
  case SHAVE::SAU_MACP_SEQ_f32:
    MACNWOpc = SHAVE::SAU_MACNW_f32_pl;
    break;

  // VAU.MACNW
  case SHAVE::VAU_MACP_SEQ_i8:
    MACNWOpc = SHAVE::VAU_MACNW_i8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i8_Myr4:
    MACNWOpc = SHAVE::VAU_MACNW_i8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i16:
    MACNWOpc = SHAVE::VAU_MACNW_i16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i16_Myr4:
    MACNWOpc = SHAVE::VAU_MACNW_i16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_i32:
    MACNWOpc = SHAVE::VAU_MACNW_i32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_i32_Myr4:
    MACNWOpc = SHAVE::VAU_MACNW_i32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u8:
    MACNWOpc = SHAVE::VAU_MACNW_u8_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u8_Myr4:
    MACNWOpc = SHAVE::VAU_MACNW_u8_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u16:
    MACNWOpc = SHAVE::VAU_MACNW_u16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u16_Myr4:
    MACNWOpc = SHAVE::VAU_MACNW_u16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_u32:
    MACNWOpc = SHAVE::VAU_MACNW_u32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_u32_Myr4:
    MACNWOpc = SHAVE::VAU_MACNW_u32_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f16:
    MACNWOpc = SHAVE::VAU_MACNW_f16_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f16_Myr4:
    MACNWOpc = SHAVE::VAU_MACNW_f16_pl_Myr4;
    break;
  case SHAVE::VAU_MACP_SEQ_f32:
    MACNWOpc = SHAVE::VAU_MACNW_f32_pl;
    break;
  case SHAVE::VAU_MACP_SEQ_f32_Myr4:
    MACNWOpc = SHAVE::VAU_MACNW_f32_pl_Myr4;
    break;
  }
  return MACNWOpc;
}

unsigned SHAVEInstrInfo::FUnitsToPCXXMask(FUnitMask FUnits) const {
  unsigned Mask = 0;

  if (FUnits & FmIAU)
    Mask |= (1u << 0);
  if (FUnits & FmSAU)
    Mask |= (1u << 1);
  if (FUnits & FmCMU)
    Mask |= (1u << 2);
  if (FUnits & FmVAU)
    Mask |= (1u << 3);
  if (FUnits & FmLSU0)
    Mask |= (1u << 4);
  if (FUnits & FmLSU1)
    Mask |= (1u << 5);

  return Mask;
}

FUnitMask SHAVEInstrInfo::PCXXMaskToFUnits(int64_t Mask) const {
  FUnitMask FUnits = FmNone;

  if (Mask & (1 << 0))
    FUnits |= FmIAU;
  if (Mask & (1 << 1))
    FUnits |= FmSAU;
  if (Mask & (1 << 2))
    FUnits |= FmCMU;
  if (Mask & (1 << 3))
    FUnits |= FmVAU;
  if (Mask & (1 << 4))
    FUnits |= FmLSU0;
  if (Mask & (1 << 5))
    FUnits |= FmLSU1;

  return FUnits;
}

bool SHAVEInstrInfo::isPredicate(MachineInstr *MI, SHAVECC::CondCode &CC,
                                 FUnitMask &PredUnits) const {
  unsigned Opc = MI->getOpcode();

  switch (Opc) {
  default:
    // Unknown PEU instructions do not predicate the instruction.
    CC = SHAVECC::AL;
    PredUnits = FmNone;
    return false;
  case SHAVE::PEU_PVV_8:
  case SHAVE::PEU_PVV_16:
  case SHAVE::PEU_PVV_32:
  case SHAVE::PEU_PVS_8:
  case SHAVE::PEU_PVS_16:
  case SHAVE::PEU_PVS_32:
  case SHAVE::PEU_PC1C:
  case SHAVE::PEU_PC1I:
    CC = (SHAVECC::CondCode)MI->getOperand(0).getImm();
    PredUnits = FmAllUnits & ~FmPEU;
    return true;
  case SHAVE::PEU_PCCX_EQ:
  case SHAVE::PEU_PCCX_NEQ:
    CC = (Opc == SHAVE::PEU_PCCX_EQ) ? SHAVECC::EQ : SHAVECC::NEQ;
    PredUnits = PCXXMaskToFUnits(MI->getOperand(0).getImm());
    return true;
  }
}

bool SHAVEInstrInfo::setPredicate(MachineInstr *MI, SHAVECC::CondCode CC,
                                  FUnitMask PredUnits) const {
  unsigned Opc = MI->getOpcode();
  unsigned NewOpc = 0;

  switch (Opc) {
  default:
    // Unknown instructions do not predicate the instruction.
    return false;
  case SHAVE::PEU_PVV_8:
  case SHAVE::PEU_PVV_16:
  case SHAVE::PEU_PVV_32:
  case SHAVE::PEU_PVS_8:
  case SHAVE::PEU_PVS_16:
  case SHAVE::PEU_PVS_32:
  case SHAVE::PEU_PC1I:
    // Do not allow these instructions to be partially predicated.
    if ((PredUnits | FmPEU) != FmAllUnits)
      return false;
    MI->getOperand(0).setImm((int64_t)CC);
    return true;
  case SHAVE::PEU_PC1C:
  case SHAVE::PEU_PCCX_EQ:
  case SHAVE::PEU_PCCX_NEQ:
    // These instructions support partial predication for two CCs.
    if ((CC != SHAVECC::EQ) && (CC != SHAVECC::NEQ))
      return false;
    NewOpc = ((CC == SHAVECC::EQ) ? SHAVE::PEU_PCCX_EQ : SHAVE::PEU_PCCX_NEQ);
    MI->setDesc(get(NewOpc));
    MI->getOperand(0).setImm(FUnitsToPCXXMask(PredUnits));
    return true;
  }
}

bool SHAVEInstrInfo::isReallyTriviallyReMaterializable(
    const MachineInstr &MI) const {
  unsigned Opc = MI.getOpcode();

  switch (Opc) {
  case SHAVE::LDImm32:
    return true;

  default:
    return false;
  }
}

bool SHAVEInstrInfo::hasFeature(unsigned int feature) const {
  return SHAVEST.hasFeature(feature);
}

bool SHAVEInstrInfo::hasSAU() const {
  return SHAVEST.hasSAU();
}
bool SHAVEInstrInfo::hasVAU() const {
  return SHAVEST.hasVAU();
}

// Disable StashRetrieve functionality on SHAVE512 until EISW-76925 is fixed
bool SHAVEInstrInfo::isStashRetrieveEnabledForArch() const {
  return hasVAU() && hasFeature(SHAVE::HasVRF128_Feature);
}

unsigned int SHAVEInstrInfo::getCMXCutSize() const {
  return SHAVEST.getCMXCutSize();
}

unsigned int SHAVEInstrInfo::getCMXNumberOfCuts() const {
  return SHAVEST.getCMXNumberOfCuts();
}

// Fetch the maximum range (in bytes) of a BRU.BRA for this target
unsigned int SHAVEInstrInfo::getBRARange() const {
  return SHAVEST.getBRARange();
}

unsigned int SHAVEInstrInfo::getLDOSTO_OffsetBits() const {
  return SHAVEST.getLDOSTO_OffsetBits();
}
