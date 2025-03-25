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
// File       : SHAVESchedule.td
// Description: Tablegen classes for scheduling information
// ---------------------------------------------------------------------------
// ***************************************************************************

class SHAVEFuncUnit<string name, int id, int n> : FuncUnit
{
	field string Name = name;
	field int Id = id;
	field int NumberOfUnits = n;
}

class SHAVEPort<string name, int id>
{
  field string Name = name;
  field int Id = id;
}

// the Id field in the SHAVEPort must be kept in sync
// with SHAVEPort enum in SHAVEInstrInfo.h
// Enumeration of the read and write ports to the register file
def NONE_RW : SHAVEPort<"NONE_RW", 0>;

def VRF_R0 : SHAVEPort<"VRF_R0", 1>;
def VRF_R1 : SHAVEPort<"VRF_R1", 2>;
def VRF_R2 : SHAVEPort<"VRF_R2", 3>;
def VRF_R3 : SHAVEPort<"VRF_R3", 4>;
def VRF_R4 : SHAVEPort<"VRF_R4", 5>;
def VRF_R5 : SHAVEPort<"VRF_R5", 6>;

def VRF_W0 : SHAVEPort<"VRF_W0", 7>;
def VRF_W1 : SHAVEPort<"VRF_W1", 8>;
def VRF_W2 : SHAVEPort<"VRF_W2", 9>;
def VRF_W3 : SHAVEPort<"VRF_W3", 10>;
def VRF_W4 : SHAVEPort<"VRF_W4", 11>;
def VRF_W5 : SHAVEPort<"VRF_W5", 12>;

def IRF_R0 : SHAVEPort<"IRF_R0", 13>;
def IRF_R1 : SHAVEPort<"IRF_R1", 14>;
def IRF_R2 : SHAVEPort<"IRF_R2", 15>;
def IRF_R3 : SHAVEPort<"IRF_R3", 16>;
def IRF_R4 : SHAVEPort<"IRF_R4", 17>;
def IRF_R5 : SHAVEPort<"IRF_R5", 18>;
def IRF_R6 : SHAVEPort<"IRF_R6", 19>;
def IRF_R7 : SHAVEPort<"IRF_R7", 20>;
def IRF_R8 : SHAVEPort<"IRF_R8", 21>;
def IRF_R9 : SHAVEPort<"IRF_R9", 22>;
def IRF_R10 : SHAVEPort<"IRF_R10", 23>;
def IRF_R11 : SHAVEPort<"IRF_R11", 24>;

def IRF_W0 : SHAVEPort<"IRF_W0", 25>;
def IRF_W1 : SHAVEPort<"IRF_W1", 26>;
def IRF_W2 : SHAVEPort<"IRF_W2", 27>;
def IRF_W3 : SHAVEPort<"IRF_W3", 28>;
def IRF_W4 : SHAVEPort<"IRF_W4", 29>;
def IRF_W5 : SHAVEPort<"IRF_W5", 30>;

def MEM_R : SHAVEPort<"MEM_R", 43>;
def MEM_W : SHAVEPort<"MEM_W", 44>;

// the Id field in the SHAVEFuncUnit must be kept in sync
// with SHAVEFuncUnit enum in SHAVEInstrInfo.h
// Functional units
def IAU : SHAVEFuncUnit<"IAU", 1, 1>;
def SAU : SHAVEFuncUnit<"SAU", 2, 1>;
def VAU : SHAVEFuncUnit<"VAU", 3, 1>;
def PEU : SHAVEFuncUnit<"PEU", 4, 1>;
def CMU : SHAVEFuncUnit<"CMU", 5, 1>;
def BRU : SHAVEFuncUnit<"BRU", 6, 1>;
def LSU0 : SHAVEFuncUnit<"LSU0", 7, 1>;
def LSU1 : SHAVEFuncUnit<"LSU1", 8, 1>;
def PseudoUnit : SHAVEFuncUnit<"Pseudo", 0, 0>;

def FU_VRF_R0 : SHAVEFuncUnit<"FU_VRF_R0", 9, 1>;
def FU_VRF_R1 : SHAVEFuncUnit<"FU_VRF_R1", 10, 1>;
def FU_VRF_R2 : SHAVEFuncUnit<"FU_VRF_R2", 11, 1>; 
def FU_VRF_R3 : SHAVEFuncUnit<"FU_VRF_R3", 12, 1>;
def FU_VRF_R4 : SHAVEFuncUnit<"FU_VRF_R4", 13, 1>;
def FU_VRF_R5 : SHAVEFuncUnit<"FU_VRF_R5", 14, 1>;

def FU_VRF_W0 : SHAVEFuncUnit<"FU_VRF_W0", 15, 1>;
def FU_VRF_W1 : SHAVEFuncUnit<"FU_VRF_W1", 16, 1>;
def FU_VRF_W2 : SHAVEFuncUnit<"FU_VRF_W2", 17, 1>;
def FU_VRF_W3 : SHAVEFuncUnit<"FU_VRF_W3", 18, 1>;
def FU_VRF_W4 : SHAVEFuncUnit<"FU_VRF_W4", 19, 1>;
def FU_VRF_W5 : SHAVEFuncUnit<"FU_VRF_W5", 20, 1>;

def FU_IRF_R0 : SHAVEFuncUnit<"FU_IRF_R0", 21, 1>;
def FU_IRF_R1 : SHAVEFuncUnit<"FU_IRF_R1", 22, 1>;
def FU_IRF_R2 : SHAVEFuncUnit<"FU_IRF_R2", 23, 1>;
def FU_IRF_R3 : SHAVEFuncUnit<"FU_IRF_R3", 24, 1>;
def FU_IRF_R4 : SHAVEFuncUnit<"FU_IRF_R4", 25, 1>;
def FU_IRF_R5 : SHAVEFuncUnit<"FU_IRF_R5", 26, 1>;
def FU_IRF_R6 : SHAVEFuncUnit<"FU_IRF_R6", 27, 1>;
def FU_IRF_R7 : SHAVEFuncUnit<"FU_IRF_R7", 28, 1>;
def FU_IRF_R8 : SHAVEFuncUnit<"FU_IRF_R8", 29, 1>;
def FU_IRF_R9 : SHAVEFuncUnit<"FU_IRF_R9", 30, 1>;
def FU_IRF_R10 : SHAVEFuncUnit<"FU_IRF_R10", 31, 1>;
def FU_IRF_R11 : SHAVEFuncUnit<"FU_IRF_R11", 32, 1>;

def FU_IRF_W0 : SHAVEFuncUnit<"FU_IRF_W0", 33, 1>;
def FU_IRF_W1 : SHAVEFuncUnit<"FU_IRF_W1", 34, 1>;
def FU_IRF_W2 : SHAVEFuncUnit<"FU_IRF_W2", 35, 1>;
def FU_IRF_W3 : SHAVEFuncUnit<"FU_IRF_W3", 36, 1>;
def FU_IRF_W4 : SHAVEFuncUnit<"FU_IRF_W4", 37, 1>;
def FU_IRF_W5 : SHAVEFuncUnit<"FU_IRF_W5", 38, 1>;

def FU_DUMMY : SHAVEFuncUnit<"FU_DUMMY", 51, 1>;
def FU_SPECIAL : SHAVEFuncUnit<"FU_SPECIAL", 52, 1>;

def II_DUMMY : InstrItinClass;
def II_NOP : InstrItinClass;

def SHAVEModel : SchedMachineModel {
  let IssueWidth = 7;
  let CompleteModel = 0;
}

let SchedModel = SHAVEModel in {

// Predicates for tweaking register port delays
def Has_6_Cycle_Load_Latency : SchedPredicate<"(((const SHAVESubtarget*)SchedModel->getSubtargetInfo())->getMinimumLoadLatency() == 6)">;
def Has_8_Cycle_Load_Latency : SchedPredicate<"(((const SHAVESubtarget*)SchedModel->getSubtargetInfo())->getMinimumLoadLatency() == 8)">;
def HasSharedResources_Not   : SchedPredicate<"!(((const SHAVESubtarget*)SchedModel->getSubtargetInfo())->hasFeature(SHAVE::HasSharedResources_Feature))">;
def HasSharedResources       : SchedPredicate<"((const SHAVESubtarget*)SchedModel->getSubtargetInfo())->hasFeature(SHAVE::HasSharedResources_Feature)">;

// Port Resource Model

// VRF_R0 - VRF_R5
// VRF_W0 - VRF_W5
foreach NumPort = 0-5 in {
def PR_VRF_R#NumPort : ProcResource<1>;
def PR_VRF_W#NumPort : ProcResource<1>;
}

// IRF_R0 - IRF_R11
foreach NumPort = 0-11 in {
def PR_IRF_R#NumPort : ProcResource<1>;
}

// IRF_W0 - IRF_W5
foreach NumPort = 0-5 in {
def PR_IRF_W#NumPort : ProcResource<1>;
}

// NPU4 shared resources definitions
// S0  VAU lane 0/SAU adder input stage
// S1  S_ACC0 write
// S2  S_ACC1 write
// S3  V_ACC0 write
// S4  V_ACC1 write
// S5  VAU lane 0/Virtual SAU input
// S6  CMU Swizzle input
// S7  VAU Swizzle input
// S8  CMU condition codes
// S9  VAU lane 0/Virtual SAU output

foreach ResNumber = [0, 1, 2, 3, 4, 5, 8, 9] in {
  def SHARED_RES_S#ResNumber : ProcResource<1>;
}

// Dummy ProcResource
def PR_DUMMY : ProcResource<0>;

//Condition Register Resource Model
//In order to denote the latency of implicitly def and use.

// CC_CMU0 - CC_CMU15
foreach NumCondReg = 0-15 in {
def PR_CC_CMU#NumCondReg : ProcResource<1>;
}

// CC_SAU0 - CC_SAU3
foreach NumCondReg = 0-3 in {
def PR_CC_SAU#NumCondReg : ProcResource<1>;
}

// CC_IAU0
def PR_CC_IAU0 : ProcResource<1>;

// C_CSI
def PR_C_CSI : ProcResource<1>;

// F_AE
def PR_F_AE : ProcResource<1>;

// I_AE
def PR_I_AE : ProcResource<1>;

// V_ACC3
def PR_V_ACC3 : ProcResource<1>;

// V_ACC2
def PR_V_ACC2 : ProcResource<1>;

// V_ACC1
def PR_V_ACC1 : ProcResource<1>;

// V_ACC0
def PR_V_ACC0 : ProcResource<1>;

// S_ACC
def PR_S_ACC : ProcResource<1>;

// V_STATE
def PR_V_STATE : ProcResource<1>;

// S_STATE
def PR_S_STATE : ProcResource<1>;

// I_STATE
def PR_I_STATE : ProcResource<1>;

class SHAVEReadOp<int acquireCycle, ProcResourceKind resource, int releaseLatency = 1> : SchedWriteRes<[resource]> {
    let Latency = !sub(!add(acquireCycle, releaseLatency), 1);
    let AcquireAtCycles = [acquireCycle];
    let ReleaseAtCycles = [!add(acquireCycle, releaseLatency)];
}

class SHAVEWriteOp<int acquireCycle, ProcResourceKind resource, int releaseLatency = 1> : SchedWriteRes<[resource]> {
    let Latency = !sub(!add(acquireCycle, releaseLatency), 1);
    let AcquireAtCycles = [acquireCycle];
    let ReleaseAtCycles = [!add(acquireCycle, releaseLatency)];
}

class SHAVEReadOpNoPort<int latency> : SchedWriteRes<[]> {
    let Latency = latency;
}

class SHAVEWriteOpNoPort<int latency> : SchedWriteRes<[]> {
    let Latency = latency;
}

// VRF port uses.
foreach NumLatency = 0-15 in {
  foreach Port = 0-5 in {
    def ""#NumLatency#"_LATENCY_WITH_VRF_W"#Port :
        SHAVEWriteOp<NumLatency, !cast<ProcResource>("PR_VRF_W"#Port)>;
  }
  foreach Port = 0-5 in {
    def ""#NumLatency#_LATENCY_WITH_VRF_R#Port :
        SHAVEWriteOp<NumLatency, !cast<ProcResource>("PR_VRF_R"#Port)>;
  }
}

// IRF port uses.
foreach NumLatency = 0-15 in {
  foreach Port = 0-5 in {
    def ""#NumLatency#_LATENCY_WITH_IRF_W#Port :
        SHAVEWriteOp<NumLatency, !cast<ProcResource>("PR_IRF_W"#Port)>;
  }
  foreach Port = 0-11 in {
    def ""#NumLatency#_LATENCY_WITH_IRF_R#Port :
        SHAVEWriteOp<NumLatency, !cast<ProcResource>("PR_IRF_R"#Port)>;
  }
}

foreach NumLatency = 0-15 in {
  foreach ResNumber = [0, 1, 2, 3, 4, 5, 8, 9] in {
    def NumLatency#_LATENCY_WITH_SHARED_RES_S#ResNumber : SchedWriteVariant<[
      SchedVar<HasSharedResources_Not, []>,
      SchedVar<HasSharedResources, [SHAVEWriteOp<NumLatency, !cast<ProcResource>("SHARED_RES_S"#ResNumber)>]>
    ]>;
  }
}

// CMU portless uses.
foreach NumLatency = 0-5 in {
  foreach CCIdx = 0-15 in {
    def ""#NumLatency#_LATENCY_WITH_WRITE_CC_CMU#CCIdx
        : SHAVEWriteOpNoPort<NumLatency>;
  }
}

def 0_LATENCY_WITH_TRF_WRITE : SHAVEWriteOpNoPort<0>;
def 0_LATENCY_WITH_CMU0_3 : SHAVEWriteOpNoPort<0>;
def 0_LATENCY_WITH_CMU0_7 : SHAVEWriteOpNoPort<0>;
def 0_LATENCY_WITH_CMU0_15 : SHAVEWriteOpNoPort<0>;

foreach NumLatency = 0-15 in {
  foreach SAUIdx = 0-3 in {
    def ""#NumLatency#_LATENCY_WITH_WRITE_CC_SAU#SAUIdx
        : SHAVEWriteOpNoPort<NumLatency>;
  }
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_WRITE_CC_IAU0 : SHAVEWriteOpNoPort<NumLatency>;
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_WRITE_C_CSI : SHAVEWriteOpNoPort<NumLatency>;
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_WRITE_F_AE : SHAVEWriteOpNoPort<NumLatency>;
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_WRITE_I_AE : SHAVEWriteOpNoPort<NumLatency>;
}

foreach NumLatency = 0-15 in {
  foreach ACCIdx = 0-3 in {
    def ""#NumLatency#_LATENCY_WITH_WRITE_V_ACC#ACCIdx
        : SHAVEWriteOpNoPort<NumLatency>;
  }
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_READ_V_ACC : SHAVEReadOpNoPort<NumLatency>;
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_READ_S_ACC : SHAVEReadOpNoPort<NumLatency>;
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_WRITE_V_ACC : SHAVEWriteOpNoPort<NumLatency>;
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_WRITE_S_ACC : SHAVEWriteOpNoPort<NumLatency>;
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_WRITE_V_STATE : SHAVEWriteOpNoPort<NumLatency>;
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_WRITE_S_STATE : SHAVEWriteOpNoPort<NumLatency>;
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_WITH_WRITE_I_STATE : SHAVEWriteOpNoPort<NumLatency>;
}

foreach NumLatency = 0-15 in {
  def ""#NumLatency#_LATENCY_NOPORT : SHAVEWriteOpNoPort<NumLatency>;
}

// This fools tablegen into thinking these ports are used.
def :ItinRW<[0_LATENCY_WITH_IRF_R5,
             0_LATENCY_WITH_VRF_W5],
            [II_DUMMY]>;

} // SHAVEModel

include "SHAVESchedule_CMU.td"
include "SHAVESchedule_SAU.td"
include "SHAVESchedule_LSU.td"
include "SHAVESchedule_IAU.td"
include "SHAVESchedule_VAU.td"
include "SHAVESchedule_PEU.td"
include "SHAVESchedule_BRU.td"
