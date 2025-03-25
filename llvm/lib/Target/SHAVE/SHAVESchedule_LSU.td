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
// File       : SHAVESchedule_LSU.td
// Description: Tablegen classes for scheduling information
// ---------------------------------------------------------------------------
// ***************************************************************************

def 0_LATENCY_WITH_LSU0_IOA : SHAVEWriteOp<0, PR_IRF_W4>;
def 0_LATENCY_WITH_LSU1_IOA : SHAVEWriteOp<0, PR_IRF_W5>;
def 0_LATENCY_WITH_LSU0_VOA : SHAVEWriteOp<0, PR_VRF_W2>;
def 0_LATENCY_WITH_LSU1_VOA : SHAVEWriteOp<0, PR_VRF_W3>;

def LOAD_LATENCY_WITH_LSU0_IOA : SHAVEWriteOp<6, PR_IRF_W4>;
def LOAD_LATENCY_WITH_LSU1_IOA : SHAVEWriteOp<6, PR_IRF_W5>;
def LOAD_LATENCY_WITH_LSU0_IOB : SHAVEWriteOp<6, PR_IRF_W5>;
def LOAD_LATENCY_WITH_LSU1_IOB : SHAVEWriteOp<6, PR_IRF_W4>;
def LOAD_LATENCY_WITH_LSU0_VOA : SHAVEWriteOp<6, PR_VRF_W2>;
def LOAD_LATENCY_WITH_LSU1_VOA : SHAVEWriteOp<6, PR_VRF_W3>;
def LOAD_LATENCY_PLUS_ONE_WITH_LSU0_VOA : SHAVEWriteOp<6, PR_VRF_W2, 2>;
def LOAD_LATENCY_PLUS_ONE_WITH_LSU1_VOA : SHAVEWriteOp<6, PR_VRF_W3, 2>;

def LOAD_LATENCY_WITH_LSU0_IOA_Myr4 : SHAVEWriteOp<8, PR_IRF_W4>;
def LOAD_LATENCY_WITH_LSU1_IOA_Myr4 : SHAVEWriteOp<8, PR_IRF_W5>;
def LOAD_LATENCY_WITH_LSU0_IOB_Myr4 : SHAVEWriteOp<8, PR_IRF_W5>;
def LOAD_LATENCY_WITH_LSU1_IOB_Myr4 : SHAVEWriteOp<8, PR_IRF_W4>;
def LOAD_LATENCY_WITH_LSU0_VOA_Myr4 : SHAVEWriteOp<8, PR_VRF_W2>;
def LOAD_LATENCY_WITH_LSU1_VOA_Myr4 : SHAVEWriteOp<8, PR_VRF_W3>;
def LOAD_LATENCY_PLUS_ONE_WITH_LSU0_VOA_Myr4 : SHAVEWriteOp<8, PR_VRF_W2, 2>;
def LOAD_LATENCY_PLUS_ONE_WITH_LSU1_VOA_Myr4 : SHAVEWriteOp<8, PR_VRF_W3, 2>;

// Define predicates to select the LSU operations based on the LSU unit ID.
foreach Unit = 0-1 in {
  def LSU#Unit#Pred : SchedPredicate<"((const SHAVEInstrInfo*)SchedModel->getInstrInfo())->GetFunctionalUnit(MI) == SHAVE::LSU"#Unit>;
}

def LOAD_LATENCY_WITH_LSU0_IOA_TGT : SchedWriteVariant<[
  SchedVar<Has_6_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU0_IOA]>,
  SchedVar<Has_8_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU0_IOA_Myr4]>
]>;

def LOAD_LATENCY_WITH_LSU1_IOA_TGT : SchedWriteVariant<[
  SchedVar<Has_6_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU1_IOA]>,
  SchedVar<Has_8_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU1_IOA_Myr4]>
]>;

def LOAD_LATENCY_WITH_LSU0_IOB_TGT : SchedWriteVariant<[
  SchedVar<Has_6_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU0_IOB]>,
  SchedVar<Has_8_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU0_IOB_Myr4]>
]>;

def LOAD_LATENCY_WITH_LSU1_IOB_TGT : SchedWriteVariant<[
  SchedVar<Has_6_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU1_IOB]>,
  SchedVar<Has_8_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU1_IOB_Myr4]>
]>;

def LOAD_LATENCY_WITH_LSU0_VOA_TGT : SchedWriteVariant<[
  SchedVar<Has_6_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU0_VOA]>,
  SchedVar<Has_8_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU0_VOA_Myr4]>
]>;

def LOAD_LATENCY_WITH_LSU1_VOA_TGT : SchedWriteVariant<[
  SchedVar<Has_6_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU1_VOA]>,
  SchedVar<Has_8_Cycle_Load_Latency, [LOAD_LATENCY_WITH_LSU1_VOA_Myr4]>
]>;

def LOAD_LATENCY_PLUS_ONE_WITH_LSU0_VOA_TGT : SchedWriteVariant<[
  SchedVar<Has_6_Cycle_Load_Latency, [LOAD_LATENCY_PLUS_ONE_WITH_LSU0_VOA]>,
  SchedVar<Has_8_Cycle_Load_Latency, [LOAD_LATENCY_PLUS_ONE_WITH_LSU0_VOA_Myr4]>
]>;

def LOAD_LATENCY_PLUS_ONE_WITH_LSU1_VOA_TGT : SchedWriteVariant<[
  SchedVar<Has_6_Cycle_Load_Latency, [LOAD_LATENCY_PLUS_ONE_WITH_LSU1_VOA]>,
  SchedVar<Has_8_Cycle_Load_Latency, [LOAD_LATENCY_PLUS_ONE_WITH_LSU1_VOA_Myr4]>
]>;

// Dynamic per-operand scheduling definitions.
def LOAD_LATENCY_WITH_LSU_IOA : SchedWriteVariant<[
  SchedVar<LSU0Pred, [LOAD_LATENCY_WITH_LSU0_IOA_TGT]>,
  SchedVar<LSU1Pred, [LOAD_LATENCY_WITH_LSU1_IOA_TGT]>
]>;

def LOAD_LATENCY_WITH_LSU_IOB : SchedWriteVariant<[
  SchedVar<LSU0Pred, [LOAD_LATENCY_WITH_LSU0_IOB_TGT]>,
  SchedVar<LSU1Pred, [LOAD_LATENCY_WITH_LSU1_IOB_TGT]>
]>;

def LOAD_LATENCY_WITH_LSU_VOA : SchedWriteVariant<[
  SchedVar<LSU0Pred, [LOAD_LATENCY_WITH_LSU0_VOA_TGT]>,
  SchedVar<LSU1Pred, [LOAD_LATENCY_WITH_LSU1_VOA_TGT]>
]>;

def LOAD_LATENCY_PLUS_ONE_WITH_LSU_VOA : SchedWriteVariant<[
  SchedVar<LSU0Pred, [LOAD_LATENCY_PLUS_ONE_WITH_LSU0_VOA_TGT]>,
  SchedVar<LSU1Pred, [LOAD_LATENCY_PLUS_ONE_WITH_LSU1_VOA_TGT]>
]>;

def 0_LATENCY_WITH_LSU_IOA : SchedWriteVariant<[
  SchedVar<LSU0Pred, [0_LATENCY_WITH_LSU0_IOA]>,
  SchedVar<LSU1Pred, [0_LATENCY_WITH_LSU1_IOA]>
]>;

def 1_LATENCY_WITH_LSU_IOC : SchedWriteVariant<[
  SchedVar<LSU0Pred, [1_LATENCY_WITH_IRF_W2]>,
  SchedVar<LSU1Pred, [1_LATENCY_WITH_IRF_W3]>
]>;

def 0_LATENCY_WITH_LSU_IIA : SchedWriteVariant<[
  SchedVar<LSU0Pred, [0_LATENCY_WITH_IRF_R8]>,
  SchedVar<LSU1Pred, [0_LATENCY_WITH_IRF_R9]>
]>;

def 0_LATENCY_WITH_LSU_IIB : SchedWriteVariant<[
  SchedVar<LSU0Pred, [0_LATENCY_WITH_IRF_R9]>,
  SchedVar<LSU1Pred, [0_LATENCY_WITH_IRF_R8]>
]>;

def 0_LATENCY_WITH_LSU_VIA : SchedWriteVariant<[
  SchedVar<LSU0Pred, [0_LATENCY_WITH_VRF_R4]>,
  SchedVar<LSU1Pred, [0_LATENCY_WITH_VRF_R5]>
]>;

def 0_LATENCY_WITH_LSU_VIB : SchedWriteVariant<[
  SchedVar<LSU0Pred, [0_LATENCY_WITH_VRF_R3]>,
  SchedVar<LSU1Pred, [0_LATENCY_WITH_VRF_R1]>
]>;

def 0_LATENCY_WITH_LSU_VOA : SchedWriteVariant<[
  SchedVar<LSU0Pred, [0_LATENCY_WITH_LSU0_VOA]>,
  SchedVar<LSU1Pred, [0_LATENCY_WITH_LSU1_VOA]>
]>;

def 0_LATENCY_WITH_LSU_IIC : SchedWriteVariant<[
  SchedVar<LSU0Pred, [0_LATENCY_WITH_IRF_R4]>,
  SchedVar<LSU1Pred, [0_LATENCY_WITH_IRF_R6]>
]>;

def 0_LATENCY_WITH_LSU_IID : SchedWriteVariant<[
  SchedVar<LSU0Pred, [0_LATENCY_WITH_IRF_R5]>,
  SchedVar<LSU1Pred, [0_LATENCY_WITH_IRF_R7]>
]>;

def II_LSU_LDIL_IRF : InstrItinClass;
def II_LSU_LDIH_IRF : InstrItinClass;
def II_LSU_LD_IRF : InstrItinClass;
def II_LSU_LD_IRF64 : InstrItinClass;
def II_LSU_LD_VRF : InstrItinClass;
def II_LSU_LD_VRF_Element : InstrItinClass;
def II_LSU_LD_VRF_VRF_Element : InstrItinClass;
def II_LSU_LDD_IRF : InstrItinClass;
def II_LSU_LDD_IRF64 : InstrItinClass;
def II_LSU_LDD_VRF : InstrItinClass;
def II_LSU_LDI_IRF : InstrItinClass;
def II_LSU_LDI_IRF64 : InstrItinClass;
def II_LSU_LDI_VRF : InstrItinClass;
def II_LSU_LD128H_VRF : InstrItinClass;
def II_LSU_LDINC_IRF : InstrItinClass;
def II_LSU_LDINC_IRF64 : InstrItinClass;
def II_LSU_LDINC_VRF : InstrItinClass;
def II_LSU_LDO_IRF : InstrItinClass;
def II_LSU_LDO_IRF64 : InstrItinClass;
def II_LSU_LDO_VRF : InstrItinClass;
def II_LSU_LDX_IRF : InstrItinClass;
def II_LSU_LDX_IRF64 : InstrItinClass;
def II_LSU_LDX_VRF : InstrItinClass;
def II_LSU_LDX128_VRF : InstrItinClass;
def II_LSU_PFO_IRF : InstrItinClass;
def II_LSU_ST_IRF : InstrItinClass;
def II_LSU_ST_IRF64 : InstrItinClass;
def II_LSU_ST_VRF : InstrItinClass;
def II_LSU_STID_IRF : InstrItinClass;
def II_LSU_STID_VRF : InstrItinClass;
def II_LSU_ST128L_VRF : InstrItinClass;
def II_LSU_STX_IRF : InstrItinClass;
def II_LSU_STX_VRF : InstrItinClass;
def II_LSU_SWZ8 : InstrItinClass;
def II_LSU_SWZM4 : InstrItinClass;
def II_LSU_SWZM8 : InstrItinClass;
def II_LSU_CP_VRF_IRF : InstrItinClass;
def II_LSU_CP_IRF_VRF : InstrItinClass;
def II_LSU_CP_IRF_IRF : InstrItinClass;
def II_LSU_CP_VRF_VRF : InstrItinClass;

let SchedModel = SHAVEModel in {

// For II_LSU_SWZ*
def :ItinRW<[0_LATENCY_NOPORT],
            [II_LSU_SWZ8]>;

def :ItinRW<[0_LATENCY_NOPORT],
            [II_LSU_SWZM4]>;

def :ItinRW<[0_LATENCY_NOPORT],
            [II_LSU_SWZM8]>;

// For II_LSU_LDIL_IRF
def :ItinRW<[0_LATENCY_WITH_LSU_IOA],
            [II_LSU_LDIL_IRF]>;

// For II_LSU_LDIH_IRF
def :ItinRW<[0_LATENCY_WITH_LSU_IOA,
             0_LATENCY_NOPORT],
            [II_LSU_LDIH_IRF]>;

// For II_LSU_LD_IRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LD_IRF]>;

// For II_LSU_LD_IRF64
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             LOAD_LATENCY_WITH_LSU_IOB,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LD_IRF64]>;

// For II_LSU_LD_VRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_VOA,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LD_VRF]>;

// For II_LSU_LD_VRF_Element
def :ItinRW<[LOAD_LATENCY_WITH_LSU_VOA,
             0_LATENCY_NOPORT,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LD_VRF_Element]>;

// For II_LSU_LD_VRF_VRF_Element
def :ItinRW<[LOAD_LATENCY_WITH_LSU_VOA,
             0_LATENCY_NOPORT,
             0_LATENCY_WITH_LSU_VIB],
            [II_LSU_LD_VRF_VRF_Element]>;

// For II_LSU_LDD_IRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             1_LATENCY_WITH_LSU_IOC,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LDD_IRF]>;

// For II_LSU_LDD_IRF64
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             LOAD_LATENCY_WITH_LSU_IOB,
             1_LATENCY_WITH_LSU_IOC,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LDD_IRF64]>;

// For II_LSU_LDD_VRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_VOA,
             1_LATENCY_WITH_LSU_IOC,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LDD_VRF]>;

// For II_LSU_LDI_IRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             1_LATENCY_WITH_LSU_IOC,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LDI_IRF]>;

// For II_LSU_LDI_IRF64
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             LOAD_LATENCY_WITH_LSU_IOB,
             1_LATENCY_WITH_LSU_IOC,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LDI_IRF64]>;

// For II_LSU_LDI_VRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_VOA,
             1_LATENCY_WITH_LSU_IOC,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LDI_VRF]>;

// For II_LSU_LDINC_IRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             1_LATENCY_WITH_LSU_IOC,
             0_LATENCY_WITH_LSU_IIC,
             0_LATENCY_WITH_LSU_IID],
            [II_LSU_LDINC_IRF]>;

// For II_LSU_LDINC_IRF64
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             LOAD_LATENCY_WITH_LSU_IOB,
             1_LATENCY_WITH_LSU_IOC,
             0_LATENCY_WITH_LSU_IIC,
             0_LATENCY_WITH_LSU_IID],
            [II_LSU_LDINC_IRF64]>;

// For II_LSU_LDINC_VRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_VOA,
             1_LATENCY_WITH_LSU_IOC,
             0_LATENCY_WITH_LSU_IIC,
             0_LATENCY_WITH_LSU_IID],
            [II_LSU_LDINC_VRF]>;

// For II_LSU_LD128H_VRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_VOA,
             0_LATENCY_NOPORT,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LD128H_VRF]>;

// For II_LSU_LDO_IRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LDO_IRF]>;

// For II_LSU_LDO_IRF64
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             LOAD_LATENCY_WITH_LSU_IOB,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LDO_IRF64]>;

// For II_LSU_LDO_VRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_VOA,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_LDO_VRF]>;

// For II_LSU_LDX_IRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             0_LATENCY_WITH_LSU_IIC,
             0_LATENCY_WITH_LSU_IID],
            [II_LSU_LDX_IRF]>;

// For II_LSU_LDX_IRF64
def :ItinRW<[LOAD_LATENCY_WITH_LSU_IOA,
             LOAD_LATENCY_WITH_LSU_IOB,
             0_LATENCY_WITH_LSU_IIC,
             0_LATENCY_WITH_LSU_IID],
            [II_LSU_LDX_IRF64]>;

// For II_LSU_LDX_VRF
def :ItinRW<[LOAD_LATENCY_WITH_LSU_VOA,
             0_LATENCY_WITH_LSU_IIC,
             0_LATENCY_WITH_LSU_IID],
            [II_LSU_LDX_VRF]>;

// For II_LSU_LDX128_VRF
def :ItinRW<[LOAD_LATENCY_PLUS_ONE_WITH_LSU_VOA,
             0_LATENCY_WITH_LSU_IIC,
             0_LATENCY_WITH_LSU_IID],
            [II_LSU_LDX128_VRF]>;

// For II_LSU_PFO_IRF
def :ItinRW<[0_LATENCY_WITH_LSU_IIC],
            [II_LSU_PFO_IRF]>;

// For II_LSU_ST_IRF
def :ItinRW<[0_LATENCY_WITH_LSU_IIA,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_ST_IRF]>;

// For II_LSU_ST_IRF64
def :ItinRW<[0_LATENCY_WITH_LSU_IIA,
             0_LATENCY_WITH_LSU_IIB,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_ST_IRF64]>;

// For II_LSU_ST_VRF
def :ItinRW<[0_LATENCY_WITH_LSU_VIA,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_ST_VRF]>;

// For II_LSU_STID_IRF
def :ItinRW<[1_LATENCY_WITH_LSU_IOC,
             0_LATENCY_WITH_LSU_IIA,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_STID_IRF]>;

// For II_LSU_STID_VRF
def :ItinRW<[1_LATENCY_WITH_LSU_IOC,
             0_LATENCY_WITH_LSU_VIA,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_STID_VRF]>;

// For II_LSU_ST128L_VRF
def :ItinRW<[0_LATENCY_NOPORT,
             0_LATENCY_WITH_LSU_VIA,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_ST128L_VRF]>;

// For II_LSU_STX_IRF
def :ItinRW<[0_LATENCY_WITH_LSU_IID,
             0_LATENCY_WITH_LSU_IIA,
             0_LATENCY_WITH_LSU_IIC],
            [II_LSU_STX_IRF]>;

// For II_LSU_STX_VRF
def :ItinRW<[0_LATENCY_WITH_LSU_VIA,
             0_LATENCY_WITH_LSU_IIC,
             0_LATENCY_WITH_LSU_IID],
            [II_LSU_STX_VRF]>;


// LSU_CP*
def :ItinRW<[0_LATENCY_WITH_LSU_VOA,
             0_LATENCY_WITH_LSU_IIA],
            [II_LSU_CP_VRF_IRF]>;

def :ItinRW<[0_LATENCY_WITH_LSU_IOA,
             0_LATENCY_WITH_LSU_VIA],
            [II_LSU_CP_IRF_VRF]>;

def :ItinRW<[0_LATENCY_WITH_LSU_IOA,
             0_LATENCY_WITH_LSU_IIA],
            [II_LSU_CP_IRF_IRF]>;

def :ItinRW<[0_LATENCY_WITH_LSU_VOA,
             0_LATENCY_WITH_LSU_VIA],
            [II_LSU_CP_VRF_VRF]>;       

} // SHAVEModel
