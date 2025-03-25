//===-- SHAVEFrameLowering.cpp - Function Frame Information -----*- C++ -*-===//
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

#include <set>

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/MachineLocation.h"

#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVEFrameLowering.h"
#include "SHAVEInstrInfo.h"
#include "SHAVEMachineFunctionInfo.h"
#include "SHAVERegisterInfo.h"
#include "SHAVETargetMachine.h"

using namespace llvm;


int64_t SHAVEFrameLowering::computeStackSize(MachineFunction &MF) const {
  SHAVEMachineFunctionInfo *SMFI = MF.getInfo<SHAVEMachineFunctionInfo>();

  if (!SMFI->isFrameInfoReady())
    SMFI->initialiseFrameInfo(MF, getStackAlign());

  return SMFI->getStackSize();
}

bool SHAVEFrameLowering::hasFP(const MachineFunction &MF) const {
  return MF.getFrameInfo().hasVarSizedObjects();
}


//===----------------------------------------------------------------------===//
// Stack Frame Processing methods
//===----------------------------------------------------------------------===//

// Call stack             |     save LR      | no save LR
// ----------------------------------------------------------------------------
// FP + (stacksize - 4)   |  Link Reg (I3O)  |  old FP (*)
// FP + (stacksize - 8)   |    old FP (*)    |
// (*) if the function does not use dynamic stack allocation (i.e. variable sized arrays, alloca)
//     there is no need to save the old FP (frame pointer, I31), and all the stack objects are
//     accessed relative to the stack pointer (I19)

// Insert prologue code into the function.
//   The generalised pattern is as follows:
//     o  Adjust the SP if necessary to reserve stack for this function
//     o  Save the Return Address if necessary
//     o  Save the FP register if necessary
//     o  Initialise the FP from the SP if required
void SHAVEFrameLowering::emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator MBBI = MBB.begin();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  DebugLoc dbgLoc;
#ifndef NDEBUG
  // FIXME: Movidius - sometimes the incoming DebugLoc is empty - why?  See Bugzilla #22908
//  assert(dbgLoc.getLine() != 0);
#endif // NDEBUG

  // Fetch the full stack size, including space for storage LR, FP, Varargs
  int64_t fixedStackSize = computeStackSize(MF);
  SHAVEMachineFunctionInfo *SMFI = MF.getInfo<SHAVEMachineFunctionInfo>();

  // Insert a pessimistic load barrier on entry to a function to prevent LSU0/LSU1 load-store race condition
  if (SHAVEOptions::LSUFlushOnEnter) {
    // FIXME: Movidius - Insert a dummy load on both LSU0 and LSU1 (a VRF Hi/Lo to a dummy register will do)
    //        This does not really work, because it is subject to the LSU policies, and the barrier needs to
    //        ignore this and use both regardless of the policy settings.
    MachinePointerInfo memInfo;
    MachineMemOperand *MMO = MF.getMachineMemOperand(memInfo, (MachineMemOperand::Flags)(MachineMemOperand::MOLoad | MachineMemOperand::MOStore), 8u, Align(64));

    BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::FLUSH_LSU0), SHAVE::V0)  // FIXME: Movidius - not safe, need to chose a better scratch register
       .addReg(SHAVE::V0)
       .addReg(SHAVE::I19)
       .addMemOperand(MMO)
       .addImm(SHAVECC::AL)  // Always execute
       .addReg(0)  // No predication register used
       .addImm(SHAVE::LSU0)
       .setMIFlag(MachineInstr::FrameSetup);

    if (SHAVEOptions::HasLSU1)
      BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::FLUSH_LSU1), SHAVE::V0)  // FIXME: Movidius - not safe, need to chose a better scratch register
        .addReg(SHAVE::V0)
        .addReg(SHAVE::I19)
        .addMemOperand(MMO)
        .addImm(SHAVECC::AL)  // Always execute
        .addReg(0)  // No predication register used
        .addImm(SHAVE::LSU1)
        .setMIFlag(MachineInstr::FrameSetup);

    // Bundle these two instructions - the 'FLUSH_LSU0' should be the first instruction on the edited MBB
    MachineInstr &flushLSU0MI = *MBB.begin();
    assert((flushLSU0MI.getOpcode() == SHAVE::FLUSH_LSU0) && "Expecting 'FLUSH_LSU0'");
    if (SHAVEOptions::HasLSU1)
      flushLSU0MI.bundleWithSucc();
  }

  // If this is an entry-point ('dllexport') function, then capture the execution context
  if (SMFI->hasDllExport())
    captureStackInfo(MF, MBBI, MBB, dbgLoc);

  if (!fixedStackSize && !MFI.adjustsStack())
    return;

  const bool saveLR = SMFI->saveLR();
  const bool saveFP = SMFI->saveFP();
  const bool initFP = SMFI->initFP();
  const int64_t LROffset = SMFI->getLRStackOffset(); // Link register offset from top of stack
  const int64_t FramePointerOffset = SMFI->getFPStackOffset(); // Slot to save the old frame pointer

  // Adjust the SP by the amount to be reserved if the adjustment is not zero
  if (fixedStackSize) {
    if (isUInt<8>(fixedStackSize)) {
      // bit_length(stackSize) <= 8
      // stackSize fits into the immediate field of IAU.SUB
      TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::IAU_SUB_32_imm), SHAVERegisterInfo::getSPReg())
                        .addReg(SHAVERegisterInfo::getSPReg())
                        .addImm(fixedStackSize),
                     SHAVEInstrInfo::inFunctionPrologue);
    } else {
      unsigned tmpPhysicalReg = SHAVERegisterInfo::getScratchReg();

      TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LDImm32), tmpPhysicalReg)
                        .addImm(fixedStackSize),
                     SHAVEInstrInfo::inFunctionPrologue);

      TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::IAU_SUB_32), SHAVERegisterInfo::getSPReg())
                        .addReg(SHAVERegisterInfo::getSPReg())
                        .addReg(tmpPhysicalReg),
                     SHAVEInstrInfo::inFunctionPrologue);
    }

    // Update the stack high-water mark - do this first so that we can know how much overflow occurred
    updateHighWaterMark(MF, MBBI, MBB, dbgLoc, SHAVEInstrInfo::inFunctionPrologue);

    // Check if the reservation causes a stack overflow
    checkStackOverflow(MF, MBBI, MBB, dbgLoc, SHAVEInstrInfo::inFunctionPrologue);

    // Save the actual registers if necessary
    if (saveLR)
      saveRegOnFixedStack(MBB, MBBI, dbgLoc, LROffset, SHAVERegisterInfo::getSPReg(), registerInfo->getRARegister());

    // Save the old frame pointer
    if (saveFP)
      saveRegOnFixedStack(MBB, MBBI, dbgLoc, FramePointerOffset,
                          SHAVERegisterInfo::getSPReg(),
                          SHAVERegisterInfo::getFPReg());

    // Initialize the FP register
    if (initFP)
      TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::CMU_CPII_32_Raw), SHAVERegisterInfo::getFPReg())
                        .addReg(SHAVERegisterInfo::getSPReg()),
                     SHAVEInstrInfo::inFunctionPrologue);
  }

  // Create the CFI debug frame information (could probably interleave this with the actual instruction)
  MachineModuleInfo &MMI = MF.getMMI();

  if (MMI.hasDebugInfo()) {
    // First record the stack adjustment (.cfi_def_cfa_offset fixedStackSize)
    MCSymbol *SPLabel = MMI.getContext().createTempSymbol();
    const MCRegisterInfo *MRI = MMI.getContext().getRegisterInfo();
    unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::cfiDefCfaOffset(SPLabel, -fixedStackSize));

    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::CFI_INSTRUCTION))
                      .addCFIIndex(CFIIndex),
                   SHAVEInstrInfo::inFunctionPrologue);

    // Record the moves for the frame, in reverse order to their implementation above
    // The location of the old frame-pointer on the stack if applicable
    if (saveFP) {
      MCSymbol *SaveFPLabel = MMI.getContext().createTempSymbol();
      const unsigned Reg = MRI->getDwarfRegNum(SHAVERegisterInfo::getFPReg(), true);

      CFIIndex = MF.addFrameInst(MCCFIInstruction::createOffset(SaveFPLabel, Reg, FramePointerOffset));

      TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::CFI_INSTRUCTION))
                        .addCFIIndex(CFIIndex),
                     SHAVEInstrInfo::inFunctionPrologue);
    }

    // The location of the return address on the stack if applicable
    if (saveLR) {
      MCSymbol *SaveLRLabel = MMI.getContext().createTempSymbol();
      unsigned Reg = MRI->getDwarfRegNum(SHAVERegisterInfo::getLinkReg(), true);

      CFIIndex = MF.addFrameInst(MCCFIInstruction::createOffset(SaveLRLabel, Reg, LROffset));

      TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::CFI_INSTRUCTION))
                        .addCFIIndex(CFIIndex),
                     SHAVEInstrInfo::inFunctionPrologue);
    }
  }
}

// Insert epilogue code into the function.
//   The generalised pattern is as follows:
//     o  If this is a DLLEXPORT or NORETURN function, we're done; else ...
//     o  If the return address was saved in the prologue, restore it from the current frame
//     o  If a frame-pointer was needed:
//        x  Restore the SP from the current value of the FP
//        x  Restore the FP from the current frame
//     o  Apply any necessary adjustment to the SP in order to restore it to the caller's value
void SHAVEFrameLowering::emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator MBBI = std::prev(MBB.end());
  DebugLoc dbgLoc;
  const int64_t fixedStackSize = computeStackSize(MF);
  SHAVEMachineFunctionInfo *SMFI = MF.getInfo<SHAVEMachineFunctionInfo>();

  // No need to restore anything
  if (SMFI->hasNoReturn() || SMFI->hasDllExport() || (fixedStackSize == 0))
    return;

  // FIXME: Movidius - does LLVM not already calculate space for varargs and for the LR and FP?
  const bool saveLR = SMFI->saveLR();
  const bool saveFP = SMFI->saveFP();

  // Restore the return address if required
  if (saveLR) {
    const int64_t LROffset = SMFI->getLRStackOffset(); // Link register offset from top of stack

    restoreRegFromFixedStack(MBB, MBBI, dbgLoc, LROffset,
                             registerInfo->getFrameRegister(MF),
                             registerInfo->getRARegister());
  }

  // Restore the frame-pointer is required
  if (saveFP) {
    // To ensure stack-pointer integrity, restore the SP from the FP first
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::CMU_CPII_32_Raw), SHAVERegisterInfo::getSPReg())
                      .addReg(registerInfo->getFrameRegister(MF)),
                   SHAVEInstrInfo::inFunctionEpilogue);

    // Now restore the FP itself
    const int64_t FramePointerOffset = SMFI->getFPStackOffset(); // Slot where the FP was saved

    restoreRegFromFixedStack(MBB, MBBI, dbgLoc, FramePointerOffset,
                             registerInfo->getFrameRegister(MF),
                             registerInfo->getFPReg());
  }

  // Restore SP register
  if (isUInt<8>(fixedStackSize)) {
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::IAU_ADD_32_imm), SHAVERegisterInfo::getSPReg())
                      .addReg(registerInfo->getSPReg())
                      .addImm(fixedStackSize),
                   SHAVEInstrInfo::inFunctionEpilogue);
  } else {
    unsigned tmpPhysicalReg = SHAVERegisterInfo::getScratchReg();

    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LDImm32), tmpPhysicalReg)
                      .addImm(fixedStackSize),
                   SHAVEInstrInfo::inFunctionEpilogue);

    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::IAU_ADD_32), SHAVERegisterInfo::getSPReg())
                      .addReg(registerInfo->getSPReg())
                      .addReg(tmpPhysicalReg),
                   SHAVEInstrInfo::inFunctionEpilogue);
  }
}

// On entry to a 'dllexport' function, the execution context needs to be captured.
// This consists of 3 registers containing the stack address (IRF19), the top-of-
// stack address (IRF20) and the MDK Context addres (IRF21).
void SHAVEFrameLowering::captureStackInfo(MachineFunction &MF,
                                          MachineBasicBlock::iterator MBBI,
                                          MachineBasicBlock &MBB,
                                          const DebugLoc &dbgLoc) const {
  // symbol := "__stackMaximumExtent"
  MCSymbol *symbol = MF.getContext().getOrCreateSymbol("__stackMaximumExtent");

  // I0 := loadSymbolAddress(__stackMaximumExtent) using LDILSym and LDIHSym
  TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_LDILSym), SHAVE::I0) //destReg
                    .addExternalSymbol(symbol->getName().data()),
                 SHAVEInstrInfo::inFunctionPrologue);

  TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc,
                         TII.get(SHAVE::LSU_LDIHSym),
                         SHAVE::I0) //destReg
                    .addReg(SHAVE::I0)
                    .addExternalSymbol(symbol->getName().data()),
                 SHAVEInstrInfo::inFunctionPrologue);

  // Save I20 and I21 regs in one operation (if ST64 is available):
  //    I20 is the "Top-of-Stack" address and is saved to '__stackMaximumExtent'
  //    I21 is the MDK Dynamic Loading "Context" address and is saved to '__executionContext'
  //        which is at '__stackMaximumExtent + 4'

  MachinePointerInfo memInfo;

  // ToDo doubleckeck
  // MachineMemOperand *MMO = MF.getMachineMemOperand(memInfo, MachineMemOperand::MOStore, 8u, Align(64));
  // MMO->setFlags(MachineMemOperand::MOVolatile);
  MachineMemOperand *MMO;

  if (!TII.hasFeature(SHAVE::HasIRF64_LSU_CMU_Instrs_Feature)) {
    // ToDo doublecheck
    MMO = MF.getMachineMemOperand(memInfo, MachineMemOperand::MOStore, 4u, Align(32));
    MMO->setFlags(MachineMemOperand::MOVolatile);

    MachineInstrBuilder MIB = BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_ST_i32))
      .addReg(SHAVE::I20)
      .addReg(SHAVE::I0)
      .addMemOperand(MMO);
    TII.finaliseMI(MIB, SHAVEInstrInfo::inFunctionPrologue);

    MachineMemOperand *MMO2 = MF.getMachineMemOperand(MMO, 4u, 4u);

    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_STO_i32))
                       .addReg(SHAVE::I21)
                       .addReg(SHAVE::I0)
                       .addImm(4)
                       .addMemOperand(MMO2),
                   SHAVEInstrInfo::inFunctionPrologue);

    // MIB->bundleWithSucc(); // FIXME: look at bundling these
  } else {
    MMO = MF.getMachineMemOperand(memInfo, MachineMemOperand::MOStore, 8u, Align(64));
    MMO->setFlags(MachineMemOperand::MOVolatile);

    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_ST64_Raw))
                       .addReg(SHAVE::I20_21)
                       .addReg(SHAVE::I0)
                       .addMemOperand(MMO),
                   SHAVEInstrInfo::inFunctionPrologue);
  }

  // Initialise the stack "High-Water Mark" from the SP passed to this function.
  // This is saved in '__stackHighWater' which is at '__stackMaximumExtent + 8'
  MMO = MF.getMachineMemOperand(memInfo, MachineMemOperand::MOStore, 4u, Align(16));
  MMO->setFlags(MachineMemOperand::MOVolatile);

  TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_STO_i32))
                    .addReg(SHAVERegisterInfo::getSPReg())
                    .addReg(SHAVE::I0)
                    .addImm(8)
                    .addMemOperand(MMO),
                 SHAVEInstrInfo::inFunctionPrologue);
}

// If stack overflow checking is enabled, compare the value of the adjusted stack
// pointer with the value stored in '__stackMaximumExtent'
void SHAVEFrameLowering::checkStackOverflow(MachineFunction &MF,
                                            MachineBasicBlock::iterator MBBI,
                                            MachineBasicBlock &MBB,
                                            const DebugLoc &dbgLoc,
                                            SHAVEInstrInfo::FrameContext_t frameContext) const {
  if (!SHAVEOptions::StackOverflowChecking)
    return;

  if ((frameContext == SHAVEInstrInfo::inFunctionPrologue) && MF.getInfo<SHAVEMachineFunctionInfo>()->hasDllExport()) {
    // An optimisation for 'dllexport' functions can compare this directly with IRF20 if
    // this is being performed for a fixed stack reservation
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::CMU_CMII_i32))
                      .addReg(SHAVERegisterInfo::getSPReg())
                      .addReg(SHAVE::I20),
                   frameContext);
  } else {
    // if in prologue but not dll-export function, then we can use I0 reg
    // otherwise, we need to create a virtual temp one for the dynamic stack reservations
    // Plus, we have to use I0 only if we are in post-RA phase as we can't create more VirtRegs.
    unsigned tmpReg = (frameContext == SHAVEInstrInfo::inFunctionPrologue ||
      !MF.getRegInfo().getNumVirtRegs()) ? SHAVE::I0 : MF.getRegInfo().createVirtualRegister(&SHAVE::IRF32RegClass);

    // symbol := "__stackMaximumExtent"
    MCSymbol *symbol = MF.getContext().getOrCreateSymbol("__stackMaximumExtent");

    // tmpReg := loadSymbolAddr(__stackMaximumExtent) using LDILSym and LDIHSym
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_LDILSym), tmpReg)
                      .addExternalSymbol(symbol->getName().data()),
                   frameContext);
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_LDIHSym), tmpReg)
                      .addReg(tmpReg)
                      .addExternalSymbol(symbol->getName().data()),
                   frameContext);

    // tmpReg := the top-of-stack address
    MachinePointerInfo memInfo;
    MachineMemOperand *MMO = MF.getMachineMemOperand(memInfo, MachineMemOperand::MOLoad, 4u, Align(16));
    MMO->setFlags(MachineMemOperand::MOVolatile);

    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_LD32_Raw), tmpReg)
                      .addReg(tmpReg)
                      .addMemOperand(MMO),
                   frameContext);

    // Check if SP < __stackMaximumExtent
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::CMU_CMII_i32))
                      .addReg(SHAVERegisterInfo::getSPReg())
                      .addReg(tmpReg),
                   frameContext);
  }

  // If I19 is less than the top-of-stack, then an overflow has occurred so abort
  // with 'SHAVEExitCodes_t::SHAVEExitStackOverflow' and set 'IRF18' to '-3'
  MachineInstrBuilder MIB = BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_LDIL), SHAVE::I18)
            .addImm(-3)
            .add(MachineOperand::CreateImm(SHAVECC::LT))
            .addReg(SHAVE::CC_CMU0)
            .addImm(SHAVE::LSU0);
  if (frameContext == SHAVEInstrInfo::inFunctionPrologue)
    MIB.setMIFlag(MachineInstr::FrameSetup);

  MIB = BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_LDIH), SHAVE::I18)
            .addReg(SHAVE::I18)
            .addImm(-3)
            .add(MachineOperand::CreateImm(SHAVECC::LT))
            .addReg(SHAVE::CC_CMU0)
            .addImm(SHAVE::LSU0);
  if (frameContext == SHAVEInstrInfo::inFunctionPrologue)
    MIB.setMIFlag(MachineInstr::FrameSetup);

  MIB = BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::BRU_SWIH_imm))
            .addImm(0x3)    // SHAVEProcessExitStackOverflow
            .add(MachineOperand::CreateImm(SHAVECC::LT))
            .addReg(SHAVE::CC_CMU0);
  if (frameContext == SHAVEInstrInfo::inFunctionPrologue)
    MIB.setMIFlag(MachineInstr::FrameSetup);
}

// Tracks stack usage by comparing the value in '__stackHighWater' to the adjusted
// stack pointer, and if the new SP is less than the high-water mark, then update
// the high-water mark to the value in the SP
void SHAVEFrameLowering::updateHighWaterMark(MachineFunction &MF,
                                             MachineBasicBlock::iterator MBBI,
                                             MachineBasicBlock &MBB,
                                             const DebugLoc &dbgLoc,
                                             SHAVEInstrInfo::FrameContext_t frameContext) const {

  if (!SHAVEOptions::TrackStackUsage)
    return;

  MCSymbol *symbol = MF.getContext().getOrCreateSymbol("__stackHighWater");

  if ((frameContext == SHAVEInstrInfo::inFunctionPrologue) && MF.getInfo<SHAVEMachineFunctionInfo>()->hasDllExport()) {
    // If this is a 'dllexport' function, then simply store the new value of the SP
    // I0 := loadSymbolAddr(__stackHighWater) using LDILSym and LDIHSym
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_LDILSym), SHAVE::I0)
                      .addExternalSymbol(symbol->getName().data()),
                   frameContext);
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_LDIHSym), SHAVE::I0)
                      .addReg(SHAVE::I0)
                      .addExternalSymbol(symbol->getName().data()),
                   frameContext);

    MachinePointerInfo memInfo;
    MachineMemOperand *MMO = MF.getMachineMemOperand(memInfo, MachineMemOperand::MOStore, 4u, Align(16));
    MMO->setFlags(MachineMemOperand::MOVolatile);

    // *I0 := I19   // __stackHighWater := I19
    MachineInstrBuilder MIBST = BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_ST_i32))
                                   .addReg(SHAVE::I0)
                                   .addReg(SHAVERegisterInfo::getSPReg())
                                   .addMemOperand(MMO);
    TII.finaliseMI(MIBST, frameContext);
  } else {
    // if in prologue (fixed reservation but not in dll-export func
    // we can still use I0 reg but we need to load the symbol
    // if we are not in prologue (dynamic reservation)
    // we need to create a virtual register instead of I0.
    // Also in case we are in pre-RA phase (no generated VirtRegs yet)
    // we cannot create new virtual registers, instead we use IO
    unsigned tmpReg1 = (frameContext == SHAVEInstrInfo::inFunctionPrologue ||
      !MF.getRegInfo().getNumVirtRegs()) ? SHAVE::I0 : MF.getRegInfo().createVirtualRegister(&SHAVE::IRF32RegClass);
    unsigned tmpReg2 = (frameContext == SHAVEInstrInfo::inFunctionPrologue ||
      !MF.getRegInfo().getNumVirtRegs()) ? SHAVE::I1 : MF.getRegInfo().createVirtualRegister(&SHAVE::IRF32RegClass);

    // tmpReg1 := loadSymbolAddr(__stackHighWater) using LDILSym and LDIHSym
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_LDILSym), tmpReg1)
                      .addExternalSymbol(symbol->getName().data()),
                   frameContext);
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_LDIHSym), tmpReg1)
                      .addReg(tmpReg1)
                      .addExternalSymbol(symbol->getName().data()),
                   frameContext);

    // tmpReg2 := *tmpReg1  // tmpReg2 := __stackHighWater
    MachinePointerInfo memInfo;
    MachineMemOperand *MMOLd = MF.getMachineMemOperand(memInfo, MachineMemOperand::MOLoad, 4u, Align(16));
    MMOLd->setFlags(MachineMemOperand::MOVolatile);

    MachineInstrBuilder MIBLd = BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_LD32_Raw), tmpReg2)
                                   .addReg(tmpReg1)
                                   .addMemOperand(MMOLd);
    TII.finaliseMI(MIBLd, frameContext);

    // Check if SP < __stackHighWater
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::CMU_CMII_i32))
                      .addReg(SHAVERegisterInfo::getSPReg())
                      .addReg(tmpReg2),
                   frameContext);

    // *tmpReg := I19  // __stackHighWater := SP
    MachineMemOperand *MMOSt = MF.getMachineMemOperand(memInfo, MachineMemOperand::MOStore, 4u, Align(16));
    MMOSt->setFlags(MachineMemOperand::MOVolatile);

    // FIXME: Movidius - for some reason the LSU policy is not being enforced if the LSU instruction
    //        is within a predicated-bundle, so explictly handle it here
    int64_t useThisLSU = (SHAVEOptions::LSUVolatileLoadStorePolicy == SHAVEOptions::AlwaysUseLSU0) ? SHAVE::LSU0 : SHAVE::LSU1;
    MachineInstrBuilder MIB = BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LSU_ST_i32))
                                        .addReg(SHAVERegisterInfo::getSPReg())
                                        .addReg(tmpReg1)
                                        .add(MachineOperand::CreateImm(SHAVECC::LT))
                                        .addReg(SHAVE::CC_CMU0)
                                        .addImm(useThisLSU)
                                        .addMemOperand(MMOSt);  // FIXME: Movidius - should this not precede adding the predicate?  Perhaps this is why the policy is ignored?
    if (frameContext == SHAVEInstrInfo::inFunctionPrologue)
      MIB.setMIFlag(MachineInstr::FrameSetup);
  }
}

bool SHAVEFrameLowering::IsValid32BitOffset(int64_t stackOffset) const {
  APInt offset = APInt(64, stackOffset, true);
  return offset.getSignificantBits() <= TII.getLDOSTO_OffsetBits();
}

void SHAVEFrameLowering::saveRegOnFixedStack(MachineBasicBlock &MBB,
                                             MachineBasicBlock::iterator MBBI,
                                             DebugLoc &dbgLoc,
                                             int64_t stackOffset,
                                             unsigned refReg,
                                             unsigned regToSave) const {
  unsigned StoreOpc = 0;
  bool UseImmOffset = false;

  if (stackOffset == 0) {
    // In the simple case where there is no offset, a simple store is all that is needed
    StoreOpc = SHAVE::LSU_ST_i32;
  } else if (IsValid32BitOffset(stackOffset)) {
    // If the offset if in the range of a signed 15-bit integer, use STO with an immediate offset
    StoreOpc = SHAVE::LSU_STO_i32;
    UseImmOffset = true;
  } else {
    // Offset does not fit in 15 bits, signed so compute in another register
    // FIXME: Movidius - should optimise this to use STX when possible
    // FIXME: Movidius - we need to wean 'moviCompile' off requiring a physical scratch register
    unsigned tmpPhysicalReg = SHAVERegisterInfo::getScratchReg();

    BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LDImm32), tmpPhysicalReg)
       .addImm(stackOffset)
       .setMIFlag(MachineInstr::FrameSetup);
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::IAU_ADD_32), tmpPhysicalReg)
                      .addReg(tmpPhysicalReg)
                      .addReg(refReg)
                      .setMIFlag(MachineInstr::FrameSetup),
                   SHAVEInstrInfo::inFunctionPrologue);

    refReg = tmpPhysicalReg;
    StoreOpc = SHAVE::LSU_ST_i32;
  }

  // FIXME: Movidius - What about vector stores and other non-32-bit stores?
  MachineFunction *MF = MBB.getParent();
  MachinePointerInfo PtrInfo = MachinePointerInfo::getStack(*MF, stackOffset);
  MachineMemOperand *MMO = MF->getMachineMemOperand(PtrInfo,
                                                    MachineMemOperand::MOStore,
                                                    0, Align(16));
  MachineInstrBuilder MIB = BuildMI(MBB, MBBI, dbgLoc, TII.get(StoreOpc))
                               .addReg(regToSave)
                               .addReg(refReg);

  if (UseImmOffset)
    MIB.addImm(stackOffset);

  MIB.addMemOperand(MMO);
  MIB.setMIFlag(MachineInstr::FrameSetup);
  TII.finaliseMI(MIB, SHAVEInstrInfo::inFunctionPrologue);
}

void SHAVEFrameLowering::restoreRegFromFixedStack(MachineBasicBlock &MBB,
                                                  MachineBasicBlock::iterator MBBI,
                                                  DebugLoc &dbgLoc,
                                                  int64_t stackOffset,
                                                  unsigned refReg,
                                                  unsigned regToRestore) const {
  unsigned LoadOpc = 0;
  bool UseImmOffset = false;

  if (stackOffset == 0) {
    // In the simple case where there is no offset, a simple load is all that is needed
    LoadOpc = SHAVE::LSU_LD_i32;
  } else if (IsValid32BitOffset(stackOffset)) {
    // If the offset if in the range of a signed 15-bit integer, use LDO with an immediate offset
    LoadOpc = SHAVE::LSU_LDO_i32;
    UseImmOffset = true;
  } else {
    // Offset does not fit in 15 bits, signed so compute in another register
    // FIXME: Movidius - should optimise this to use LDX when possible
    // FIXME: Movidius - we need to wean 'moviCompile' off requiring a physical scratch register
    unsigned tmpPhysicalReg = SHAVERegisterInfo::getScratchReg();

    BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::LDImm32), tmpPhysicalReg)
       .addImm(stackOffset)
       .setMIFlag(MachineInstr::FrameSetup);
    TII.finaliseMI(BuildMI(MBB, MBBI, dbgLoc, TII.get(SHAVE::IAU_ADD_32), tmpPhysicalReg)
                      .addReg(tmpPhysicalReg)
                      .addReg(refReg)
                      .setMIFlag(MachineInstr::FrameSetup),
                   SHAVEInstrInfo::inFunctionEpilogue);

    refReg = tmpPhysicalReg;
    LoadOpc = SHAVE::LSU_LD_i32;
  }

  // FIXME: Movidius - what about vector loads and other non-32-bit loads?
  MachineFunction *MF = MBB.getParent();
  MachinePointerInfo PtrInfo = MachinePointerInfo::getStack(*MF, stackOffset);
  MachineMemOperand *MMO  = MF->getMachineMemOperand(PtrInfo,
                                                     MachineMemOperand::MOLoad,
                                                     0, Align(16));
  MachineInstrBuilder MIB = BuildMI(MBB, MBBI, dbgLoc, TII.get(LoadOpc), regToRestore)
                               .addReg(refReg);

  if (UseImmOffset)
    MIB.addImm(stackOffset);

  MIB.addMemOperand(MMO);
  MIB.setMIFlag(MachineInstr::FrameSetup);
  TII.finaliseMI(MIB, SHAVEInstrInfo::inFunctionEpilogue);
}


//--------------------------------------------------------------------------
// EliminateCallFramePseudoInstr - If call frame setup or destroy pseudo instructions
// are used, this can be called to eliminate them
MachineBasicBlock::iterator SHAVEFrameLowering::eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB, MachineBasicBlock::iterator I) const {
  DebugLoc dbgLoc = I->getDebugLoc();
  unsigned amount = I->getOperand(0).getImm();

  // MF.dump();
  // llvm::dbgs() << "Stack size = " << MF.getFrameInfo()->getStackSize() << "\n";
  // llvm::dbgs() << "Local frame size = " << MF.getFrameInfo()->getLocalFrameSize() << "\n";

  if (amount && hasFP(MF)) {
    // if the amount is > 0, adjust the stack accordingly

    bool opcodeAdd = false;
    unsigned opcode = 0;

    if (I->getOpcode() == SHAVE::ADJCALLSTACKDOWN)
      opcodeAdd = false;
    else if (I->getOpcode() == SHAVE::ADJCALLSTACKUP)
      opcodeAdd = true;

    if (isUInt<8>(amount)) {
      opcode = opcodeAdd ? SHAVE::IAU_ADD_32_imm : SHAVE::IAU_SUB_32_imm;
      TII.finaliseMI(BuildMI(MBB, I, dbgLoc, TII.get(opcode), SHAVERegisterInfo::getSPReg())
                        .addReg(SHAVERegisterInfo::getSPReg())
                        .addImm(amount));
    } else {
      // FIXME: Movidius - we need to wean 'moviCompile' off requiring a physical scratch register
      unsigned tmpPhysicalReg = SHAVERegisterInfo::getScratchReg();

      opcode = opcodeAdd ? SHAVE::IAU_ADD_32 : SHAVE::IAU_SUB_32;
      BuildMI(MBB, I, dbgLoc, TII.get(SHAVE::LDImm32), tmpPhysicalReg)
         .addImm(amount);
      TII.finaliseMI(BuildMI(MBB, I, dbgLoc, TII.get(opcode), SHAVERegisterInfo::getSPReg())
                        .addReg(SHAVERegisterInfo::getSPReg())
                        .addReg(tmpPhysicalReg));
    }

    // If this is a dynamic reservation, also insert stack instrumentation as appropriate
    if ((opcode == SHAVE::IAU_SUB_32_imm) || (opcode == SHAVE::IAU_SUB_32)) {
      // Update the stack high-water mark - do this first so that we can know how much overflow occurred
      updateHighWaterMark(MF, I, MBB, dbgLoc, SHAVEInstrInfo::inFunctionBody);

      // Check if the reservation causes a stack overflow
      checkStackOverflow(MF, I, MBB, dbgLoc, SHAVEInstrInfo::inFunctionBody);
    }
  }

  return MBB.erase(I);
}

class StashRegAllocator {
private:
  const TargetRegisterClass *RC = &SHAVE::VRF32_qNRegClass;
  TargetRegisterClass::iterator curRegIterator;
  const MCPhysReg *CSRegs;
  bool isNonLeafFunction;
  std::set <unsigned> alreadySavedCSRs;

  unsigned getCalleeSavedSuperReg(unsigned Reg, const MCPhysReg *CSRegs, const TargetRegisterInfo *TRI) {
    for (unsigned i = 0; CSRegs[i]; ++i)
      if (TRI->isSubRegister(CSRegs[i], Reg)) {

        // Find the 64 bit subreg for the current register
        unsigned laneIdx = (Reg - SHAVE::V0_q0) % 4;
        unsigned vrfnum = (Reg - SHAVE::V0_q0) / 4;

        unsigned VRF64_reg;
        if (laneIdx / 2 == 0)
          VRF64_reg = SHAVE::V0_l + vrfnum;
        else
          VRF64_reg = SHAVE::V0_h + vrfnum;

        // Check if the assumption we made for the above calculation hold
        assert(((SHAVE::V0_q0 + 1 == SHAVE::V0_q1) && (SHAVE::V0_l + 1 == SHAVE::V1_l)) && "Register ordering assumption failure in VRF spill code");

        return VRF64_reg;
      }

    return ~0u;
  }


public:
  StashRegAllocator(MachineFunction &MF, const TargetRegisterInfo *TRI, std::vector<CalleeSavedInfo> &CSI) {
    CSRegs = TRI->getCalleeSavedRegs(&MF);
    curRegIterator = RC->begin();
    isNonLeafFunction = MF.getFrameInfo().hasCalls();

    // initialize the already saved regs set to match CSI. This is to
    // avoid duplicates when saving stash registers to stack
    // Some subregisters of this super reg may have already been used
    // by the register allocator and caused the super reg to be already
    // present in the CSI list before coming to prolog/epilog generation
    for (std::vector<CalleeSavedInfo>::iterator i = CSI.begin(); i != CSI.end(); i++) {
      alreadySavedCSRs.insert((*i).getReg());
    }
  }

  unsigned allocateStashReg(unsigned spillReg, MachineFunction &MF, const TargetRegisterInfo *TRI, std::vector<CalleeSavedInfo> &CSI, SmallBitVector &pairedRegMask, unsigned numSpillsRemaining) {
    const MachineRegisterInfo &MRI = MF.getRegInfo();

    while (curRegIterator != RC->end()) {
      bool regAliasInUse = false;

      // get the super register
      // FIXME: Movidius - This is a temporary fix until we correct the IRF64 patterns
      unsigned superReg = (*curRegIterator - SHAVE::V0_q0) / 4 + SHAVE::V0;

      // Next check if any register in the alias set of the superreg in in use
      for (MCRegAliasIterator iterAlias(superReg, TRI, true); iterAlias.isValid(); ++iterAlias) {
        // This should be the actual loop. We should not use the superReg
        // for (MCRegAliasIterator iterAlias(*curRegIterator, TRI, true); iterAlias.isValid(); ++iterAlias) {
        if (!MRI.reg_nodbg_empty(*iterAlias)) {
          regAliasInUse = true;
          //DEBUG(dbgs()<<"But an alias "<< TRI->getName(*iterAlias)<<" was used. Skipping \n");
          break;
        }
      }

      if (regAliasInUse) {
        curRegIterator++;
        continue;
      }

      unsigned CSSuperReg = getCalleeSavedSuperReg(*curRegIterator, CSRegs, TRI);

      // If this is not a leaf function, we assign only callee saved registers,
      // since it is going to be live throughout the function
      if (isNonLeafFunction) {
        if (pairedRegMask[spillReg]) {
          return 0;
        }

        if (CSSuperReg == ~0u) {
          curRegIterator++;
          continue;
        }

      } 

      // For non-leaf functions, this is always true here.
      // This check is to catch the leaf functions with CSR allocated as Stash registers
      if (CSSuperReg != ~0u) {
        // If the last IRF spill uses a new VRF register, it will increase the number of instructions executed
        // from 1 st32 to 1cpiv+1st64. Don't do it.
        unsigned laneIdx = (*curRegIterator - SHAVE::V0_q0) % 4;
        if ((numSpillsRemaining == 1) && (laneIdx % 2 == 0)) {
          return 0;
        }

        // push the newly used super reg to CSI if it is a CSR and is not already on the list of saved regs
        auto result = alreadySavedCSRs.insert(CSSuperReg);
        if (result.second)
          CSI.push_back(CalleeSavedInfo(CSSuperReg));
      }

      // We found a free VRF q register. Return this and update the iterator to start with the
      // next register in the subsequent call to allocateStashReg
      return *(curRegIterator++);
    }

    //DEBUG (dbgs()<< "Ran out of stash registers\n");
    // Ran out of stash registers. Let the caller know
    return SHAVE::NUM_TARGET_REGS;
  }
};

// Analyse the CSI list for possible register pairs
// Returns the number of unpaired CSRs out of CSI list
static unsigned analyseIRF32CSRPairs(std::vector<CalleeSavedInfo> &CSI, const TargetRegisterInfo *TRI, SmallBitVector &pairedRegMask) {
  unsigned numUnpairedIRFRegsInCSR = 0;
  unsigned lastMergedReg = 0;

  // sort the CSI to make sure the regs are in decreasing numerical order
  std::sort(CSI.begin(), CSI.end(), [](CalleeSavedInfo const& a, CalleeSavedInfo const& b) {return a.getReg() > b.getReg(); });

  for (unsigned int i = 0; i < CSI.size() - 1; i++) {
    unsigned curreg = CSI[i].getReg();
    unsigned nextreg = CSI[i + 1].getReg();

    const TargetRegisterClass *curRC = TRI->getMinimalPhysRegClass(curreg);
    const TargetRegisterClass *nextRC = TRI->getMinimalPhysRegClass(nextreg);

    // Pairs like I21, I20 into I20_21 can be merged
    if (curRC->getID() == SHAVE::IRF32RegClassID) {
      if (nextRC->getID() == SHAVE::IRF32RegClassID
            && nextreg == curreg - 1) {
        pairedRegMask.set(curreg);
        pairedRegMask.set(nextreg);
        lastMergedReg = nextreg;
        i++; // skip nextreg in next iteration
      } else
        numUnpairedIRFRegsInCSR++;
    }
  }

  // Save the last reg from CSI if not already done
  unsigned lastCSIreg = CSI[CSI.size() - 1].getReg();
  if (lastMergedReg != lastCSIreg)
    numUnpairedIRFRegsInCSR++;

  return numUnpairedIRFRegsInCSR;
}

// Merge 32b ld/st of consecutive IRF regs to ld64/st64
// Replace the IRF32 regs with IRF64 counterparts
static void mergeIRF32CSRPairs(std::vector<CalleeSavedInfo> &CSI, const TargetRegisterInfo *TRI) {
  bool changed = false;
  unsigned lastMergedReg = 0;

  std::vector<CalleeSavedInfo> compactCSI;

  for (unsigned int i = 0; i < CSI.size() - 1; i++) {
    unsigned curreg = CSI[i].getReg();
    unsigned nextreg = CSI[i + 1].getReg();

    const TargetRegisterClass *curRC = TRI->getMinimalPhysRegClass(curreg);
    const TargetRegisterClass *nextRC = TRI->getMinimalPhysRegClass(nextreg);

    // convert pairs like I21, I20 into I20_21
    if (curRC->getID() == SHAVE::IRF32RegClassID
          && nextRC->getID() == SHAVE::IRF32RegClassID
          && CSI[i].getStashReg() == 0
          && CSI[i + 1].getStashReg() == 0
          && nextreg == curreg - 1) {
      compactCSI.push_back(CalleeSavedInfo(nextreg - SHAVE::I0 + SHAVE::I0_1));
      i++; // skip lowerreg in next iteration
      changed = true;
      lastMergedReg = nextreg;
    } else
      compactCSI.push_back(CSI[i]);
  }

  if (changed) {
    // Save the last reg from CSI if not already done
    unsigned lastCSIreg = CSI[CSI.size() - 1].getReg();

    if (lastMergedReg != lastCSIreg)
      compactCSI.push_back(CSI[CSI.size() - 1]);

    // Move the compacted CSI into the original one
    CSI.swap(compactCSI);
  }
}

/// Use the target hook to assign stash registers to the CSRegs
bool SHAVEFrameLowering::assignCalleeSavedSpillSlots(MachineFunction &MF, const TargetRegisterInfo *TRI,
                                                     std::vector<CalleeSavedInfo> &CSI) const {
  SmallBitVector pairedRegMask(TRI->getNumRegs());

  if (CSI.empty())
    return false;

  unsigned numIRF32sInCSR = analyseIRF32CSRPairs(CSI, TRI, pairedRegMask);

  StashRegAllocator *SRA = new StashRegAllocator(MF, TRI, CSI);

  // If the reg 2 reg spiller is enabled, we try to find a wider register
  // to stash the callee saved reg
  if (SHAVEOptions::EnableStashRetrieve && TII.isStashRetrieveEnabledForArch()) {
    for (unsigned i = 0, size = CSI.size(); i < size; i++) {
      unsigned Reg = CSI[i].getReg();
      const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(Reg);

      // At this point there are no vregs and the stash regs allocated here need
      // to be live across the entire function. We can go with the scratch registers
      // if this function does not have any calls and use CSRs if it does
      // The non-leaf functions will stash only if there are more than one (non-contiguous) IRF32 registers left
      // after pairing.  In most cases, the callee-save IRF32 registers will be contiguous and hence spilled as pairs
      // since there is not benefit in moving them into a stash register first
      if (RC->getID() == SHAVE::IRF32RegClassID) {// limit this to irf32 registers
        unsigned StashReg = SRA->allocateStashReg(Reg, MF, TRI, CSI, pairedRegMask, numIRF32sInCSR /* numSpillsRemaining */);

        // Exit if we ran out of stash registers
        if (StashReg == SHAVE::NUM_TARGET_REGS)
          break;

        if (StashReg) {
          numIRF32sInCSR--;
          CSI[i].setStashReg(StashReg);
        }
      }
    }
  }

  delete (SRA);

  // Convert 32-bit load/stores to 64-bit I64 ld/st
  if (TII.hasFeature(SHAVE::HasIRF64_LSU_CMU_Instrs_Feature))
    mergeIRF32CSRPairs(CSI, TRI);

  // we need to sort the CSI array in decreasing order of register size to ensure the stash registers get saved first
  // We want to minimise the runtime cost of this sort, so rely on stash (vector) regs having higher numeric value than
  // the I32 registers. Alternative would be to get the class for each pair of registers and do a getSize(),
  // but would be a waste of time. Adding an assert here to ensure that this assumption holds at runtime
  assert((SHAVE::I0 < SHAVE::V0) && "Failed assumption in sorting Stash Registers");
  std::sort(CSI.begin(), CSI.end(), [](CalleeSavedInfo const& a, CalleeSavedInfo const& b) {return a.getReg() > b.getReg(); });

  // We haven't really done the stack assignment, so hand off to default implementation
  return false;
}
