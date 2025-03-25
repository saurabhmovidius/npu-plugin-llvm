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
// File       : SHAVESchedule_IAU.td
// Description: Tablegen classes for scheduling information
// ---------------------------------------------------------------------------
// ***************************************************************************

def II_IAU_RI_WCC : InstrItinClass;
def II_IAU_RR : InstrItinClass;
def II_IAU_RR_WCC : InstrItinClass;
def II_IAU_RR_WCC_WI : InstrItinClass;
def II_IAU_RRI : InstrItinClass;
def II_IAU_RRI_WCC : InstrItinClass;
def II_IAU_RRI_WCC_WI : InstrItinClass;
def II_IAU_RRR_RWI : InstrItinClass;
def II_IAU_RRR_WI : InstrItinClass;
def II_IAU_RRR_WCC : InstrItinClass;
def II_IAU_RRR_WCC_WI : InstrItinClass;
def II_IAU_RRRI : InstrItinClass;
def II_IAU_RRRR_WCC : InstrItinClass;
def II_IAU_MUL_RRR : InstrItinClass;
def II_IAU_MUL_RRI : InstrItinClass;
def II_IAU_LMUL_MULS : InstrItinClass;

let SchedModel = SHAVEModel in {

// No dest, one source, impDef: CC_IAU0.
def :ItinRW<[0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_WRITE_CC_IAU0],
            [II_IAU_RI_WCC]>;

// One dest, one source, impDef: none.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0],
            [II_IAU_RR]>;

// One dest, one source, impDef: CC_IAU0.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_WRITE_CC_IAU0],
            [II_IAU_RR_WCC]>;

// One dest, one source, impDef: CC_IAU0, I_STATE.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_WRITE_CC_IAU0,
             0_LATENCY_WITH_WRITE_I_STATE],
            [II_IAU_RR_WCC_WI]>;

// One dest, one source, impDef: none.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0],
            [II_IAU_RRI]>;

// One dest, one source, impDef: CC_IAU0.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_WRITE_CC_IAU0],
            [II_IAU_RRI_WCC]>;

// One dest, one source, impDef: CC_IAU0, I_STATE.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_WRITE_CC_IAU0,
             0_LATENCY_WITH_WRITE_I_STATE],
            [II_IAU_RRI_WCC_WI]>;

// One dest, two sources, impDef: I_STATE.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_IRF_R1,
             0_LATENCY_WITH_WRITE_I_STATE],
            [II_IAU_RRR_WI]>;

// One dest, two sources, impDef/Use: I_STATE.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_IRF_R1,
             0_LATENCY_WITH_WRITE_I_STATE,
             0_LATENCY_WITH_WRITE_I_STATE],
            [II_IAU_RRR_RWI]>;

// One dest, two sources, impDef: CC_IAU0.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_IRF_R1,
             0_LATENCY_WITH_WRITE_CC_IAU0],
            [II_IAU_RRR_WCC]>;

// One dest, two sources, impDef: CC_IAU0, I_STATE.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_IRF_R1,
             0_LATENCY_WITH_WRITE_CC_IAU0,
             0_LATENCY_WITH_WRITE_I_STATE],
            [II_IAU_RRR_WCC_WI]>;

// One dest, two sources.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_IRF_R1],
            [II_IAU_RRRI]>;

// One dest, three sources, impDef: CC_IAU0.
def :ItinRW<[0_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_IRF_R1,
             0_LATENCY_WITH_IRF_R11,
             0_LATENCY_WITH_WRITE_CC_IAU0],
            [II_IAU_RRRR_WCC]>;

// For II_IAU_MUL_RRR
def :ItinRW<[1_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_IRF_R1,
             1_LATENCY_WITH_WRITE_I_AE,
             1_LATENCY_WITH_WRITE_CC_IAU0],
            [II_IAU_MUL_RRR]>;

// For II_IAU_MUL_RRI
def :ItinRW<[1_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0],
            [II_IAU_MUL_RRI]>;

// For II_IAU_LMUL_MULS
def :ItinRW<[1_LATENCY_WITH_IRF_W0,
             0_LATENCY_WITH_IRF_R0,
             0_LATENCY_WITH_IRF_R1,
             1_LATENCY_WITH_WRITE_CC_IAU0],
            [II_IAU_LMUL_MULS]>;

} // SHAVEModel
