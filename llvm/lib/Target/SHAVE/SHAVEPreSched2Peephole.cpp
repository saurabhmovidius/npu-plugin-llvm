//===-- SHAVEPreSched2Peephole.cpp - Pre-RASched Peephole Pass -*- C++ -*-===//
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

#define DEBUG_TYPE "shave-pre-sched2-peephole"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/InitializePasses.h"
#include "SHAVEPreSched2Peephole.h"

#include "SHAVEGenConflicts.inc"

using namespace llvm;

//
// Initialisation code required by the Pass Manager
//
char SHAVEPreSched2Peephole::ID = 0;

INITIALIZE_PASS(SHAVEPreSched2Peephole, "shavepresched2peepholepass", "SHAVE Pre-Sched2 Peephole Pass", false, false)

MachineFunctionPass *llvm::createSHAVEPreSched2PeepholePass() {
  return new SHAVEPreSched2Peephole();
}

// Peephole to detect subreg-to-reg identity copies due to i8->i32 anyext instructions
// e.g. a CMU_CPII_i8_i32_anyext i8 i8_q0 is emitted as a CMU.CP i8 i8 and can be eliminated
bool SHAVEPreSched2Peephole::eliminateIdentityCopies() {
  std::vector<MachineInstr *> identityCopies;

  // Collect a list of identity copy instructions  
  for (MachineBasicBlock &block : *currentFunction) {
    for (MachineInstr &instr : block.instrs()) {
      unsigned int opc = instr.getOpcode();
      switch (opc) {
      case SHAVE::CMU_CPII_i8_i16_anyext:
      case SHAVE::CMU_CPII_i8_i32_anyext:
      case SHAVE::CMU_CPII_i16_i32_anyext:
        if(SRI->isSuperRegister(instr.getOperand(1).getReg(), instr.getOperand(0).getReg()))
          identityCopies.push_back(&instr);
        break;

      case SHAVE::CMU_CPII_32_8_trunc:
      case SHAVE::CMU_CPII_32_16_trunc:
      case SHAVE::CMU_CPII_16_8_trunc:
        if (SRI->isSuperRegister(instr.getOperand(0).getReg(), instr.getOperand(1).getReg()))
          identityCopies.push_back(&instr);
        break;
      // CMU.ALIGNVEC with the same output register as both input registers and immediate 0
      // is an identity copy
      case SHAVE::CMU_ALIGNVEC_imm_vrf:
      case SHAVE::CMU_ALIGNVEC_imm_vrf64:
        if (!instr.isBundled() &&
            instr.getOperand(0).getReg() == instr.getOperand(1).getReg() &&
            instr.getOperand(1).getReg() == instr.getOperand(2).getReg() &&
            instr.getOperand(3).getImm() == 0)
          identityCopies.push_back(&instr);
        break;
      case SHAVE::CMU_ALIGNVEC_imm_vrf_vrf64:
        if (!instr.isBundled() && 
            SRI->isSuperRegister(instr.getOperand(0).getReg(), instr.getOperand(1).getReg()) &&
            instr.getOperand(1).getReg() == instr.getOperand(2).getReg() &&
            instr.getOperand(3).getImm() == 0)
          identityCopies.push_back(&instr);
        break;
      case SHAVE::CMU_ALIGNVEC_imm_vrf64_vrf:
        if (!instr.isBundled() && 
            SRI->isSuperRegister(instr.getOperand(1).getReg(), instr.getOperand(0).getReg()) &&
            instr.getOperand(1).getReg() == instr.getOperand(2).getReg() &&
            instr.getOperand(3).getImm() == 0)
          identityCopies.push_back(&instr);
        break;
      }

      if (SHAVEConflicts::check_isAlignvecExtract(instr.getOpcode())) {
        if (!instr.isBundled() &&
            SRI->isSuperRegister(instr.getOperand(0).getReg(), instr.getOperand(1).getReg())
            && instr.getOperand(2).getImm() == 0)
          identityCopies.push_back(&instr);
      }
    }
  }
  
  // Delete the instructions in the collected list
  for (MachineInstr *i:identityCopies)
    i->removeFromParent();

  return !identityCopies.empty();
}

// Final check to ensure that the LDO and STO offsets used are valid w.r.t. the shift amount needed
// by the Myriad4.0 ISA
bool SHAVEPreSched2Peephole::sanitizeMyr4LDOSTO() {
  std::vector<MachineInstr *> illegalLdoStoInsns;

  for (MachineBasicBlock &block : *currentFunction)
    for (MachineInstr &instr : block.instrs()) {
      unsigned int instrOpcode = instr.getOpcode();
      unsigned mask = 0;
      unsigned newOpcode = 0;
      bool isLoad = false;

      switch (instrOpcode) {
      case SHAVE::LSU_LDOV_256_v32i8:
      case SHAVE::LSU_LDOV_256_v16i16:
      case SHAVE::LSU_LDOV_256_v16f16:
      case SHAVE::LSU_LDOV_256_v8i32:
      case SHAVE::LSU_LDOV_256_v8f32:
        mask = 0x01f;
        newOpcode = SHAVE::LSU_LDV_256256_v32i8;
        isLoad = true;
        break;
      case SHAVE::LSU_STOV_256_v32i8:
      case SHAVE::LSU_STOV_256_v16i16:
      case SHAVE::LSU_STOV_256_v16f16:
      case SHAVE::LSU_STOV_256_v8i32:
      case SHAVE::LSU_STOV_256_v8f32:
        mask = 0x01f;
        newOpcode = SHAVE::LSU_STV_256_v32i8;
        break;

      case SHAVE::LSU_LDOV_128_v16i8:
      case SHAVE::LSU_LDOV_128_v8i16:
      case SHAVE::LSU_LDOV_128_v8f16:
      case SHAVE::LSU_LDOV_128_v4i32:
      case SHAVE::LSU_LDOV_128_v4f32:
        mask = 0x0f;
        newOpcode = SHAVE::LSU_LDV_128128_v16i8;
        isLoad = true;
        break;
      case SHAVE::LSU_STOV_128_v16i8:
      case SHAVE::LSU_STOV_128_v8i16:
      case SHAVE::LSU_STOV_128_v8f16:
      case SHAVE::LSU_STOV_128_v4i32:
      case SHAVE::LSU_STOV_128_v4f32:
        mask = 0x0f;
        newOpcode = SHAVE::LSU_STV_128_v16i8;
        break;

      case SHAVE::LSU_LDOV_64_v8i8:
      case SHAVE::LSU_LDOV_64_v4i16:
      case SHAVE::LSU_LDOV_64_v4f16:
      case SHAVE::LSU_LDOV_64_v2i32:
      case SHAVE::LSU_LDOV_64_v2f32:
        mask = 0x07;
        newOpcode = SHAVE::LSU_LDV_6464_v8i8;
        isLoad = true;
        break;
      case SHAVE::LSU_STOV_64_v8i8:
      case SHAVE::LSU_STOV_64_v4i16:
      case SHAVE::LSU_STOV_64_v4f16:
      case SHAVE::LSU_STOV_64_v2i32:
      case SHAVE::LSU_STOV_64_v2f32:
        mask = 0x07;
        newOpcode = SHAVE::LSU_STV_64_v8i8;
        break;

      case SHAVE::LSU_LDOV_32_v4i8:
      case SHAVE::LSU_LDOV_32_v2f16:
      case SHAVE::LSU_LDOV_32_v2i16:
        mask = 0x03;
        newOpcode = SHAVE::LSU_LDV_3232_v4i8;
        isLoad = true;
        break;
      case SHAVE::LSU_STOV_32_v2f16:
      case SHAVE::LSU_STOV_32_v2i16: 
      case SHAVE::LSU_STOV_32_v4i8:
        mask = 0x03;
        newOpcode = SHAVE::LSU_STV_32_v4i8;
        break;

      case SHAVE::LSU_LDO_i32:
      case SHAVE::LSU_LDO_f32:
        mask = 0x03;
        newOpcode = SHAVE::LSU_LD_i32;
        isLoad = true;
        break;
      case SHAVE::LSU_STO_i32:
      case SHAVE::LSU_STO_f32:
        mask = 0x03;
        newOpcode = SHAVE::LSU_ST_i32;
        break;

      case SHAVE::LSU_LDO_i16:
      case SHAVE::LSU_LDO_f16:
        mask = 0x01;
        newOpcode = SHAVE::LSU_LD_i16;
        isLoad = true;
        break;
      case SHAVE::LSU_STO_i16:
      case SHAVE::LSU_STO_f16:
        mask = 0x01;
        newOpcode = SHAVE::LSU_ST_i16;
        break;

      default:
        break;
      }

      if (newOpcode == 0)
        continue;

      unsigned offset = instr.getOperand(2).getImm();
      if ((offset & mask) == 0)
        continue;

      // Offset must fit in the 8-bit sbimm field of IAU.ADD
      // If not we need to load it in a temporary reg first
      if (offset > 0x0000007F) {

        MachineInstrBuilder ldilInstr =
            BuildMI(block, instr, instr.getDebugLoc(),
                    SII->get(SHAVE::LSU_LDIL), SHAVE::I0);
        ldilInstr.addImm(offset & 0x0000FFFF);
        ldilInstr.copyImplicitOps(instr);
        SII->finaliseMI(ldilInstr);

        unsigned ImmHi = (offset & 0xFFFF0000);
        if (ImmHi) {
          SII->finaliseMI(BuildMI(block, instr, instr.getDebugLoc(),
                                  SII->get(SHAVE::LSU_LDIH), SHAVE::I0)
                              .copyImplicitOps(instr)
                              .addReg(SHAVE::I0)
                              .addImm(ImmHi));
        }

        MachineInstrBuilder addInstr =
            BuildMI(block, instr, instr.getDebugLoc(),
                    SII->get(SHAVE::IAU_ADD_32), SHAVE::I0);
        addInstr.addReg(instr.getOperand(1).getReg());
        addInstr.addReg(SHAVE::I0);
        SII->finaliseMI(addInstr);

      } else {
	// Offset fits in IAU.ADD imm field
        MachineInstrBuilder newInstr =
            BuildMI(block, instr, instr.getDebugLoc(),
                    SII->get(SHAVE::IAU_ADD_32), SHAVE::I0);
        newInstr.addReg(instr.getOperand(1).getReg());
        newInstr.addImm(offset);
        SII->finaliseMI(newInstr);
      }

      MachineInstrBuilder newInstr =
          BuildMI(block, instr, instr.getDebugLoc(), SII->get(newOpcode));
      if (isLoad)
        newInstr.addDef(instr.getOperand(0).getReg());
      else
        newInstr.addReg(instr.getOperand(0).getReg());
      newInstr.addReg(SHAVE::I0);
      SII->finaliseMI(newInstr);

      illegalLdoStoInsns.push_back(&instr);
    }

  // Delete the instructions in the collected list
  for (MachineInstr *i : illegalLdoStoInsns)
    i->removeFromParent();

  return !illegalLdoStoInsns.empty();
}

//
// Definition of public member functions of SHAVEPreSched2Peephole
//
bool SHAVEPreSched2Peephole::runOnMachineFunction(MachineFunction &MF) {
  DEBUG(dbgs() << "SHAVEPreSched2Peephole: Starting pre-Sched2 peepholes for machine function " << MF.getName() << "\n");
  currentFunction = &MF;
  SII = currentFunction->getSubtarget<SHAVESubtarget>().getInstrInfo();
  SRI = SII->getSHAVERegisterInfo();

  bool returnValue = eliminateIdentityCopies();

  // For NPU4+ ensure that the LDO/STO offsets can be right shifted losslessly
  // during assembly
  if (SII->hasFeature(SHAVE::HasLDOSTOMode_Feature))
    returnValue = sanitizeMyr4LDOSTO() || returnValue;

  DEBUG(dbgs() << "SHAVEPreSched2Peephole: Finished pre-Sched2 peepholes for machine function " << MF.getName() << "\n");

  return returnValue;
}
