//===-- SHAVEPredicator.cpp - Predication Transforms ------------*- C++ -*-===//
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
// Transforms the predicated instructions to VLIW predicated bundles
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "shave-predicator-passes"

#include "SHAVEPredicator.h"

#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;


char SHAVEPredicator::ID = 0;

unsigned SHAVEPredicator::getPredOpcode(const SHAVEInstrInfo &TII,
                                        PredRegAndMask prm,
                                        unsigned int predicatedOpcode,
                                        MachineInstr &I, int &lane) const {
  // FIXME: Movidius - the vector comparisons assume that all vectors are 128-bits, but this won't work for 32-bit vectors
  // Need to better expand (and name) the possible condition code sets so that PEU.PVS can also be used for small-vectors
  switch (prm.reg) {
  case SHAVE::CC_CMU0:     return SHAVE::PEU_PC1C;
  case SHAVE::CC_CMU1:
  case SHAVE::CC_CMU2:
  case SHAVE::CC_CMU3:
  case SHAVE::CC_CMU4:
  case SHAVE::CC_CMU5:
  case SHAVE::CC_CMU6:
  case SHAVE::CC_CMU7:
  case SHAVE::CC_CMU8:
  case SHAVE::CC_CMU9:
  case SHAVE::CC_CMU10:
  case SHAVE::CC_CMU11:
  case SHAVE::CC_CMU12:
  case SHAVE::CC_CMU13:
  case SHAVE::CC_CMU14:
  case SHAVE::CC_CMU15:
    if (TII.hasFeature(SHAVE::HasPCXC_Feature)) {
      lane = prm.reg - SHAVE::CC_CMU0;
      return SHAVE::PEU_PCXC;
    }
    // FIXME: Need to provide an alternative pattern for >= npu4.
    report_fatal_error(
        "Target doesn't support PCXC, unable to predicate on CC_CMU1-CC_CMU15");

  case SHAVE::C_CMU0:      
    if (SHAVEConflicts::check_usesSAU(predicatedOpcode))
      return SHAVE::PEU_PVS_16;
    else if (SHAVEConflicts::check_usesVAU(predicatedOpcode))
      return SHAVE::PEU_PVV_16; // This is broken on Myriad2 v1 - See Bugzilla #20528
    else
      return SHAVE::PEU_PVL0_16;
  case SHAVE::C_CMU_0_3:
    if (SHAVEConflicts::check_usesSAU(predicatedOpcode))
      return SHAVE::PEU_PVS_32;
    else if (SHAVEConflicts::check_usesVAU(predicatedOpcode))
      return SHAVE::PEU_PVV_32;
    else {
      if (I.getOperand(0).isReg() && SHAVE::IRF32RegClass.contains(I.getOperand(0).getReg()))
	return SHAVE::PEU_PVL0_8;
      else
	return SHAVE::PEU_PVL0_32;
    }
  case SHAVE::C_CMU_0_15:
    if (SHAVEConflicts::check_usesSAU(predicatedOpcode))
      return SHAVE::PEU_PVS_8;
    else if (SHAVEConflicts::check_usesVAU(predicatedOpcode))
      return SHAVE::PEU_PVV_8;
    else
      return SHAVE::PEU_PVL0_8;

  default:
    llvm_unreachable("Unknown predication type.");
    return 0;
  }
}

SHAVEPredicator::PredRegAndMask SHAVEPredicator::getPredRegAndMask(const TargetInstrInfo &TII, MachineInstr &I) {
  assert(TII.isPredicated(I) && "This must be a predicated instruction!");
  assert(I.getNumOperands() > 0 && "Instruction must have at least one operand!");

  const MCInstrDesc &TID = I.getDesc();

  for (unsigned i = 0; i < I.getNumOperands(); ++i) {
    if (TID.operands()[i].isPredicate()) {
      PredRegAndMask retVal;
      retVal.mask = (SHAVECC::CondCode)I.getOperand(i).getImm();
      retVal.reg = I.getOperand(i + 1).getReg();
      return retVal; // FIXME: Movidius - bug? Raises a 'Run-Time Check Failure'
    }
  }

  llvm_unreachable("No predicate operand");
}

bool SHAVEPredicator::runOnMBB(MachineBasicBlock &MBB) {
  const auto &STI = MBB.getParent()->getSubtarget<SHAVESubtarget>();
  const SHAVEInstrInfo &TII = *STI.getInstrInfo();
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();
  MachineBasicBlock::iterator pipeInst;
  MachineBasicBlock::iterator it = MBB.begin();

  while (it != MBB.end()) {
    if (TII.isPredicated(*it) && !it->isInsideBundle()) {
      PredRegAndMask prm = getPredRegAndMask(TII, *it);
      int lane = -1;
      unsigned opcode = getPredOpcode(TII, prm, it->getOpcode(), *it, lane);
      DebugLoc dbgLoc = it->getDebugLoc();

      MachineInstrBuilder PredMI = BuildMI(MBB, it, dbgLoc, TII.get(opcode)).addImm(prm.mask);

      if (lane != -1)
        PredMI = PredMI.addImm(lane);

      for (unsigned i = 0, e = it->getNumOperands(); i != e; ++i) {
        const MachineOperand &MO = it->getOperand(i);

        if (!MO.isReg())
          continue;

        unsigned Reg = MO.getReg();

        if (!Reg)
          continue;

        if (MO.isDef()) {
          it->addOperand(MachineOperand::CreateReg(Reg, false/*IsDef*/,
                         true/*IsImp*/, false/*IsKill*/,
                         false/*IsDead*/, true/*IsUndef*/));

          for (MCSubRegIterator SubRegs(Reg, TRI); SubRegs.isValid(); ++SubRegs) {
            it->addOperand(MachineOperand::CreateReg(*SubRegs, false/*IsDef*/,
                           true/*IsImp*/, false/*IsKill*/,
                           false/*IsDead*/, true/*IsUndef*/));
          }
        }
      }

      it++;
      MIBundleBuilder MIBB(MBB, PredMI, it);
    } else
      it++;
  }

  return false;
}

bool SHAVEPredicator::runOnMachineFunction(MachineFunction &MF) {
  for (MachineFunction::iterator bit = MF.begin(), e = MF.end();  bit != e; ++bit)
    runOnMBB(*bit);

  return false;
}

FunctionPass *llvm::createSHAVEPredicator() {
  return new SHAVEPredicator();
}

INITIALIZE_PASS(SHAVEPredicator, "shave-predicator",
                "SHAVE Predicator", false, false)

////////////////////////////////////////////////////////////////////////////////

char SHAVELoopLoadHoister::ID = 0;

void SHAVELoopLoadHoister::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  AU.addPreserved<SlotIndexes>();
  AU.addPreservedID(&LiveIntervalsID);
  AU.addRequiredID(&LiveIntervalsID);
  AU.addPreservedID(&LiveVariablesID);
  AU.addRequiredID(&MachineLoopInfoID);
  AU.addPreservedID(&MachineLoopInfoID);
  MachineFunctionPass::getAnalysisUsage(AU);
}

bool SHAVELoopLoadHoister::runOnMBB(MachineBasicBlock &BB) {
  MachineFunction &MF = *BB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  MachineLoopInfo &LI = getAnalysis<MachineLoopInfo>();
  LiveIntervals &LIS = getAnalysis<LiveIntervals>();
  MachineLoop *L = LI.getLoopFor(&BB);

  if (!L)
    return false;

  // Find load MIs whose address depend on a PHI value.
  bool Changed = false;
  std::set<unsigned> LoopRegs;
  std::vector<MachineInstr *> LoopLoadMIs;
  const unsigned MaxHoistLoadsVRF = 24;
  const unsigned MaxHoistLoadsIRF = 16;
  unsigned HoistedLoadsVRF = 0;
  unsigned HoistedLoadsIRF = 0;
  const SHAVETargetMachine &TM = static_cast<const SHAVETargetMachine&>(MF.getTarget());
  const TargetRegisterInfo *TRI = TM.getSubtargetImpl()->getRegisterInfo();
  const TargetInstrInfo *TII = TM.getSubtargetImpl()->getInstrInfo();

  for (MachineBasicBlock::iterator I = BB.begin(), E = BB.end(); I != E; ++I) {
    const MCInstrDesc &MCID = I->getDesc();

    if (I->isDebugValue() || I->isPosition() || I->isCFIInstruction())
      continue;

    if (MRI.isSSA() && (I->getOpcode() == TargetOpcode::PHI)) {
      unsigned PhiReg = I->getOperand(0).getReg();
      LoopRegs.insert(PhiReg);
      continue;
    }

    // Look for loads using a loop register.
    if (SHAVEConflicts::check_isLoad(I->getOpcode())) {
      bool IsCandidate = false;

      if (MRI.isSSA()) {
        for (unsigned iOperand = 0; iOperand < I->getNumOperands(); iOperand++) {
          MachineOperand &MO = I->getOperand(iOperand);

          if (!MO.isReg() || MO.isImplicit() || !MO.isUse() ||
              (!MCID.operands().empty() &&
               MCID.operands()[iOperand].isPredicate()))
            continue;

          if (LoopRegs.find(MO.getReg()) != LoopRegs.end()) {
            IsCandidate = true;
            break;
          }
        }
      } else
        IsCandidate = true;

      if (IsCandidate) {
        const TargetRegisterClass *DefRC = TII->getRegClass(I->getDesc(), 0, TRI, MF);

        if (DefRC) {
          if (DefRC->hasSuperClassEq(&SHAVE::VRF128RegClass)) {
            if (HoistedLoadsVRF < MaxHoistLoadsVRF) {
              HoistedLoadsVRF++;
              LoopLoadMIs.push_back(&*I);
            }
          } else if (HoistedLoadsIRF < MaxHoistLoadsIRF) {
            HoistedLoadsIRF++;
            LoopLoadMIs.push_back(&*I);
          }
        }
      }

      continue;
    }

    if (MRI.isSSA()) {
      // Figure out whether the instruction uses a loop register.
      bool UsesLoopReg = false;

      for (unsigned iOperand = 0; iOperand < I->getNumOperands(); iOperand++) {
        MachineOperand &MO = I->getOperand(iOperand);

        if (!MO.isReg() || MO.isImplicit() || !MO.isUse() ||
            (!MCID.operands().empty() &&
             MCID.operands()[iOperand].isPredicate()))
          continue;

        unsigned UseReg = MO.getReg();

        UsesLoopReg = UsesLoopReg || (LoopRegs.find(UseReg) != LoopRegs.end());
      }

      // Update the set of live loop registers.
      for (unsigned iOperand = 0; iOperand < I->getNumOperands(); iOperand++) {
        MachineOperand &MO = I->getOperand(iOperand);

        if (!MO.isReg() || MO.isImplicit() || !MO.isDef() ||
            (!MCID.operands().empty() &&
             MCID.operands()[iOperand].isPredicate()))
          continue;

        unsigned DefReg = MO.getReg();
        std::set<unsigned>::iterator RegIt = LoopRegs.find(DefReg);

        if (UsesLoopReg)
          LoopRegs.insert(DefReg);
        else if (RegIt != LoopRegs.end())
          LoopRegs.erase(RegIt);
      }
    }
  }

  // Try to host the load MIs we found as high as possible in the BB.
  std::set<unsigned> RegUses;

  for (unsigned iLoopMI = 0; iLoopMI < LoopLoadMIs.size(); iLoopMI++) {
    // Cache the list of registers used by the load MI.
    MachineInstr *LoadMI = LoopLoadMIs[iLoopMI];

    if (LoadMI->isDebugValue() || LoadMI->isPosition() || LoadMI->isCFIInstruction())
      continue;

    RegUses.clear();

    for (unsigned iOperand = 0; iOperand < LoadMI->getNumOperands(); iOperand++) {
      MachineOperand &MO = LoadMI->getOperand(iOperand);

      if (!MO.isReg() || !MO.isUse() || !MO.getReg())
        continue;

      RegUses.insert(MO.getReg());
    }

    // Find the highest point in the BB for the MI.
    MachineBasicBlock::iterator InitialPoint = LoadMI;
    MachineBasicBlock::iterator InsertPoint = InitialPoint;
    unsigned DefReg = LoadMI->getOperand(0).getReg();

    while (InsertPoint != BB.begin()) {
      MachineInstr *PrevMI = std::prev(&*InsertPoint);

      // Do not move a load above another load or store, branches and calls.
      if (SHAVEConflicts::check_isLoad(PrevMI->getOpcode()) || SHAVEConflicts::check_isStore(PrevMI->getOpcode())
          || PrevMI->isBranch() || PrevMI->isCall() || PrevMI->isPHI())
        break;

      // Do not move a load above an instruction that defines the pointer,
      // or an instruction that uses the loaded value.
      bool DefUseConflict = false;
      bool UseDefConflict = false;

      for (unsigned iOperand = 0; iOperand < PrevMI->getNumOperands(); iOperand++) {
        MachineOperand &MO = PrevMI->getOperand(iOperand);

        if (!MO.isReg())
          continue;

        if (MO.isDef() && (RegUses.find(MO.getReg()) != RegUses.end())) {
          DefUseConflict = true;
          break;
        } else if (MO.isUse() && (DefReg == MO.getReg())) {
          UseDefConflict = true;
          break;
        }
      }

      if (DefUseConflict || UseDefConflict)
        break;

      InsertPoint--;
    }

    // Hoist the MI if possible.
    if (InsertPoint != InitialPoint) {
      LoadMI->removeFromParent();
      BB.insert(InsertPoint, LoadMI);

      if (!MRI.isSSA())
        LIS.handleMove(*LoadMI, /*UpdateFlags=*/true);

      Changed = true;
    }
  }

  return Changed;
}

bool SHAVELoopLoadHoister::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;

  for (MachineBasicBlock& MBB : MF)
    Changed = Changed || runOnMBB(MBB);

  return Changed;
}

FunctionPass *llvm::createSHAVELoopLoadHoister() {
  return new SHAVELoopLoadHoister();
}

INITIALIZE_PASS(SHAVELoopLoadHoister, "Loop Load Hoister",
                "shave-loop-load-hoister", false, false)

////////////////////////////////////////////////////////////////////////////////

char SHAVECopyAddCombining::ID = 0;

bool SHAVECopyAddCombining::optimiseINCS(MachineInstr &MI /* MachineBasicBlock::iterator I*/) {
  // FIXME: Movidius - This function can replace the use of a register defined by a copy with the source
  //                   of that copy. It doesn't consider the case where the source of the copy has been
  //                   killed between the copy and the INC instruction. This results in undefined behaviour
#if 1
  return false;
#else
  // Identify INCS instructions and extract their immediates.
  const static int64_t MaxAddSubImm = 255ll;
  int64_t Imm = 0;

  switch (MI.getOpcode()) {
  default:
    return false;
  case SHAVE::IAU_INCS_i32:
    Imm = MI.getOperand(2).getImm();
    break;
  }

  assert((Imm != 0) && "Not expecting IAU.INCS to be selected with an immediate constant of 0");

  MachineFunction *MF = MI.getParent()->getParent();
  const SHAVEInstrInfo *TII = static_cast<const SHAVEInstrInfo *>(MF->getSubtarget().getInstrInfo());
  const SHAVERegisterInfo *regInfo = TII->getSHAVERegisterInfo();

  // Make sure we can use immediate variants of ADD/SUB.
  if ((Imm < -MaxAddSubImm) || (Imm > MaxAddSubImm))
    return false;

  // Find the copy MI that defines the INCS's def/use register.
  if (!MI.getOperand(0).isReg())
    return false;

  unsigned TiedReg = MI.getOperand(0).getReg();

  if (LiveCopyDefs.find(TiedReg) == LiveCopyDefs.end())
    return false;

  // Make sure that no instruction writes to the source register between the
  // COPY and INCS instructions.
  MachineInstr *CopyMI = LiveCopyDefs[TiedReg];

  // Finished if the pre-conditions for the transformatio are not met
  if (!CopyMI || (CopyMI->getOpcode() != SHAVE::COPY) || !CopyMI->getOperand(1).isReg())
    return false;

  assert((TiedReg == CopyMI->getOperand(0).getReg()) && "Expecting the COPY destination and IAU.INCS source registers to be the same");

  unsigned copySrcReg = CopyMI->getOperand(1).getReg();

  // If the copy source register is not an IRF, then this transformation is not valid
  if (!regInfo->isIRFRegister(CopyMI->getOperand(1).getReg()))
    return false;

  if (LiveCopyUses.find(copySrcReg) == LiveCopyUses.end())
    return false;

  // FIXME: Movidius - for some reason this optimisation breaks when the copy is from
  // an implicitly used 64 bit register. It should be possible to make this work
  for (unsigned i = 0; i < CopyMI->getNumOperands(); ++i)
    if (SHAVE::IRF64RegClass.contains(CopyMI->getOperand(i).getReg()))
      return false;

  // Determine the new instruction opcode.
  unsigned NewOpc = 0;

  switch (MI.getOpcode()) {
  case SHAVE::IAU_INCS_i32:
    NewOpc = (Imm >= 0) ? SHAVE::IAU_ADD_32_imm : SHAVE::IAU_SUB_32_imm;
    break;
  }

  if (NewOpc == 0)
    return false;

  if (Imm < 0)
    Imm = -Imm;

  // Create an ADD/SUB instruction to replace the COPY/INCS pair.
  MachineInstrBuilder MIB = BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                                    TII->get(NewOpc), TiedReg);

  MIB.add(CopyMI->getOperand(1));
  MIB.addImm(Imm);
  TII->finaliseMI(MIB);

  // Set the implicitely-defined registers as killed, since INCS has none.
  for (unsigned i = 0; i < MIB->getNumOperands(); i++) {
    MachineOperand &MO = MIB->getOperand(i);

    if (!MO.isReg() || !MO.isDef() || !MO.isImplicit())
      continue;

    MO.setIsDead(true);
  }

  if (std::find(DeleteList.begin(), DeleteList.end(), CopyMI) == DeleteList.end())
    DeleteList.push_back(CopyMI);

  if (std::find(DeleteList.begin(), DeleteList.end(), &MI) == DeleteList.end())
    DeleteList.push_back(&MI);

  return true;
#endif // 1
}

bool SHAVECopyAddCombining::runOnMBB(MachineBasicBlock &BB) {
  // Find sequences of COPY and INCS:
  // %I6<def> = COPY %I10
  // %I6<def,tied1> = IAU_INCS_i32 %I6<kill,tied0>, -30, pred:1, pred:%noreg
  bool Changed = false;

  MachineFunction *MF = BB.getParent();
  const SHAVEInstrInfo *TII = static_cast<const SHAVEInstrInfo *>(MF->getSubtarget().getInstrInfo());
  const SHAVERegisterInfo *regInfo = TII->getSHAVERegisterInfo();

  LiveCopyDefs.clear();
  LiveCopyUses.clear();

  for (MachineInstr& MI : BB) {
    unsigned Opc = MI.getOpcode();

    if (Opc == TargetOpcode::COPY) {
      unsigned CopyDefReg = MI.getOperand(0).getReg();
      unsigned CopyUseReg = MI.getOperand(1).getReg();

      LiveCopyDefs[CopyDefReg] = &MI;
      LiveCopyUses.insert(CopyUseReg);
    } else if (optimiseINCS(MI)) {
      Changed = true;
      DEBUG(dbgs() << "Optimised redundant COPY and INCS: " << MI);
    } else if (MI.isCall()) {
      // Do not combine COPY/INCS across function calls.
      LiveCopyDefs.clear();
      LiveCopyUses.clear();
    }

    // Keep the mapping of live copy-defined registers up to date.
    for (unsigned i = 0; i < MI.getNumOperands(); i++) {
      MachineOperand &MO = MI.getOperand(i);

      if (!MO.isReg())
        continue;

      if (MO.isUse() && MO.isKill()) {
        unsigned KillReg = MO.getReg();

        if (SHAVE::IRF64RegClass.contains(KillReg)) {
          unsigned KillReg0 = regInfo->getSubReg(KillReg, SHAVE::vsub32_0);
          unsigned KillReg1 = regInfo->getSubReg(KillReg, SHAVE::vsub32_1);
          if (LiveCopyDefs.find(KillReg0) != LiveCopyDefs.end())
            LiveCopyDefs.erase(KillReg0);
          if (LiveCopyDefs.find(KillReg1) != LiveCopyDefs.end())
            LiveCopyDefs.erase(KillReg1);
        }

        if (LiveCopyDefs.find(KillReg) != LiveCopyDefs.end())
          LiveCopyDefs.erase(KillReg);
      } else if (MO.isDef()) {
        unsigned DefReg = MO.getReg();

        if (SHAVE::IRF64RegClass.contains(DefReg)) {
          unsigned DefReg0 = regInfo->getSubReg(DefReg, SHAVE::vsub32_0);
          unsigned DefReg1 = regInfo->getSubReg(DefReg, SHAVE::vsub32_1);
          if (LiveCopyUses.find(DefReg0) != LiveCopyUses.end())
            LiveCopyUses.erase(DefReg0);
          if (LiveCopyUses.find(DefReg1) != LiveCopyUses.end())
            LiveCopyUses.erase(DefReg1);
        }

        if (LiveCopyUses.find(DefReg) != LiveCopyUses.end())
          LiveCopyUses.erase(DefReg);
      }
    }
  }

  // Delete uneeded instructions.
  for (unsigned i = 0; i < DeleteList.size(); i++) {
    MachineInstr *DeleteMI = DeleteList[i];
    DeleteMI->eraseFromParent();
  }

  DeleteList.clear();

  return Changed;
}

bool SHAVECopyAddCombining::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;

  for (MachineBasicBlock& MBB : MF)
    Changed = Changed || runOnMBB(MBB);

  return Changed;
}

FunctionPass *llvm::createSHAVECopyAddCombining() {
  return new SHAVECopyAddCombining();
}
