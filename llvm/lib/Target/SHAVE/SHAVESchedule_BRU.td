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
// File       : SHAVESchedule_BRU.td
// Description: Tablegen classes for scheduling information
// ---------------------------------------------------------------------------
// ***************************************************************************

def II_BRU_BRA : InstrItinClass;
def II_BRU_JMP_IRF : InstrItinClass;
def II_BRU_RFB : InstrItinClass;
def II_BRU_RPI_IRF : InstrItinClass;
def II_BRU_RPIM : InstrItinClass;
def II_BRU_RPL_IRF : InstrItinClass;
def II_BRU_RPS_IRF : InstrItinClass;
def II_BRU_SWIC_SWIH : InstrItinClass;
def II_BRU_SWP_IRF : InstrItinClass;

let SchedModel = SHAVEModel in {

// For II_BRU_BRA
def :ItinRW<[0_LATENCY_NOPORT],
            [II_BRU_BRA]>;

// For II_BRU_JMP_IRF
def :ItinRW<[0_LATENCY_WITH_IRF_R10,
             0_LATENCY_NOPORT],
            [II_BRU_JMP_IRF]>;

// For II_BRU_RFB
def :ItinRW<[0_LATENCY_NOPORT],
            [II_BRU_RFB]>;

// For II_BRU_RPI_IRF
def :ItinRW<[0_LATENCY_WITH_IRF_R10,
             0_LATENCY_NOPORT,
             0_LATENCY_NOPORT,
             0_LATENCY_NOPORT],
            [II_BRU_RPI_IRF]>;

// For II_BRU_RPIM
def :ItinRW<[0_LATENCY_NOPORT,
             0_LATENCY_NOPORT,
             0_LATENCY_NOPORT],
            [II_BRU_RPIM]>;


// For II_BRU_RPL_IRF
def :ItinRW<[0_LATENCY_WITH_IRF_R10,
             0_LATENCY_WITH_IRF_R2,
             0_LATENCY_NOPORT,
             0_LATENCY_NOPORT,
             0_LATENCY_NOPORT,
             0_LATENCY_NOPORT,
             0_LATENCY_NOPORT],
            [II_BRU_RPL_IRF]>;

// For II_BRU_RPS_IRF
def :ItinRW<[0_LATENCY_WITH_IRF_R10,
             0_LATENCY_NOPORT,
             0_LATENCY_NOPORT,
             0_LATENCY_NOPORT,
             0_LATENCY_NOPORT],
            [II_BRU_RPS_IRF]>;

// For II_BRU_SWIC_SWIH
def :ItinRW<[0_LATENCY_NOPORT],
            [II_BRU_SWIC_SWIH]>;

// For II_BRU_SWP_IRF
def :ItinRW<[6_LATENCY_WITH_IRF_W2,
             0_LATENCY_WITH_IRF_R10],
            [II_BRU_SWP_IRF]>;

} //SHAVEModel
