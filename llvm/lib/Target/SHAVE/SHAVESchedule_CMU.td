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
// File       : SHAVESchedule_CMU.td
// Description: Tablegen classes for scheduling information
// ---------------------------------------------------------------------------
// ***************************************************************************

def II_CMU_CMII_COMPARISON_WITH_WRITE_1_CONDITION : InstrItinClass;
def II_CMU_CMII_COMPARISON_WITH_WRITE_2_CONDITION : InstrItinClass;
def II_CMU_CMII_COMPARISON_WITH_WRITE_4_CONDITION : InstrItinClass;
def II_CMU_CMVV_COMPARISON_WITH_WRITE_8_CONDITION : InstrItinClass;
def II_CMU_CMVV_COMPARISON_WITH_WRITE_16_CONDITION : InstrItinClass;
def II_CMU_CMZ_COMPARISON_IRF_WITH_WRITE_1_CONDITION : InstrItinClass;
def II_CMU_CMZ_COMPARISON_IRF_WITH_WRITE_2_CONDITION : InstrItinClass;
def II_CMU_CMZ_COMPARISON_IRF_WITH_WRITE_4_CONDITION : InstrItinClass;
def II_CMU_CMZ_COMPARISON_VRF_WITH_WRITE_16_CONDITION : InstrItinClass;
def II_CMU_CMTI_TRF_IRF : InstrItinClass;
def II_CMU_RRR_IRF_MIN_MAX_Myr2v3 : InstrItinClass;
def II_CMU_RRR_VRF_MIN_MAX_Myr2v3 : InstrItinClass;
def II_CMU_VSZM_VRF : InstrItinClass;
def II_CMU_CPII_IRF : InstrItinClass;
def II_CMU_CPII_IRF64 : InstrItinClass;
def II_CMU_CPII_IRF_FLOAT_INT : InstrItinClass;
def II_CMU_CPTI_IRF : InstrItinClass;
def II_CMU_CPIT_IRF : InstrItinClass;
def II_CMU_CPVV_VRF : InstrItinClass;
def II_CMU_CPVV_VRF_CONV : InstrItinClass;
def II_CMU_CPVI_IRF_VRF : InstrItinClass;
def II_CMU_CPVI_IRF64_VRF : InstrItinClass;
def II_CMU_CPVI_IRF_VRF_FCONV : InstrItinClass;
def II_CMU_CPEI_VRF_VRF : InstrItinClass;
def II_CMU_CPEI_IRF_VRF : InstrItinClass;
def II_CMU_CPEI_VRF_IRF : InstrItinClass;
def II_CMU_LUT_IRF : InstrItinClass;
def II_CMU_LUTW_IRF : InstrItinClass;
def II_CMU_CPIV_VRF_IRF : InstrItinClass;
def II_CMU_CPIV_VRF_IRF64 : InstrItinClass;
def II_CMU_CPIV_VRF_IRF_FCONV : InstrItinClass;
def II_CMU_CPZV_VRF : InstrItinClass;
def II_CMU_CPZI_IRF : InstrItinClass;
def II_CMU_CPZT_TRF : InstrItinClass;
def II_CMU_VILV_VRF : InstrItinClass;
def II_CMU_SHLIV : InstrItinClass;
def II_CMU_SHLSV : InstrItinClass;
def II_CMU_VNZ : InstrItinClass;
def II_CMU_VSIGNF_VRF : InstrItinClass;
def II_CMU_VSIGNF_IRF : InstrItinClass;
def II_CMU_CLAMPAB_VRF : InstrItinClass;
def II_CMU_CLAMPAB_IRF : InstrItinClass;
def II_CMU_CLAMP_IRF : InstrItinClass;
def II_CMU_CLAMP_VRF : InstrItinClass;
def II_CMU_CPVCR_VRF : InstrItinClass;
def II_CMU_CPVRC_VRF : InstrItinClass;
def II_CMU_ALIGN_VRF : InstrItinClass;
def II_CMU_SWZ_VRF : InstrItinClass;
def II_CMU_CMASK : InstrItinClass;
def II_CMU_VSZM_IRF : InstrItinClass;
def II_CMU_EXPAND : InstrItinClass;
def II_CMU_EXPAND_X8 : InstrItinClass;
def II_CMU_BIT : InstrItinClass;
def II_CMU_COMPRESS : InstrItinClass;
def II_CMU_COMPRESS_X8 : InstrItinClass;
def II_CMU_PERM : InstrItinClass;
def II_CMU_PERMB : InstrItinClass;

let SchedModel = SHAVEModel in {

// For CMU.CMII comparision with 1 condition register
def :ItinRW<[0_LATENCY_WITH_IRF_R7, 0_LATENCY_WITH_IRF_R6,
             0_LATENCY_WITH_WRITE_CC_CMU0,
             0_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_CMII_COMPARISON_WITH_WRITE_1_CONDITION]>;

// For CMU.CMII comparision with 2 condition register
def :ItinRW<[0_LATENCY_WITH_IRF_R7, 0_LATENCY_WITH_IRF_R6,
             0_LATENCY_WITH_WRITE_CC_CMU0,
             0_LATENCY_WITH_WRITE_CC_CMU1,
             0_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_CMII_COMPARISON_WITH_WRITE_2_CONDITION]>;

// For CMU.CMII comparision with 4 condition register
def :ItinRW<[0_LATENCY_WITH_IRF_R7, 0_LATENCY_WITH_IRF_R6,
             0_LATENCY_WITH_WRITE_CC_CMU0,
             0_LATENCY_WITH_WRITE_CC_CMU1,
             0_LATENCY_WITH_WRITE_CC_CMU2,
             0_LATENCY_WITH_WRITE_CC_CMU3,
             0_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_CMII_COMPARISON_WITH_WRITE_4_CONDITION]>;

// For CMU.CMTI TRF-IRF comparison
def :ItinRW<[0_LATENCY_WITH_IRF_R7,
             0_LATENCY_NOPORT,
             0_LATENCY_WITH_WRITE_CC_CMU0,
             0_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_CMTI_TRF_IRF]>;

// For CMU.CMVV comparision with 8 condition register
def :ItinRW<[0_LATENCY_WITH_VRF_R3, 0_LATENCY_WITH_VRF_R2,
             0_LATENCY_WITH_WRITE_CC_CMU0,
             0_LATENCY_WITH_WRITE_CC_CMU1,
             0_LATENCY_WITH_WRITE_CC_CMU2,
             0_LATENCY_WITH_WRITE_CC_CMU3,
             0_LATENCY_WITH_WRITE_CC_CMU4,
             0_LATENCY_WITH_WRITE_CC_CMU5,
             0_LATENCY_WITH_WRITE_CC_CMU6,
             0_LATENCY_WITH_WRITE_CC_CMU7,
             0_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_CMVV_COMPARISON_WITH_WRITE_8_CONDITION]>;

// For CMU.CMVV comparision with 16 condition register
def :ItinRW<[0_LATENCY_WITH_VRF_R3, 0_LATENCY_WITH_VRF_R2,
             0_LATENCY_WITH_WRITE_CC_CMU0,
             0_LATENCY_WITH_WRITE_CC_CMU1,
             0_LATENCY_WITH_WRITE_CC_CMU2,
             0_LATENCY_WITH_WRITE_CC_CMU3,
             0_LATENCY_WITH_WRITE_CC_CMU4,
             0_LATENCY_WITH_WRITE_CC_CMU5,
             0_LATENCY_WITH_WRITE_CC_CMU6,
             0_LATENCY_WITH_WRITE_CC_CMU7,
             0_LATENCY_WITH_WRITE_CC_CMU8,
             0_LATENCY_WITH_WRITE_CC_CMU9,
             0_LATENCY_WITH_WRITE_CC_CMU10,
             0_LATENCY_WITH_WRITE_CC_CMU11,
             0_LATENCY_WITH_WRITE_CC_CMU12,
             0_LATENCY_WITH_WRITE_CC_CMU13,
             0_LATENCY_WITH_WRITE_CC_CMU14,
             0_LATENCY_WITH_WRITE_CC_CMU15,
             0_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_CMVV_COMPARISON_WITH_WRITE_16_CONDITION]>;

// For CMU.CMZ IRF comparision with 1 condition register
def :ItinRW<[0_LATENCY_WITH_IRF_R7,
             0_LATENCY_WITH_WRITE_CC_CMU0,
             0_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_CMZ_COMPARISON_IRF_WITH_WRITE_1_CONDITION]>;

// For CMU.CMZ IRF comparision with 2 condition register
def :ItinRW<[0_LATENCY_WITH_IRF_R7,
             0_LATENCY_WITH_WRITE_CC_CMU0,
             0_LATENCY_WITH_WRITE_CC_CMU1,
             0_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_CMZ_COMPARISON_IRF_WITH_WRITE_2_CONDITION]>;

// For CMU.CMZ IRF comparision with 4 condition register
def :ItinRW<[0_LATENCY_WITH_IRF_R7,
             0_LATENCY_WITH_WRITE_CC_CMU0,
             0_LATENCY_WITH_WRITE_CC_CMU1,
             0_LATENCY_WITH_WRITE_CC_CMU2,
             0_LATENCY_WITH_WRITE_CC_CMU3,
             0_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_CMZ_COMPARISON_IRF_WITH_WRITE_4_CONDITION]>;

// For CMU.CMZ VRF comparision with 16 condition register
def :ItinRW<[0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_WRITE_CC_CMU0,
             0_LATENCY_WITH_WRITE_CC_CMU1,
             0_LATENCY_WITH_WRITE_CC_CMU2,
             0_LATENCY_WITH_WRITE_CC_CMU3,
             0_LATENCY_WITH_WRITE_CC_CMU4,
             0_LATENCY_WITH_WRITE_CC_CMU5,
             0_LATENCY_WITH_WRITE_CC_CMU6,
             0_LATENCY_WITH_WRITE_CC_CMU7,
             0_LATENCY_WITH_WRITE_CC_CMU8,
             0_LATENCY_WITH_WRITE_CC_CMU9,
             0_LATENCY_WITH_WRITE_CC_CMU10,
             0_LATENCY_WITH_WRITE_CC_CMU11,
             0_LATENCY_WITH_WRITE_CC_CMU12,
             0_LATENCY_WITH_WRITE_CC_CMU13,
             0_LATENCY_WITH_WRITE_CC_CMU14,
             0_LATENCY_WITH_WRITE_CC_CMU15,
             0_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_CMZ_COMPARISON_VRF_WITH_WRITE_16_CONDITION]>;

// For CMU.RRR IRF min max instruction

// Starting with Myriad2v3, Min and Max also write to CC
def :ItinRW<[1_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_IRF_R7,
             0_LATENCY_WITH_IRF_R6,
             1_LATENCY_WITH_WRITE_CC_CMU0,
             1_LATENCY_WITH_WRITE_CC_CMU1,
             1_LATENCY_WITH_WRITE_CC_CMU2,
             1_LATENCY_WITH_WRITE_CC_CMU3,
             1_LATENCY_WITH_WRITE_CC_CMU4,
             1_LATENCY_WITH_WRITE_CC_CMU5,
             1_LATENCY_WITH_WRITE_CC_CMU6,
             1_LATENCY_WITH_WRITE_CC_CMU7,
             1_LATENCY_WITH_WRITE_CC_CMU8,
             1_LATENCY_WITH_WRITE_CC_CMU9,
             1_LATENCY_WITH_WRITE_CC_CMU10,
             1_LATENCY_WITH_WRITE_CC_CMU11,
             1_LATENCY_WITH_WRITE_CC_CMU12,
             1_LATENCY_WITH_WRITE_CC_CMU13,
             1_LATENCY_WITH_WRITE_CC_CMU14,
             1_LATENCY_WITH_WRITE_CC_CMU15,
             1_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_RRR_IRF_MIN_MAX_Myr2v3]>;

// For CMU.RRR VRF min max instruction

// Starting with Myriad2v3, Min and Max also write to CC
def :ItinRW<[1_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_VRF_R2,
             1_LATENCY_WITH_WRITE_CC_CMU0,
             1_LATENCY_WITH_WRITE_CC_CMU1,
             1_LATENCY_WITH_WRITE_CC_CMU2,
             1_LATENCY_WITH_WRITE_CC_CMU3,
             1_LATENCY_WITH_WRITE_CC_CMU4,
             1_LATENCY_WITH_WRITE_CC_CMU5,
             1_LATENCY_WITH_WRITE_CC_CMU6,
             1_LATENCY_WITH_WRITE_CC_CMU7,
             1_LATENCY_WITH_WRITE_CC_CMU8,
             1_LATENCY_WITH_WRITE_CC_CMU9,
             1_LATENCY_WITH_WRITE_CC_CMU10,
             1_LATENCY_WITH_WRITE_CC_CMU11,
             1_LATENCY_WITH_WRITE_CC_CMU12,
             1_LATENCY_WITH_WRITE_CC_CMU13,
             1_LATENCY_WITH_WRITE_CC_CMU14,
             1_LATENCY_WITH_WRITE_CC_CMU15,
             1_LATENCY_WITH_SHARED_RES_S8],
            [II_CMU_RRR_VRF_MIN_MAX_Myr2v3]>;

// For CMU.VSZM VRF 
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_VSZM_VRF]>;


// For CMU.CPII IRF move
def :ItinRW<[0_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_CPII_IRF]>;

// For CMU.CP.64 IRF64 move (Myriad2.3)
def :ItinRW<[0_LATENCY_WITH_IRF_W1,
             0_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_IRF_R6,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_CPII_IRF64]>;

// For CMU.CPII IRF move between float and int
def :ItinRW<[1_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_CPII_IRF_FLOAT_INT]>;

// For CMU.CPTI TRF to IRF move
def :ItinRW<[0_LATENCY_WITH_IRF_W3,
             0_LATENCY_NOPORT],
            [II_CMU_CPTI_IRF]>;

// For CMU.CPTI IRF to TRF move
def :ItinRW<[0_LATENCY_NOPORT,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_CPIT_IRF]>;

// For CMU.CPVV VRF move
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_CPVV_VRF]>;

// For CMU.CPVV VRF move conv
def :ItinRW<[1_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_CPVV_VRF_CONV]>;

// For CMU.CPVI from VRF to IRF move
def :ItinRW<[0_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_CPVI_IRF_VRF]>;

// For CMU.CPVI from VRF to IRF64 move
def :ItinRW<[0_LATENCY_WITH_IRF_W1,
             0_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_CPVI_IRF64_VRF]>;

// For CMU.CPVI from VRF to IRF move with conversion between int and float types
def :ItinRW<[1_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_CPVI_IRF_VRF_FCONV]>;
// For CMU.LUT[R] IRF
def :ItinRW<[1_LATENCY_WITH_IRF_W3,
             1_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_LUT_IRF]>;

// For CMU.LUTW IRF
def:ItinRW<[1_LATENCY_WITH_VRF_W1,
            0_LATENCY_WITH_VRF_R3,
            0_LATENCY_WITH_IRF_R6,
            0_LATENCY_WITH_IRF_R7],
            [II_CMU_LUTW_IRF]>;

// For CMU.CPIV from IRF to VRF move
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_NOPORT,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_CPIV_VRF_IRF]>;

// For CMU.CPIV from IRF64 to VRF move
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_NOPORT,
             0_LATENCY_WITH_IRF_R6,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_CPIV_VRF_IRF64]>;

// For CMU.CPIV from IRF to VRF move with conversion between int and float types
def :ItinRW<[1_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_CPIV_VRF_IRF_FCONV]>;

// For CMU.CPEI from VRF to IRF move
def :ItinRW<[0_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_IRF_R10],
            [II_CMU_CPEI_IRF_VRF]>;

// For CMU.CPEI from IRF to VRF move
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_IRF_R7,
             0_LATENCY_WITH_IRF_R5],
            [II_CMU_CPEI_VRF_IRF]>;

// For CMU.CPEI from VRF to VRF move
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_IRF_R10,
             0_LATENCY_WITH_IRF_R5],
            [II_CMU_CPEI_VRF_VRF]>;

// For CMU.CPZV
def :ItinRW<[0_LATENCY_WITH_VRF_W1],
            [II_CMU_CPZV_VRF]>;

// For CMU.CPZI
def :ItinRW<[0_LATENCY_WITH_IRF_W3],
            [II_CMU_CPZI_IRF]>;

// For CMU.CPZT
def :ItinRW<[0_LATENCY_WITH_TRF_WRITE],
            [II_CMU_CPZT_TRF]>;

// For CMU.VILV
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_W4,
             0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_VRF_R2],
            [II_CMU_VILV_VRF]>;

// For CMU.SHLIV
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_SHLIV]>;

// For CMU.VNZ
def :ItinRW<[0_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_VNZ]>;

// For CMU.VSIGNF VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_VSIGNF_VRF]>;

// For CMU.VSIGNF IRF
def :ItinRW<[0_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_VSIGNF_IRF]>;

// For CMU.CLAMPAB VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W1,
			 0_LATENCY_WITH_VRF_R3,
			 0_LATENCY_WITH_VRF_R2,
			 0_LATENCY_WITH_VRF_R4],
			[II_CMU_CLAMPAB_VRF]>;

// For CMU.CLAMPAB IRF
def :ItinRW<[1_LATENCY_WITH_IRF_W3,
			 0_LATENCY_WITH_IRF_R7,
			 0_LATENCY_WITH_IRF_R6,
			 0_LATENCY_WITH_IRF_R10],
			[II_CMU_CLAMPAB_IRF]>;

// For CMU.CLAMP IRF
def :ItinRW<[1_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_IRF_R7,
             0_LATENCY_WITH_IRF_R6],
            [II_CMU_CLAMP_IRF]>;

// For CMU.CLAMP VRF
def :ItinRW<[1_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_VRF_R2],
            [II_CMU_CLAMP_VRF]>;

// For CMU.CPVCR VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_CPVCR_VRF]>;

// For CMU.CPVRC VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_CPVRC_VRF]>;

// For CMU.ALIGN VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_VRF_R2],
            [II_CMU_ALIGN_VRF]>;

// For CMU.SWZ VRF
def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0],
            [II_CMU_SWZ_VRF]>;

// For CMU.CMASK
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_CMASK]>;

// For CMU.VSZM IRF 
def :ItinRW<[0_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_VSZM_IRF]>;

// For CMU.EXPAND NPU4+
def :ItinRW<[1_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_EXPAND]>;

def :ItinRW<[1_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_IRF_R6,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_EXPAND_X8]>;

// For CMU.BIT
def :ItinRW<[0_LATENCY_WITH_IRF_W3,
             0_LATENCY_WITH_IRF_R7,
             0_LATENCY_WITH_IRF_R6],
            [II_CMU_BIT]>;

// For CMU.COMPRESS
def :ItinRW<[1_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_COMPRESS]>;

// For CMU.COMPRESS.X8
def :ItinRW<[1_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R3,
             0_LATENCY_WITH_IRF_R6,
             0_LATENCY_WITH_IRF_R7],
            [II_CMU_COMPRESS_X8]>;

// For CMU.PERM
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_WITH_VRF_R2,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_PERM]>;

// For CMU.PERMB
def :ItinRW<[0_LATENCY_WITH_VRF_W1,
             0_LATENCY_NOPORT,
             0_LATENCY_WITH_VRF_R2,
             0_LATENCY_WITH_VRF_R3],
            [II_CMU_PERMB]>;

} // SHAVEModel
