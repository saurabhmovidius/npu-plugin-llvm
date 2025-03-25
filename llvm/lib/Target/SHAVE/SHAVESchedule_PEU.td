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
// File       : SHAVESchedule_PEU.td
// Description: Tablegen classes for scheduling information
// ---------------------------------------------------------------------------
// ***************************************************************************

def II_PEU_PCC_CMU : InstrItinClass;
def II_PEU_PVV_CMU_0_3 : InstrItinClass;
def II_PEU_PVV_CMU_0_7 : InstrItinClass;
def II_PEU_PVV_CMU_0_15 : InstrItinClass;
def II_PEU_PVEN8C : InstrItinClass;
def II_PEU_PVEN8C_Myr2v1 : InstrItinClass;

let SchedModel = SHAVEModel in {

def :ItinRW<[0_LATENCY_WITH_WRITE_CC_CMU0],
            [II_PEU_PCC_CMU]>;
def :ItinRW<[0_LATENCY_WITH_CMU0_3],
            [II_PEU_PVV_CMU_0_3]>;
def :ItinRW<[0_LATENCY_WITH_CMU0_7],
            [II_PEU_PVV_CMU_0_7]>;
def :ItinRW<[0_LATENCY_WITH_CMU0_15],
            [II_PEU_PVV_CMU_0_15]>;

def :ItinRW<[0_LATENCY_NOPORT],
            [II_PEU_PVEN8C]>;

def :ItinRW<[0_LATENCY_WITH_VRF_W0,
             0_LATENCY_WITH_VRF_R0,
             0_LATENCY_WITH_VRF_R1],
            [II_PEU_PVEN8C_Myr2v1]>;

} // SHAVEModel
