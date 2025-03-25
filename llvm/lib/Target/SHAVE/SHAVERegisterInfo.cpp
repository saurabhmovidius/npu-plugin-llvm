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
// File       :  SHAVERegisterInfo.cpp
// Description:  Emit prolog and epilog code, handle frame index
//               and calling convention registers
// ---------------------------------------------------------------------------


#define DEBUG_TYPE "reginfo"

#include <cstdlib>

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVE.h"
#include "SHAVEInstrInfo.h"
#include "SHAVEFrameLowering.h"
#include "SHAVEMachineFunctionInfo.h"
#include "SHAVERegisterInfo.h"
#include "SHAVESubtarget.h"

#define GET_REGINFO_TARGET_DESC
#include "SHAVEGenRegisterInfo.inc"

using namespace llvm;


SHAVERegisterInfo::SHAVERegisterInfo(const SHAVESubtarget &subtarget, const SHAVEInstrInfo &tii)
: SHAVEGenRegisterInfo(getLinkReg()),
  Subtarget(subtarget),
  TII(tii)
{}

// Register file handling
SHAVERegisterInfo::RegisterFileTypes SHAVERegisterInfo::getRegisterFileTypeForReg(unsigned regNo) const {
  if (isIRFRegister(regNo))
    return IRF_RegFile;
  if (isVRFRegister(regNo))
    return VRF_RegFile;
  return Unknown_RegFile;
}

bool SHAVERegisterInfo::isIRFRegister(unsigned regNo) const {
  if (Register::isPhysicalRegister(regNo))
    if ((regNo >= SHAVE::I0) && (regNo <= SHAVE::I31))
      return true;
  return false;
}
bool SHAVERegisterInfo::isVRFRegister(unsigned regNo) const {
  if (Register::isPhysicalRegister(regNo))
    if ((regNo >= SHAVE::V0) && (regNo <= SHAVE::V31))
      return true;
  return false;
}

// Return the register class to use to hold pointers
const TargetRegisterClass *SHAVERegisterInfo::getPointerRegClass(const MachineFunction & /*MF*/, unsigned /*Kind*/) const {
  return &SHAVE::IRF32RegClass;
}

const MCPhysReg *SHAVERegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  // Registers to be preserved across function calls,
  // i.e. it is the duty of the current function to save and restore them
  if (MF->getFunction().hasDLLExportStorageClass()) {
    return CSR_EmptySHAVE_SaveList;
  }
  switch (MF->getFunction().getCallingConv()) {
  case CallingConv::X86_StdCall:
    return CSR_StdSHAVE_SaveList;
  default:
    return CSR_OrigSHAVE_SaveList;
  }
}

const uint32_t *SHAVERegisterInfo::getCallPreservedMask(const MachineFunction &MF, CallingConv::ID ID) const {
  // FIXME_37: Movidius - LLVM v3.7 adds the 'MachineFunction' argument; what should we do with this?
  switch (ID) {
  case CallingConv::X86_StdCall:
    return CSR_StdSHAVE_RegMask;
  default:
    return CSR_OrigSHAVE_RegMask;
  }
}

static void addAliasedRegs(unsigned Reg, BitVector &Regs,
                           const SHAVERegisterInfo *TRI) {
  for (MCRegAliasIterator I(Reg, TRI, true); I.isValid(); ++I)
    Regs.set(*I);
}

BitVector SHAVERegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  addAliasedRegs(getSPReg(), Reserved, this); // stack pointer register
  addAliasedRegs(getLinkReg(), Reserved, this); // link-register
  addAliasedRegs(getFPReg(), Reserved, this); // frame pointer register
  // FIXME: Movidius - need to wean 'moviCompile' off requiring a physical scratch register
  addAliasedRegs(getScratchReg(), Reserved, this); // scratch register

  addAliasedRegs(SHAVE::P_SVID, Reserved, this); // Required for implementation of the get CPU id builtin
  addAliasedRegs(SHAVE::P_ID_Myr4, Reserved, this);
  addAliasedRegs(SHAVE::P_CFG, Reserved, this); // Required for implementation of the get/setP_CFG builtin
  // For Myriad2.3 we need to reserve the accumulators so the Machine Verifier doesn't
  // give out about undefined registers
  addAliasedRegs(SHAVE::V_ACC0x, Reserved, this);
  addAliasedRegs(SHAVE::V_ACC1x, Reserved, this);
  addAliasedRegs(SHAVE::S_ACC0, Reserved, this);
  addAliasedRegs(SHAVE::S_ACC1, Reserved, this);
  addAliasedRegs(SHAVE::V_STATE, Reserved, this);
  addAliasedRegs(SHAVE::S_STATE, Reserved, this);

  for (const auto &reg : SHAVE::TRFRegClass)
    addAliasedRegs(reg, Reserved, this);

  for (const auto &reg : SHAVE::PEU_TRFRegClass)
    addAliasedRegs(reg, Reserved, this);

  for (const auto &reg : SHAVE::Combined_TRFRegClass)
    addAliasedRegs(reg, Reserved, this);

  return Reserved;
}

// eliminateFrameIndex - eliminate abstract frame indices from instructions that may use them
bool SHAVERegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                            int SPAdj,
                                            unsigned FIOperandNum,
                                            RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  DebugLoc dl = MI.getDebugLoc();

  const MCInstrDesc &TID = MI.getDesc();

  bool isOffsetMemOp = false; // Is this a LDO/STO or just LD/ST?
  int spOffset;
  int Offset = 0;

  assert(SPAdj == 0 && "Unexpected SPAdj != 0");

  const SHAVEFrameLowering *SFL = static_cast<const SHAVEFrameLowering*>(MF.getSubtarget().getFrameLowering());
  int baseStackSize = MFI.getStackSize();
  int stackSize = SFL->computeStackSize(MF);
  int frameIndex = MI.getOperand(FIOperandNum).getIndex();

  // If the function has a frame pointer, use it for reference, else use the stack pointer
  // outgoing parameters are always passed relative to the stack pointer
  Register refReg = getFrameRegister(MF);
  int LoadStoreOffset = 0;

  spOffset = MFI.getObjectOffset(frameIndex);

  if (TII.findLoadStoreOffset(&MI, &LoadStoreOffset)) {
    spOffset += LoadStoreOffset;
    isOffsetMemOp = true;
  }

  if ((MF.getFrameInfo().isImmutableObjectIndex(frameIndex)) && (spOffset >= 0)) {
    if (SHAVEConflicts::check_isLoad(TID.getOpcode())) {
      // Receive parameters from caller function
      Offset = stackSize + spOffset;
    } else if (SHAVEConflicts::check_isStore(TID.getOpcode())) {
      // Send parameters to callee function
      Offset = spOffset;
      // Always pass outgoing params relative to the stack pointer
      refReg = getSPReg();
    } else {
      // This is reached by va_arg...
      Offset = stackSize + spOffset;
      refReg = getSPReg();
    }
  } else if (spOffset < 0)
      // FIXME: Movidius - why spOffset < 0 means baseStackSize is used instead of stackSize ?
      Offset = baseStackSize + spOffset;
  else
      Offset = stackSize + spOffset;

  if (MI.getOpcode() == SHAVE::IAU_ADD_32_imm) {
    // This comes from a special handling of a frame index node that reaches instruction selection
    // i.e. it is not consumed by a load/store
    bool PositiveOffset = (Offset >= 0);
    unsigned Opcode = II->getOpcode();
    const int MaxOffset = 255;

    Offset = (Offset >= 0) ? Offset : -Offset; // get the absolute value

    // Replace the frame index operand to the stack pointer register.
    II->getOperand(1).ChangeToRegister(refReg, false);

    if (Offset <= MaxOffset) {
      // The offset fits in the immediate.
      Opcode = PositiveOffset ? SHAVE::IAU_ADD_32_imm : SHAVE::IAU_SUB_32_imm;
      II->getOperand(2).ChangeToImmediate(Offset);
    } else {
      // The offset does not fit in the immediate, materialize the offset.
      Opcode = PositiveOffset ? SHAVE::IAU_ADD_32 : SHAVE::IAU_SUB_32;
      // FIXME: Movidius - we need to wean 'moviCompile' off requiring a physical scratch register
      unsigned tmpPhysicalReg = SHAVERegisterInfo::getScratchReg();

      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(SHAVE::LDImm32), tmpPhysicalReg)
          .addImm(Offset);
      II->getOperand(2).ChangeToRegister(tmpPhysicalReg, false);
    }

    II->setDesc(TII.get(Opcode));

    // FIXME: Movidius - remove this exit point
    return false;
  }

  if (isOffsetMemOp) {
    // Maximum LSU_STO_i32 immediate on Myriad4 is 1023
    const int MaxOffset = TII.hasFeature(SHAVE::HasLDOSTOMode_Feature) ? 0x3FF : 0x3FFF;
    if (Offset <= MaxOffset) {
      MI.getOperand(FIOperandNum).ChangeToRegister(refReg, false);
      MI.getOperand(FIOperandNum + 1).setImm(Offset);
    } else {
      // FIXME: Movidius - we need to wean 'moviCompile' off requiring a physical scratch register
      unsigned tmpPhysicalReg = SHAVERegisterInfo::getScratchReg();

      BuildMI(MBB, II, dl, TII.get(SHAVE::LDImm32), tmpPhysicalReg).addImm(Offset);
      TII.finaliseMI(BuildMI(MBB, II, dl, TII.get(SHAVE::IAU_ADD_32), tmpPhysicalReg)
         .addReg(refReg).addReg(tmpPhysicalReg));

      MI.getOperand(FIOperandNum).ChangeToRegister(tmpPhysicalReg, false);
      MI.getOperand(FIOperandNum + 1).setImm(0);
    }
  } else {
    // This load/store operation has no immediate offset field
    // so we need to insert another operation to compute the offset
    unsigned Opcode = 0;

    // All this is for choosing appropriate instructions for computing the
    // memory address from where to load/store
    if ((0 < Offset) && (Offset <= 255))
      Opcode = SHAVE::IAU_ADD_32_imm; // Positive offset, fits 8 bits in the imm field of IAU.ADD
    else if (Offset > 255)
      Opcode = SHAVE::IAU_ADD_32; // Positive offset, doesn't fit 8 bits in the imm field of IAU.ADD
    else if ((-255 <= Offset) && (Offset < 0)) {
      // Negative offset, fits the 8 imm bits of IAU.SUB
      Opcode = SHAVE::IAU_SUB_32_imm;
      Offset = -Offset;
    } else if (Offset < -255) {
      // Negative offset, doesn't fit the 8 imm bits of IAU.SUB
      Opcode = SHAVE::IAU_SUB_32;
      Offset = -Offset;
    }

    // FIXME: Movidius - we need to wean 'moviCompile' off requiring a physical scratch register
    unsigned tmpPhysicalReg = SHAVERegisterInfo::getScratchReg();

    // Now the Offset is positive for sure
    if (Offset > 0xFF) {
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(SHAVE::LDImm32), tmpPhysicalReg)
        .addImm(Offset);
      TII.finaliseMI(BuildMI(MBB, II, MI.getDebugLoc(), TII.get(Opcode), tmpPhysicalReg)
        .addReg(tmpPhysicalReg).addReg(refReg));
      MI.getOperand(FIOperandNum).ChangeToRegister(tmpPhysicalReg, false);
    } else if (Offset == 0)
      MI.getOperand(FIOperandNum).ChangeToRegister(refReg, false); // If there's no offset, use the stack pointer straight-forward
    else {
      TII.finaliseMI(BuildMI(MBB, II, MI.getDebugLoc(), TII.get(Opcode), tmpPhysicalReg)
        .addReg(refReg).addImm(Offset));
      MI.getOperand(FIOperandNum).ChangeToRegister(tmpPhysicalReg, false);
    }
  }

  return false;
}



Register SHAVERegisterInfo::getFrameRegister(MachineFunction const &MF) const {
  if(MF.getSubtarget().getFrameLowering()->hasFP(MF))
    return getFPReg();
  else
    return getSPReg();
}

unsigned SHAVERegisterInfo::getRegPressureLimit(const TargetRegisterClass *RC,
                                                MachineFunction &MF) const {
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
  const unsigned NumRegs = 32;
  const unsigned RegThreshold = NumRegs - (NumRegs / 4);
  unsigned FP = TFI->hasFP(MF) ? 1 : 0;

  switch (RC->getID()) {
  default:
    return 0;
  case SHAVE::IRF8_q0RegClassID:
  case SHAVE::IRF16_lRegClassID:
  case SHAVE::IRF32RegClassID:
    return RegThreshold - FP;
  case SHAVE::VRF32_q0RegClassID:
  case SHAVE::VRF64_lRegClassID:
  case SHAVE::VRF128RegClassID:
    return RegThreshold;
  }
}

const TargetRegisterClass *
  SHAVERegisterInfo::getVectorStashClass(const TargetRegisterClass *RC,
                                         const MachineFunction &MF) const {
  // Stash-retrieve needs to be ported to 512b vectors
  if (TII.hasFeature(SHAVE::HasVRF512_Feature))
    return nullptr;

  // We handle only 32-bit IRF-> 128-bit VRF stashes
  if (SHAVEOptions::EnableStashRetrieve &&
      TII.isStashRetrieveEnabledForArch() &&
      (RC->getID() == SHAVE::IRF32RegClassID))
    return &SHAVE::VRF32_q0_1RegClass;
  else
    return nullptr;
}

bool SHAVERegisterInfo::trackLivenessAfterRegAlloc(const MachineFunction &MF) const {
  return true;
}


bool SHAVERegisterInfo::getPhysicalRegisterData(unsigned int reg, SHAVERegisterInfo::SHAVEPhysicalRegInfo &info, bool includeTRF) const {
  if (!Register::isPhysicalRegister(reg))
    return false;

  const TargetRegisterClass *minimalClass = getMinimalPhysRegClassLLT(reg);
  if (minimalClass == nullptr)
    return false;

  auto returnClass = [&info]
                     (const TargetRegisterClass *superClass, unsigned int reg1, unsigned int reg2 = 0) {
                        info.regClass = superClass;
                        info.subregs = { reg1, reg2 };
                        return true;
                      };

#define cases(RegClassPrefix, SuperClassPrefix, SubRegPrefix, Count) \
  case SHAVE::RegClassPrefix##Count##RegClassID: return returnClass(&SHAVE::SuperClassPrefix##RegClass, SHAVE::SubRegPrefix##_##Count);

  // Each case statement here will either:
  //   - assign the super-register class corresponding to the specified sub-register class to "regClass"
  //   - assign the corresponding sub-register index to subreg1
  // OR in the case of artificial super-registers (like IRF64):
  //   - assign the artificial super-register class to "regClass"
  //   - assign the sub-register indexes corresponding to the "real" registers that make it up to "subreg1" and "subreg2"
  // AND return true
  switch(minimalClass->getID()) {
  // IRF64
  case SHAVE::IRF64RegClassID:   return returnClass(&SHAVE::IRF64RegClass, SHAVE::vsub32_0, SHAVE::vsub32_1);
  // IRF32
  case SHAVE::IRF32RegClassID:   return returnClass(nullptr, 0);
  // IRF16
  case SHAVE::IRF16_lRegClassID: return returnClass(&SHAVE::IRF32RegClass, SHAVE::hsub_0);
  case SHAVE::IRF16_hRegClassID: return returnClass(&SHAVE::IRF32RegClass, SHAVE::hsub_1);
  // IRF8
  cases(IRF8_q, IRF32, qsub, 0)
  cases(IRF8_q, IRF32, qsub, 1)
  cases(IRF8_q, IRF32, qsub, 2)
  cases(IRF8_q, IRF32, qsub, 3)
  // WVRF512
  case SHAVE::WVRF512RegClassID: return returnClass(nullptr, 0);
  // WVRF256
  cases(WVRF256_, WVRF512, wsub_256, 0)
  cases(WVRF256_, WVRF512, wsub_256, 1)
  // WVRF128
  cases(WVRF128_, WVRF512, wsub_128, 0)
  cases(WVRF128_, WVRF512, wsub_128, 1)
  cases(WVRF128_, WVRF512, wsub_128, 2)
  cases(WVRF128_, WVRF512, wsub_128, 3)
  // WVRF64
  cases(WVRF64_, WVRF512, wsub_64, 0)
  cases(WVRF64_, WVRF512, wsub_64, 1)
  cases(WVRF64_, WVRF512, wsub_64, 2)
  cases(WVRF64_, WVRF512, wsub_64, 3)
  cases(WVRF64_, WVRF512, wsub_64, 4)
  cases(WVRF64_, WVRF512, wsub_64, 5)
  cases(WVRF64_, WVRF512, wsub_64, 6)
  cases(WVRF64_, WVRF512, wsub_64, 7)
  // WVRF32
  cases(WVRF32_, WVRF512, wsub_32, 0)
  cases(WVRF32_, WVRF512, wsub_32, 1)
  cases(WVRF32_, WVRF512, wsub_32, 2)
  cases(WVRF32_, WVRF512, wsub_32, 3)
  cases(WVRF32_, WVRF512, wsub_32, 4)
  cases(WVRF32_, WVRF512, wsub_32, 5)
  cases(WVRF32_, WVRF512, wsub_32, 6)
  cases(WVRF32_, WVRF512, wsub_32, 7)
  cases(WVRF32_, WVRF512, wsub_32, 8)
  cases(WVRF32_, WVRF512, wsub_32, 9)
  cases(WVRF32_, WVRF512, wsub_32, 10)
  cases(WVRF32_, WVRF512, wsub_32, 11)
  cases(WVRF32_, WVRF512, wsub_32, 12)
  cases(WVRF32_, WVRF512, wsub_32, 13)
  cases(WVRF32_, WVRF512, wsub_32, 14)
  cases(WVRF32_, WVRF512, wsub_32, 15)
  // WVRF16
  cases(WVRF16_, WVRF512, wsub_16, 0)
  cases(WVRF16_, WVRF512, wsub_16, 1)
  cases(WVRF16_, WVRF512, wsub_16, 2)
  cases(WVRF16_, WVRF512, wsub_16, 3)
  cases(WVRF16_, WVRF512, wsub_16, 4)
  cases(WVRF16_, WVRF512, wsub_16, 5)
  cases(WVRF16_, WVRF512, wsub_16, 6)
  cases(WVRF16_, WVRF512, wsub_16, 7)
  cases(WVRF16_, WVRF512, wsub_16, 8)
  cases(WVRF16_, WVRF512, wsub_16, 9)
  cases(WVRF16_, WVRF512, wsub_16, 10)
  cases(WVRF16_, WVRF512, wsub_16, 11)
  cases(WVRF16_, WVRF512, wsub_16, 12)
  cases(WVRF16_, WVRF512, wsub_16, 13)
  cases(WVRF16_, WVRF512, wsub_16, 14)
  cases(WVRF16_, WVRF512, wsub_16, 15)
  cases(WVRF16_, WVRF512, wsub_16, 16)
  cases(WVRF16_, WVRF512, wsub_16, 17)
  cases(WVRF16_, WVRF512, wsub_16, 18)
  cases(WVRF16_, WVRF512, wsub_16, 19)
  cases(WVRF16_, WVRF512, wsub_16, 20)
  cases(WVRF16_, WVRF512, wsub_16, 21)
  cases(WVRF16_, WVRF512, wsub_16, 22)
  cases(WVRF16_, WVRF512, wsub_16, 23)
  cases(WVRF16_, WVRF512, wsub_16, 24)
  cases(WVRF16_, WVRF512, wsub_16, 25)
  cases(WVRF16_, WVRF512, wsub_16, 26)
  cases(WVRF16_, WVRF512, wsub_16, 27)
  cases(WVRF16_, WVRF512, wsub_16, 28)
  cases(WVRF16_, WVRF512, wsub_16, 29)
  cases(WVRF16_, WVRF512, wsub_16, 30)
  cases(WVRF16_, WVRF512, wsub_16, 31)
  // VRF128
  case SHAVE::VRF128RegClassID: return returnClass(nullptr, 0);
  // VRF64
  case SHAVE::VRF64_lRegClassID: return returnClass(&SHAVE::VRF128RegClass, SHAVE::hsub_0);
  case SHAVE::VRF64_hRegClassID: return returnClass(&SHAVE::VRF128RegClass, SHAVE::hsub_1);
  // VRF32
  cases(VRF32_q, VRF128, qsub, 0)
  cases(VRF32_q, VRF128, qsub, 1)
  cases(VRF32_q, VRF128, qsub, 2)
  cases(VRF32_q, VRF128, qsub, 3)
  // VRF16
  cases(VRF16_e, VRF128, vsub16, 0)
  cases(VRF16_e, VRF128, vsub16, 1)
  cases(VRF16_e, VRF128, vsub16, 2)
  cases(VRF16_e, VRF128, vsub16, 3)
  cases(VRF16_e, VRF128, vsub16, 4)
  cases(VRF16_e, VRF128, vsub16, 5)
  cases(VRF16_e, VRF128, vsub16, 6)
  cases(VRF16_e, VRF128, vsub16, 7)
  // TRF
  case SHAVE::PEU_TRFRegClassID:
    if (!includeTRF)
      return false;

    switch(reg) {
    // Match C_CMU0
    case SHAVE::CC_CMU0:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_0);
    case SHAVE::CC_CMU1:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_1);
    case SHAVE::CC_CMU2:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_2);
    case SHAVE::CC_CMU3:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_3);
    case SHAVE::CC_CMU4:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_4);
    case SHAVE::CC_CMU5:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_5);
    case SHAVE::CC_CMU6:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_6);
    case SHAVE::CC_CMU7:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_7);
    // Match C_CMU1
    case SHAVE::CC_CMU8:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_0);
    case SHAVE::CC_CMU9:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_1);
    case SHAVE::CC_CMU10: return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_2);
    case SHAVE::CC_CMU11: return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_3);
    case SHAVE::CC_CMU12: return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_4);
    case SHAVE::CC_CMU13: return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_5);
    case SHAVE::CC_CMU14: return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_6);
    case SHAVE::CC_CMU15: return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_7);
    // Match C_CSI
    case SHAVE::CC_SAU0:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_0);
    case SHAVE::CC_SAU1:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_1);
    case SHAVE::CC_SAU2:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_2);
    case SHAVE::CC_SAU3:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_3);
    case SHAVE::CC_IAU0:  return returnClass(&SHAVE::TRFRegClass, SHAVE::cmu_idx_4);
    default: llvm_unreachable("Unhandled member of PEU_TRF Register Class");
    }
  case SHAVE::TRFRegClassID:
    if (!includeTRF)
      return false;
    return returnClass(nullptr, 0);
  case SHAVE::Combined_TRFRegClassID:
    if (!includeTRF)
      return false;
    switch(reg) {
    case SHAVE::C_CMU_0_3:  return returnClass(&SHAVE::Combined_TRFRegClass, SHAVE::cmu_idx_0);
    case SHAVE::C_CMU_0_7:  return returnClass(&SHAVE::Combined_TRFRegClass, SHAVE::cmu_idx_0);
    case SHAVE::C_CMU_0_15: return returnClass(&SHAVE::Combined_TRFRegClass, SHAVE::cmu_idx_0, SHAVE::cmu_idx_8);
    default: llvm_unreachable("Unhandled member of Combined_TRF Register Class");
    }
  }

#undef cases

  return false;
}

// Given a physical SHAVE register "reg", this function returns the matching "full" physical register(s)
// For example, with reg I8_q1, this function will return { I8, NoRegister }
// For IRF64, the super-register will broken down into both physical registers, e.g. I8_9 returns { I8, I9 }
std::array<unsigned int, 2> SHAVERegisterInfo::getPhysicalSHAVERegisters(unsigned int reg, bool includeTRF) const {
  static_assert(SHAVE::NoRegister == 0, "Function uses zero-initialisation for return values which are assumed to equal SHAVE::NoRegister");

  SHAVEPhysicalRegInfo info;
  if (!getPhysicalRegisterData(reg, info, includeTRF))
    return { };

  // "reg" is already a complete physical register
  if (info.regClass == nullptr)
    return { reg };

  if (info.regClass == &SHAVE::IRF64RegClass) {
    // IRF64 breaks down into two physical IRF32 registers
    return { getSubReg(reg, info.subregs[0]), getSubReg(reg, info.subregs[1]) };
  }
  else if (info.regClass == &SHAVE::Combined_TRFRegClass) {
    // Combined_TRFRegClass can either map to one or two physical TRFs, depending on the width of the combined register
    std::array<unsigned int, 2> result;
    result[0] = getMatchingSuperReg(getSubReg(reg, info.subregs[0]), SHAVE::cmu_idx_0, &SHAVE::TRFRegClass);
    if (info.subregs[1] != 0)
      getMatchingSuperReg(getSubReg(reg, info.subregs[1]), SHAVE::cmu_idx_0, &SHAVE::TRFRegClass);
    return result;
  }
  else {
    // All other registers are sub-registers of a single physical register
    return { getMatchingSuperReg(reg, info.subregs[0], info.regClass) };
  }
}

// Inverse of getPhysicalSHAVERegisters. Pass in a physical register "reg" and an (potentially) unrelated sub-register
// and this function will return the matching sub-reg of "reg".
// For example, with "reg" I8 and "correspondingSubReg" I17_q1, this function will return I8_q1
unsigned int SHAVERegisterInfo::getSubSHAVERegister(unsigned int reg, unsigned int correspondingSubReg) const {
  SHAVEPhysicalRegInfo info;
  LLVM_ATTRIBUTE_UNUSED bool regInfo = getPhysicalRegisterData(correspondingSubReg, info, false);
  assert(regInfo);

  if (info.regClass == nullptr)
    return reg;

  assert((info.regClass == &SHAVE::IRF32RegClass || info.regClass == &SHAVE::VRF128RegClass || info.regClass == &SHAVE::WVRF512RegClass)
         && "Can only fetch sub-register for IRF, VRF and WVRF");

  return getSubReg(reg, info.subregs[0]);
}
