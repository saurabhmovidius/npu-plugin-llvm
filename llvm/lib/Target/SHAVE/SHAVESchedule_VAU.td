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
// File       : SHAVESchedule_VAU.td
// Description: Tablegen classes for scheduling information
// ---------------------------------------------------------------------------
// ***************************************************************************

def II_VAU_ADD_SUB_RRR_VRF : InstrItinClass;
def II_VAU_ADD_SUB_RRI_VRF : InstrItinClass;
def II_VAU_ADD_SUB_MUL_RRR_VRF : InstrItinClass;
def II_VAU_ADD_SUB_MUL_RRI_VRF : InstrItinClass;
def II_VAU_SHL_RRR_VRF : InstrItinClass;
def II_VAU_SHL_RRI_VRF : InstrItinClass;
def II_VAU_SHR_RRR_VRF : InstrItinClass;
def II_VAU_SHR_RRI_VRF : InstrItinClass;
def II_VAU_AND_OR_XOR_VRF : InstrItinClass;
def II_VAU_NOT_VRF : InstrItinClass;
def II_VAU_SWZ_VRF : InstrItinClass;
def II_VAU_ISA26_TRANSCENDENTALS_VRF : InstrItinClass;
def II_VAU_ALIGN_VRF : InstrItinClass;
def II_VAU_CMB_VRF : InstrItinClass;
def II_VAU_ADIFF_VRF_FLOAT : InstrItinClass;
def II_VAU_ADIFF_VRF : InstrItinClass;
def II_VAU_ADD_SUB_RRR_VRF_WITH_NO_CONDITION : InstrItinClass;
def II_VAU_MUL_RRR_VRF_WITH_NO_CONDITION : InstrItinClass;
def II_VAU_ADD_SUB_RRI_VRF_WITH_NO_CONDITION : InstrItinClass;
def II_VAU_MUL_RRI_VRF_WITH_NO_CONDITION : InstrItinClass;
def II_VAU_AVG_VRF : InstrItinClass;
def II_VAU_ILMULH_VRF : InstrItinClass;
def II_VAU_REGX_VRF : InstrItinClass;
def II_VAU_GMUL_VRF : InstrItinClass;
def II_VAU_ABS_VRF : InstrItinClass;
def II_VAU_BSF_VRF : InstrItinClass;
def II_VAU_FRAC_VRF : InstrItinClass;
def II_VAU_XORT_VRF : InstrItinClass;
def II_VAU_RVRS_VRF : InstrItinClass;
def II_VAU_SHF_DEA_VRF : InstrItinClass;
def II_VAU_ADDCI_SUBCI_VRF : InstrItinClass;

def II_VAU_MAC_INT : InstrItinClass;
def II_VAU_MACW_INT : InstrItinClass;
def II_VAU_MAC_FP_NonPipe : InstrItinClass;
def II_VAU_MACW_FP_NonPipe : InstrItinClass;
def II_VAU_MAC_FP_Pipe : InstrItinClass;
def II_VAU_MACW_FP_Pipe : InstrItinClass;

def II_VAU_ACC_INT : InstrItinClass;
def II_VAU_ACCW_INT : InstrItinClass;
def II_VAU_ACC_FP_NonPipe : InstrItinClass;
def II_VAU_ACCW_FP_NonPipe : InstrItinClass;
def II_VAU_ACC_FP_Pipe : InstrItinClass;
def II_VAU_ACCW_FP_Pipe : InstrItinClass;

def II_VAU_DP4_NONACC_Myr2v6 : InstrItinClass;
def II_VAU_DP4_ACC_Myr2v6 : InstrItinClass;

let SchedModel = SHAVEModel in {

// For II_VAU_ADD_SUB_RRR_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             1_LATENCY_WITH_WRITE_I_AE,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ADD_SUB_RRR_VRF]>;

// For II_VAU_ADD_SUB_RRI_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             1_LATENCY_WITH_WRITE_I_AE,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ADD_SUB_RRI_VRF]>;

// For II_VAU_ADD_SUB_MUL_RRR_VRF
def :ItinRW<[2_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             2_LATENCY_WITH_WRITE_I_AE,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ADD_SUB_MUL_RRR_VRF]>;

// For II_VAU_ADD_SUB_MUL_RRI_VRF
def :ItinRW<[2_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             2_LATENCY_WITH_WRITE_I_AE,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ADD_SUB_MUL_RRI_VRF]>;

// For II_VAU_SHL_RRR_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             1_LATENCY_WITH_WRITE_V_STATE,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_SHL_RRR_VRF]>;

// For II_VAU_SHL_RRI_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             1_LATENCY_WITH_WRITE_V_STATE,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_SHL_RRI_VRF]>;

// For II_VAU_SHR_RRR_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             1_LATENCY_WITH_WRITE_V_STATE,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_SHR_RRR_VRF]>;

// For II_VAU_SHR_RRI_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             1_LATENCY_WITH_WRITE_V_STATE,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_SHR_RRI_VRF]>;

// For II_VAU_AND_OR_XOR_VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_SHARED_RES_S5,
             0_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_AND_OR_XOR_VRF]>;

// For II_VAU_NOT_VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_SHARED_RES_S5,
             0_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_NOT_VRF]>;

// For II_VAU_ISA26_TRANSCENDENTALS_VRF
def :ItinRW<[4_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_SHARED_RES_S5,
             4_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ISA26_TRANSCENDENTALS_VRF]>;

// For II_VAU_SWZ_VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0],
            [II_VAU_SWZ_VRF]>;

// For II_VAU_ALIGN_VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_SHARED_RES_S5,
             0_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ALIGN_VRF]>;

// For II_VAU_CMB_VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1],
            [II_VAU_CMB_VRF]>;

// For II_VAU_ADIFF_VRF_FLOAT
def :ItinRW<[2_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ADIFF_VRF_FLOAT]>;

// For II_VAU_ADIFF_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ADIFF_VRF]>;

// For II_VAU_ADD_SUB_RRR_VRF_WITH_NO_CONDITION
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ADD_SUB_RRR_VRF_WITH_NO_CONDITION]>;

// For II_VAU_MUL_RRR_VRF_WITH_NO_CONDITION
def :ItinRW<[2_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_MUL_RRR_VRF_WITH_NO_CONDITION]>;

// For II_VAU_ADD_SUB_RRI_VRF_WITH_NO_CONDITION
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ADD_SUB_RRI_VRF_WITH_NO_CONDITION]>;

// For II_VAU_MUL_RRI_VRF_WITH_NO_CONDITION
def :ItinRW<[2_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_MUL_RRI_VRF_WITH_NO_CONDITION]>;

// For II_VAU_AVG_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_AVG_VRF]>;

// For II_VAU_ILMULH_VRF
def :ItinRW<[2_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ILMULH_VRF]>;

// For II_VAU_REGX_VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1],
            [II_VAU_REGX_VRF]>;

// For II_VAU_GMUL_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1],
            [II_VAU_GMUL_VRF]>;

// For II_VAU_ABS_VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_SHARED_RES_S5,
             0_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ABS_VRF]>;

// For II_VAU_BSF_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0],
            [II_VAU_BSF_VRF]>;

// For II_VAU_FRAC_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0],
            [II_VAU_FRAC_VRF]>;

// For II_VAU_XORT_VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_WRITE_V_STATE],
            [II_VAU_XORT_VRF]>;

// For II_VAU_RVRS_VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0],
            [II_VAU_RVRS_VRF]>;

// For II_VAU_SHF_DEA_VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0],
            [II_VAU_SHF_DEA_VRF]>;

// For II_VAU_ADDCI_SUBCI_VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             1_LATENCY_WITH_WRITE_V_STATE,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ADDCI_SUBCI_VRF]>;

// Integer MAC accumulation
// MAC operands - { accreg, in1, in2, accreg }
// MACW operands - { out1, accreg, in1, in2, accreg }

//def : ReadAdvance < 0_LATENCY_WITH_READ_S_ACC, 2,
//  [ 2_LATENCY_WITH_WRITE_V_ACC]>;

def :ItinRW<[0_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_READ_V_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S3,
             2_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_VAU_MAC_INT]>;

def :ItinRW<[3_LATENCY_WITH_VRF_W0,
             2_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_READ_V_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S3,
             2_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5,
             3_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_MACW_INT]>;

// Floating-point MAC accumulation
def :ItinRW<[4_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_READ_V_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             4_LATENCY_WITH_SHARED_RES_S3,
             4_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_VAU_MAC_FP_NonPipe]>;

def :ItinRW<[4_LATENCY_WITH_VRF_W0,
             4_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_READ_V_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             4_LATENCY_WITH_SHARED_RES_S3,
             4_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5,
             4_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_MACW_FP_NonPipe]>;

def :ItinRW<[2_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_READ_V_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             4_LATENCY_WITH_SHARED_RES_S3,
             4_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5,
             4_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_MAC_FP_Pipe]>;

def :ItinRW<[4_LATENCY_WITH_VRF_W0,
             2_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_READ_V_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             4_LATENCY_WITH_SHARED_RES_S3,
             4_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5,
             4_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_MACW_FP_Pipe]>;


// Integer accumulation
def :ItinRW<[0_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_READ_V_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S3,
             0_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_VAU_ACC_INT]>;

def :ItinRW<[1_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_READ_V_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S3,
             0_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5,
             1_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ACCW_INT]>;

// Floating-point accumulation
def :ItinRW<[2_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_READ_V_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S3,
             2_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_VAU_ACC_FP_NonPipe]>;

def :ItinRW<[2_LATENCY_WITH_VRF_W0,
             2_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_READ_V_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             0_LATENCY_WITH_SHARED_RES_S3,
             0_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ACCW_FP_NonPipe]>;

def :ItinRW<[2_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_READ_V_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S3,
             2_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5],
            [II_VAU_ACC_FP_Pipe]>;

def :ItinRW<[2_LATENCY_WITH_VRF_W0,
             2_LATENCY_WITH_WRITE_V_ACC,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_READ_V_ACC,
             0_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S3,
             2_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_ACCW_FP_Pipe]>;

def :ItinRW<[2_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             0_LATENCY_WITH_SHARED_RES_S5,
             2_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_DP4_NONACC_Myr2v6]>;

def :ItinRW<[3_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1,
             2_LATENCY_WITH_WRITE_V_ACC,
             2_LATENCY_WITH_SHARED_RES_S0,
             2_LATENCY_WITH_SHARED_RES_S3,
             2_LATENCY_WITH_SHARED_RES_S4,
             0_LATENCY_WITH_SHARED_RES_S5,
             3_LATENCY_WITH_SHARED_RES_S9],
            [II_VAU_DP4_ACC_Myr2v6]>;

} // Shavemodel
