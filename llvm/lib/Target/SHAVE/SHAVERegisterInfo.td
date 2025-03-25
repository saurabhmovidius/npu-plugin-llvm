/****************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2025 Intel Corporation.
 *
 * This software and the related documents are Intel copyrighted materials, and
 * your use of them is governed by the express license under which they were
 * provided to you ("License"). Unless the License provides otherwise, you may
 * not use, modify, copy, publish, distribute, disclose or transmit this software
 * or the related documents without Intel's prior written permission.
 *
 * This software and the related documents are provided as is, with no express or
 * implied warranties, other than those that are expressly stated in the License.
 *
 * IMPORTANT NOTICE:
 *
 *    The enumeration of the Dwarf debug numbers for the registers is assumed
 *    by other tools, in particular 'moviDebug', so these should not be
 *    changed without coordinating that change with the other tool teams.
 *
 ****************************************************************************/

class SHAVEReg<string n> : Register<n> {
  let Namespace = "SHAVE";
}

class SHAVERegWithSubregs<string n, list<Register> subregs> : SHAVEReg<n> {
  let SubRegs = subregs;
}

// Subregister indices
let Namespace = "SHAVE" in {
  def vsub16_0 : SubRegIndex<16>;
  def vsub16_1 : SubRegIndex<16, 16>;
  def vsub16_2 : SubRegIndex<16, 32>;
  def vsub16_3 : SubRegIndex<16, 48>;
  def vsub16_4 : SubRegIndex<16, 64>;
  def vsub16_5 : SubRegIndex<16, 80>;
  def vsub16_6 : SubRegIndex<16, 96>;
  def vsub16_7 : SubRegIndex<16, 108>;

  def qsub_0 : SubRegIndex<8>;
  def qsub_1 : SubRegIndex<8,8>;
  def qsub_2 : SubRegIndex<8,16>;
  def qsub_3 : SubRegIndex<8,24>;

  def hsub_0 : SubRegIndex<16>;
  def hsub_1 : SubRegIndex<16,16>;

  def vsub32_0 : SubRegIndex<32>;
  def vsub32_1 : SubRegIndex<32, 32>;
}


// Quarters of the IRF32 registers [i8]
foreach nIRF = 0 - 31 in {
  foreach nByte = 0 - 3 in
    def I#nIRF#_q#nByte : SHAVEReg<"i"#nIRF>;
}

// Eighths of the VRF128 registers [i16, f16, v2i8]
foreach nVRF = 0 - 31 in {
  foreach nByte = 0 - 7 in
    def V#nVRF#_e#nByte : SHAVEReg<"v"#nVRF>;
}

// Quarters of the VRF32 registers [i32, f32, v2i16, v2f16, v4i8]
let SubRegIndices = [vsub16_0, vsub16_1], CoveredBySubRegs = 1 in {
  foreach nVRF = 0 - 31 in {
    def V#nVRF#_q0 : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("V"#nVRF#"_e0"), !cast<Register>("V"#nVRF#"_e1")]>;
  }
}

let SubRegIndices = [vsub16_2, vsub16_3], CoveredBySubRegs = 1 in {
  foreach nVRF = 0 - 31 in {
    def V#nVRF#_q1 : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("V"#nVRF#"_e2"), !cast<Register>("V"#nVRF#"_e3")]>;
  }
}

let SubRegIndices = [vsub16_4, vsub16_5], CoveredBySubRegs = 1 in {
  foreach nVRF = 0 - 31 in {
    def V#nVRF#_q2 : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("V"#nVRF#"_e4"), !cast<Register>("V"#nVRF#"_e5")]>;
  }
}

let SubRegIndices = [vsub16_6, vsub16_7], CoveredBySubRegs = 1 in {
  foreach nVRF = 0 - 31 in {
    def V#nVRF#_q3 : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("V"#nVRF#"_e6"), !cast<Register>("V"#nVRF#"_e7")]>;
  }
}

// Lower halves of the IRF32 registers [i16, f16, v2i8]
let SubRegIndices = [qsub_0, qsub_1], CoveredBySubRegs = 1 in {
  foreach nIRF = 0 - 31 in
    def I#nIRF#_l : SHAVERegWithSubregs<"i"#nIRF, [!cast<Register>("I"#nIRF#"_q0"), !cast<Register>("I"#nIRF#"_q1")]>;
}
// Upper halves of the IRF32 registers [i16, f16, v2i8]
let SubRegIndices = [qsub_2, qsub_3], CoveredBySubRegs = 1 in {
  foreach nIRF = 0 - 31 in
    def I#nIRF#_h : SHAVERegWithSubregs<"i"#nIRF, [!cast<Register>("I"#nIRF#"_q2"), !cast<Register>("I"#nIRF#"_q3")]>;
}

// Lower halves of the VRF128 registers [v2i32, v2f32, v4i16, v4f16, v8i8]
let SubRegIndices = [qsub_0, qsub_1] /* , CoveredBySubRegs = 1 */ in {
  foreach nVRF = 0 - 31 in
    def V#nVRF#_l : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("V"#nVRF#"_q0"), !cast<Register>("V"#nVRF#"_q1")]>;
}
// Upper halves of the VRF128 registers [v2i32, v2f32, v4i16, v4f16, v8i8]
let SubRegIndices = [qsub_2, qsub_3] /* , CoveredBySubRegs = 1 */ in {
  foreach nVRF = 0 - 31 in
    def V#nVRF#_h : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("V"#nVRF#"_q2"), !cast<Register>("V"#nVRF#"_q3")]>;
}

// Full width IRF32 registers [i32, f32, v2i16, v2f16, v4i8]
//   Dwarf registers are '0' thru '31'
let SubRegIndices = [hsub_0, hsub_1], CoveredBySubRegs = 1 in {
  foreach nIRF = 0 - 31 in
    def I#nIRF : SHAVERegWithSubregs<"i"#nIRF, [!cast<Register>("I"#nIRF#"_l"), !cast<Register>("I"#nIRF#"_h")]>, DwarfRegNum<[nIRF]>;
}

// Full width VRF128 registers [v4i32, v4f32, v8i16, v8f16m v16i8]
//   Dwarf registers are '64' thru '95'
let SubRegIndices = [hsub_0, hsub_1] /*, CoveredBySubRegs = 1 */ in {
  foreach nVRF = 0 - 31 in
    def V#nVRF : SHAVERegWithSubregs<"v"#nVRF, [!cast<Register>("V"#nVRF#"_l"), !cast<Register>("V"#nVRF#"_h")]>, DwarfRegNum<[!add(64, nVRF)]>;
}

// IRF64 super-registers [i64]
let SubRegIndices = [vsub32_0, vsub32_1], CoveredBySubRegs = 1 in {
  foreach nIRF = 0 - 30 in
    def I#nIRF#_#!add(nIRF, 1) : SHAVERegWithSubregs<"i"#nIRF, [!cast<Register>("I"#nIRF), !cast<Register>("I"#!add(nIRF, 1))]>;
}

/////////////////////////////////////////////////
// TRF registers				               			   //
/////////////////////////////////////////////////

def P_GPR    : SHAVEReg<"P_GPR">,    DwarfRegNum<[96]>;
def P_GPI    : SHAVEReg<"P_GPI">,    DwarfRegNum<[97]>;
def TRF_2    : SHAVEReg<"TRF_2">,    DwarfRegNum<[98]>;
def P_SVID   : SHAVEReg<"P_SVID">,   DwarfRegNum<[99]>;
def P_ID_Myr4: SHAVEReg<"P_ID">,     DwarfRegNum<[99]>;
def P_CFG    : SHAVEReg<"P_CFG">,    DwarfRegNum<[100]>;
def TRF_5    : SHAVEReg<"TRF_5">,    DwarfRegNum<[101]>;
def TRF_6    : SHAVEReg<"TRF_6">,    DwarfRegNum<[102]>;
def TRF_7    : SHAVEReg<"TRF_7">,    DwarfRegNum<[103]>;
def TRF_8    : SHAVEReg<"TRF_8">,    DwarfRegNum<[104]>;
def TRF_9    : SHAVEReg<"TRF_9">,    DwarfRegNum<[105]>;
def G_GALOIS : SHAVEReg<"G_GALOIS">, DwarfRegNum<[106]>;
def B_SREPS  : SHAVEReg<"B_SREPS">,  DwarfRegNum<[107]>;

// C_CMU0 is:                                     108
// C_CMU1 is:                                     109
// C_CSI is:                                      110

def F_AE     : SHAVEReg<"F_AE">,     DwarfRegNum<[111]>;
def I_AE     : SHAVEReg<"I_AE">,     DwarfRegNum<[112]>;
def TRF_17   : SHAVEReg<"TRF_17">,   DwarfRegNum<[113]>;
def V_ACC3   : SHAVEReg<"V_ACC3">,   DwarfRegNum<[114]>;
def V_ACC2   : SHAVEReg<"V_ACC2">,   DwarfRegNum<[115]>;
def V_ACC1   : SHAVEReg<"V_ACC1">,   DwarfRegNum<[116]>;
def V_ACC0   : SHAVEReg<"V_ACC0">,   DwarfRegNum<[117]>;
def S_ACC    : SHAVEReg<"S_ACC">,    DwarfRegNum<[118]>;
def V_STATE  : SHAVEReg<"V_STATE">,  DwarfRegNum<[119]>;
def S_STATE  : SHAVEReg<"S_STATE">,  DwarfRegNum<[120]>;
def I_STATE  : SHAVEReg<"I_STATE">,  DwarfRegNum<[121]>;
def B_RFB    : SHAVEReg<"B_RFB">,    DwarfRegNum<[122]>;
def B_STATE  : SHAVEReg<"B_STATE">,  DwarfRegNum<[123]>;
def B_MREPS  : SHAVEReg<"B_MREPS">,  DwarfRegNum<[124]>;
def B_LEND   : SHAVEReg<"B_LEND">,   DwarfRegNum<[125]>;
def B_LBEG   : SHAVEReg<"B_LBEG">,   DwarfRegNum<[126]>;
def B_IP_0   : SHAVEReg<"B_IP_0">,   DwarfRegNum<[127]>;
def ISR_SA   : SHAVEReg<"ISR_SA">,   DwarfRegNum<[128]>;
def B_ISR_SA : SHAVEReg<"B_ISR_SA">, DwarfRegNum<[128]>; // ISR_SA was renamed for NPU4
def B_CFG    : SHAVEReg<"B_CFG">,    DwarfRegNum<[129]>;

// FIXME: Decide on the dwarf reg numbers for new accumulators in myriad2.3. 
// For now reusing the values for deprecated SRF regs (S0 through S9)
def S_ACC0    : SHAVEReg<"S_ACC0">,    DwarfRegNum<[32]>;
def S_ACC1    : SHAVEReg<"S_ACC1">,    DwarfRegNum<[33]>;
def V_ACC00   : SHAVEReg<"V_ACC00">,   DwarfRegNum<[34]>;
def V_ACC01   : SHAVEReg<"V_ACC01">,   DwarfRegNum<[35]>;
def V_ACC02   : SHAVEReg<"V_ACC02">,   DwarfRegNum<[36]>;
def V_ACC03   : SHAVEReg<"V_ACC03">,   DwarfRegNum<[37]>;
def V_ACC10   : SHAVEReg<"V_ACC10">,   DwarfRegNum<[38]>;
def V_ACC11   : SHAVEReg<"V_ACC11">,   DwarfRegNum<[39]>;
def V_ACC12   : SHAVEReg<"V_ACC12">,   DwarfRegNum<[40]>;
def V_ACC13   : SHAVEReg<"V_ACC13">,   DwarfRegNum<[41]>;

def CC_SAU0  : SHAVEReg<"C_SAU0">;
def CC_SAU1  : SHAVEReg<"C_SAU1">;
def CC_SAU2  : SHAVEReg<"C_SAU2">;
def CC_SAU3  : SHAVEReg<"C_SAU3">;
def CC_IAU0  : SHAVEReg<"C_IAU0">;

def CC_CMU0  : SHAVEReg<"C_CMU0">;
def CC_CMU1  : SHAVEReg<"C_CMU1">;
def CC_CMU2  : SHAVEReg<"C_CMU2">;
def CC_CMU3  : SHAVEReg<"C_CMU3">;
def CC_CMU4  : SHAVEReg<"C_CMU4">;
def CC_CMU5  : SHAVEReg<"C_CMU5">;
def CC_CMU6  : SHAVEReg<"C_CMU6">;
def CC_CMU7  : SHAVEReg<"C_CMU7">;
def CC_CMU8  : SHAVEReg<"C_CMU8">;
def CC_CMU9  : SHAVEReg<"C_CMU9">;
def CC_CMU10 : SHAVEReg<"C_CMU10">;
def CC_CMU11 : SHAVEReg<"C_CMU11">;
def CC_CMU12 : SHAVEReg<"C_CMU12">;
def CC_CMU13 : SHAVEReg<"C_CMU13">;
def CC_CMU14 : SHAVEReg<"C_CMU14">;
def CC_CMU15 : SHAVEReg<"C_CMU15">;

// CMU sub-register indices
let Namespace = "SHAVE" in
{
  // Note: Code depends on these having consecutive numbers.
  // FIXME: Movidius - I don't know what sizes to gives these guys, so assume 4-bits for now
  def cmu_idx_0   : SubRegIndex<4>;
  def cmu_idx_1   : SubRegIndex<4,4>;
  def cmu_idx_2   : SubRegIndex<4,8>;
  def cmu_idx_3   : SubRegIndex<4,12>;
  def cmu_idx_4   : SubRegIndex<4,16>;
  def cmu_idx_5   : SubRegIndex<4,20>;
  def cmu_idx_6   : SubRegIndex<4,24>;
  def cmu_idx_7   : SubRegIndex<4,28>;
  def cmu_idx_8   : SubRegIndex<4,32>;
  def cmu_idx_9   : SubRegIndex<4,36>;
  def cmu_idx_10  : SubRegIndex<4,40>;
  def cmu_idx_11  : SubRegIndex<4,44>;
  def cmu_idx_12  : SubRegIndex<4,48>;
  def cmu_idx_13  : SubRegIndex<4,52>;
  def cmu_idx_14  : SubRegIndex<4,56>;
  def cmu_idx_15  : SubRegIndex<4,60>;
}

let SubRegIndices = [cmu_idx_0, cmu_idx_1, cmu_idx_2, cmu_idx_3,
                     cmu_idx_4, cmu_idx_5, cmu_idx_6, cmu_idx_7] in
{
  def C_CMU0 : SHAVERegWithSubregs<"C_CMU0",
                 [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3,
                  CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7]>, DwarfRegNum<[108]>;

  def C_CMU1 : SHAVERegWithSubregs<"C_CMU1",
                 [CC_CMU8,  CC_CMU9,  CC_CMU10, CC_CMU11,
                  CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15]>, DwarfRegNum<[109]>;
}


def C_CMU_0_1 : SHAVERegWithSubregs<"C_CMU01", [CC_CMU0, CC_CMU1]>
{
    let SubRegIndices = [cmu_idx_0, cmu_idx_1];
}
def C_CMU_0_3 : SHAVERegWithSubregs<"C_CMU0123", [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3]>
{
    let SubRegIndices = [cmu_idx_0, cmu_idx_1, cmu_idx_2, cmu_idx_3];
}
def C_CMU_0_7 : SHAVERegWithSubregs<"C_CMU01234567",
    [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7]>
{
    let SubRegIndices = [cmu_idx_0, cmu_idx_1, cmu_idx_2, cmu_idx_3,
                         cmu_idx_4, cmu_idx_5, cmu_idx_6, cmu_idx_7];
}
def C_CMU_0_15 : SHAVERegWithSubregs<"C_CMU0123456789ABCDEF",
    [CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5, CC_CMU6, CC_CMU7,
     CC_CMU8,  CC_CMU9,  CC_CMU10, CC_CMU11, CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15]>
{
    let SubRegIndices = [cmu_idx_0, cmu_idx_1, cmu_idx_2, cmu_idx_3,
                         cmu_idx_4, cmu_idx_5, cmu_idx_6, cmu_idx_7,
                         cmu_idx_8, cmu_idx_9, cmu_idx_10, cmu_idx_11,
                         cmu_idx_12, cmu_idx_13, cmu_idx_14, cmu_idx_15];
}

let SubRegIndices = [cmu_idx_0, cmu_idx_1, cmu_idx_2, cmu_idx_3, cmu_idx_4] in
{
  def C_CSI    : SHAVERegWithSubregs<"C_CSI",
                  [CC_SAU0, CC_SAU1, CC_SAU2, CC_SAU3, CC_IAU0]>, DwarfRegNum<[110]>;
}


let Namespace = "SHAVE" in
{
  // FIXME: Movidius - I don't know what values to give these, so assume 32-bits for now
  def vacc_idx_0  : SubRegIndex<32>;
  def vacc_idx_1  : SubRegIndex<32,32>;
  def vacc_idx_2  : SubRegIndex<32,64>;
  def vacc_idx_3  : SubRegIndex<32,96>;
}

let SubRegIndices = [vacc_idx_0, vacc_idx_1, vacc_idx_2, vacc_idx_3] in
{
  // FIXME: Movidius - this has the same physical register number as 'C_SAU0'
  def V_ACC   : SHAVERegWithSubregs<"V_ACC", [V_ACC0, V_ACC1, V_ACC2, V_ACC3]>;

  def V_ACC0x : SHAVERegWithSubregs<"V_ACC0", [V_ACC00, V_ACC01, V_ACC02, V_ACC03]>;
  def V_ACC1x : SHAVERegWithSubregs<"V_ACC1", [V_ACC10, V_ACC11, V_ACC12, V_ACC13]>;
}


/////////////////////////////////////////////////
// Register classes						            	   //
/////////////////////////////////////////////////

def IRF8_q0:  RegisterClass<"SHAVE", [i8], 8,
						(add
             // scratch registers
             I10_q0, I9_q0, I8_q0, I7_q0, I6_q0, I5_q0, I4_q0, I3_q0, I2_q0, I1_q0,

             // scratch + calling convention registers
             I11_q0, I12_q0, I13_q0, I14_q0, I15_q0, I16_q0, I17_q0, I18_q0,

             // preserved registers
             I29_q0, I28_q0, I27_q0, I26_q0, I25_q0, I24_q0, I23_q0, I22_q0, I21_q0, I20_q0,

             // temp register, stack pointer, link pointer. frame pointer
             I0_q0, I19_q0, I30_q0, I31_q0
             )>;

def IRF8_q1:  RegisterClass<"SHAVE", [i8], 8,
            (add
             // scratch registers
             I10_q1, I9_q1, I8_q1, I7_q1, I6_q1, I5_q1, I4_q1, I3_q1, I2_q1, I1_q1,

             // scratch + calling convention registers
             I11_q1, I12_q1, I13_q1, I14_q1, I15_q1, I16_q1, I17_q1, I18_q1,

             // preserved registers
             I29_q1, I28_q1, I27_q1, I26_q1, I25_q1, I24_q1, I23_q1, I22_q1, I21_q1, I20_q1,

             // temp register + stack pointer + link pointer + frame pointer
             I0_q1, I19_q1, I30_q1, I31_q1
             )>;

def IRF8_q2:  RegisterClass<"SHAVE", [i8], 8,
            (add
             // scratch registers
             I10_q2, I9_q2, I8_q2, I7_q2, I6_q2, I5_q2, I4_q2, I3_q2, I2_q2, I1_q2,

             // scratch + calling convention registers
             I11_q2, I12_q2, I13_q2, I14_q2, I15_q2, I16_q2, I17_q2, I18_q2,

             // preserved registers
             I29_q2, I28_q2, I27_q2, I26_q2, I25_q2, I24_q2, I23_q2, I22_q2, I21_q2, I20_q2,

             // temp register + stack pointer + link pointer + frame pointer
             I0_q2, I19_q2, I30_q2, I31_q2
             )>;

def IRF8_q3:  RegisterClass<"SHAVE", [i8], 8,
            (add
             // scratch registers
             I10_q3, I9_q3, I8_q3, I7_q3, I6_q3, I5_q3, I4_q3, I3_q3, I2_q3, I1_q3,

             // scratch + calling convention registers
             I11_q3, I12_q3, I13_q3, I14_q3, I15_q3, I16_q3, I17_q3, I18_q3,

             // preserved registers
             I29_q3, I28_q3, I27_q3, I26_q3, I25_q3, I24_q3, I23_q3, I22_q3, I21_q3, I20_q3,

             // temp register + stack pointer + link pointer + frame pointer
             I0_q3, I19_q3, I30_q3, I31_q3
             )>;

def IRF16_l:  RegisterClass<"SHAVE", [i16, f16, v2i8], 16,
						(add
             // scratch registers
             I10_l, I9_l, I8_l, I7_l, I6_l, I5_l, I4_l, I3_l, I2_l, I1_l,

             // scratch + calling convention registers
             I11_l, I12_l, I13_l, I14_l, I15_l, I16_l, I17_l, I18_l,

             // preserved registers
             I29_l, I28_l, I27_l, I26_l, I25_l, I24_l, I23_l, I22_l, I21_l, I20_l,

             // temp register, stack pointer, link pointer, frame pointer
             I0_l, I19_l, I30_l, I31_l
             )>;

def IRF16_h:  RegisterClass<"SHAVE", [i16, f16, v2i8], 16,
            (add
             // scratch registers
             I10_h, I9_h, I8_h, I7_h, I6_h, I5_h, I4_h, I3_h, I2_h, I1_h,

             // scratch + calling convention registers
             I11_h, I12_h, I13_h, I14_h, I15_h, I16_h, I17_h, I18_h,

             // preserved registers
             I29_h, I28_h, I27_h, I26_h, I25_h, I24_h, I23_h, I22_h, I21_h, I20_h,

             // temp register, stack pointer, link pointer. frame pointer
             I0_h, I19_h, I30_h, I31_h
             )>;

def IRF32 : RegisterClass<"SHAVE", [i32, v4i8, v2i16, f32, v2f16], 32,
						(add
               // scratch registers
               I10, I9, I8, I7, I6, I5, I4, I3, I2, I1,

               // scratch + calling convention registers
               I11, I12, I13, I14, I15, I16, I17, I18,

               // preserved registers
               I29, I28, I27, I26, I25, I24, I23, I22, I21, I20,

               // temp register, stack pointer, link pointer. frame pointer
               I0, I19, I30, I31
             )>;

def IRF64 : RegisterClass<"SHAVE", [i64], 64,
            (add I0_1,   I1_2,   I2_3,   I3_4,   I4_5,   I5_6,   I6_7,   I7_8,   I8_9,   I9_10,
                 I10_11, I11_12, I12_13, I13_14, I14_15, I15_16, I16_17, I17_18,
                 I20_21, I21_22, I22_23, I23_24, I24_25, I25_26, I26_27, I27_28, I28_29
            )>;

def VRF32_q0 : RegisterClass<"SHAVE", [v4i8, v2i16, i32, v2f16, f32], 32,
             (add
			 // scratch registers
             V15_q0, V14_q0, V13_q0, V12_q0, V11_q0, V10_q0, V9_q0, V8_q0, V7_q0, V6_q0, V5_q0, V4_q0, V3_q0, V2_q0, V1_q0, V0_q0,
             // scratch + calling convention registers
             V16_q0, V17_q0, V18_q0, V19_q0, V20_q0, V21_q0, V22_q0, V23_q0,
             // preserved registers
             V24_q0, V25_q0, V26_q0, V27_q0, V28_q0, V29_q0, V30_q0, V31_q0
             )>;

def VRF32_q1 : RegisterClass<"SHAVE", [v4i8, v2i16, i32, v2f16, f32], 32,
             (add
			 // scratch registers
             V15_q1, V14_q1, V13_q1, V12_q1, V11_q1, V10_q1, V9_q1, V8_q1, V7_q1, V6_q1, V5_q1, V4_q1, V3_q1, V2_q1, V1_q1, V0_q1,
             // scratch + calling convention registers
             V16_q1, V17_q1, V18_q1, V19_q1, V20_q1, V21_q1, V22_q1, V23_q1,
             // preserved registers
             V24_q1, V25_q1, V26_q1, V27_q1, V28_q1, V29_q1, V30_q1, V31_q1
             )>;

def VRF32_q2 : RegisterClass<"SHAVE", [v4i8, v2i16, i32, v2f16, f32], 32,
             (add
			 // scratch registers
             V15_q2, V14_q2, V13_q2, V12_q2, V11_q2, V10_q2, V9_q2, V8_q2, V7_q2, V6_q2, V5_q2, V4_q2, V3_q2, V2_q2, V1_q2, V0_q2,
             // scratch + calling convention registers
             V16_q2, V17_q2, V18_q2, V19_q2, V20_q2, V21_q2, V22_q2, V23_q2,
             // preserved registers
             V24_q2, V25_q2, V26_q2, V27_q2, V28_q2, V29_q2, V30_q2, V31_q2
             )>;

def VRF32_q3 : RegisterClass<"SHAVE", [v4i8, v2i16, i32, v2f16, f32], 32,
             (add
			 // scratch registers
             V15_q3, V14_q3, V13_q3, V12_q3, V11_q3, V10_q3, V9_q3, V8_q3, V7_q3, V6_q3, V5_q3, V4_q3, V3_q3, V2_q3, V1_q3, V0_q3,
             // scratch + calling convention registers
             V16_q3, V17_q3, V18_q3, V19_q3, V20_q3, V21_q3, V22_q3, V23_q3,
             // preserved registers
             V24_q3, V25_q3, V26_q3, V27_q3, V28_q3, V29_q3, V30_q3, V31_q3
             )>;

def VRF32_qN :RegisterClass<"SHAVE", [v4i8, v2i16, i32, v2f16, f32], 32,
             (add
             //Scratch registers
             V15_q0, V15_q1, V15_q2, V15_q3, V14_q0, V14_q1, V14_q2, V14_q3,
             V13_q0, V13_q1, V13_q2, V13_q3, V12_q0, V12_q1, V12_q2, V12_q3,
             V11_q0, V11_q1, V11_q2, V11_q3, V10_q0, V10_q1, V10_q2, V10_q3,
             V9_q0, V9_q1, V9_q2, V9_q3, V8_q0, V8_q1, V8_q2, V8_q3,
             V7_q0, V7_q1, V7_q2, V7_q3, V6_q0, V6_q1, V6_q2, V6_q3,
             V5_q0, V5_q1, V5_q2, V5_q3, V4_q0, V4_q1, V4_q2, V4_q3,
             V3_q0, V3_q1, V3_q2, V3_q3, V2_q0, V2_q1, V2_q2, V2_q3,
             V1_q0, V1_q1, V1_q2, V1_q3, V0_q0, V0_q1, V0_q2, V0_q3,

             //  scratch + calling convention registers
             V16_q0, V16_q1, V16_q2, V16_q3, V17_q0, V17_q1, V17_q2, V17_q3,
             V18_q0, V18_q1, V18_q2, V18_q3, V19_q0, V19_q1, V19_q2, V19_q3,
             V20_q0, V20_q1, V20_q2, V20_q3, V21_q0, V21_q1, V21_q2, V21_q3,
             V22_q0, V22_q1, V22_q2, V22_q3, V23_q0, V23_q1, V23_q2, V23_q3,

             //  preserved registers
             V24_q0, V24_q1, V24_q2, V24_q3, V25_q0, V25_q1, V25_q2, V25_q3,
             V26_q0, V26_q1, V26_q2, V26_q3, V27_q0, V27_q1, V27_q2, V27_q3,
             V28_q0, V28_q1, V28_q2, V28_q3, V29_q0, V29_q1, V29_q2, V29_q3,
             V30_q0, V30_q1, V30_q2, V30_q3, V31_q0, V31_q1, V31_q2, V31_q3
          )>;

def VRF32_q0_1 :RegisterClass<"SHAVE", [v4i8, v2i16, i32, v2f16, f32], 32,
             (add
             // Scratch registers
             V15_q0, V15_q1, V14_q0, V14_q1, V13_q0, V13_q1, V12_q0, V12_q1,
             V11_q0, V11_q1, V10_q0, V10_q1, V9_q0,  V9_q1,  V8_q0,  V8_q1,
             V7_q0,  V7_q1,  V6_q0,  V6_q1,  V5_q0,  V5_q1,  V4_q0,  V4_q1,
             V3_q0,  V3_q1,  V2_q0,  V2_q1,  V1_q0,  V1_q1,  V0_q0,  V0_q1,

             // scratch + calling convention registers
             V16_q0, V16_q1, V17_q0, V17_q1, V18_q0, V18_q1, V19_q0, V19_q1,
             V20_q0, V20_q1, V21_q0, V21_q1, V22_q0, V22_q1, V23_q0, V23_q1,

             // preserved registers
             V24_q0, V24_q1, V25_q0, V25_q1, V26_q0, V26_q1, V27_q0, V27_q1,
             V28_q0, V28_q1, V29_q0, V29_q1, V30_q0, V30_q1, V31_q0, V31_q1
          )>;

foreach n = 0 - 7 in {
  def VRF16_e#n : RegisterClass<"SHAVE", [i16, f16, v2i8], 16,
                        (add
			            // scratch registers
                        !cast<Register>("V15_e"#n), !cast<Register>("V14_e"#n), !cast<Register>("V13_e"#n), 
                        !cast<Register>("V12_e"#n), !cast<Register>("V11_e"#n), !cast<Register>("V10_e"#n), 
                        !cast<Register>("V9_e"#n),  !cast<Register>("V8_e"#n),  !cast<Register>("V7_e"#n),
                        !cast<Register>("V6_e"#n),  !cast<Register>("V5_e"#n),  !cast<Register>("V4_e"#n), 
                        !cast<Register>("V3_e"#n),  !cast<Register>("V2_e"#n),  !cast<Register>("V1_e"#n), 
                        !cast<Register>("V0_e"#n),
                        // scratch + calling convention registers
                        !cast<Register>("V16_e"#n), !cast<Register>("V17_e"#n), !cast<Register>("V18_e"#n),
                        !cast<Register>("V19_e"#n), !cast<Register>("V20_e"#n), !cast<Register>("V21_e"#n),
                        !cast<Register>("V22_e"#n), !cast<Register>("V23_e"#n),
                        // preserved registers
                        !cast<Register>("V24_e"#n), !cast<Register>("V25_e"#n), !cast<Register>("V26_e"#n),
                        !cast<Register>("V27_e"#n), !cast<Register>("V28_e"#n), !cast<Register>("V29_e"#n),
                        !cast<Register>("V30_e"#n), !cast<Register>("V31_e"#n)
                        )>;
}

def VRF64_h : RegisterClass<"SHAVE", [v8i8, v4i16, v2i32, v2f32, v4f16], 64,
             (add
			 // scratch registers
             V15_h, V14_h, V13_h, V12_h, V11_h, V10_h, V9_h, V8_h, V7_h, V6_h, V5_h, V4_h, V3_h, V2_h, V1_h, V0_h,
             // scratch + calling convention registers
             V16_h, V17_h, V18_h, V19_h, V20_h, V21_h, V22_h, V23_h,
             // preserved registers
             V24_h, V25_h, V26_h, V27_h, V28_h, V29_h, V30_h, V31_h
             )>;

def VRF64_l : RegisterClass<"SHAVE", [v8i8, v4i16, v2i32, v2f32, v4f16], 64,
            (add
			 // scratch registers
             V15_l, V14_l, V13_l, V12_l, V11_l, V10_l, V9_l, V8_l, V7_l, V6_l, V5_l, V4_l, V3_l, V2_l, V1_l, V0_l,
             // scratch + calling convention registers
             V16_l, V17_l, V18_l, V19_l, V20_l, V21_l, V22_l, V23_l,
             // preserved registers
             V24_l, V25_l, V26_l, V27_l, V28_l, V29_l, V30_l, V31_l
             )>;

def VRF128 : RegisterClass<"SHAVE", [v16i8, v8i16, v4i32, v4f32, v8f16], 128,
            (add
			 // scratch registers
             V15, V14, V13, V12, V11, V10, V9, V8, V7, V6, V5, V4, V3, V2, V1, V0, 
             // scratch + calling convention registers
             V16, V17, V18, V19, V20, V21, V22, V23,
             // preserved registers
             V24, V25, V26, V27, V28, V29, V30, V31
             )>;

def TRF : RegisterClass<"SHAVE", [i32], 32,
             (add
			 P_GPR, P_GPI, TRF_2, P_SVID, P_ID_Myr4, P_CFG, TRF_5, TRF_6, TRF_7, TRF_8,
             TRF_9, G_GALOIS, B_SREPS, C_CMU1, C_CMU0, C_CSI, F_AE, I_AE,
             TRF_17, V_ACC3, V_ACC2, V_ACC1, V_ACC0, S_ACC, V_STATE, S_STATE, I_STATE,
             // TODO: need to separate out the old and new accumulator sets
             V_ACC00, V_ACC01, V_ACC02, V_ACC03,V_ACC10, V_ACC11, V_ACC12, V_ACC13, S_ACC0, S_ACC1,
             B_RFB, B_STATE, B_MREPS, B_LEND, B_LBEG, B_IP_0, ISR_SA, B_ISR_SA, B_CFG
             )>;


def PEU_TRF : RegisterClass<"SHAVE", [i8], 8,
			 (add
			 CC_CMU0, CC_CMU1, CC_CMU2, CC_CMU3, CC_CMU4, CC_CMU5,
			 CC_CMU6, CC_CMU7, CC_CMU8, CC_CMU9, CC_CMU10, CC_CMU11,
			 CC_CMU12, CC_CMU13, CC_CMU14, CC_CMU15,
			 CC_SAU0, CC_SAU1, CC_SAU2, CC_SAU3,
			 CC_IAU0
			 )>;

def Combined_TRF : RegisterClass<"SHAVE", [i8], 8,
                   (add C_CMU_0_3, C_CMU_0_7, C_CMU_0_15)>;

def TRF_ACC32 : RegisterClass<"SHAVE", [i32, f32, v4i8, v2i16, v2f16], 32, (add S_ACC, S_ACC0, S_ACC1)>;
def TRF_ACC128 : RegisterClass<"SHAVE", [v16i8, v8i16, v4i32, v4f32, v8f16], 128, (add V_ACC, V_ACC0x, V_ACC1x)>;

include "SHAVEv3/SHAVEv3RegisterInfo.td"
