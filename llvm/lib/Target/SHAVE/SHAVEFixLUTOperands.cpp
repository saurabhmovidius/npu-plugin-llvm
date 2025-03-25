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
// File       : SHAVEFixLUTOperands.cpp
// Description: Adjusts the register to which the promoted array is assigned
// the actual promotion is done in ScalarReplAggregates.cpp
// ---------------------------------------------------------------------------

#define DEBUG_TYPE "shave-fix-lut-operands"

#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "SHAVEFixLUTOperands.h"
#include "SHAVETargetMachine.h"


char SHAVEFixLUTOperands::ID = 0;


FunctionPass *llvm::createSHAVEFixLUTOperands() {
  return new SHAVEFixLUTOperands();
}

SHAVEFixLUTOperands::SHAVEFixLUTOperands()
: MachineFunctionPass(ID)
{}


bool SHAVEFixLUTOperands::isLUTPseudoInstruction(unsigned opcode, const MachineFunction &MF) {
  switch (opcode) {
  // Myriad4
  case SHAVE::CMU_LUT_i32_v16i32_Myr4_pseudo:
  case SHAVE::CMU_LUT_i32_v8i32_Myr4_pseudo:
  case SHAVE::CMU_LUT_i32_v4i32_Myr4_pseudo:
  case SHAVE::CMU_LUT_i32_v2i32_Myr4_pseudo:

  case SHAVE::CMU_LUT_i16_v32i16_Myr4_pseudo:
  case SHAVE::CMU_LUT_i16_v16i16_Myr4_pseudo:
  case SHAVE::CMU_LUT_i16_v8i16_Myr4_pseudo:
  case SHAVE::CMU_LUT_i16_v4i16_Myr4_pseudo:
  case SHAVE::CMU_LUT_i16_v2i16_Myr4_pseudo:

  case SHAVE::CMU_LUT_i8_v64i8_Myr4_pseudo:
  case SHAVE::CMU_LUT_i8_v32i8_Myr4_pseudo:
  case SHAVE::CMU_LUT_i8_v16i8_Myr4_pseudo:
  case SHAVE::CMU_LUT_i8_v8i8_Myr4_pseudo:
  case SHAVE::CMU_LUT_i8_v4i8_Myr4_pseudo:

  case SHAVE::CMU_LUT_f32_v16f32_Myr4_pseudo:
  case SHAVE::CMU_LUT_f32_v8f32_Myr4_pseudo:
  case SHAVE::CMU_LUT_f32_v4f32_Myr4_pseudo:
  case SHAVE::CMU_LUT_f32_v2f32_Myr4_pseudo:

  case SHAVE::CMU_LUT_f16_v32f16_Myr4_pseudo:
  case SHAVE::CMU_LUT_f16_v16f16_Myr4_pseudo:
  case SHAVE::CMU_LUT_f16_v8f16_Myr4_pseudo:
  case SHAVE::CMU_LUT_f16_v4f16_Myr4_pseudo:
  case SHAVE::CMU_LUT_f16_v2f16_Myr4_pseudo:

  case SHAVE::CMU_LUTW_i32_v16i32_Myr4_pseudo:
  case SHAVE::CMU_LUTW_i32_v8i32_Myr4_pseudo:
  case SHAVE::CMU_LUTW_i32_v4i32_Myr4_pseudo:
  case SHAVE::CMU_LUTW_i32_v2i32_Myr4_pseudo:

  case SHAVE::CMU_LUTW_i16_v32i16_Myr4_pseudo:
  case SHAVE::CMU_LUTW_i16_v16i16_Myr4_pseudo:
  case SHAVE::CMU_LUTW_i16_v8i16_Myr4_pseudo:
  case SHAVE::CMU_LUTW_i16_v4i16_Myr4_pseudo:
  case SHAVE::CMU_LUTW_i16_v2i16_Myr4_pseudo:

  case SHAVE::CMU_LUTW_i8_v64i8_Myr4_pseudo:
  case SHAVE::CMU_LUTW_i8_v32i8_Myr4_pseudo:
  case SHAVE::CMU_LUTW_i8_v16i8_Myr4_pseudo:
  case SHAVE::CMU_LUTW_i8_v8i8_Myr4_pseudo:
  case SHAVE::CMU_LUTW_i8_v4i8_Myr4_pseudo:

  case SHAVE::CMU_LUTW_f32_v16f32_Myr4_pseudo:
  case SHAVE::CMU_LUTW_f32_v8f32_Myr4_pseudo:
  case SHAVE::CMU_LUTW_f32_v4f32_Myr4_pseudo:
  case SHAVE::CMU_LUTW_f32_v2f32_Myr4_pseudo:

  case SHAVE::CMU_LUTW_f16_v32f16_Myr4_pseudo:
  case SHAVE::CMU_LUTW_f16_v16f16_Myr4_pseudo:
  case SHAVE::CMU_LUTW_f16_v8f16_Myr4_pseudo:
  case SHAVE::CMU_LUTW_f16_v4f16_Myr4_pseudo:
  case SHAVE::CMU_LUTW_f16_v2f16_Myr4_pseudo:

  // Myriad2
  case SHAVE::CMU_LUT_i32_v4i32_pseudo:
  case SHAVE::CMU_LUT_i32_v2i32_pseudo:
  case SHAVE::CMU_LUT_i16_v8i16_pseudo:
  case SHAVE::CMU_LUT_i16_v4i16_pseudo:
  case SHAVE::CMU_LUT_i16_v2i16_pseudo:
  case SHAVE::CMU_LUT_i8_v16i8_pseudo:
  case SHAVE::CMU_LUT_i8_v8i8_pseudo:
  case SHAVE::CMU_LUT_i8_v4i8_pseudo:
  case SHAVE::CMU_LUT_i8_v2i8_pseudo:
  case SHAVE::CMU_LUT_f32_v4f32_pseudo:
  case SHAVE::CMU_LUT_f32_v2f32_pseudo:
  case SHAVE::CMU_LUT_f16_v8f16_pseudo:
  case SHAVE::CMU_LUT_f16_v4f16_pseudo:
  case SHAVE::CMU_LUT_f16_v2f16_pseudo:

  case SHAVE::CMU_LUTW_i32_v4i32_pseudo:
  case SHAVE::CMU_LUTW_i32_v2i32_pseudo:
  case SHAVE::CMU_LUTW_i16_v8i16_pseudo:
  case SHAVE::CMU_LUTW_i16_v4i16_pseudo:
  case SHAVE::CMU_LUTW_i8_v16i8_pseudo:
  case SHAVE::CMU_LUTW_i8_v8i8_pseudo:
  case SHAVE::CMU_LUTW_f32_v4f32_pseudo:
  case SHAVE::CMU_LUTW_f32_v2f32_pseudo:
  case SHAVE::CMU_LUTW_f16_v8f16_pseudo:
  case SHAVE::CMU_LUTW_f16_v4f16_pseudo:
    return true;

  default:
    return false;
  }
}

bool SHAVEFixLUTOperands::getLUTInfo(unsigned opcode, unsigned &laneWidth, bool &isWrite) {
  switch (opcode) {
  default:
    laneWidth = 0;
    isWrite = false;
    return false;

  case SHAVE::CMU_LUTR_v64i8_Myr4:
  case SHAVE::CMU_LUTR_v32i8_Myr4:
  case SHAVE::CMU_LUTR_v16i8_Myr4:
  case SHAVE::CMU_LUTR_v8i8_Myr4:
  case SHAVE::CMU_LUTR_v64u8_Myr4:
  case SHAVE::CMU_LUTR_v32u8_Myr4:
  case SHAVE::CMU_LUTR_v16u8_Myr4:
  case SHAVE::CMU_LUTR_v8u8_Myr4:
  case SHAVE::CMU_LUTR_v4i8_Myr4:
    laneWidth = 6;
    isWrite = false;
    break;

  case SHAVE::CMU_LUT_i8_v16i8:
  case SHAVE::CMU_LUT_i8_v8i8:
  case SHAVE::CMU_LUT_i8_v4i8:
  case SHAVE::CMU_LUT_i8_v2i8:
  case SHAVE::CMU_LUT_u8:
    laneWidth = 4;
    isWrite = false;
    break;

  case SHAVE::CMU_LUTR_v32i16_Myr4:
  case SHAVE::CMU_LUTR_v16i16_Myr4:
  case SHAVE::CMU_LUTR_v8i16_Myr4:
  case SHAVE::CMU_LUTR_v4i16_Myr4:
  case SHAVE::CMU_LUTR_v32u16_Myr4:
  case SHAVE::CMU_LUTR_v16u16_Myr4:
  case SHAVE::CMU_LUTR_v8u16_Myr4:
  case SHAVE::CMU_LUTR_v4u16_Myr4:
  case SHAVE::CMU_LUTR_v2i16_Myr4:
  case SHAVE::CMU_LUTR_v2u16_Myr4:
    laneWidth = 5;
    isWrite = false;
    break;

  case SHAVE::CMU_LUT_i16_v8i16:
  case SHAVE::CMU_LUT_i16_v4i16:
  case SHAVE::CMU_LUT_i16_v2i16:
  case SHAVE::CMU_LUT_u16_v8i16:
  case SHAVE::CMU_LUT_u16_v4i16:
  case SHAVE::CMU_LUT_u16_v2i16:
    laneWidth = 3;
    isWrite = false;
    break;

  case SHAVE::CMU_LUTR_v16i32_Myr4:
  case SHAVE::CMU_LUTR_v8i32_Myr4:
  case SHAVE::CMU_LUTR_v4i32_Myr4:
  case SHAVE::CMU_LUTR_v2i32_Myr4:
  case SHAVE::CMU_LUTR_v16u32_Myr4:
  case SHAVE::CMU_LUTR_v8u32_Myr4:
  case SHAVE::CMU_LUTR_v4u32_Myr4:
  case SHAVE::CMU_LUTR_v2u32_Myr4:
    laneWidth = 4;
    isWrite = false;
    break;

  case SHAVE::CMU_LUT_i32_v4i32:
  case SHAVE::CMU_LUT_i32_v2i32:
  case SHAVE::CMU_LUT_u32_v4i32:
  case SHAVE::CMU_LUT_u32_v2i32:
    laneWidth = 2;
    isWrite = false;
    break;

  case SHAVE::CMU_LUTW_v64i8_Myr4:
  case SHAVE::CMU_LUTW_v32i8_Myr4:
  case SHAVE::CMU_LUTW_v16i8_Myr4:
  case SHAVE::CMU_LUTW_v8i8_Myr4:
  case SHAVE::CMU_LUTW_v4i8_Myr4:
    laneWidth = 6;
    isWrite = true;
    break;

  case SHAVE::CMU_LUTW_x8_v16i8:
  case SHAVE::CMU_LUTW_x8_v8i8:
    laneWidth = 4;
    isWrite = true;
    break;

  case SHAVE::CMU_LUTW_v32i16_Myr4:
  case SHAVE::CMU_LUTW_v16i16_Myr4:
  case SHAVE::CMU_LUTW_v8i16_Myr4:
  case SHAVE::CMU_LUTW_v4i16_Myr4:
  case SHAVE::CMU_LUTW_v2i16_Myr4:
    laneWidth = 5;
    isWrite = true;
    break;

  case SHAVE::CMU_LUTW_x16_v8i16:
  case SHAVE::CMU_LUTW_x16_v4i16:
    laneWidth = 3;
    isWrite = true;
    break;

  case SHAVE::CMU_LUTW_v16i32_Myr4:
  case SHAVE::CMU_LUTW_v8i32_Myr4:
  case SHAVE::CMU_LUTW_v4i32_Myr4:
  case SHAVE::CMU_LUTW_v2i32_Myr4:
    laneWidth = 4;
    isWrite = true;
    break;

  case SHAVE::CMU_LUTW_x32_v4i32:
  case SHAVE::CMU_LUTW_x32_v2i32:
    laneWidth = 2;
    isWrite = true;
    break;

#if 0 // FIXME: Movidius - These are not included; should they be?
  case SHAVE::CMU_LUTR_x32:
  case SHAVE::CMU_LUTR_x16:
  case SHAVE::CMU_LUTR_x8:
    LaneWidth = XXX; // FIXME: Movidius - What values should we use?
    IsWrite = false;
    break;
#endif
  }

  return true;
}

// FIXME: Movidius - TODO: when we have an array that expands over 128 bits we must make some changes here
bool SHAVEFixLUTOperands::runOnMachineFunction(MachineFunction &MF) {
  // Find LUT instructions and fix their operands.
  bool changeResult = false;
  bool changeMade = true;

  while (changeMade) {
    changeMade = false;
    for (MachineFunction::iterator MBB = MF.begin(); MBB != MF.end(); ++MBB) {
      for (MachineBasicBlock::iterator index = MBB->begin(); index != MBB->end(); index++) {
        if (isLUTPseudoInstruction(index->getDesc().getOpcode(), MF)) {
          fixLUTOperands(index);
          changeResult = true;
          changeMade = true;
          break;
        }
      }
      if (changeMade)
        break;
    }
  }
  return changeResult;
}

bool SHAVEFixLUTOperands::getVRFIndex(unsigned Reg, int &VRFIdx) {
  // Determine the VRF register ID.
  // XXX This must be done already somewhere else...
  // FIXME: Movidius - This is awful
  unsigned VRFRegs[] = { SHAVE::V0,  SHAVE::V1,  SHAVE::V2,  SHAVE::V3,
                        SHAVE::V4,  SHAVE::V5,  SHAVE::V6,  SHAVE::V7,
                        SHAVE::V8,  SHAVE::V9,  SHAVE::V10, SHAVE::V11,
                        SHAVE::V12, SHAVE::V13, SHAVE::V14, SHAVE::V15,
                        SHAVE::V16, SHAVE::V17, SHAVE::V18, SHAVE::V19,
                        SHAVE::V20, SHAVE::V21, SHAVE::V22, SHAVE::V23,
                        SHAVE::V24, SHAVE::V25, SHAVE::V26, SHAVE::V27,
                        SHAVE::V28, SHAVE::V29, SHAVE::V30, SHAVE::V31,
                        0 };
  unsigned VRFRegs_l[] = { SHAVE::V0_l,  SHAVE::V1_l,  SHAVE::V2_l,  SHAVE::V3_l,
                          SHAVE::V4_l,  SHAVE::V5_l,  SHAVE::V6_l,  SHAVE::V7_l,
                          SHAVE::V8_l,  SHAVE::V9_l,  SHAVE::V10_l, SHAVE::V11_l,
                          SHAVE::V12_l, SHAVE::V13_l, SHAVE::V14_l, SHAVE::V15_l,
                          SHAVE::V16_l, SHAVE::V17_l, SHAVE::V18_l, SHAVE::V19_l,
                          SHAVE::V20_l, SHAVE::V21_l, SHAVE::V22_l, SHAVE::V23_l,
                          SHAVE::V24_l, SHAVE::V25_l, SHAVE::V26_l, SHAVE::V27_l,
                          SHAVE::V28_l, SHAVE::V29_l, SHAVE::V30_l, SHAVE::V31_l };
  unsigned VRFRegs_q[] = { SHAVE::V0_q0,  SHAVE::V1_q0,  SHAVE::V2_q0,  SHAVE::V3_q0,
                          SHAVE::V4_q0,  SHAVE::V5_q0,  SHAVE::V6_q0,  SHAVE::V7_q0,
                          SHAVE::V8_q0,  SHAVE::V9_q0,  SHAVE::V10_q0, SHAVE::V11_q0,
                          SHAVE::V12_q0, SHAVE::V13_q0, SHAVE::V14_q0, SHAVE::V15_q0,
                          SHAVE::V16_q0, SHAVE::V17_q0, SHAVE::V18_q0, SHAVE::V19_q0,
                          SHAVE::V20_q0, SHAVE::V21_q0, SHAVE::V22_q0, SHAVE::V23_q0,
                          SHAVE::V24_q0, SHAVE::V25_q0, SHAVE::V26_q0, SHAVE::V27_q0,
                          SHAVE::V28_q0, SHAVE::V29_q0, SHAVE::V30_q0, SHAVE::V31_q0 };
  unsigned VRFRegs_e[] = { SHAVE::V0_e0,  SHAVE::V1_e0,  SHAVE::V2_e0,  SHAVE::V3_e0,
                          SHAVE::V4_e0,  SHAVE::V5_e0,  SHAVE::V6_e0,  SHAVE::V7_e0,
                          SHAVE::V8_e0,  SHAVE::V9_e0,  SHAVE::V10_e0, SHAVE::V11_e0,
                          SHAVE::V12_e0, SHAVE::V13_e0, SHAVE::V14_e0, SHAVE::V15_e0,
                          SHAVE::V16_e0, SHAVE::V17_e0, SHAVE::V18_e0, SHAVE::V19_e0,
                          SHAVE::V20_e0, SHAVE::V21_e0, SHAVE::V22_e0, SHAVE::V23_e0,
                          SHAVE::V24_e0, SHAVE::V25_e0, SHAVE::V26_e0, SHAVE::V27_e0,
                          SHAVE::V28_e0, SHAVE::V29_e0, SHAVE::V30_e0, SHAVE::V31_e0 };

  for (int i = 0; VRFRegs[i]; i++) {
    if ((VRFRegs[i] == Reg) || (VRFRegs_l[i] == Reg) || (VRFRegs_q[i] == Reg) || (VRFRegs_e[i] == Reg)) {
      VRFIdx = i;
      return true;
    }
  }

  // FIXME: Movidius - This is really awful.
  //                   We could do this using getSubReg in SHAVERegisterInfo but we don't define these registers
  //                   with subregs.
  unsigned WVRFRegs[] = { SHAVE::W0,  SHAVE::W1,  SHAVE::W2,  SHAVE::W3,
                          SHAVE::W4,  SHAVE::W5,  SHAVE::W6,  SHAVE::W7,
                          SHAVE::W8,  SHAVE::W9,  SHAVE::W10, SHAVE::W11,
                          SHAVE::W12, SHAVE::W13, SHAVE::W14, SHAVE::W15,
                          SHAVE::W16, SHAVE::W17, SHAVE::W18, SHAVE::W19,
                          SHAVE::W20, SHAVE::W21, SHAVE::W22, SHAVE::W23,
                          SHAVE::W24, SHAVE::W25, SHAVE::W26, SHAVE::W27,
                          SHAVE::W28, SHAVE::W29, SHAVE::W30, SHAVE::W31,
                          0 };

  unsigned WVRF256Regs[] = { SHAVE::W0_256_0,  SHAVE::W1_256_0,  SHAVE::W2_256_0,  SHAVE::W3_256_0,
                             SHAVE::W4_256_0,  SHAVE::W5_256_0,  SHAVE::W6_256_0,  SHAVE::W7_256_0,
                             SHAVE::W8_256_0,  SHAVE::W9_256_0,  SHAVE::W10_256_0, SHAVE::W11_256_0,
                             SHAVE::W12_256_0, SHAVE::W13_256_0, SHAVE::W14_256_0, SHAVE::W15_256_0,
                             SHAVE::W16_256_0, SHAVE::W17_256_0, SHAVE::W18_256_0, SHAVE::W19_256_0,
                             SHAVE::W20_256_0, SHAVE::W21_256_0, SHAVE::W22_256_0, SHAVE::W23_256_0,
                             SHAVE::W24_256_0, SHAVE::W25_256_0, SHAVE::W26_256_0, SHAVE::W27_256_0,
                             SHAVE::W28_256_0, SHAVE::W29_256_0, SHAVE::W30_256_0, SHAVE::W31_256_0 };

  unsigned WVRF128Regs[] = { SHAVE::W0_128_0,  SHAVE::W1_128_0,  SHAVE::W2_128_0,  SHAVE::W3_128_0,
                             SHAVE::W4_128_0,  SHAVE::W5_128_0,  SHAVE::W6_128_0,  SHAVE::W7_128_0,
                             SHAVE::W8_128_0,  SHAVE::W9_128_0,  SHAVE::W10_128_0, SHAVE::W11_128_0,
                             SHAVE::W12_128_0, SHAVE::W13_128_0, SHAVE::W14_128_0, SHAVE::W15_128_0,
                             SHAVE::W16_128_0, SHAVE::W17_128_0, SHAVE::W18_128_0, SHAVE::W19_128_0,
                             SHAVE::W20_128_0, SHAVE::W21_128_0, SHAVE::W22_128_0, SHAVE::W23_128_0,
                             SHAVE::W24_128_0, SHAVE::W25_128_0, SHAVE::W26_128_0, SHAVE::W27_128_0,
                             SHAVE::W28_128_0, SHAVE::W29_128_0, SHAVE::W30_128_0, SHAVE::W31_128_0 };

  unsigned WVRF64Regs[] = { SHAVE::W0_64_0,  SHAVE::W1_64_0,  SHAVE::W2_64_0,  SHAVE::W3_64_0,
                            SHAVE::W4_64_0,  SHAVE::W5_64_0,  SHAVE::W6_64_0,  SHAVE::W7_64_0,
                            SHAVE::W8_64_0,  SHAVE::W9_64_0,  SHAVE::W10_64_0, SHAVE::W11_64_0,
                            SHAVE::W12_64_0, SHAVE::W13_64_0, SHAVE::W14_64_0, SHAVE::W15_64_0,
                            SHAVE::W16_64_0, SHAVE::W17_64_0, SHAVE::W18_64_0, SHAVE::W19_64_0,
                            SHAVE::W20_64_0, SHAVE::W21_64_0, SHAVE::W22_64_0, SHAVE::W23_64_0,
                            SHAVE::W24_64_0, SHAVE::W25_64_0, SHAVE::W26_64_0, SHAVE::W27_64_0,
                            SHAVE::W28_64_0, SHAVE::W29_64_0, SHAVE::W30_64_0, SHAVE::W31_64_0 };

  unsigned WVRF32Regs[] = { SHAVE::W0_32_0,  SHAVE::W1_32_0,  SHAVE::W2_32_0,  SHAVE::W3_32_0,
                            SHAVE::W4_32_0,  SHAVE::W5_32_0,  SHAVE::W6_32_0,  SHAVE::W7_32_0,
                            SHAVE::W8_32_0,  SHAVE::W9_32_0,  SHAVE::W10_32_0, SHAVE::W11_32_0,
                            SHAVE::W12_32_0, SHAVE::W13_32_0, SHAVE::W14_32_0, SHAVE::W15_32_0,
                            SHAVE::W16_32_0, SHAVE::W17_32_0, SHAVE::W18_32_0, SHAVE::W19_32_0,
                            SHAVE::W20_32_0, SHAVE::W21_32_0, SHAVE::W22_32_0, SHAVE::W23_32_0,
                            SHAVE::W24_32_0, SHAVE::W25_32_0, SHAVE::W26_32_0, SHAVE::W27_32_0,
                            SHAVE::W28_32_0, SHAVE::W29_32_0, SHAVE::W30_32_0, SHAVE::W31_32_0};

  unsigned WVRF16Regs[] = { SHAVE::W0_16_0,  SHAVE::W1_16_0,  SHAVE::W2_16_0,  SHAVE::W3_16_0,
                            SHAVE::W4_16_0,  SHAVE::W5_16_0,  SHAVE::W6_16_0,  SHAVE::W7_16_0,
                            SHAVE::W8_16_0,  SHAVE::W9_16_0,  SHAVE::W10_16_0, SHAVE::W11_16_0,
                            SHAVE::W12_16_0, SHAVE::W13_16_0, SHAVE::W14_16_0, SHAVE::W15_16_0,
                            SHAVE::W16_16_0, SHAVE::W17_16_0, SHAVE::W18_16_0, SHAVE::W19_16_0,
                            SHAVE::W20_16_0, SHAVE::W21_16_0, SHAVE::W22_16_0, SHAVE::W23_16_0,
                            SHAVE::W24_16_0, SHAVE::W25_16_0, SHAVE::W26_16_0, SHAVE::W27_16_0,
                            SHAVE::W28_16_0, SHAVE::W29_16_0, SHAVE::W30_16_0, SHAVE::W31_16_0};

  for (unsigned int i = 0; WVRFRegs[i]; ++i) {
    if (WVRFRegs[i] == Reg || WVRF256Regs[i] == Reg || WVRF128Regs[i] == Reg ||
        WVRF64Regs[i] == Reg || WVRF32Regs[i] == Reg || WVRF16Regs[i] == Reg) {
      VRFIdx = i;
      return true;
    }
  }

  VRFIdx = -1;
  return false;
}

unsigned SHAVEFixLUTOperands::getLUTOpcodeFromPseudo(unsigned pseudoOpcode) {
  unsigned opcode = 0;

  switch (pseudoOpcode) {
  //
  // Myriad4
  //
  // i32 vector element extract
  case SHAVE::CMU_LUT_i32_v16i32_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v16i32_Myr4; break;
  case SHAVE::CMU_LUT_i32_v8i32_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v8i32_Myr4; break;
  case SHAVE::CMU_LUT_i32_v4i32_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v4i32_Myr4; break;
  case SHAVE::CMU_LUT_i32_v2i32_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v2i32_Myr4; break;

  // i16 vector element extract
  case SHAVE::CMU_LUT_i16_v32i16_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v32i16_Myr4; break;
  case SHAVE::CMU_LUT_i16_v16i16_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v16i16_Myr4; break;
  case SHAVE::CMU_LUT_i16_v8i16_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v8i16_Myr4; break;
  case SHAVE::CMU_LUT_i16_v4i16_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v4i16_Myr4; break;
  case SHAVE::CMU_LUT_i16_v2i16_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v2i16_Myr4; break;

  // i8 vector element extract
  case SHAVE::CMU_LUT_i8_v64i8_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v64i8_Myr4; break;
  case SHAVE::CMU_LUT_i8_v32i8_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v32i8_Myr4; break;
  case SHAVE::CMU_LUT_i8_v16i8_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v16i8_Myr4; break;
  case SHAVE::CMU_LUT_i8_v8i8_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v8i8_Myr4; break;
  case SHAVE::CMU_LUT_i8_v4i8_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v4i8_Myr4; break;

  // f32 vector element extract
  case SHAVE::CMU_LUT_f32_v16f32_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v16u32_Myr4; break;
  case SHAVE::CMU_LUT_f32_v8f32_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v8u32_Myr4; break;
  case SHAVE::CMU_LUT_f32_v4f32_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v4u32_Myr4; break;
  case SHAVE::CMU_LUT_f32_v2f32_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v2u32_Myr4; break;

  // f16 vector element extract
  case SHAVE::CMU_LUT_f16_v32f16_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v32u16_Myr4; break;
  case SHAVE::CMU_LUT_f16_v16f16_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v16u16_Myr4; break;
  case SHAVE::CMU_LUT_f16_v8f16_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v8u16_Myr4; break;
  case SHAVE::CMU_LUT_f16_v4f16_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v4u16_Myr4; break;
  case SHAVE::CMU_LUT_f16_v2f16_Myr4_pseudo: opcode = SHAVE::CMU_LUTR_v2u16_Myr4; break;

  // i32 vector element insert
  case SHAVE::CMU_LUTW_i32_v16i32_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v16i32_Myr4; break;
  case SHAVE::CMU_LUTW_i32_v8i32_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v8i32_Myr4; break;
  case SHAVE::CMU_LUTW_i32_v4i32_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v4i32_Myr4; break;
  case SHAVE::CMU_LUTW_i32_v2i32_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v2i32_Myr4; break;

  // i16 vector element insert
  case SHAVE::CMU_LUTW_i16_v32i16_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v32i16_Myr4; break;
  case SHAVE::CMU_LUTW_i16_v16i16_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v16i16_Myr4; break;
  case SHAVE::CMU_LUTW_i16_v8i16_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v8i16_Myr4; break;
  case SHAVE::CMU_LUTW_i16_v4i16_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v4i16_Myr4; break;
  case SHAVE::CMU_LUTW_i16_v2i16_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v2i16_Myr4; break;

  // i8 vector element insert
  case SHAVE::CMU_LUTW_i8_v64i8_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v64i8_Myr4; break;
  case SHAVE::CMU_LUTW_i8_v32i8_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v32i8_Myr4; break;
  case SHAVE::CMU_LUTW_i8_v16i8_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v16i8_Myr4; break;
  case SHAVE::CMU_LUTW_i8_v8i8_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v8i8_Myr4; break;
  case SHAVE::CMU_LUTW_i8_v4i8_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v4i8_Myr4; break;

  // f32 vector element insert
  case SHAVE::CMU_LUTW_f32_v16f32_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v16i32_Myr4; break;
  case SHAVE::CMU_LUTW_f32_v8f32_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v8i32_Myr4; break;
  case SHAVE::CMU_LUTW_f32_v4f32_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v4i32_Myr4; break;
  case SHAVE::CMU_LUTW_f32_v2f32_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v2i32_Myr4; break;

  // f16 vector element insert
  case SHAVE::CMU_LUTW_f16_v32f16_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v32i16_Myr4; break;
  case SHAVE::CMU_LUTW_f16_v16f16_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v16i16_Myr4; break;
  case SHAVE::CMU_LUTW_f16_v8f16_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v8i16_Myr4; break;
  case SHAVE::CMU_LUTW_f16_v4f16_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v4i16_Myr4; break;
  case SHAVE::CMU_LUTW_f16_v2f16_Myr4_pseudo: opcode = SHAVE::CMU_LUTW_v2i16_Myr4; break;

  //
  // Myriad2
  //
  // i32 vector element extract
  case SHAVE::CMU_LUT_i32_v4i32_pseudo: opcode = SHAVE::CMU_LUT_i32_v4i32; break;
  case SHAVE::CMU_LUT_i32_v2i32_pseudo: opcode = SHAVE::CMU_LUT_i32_v2i32; break;

  // i16 vector element extract
  case SHAVE::CMU_LUT_i16_v8i16_pseudo: opcode = SHAVE::CMU_LUT_i16_v8i16; break;
  case SHAVE::CMU_LUT_i16_v4i16_pseudo: opcode = SHAVE::CMU_LUT_i16_v4i16; break;
  case SHAVE::CMU_LUT_i16_v2i16_pseudo: opcode = SHAVE::CMU_LUT_i16_v2i16; break;

  // i8 vector element extract
  case SHAVE::CMU_LUT_i8_v16i8_pseudo: opcode = SHAVE::CMU_LUT_i8_v16i8; break;
  case SHAVE::CMU_LUT_i8_v8i8_pseudo:  opcode = SHAVE::CMU_LUT_i8_v8i8; break;
  case SHAVE::CMU_LUT_i8_v4i8_pseudo:  opcode = SHAVE::CMU_LUT_i8_v4i8; break;
  case SHAVE::CMU_LUT_i8_v2i8_pseudo:  opcode = SHAVE::CMU_LUT_i8_v2i8; break;

  // f32 vector element extract for Myriad 2
  case SHAVE::CMU_LUT_f32_v4f32_pseudo: opcode = SHAVE::CMU_LUT_u32_v4i32; break;
  case SHAVE::CMU_LUT_f32_v2f32_pseudo: opcode = SHAVE::CMU_LUT_u32_v2i32; break;

  // f16 vector element extract for Myriad 2
  case SHAVE::CMU_LUT_f16_v8f16_pseudo: opcode = SHAVE::CMU_LUT_u16_v8i16; break;
  case SHAVE::CMU_LUT_f16_v4f16_pseudo: opcode = SHAVE::CMU_LUT_u16_v4i16; break;
  case SHAVE::CMU_LUT_f16_v2f16_pseudo: opcode = SHAVE::CMU_LUT_u16_v2i16; break;

  // i32 vector element insert
  case SHAVE::CMU_LUTW_i32_v4i32_pseudo: opcode = SHAVE::CMU_LUTW_x32_v4i32; break;
  case SHAVE::CMU_LUTW_i32_v2i32_pseudo: opcode = SHAVE::CMU_LUTW_x32_v2i32; break;

  // i16 vector element insert
  case SHAVE::CMU_LUTW_i16_v8i16_pseudo: opcode = SHAVE::CMU_LUTW_x16_v8i16; break;
  case SHAVE::CMU_LUTW_i16_v4i16_pseudo: opcode = SHAVE::CMU_LUTW_x16_v4i16; break;

  // i8 vector element insert
  case SHAVE::CMU_LUTW_i8_v16i8_pseudo: opcode = SHAVE::CMU_LUTW_x8_v16i8; break;
  case SHAVE::CMU_LUTW_i8_v8i8_pseudo: opcode = SHAVE::CMU_LUTW_x8_v8i8; break;

  // f32 vector element insert for Myriad 2
  case SHAVE::CMU_LUTW_f32_v4f32_pseudo: opcode = SHAVE::CMU_LUTW_x32_v4i32; break;
  case SHAVE::CMU_LUTW_f32_v2f32_pseudo: opcode = SHAVE::CMU_LUTW_x32_v2i32; break;

  // f16 vector element insert for Myriad 2
  case SHAVE::CMU_LUTW_f16_v8f16_pseudo: opcode = SHAVE::CMU_LUTW_x16_v8i16; break;
  case SHAVE::CMU_LUTW_f16_v4f16_pseudo: opcode = SHAVE::CMU_LUTW_x16_v4i16; break;

  default:
    llvm_unreachable("Invalid LUT psuedo opcode found during LUT operand fixing");
  }

  return opcode;
}

void SHAVEFixLUTOperands::fixLUTOperands(MachineBasicBlock::iterator I) {
  MachineInstr * MI = &*I;
  MachineBasicBlock * MBB = MI->getParent();
  DebugLoc dbgLoc = I->getDebugLoc();

  const SHAVETargetMachine &TM = static_cast<const SHAVETargetMachine &>(MBB->getParent()->getTarget());
  const SHAVEInstrInfo * TII = static_cast<const SHAVEInstrInfo *>(TM.getSubtargetImpl()->getInstrInfo());

  unsigned opcode = getLUTOpcodeFromPseudo(MI->getOpcode());

  unsigned laneBits = 0;
  bool isWrite = false;

  if (getLUTInfo(opcode, laneBits, isWrite)) {
    int vecNum = -1;
    unsigned vecReg = MI->getOperand(2).getReg();

    if (getVRFIndex(vecReg, vecNum)) {
      unsigned srcConst = vecNum << laneBits;
      unsigned laneOpIdx = isWrite ? 4 : 3;
      unsigned laneReg = MI->getOperand(laneOpIdx).getReg();
      unsigned tempReg = MI->getOperand(laneOpIdx+1).getReg();
      unsigned dstReg = MI->getOperand(0).getReg();

      // If (vecReg << laneBits) + laneReg fits in the immediate field of IAU.ADD.I32
      if (srcConst < 256) {
        TII->finaliseMI(BuildMI(*MBB, I, dbgLoc, TII->get(SHAVE::IAU_ADD_32_imm), tempReg).addReg(laneReg).addImm(srcConst));
      }
      else {
        TII->finaliseMI(BuildMI(*MBB, I, dbgLoc, TII->get(SHAVE::LSU_LDIL), tempReg).addImm(srcConst));
        TII->finaliseMI(BuildMI(*MBB, I, dbgLoc, TII->get(SHAVE::IAU_OR_32), tempReg).addReg(tempReg).addReg(laneReg));
      }

      if (isWrite)
        TII->finaliseMI(BuildMI(*MBB, I, dbgLoc, TII->get(opcode), dstReg).addReg(vecReg).addReg(MI->getOperand(3).getReg()).addReg(tempReg));
      else
        TII->finaliseMI(BuildMI(*MBB, I, dbgLoc, TII->get(opcode), dstReg).addReg(vecReg).addReg(tempReg));

      MBB->erase(MI);
    }
    else {
      llvm_unreachable("Invalid VRF index found during LUT operand fixing");
    }
  }
  else {
    llvm_unreachable("Unsupported LUT opcode found during LUT operand fixing");
  }
}
