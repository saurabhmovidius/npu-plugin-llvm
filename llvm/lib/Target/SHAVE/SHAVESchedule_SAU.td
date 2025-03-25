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
// File       : SHAVESchedule_SAU.td
// Description: Tablegen classes for scheduling information
// ---------------------------------------------------------------------------
// ***************************************************************************

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_SAU_IIA : SHAVEWriteOp<NumLatency, PR_IRF_R2>;
  def ""#NumLatency#_LATENCY_WITH_SAU_IIB : SHAVEWriteOp<NumLatency, PR_IRF_R3>;
  def ""#NumLatency#_LATENCY_WITH_SAU_IOA : SHAVEWriteOp<NumLatency, PR_IRF_W1>;
}

def II_SAU_ADD_SUB_IRF_WITH_WRITE_2_CONDITION : InstrItinClass;
def II_SAU_SHR_IRF_WITH_WRITE_1_CONDITION : InstrItinClass;
def II_SAU_MUL_IRF_WITH_WRITE_2_CONDITION : InstrItinClass;
def II_SAU_DIV16_IRF_WITH_WRITE_2_CONDITION : InstrItinClass;
def II_SAU_DIV32_IRF_WITH_WRITE_2_CONDITION : InstrItinClass;
def II_SAU_MOD16_IRF_WITH_WRITE_2_CONDITION : InstrItinClass;
def II_SAU_MOD32_IRF_WITH_WRITE_2_CONDITION : InstrItinClass;
def II_SAU_ADD_SUB_MUL_IRF_f16_f32_WITH_WRITE_2_CONDITION : InstrItinClass;
def II_SAU_ADD_SUB_MUL_IRF_f16_f32_IMM_WITH_WRITE_2_CONDITION : InstrItinClass;
def II_SAU_DIV_f16 : InstrItinClass;
def II_SAU_DIV_f32 : InstrItinClass;
def II_SAU_FRAC_IRF : InstrItinClass;
def II_SAU_ABS_IRF : InstrItinClass;
def II_SAU_MATH_IRF : InstrItinClass;
def II_SAU_MATH_VRF : InstrItinClass;
def II_SAU_SWZ_IRF : InstrItinClass;
def II_SAU_OR_AND_IRF_VRF : InstrItinClass;
def II_SAU_SHL_RRR_IRF : InstrItinClass;
def II_SAU_SHL_RRI_IRF : InstrItinClass;
def II_SAU_SHR_RRR_IRF : InstrItinClass;
def II_SAU_SHR_RRI_IRF : InstrItinClass;
def II_SAU_SUM_IRF_VRF_INT : InstrItinClass;
def II_SAU_SUM_IRF_IRF_INT : InstrItinClass;
def II_SAU_SUM_IRF_VRF_FP : InstrItinClass;
def II_SAU_SUM_IRF_IRF_FP : InstrItinClass;
def II_SAU_ADIFF_IRF : InstrItinClass;
def II_SAU_ADD_RRR_IRF_WITH_1_CONDITION : InstrItinClass;
def II_SAU_MUL_RRR_IRF_WITH_1_CONDITION : InstrItinClass;
def II_SAU_ILMULH_RRR_IRF_WITH_1_CONDITION : InstrItinClass;
def II_SAU_ONES_ZEROS_IRF : InstrItinClass;
def II_SAU_ONES_ZEROS_VRF : InstrItinClass;
def II_SAU_CMB_IRF : InstrItinClass;
def II_SAU_ADDCI_SUBCI_IRF : InstrItinClass;
def II_SAU_ALIGNBYTE_IRF : InstrItinClass;
def II_SAU_AVG_IRF : InstrItinClass;
def II_SAU_NOT_IRF : InstrItinClass;
def II_SAU_SUMC_IRF_VRF : InstrItinClass;

def II_SAU_ACC_INT : InstrItinClass;
def II_SAU_ACCW_INT : InstrItinClass;
def II_SAU_ACC_FP_NonPipe : InstrItinClass;
def II_SAU_ACCW_FP_NonPipe : InstrItinClass;
def II_SAU_ACC_FP_Pipe : InstrItinClass;
def II_SAU_ACCW_FP_Pipe : InstrItinClass;

def II_SAU_MAC_INT : InstrItinClass;
def II_SAU_MACW_INT : InstrItinClass;
def II_SAU_MAC_FP_NonPipe : InstrItinClass;
def II_SAU_MACW_FP_NonPipe : InstrItinClass;
def II_SAU_MAC_FP_Pipe : InstrItinClass;
def II_SAU_MACW_FP_Pipe : InstrItinClass;

def II_SAU_AND_OR_XOR_IRF : InstrItinClass;

def II_SAU_DP4_NONACC_Myr2v6 : InstrItinClass;
def II_SAU_DP4_ACC_Myr2v6 : InstrItinClass;

let SchedModel = SHAVEModel in {

//For II_SAU_ADD_SUB_IRF_WITH_WRITE_2_CONDITION
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             1_LATENCY_WITH_WRITE_S_STATE,
             1_LATENCY_WITH_WRITE_C_CSI,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ADD_SUB_IRF_WITH_WRITE_2_CONDITION]>;

//For II_SAU_SHR_IRF_WITH_WRITE_1_CONDITION
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             1_LATENCY_WITH_WRITE_S_STATE],
            [II_SAU_SHR_IRF_WITH_WRITE_1_CONDITION]>;

//For II_SAU_MUL_IRF_WITH_WRITE_2_CONDITION
def :ItinRW<[2_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             2_LATENCY_WITH_WRITE_S_STATE,
             2_LATENCY_WITH_WRITE_C_CSI,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_MUL_IRF_WITH_WRITE_2_CONDITION]>;

//For II_SAU_DIV16_IRF_WITH_WRITE_2_CONDITION
def :ItinRW<[8_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             8_LATENCY_WITH_WRITE_I_AE,
             8_LATENCY_WITH_WRITE_C_CSI],
            [II_SAU_DIV16_IRF_WITH_WRITE_2_CONDITION]>;

//For II_SAU_DIV32_IRF_WITH_WRITE_2_CONDITION
def :ItinRW<[13_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             13_LATENCY_WITH_WRITE_I_AE,
             13_LATENCY_WITH_WRITE_C_CSI],
            [II_SAU_DIV32_IRF_WITH_WRITE_2_CONDITION]>;

//For II_SAU_MOD16_IRF_WITH_WRITE_2_CONDITION
def :ItinRW<[9_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             9_LATENCY_WITH_WRITE_I_AE,
             9_LATENCY_WITH_WRITE_C_CSI],
            [II_SAU_MOD16_IRF_WITH_WRITE_2_CONDITION]>;

//For II_SAU_MOD32_IRF_WITH_WRITE_2_CONDITION
def :ItinRW<[14_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             14_LATENCY_WITH_WRITE_I_AE,
             14_LATENCY_WITH_WRITE_C_CSI],
            [II_SAU_MOD32_IRF_WITH_WRITE_2_CONDITION]>;

//For II_SAU_ADD_SUB_MUL_IRF_f16_f32_WITH_WRITE_2_CONDITION
def :ItinRW<[2_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             2_LATENCY_WITH_WRITE_I_AE,
             2_LATENCY_WITH_WRITE_C_CSI,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ADD_SUB_MUL_IRF_f16_f32_WITH_WRITE_2_CONDITION]>;

//For II_SAU_ADD_SUB_MUL_IRF_f16_f32_IMM_WITH_WRITE_2_CONDITION
def :ItinRW<[2_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             2_LATENCY_WITH_WRITE_I_AE,
             2_LATENCY_WITH_WRITE_C_CSI,
             0_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ADD_SUB_MUL_IRF_f16_f32_IMM_WITH_WRITE_2_CONDITION]>;

//For II_SAU_DIV_f16
def :ItinRW<[7_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             7_LATENCY_WITH_WRITE_C_CSI],
            [II_SAU_DIV_f16]>;

//For II_SAU_DIV_f32
def :ItinRW<[11_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             11_LATENCY_WITH_WRITE_C_CSI],
            [II_SAU_DIV_f32]>;

//For II_SAU_FRAC_IRF
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA],
            [II_SAU_FRAC_IRF]>;

//For II_SAU_ABS_IRF
def :ItinRW<[0_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SHARED_RES_S5,
             0_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ABS_IRF]>;

//For II_SAU_MATH_IRF
def :ItinRW<[4_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA],
            [II_SAU_MATH_IRF]>;

//For II_SAU_MATH_VRF
def :ItinRW<[4_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_VRF_R2],
            [II_SAU_MATH_VRF]>;      

//For II_SAU_SWZ_IRF
def :ItinRW<[0_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SHARED_RES_S5,
             0_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_SWZ_IRF]>;

//For II_SAU_OR_AND_IRF_VRF
def :ItinRW<[0_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_VRF_R2,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_SAU_OR_AND_IRF_VRF]>;

//For II_SAU_SHL_RRR_IRF
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             1_LATENCY_WITH_WRITE_C_CSI,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_SHL_RRR_IRF]>;

//For II_SAU_SHL_RRI_IRF
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             1_LATENCY_WITH_WRITE_C_CSI,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_SHL_RRI_IRF]>;

//For II_SAU_SHR_RRR_IRF
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             1_LATENCY_WITH_WRITE_S_STATE,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_SHR_RRR_IRF]>;

//For II_SAU_SHR_RRI_IRF
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             1_LATENCY_WITH_WRITE_S_STATE,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_SHR_RRI_IRF]>;

//For II_SAU_SUM_IRF_VRF_FP
def :ItinRW<[4_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_VRF_R2],
            [II_SAU_SUM_IRF_VRF_FP]>;

//For II_SAU_SUM_IRF_IRF_FP
def :ItinRW<[4_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA],
            [II_SAU_SUM_IRF_IRF_FP]>;

//For II_SAU_SUM_IRF_VRF_INT
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_VRF_R2],
            [II_SAU_SUM_IRF_VRF_INT]>;

//For II_SAU_SUM_IRF_IRF_INT
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA],
            [II_SAU_SUM_IRF_IRF_INT]>;

//For II_SAU_ADIFF_IRF
def :ItinRW<[2_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             2_LATENCY_WITH_WRITE_C_CSI,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ADIFF_IRF]>;

//For II_SAU_ADD_RRR_IRF_WITH_1_CONDITION
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             1_LATENCY_WITH_WRITE_C_CSI,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ADD_RRR_IRF_WITH_1_CONDITION]>;

//For II_SAU_MUL_RRR_IRF_WITH_1_CONDITION
def :ItinRW<[2_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             1_LATENCY_WITH_WRITE_C_CSI,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_MUL_RRR_IRF_WITH_1_CONDITION]>;

//For II_SAU_ILMULH_RRR_IRF_WITH_1_CONDITION
def :ItinRW<[2_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             2_LATENCY_WITH_WRITE_C_CSI,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ILMULH_RRR_IRF_WITH_1_CONDITION]>;

//For II_SAU_ONES_ZEROS_IRF
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA],
            [II_SAU_ONES_ZEROS_IRF]>;

//For II_SAU_ONES_ZEROS_VRF
def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_VRF_R2],
            [II_SAU_ONES_ZEROS_VRF]>;

// For II_SAU_CMB_IRF
def :ItinRW<[0_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             0_LATENCY_WITH_SHARED_RES_S5,
             0_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_CMB_IRF]>;

// Integer accumulation
def :ItinRW<[0_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_READ_S_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S1,
             0_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_SAU_ACC_INT]>;

def :ItinRW<[1_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_READ_S_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S1,
             0_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ACCW_INT]>;

// Floating-point accumulation
def :ItinRW<[2_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_READ_S_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S1,
             2_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_SAU_ACC_FP_NonPipe]>;

def :ItinRW<[2_LATENCY_WITH_SAU_IOA,
             2_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_READ_S_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S1,
             2_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ACCW_FP_NonPipe]>;

def :ItinRW<[2_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_READ_S_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S1,
             2_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_SAU_ACC_FP_Pipe]>;

def :ItinRW<[2_LATENCY_WITH_SAU_IOA,
             2_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_READ_S_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S1,
             2_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ACCW_FP_Pipe]>;

// Integer MAC accumulation
// MAC operands - { accreg, in1, in2, accreg }
// MACW operands - { out1, accreg, in1, in2, accreg }

//def : ReadAdvance < 0_LATENCY_WITH_READ_S_ACC, 2,
//  [ 2_LATENCY_WITH_WRITE_S_ACC]>;

def :ItinRW<[0_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             0_LATENCY_WITH_READ_S_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S1,
             2_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_SAU_MAC_INT]>;

def :ItinRW<[3_LATENCY_WITH_SAU_IOA,
             2_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             0_LATENCY_WITH_READ_S_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S1,
             2_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5,
             3_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_MACW_INT]>;

// Floating-point MAC accumulation
def :ItinRW<[4_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             0_LATENCY_WITH_READ_S_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             4_LATENCY_WITH_SHARED_RES_S1,
             4_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_SAU_MAC_FP_NonPipe]>;

def :ItinRW<[4_LATENCY_WITH_SAU_IOA,
             4_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             0_LATENCY_WITH_READ_S_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             4_LATENCY_WITH_SHARED_RES_S1,
             4_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5,
             4_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_MACW_FP_NonPipe]>;

def :ItinRW<[2_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             0_LATENCY_WITH_READ_S_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             4_LATENCY_WITH_SHARED_RES_S1,
             4_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_SAU_MAC_FP_Pipe]>;

def :ItinRW<[4_LATENCY_WITH_SAU_IOA,
             4_LATENCY_WITH_WRITE_S_ACC,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             0_LATENCY_WITH_READ_S_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             4_LATENCY_WITH_SHARED_RES_S1,
             4_LATENCY_WITH_SHARED_RES_S2,
             0_LATENCY_WITH_SHARED_RES_S5,
             4_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_MACW_FP_Pipe]>;

// For II_SAU_AND_OR_XOR_IRF
def :ItinRW<[0_LATENCY_WITH_IRF_W1,
             0_LATENCY_WITH_IRF_R2,
             0_LATENCY_WITH_IRF_R3,
             0_LATENCY_WITH_SHARED_RES_S5,
             0_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_AND_OR_XOR_IRF]>;

// For II_SAU_ADDCI_SUBCI_IRF
def :ItinRW<[0_LATENCY_WITH_IRF_R2,
             0_LATENCY_WITH_IRF_R3,
             1_LATENCY_WITH_IRF_W1,
             1_LATENCY_WITH_WRITE_S_STATE,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ADDCI_SUBCI_IRF]>;

// For II_SAU_ALIGNBYTE_IRF
def :ItinRW<[0_LATENCY_WITH_IRF_R2,
             0_LATENCY_WITH_IRF_R3,
             0_LATENCY_WITH_IRF_W1,
             0_LATENCY_WITH_SHARED_RES_S5,
             0_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_ALIGNBYTE_IRF]>;

// For II_SAU_AVG_IRF
def :ItinRW<[0_LATENCY_WITH_IRF_R2,
             0_LATENCY_WITH_IRF_R3,
             1_LATENCY_WITH_IRF_W1,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_AVG_IRF]>;

// For II_SAU_NOT_IRF
def :ItinRW<[0_LATENCY_WITH_IRF_R2,
             0_LATENCY_WITH_IRF_W1,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_SAU_NOT_IRF]>;

// For II_SAU_SUMC_IRF_VRF
def :ItinRW<[0_LATENCY_WITH_VRF_R2,
             1_LATENCY_WITH_IRF_W1],
            [II_SAU_SUMC_IRF_VRF]>;

// For II_SAU_BSF_IRF
// def :ItinRW<[1_LATENCY_WITH_IRF_W1,
//              0_LATENCY_WITH_IRF_R2],
//             [II_SAU_BSF_IRF]>;

def :ItinRW<[2_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_DP4_NONACC_Myr2v6]>;

def :ItinRW<[3_LATENCY_WITH_SAU_IOA,
             0_LATENCY_WITH_SAU_IIA,
             0_LATENCY_WITH_SAU_IIB,
             2_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             3_LATENCY_WITH_SHARED_RES_S9],
            [II_SAU_DP4_ACC_Myr2v6]>;

} // SHAVEModel
