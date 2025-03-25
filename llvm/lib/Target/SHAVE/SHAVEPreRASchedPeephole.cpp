//===-- SHAVEPreRASchedPeephole.cpp - Pre-RASched Peephole Pass -*- C++ -*-===//
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

#define DEBUG_TYPE "shave-prera-scheduler-peephole"

#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SlotIndexes.h"
#include "llvm/Support/Debug.h"
#include "llvm/InitializePasses.h"

#include "SHAVEPreRASchedPeephole.h"

using namespace llvm;

/*
    Desirable peepholes (adapted for predication as necessary):

    1.  'JMP I0' with 'JMP vreg'
        Replace sequence:
          LSU.LDIL I0, sym
          LSU.LDIH I0, sym
          BRU.JMP  I0<kill>
        with:
          LSU.LDIL vreg, sym
          LSU.LDIH vreg, sym
          BRU.JMP  vreg<kill>

    2.  'JMP' with 'BRA'
        Replace sequence:
          LSU.LDIL vreg|I0, label
          LSU.LDIL vreg|I0, label
          BRU.JMP  vreg|I0<kill>
        with:
          BRU.BRA  label

 */


//
// Initialisation code required by the Pass Manager
//
char SHAVEPreRASchedPeephole::ID = 0;

INITIALIZE_PASS_BEGIN(SHAVEPreRASchedPeephole, "shavepreraschedulerpeepholepass", "SHAVE Pre-RA Scheduler Peephole Pass", false, false)
INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_DEPENDENCY(SlotIndexes)
INITIALIZE_PASS_END(SHAVEPreRASchedPeephole, "shavepreraschedulerpeepholepass", "SHAVE Pre-RA Scheduler Peephole Pass", false, false)

MachineFunctionPass *llvm::createSHAVEPreRASchedPeepholePass() {
  return new SHAVEPreRASchedPeephole();
}

void SHAVEPreRASchedPeephole::setupShuffle(ShuffleInfo shuffleInfo,
                                           MachineInstrBuilder &shuffle,
                                           int64_t swizzleLane) {
  if (shuffleInfo.is32bit) {
    for (unsigned int i = 0; i < 4; ++i) {
      shuffle.addImm(swizzleLane * 2);
      shuffle.addImm(swizzleLane * 2 + 1);
    }
  } else {
    for (unsigned int i = 0; i < 8; ++i)
      shuffle.addImm(swizzleLane);
  }
}

bool SHAVEPreRASchedPeephole::replaceShuffles() {
  bool changed = false;
  MachineRegisterInfo &MRI = currentFunction->getRegInfo();
  SlotIndexes * indexes = LIS->getSlotIndexes();

  struct ShuffleInfo shuffles[] = {
    // 128-bit splats
    { SHAVE::CMU_CPIVR_i16,   SHAVE::CMU_CPVI_x16_l, false },
    { SHAVE::CMU_CPIVR_f16,   SHAVE::CMU_CPVI_f16_l, false },
    { SHAVE::CMU_CPIVR_i32,   SHAVE::CMU_CPVI_x32,   true },
    { SHAVE::CMU_CPIVR_f32,   SHAVE::CMU_CPVI_f32,   true },
    // 64-bit splats
    { SHAVE::CMU_CPIVR_v4i16, SHAVE::CMU_CPVI_v4i16_l, false },
    { SHAVE::CMU_CPIVR_v4f16, SHAVE::CMU_CPVI_v4f16_l, false },
    { SHAVE::CMU_CPIVR_v2i32, SHAVE::CMU_CPVI_x32_v2i32,   true },
    { SHAVE::CMU_CPIVR_v2f32, SHAVE::CMU_CPVI_f32_v2f32,   true },
  };

  for (ShuffleInfo &shuffleInfo : shuffles) {
    bool changesMade = false;

    do {
      changesMade = false;

      for (MachineBasicBlock &block : *currentFunction) {
        for (MachineInstr &splat : block) {
          if (splat.getOpcode() == shuffleInfo.splatOpcode) {
            unsigned int vectorSplatInputReg = splat.getOperand(1).getReg();
            MachineInstr * extract = nullptr;

            for (MachineInstr &input : block) {
              if (input.definesRegister(vectorSplatInputReg)) {
                if (input.getOpcode() == shuffleInfo.extractOpcode)
                  extract = &input;
                break;
              }
            }

            if (extract == nullptr)
              continue;

            unsigned int vectorSplatReg = splat.getOperand(0).getReg();
            unsigned int inputReg = extract->getOperand(1).getReg();
            int64_t swizzleLane = extract->getOperand(2).getImm();

            // FIXME: Movidius - Why is this even possible? Surely an input swizzle on a CPVI is always pointless?
            unsigned int inputOpcode = MRI.def_instr_begin(inputReg)->getOpcode();
            if (inputOpcode == SHAVE::INPUT_SWIZZLE8 || inputOpcode == SHAVE::INPUT_SWIZZLE8_vrf64)
              continue;

            bool shuffleAdded;
            bool useInstructionExists = false;
            do {
              shuffleAdded = false;
              for (MachineInstr &useInstruction : block) {
                if (useInstruction.readsRegister(vectorSplatReg) && useInstruction.getBundleSize() == 0 &&
                    !SHAVEConflicts::check_isCMU_CP(useInstruction.getOpcode())) {
                  unsigned int operand = useInstruction.findRegisterUseOperandIdx(vectorSplatReg);
                  unsigned int splatOperandIndex = useInstruction.getNumExplicitDefs();

                  if (operand == (splatOperandIndex + 1) && useInstruction.isCommutable()) {
                    SII->commuteInstruction(useInstruction, false, splatOperandIndex, splatOperandIndex + 1);
                    operand = useInstruction.findRegisterUseOperandIdx(vectorSplatReg);
                  }

                  if (operand == splatOperandIndex && !useInstruction.isRegTiedToDefOperand(operand)) { // if this is the first input operand
                    unsigned int opcode = 0;
                    if (SHAVEConflicts::check_usesVAU(useInstruction.getOpcode()))
                      opcode = SHAVE::LSU_SWZV8;
                    else if (SHAVEConflicts::check_usesCMU(useInstruction.getOpcode()))
                      opcode = SHAVE::LSU_SWZC8;
                    else if (SHAVEConflicts::check_usesSAU(useInstruction.getOpcode()))
                      opcode = SHAVE::LSU_SWZS8;
                    else
                      continue;

                    // Change the input operand to the original vector register from which we are splatting
                    useInstruction.getOperand(operand).setReg(inputReg);

                    // Add the swizzle instruction and bundle it with the use instruction
                    MachineInstrBuilder shuffle = BuildMI(block, std::next(MachineBasicBlock::instr_iterator(useInstruction)), useInstruction.getDebugLoc(), SII->get(opcode));
                    setupShuffle(shuffleInfo, shuffle, swizzleLane);

                    SII->finaliseMI(shuffle);
                    shuffle->bundleWithPred();

                    LIS->handleMove(useInstruction);

                    changed = changesMade = shuffleAdded = useInstructionExists = true;
                    break;
                  }
                }
              }
            } while (shuffleAdded);

            // If there's no original instruction to attach the shuffle to,
            // create our own
            if (!useInstructionExists) {
              MachineInstrBuilder copy;
              assert(SII->hasFeature(SHAVE::HasVRF128_Feature));

            // LSU.SWZC8 has no effect on CMU.CPED on 2.3, so use
            // CMU.ALIGNVEC instead
            copy = BuildMI(
                block, std::next(MachineBasicBlock::instr_iterator(splat)),
                splat.getDebugLoc(), SII->get(SHAVE::CMU_ALIGNVEC_imm_vrf));
            copy.addDef(vectorSplatReg);
            copy.addReg(inputReg);
            copy.addReg(inputReg);
            copy.addImm(0);

              SII->finaliseMI(copy);

              MachineInstrBuilder shuffle = BuildMI(
                  block, std::next(MachineBasicBlock::instr_iterator(copy)),
                  copy->getDebugLoc(), SII->get(SHAVE::LSU_SWZC8));
              setupShuffle(shuffleInfo, shuffle, swizzleLane);

              SII->finaliseMI(shuffle);
              shuffle->bundleWithPred();

	      // FIXME: The SlotIndexes need to be reviewed. The newly created copy
	      //        instruction ends up with incorrect SlotIndex here. Hence the cleanup
	      //        See EISW-9233 for details
	      if (indexes->hasIndex(*copy))
		LIS->RemoveMachineInstrFromMaps(*copy);
	      LIS->InsertMachineInstrInMaps(*copy);

              // Remove the old instructions
              splat.eraseFromBundle();
              if (MRI.use_empty(extract->getOperand(0).getReg()))
                extract->eraseFromBundle();

              changed = changesMade = true;
            } else {
              if (MRI.use_empty(vectorSplatReg))
                splat.eraseFromBundle();
              if (MRI.use_empty(extract->getOperand(0).getReg()))
                extract->eraseFromBundle();
            }

          }

          if (changesMade)
            break;
        }

        if (changesMade)
          break;
      }

    } while (changesMade);
  }

  return changed;
}

static std::pair<bool, bool> getACCType(unsigned int opcode) {
  bool isFloat = false, isVector = false;
  switch (opcode) {
  case SHAVE::VAU_ACCP_SEQ_f16:
  case SHAVE::VAU_ACCP_SEQ_f32:
  case SHAVE::VAU_ACCP_SEQ_f16_Myr4:
  case SHAVE::VAU_ACCP_SEQ_f32_Myr4:
    isVector = true;
    LLVM_FALLTHROUGH;
  case SHAVE::SAU_ACCP_SEQ_f16:
  case SHAVE::SAU_ACCP_SEQ_f32:
    isFloat = true;
    break;
  case SHAVE::VAU_ACCP_SEQ_i16:
  case SHAVE::VAU_ACCP_SEQ_i32:
  case SHAVE::VAU_ACCP_SEQ_i8:
  case SHAVE::VAU_ACCP_SEQ_u16:
  case SHAVE::VAU_ACCP_SEQ_u32:
  case SHAVE::VAU_ACCP_SEQ_u8:
  case SHAVE::VAU_ACCP_SEQ_i8_Myr4:
  case SHAVE::VAU_ACCP_SEQ_i16_Myr4:
  case SHAVE::VAU_ACCP_SEQ_i32_Myr4:
    isVector = true;
    break;
  }
  return std::pair<bool, bool>(isFloat, isVector);
}

static std::pair<bool, bool> getMACType(unsigned int opcode) {
  bool isFloat = false, isVector = false;
  switch (opcode) {
  case SHAVE::VAU_MACP_SEQ_f16:
  case SHAVE::VAU_MACP_SEQ_f32:
  case SHAVE::VAU_MACP_SEQ_f16_Myr4:
  case SHAVE::VAU_MACP_SEQ_f32_Myr4:
    isVector = true;
    LLVM_FALLTHROUGH;
  case SHAVE::SAU_MACP_SEQ_f16:
  case SHAVE::SAU_MACP_SEQ_f32:
    isFloat = true;
    break;
  case SHAVE::VAU_MACP_SEQ_i16:
  case SHAVE::VAU_MACP_SEQ_i32:
  case SHAVE::VAU_MACP_SEQ_i8:
  case SHAVE::VAU_MACP_SEQ_u16:
  case SHAVE::VAU_MACP_SEQ_u32:
  case SHAVE::VAU_MACP_SEQ_u8:
  case SHAVE::VAU_MACP_SEQ_i8_Myr4:
  case SHAVE::VAU_MACP_SEQ_i16_Myr4:
  case SHAVE::VAU_MACP_SEQ_i32_Myr4:
    isVector = true;
    break;
  }
  return std::pair<bool, bool>(isFloat, isVector);
}

bool SHAVEPreRASchedPeephole::expandACCMACPseudos() {
  bool changed = false;
  bool changesMade;

  do {
    changesMade = false;

    for (MachineBasicBlock &block : *currentFunction) {
      for (MachineInstr &instruction : block) {
        unsigned int opcode = instruction.getOpcode();
        if (SHAVEConflicts::check_isMACPSeq(opcode)) {
          std::pair<bool, bool> type = getMACType(opcode);
          SII->ExpandMACPSequence(block, instruction, type.first, type.second, LIS); // Expand the pseudo instruction

          // Delete the pseudo instruction
          LIS->RemoveMachineInstrFromMaps(instruction);
          instruction.eraseFromParent();

          changed = changesMade = true;
          break;
        }
        else if (SHAVEConflicts::check_isACCPSeq(opcode)) {
          std::pair<bool, bool> type = getACCType(opcode);
          SII->ExpandACCSequence(block, instruction, type.first, type.second, LIS); // Expand the pseudo instruction

          // Delete the pseudo instruction
          LIS->RemoveMachineInstrFromMaps(instruction);
          instruction.eraseFromParent(); 

          changed = changesMade = true;
          break;
        }
      }
      if (changesMade)
        break;
    }
  } while (changesMade);

  return changed;
}

//
// Definition of public member functions of SHAVEPreRASchedPeephole
//

void SHAVEPreRASchedPeephole::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  AU.addRequired<LiveIntervals>();
  AU.addRequired<SlotIndexes>();
  MachineFunctionPass::getAnalysisUsage(AU);
}

bool SHAVEPreRASchedPeephole::runOnMachineFunction(MachineFunction &MF) {
  DEBUG(dbgs() << "SHAVEPreRASchedPeephole: Starting pre-RA Scheduler peepholes for machine function " << MF.getName() << "\n");
  currentFunction = &MF;
  LIS = &getAnalysis<LiveIntervals>();

  const SHAVETargetMachine &TM = static_cast<const SHAVETargetMachine &>(currentFunction->getTarget());
  SII = static_cast<const SHAVEInstrInfo *>(TM.getSubtargetImpl()->getInstrInfo());

  bool changed = false;

  changed |= expandACCMACPseudos();
  changed |= replaceShuffles();

  DEBUG(dbgs() << "SHAVEPreRASchedPeephole: Finished pre-RA Scheduler peepholes for machine function " << MF.getName() << "\n");

  return changed;
}
