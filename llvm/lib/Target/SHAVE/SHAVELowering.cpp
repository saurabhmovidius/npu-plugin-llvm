//===-- SHAVELowering.cpp - Instruction Selection ---------------*- C++ -*-===//
//
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
//
//===----------------------------------------------------------------------===//
//
// SHAVE Target Instruction lowering implementation
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "shave-lowering"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/ConstantFold.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsShave.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVELowering.h"
#include "SHAVEMachineFunctionInfo.h"
#include "SHAVERegisterInfo.h"
#include "SHAVESubtarget.h"
#include "SHAVETargetMachine.h"
#include "SHAVETargetObjectFile.h"

using namespace llvm;
#include "SHAVEGenCallingConv.inc"


using namespace SHAVEOptions;


SHAVELowering::SHAVELowering(const SHAVETargetMachine &STM)
: TargetLowering(STM),
  SHAVEST(*STM.getSubtargetImpl())
{}

void SHAVELowering::initialiseLoweringActions(const SHAVETargetMachine &STM) {
  MVT pointerVT = MVT::getIntegerVT(8 * STM.getPointerSize(0));

  // Set this to 0 because we want to generate our own  memcpy, memset and memmove code, in SHAVESelectionDAGInfo
  MaxStoresPerMemcpy = 0;
  MaxStoresPerMemmove = 0;
  MaxStoresPerMemset = 0;

  setJumpIsExpensive(true); // FIXME: Movidius - not sure this is true when branch delay slots are well filled

  //  setPrefFunctionAlignment(AlignFunctionLabels ? llvm::Log2_32(AlignFunctionLabels) : 4);
  setPrefFunctionAlignment(Align(AlignFunctionLabels ? AlignFunctionLabels : 16));
  setMinFunctionAlignment(Align());
  //  setPrefLoopAlignment(AlignLoopLabels ? llvm::Log2_32(AlignLoopLabels) : 4);
  setPrefLoopAlignment(Align(AlignLoopLabels ? AlignLoopLabels : 16));

  // FIXME: Movidius - should these both be the same value? Will this cause problems if converting
  //                   between scalar and vector boolean types?
  setBooleanContents(ZeroOrOneBooleanContent);
  setBooleanVectorContents(ZeroOrNegativeOneBooleanContent);

  setSchedulingPreference(Sched::RegPressure);
  setStackPointerRegisterToSaveRestore(SHAVERegisterInfo::getSPReg());
  //setMinStackArgumentAlignment(0);

  // Attach the register classes
  addRegisterClass(MVT::i64, &SHAVE::IRF64RegClass);
  addRegisterClass(MVT::i32, &SHAVE::IRF32RegClass);
  addRegisterClass(MVT::i16, &SHAVE::IRF16_lRegClass);
  addRegisterClass(MVT::i8, &SHAVE::IRF8_q0RegClass);

  if (SHAVEST.hasFPU()) {
    addRegisterClass(MVT::f32, &SHAVE::IRF32RegClass);
    addRegisterClass(MVT::f16, &SHAVE::IRF16_lRegClass);
  }

  if (hasFeature(SHAVE::HasVRF512_Feature)) {
    SHAVEv3Lowering();
  } else if (SHAVEST.hasVAU()) {
    // 128-bit Vector register classes
    addRegisterClass(MVT::v16i8, &SHAVE::VRF128RegClass);
    addRegisterClass(MVT::v8i16, &SHAVE::VRF128RegClass);
    addRegisterClass(MVT::v4i32, &SHAVE::VRF128RegClass);

    if (SHAVEST.hasFPU()) {
      addRegisterClass(MVT::v8f16, &SHAVE::VRF128RegClass);
      addRegisterClass(MVT::v4f32, &SHAVE::VRF128RegClass);
    }

    // 64-bit Vector register classes
    addRegisterClass(MVT::v8i8, &SHAVE::VRF64_lRegClass);
    addRegisterClass(MVT::v4i16, &SHAVE::VRF64_lRegClass);
    addRegisterClass(MVT::v2i32, &SHAVE::VRF64_lRegClass);

    if (SHAVEST.hasFPU()) {
      addRegisterClass(MVT::v4f16, &SHAVE::VRF64_lRegClass);
      addRegisterClass(MVT::v2f32, &SHAVE::VRF64_lRegClass);
    }
  }


  // FIXME: Movidius - There are problems with enabling SAU vector support without VRFs
  //                   since some VAU instructions will be automatically selected for 
  //                   certain 32-bit vector operations. Once these have been resolved
  //                   the hasVAU check may be removed to re-enable SAU vector support.
  if (SHAVEST.hasSAU() && SHAVEST.hasVAU()) {
    // 32-bit Vector register classes
    addRegisterClass(MVT::v4i8, &SHAVE::IRF32RegClass);
    addRegisterClass(MVT::v2i16, &SHAVE::IRF32RegClass);

    if (SHAVEST.hasFPU())
      addRegisterClass(MVT::v2f16, &SHAVE::IRF32RegClass);

    // 16-bit Vector register classes
    addRegisterClass(MVT::v2i8, &SHAVE::IRF16_lRegClass);
  }

  computeRegisterProperties(SHAVEST.getRegisterInfo());

  InitializeScalarLoweringActions();
  InitializeVectorLoweringActions();
  if (hasFeature(SHAVE::HasVRF512_Feature))
    InitializeSHAVEv3LoweringActions();

  InitializeLibCallLoweringActions();

  // Handle FRAMEADDR and RETURNADDR
  setOperationAction(ISD::RETURNADDR, pointerVT, Custom);
  setOperationAction(ISD::FRAMEADDR, pointerVT, Custom);

  // Handle dynamic stack
  if (!SHAVEOptions::StackOverflowChecking && !SHAVEOptions::TrackStackUsage)
    setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i32, Expand);
  else
    setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i32, Custom);

  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);

  // FIXME: Movidius_v3 - Eventually these should be re-enabled for Myriad4 but
  //                      right now shuffle patterns are expanded to insert/extract
  //                      sequences resulting in a legalization loop
  if (SHAVEST.hasVAU() && hasFeature(SHAVE::HasVRF128_Feature)) {
    setTargetDAGCombine(ISD::INSERT_VECTOR_ELT);
    setTargetDAGCombine(ISD::EXTRACT_VECTOR_ELT);
    setTargetDAGCombine(ISD::BUILD_VECTOR);
  }
  setTargetDAGCombine(ISD::SELECT_CC);
  setTargetDAGCombine(ISD::VSELECT);
  setTargetDAGCombine(ISD::SDIV);
  setTargetDAGCombine(ISD::SREM);
  setTargetDAGCombine(ISD::SINT_TO_FP);
  setTargetDAGCombine(ISD::UINT_TO_FP);

  // Setup the ACC/MAC lowering actions if enabled
  if (EnableFPAccMac) {
    setTargetDAGCombine(ISD::FADD);
    setTargetDAGCombine(ISD::FSUB);
  }
  if (EnableINTAccMac){
    setTargetDAGCombine(ISD::ADD);
    setTargetDAGCombine(ISD::SUB);
  }
}

SHAVELowering::~SHAVELowering() {}

bool SHAVELowering::hasFeature(unsigned int feature) const {
  return SHAVEST.hasFeature(feature);
}

MVT SHAVELowering::getScalarShiftAmountTy(const DataLayout &DL, EVT LHSTy) const {
  if (LHSTy.isSimple())
    return LHSTy.getSimpleVT();
  else
    return MVT::i32;
}

bool SHAVELowering::isFNegFree(EVT VT) const {
  EVT simpleVT = VT.getScalarType();
  return ((simpleVT == MVT::f16) || (simpleVT == MVT::f32));
}

bool SHAVELowering::isFAbsFree(EVT VT) const {
  EVT simpleVT = VT.getScalarType();
  return ((simpleVT == MVT::f16) || (simpleVT == MVT::f32));
}

bool SHAVELowering::allowsMisalignedMemoryAccesses(
    EVT, unsigned AddrSpace, Align Alignment, MachineMemOperand::Flags Flags,
    unsigned *Fast) const {
  // SHAVE supports unaligned mem access
  if (Fast)
#if 0 // FIXME: Movidius - enable this block and measure performance changes
    // What exactly does LLVM mean by 'fast'?
    *Fast = 0;
#else
    *Fast = 1;
#endif

  return true;
}

void SHAVELowering::InitializeScalarLoweringActions() {
  // Resolve LOAD
  setOperationAction(ISD::LOAD, MVT::i1, Promote);

  if (UseLDI) {
    // Handle post incrementing loads and stores
    setIndexedLoadAction(ISD::POST_INC, MVT::i32, Legal);
    setIndexedLoadAction(ISD::POST_INC, MVT::i16, Legal);
    setIndexedLoadAction(ISD::POST_INC, MVT::i8, Legal);
    setIndexedLoadAction(ISD::POST_INC, MVT::f16, Legal);
    setIndexedLoadAction(ISD::POST_INC, MVT::f32, Legal);
  }

  setIndexedStoreAction(ISD::POST_INC, MVT::i32, Legal);
  setIndexedStoreAction(ISD::POST_INC, MVT::i16, Legal);
  setIndexedStoreAction(ISD::POST_INC, MVT::i8, Legal);
  setIndexedStoreAction(ISD::POST_INC, MVT::f16, Legal);
  setIndexedStoreAction(ISD::POST_INC, MVT::f32, Legal);

  // Disable all atomics
  // FIXME: Movidius - Why are we disabling atomics? Surely we can do atomic operations for 'i1' and 'i8',
  //        and aligned accesses to 'i16', 'i32', 'f16' and 'f32'?  Should we use 'LibCall' instead of 'Expand'?
  for (unsigned opcode = ISD::ATOMIC_FENCE; opcode <= ISD::ATOMIC_LOAD_UMAX; opcode++)
    for (auto type : { MVT::i1, MVT::i8, MVT::i16, MVT::i32, MVT::i64,
                                         MVT::f16, MVT::f32, MVT::f64 })
      setOperationAction(opcode, type, /* LibCall */ Expand);

  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::EXTLOAD,  VT, MVT::i1, Promote);
  }
  for (MVT VT : MVT::fp_valuetypes())
    setLoadExtAction(ISD::EXTLOAD, VT, MVT::f16, Expand);
  setTruncStoreAction(MVT::f32, MVT::f16, Expand);

  if (hasFeature(SHAVE::HasVRF128_Feature)) {
    setLoadExtAction(ISD::SEXTLOAD, MVT::v4i32, MVT::v4i16, Legal);
  }

  setOperationAction(ISD::PREFETCH, MVT::Other, Legal);

  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i32, Expand);

  // Handle unsigned integer extensions
  setOperationAction(ISD::ZERO_EXTEND, MVT::i1, Expand);
  setOperationAction(ISD::ZERO_EXTEND, MVT::i16, Expand);

  // Handle GlobalAddress and Jump tables
  setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);
  setOperationAction(ISD::JumpTable, MVT::i32, Custom);

  // Promote CTLZ
  setOperationAction(ISD::CTLZ, MVT::i8, Custom);
  setOperationAction(ISD::CTLZ, MVT::i16, Custom);
  setOperationAction(ISD::CTLZ, MVT::i32, Custom);
  setOperationAction(ISD::CTLZ, MVT::i64, Expand);
  setOperationAction(ISD::CTTZ, MVT::i8, Custom);
  setOperationAction(ISD::CTTZ, MVT::i16, Custom);
  setOperationAction(ISD::CTTZ, MVT::i32, Custom);
  setOperationAction(ISD::CTTZ, MVT::i64, Expand);

  // Expand CTLZ_ZERO_UNDEF
  setOperationAction(ISD::CTLZ_ZERO_UNDEF, MVT::i8, Custom);
  setOperationAction(ISD::CTLZ_ZERO_UNDEF, MVT::i16, Custom);
  setOperationAction(ISD::CTLZ_ZERO_UNDEF, MVT::i32, Custom);
  setOperationAction(ISD::CTTZ_ZERO_UNDEF, MVT::i8, Custom);
  setOperationAction(ISD::CTTZ_ZERO_UNDEF, MVT::i16, Custom);
  setOperationAction(ISD::CTTZ_ZERO_UNDEF, MVT::i32, Legal);

  // Promote CTPOP
  setOperationAction(ISD::CTPOP, MVT::i8, Promote);
  setOperationAction(ISD::CTPOP, MVT::i16, Promote);
  setOperationAction(ISD::CTPOP, MVT::i64, Expand);

  // Expand BSWAP
  // FIXME: Movidius - why have we got 'i64' handling here?
  setOperationAction(ISD::BSWAP, MVT::i64, Expand);

  if (!SHAVEST.hasSAU()) {
    setOperationAction(ISD::BSWAP, MVT::i32, Expand);
    setOperationAction(ISD::BSWAP, MVT::i16, Expand);
  }
  // SHAVE cannot handle BB labels, only their addresses
  // setOperationAction(ISD::BasicBlock, MVT::Other, Custom);
  setOperationAction(ISD::BlockAddress, MVT::i32, Custom);

  // Handle jump tables
  setOperationAction(ISD::BR_JT, MVT::Other, Custom);

  // No sign toggle for FPs
  // FIXME: Movidius - why not?
  setOperationAction(ISD::FNEG, MVT::f32, Expand);
  // FIXME: Movidius - implement these using FEXT/FINS
  setOperationAction(ISD::FCOPYSIGN, MVT::f32, Expand);
  setOperationAction(ISD::FCOPYSIGN, MVT::f16, Expand);

  setOperationAction(ISD::SINT_TO_FP, MVT::i1, Promote);
  setOperationAction(ISD::SINT_TO_FP, MVT::i8, Legal);

  // Handle I->F conversions
  setOperationAction(ISD::UINT_TO_FP, MVT::f32, Custom);
  setOperationAction(ISD::UINT_TO_FP, MVT::i32, Legal);

  // Handle FP->INT conversions
  setOperationAction(ISD::FP_TO_UINT, MVT::i8, Legal);
  setOperationAction(ISD::FP_TO_UINT, MVT::i16, Promote);
  setOperationAction(ISD::FP_TO_UINT, MVT::i32, Legal);
  //setOperationAction(ISD::FP_TO_UINT, MVT::f32, Custom);

  // No inreg fp_round
  //  setOperationAction(ISD::FP_ROUND_INREG, MVT::f16, Expand);

  // Handle MULHU (multiply part for unsigned long multiplication)
  setOperationAction(ISD::MULHU, MVT::i32, Expand);
  setOperationAction(ISD::UMULO, MVT::i32, Expand);
  setOperationAction(ISD::SMULO, MVT::i32, Expand);
  //setOperationAction(ISD::UMUL_LOHI, MVT::i32, Expand);
  //setOperationAction(ISD::SMUL_LOHI, MVT::i32, Expand);

  // Handle right shift (SRA and SRL) for i8 and i16
  setOperationAction(ISD::SRL, MVT::i1, Promote);
  setOperationAction(ISD::SRA, MVT::i1, Promote);
  setOperationAction(ISD::SRA, MVT::i8, Legal);
  setOperationAction(ISD::SRA, MVT::i16, Legal);

  // Handle barrel-shifting (ROTL and ROTR)
  setOperationAction(ISD::ROTL, MVT::i1, Promote);
  setOperationAction(ISD::ROTL, MVT::i8, Expand);
  setOperationAction(ISD::ROTL, MVT::i16, Expand);
  setOperationAction(ISD::ROTR, MVT::i1, Promote);
  setOperationAction(ISD::ROTR, MVT::i8, Expand);
  setOperationAction(ISD::ROTR, MVT::i16, Expand);
  setOperationAction(ISD::ROTR, MVT::i32, Expand);

  // valueTypeActions.setTypeAction(MVT::i64, Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i32, Expand);//Custom);
  setOperationAction(ISD::SRL_PARTS, MVT::i32, Expand);//Custom);
  setOperationAction(ISD::SHL_PARTS, MVT::i32, Expand);//Custom);
  setOperationAction(ISD::SMUL_LOHI, MVT::i32, Custom);
  setOperationAction(ISD::UMUL_LOHI, MVT::i32, Custom);

  // Custom lowering for add to capture "horizontal" adds of vectors
  for (auto type : { MVT::i32, MVT::i16, MVT::i8 })
    for (auto op : { ISD::ADD, ISD::AND, ISD::OR, ISD::XOR }) {
      setOperationAction(op, type, Custom);
    }

  // Handle rem and div
  LegalizeAction actionLegalOrPromote = SHAVEST.hasSAU() ? Legal : Promote;
  LegalizeAction actionCustomOrPromote = SHAVEST.hasSAU() ? Custom : Promote;
  for (auto op : { ISD::UREM, ISD::UDIV, ISD::SREM, ISD::SDIV }) {
    setOperationAction(op, MVT::i8, Promote);
    setOperationAction(op, MVT::i16, actionLegalOrPromote);
  }

  LegalizeAction actionLegalOrLibCall = SHAVEST.hasSAU() ? Legal : LibCall;
  LegalizeAction actionCustomOrLibCall = SHAVEST.hasSAU() ? Custom : LibCall;
  setOperationAction(ISD::FADD, MVT::f32, actionCustomOrLibCall);
  setOperationAction(ISD::FSUB, MVT::f32, actionLegalOrLibCall);
  setOperationAction(ISD::FDIV, MVT::f32, actionLegalOrLibCall);
  setOperationAction(ISD::FMUL, MVT::f32, actionLegalOrLibCall);
  setOperationAction(ISD::FADD, MVT::f16, actionCustomOrPromote);
  setOperationAction(ISD::FSUB, MVT::f16, actionLegalOrPromote);
  setOperationAction(ISD::FDIV, MVT::f16, actionLegalOrPromote);
  setOperationAction(ISD::FMUL, MVT::f16, actionLegalOrPromote);

  setOperationAction(ISD::UREM, MVT::i32, actionLegalOrLibCall);
  setOperationAction(ISD::SREM, MVT::i32, actionLegalOrLibCall);
  setOperationAction(ISD::UDIV, MVT::i32, actionLegalOrLibCall);
  setOperationAction(ISD::SDIV, MVT::i32, actionLegalOrLibCall);


  for (auto type : { MVT::i8, MVT::i16, MVT::i32 }) {
    setOperationAction(ISD::UDIVREM, type, Expand);
    setOperationAction(ISD::SDIVREM, type, Expand);
  }

  // MIN/MAX convert to equivalent SHAVEISD opcodes
  for (MVT VT : { MVT::i8, MVT::i16, MVT::i32 } ) {
    setOperationAction(ISD::SMIN, VT, Custom);
    setOperationAction(ISD::UMIN, VT, Custom);
    setOperationAction(ISD::SMAX, VT, Custom);
    setOperationAction(ISD::UMAX, VT, Custom);
  }

  // Resolve SELECT_CC (Expand to SETCC and SELECT)
  setOperationAction(ISD::SELECT_CC, MVT::i1, Promote);
  setOperationAction(ISD::SELECT_CC, MVT::i8, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i16, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i32, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::f32, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::f16, Expand);

  // Resolve SELECT, SETCC
  for (auto op : { ISD::SELECT, ISD::SETCC }) {
    setOperationAction(op, MVT::i1, Promote);
    setOperationAction(op, MVT::i8, Custom);
    setOperationAction(op, MVT::i16, Custom);
    setOperationAction(op, MVT::i32, Custom);
    setOperationAction(op, MVT::f32, Custom);
    setOperationAction(op, MVT::f16, Custom);
  }

  // Custom Ordered Not Equal as all FP comparisons on SHAVE implicitly check for NaN
  // such that NE will always return true if either value is NaN.
  // The custom lowering is effectively an Expand operation but when set to Expand,
  // the DAGLegalizer can end up in a situation where it needs to invert a BR_CC
  // to match the expanded codes which it cannot do
  setCondCodeAction(ISD::SETONE, MVT::f32, Custom);
  setCondCodeAction(ISD::SETONE, MVT::f16, Custom);

  // The same is true for all Unordered comparisons (except for NE in this case)
  for (auto CC : { ISD::SETUEQ, ISD::SETUGT, ISD::SETUGE, ISD::SETULT, ISD::SETULE }) {
    setCondCodeAction(CC, MVT::f32, Custom);
    setCondCodeAction(CC, MVT::f16, Custom);
  }

  if (!hasFeature(SHAVE::HasPC1COrdered_Feature)) {
    setCondCodeAction(ISD::SETO, MVT::f32, Custom);
    setCondCodeAction(ISD::SETO, MVT::f16, Custom);
  }

  setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);

  // Resolve BRCOND (conditional branch)
  setOperationAction(ISD::BRCOND, MVT::i1, Expand);
  setOperationAction(ISD::BRCOND, MVT::i8, Expand);
  setOperationAction(ISD::BRCOND, MVT::i16, Expand);
  setOperationAction(ISD::BRCOND, MVT::i32, Expand);
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);

  // Resolve BR_CC (branch with condition (arithmetic))
  setOperationAction(ISD::BR_CC, MVT::i1, Promote);
  setOperationAction(ISD::BR_CC, MVT::i8, Custom);
  setOperationAction(ISD::BR_CC, MVT::i16, Custom);
  setOperationAction(ISD::BR_CC, MVT::i32, Custom);
  setOperationAction(ISD::BR_CC, MVT::f32, Custom);
  setOperationAction(ISD::BR_CC, MVT::f16, Custom);

  // Handle floating point common functions
  unsigned supportedElementaryFunctions[] = { ISD::FSQRT, ISD::FSIN,  ISD::FCOS, ISD::FLOG2, ISD::FEXP2 };

  for (auto function : supportedElementaryFunctions) {
    setOperationAction(function, MVT::f16, Legal);
    setOperationAction(function, MVT::f32, Expand);
  }

  setOperationAction(ISD::FSIN, MVT::f16, LibCall);
  setOperationAction(ISD::FCOS, MVT::f16, LibCall);

  setOperationAction(ISD::FROUND, MVT::f16, Expand);
  setOperationAction(ISD::FROUND, MVT::f32, Custom);

  // Handle 'fabs'
  // FIXME: Movidius - is it every necessary to explictly say an action is "Legal"?
  setOperationAction(ISD::FABS, MVT::f16, SHAVEST.hasSAU() ? Legal : Expand);
  setOperationAction(ISD::FABS, MVT::f32, SHAVEST.hasSAU() ? Legal : Expand);

  setOperationAction(ISD::FMINNUM, MVT::f32, Custom);
  setOperationAction(ISD::FMINNUM, MVT::f16, Custom);
  setOperationAction(ISD::FMAXNUM, MVT::f32, Custom);
  setOperationAction(ISD::FMAXNUM, MVT::f16, Custom);
  // Other variants of min/max with requirements for +/-0 that do not match CMU.MIN/MAX
  setOperationAction(ISD::FMINIMUM, MVT::f32, Custom);
  setOperationAction(ISD::FMINIMUM, MVT::f16, Custom);
  setOperationAction(ISD::FMAXIMUM, MVT::f32, Custom);
  setOperationAction(ISD::FMAXIMUM, MVT::f16, Custom);
  
  // Handle floating-point operations which should be promoted/expanded to libcalls for f16 and f32
  unsigned expandPromoteFloatingPointFunctions[] = { ISD::FRINT, ISD::FPOWI };

  for (auto function : expandPromoteFloatingPointFunctions) {
    setOperationAction(function, MVT::f16, Promote);
    setOperationAction(function, MVT::f32, Expand);
  }

  // Handle floating-point operations which should be expanded to libcalls for f16 and f32
  unsigned expandFloatingPointFunctions[] = { ISD::FNEG, ISD::FREM, ISD::FSINCOS, ISD::FFLOOR,
                                              ISD::FCEIL, ISD::FMA, ISD::FLOG,
                                              ISD::FPOW, ISD::FLOG10, ISD::FEXP, ISD::FNEARBYINT };

  for (auto function : expandFloatingPointFunctions) {
    setOperationAction(function, MVT::f16, Expand);
    setOperationAction(function, MVT::f32, Expand);
  }

  // FIXME: Movidius - should we also handle 'f16'?  And 'i8', 'i16' and 'i64' for efficiency?
  setOperationAction(ISD::ConstantPool, MVT::i32, Custom);
  setOperationAction(ISD::ConstantPool, MVT::f32, Custom);

  setOperationAction(ISD::INTRINSIC_W_CHAIN, MVT::Other, Custom);
  setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::Other, Custom);
  setOperationAction(ISD::INTRINSIC_VOID, MVT::Other, Custom);

  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VACOPY, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Custom);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);

  // Handle FP constants
  setOperationAction(ISD::ConstantFP, MVT::f16, Legal);
  setOperationAction(ISD::ConstantFP, MVT::f32, Legal);

  // i64 integer operations that need expansion
  for (unsigned i = ISD::ADD; i <= ISD::UMULO; ++i)
    setOperationAction(i, MVT::i64, Expand);

  for (unsigned i = ISD::SCALAR_TO_VECTOR; i <= ISD::FP_TO_UINT; ++i)
    setOperationAction(i, MVT::i64, Expand);

  setOperationAction(ISD::BUILD_PAIR, MVT::i64, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i64, Expand);

  for (unsigned i = ISD::EXTLOAD; i <= ISD::ZEXTLOAD; ++i)
    for (MVT VT : MVT::integer_valuetypes()) {
      // FIXME: Movidius - not 100% sure about this iteration
      setLoadExtAction(i, VT, MVT::i8,  Expand);
      setLoadExtAction(i, VT, MVT::i16, Expand);
      setLoadExtAction(i, VT, MVT::i32, Expand);
      setLoadExtAction(i, VT, MVT::i64, Expand);
    }

  setLoadExtAction(ISD::EXTLOAD, MVT::f32, MVT::f16, Legal);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i16, Legal);
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i32, MVT::i16, Legal);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i8, Legal);
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i32, MVT::i8, Legal);
 
  setTruncStoreAction(MVT::i64, MVT::i8, Expand);
  setTruncStoreAction(MVT::i64, MVT::i16, Expand);
  setTruncStoreAction(MVT::i64, MVT::i32, Expand);

  // i64 integer operations that can be lowered
  auto customI64IntegerOperations = { ISD::ADD, ISD::SUB, ISD::SHL, ISD::SRL, ISD::SRA,
                                      ISD::AND, ISD::OR, ISD::XOR,
                                      ISD::FP_TO_SINT, ISD::FP_TO_UINT, ISD::SINT_TO_FP, ISD::UINT_TO_FP,
                                      ISD::SIGN_EXTEND, ISD::ZERO_EXTEND, ISD::ANY_EXTEND,
                                      ISD::CTLZ, ISD::CTLZ_ZERO_UNDEF,
                                      ISD::SELECT, ISD::SETCC, ISD::BR_CC };

  for (auto op : customI64IntegerOperations) {
    setOperationAction(op, MVT::i64, Custom);
  }

  setOperationAction(ISD::SELECT_CC, MVT::i64, Expand);

  setOperationAction(ISD::TRUNCATE, MVT::i32, Custom);

  setOperationAction(ISD::FTRUNC, MVT::f32, Custom);
  setOperationAction(ISD::FTRUNC, MVT::f16, Custom);

  setOperationAction(ISD::FCOPYSIGN, MVT::f32, Legal);
  setOperationAction(ISD::FCOPYSIGN, MVT::f16, Legal);
}

void SHAVELowering::InitializeLibCallLoweringActions() {
  setLibcallName(RTLIB::REM_F64, "fmodl");
  setLibcallName(RTLIB::FMA_F64, "fmal");
  setLibcallName(RTLIB::POWI_F64, "__powixf2");
  setLibcallName(RTLIB::SQRT_F64, "sqrtl");
  setLibcallName(RTLIB::CBRT_F64, "cbrtl");
  setLibcallName(RTLIB::LOG_F64, "logl");
  setLibcallName(RTLIB::LOG_FINITE_F64, "__logl_finite");
  setLibcallName(RTLIB::LOG2_F64, "log2l");
  setLibcallName(RTLIB::LOG2_FINITE_F64, "__log2l_finite");
  setLibcallName(RTLIB::LOG10_F64, "log10l");
  setLibcallName(RTLIB::LOG10_FINITE_F64, "__log10l_finite");
  setLibcallName(RTLIB::EXP_F64, "expl");
  setLibcallName(RTLIB::EXP_FINITE_F64, "__expl_finite");
  setLibcallName(RTLIB::EXP2_F64, "exp2l");
  setLibcallName(RTLIB::EXP2_FINITE_F64, "__exp2l_finite");
  setLibcallName(RTLIB::EXP10_F64, "exp10l");
  setLibcallName(RTLIB::SIN_F64, "sinl");
  setLibcallName(RTLIB::COS_F64, "cosl");
  setLibcallName(RTLIB::POW_F64, "powl");
  setLibcallName(RTLIB::POW_FINITE_F64, "__powl_finite");
  setLibcallName(RTLIB::CEIL_F64, "ceill");
  setLibcallName(RTLIB::TRUNC_F64, "truncl");
  setLibcallName(RTLIB::RINT_F64, "rintl");
  setLibcallName(RTLIB::NEARBYINT_F64, "nearbyintl");
  setLibcallName(RTLIB::ROUND_F64, "roundl");
  setLibcallName(RTLIB::ROUNDEVEN_F64, "roundevenl");
  setLibcallName(RTLIB::FLOOR_F64, "floorl");
  setLibcallName(RTLIB::COPYSIGN_F64, "copysignl");
  setLibcallName(RTLIB::FMIN_F64, "fminl");
  setLibcallName(RTLIB::FMAX_F64, "fmaxl");
  setLibcallName(RTLIB::LROUND_F64, "lroundl");
  setLibcallName(RTLIB::LLROUND_F64, "llroundl");
  setLibcallName(RTLIB::LRINT_F64, "lrintl");
  setLibcallName(RTLIB::LLRINT_F64, "llrintl");
  setLibcallName(RTLIB::LDEXP_F64, "ldexpl");
  setLibcallName(RTLIB::FREXP_F64, "frexpl");
}

void SHAVELowering::InitializeVectorLoweringActions() {
  auto supported32BitIntegerVectorTypes = { MVT::v4i8,  MVT::v2i16, MVT::v2i8 };
  auto supported128BitIntegerVectorTypes = { MVT::v16i8, MVT::v8i16, MVT::v4i32,
                                             MVT::v8i8,  MVT::v4i16, MVT::v2i32 };

  auto supported32BitFloatVectorTypes = { MVT::v2f16 };
  auto supported128BitFloatVectorTypes = { MVT::v4f32, MVT::v8f16, MVT::v2f32, MVT::v4f16 };

  // add support for vector (SIMD) operations
  auto legalIntegerVectorOps = { ISD::ADD, ISD::MUL, ISD::SUB,
                                 ISD::AND, ISD::OR,  ISD::XOR,
                                 ISD::SHL, ISD::SRA, ISD::SRL, ISD::ROTL
                               };

  auto legalFloatVectorOps = { ISD::FADD, ISD::FMUL, ISD::FSUB, ISD::FNEG, ISD::FABS };

  auto expandIntegerVectorOps = { ISD::SDIV, ISD::SDIVREM, ISD::UDIVREM, ISD::UDIV,
                                  ISD::SREM, ISD::UREM,    ISD::BSWAP, ISD::ROTR
                                };

  auto noVAUexpandIntegerVectorOps = { ISD::MULHU,     ISD::ROTR,
                                       ISD::UMUL_LOHI, ISD::MULHS, ISD::SMUL_LOHI
                                     };

  auto expandFloatVectorOps = { ISD::FSQRT, ISD::FSIN,  ISD::FCOS,
                                ISD::FPOWI, ISD::FPOW,  ISD::FDIV,
                                ISD::FLOG,  ISD::FLOG2, ISD::FLOG10,
                                ISD::FEXP,  ISD::FEXP2, ISD::FTRUNC,
                                ISD::FCEIL, ISD::FRINT, ISD::FMA, ISD::FNEARBYINT
                              };

  // LDI/STI Instructions for Myriad2.3
  if (hasFeature(SHAVE::HasVRF128_Feature)){
    setIndexedStoreAction(ISD::POST_INC, MVT::v4i8,  Legal);
    setIndexedStoreAction(ISD::POST_INC, MVT::v8i8,  Legal);
    setIndexedStoreAction(ISD::POST_INC, MVT::v4i16, Legal);
    setIndexedStoreAction(ISD::POST_INC, MVT::v4f16, Legal);

    if (UseLDI) {
      setIndexedLoadAction(ISD::POST_INC, MVT::v4i16, Legal);
      setIndexedLoadAction(ISD::POST_INC, MVT::v8i8, Legal);
      setIndexedLoadAction(ISD::POST_INC, MVT::v4i8, Legal);
    }
  }

  // FIXME: Movidius - we need to clean this up, and in most cases we should be able to generate
  //        excellent code for extending vectors.  In particular we don't have any patterns for
  //        extending 16-bit to 32-bit vectors, yet this should be easily handled.  We should
  //        also handle both the non-INREG and the INREG variants
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::v2i32, Expand);
  // FIXME: Movidius - currently expanded!
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::v4i8, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::v2i16, Expand);
  //  setOperationAction(ISD::ZERO_EXTEND, MVT::v4i16, Legal);
  if (!SHAVEST.hasVAU()) {
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::v8i8, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::v16i8, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::v4i16, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::v8i16, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::v4i32, Expand);
  }

  setOperationAction(ISD::ZERO_EXTEND, MVT::v2i16, Legal);
  setOperationAction(ISD::ZERO_EXTEND, MVT::v2i32, Legal);

  // FIXME: Movidius - There are problems with enabling SAU vector support without VRFs
  //                   since some VAU instructions will be automatically selected for 
  //                   certain 32-bit vector operations. Once these have been resolved
  //                   the hasVAU check may be removed to re-enable SAU vector support.
  LegalizeAction actionCustomOrExpandVAU = SHAVEST.hasVAU() ? Custom : Expand;
  LegalizeAction actionCustomOrExpandSAU = (SHAVEST.hasSAU() && SHAVEST.hasVAU()) ? Custom : Expand;
  LegalizeAction actionLegalOrExpandVAU = SHAVEST.hasVAU() ? Legal : Expand;
  LegalizeAction actionLegalOrExpandSAU = (SHAVEST.hasSAU() && SHAVEST.hasVAU()) ? Legal : Expand;

  for (auto type :
       { MVT::v4i32, MVT::v4f32, MVT::v8i16, MVT::v8f16, MVT::v16i8,
         MVT::v2i32, MVT::v2f32, MVT::v4i16, MVT::v4f16, MVT::v8i8 }) {
    setOperationAction(ISD::SELECT, type, actionCustomOrExpandVAU);
    setOperationAction(ISD::VSELECT, type, actionCustomOrExpandVAU);
  }

  // FIXME: Movidius - currently expanded!
  setOperationAction(ISD::SELECT, MVT::v4i8, Expand);
  setOperationAction(ISD::SELECT, MVT::v2i16, actionCustomOrExpandSAU);
  setOperationAction(ISD::SELECT, MVT::v2f16, actionCustomOrExpandSAU);
  setOperationAction(ISD::SELECT, MVT::v2i8, Expand);

  setOperationAction(ISD::VSELECT, MVT::v4i8, actionCustomOrExpandSAU);
  setOperationAction(ISD::VSELECT, MVT::v2i16, actionCustomOrExpandSAU);
  setOperationAction(ISD::VSELECT, MVT::v2f16, actionCustomOrExpandSAU);
  setOperationAction(ISD::VSELECT, MVT::v2i8, actionCustomOrExpandSAU);

  // Expand CTPOP for vectors (except v4i32)
  for (auto type : { MVT::v8i16, MVT::v16i8,
                     MVT::v2i32, MVT::v4i16, MVT::v8i8,
                     MVT::v2i16, MVT::v4i8,  MVT::v2i8 }) {
    setOperationAction(ISD::CTPOP, type, Expand);
  }

  // Handle 'ffloor'
  for (auto type : { MVT::v4f32, MVT::v8f16, MVT::v2f32, MVT::v2f16 }) {
    setOperationAction(ISD::FFLOOR, type, Expand);
  }

  setOperationAction(ISD::FEXP2, MVT::v4f32, Expand);
  setOperationAction(ISD::FEXP2, MVT::v2f32, Expand);

  // Handle the "swizzle" instructions
  for (auto type : { MVT::v4i32, MVT::v4f32, MVT::v8i16, MVT::v8f16, MVT::v16i8,
                     MVT::v4i16, MVT::v4f16, MVT::v8i8 }) {
    setOperationAction(ISD::VECTOR_SHUFFLE, type, actionCustomOrExpandVAU);
  }

  // FIXME: Movidius - Temporary workaround for bug #23317
  setOperationAction(ISD::VECTOR_SHUFFLE, MVT::v4i8, Expand);
  setOperationAction(ISD::VECTOR_SHUFFLE, MVT::v2i16, Expand);
  setOperationAction(ISD::VECTOR_SHUFFLE, MVT::v2f16, Expand);
  setOperationAction(ISD::VECTOR_SHUFFLE, MVT::v2i8, Expand);

  for (auto type : { MVT::v4i8, MVT::v2i16, MVT::v2f16, MVT::v2i8 }) {
    setOperationAction(ISD::EXTRACT_SUBVECTOR, type, actionCustomOrExpandSAU);
  }

  for (auto type : {MVT::v4i8, MVT::v2i16, MVT::v2i8}) {
    setOperationAction(ISD::EXTRACT_VECTOR_ELT, type, actionCustomOrExpandSAU);
  }

  setOperationAction(ISD::EXTRACT_VECTOR_ELT, MVT::v16i8, actionCustomOrExpandVAU);
  setOperationAction(ISD::EXTRACT_VECTOR_ELT, MVT::v8i8, actionCustomOrExpandVAU);

  // Handle vector concatenation
  setOperationAction(ISD::CONCAT_VECTORS, MVT::v4i32, actionCustomOrExpandVAU);
  setOperationAction(ISD::CONCAT_VECTORS, MVT::v8i16, actionCustomOrExpandVAU);
  setOperationAction(ISD::CONCAT_VECTORS, MVT::v16i8, actionCustomOrExpandVAU);

  setOperationAction(ISD::CONCAT_VECTORS, MVT::v4f32, actionCustomOrExpandVAU);
  setOperationAction(ISD::CONCAT_VECTORS, MVT::v8f16, actionCustomOrExpandVAU);

  setOperationAction(ISD::CTLZ, MVT::v4i32, Expand);
  setOperationAction(ISD::CTLZ, MVT::v2i32, Expand);
  setOperationAction(ISD::CTLZ, MVT::v8i16, Expand);
  setOperationAction(ISD::CTLZ, MVT::v4i16, Expand);
  setOperationAction(ISD::CTLZ, MVT::v2i16, Expand);
  setOperationAction(ISD::CTLZ, MVT::v16i8, Expand);
  setOperationAction(ISD::CTLZ, MVT::v8i8, Expand);
  setOperationAction(ISD::CTLZ, MVT::v4i8, Expand);
  setOperationAction(ISD::CTLZ, MVT::v2i8, Expand);

  for (auto type : { MVT::v4i32, MVT::v4f32, MVT::v8i16, MVT::v8f16, MVT::v16i8 }) {
    setOperationAction(ISD::MLOAD, type, actionCustomOrExpandVAU);
    setOperationAction(ISD::MSTORE, type, Custom);
  }
  setOperationAction(ISD::MLOAD, MVT::v4i8, actionCustomOrExpandSAU);
  setOperationAction(ISD::MSTORE, MVT::v4i8, Custom);

  setOperationAction(ISD::VECREDUCE_ADD, MVT::v8i16, Legal);
  setOperationAction(ISD::VECREDUCE_ADD, MVT::v16i8, Legal);
  setOperationAction(ISD::VECREDUCE_FADD, MVT::v8f16, Legal);
  setOperationAction(ISD::VECREDUCE_FADD, MVT::v4f32, Legal);

  for (auto type : {MVT::v16i8, MVT::v8i16, MVT::v4i32}) {
    setOperationAction(ISD::VECREDUCE_AND, type, Legal);
    setOperationAction(ISD::VECREDUCE_OR, type, Legal);
    setOperationAction(ISD::VECREDUCE_XOR, type, Legal);
  }

  // Custom handle some vector ops for floats
  for (auto type : supported32BitFloatVectorTypes) {
    setOperationAction(ISD::BUILD_VECTOR, type, actionCustomOrExpandSAU);
    setOperationAction(ISD::SCALAR_TO_VECTOR, type, actionCustomOrExpandSAU);
  }
  for (auto type : supported128BitFloatVectorTypes) {
    setOperationAction(ISD::BUILD_VECTOR, type, actionCustomOrExpandVAU);
    setOperationAction(ISD::SCALAR_TO_VECTOR, type, actionCustomOrExpandVAU);
  }

  // Custom handle some vector ops for integers
  for (auto type : supported32BitIntegerVectorTypes) {
    setOperationAction(ISD::BUILD_VECTOR, type, actionCustomOrExpandSAU);
    setOperationAction(ISD::SCALAR_TO_VECTOR, type, actionCustomOrExpandSAU);
  }
  for (auto type : supported128BitIntegerVectorTypes) {
    setOperationAction(ISD::BUILD_VECTOR, type, actionCustomOrExpandVAU);
    setOperationAction(ISD::SCALAR_TO_VECTOR, type, actionCustomOrExpandVAU);
  }

  setOperationAction(ISD::BUILD_PAIR, MVT::i32, Expand);

  //for(unsigned i = 0; i < sizeof(promotedIntegerVectorTypes) / sizeof(unsigned); i++)
  //{
  //    setOperationAction(ISD::LOAD, (MVT::SimpleValueType)promotedIntegerVectorTypes[i], Promote);
  //    setOperationAction(ISD::STORE, (MVT::SimpleValueType)promotedIntegerVectorTypes[i], Promote);
  //}

  for (auto type : supported32BitIntegerVectorTypes) {
    for (auto op: legalIntegerVectorOps)
      setOperationAction(op, type, actionLegalOrExpandSAU);

    for (auto op: expandIntegerVectorOps)
      setOperationAction(op, type, Expand);
  }

  for (auto type : supported128BitIntegerVectorTypes) {
    for (auto op : legalIntegerVectorOps)
      setOperationAction(op, type, actionLegalOrExpandVAU);

    for (auto op : expandIntegerVectorOps)
      setOperationAction(op, type, Expand);
  }

  for (auto type : supported32BitFloatVectorTypes) {
    for (auto op : legalFloatVectorOps)
      setOperationAction(op, type, actionLegalOrExpandSAU);

    for (auto op : expandFloatVectorOps)
      setOperationAction(op, type, Expand);
  }

  for (auto type : supported128BitFloatVectorTypes) {
    for (auto op : legalFloatVectorOps)
      setOperationAction(op, type, actionLegalOrExpandVAU);

    for (auto op : expandFloatVectorOps)
      setOperationAction(op, type, Expand);
  }

  for (auto type : supported128BitFloatVectorTypes) {
    setOperationAction(ISD::FMINNUM, type, Custom);
    setOperationAction(ISD::FMAXNUM, type, Custom);
    // Other variants of min/max with requirements for +/-0 that do not match CMU.MIN/MAX
    setOperationAction(ISD::FMINIMUM, type, Custom);
    setOperationAction(ISD::FMAXIMUM, type, Custom);
  }

  if (hasFeature(SHAVE::HasVRF128_Feature)){
    for (auto type : { MVT::v4i32, MVT::v4f32, MVT::v8i16, MVT::v8f16, MVT::v16i8,
                       MVT::v2i32, MVT::v2f32, MVT::v4i16, MVT::v4f16, MVT::v8i8,
                                               MVT::v2i16, MVT::v2f16, MVT::v4i8,
                                                                       MVT::v2i8 }) {
      // PEU.PVEC does not support O/UO on SHAVE128 so they must be scalarised to use PC1C
      setCondCodeAction(ISD::SETO, type, Expand);
      setCondCodeAction(ISD::SETUO, type, Expand);
    }
  }

  //Mark all other vector operations to Expand if VAU is absent
  if (!SHAVEST.hasVAU()) {
    for (auto type : supported32BitIntegerVectorTypes)
      for (auto op : noVAUexpandIntegerVectorOps)
        setOperationAction(op, type, Expand);

    for (auto type : supported128BitIntegerVectorTypes)
      for (auto op : noVAUexpandIntegerVectorOps)
        setOperationAction(op, type, Expand);
  }
  setOperationAction(ISD::MULHS, MVT::i8, Expand);
  setOperationAction(ISD::MULHS, MVT::i16, Expand);
  setOperationAction(ISD::MULHU, MVT::i8, Expand);
  setOperationAction(ISD::MULHU, MVT::i16, Expand);


  for (auto op : { ISD::ADD, ISD::SUB, ISD::MUL,
                   ISD::SHL, ISD::SRA, ISD::SRL, ISD::ROTL }) {
    for (auto type : { MVT::v4i32, MVT::v8i16, MVT::v16i8,
                       MVT::v4i16, MVT::v8i8}) {
      setOperationAction(op, type, actionCustomOrExpandVAU);
    }
  }

  for (auto op : { ISD::FADD, ISD::FSUB, ISD::FMUL }) {
    setOperationAction(op, MVT::v2f16, actionCustomOrExpandSAU);
    for (auto type : { MVT::v4f32, MVT::v8f16, MVT::v4f16 }) {
      setOperationAction(op, type, actionCustomOrExpandVAU);
    }
  }

  setOperationAction(ISD::FDIV, MVT::v2f16, actionLegalOrExpandSAU);
  // Special case v8f16 FDIV for u8f -> f16 converting load support
  setOperationAction(ISD::FDIV, MVT::v8f16, actionLegalOrExpandVAU);

  setOperationAction(ISD::UINT_TO_FP, MVT::v2i16, Expand);
  setOperationAction(ISD::UINT_TO_FP, MVT::v4i8, hasFeature(SHAVE::HasVRF128_Feature) ? Legal : Custom);
  setOperationAction(ISD::UINT_TO_FP, MVT::v2i8, Custom);
  setOperationAction(ISD::FP_TO_UINT, MVT::v2i16, Expand);
  setOperationAction(ISD::FP_TO_UINT, MVT::v4i8, hasFeature(SHAVE::HasVRF128_Feature) ? Legal : Expand);

  setOperationAction(ISD::UINT_TO_FP, MVT::v4i32, hasFeature(SHAVE::HasVRF128_Feature) ? Legal : Expand);
  setOperationAction(ISD::UINT_TO_FP, MVT::v8i16, Expand);
  setOperationAction(ISD::UINT_TO_FP, MVT::v16i8, Expand);
  setOperationAction(ISD::UINT_TO_FP, MVT::v2i32, Expand);
  setOperationAction(ISD::UINT_TO_FP, MVT::v4i16, hasFeature(SHAVE::HasVRF128_Feature) ? Legal : Expand);
  setOperationAction(ISD::UINT_TO_FP, MVT::v8i8,  hasFeature(SHAVE::HasVRF128_Feature) ? Legal : Expand);
  setOperationAction(ISD::FP_TO_UINT, MVT::v4i32, Custom);
  setOperationAction(ISD::FP_TO_UINT, MVT::v8i16, Expand);
  setOperationAction(ISD::FP_TO_UINT, MVT::v16i8, Expand);
  setOperationAction(ISD::FP_TO_UINT, MVT::v2i32, Expand);
  setOperationAction(ISD::FP_TO_UINT, MVT::v4i16, Expand);
  setOperationAction(ISD::FP_TO_UINT, MVT::v8i8,  Legal);
  setOperationAction(ISD::FP_TO_SINT, MVT::v8i8,  Legal);


  for (auto type : { MVT::v4f32, MVT::v2f32 }) {
    for (auto op : { ISD::FSIN, ISD::FCOS, ISD::FSQRT, ISD::FRINT }) {
      setOperationAction(op, type, Expand);
    }
  }

  // FIXME: Movidius - There are problems with enabling SAU vector support without VRFs
  //                   since some VAU instructions will be automatically selected for 
  //                   certain 32-bit vector operations. Once these have been resolved
  //                   the hasVAU check may be removed to re-enable SAU vector support.
  if (SHAVEST.hasSAU() && SHAVEST.hasVAU()) {
    setOperationAction(ISD::SETCC, MVT::v2i16, Custom);
    setOperationAction(ISD::SETCC, MVT::v4i8, Custom);
    setOperationAction(ISD::SETCC, MVT::v2i8, Custom);
    setOperationAction(ISD::SETCC, MVT::v2f16, Custom);
  } else {
    // FIXME: Movidius - These do not need to be expanded for SHAVE
    setOperationAction(ISD::SETCC, MVT::v2i16, Expand);
    setOperationAction(ISD::SETCC, MVT::v4i8, Expand);
    setOperationAction(ISD::SETCC, MVT::v2i8, Expand);
    setOperationAction(ISD::SETCC, MVT::v2f16, Expand);
  }

  setOperationAction(ISD::SELECT_CC, MVT::v2i16, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::v4i8, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::v2f16, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::v2i8, Expand);

  for (auto type : { MVT::v4i32, MVT::v4f32, MVT::v8i16, MVT::v8f16, MVT::v16i8,
                     MVT::v2i32, MVT::v2f32, MVT::v4i16, MVT::v4f16, MVT::v8i8 }) {
    setOperationAction(ISD::SELECT_CC, type, Expand);
    setOperationAction(ISD::SETCC, type, actionCustomOrExpandVAU);
  }

  // MIN/MAX convert to equivalent SHAVEISD opcodes
  for (auto op : { ISD::SMIN, ISD::UMIN, ISD::SMAX, ISD::UMAX }) {
    for (auto type : supported128BitIntegerVectorTypes) {
      setOperationAction(op, type, actionCustomOrExpandVAU);
    }
    for (auto type : supported32BitIntegerVectorTypes) {
      setOperationAction(op, type, actionCustomOrExpandVAU);
    }
  }

  //  setOperationAction(ISD::SIGN_EXTEND, MVT::v4i16, Legal);
  setOperationAction(ISD::SIGN_EXTEND, MVT::v4i8, Legal);

  setOperationAction(ISD::TRUNCATE, MVT::v4i32, Legal);

  setOperationAction(ISD::FP_EXTEND, MVT::v4f16, Legal);
  setOperationAction(ISD::FP_ROUND, MVT::v4f32, Legal);

  //setOperationAction(ISD::BUILD_VECTOR, MVT::v2i32, Custom);
  //setOperationAction(ISD::BUILD_VECTOR, MVT::v4i16, Custom);
  //setOperationAction(ISD::BUILD_VECTOR, MVT::v2i16, Custom);
  setOperationAction(ISD::BUILD_VECTOR, MVT::v4f16, Custom);
  setOperationAction(ISD::SCALAR_TO_VECTOR, MVT::v4f16, Custom);

  for (MVT valType : { MVT::v2i8, MVT::v2i16, MVT::v2i32 }) {
    setLoadExtAction(ISD::SEXTLOAD, valType, MVT::v2i1, Expand);
    setLoadExtAction(ISD::ZEXTLOAD, valType, MVT::v2i1, Expand);
    setLoadExtAction(ISD::EXTLOAD, valType, MVT::v2i1, Expand);

    setTruncStoreAction(valType, MVT::v2i1, Expand);
  }

  for (MVT valType : { MVT::v4i8, MVT::v4i16, MVT::v4i32 }) {
    setLoadExtAction(ISD::SEXTLOAD, valType, MVT::v4i1, Expand);
    setLoadExtAction(ISD::ZEXTLOAD, valType, MVT::v4i1, Expand);
    setLoadExtAction(ISD::EXTLOAD, valType, MVT::v4i1, Expand);

    setTruncStoreAction(valType, MVT::v4i1, Expand);
  }

  for (MVT valType : { MVT::v8i8, MVT::v8i16 }) {
    setLoadExtAction(ISD::SEXTLOAD, valType, MVT::v8i1, Expand);
    setLoadExtAction(ISD::ZEXTLOAD, valType, MVT::v8i1, Expand);
    setLoadExtAction(ISD::EXTLOAD, valType, MVT::v8i1, Expand);

    setTruncStoreAction(valType, MVT::v8i1, Expand);
  }

  setLoadExtAction(ISD::SEXTLOAD, MVT::v16i8, MVT::v16i1, Expand);
  setLoadExtAction(ISD::ZEXTLOAD, MVT::v16i8, MVT::v16i1, Expand);
  setLoadExtAction(ISD::EXTLOAD, MVT::v16i8, MVT::v16i1, Expand);

  setTruncStoreAction(MVT::v16i8, MVT::v16i1, Expand);
}

void SHAVELowering::ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results, SelectionDAG &DAG) const {
  DEBUG(
    dbgs() << "SHAVELowering::ReplaceNodeResults: ";
    N->dump(&DAG);
    dbgs() << "\n";
  );
}

unsigned int SHAVELowering::getFunctionAlignment(const Function* f) const {
    return f->getAlignment();
}

EVT SHAVELowering::getSetCCResultType(const DataLayout &DL, LLVMContext &Context, EVT VT) const {
  // FIXME: Movidius - the operand 'Context' is new in v3.5, what should we do with it?
  if (VT.isVector())
    return VT.changeVectorElementTypeToInteger();
  return MVT::i32;
}

const char* SHAVELowering::getTargetNodeName(unsigned op) const {
  switch(op) {
  case SHAVEISD::CMU_CM:                       return "SHAVEISD::CMU_CM";
  // calling convention
  case SHAVEISD::CALL:                         return "SHAVEISD::CALL";
  case SHAVEISD::RETURN:                       return "SHAVEISD::RETURN";
  // FIXME: Movidius - TODO: legacy, to be removed
  case SHAVEISD::RET_FLAG:                     return "SHAVEISD::RET_FLAG";
  // for SHAVE, basic block nodes have no use, load symbolics
  case SHAVEISD::LDISym:                       return "SHAVEISD::LDISym";
  case SHAVEISD::CMU_CPII:                     return "SHAVEISD::CMU_CPII";
  case SHAVEISD::SELECT:                       return "SHAVEISD::SELECT";
  case SHAVEISD::VSELECT:                      return "SHAVEISD::VSELECT";
  case SHAVEISD::SETCC:                        return "SHAVEISD::SETCC";
  case SHAVEISD::MIN:                          return "SHAVEISD::MIN";
  case SHAVEISD::MAX:                          return "SHAVEISD::MAX";
  case SHAVEISD::CLAMP:                        return "SHAVEISD::CLAMP";
  // u8 <--> f16 load and store helpers
  case SHAVEISD::UITOFP_INREG:                 return "SHAVEISD::UITOFP_INREG";
  case SHAVEISD::FPTOUI_INREG:                 return "SHAVEISD::FPTOUI_INREG";
  // ACC and MAC nodes that take sequences of values.
  case SHAVEISD::ACCP_SEQ:                     return "SHAVEISD::ACCP_SEQ";
  case SHAVEISD::ACCN_SEQ:                     return "SHAVEISD::ACCN_SEQ";
  case SHAVEISD::MACP_SEQ:                     return "SHAVEISD::MACP_SEQ";
  case SHAVEISD::MACN_SEQ:                     return "SHAVEISD::MACN_SEQ";
  // i64 helpers
  case SHAVEISD::I64_EXTRACT_LOW:              return "SHAVEISD::I64_EXTRACT_LOW";
  case SHAVEISD::I64_EXTRACT_HIGH:             return "SHAVEISD::I64_EXTRACT_HIGH";
  case SHAVEISD::I64_CONCAT:                   return "SHAVEISD::I64_CONCAT";
  // vector pseudo-instructions
  case SHAVEISD::TRUNCATE_INREG:               return "SHAVEISD::TRUNCATE_INREG";
  case SHAVEISD::SHD:                          return "SHAVEISD::SHD";
  case SHAVEISD::SHLV:                         return "SHAVEISD::SHLV";
  case SHAVEISD::SPLAT:                        return "SHAVEISD::SPLAT";
  case SHAVEISD::LANESPLAT:                    return "SHAVEISD::LANESPLAT";
  case SHAVEISD::ALIGNVEC:                     return "SHAVEISD::ALIGNVEC";
  case SHAVEISD::ROTATE_VECTOR:                return "SHAVEISD::ROTATE_VECTOR";
  case SHAVEISD::INTERLEAVE_VECTORS:           return "SHAVEISD::INTERLEAVE_VECTORS";
  case SHAVEISD::INTERLEAVE_VECTORS_COMBINE:   return "SHAVEISD::INTERLEAVE_VECTORS_COMBINE";
  case SHAVEISD::DEINTERLEAVE_VECTORS:         return "SHAVEISD::DEINTERLEAVE_VECTORS";
  case SHAVEISD::COMBINE_VECTORS:              return "SHAVEISD::COMBINE_VECTORS";
  case SHAVEISD::COMPRESS_VECTOR:              return "SHAVEISD::COMPRESS_VECTOR";
  case SHAVEISD::PERMUTE_VECTOR:               return "SHAVEISD::PERMUTE_VECTOR";
  case SHAVEISD::PERMUTE_BLEND_VECTORS:        return "SHAVEISD::PERMUTE_BLEND_VECTORS";
  case SHAVEISD::UNPACK:                       return "SHAVEISD::UNPACK";
  case SHAVEISD::VECTOR_EXTEND:                return "SHAVEISD::VECTOR_EXTEND";
  case SHAVEISD::EXTRACT_SUBVECTOR:            return "SHAVEISD::EXTRACT_SUBVECTOR";
  case SHAVEISD::INSERT_SUBVECTOR:             return "SHAVEISD::INSERT_SUBVECTOR";
  case SHAVEISD::MASKED_STORE:                 return "SHAVEISD::MASKED_STORE";
  case SHAVEISD::MASKED_STORE_L:               return "SHAVEISD::MASKED_STORE_L";
  case SHAVEISD::MASKED_STORE_H:               return "SHAVEISD::MASKED_STORE_H";
  // horiztontal vector instructions
  case SHAVEISD::HORIZONTAL_ADD:               return "SHAVEISD::HORIZONTAL_ADD";
  case SHAVEISD::HORIZONTAL_AND:               return "SHAVEISD::HORIZONTAL_AND";
  case SHAVEISD::HORIZONTAL_FADD:              return "SHAVEISD::HORIZONTAL_FADD";
  case SHAVEISD::HORIZONTAL_FADD_SUBVECTOR:    return "SHAVEISD::HORIZONTAL_FADD_SUBVECTOR";
  case SHAVEISD::HORIZONTAL_OR:                return "SHAVEISD::HORIZONTAL_OR";
  case SHAVEISD::HORIZONTAL_XOR:               return "SHAVEISD::HORIZONTAL_XOR";
  // dynamic allocations - used mainly for stack overflows check
  case SHAVEISD::DYNALLOC:                     return "SHAVEISD::DYNALLOC";
#if 0
  // FIXME: Movidius - TODO: implementation of Memset. See SHAVESelectionDAGInfo.cpp for full details
  case SHAVEISD::MEMSET_BLOCK:      return "SHAVEISD::MEMSET_BLOCK";
#endif
  // Vector loads
  case SHAVEISD::LOAD64_LOW:                   return "SHAVEISD::LOAD64_LOW";
  case SHAVEISD::LOAD64_HIGH:                  return "SHAVEISD::LOAD64_HIGH";
  default:                          return nullptr;
  }
}

SDValue SHAVELowering::SHAVELowerFP_TO_INT(SDValue op, SelectionDAG &DAG) const {
  unsigned opcode = op.getOpcode();
  EVT VT = op.getValueType();

  // FIXME: Movidius - Is this necessary anymore?
  if (opcode == ISD::FP_TO_UINT && ( VT == EVT(MVT::i32) || VT == EVT(MVT::v4i32)))
    return op;

  if (opcode == ISD::FP_TO_UINT && VT != EVT(MVT::i64))
    return SHAVELowerFP_TO_UINT(op, DAG);

  if (VT != EVT(MVT::i64))
    return op;

  // Lower f32 to i64 and f32 to u64 to libcalls
  SDLoc dbgLoc(op);
  ArgListTy Args;
  const char *symbolName = nullptr;
  SDValue src;

  if (op.getOperand(0).getValueType() == MVT::f16)
    src = DAG.getNode(ISD::FP_EXTEND, dbgLoc, MVT::f32, op.getOperand(0));
  else
    src = op.getOperand(0);

  switch (opcode) {
  case ISD::FP_TO_SINT: symbolName = getLibcallName(RTLIB::FPTOSINT_F32_I64); break;
  case ISD::FP_TO_UINT: symbolName = getLibcallName(RTLIB::FPTOUINT_F32_I64); break;

  default:
    llvm_unreachable("Attempting to lower instruction that is not FP_TO_[S|U]INT");
  }

  SDValue Callee = DAG.getExternalSymbol(symbolName, getPointerTy(DAG.getDataLayout()));
  Type *RetTy = Type::getInt64Ty(*DAG.getContext());
  SDValue Chain = DAG.getEntryNode();
  ArgListEntry Arg;

  Arg.Node = src;
  Arg.Ty = Type::getFloatTy(*DAG.getContext());
  Args.push_back(Arg);

  TargetLowering::CallLoweringInfo CLI(DAG);

  CLI.setDebugLoc(dbgLoc).setChain(Chain)
      .setCallee(CallingConv::C, RetTy, Callee, std::move(Args));

  std::pair<SDValue, SDValue> CallInfo = LowerCallTo(CLI);

  return CallInfo.first;
}

SDValue SHAVELowering::SHAVELowerFP_TO_UINT(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);

  // FIXME: Movidius - this is really inefficient and could be implemented a lot better
  SDValue significandMask = DAG.getConstant(0x007FFFFF, dbgLoc, MVT::i32, false);
  SDValue exponentMask = DAG.getConstant(0x7F800000, dbgLoc, MVT::i32, false);
  SDValue binaryValue = DAG.getNode(ISD::BITCAST, dbgLoc, MVT::i32, op.getOperand(0));

  SDValue significand = DAG.getNode(ISD::OR, dbgLoc, MVT::i32,
          DAG.getConstant(0x00800000, dbgLoc, MVT::i32, false),
          DAG.getNode(ISD::AND, dbgLoc, MVT::i32, binaryValue, significandMask));
  SDValue biasedExponent = DAG.getNode(ISD::SRL, dbgLoc, MVT::i32,
          DAG.getNode(ISD::AND, dbgLoc, MVT::i32, binaryValue, exponentMask),
          DAG.getConstant(23, dbgLoc, MVT::i32, false));
  SDValue exponent = DAG.getNode(ISD::SUB, dbgLoc, MVT::i32,
          biasedExponent,
          DAG.getConstant(127, dbgLoc, MVT::i32, false));

  SDValue expMinus23 = DAG.getNode(ISD::SUB, dbgLoc, MVT::i32,
          exponent, DAG.getConstant(23, dbgLoc, MVT::i32, false));

  SDValue _23MinusExp = DAG.getNode(ISD::SUB, dbgLoc, MVT::i32,
          DAG.getConstant(23, dbgLoc, MVT::i32, false), exponent);

  SDValue setcc1 = DAG.getNode(ISD::SELECT_CC, dbgLoc, MVT::i32, exponent,
          DAG.getConstant(23, dbgLoc, MVT::i32, false),
          DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, significand, _23MinusExp),
          DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, significand, expMinus23),
          DAG.getConstant(ISD::SETLT, dbgLoc, MVT::i32, true));

  SDValue setcc2 = DAG.getNode(ISD::SELECT_CC, dbgLoc, MVT::i32, exponent,
          DAG.getConstant(32, dbgLoc, MVT::i32, false),
          setcc1,
          DAG.getConstant(APInt::getAllOnes(32), dbgLoc, MVT::i32, false),
          DAG.getConstant(ISD::SETLT, dbgLoc, MVT::i32, true));

  return setcc2;
}

SDValue SHAVELowering::SHAVELowerINT_TO_FP(SDValue op, SelectionDAG &DAG) const {
  unsigned opcode = op.getOpcode();
  EVT VT = op.getOperand(0).getValueType();

  if (opcode == ISD::UINT_TO_FP && VT != EVT(MVT::i64))
    return SHAVELowerUINT_TO_FP(op, DAG);

  if (VT != EVT(MVT::i64))
    return op;

  // Lower i64 to f32 and u64 to f32 to libcalls
  SDLoc dbgLoc(op);
  ArgListTy Args;
  const char *symbolName = nullptr;

  switch (opcode) {
  case ISD::SINT_TO_FP: symbolName = getLibcallName(RTLIB::SINTTOFP_I64_F32); break;
  case ISD::UINT_TO_FP: symbolName = getLibcallName(RTLIB::UINTTOFP_I64_F32); break;

  default:
    llvm_unreachable("Attempting to lower instruction that is not [S|U]INT_TO_FP");
  }

  SDValue Callee = DAG.getExternalSymbol(symbolName, getPointerTy(DAG.getDataLayout()));
  Type *RetTy = Type::getFloatTy(*DAG.getContext());
  SDValue Chain = DAG.getEntryNode();
  ArgListEntry Arg;

  Arg.Node = op.getOperand(0);
  Arg.Ty = Type::getInt64Ty(*DAG.getContext());
  Args.push_back(Arg);

  TargetLowering::CallLoweringInfo CLI(DAG);

  CLI.setDebugLoc(dbgLoc).setChain(Chain)
     .setCallee(CallingConv::C, RetTy, Callee, std::move(Args));

  std::pair<SDValue, SDValue> CallInfo = LowerCallTo(CLI);
  SDValue result;

  if (op.getValueType() == MVT::f16)
    result = DAG.getNode(ISD::FP_ROUND, dbgLoc, MVT::f16, CallInfo.first, DAG.getConstant(0, dbgLoc, MVT::i32));
  else
    result = CallInfo.first;

  return result;
}

SDValue SHAVELowering::SHAVELowerUINT_TO_FP(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  SDValue uintVal = op.getOperand(0);

//  EVT uintType = op.getOperand(0).getValueType();
  EVT floatType = op.getValueType();
  EVT uintType = uintVal.getValueType();

  if (uintType.isVector()) {
    MVT promotedType;

    switch (uintType.getSimpleVT().SimpleTy) {
    case MVT::v2i8: promotedType = MVT::v2i16; break;
    case MVT::v4i8: promotedType = MVT::v4i32; break;

    default:
      llvm_unreachable("Lowering UINT_TO_FP of unknown vector type");
    }

    return DAG.getNode(ISD::SINT_TO_FP, dbgLoc, floatType,
                       DAG.getNode(ISD::ZERO_EXTEND, dbgLoc, promotedType, uintVal));
  }

  assert(floatType.getSizeInBits() <= 32 && "Float type not supported.");

  if (uintType == MVT::i32) {
    // Save the last bit
    SDValue lastBit = DAG.getNode(ISD::AND, dbgLoc, MVT::i32, uintVal,
                                  DAG.getConstant(1, dbgLoc, MVT::i32, false));
    // Divide by 2
    SDValue shr = DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, uintVal,
                              DAG.getConstant(1, dbgLoc, MVT::i32, false));
    // Do a signed cast
    SDValue sintToFp = DAG.getNode(ISD::SINT_TO_FP, dbgLoc, floatType, shr);
    // Multiply by 2.0
    SDValue mul2 = DAG.getNode(ISD::FMUL, dbgLoc, floatType, sintToFp,
                               DAG.getConstantFP(2.0, dbgLoc, floatType, false));
    // Cast the last bit to fp
    SDValue lastBitCast = DAG.getNode(ISD::SINT_TO_FP, dbgLoc, floatType, lastBit);
    // Now add it to the casted value
    SDValue addLastBit = DAG.getNode(ISD::FADD, dbgLoc, floatType, mul2, lastBitCast);

    return addLastBit;
  } else
    return DAG.getNode(ISD::SINT_TO_FP, dbgLoc, floatType, DAG.getNode(ISD::ZERO_EXTEND, dbgLoc, MVT::i32, uintVal));
}

SDValue SHAVELowering::SHAVELowerCM_SELECT(SDValue LHS, SDValue RHS,
                                           SDValue TrueVal, SDValue FalseVal,
                                           ISD::CondCode CC, SDLoc dbgLoc,
                                           unsigned Opc,
                                           SelectionDAG &DAG) const {
  // If the RHS is null, it means we want to LHS compare to zero.
  EVT CompareVT = LHS.getValueType();

  if (!RHS.getNode())
    RHS = CompareVT.isFloatingPoint() ? DAG.getConstantFP(0.0, dbgLoc, CompareVT)
                                      : DAG.getConstant(0, dbgLoc, CompareVT);

  SDValue Compare = DAG.getNode(SHAVEISD::CMU_CM, dbgLoc, MVT::Glue, LHS, RHS,
                                DAG.getCondCode(CC));
  SHAVECC::CondCode TCC = SHAVECC::getSHAVECondCode(CC, CompareVT.isFloatingPoint());
  SDValue TCCVal = DAG.getTargetConstant(TCC, dbgLoc, MVT::i32);
  EVT ResultVT = TrueVal.getValueType();

  if (ResultVT == EVT(MVT::i64)) {
    // Extract the lower 32 bits of TrueVal and FalseVal
    SDValue TrueVal_l = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, MVT::i32, TrueVal);
    SDValue FalseVal_l = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, MVT::i32, FalseVal);

    // Extract the upper 32 bits of TrueVal and FalseVal
    SDValue TrueVal_h = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, MVT::i32, TrueVal);
    SDValue FalseVal_h = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, MVT::i32, FalseVal);

    SDValue result_h = DAG.getSelectCC(dbgLoc, LHS, RHS, TrueVal_h, FalseVal_h, CC);
    SDValue result_l = DAG.getSelectCC(dbgLoc, LHS, RHS, TrueVal_l, FalseVal_l, CC);

    return DAG.getNode(SHAVEISD::I64_CONCAT, dbgLoc, MVT::i64, result_h, result_l);
  }

  return DAG.getNode(Opc, dbgLoc, ResultVT, TrueVal, FalseVal, TCCVal, Compare);
}

SDValue SHAVELowering::SHAVEComparei64(SDValue LHS, SDValue RHS,
  ISD::CondCode CC, SDLoc dbgLoc,
                                       SelectionDAG &DAG) const {
  // Extract the lower 32 bits of LHS and RHS
  SDValue LHS_l = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, MVT::i32, LHS);
  SDValue RHS_l = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, MVT::i32, RHS);

  // Extract the upper 32 bits of LHS and RHS
  SDValue LHS_h = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, MVT::i32, LHS);
  SDValue RHS_h = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, MVT::i32, RHS);

  ISD::CondCode cc_h, cc_eq_h, cc_l;

  switch (CC) {
  case ISD::SETOGT:
  case ISD::SETGT:
    cc_h = ISD::SETGT; cc_eq_h = ISD::SETEQ; cc_l = ISD::SETUGT;
    break;
  case ISD::SETUGT:
    cc_h = ISD::SETUGT; cc_eq_h = ISD::SETEQ; cc_l = ISD::SETUGT;
    break;
  case ISD::SETOGE:
  case ISD::SETGE:
    cc_h = ISD::SETGT; cc_eq_h = ISD::SETEQ; cc_l = ISD::SETUGE;
    break;
  case ISD::SETUGE:
    cc_h = ISD::SETUGT; cc_eq_h = ISD::SETEQ; cc_l = ISD::SETUGE;
    break;
  case ISD::SETLT:
  case ISD::SETOLT:
    cc_h = ISD::SETLT; cc_eq_h = ISD::SETEQ; cc_l = ISD::SETULT;
    break;
  case ISD::SETULT:
    cc_h = ISD::SETULT; cc_eq_h = ISD::SETEQ; cc_l = ISD::SETULT;
    break;
  case ISD::SETLE:
  case ISD::SETOLE:
    cc_h = ISD::SETLT; cc_eq_h = ISD::SETEQ; cc_l = ISD::SETULE;
    break;
  case ISD::SETULE:
    cc_h = ISD::SETULT; cc_eq_h = ISD::SETEQ; cc_l = ISD::SETULE;
    break;
  case ISD::SETOEQ:
  case ISD::SETEQ:
  case ISD::SETUEQ:
    cc_h = ISD::SETEQ; cc_l = ISD::SETEQ;
    break;
  case ISD::SETONE:
  case ISD::SETNE:
  case ISD::SETUNE:
    cc_h = ISD::SETNE; cc_l = ISD::SETNE;
    break;
  default:
    break;
  }

  switch (CC) {
  case ISD::SETOGT:
  case ISD::SETGT:
  case ISD::SETUGT:
  case ISD::SETOGE:
  case ISD::SETGE:
  case ISD::SETUGE:
  case ISD::SETOLT:
  case ISD::SETLT:
  case ISD::SETULT:
  case ISD::SETOLE:
  case ISD::SETLE:
  case ISD::SETULE: {
      SDValue cmp_h = DAG.getSetCC(dbgLoc, MVT::i32, LHS_h, RHS_h, cc_h);
      SDValue cmpeq_h = DAG.getSetCC(dbgLoc, MVT::i32, LHS_h, RHS_h, cc_eq_h);
      SDValue cmp_l = DAG.getSetCC(dbgLoc, MVT::i32, LHS_l, RHS_l, cc_l);

      return DAG.getSelect(dbgLoc, MVT::i32, cmpeq_h, cmp_l, cmp_h);
    }

  case ISD::SETEQ:
  case ISD::SETOEQ:
  case ISD::SETUEQ: {
      SDValue cmp_h = DAG.getSetCC(dbgLoc, MVT::i32, LHS_h, RHS_h, cc_h);
      SDValue cmp_l = DAG.getSetCC(dbgLoc, MVT::i32, LHS_l, RHS_l, cc_l);

      return DAG.getNode(ISD::AND, dbgLoc, MVT::i32, cmp_h, cmp_l);
    }

  case ISD::SETONE:
  case ISD::SETNE:
  case ISD::SETUNE: {
      SDValue cmp_h = DAG.getSetCC(dbgLoc, MVT::i32, LHS_h, RHS_h, cc_h);
      SDValue cmp_l = DAG.getSetCC(dbgLoc, MVT::i32, LHS_l, RHS_l, cc_l);

      return DAG.getNode(ISD::OR, dbgLoc, MVT::i32, cmp_h, cmp_l);
    }

  default:
    return SDValue();
  }
}

SDValue SHAVELowering::SHAVELowerCM_SELECT_i64(SDValue LHS, SDValue RHS,
                                               SDValue TrueVal, SDValue FalseVal,
                                               ISD::CondCode CC, SDLoc dbgLoc,
                                               SelectionDAG &DAG) const {
  SDValue cmp = SHAVEComparei64(LHS, RHS, CC, dbgLoc, DAG);
  SDValue result;

  if (cmp == SDValue())
    return SDValue();

  assert(TrueVal.getValueType() == FalseVal.getValueType() &&
         "VT for TrueVal and FalseVal doesn't match");
  EVT ResultVT = TrueVal.getValueType();

  if (ResultVT == MVT::i64) {
    // Extract the lower 32 bits of TrueVal and FalseVal
    SDValue TrueVal_l = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, MVT::i32, TrueVal);
    SDValue FalseVal_l = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, MVT::i32, FalseVal);

    // Extract the upper 32 bits of TrueVal and FalseVal
    SDValue TrueVal_h = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, MVT::i32, TrueVal);
    SDValue FalseVal_h = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, MVT::i32, FalseVal);

    SDValue result_h = DAG.getSelect(dbgLoc, MVT::i32, cmp, TrueVal_h, FalseVal_h);
    SDValue result_l = DAG.getSelect(dbgLoc, MVT::i32, cmp, TrueVal_l, FalseVal_l);

    result = DAG.getNode(SHAVEISD::I64_CONCAT, dbgLoc, MVT::i64, result_h, result_l);
  } else
    result = DAG.getSelect(dbgLoc, ResultVT, cmp, TrueVal, FalseVal);

  return result;
}

SDValue SHAVELowering::SHAVELowerSELECT(SDValue op, SelectionDAG &DAG) const {
  SDValue Cond = op.getOperand(0);
  SDValue TrueVal = op.getOperand(1);
  SDValue FalseVal = op.getOperand(2);

  assert(TrueVal.getValueType() == FalseVal.getValueType() &&
         "VT for TrueVal and FalseVal doesn't match");
  EVT ResultVT = TrueVal.getValueType();

  // Optimise (select (setcc lhs rhs cond) trueval falseval).
  SDValue CompareLHS = Cond;
  SDValue CompareRHS;
  ISD::CondCode CompareCC = ISD::SETNE;

  if (Cond.getOpcode() == ISD::SETCC) {
    Cond = SHAVELowerSETCC(Cond, DAG);
  }

  if (Cond.getOpcode() == SHAVEISD::SETCC) {
    CompareLHS = Cond.getOperand(0);
    CompareRHS = Cond.getOperand(1);
    CompareCC = (ISD::CondCode)Cond.getConstantOperandVal(2);
  }

  // Emit compare and select.
  unsigned Opc = (op.getOpcode() == ISD::VSELECT) ? SHAVEISD::VSELECT
                                                  : SHAVEISD::SELECT;

  if (CompareLHS.getValueType() == MVT::i64)
    return SHAVELowerCM_SELECT_i64(CompareLHS, CompareRHS, TrueVal, FalseVal,
                                   CompareCC, SDLoc(op), DAG);
  else
    return SHAVELowerCM_SELECT(CompareLHS, CompareRHS, TrueVal, FalseVal,
                               CompareCC, SDLoc(op), Opc, DAG);
}

static SDValue fixCompareResult(SDValue compare, EVT VT, SelectionDAG &DAG) {
  // Scalar SETCC inherently returns a 32-bit value on SHAVE because of XOR.
  EVT NativeVT = VT.isVector() ? VT : MVT::i32;

  // Truncate the result to the actual return type if needed.
  if (VT != NativeVT) {
    switch (VT.getSimpleVT().SimpleTy) {
    default:
      llvm_unreachable("Unhandle type for scalar SETCC");
      break;
    case MVT::i8:
    case MVT::i16: {
      SDLoc dbgLoc(compare);
      return DAG.getNode(ISD::TRUNCATE, dbgLoc, VT, compare);
    }
    }
  }

  return compare;
}

SDValue SHAVELowering::SHAVELowerSETCC_i64(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  SDValue LHS = op.getOperand(0);
  SDValue RHS = op.getOperand(1);
  ISD::CondCode CC = ISD::SETCC_INVALID;

  if (op.getOpcode() == ISD::SETCC)
    CC = cast<CondCodeSDNode>(op.getOperand(2))->get();
  else if (op.getOpcode() == SHAVEISD::SETCC)
    CC = (ISD::CondCode)op.getConstantOperandVal(2);

  SDValue loweredCompare = SHAVEComparei64(LHS, RHS, CC, dbgLoc, DAG);

  return fixCompareResult(loweredCompare, op.getValueType(), DAG);
}

SDValue SHAVELowering::SHAVELowerSETCC(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  EVT VT = op.getValueType();
   // Scalar SETCC inherently returns a 32-bit value on SHAVE because of XOR.
  EVT NativeVT = VT.isVector() ? VT : MVT::i32;
  SDValue LHS = op.getOperand(0);
  SDValue RHS = op.getOperand(1);
  EVT LHSVT = LHS.getValueType();
#ifndef NDEBUG
  EVT RHSVT = RHS.getValueType();
  assert(LHSVT == RHSVT && "rhs and lhs expected to have same VT");
#endif // NDEBUG

  if (LHSVT == EVT(MVT::i64))
    return SHAVELowerSETCC_i64(op, DAG);

  ISD::CondCode condMask = cast<CondCodeSDNode>(op.getOperand(2))->get();

  auto getSHAVESETCC = [&](ISD::CondCode code) {
    SDValue cond = DAG.getTargetConstant(code, dbgLoc, MVT::i32);

    // Pass the ISD::CondCode unchanged to selection and the to EmitInstructionWithCustomInserter
    // the custom insertion uses it to find if the comparison is signed or unsigned
    return DAG.getNode(SHAVEISD::SETCC, dbgLoc, NativeVT,
                       LHS, RHS, cond);
  };

  // Special-case for Myriad2.3 which doesn't have "PEU.PC1C O", only "PEU.PC1C UO"
  if (condMask == ISD::SETO && !hasFeature(SHAVE::HasPC1COrdered_Feature)) {
    SDValue setUO = getSHAVESETCC(ISD::SETUO);

    // O -> not UO
    return DAG.getNode(SHAVEISD::SETCC,
                       dbgLoc,
                       NativeVT,
                       setUO,
                       DAG.getConstant(1, dbgLoc, MVT::i32),
                       DAG.getTargetConstant(ISD::SETNE, dbgLoc, MVT::i32));
  }

  auto replacementCodes = SHAVECC::getReplacementCodes(condMask, LHSVT.isFloatingPoint());

  if (replacementCodes.has_value()) {
    SDValue CC1 = getSHAVESETCC(replacementCodes->A);
    SDValue CC2 = getSHAVESETCC(replacementCodes->B);

    SDValue combined = DAG.getNode(replacementCodes->opcode, dbgLoc, NativeVT, CC1, CC2);

    return fixCompareResult(combined, VT, DAG);
  }

  return fixCompareResult(getSHAVESETCC(condMask), VT, DAG);
}

static SDValue splatConstant(SelectionDAG &DAG, SDLoc dbgLoc, EVT VT, unsigned val) {
  SDValue node = DAG.getConstant(val, dbgLoc, VT.getScalarType());
  SmallVector<SDValue, 16> operands(VT.getVectorNumElements(), node);

  return DAG.getNode(ISD::BUILD_VECTOR, dbgLoc, VT, operands);
}

static SDValue LowerIsNaN(SDValue Src, EVT VT, SDLoc dbgLoc, SelectionDAG &DAG) {
  // We can avoid all of these comparisons if the source is constant.
  if (ConstantFPSDNode *FPConst = dyn_cast<ConstantFPSDNode>(Src.getNode()))
    return DAG.getConstant(FPConst->isNaN() ? 1 : 0, dbgLoc, MVT::i32);

  EVT SrcVT = Src.getValueType();
  EVT IntVT;
  uint32_t ExpMaskVal = 0;
  uint32_t MantShiftVal = 0;

  switch (SrcVT.getSimpleVT().SimpleTy) {
  default:
    llvm_unreachable("Unsupported source type for LowerIsNaN");
    break;
  case MVT::f16:
    IntVT = MVT::i16;
    ExpMaskVal = 0x7c00;
    MantShiftVal = 6;
    break;
  case MVT::v32f16:
    IntVT = MVT::v32i16;
    ExpMaskVal = 0x7c00;
    MantShiftVal = 6;
    break;
  case MVT::v16f16:
    IntVT = MVT::v16i16;
    ExpMaskVal = 0x7c00;
    MantShiftVal = 6;
    break;
  case MVT::v8f16:
    IntVT = MVT::v8i16;
    ExpMaskVal = 0x7c00;
    MantShiftVal = 6;
    break;
  case MVT::v4f16:
    IntVT = MVT::v4i16;
    ExpMaskVal = 0x7c00;
    MantShiftVal = 6;
    break;
  case MVT::v2f16:
    IntVT = MVT::v2i16;
    ExpMaskVal = 0x7c00;
    MantShiftVal = 6;
    break;
  case MVT::f32:
    IntVT = MVT::i32;
    ExpMaskVal = 0x7f800000;
    MantShiftVal = 9;
    break;
  case MVT::v16f32:
    IntVT = MVT::v16i32;
    ExpMaskVal = 0x7f800000;
    MantShiftVal = 9;
    break;
  case MVT::v8f32:
    IntVT = MVT::v8i32;
    ExpMaskVal = 0x7f800000;
    MantShiftVal = 9;
    break;
  case MVT::v4f32:
    IntVT = MVT::v4i32;
    ExpMaskVal = 0x7f800000;
    MantShiftVal = 9;
    break;
  case MVT::v2f32:
    IntVT = MVT::v2i32;
    ExpMaskVal = 0x7f800000;
    MantShiftVal = 9;
    break;
  }

  SDValue SrcInt = DAG.getNode(ISD::BITCAST, dbgLoc, IntVT, Src);

  SDValue ExpMask, MantShift, Zero;

  if (SrcVT.getSimpleVT().isVector()) {
    ExpMask = splatConstant(DAG, dbgLoc, IntVT, ExpMaskVal);
    MantShift = splatConstant(DAG, dbgLoc, IntVT, MantShiftVal);
    Zero = splatConstant(DAG, dbgLoc, IntVT, 0);
  }
  else {
    ExpMask = DAG.getConstant(ExpMaskVal, dbgLoc, IntVT);
    MantShift = DAG.getConstant(MantShiftVal, dbgLoc, IntVT);
    Zero = DAG.getConstant(0, dbgLoc, IntVT);
  }

  SDValue CondEQ = DAG.getTargetConstant(ISD::SETEQ, dbgLoc, MVT::i32);
  SDValue CondNE = DAG.getTargetConstant(ISD::SETNE, dbgLoc, MVT::i32);

  SDValue Exp = DAG.getNode(ISD::AND, dbgLoc, IntVT, SrcInt, ExpMask);
  SDValue ExpMatches = DAG.getNode(SHAVEISD::SETCC, dbgLoc, VT, Exp, ExpMask, CondEQ);
  SDValue Mant = DAG.getNode(ISD::SHL, dbgLoc, IntVT, SrcInt, MantShift);
  SDValue MantMatches = DAG.getNode(SHAVEISD::SETCC, dbgLoc, VT, Mant, Zero, CondNE);

  return DAG.getNode(ISD::AND, dbgLoc, VT, ExpMatches, MantMatches);
}

SDValue SHAVELowering::SHAVELowerBR_CC_i64(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  SDValue condition = op.getOperand(1);
  SDValue LHS = op.getOperand(2);
  SDValue RHS = op.getOperand(3);

  ISD::CondCode cc = cast<CondCodeSDNode>(condition)->get();
  SDValue compare = SHAVEComparei64(LHS, RHS, cc, dbgLoc, DAG);
  SDValue br_cc = DAG.getNode(ISD::BR_CC, dbgLoc, MVT::i32, op.getOperand(0), DAG.getCondCode(ISD::SETNE),
                                                            compare, DAG.getConstant(0, dbgLoc, MVT::i32),
                                                            op.getOperand(4));

  return SHAVELowerBR_CC(br_cc, DAG);
}

SDValue SHAVELowering::SHAVELowerBR_CC(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  SDValue chain = op.getOperand(0);
  SDValue condition = op.getOperand(1);
  SDValue leftOp = op.getOperand(2);
  SDValue rightOp = op.getOperand(3);
  bool isFloatingPoint = leftOp.getValueType().isFloatingPoint();

  if (leftOp.getValueType() == MVT::i64)
    return SHAVELowerBR_CC_i64(op, DAG);

  ISD::CondCode cc = cast<CondCodeSDNode>(condition)->get();
  auto replacementCodes = SHAVECC::getReplacementCodes(cc, isFloatingPoint);

  // branch target if condition evaluates to true
  SDValue target = op.getOperand(4);

  if (leftOp.getValueType () != rightOp.getValueType ())
    llvm_unreachable ( "Comparison between different fundamental types is not supported!" );

  SHAVECC::CondCode condMask = SHAVECC::NONE;

  if (replacementCodes.has_value()) {
    auto getSHAVESETCC = [&](ISD::CondCode code) {
      SDValue cond = DAG.getTargetConstant(code, dbgLoc, MVT::i32);

      return DAG.getNode(SHAVEISD::SETCC, dbgLoc, MVT::i32,
                         leftOp, rightOp, cond);
    };

    SDValue CC1 = getSHAVESETCC(replacementCodes->A);
    SDValue CC2 = getSHAVESETCC(replacementCodes->B);

    SDValue combined = DAG.getNode(replacementCodes->opcode, dbgLoc, MVT::i32, CC1, CC2);

    leftOp = combined;
    rightOp = DAG.getConstant(0, dbgLoc, MVT::i32);
    condMask = SHAVECC::NEQ;
    condition = DAG.getCondCode(ISD::SETNE);
  }
  else {
    condMask = SHAVECC::getSHAVECondCode(cc, isFloatingPoint);
  }

  // This is a constant mask used for predication
  SDValue mask = DAG.getTargetConstant(condMask, dbgLoc, MVT::i8);

  // This node produces a glue, so  pass the CondCodeSDNode as is; it will be intercepted at ISel to
  // generate signed or unsigned comparisons
  SDValue compareNode = DAG.getNode(SHAVEISD::CMU_CM, dbgLoc, MVT::Glue, leftOp, rightOp, condition);
  SDValue jumpNode = DAG.getNode(ISD::BR, dbgLoc, MVT::Other,
          chain, target,
          mask, // predication mask
          DAG.getRegister(SHAVE::CC_CMU0, MVT::i8),
          compareNode);

  // This node produces a glue
  return jumpNode;
}


SDValue SHAVELowering::SHAVELowerConstantFP(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);

  if (ConstantFPSDNode *FPN = dyn_cast<ConstantFPSDNode>(op.getNode())) {
    uint32_t binVal =
        llvm::bit_cast<uint32_t>(FPN->getValueAPF().convertToFloat());
    SDValue constant = DAG.getConstant(binVal, dbgLoc, MVT::i32, false);
    SDValue convert = DAG.getNode(ISD::BITCAST, dbgLoc, MVT::f32, constant);

    return convert;
  } else
    assert("Unexpected non constant node");

  return SDValue();
}

SDValue SHAVELowering::SHAVELowerSCALAR_TO_VECTOR(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);

  return DAG.getNode(ISD::INSERT_VECTOR_ELT, dbgLoc, op.getValueType(),
            DAG.getUNDEF(op.getValueType()), op.getOperand(0),
            DAG.getConstant(0, dbgLoc, MVT::i8, false));
}

SDValue SHAVELowering::SHAVELowerCTLZ_64bit(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  SDValue src = op.getOperand(0);
  unsigned BSRIntrId = Intrinsic::shave_iau_bsr_32_r;
  SDValue src_l = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, MVT::i32, src);
  SDValue src_h = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, MVT::i32, src);
  SDValue intrID = DAG.getTargetConstant(BSRIntrId, dbgLoc, MVT::i32);

  // Find the position of the most significant bit in the high half
  SDValue index_h = DAG.getNode(ISD::INTRINSIC_WO_CHAIN, dbgLoc, MVT::i32, intrID, src_h);
  SDValue index_h_adjusted = DAG.getNode(ISD::SUB, dbgLoc, MVT::i32, DAG.getConstant(31, dbgLoc, MVT::i32), index_h);

  // Find the position of the most significant bit in the low half
  SDValue index_l = DAG.getNode(ISD::INTRINSIC_WO_CHAIN, dbgLoc, MVT::i32, intrID, src_l);
  SDValue index_l_adjusted = DAG.getNode(ISD::SUB, dbgLoc, MVT::i32, DAG.getConstant(63, dbgLoc, MVT::i32), index_l);

  // If the high result > 31 then there are no bits set to 1 in the high 32 bits of src
  SDValue result = DAG.getSelectCC(dbgLoc, index_h, DAG.getConstant(31, dbgLoc, MVT::i32), index_l_adjusted, index_h_adjusted, ISD::SETUGT);

  // With CTLZ the result is defined when src == 0
  if (op.getOpcode() == ISD::CTLZ)
    result = DAG.getSelectCC(dbgLoc, result, DAG.getConstant(64, dbgLoc, MVT::i32), DAG.getConstant(64, dbgLoc, MVT::i32), result, ISD::SETUGT);

  return DAG.getNode(SHAVEISD::I64_CONCAT, dbgLoc, MVT::i64, DAG.getConstant(0, dbgLoc, MVT::i32), result);
}

SDValue SHAVELowering::SHAVELowerCTLZ(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  EVT VT = op.getValueType();

  if (VT == MVT::i64)
    return SHAVELowerCTLZ_64bit(op, DAG);

  // Zero-extend the source to 32-bit which is the only legal type for IAU.BSR.
  MVT BSRVT = MVT::i32;
  SDValue src = op.getOperand(0);
  SDValue promotedSrc;
  EVT SrcVT = src.getValueType();
  unsigned BSRIntrId = Intrinsic::shave_iau_bsr_32_r;
  unsigned Width = SrcVT.getSizeInBits();

  switch(SrcVT.getSimpleVT().SimpleTy) {
  default:
    return SDValue();
  case MVT::i8:
  case MVT::i16:
    promotedSrc = DAG.getNode(ISD::ZERO_EXTEND, dbgLoc, BSRVT, src);
    break;
  case MVT::i32:
    promotedSrc = src;
    break;
  }

  // Find the position of the most significant
  SDValue intrID = DAG.getTargetConstant(BSRIntrId, dbgLoc, MVT::i32);
  SDValue index = DAG.getNode(ISD::INTRINSIC_WO_CHAIN, dbgLoc, BSRVT, intrID, promotedSrc);
  SDValue WidthMinuxOne = DAG.getConstant(Width - 1, dbgLoc, BSRVT);

  // Cap this position to the source bit width.
  if(SrcVT != BSRVT)
    index = DAG.getNode(ISD::SELECT_CC, dbgLoc, BSRVT, index, WidthMinuxOne,
                        WidthMinuxOne, index, DAG.getCondCode(ISD::SETGT));

    // Compute the number of leading zeros.
  SDValue CLZ = DAG.getNode(ISD::SUB, dbgLoc, BSRVT, WidthMinuxOne, index);

  if(SrcVT != BSRVT)
    CLZ = DAG.getNode(ISD::TRUNCATE, dbgLoc, VT, CLZ);

  return CLZ;
}

SDValue SHAVELowering::SHAVELowerCTTZ(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  EVT VT = op.getValueType();
  SDValue src = op.getOperand(0);
  EVT SrcVT = src.getValueType();

  switch(SrcVT.getSimpleVT().SimpleTy) {
  default:
    return SDValue();
  case MVT::i8:
  case MVT::i16:
  case MVT::i32:
    break;
  }

  // Count the number of trailing zeros when src is not zero.
  SDValue CTTZ;

  if ((SrcVT == MVT::i8) || (SrcVT == MVT::i16)) {
    SDValue ZExt = DAG.getNode(ISD::ZERO_EXTEND, dbgLoc, MVT::i32, src);

    CTTZ = DAG.getNode(ISD::CTTZ_ZERO_UNDEF, dbgLoc, MVT::i32, ZExt);
    CTTZ = DAG.getNode(ISD::TRUNCATE, dbgLoc, VT, CTTZ);
  } else
    CTTZ = DAG.getNode(ISD::CTTZ_ZERO_UNDEF, dbgLoc, VT, src);

  if (op->getOpcode() == ISD::CTTZ_ZERO_UNDEF)
    return CTTZ;

  // Return the source's bit width when src is zero.
  SDValue Width = DAG.getConstant(SrcVT.getSizeInBits(), dbgLoc, VT);

  return DAG.getNode(ISD::SELECT, dbgLoc, VT, src, CTTZ, Width);
}

SDValue SHAVELowering::SHAVELowerSIGN_EXTEND(SDValue op, SelectionDAG &DAG) const {
  if (op.getValueType() != MVT::i64)
    return op;

  SDLoc dbgLoc(op);
  SDValue extendFrom = op.getOperand(0);
  SDValue resultLo;

  if (extendFrom.getValueType() != MVT::i32)
    resultLo = DAG.getNode(ISD::SIGN_EXTEND, dbgLoc, MVT::i32, extendFrom);
  else
    resultLo = extendFrom;

  SDValue resultHi = DAG.getNode(ISD::SRA, dbgLoc, MVT::i32, resultLo, DAG.getConstant(31, dbgLoc, MVT::i32));

  return DAG.getNode(SHAVEISD::I64_CONCAT, dbgLoc, MVT::i64, resultHi, resultLo);
}

SDValue SHAVELowering::SHAVELowerZERO_EXTEND(SDValue op, SelectionDAG &DAG) const {
  if (op.getValueType() != MVT::i64)
    return op;

  SDLoc dbgLoc(op);
  SDValue extendFrom = op.getOperand(0);
//  MVT extendFromType = extendFrom.getValueType().getSimpleVT().SimpleTy;
  unsigned extendLoMask;

  switch (extendFrom.getValueType().getSimpleVT().SimpleTy) {
  default:       extendLoMask = 0; break;
  case MVT::i16: extendLoMask = 0x0000FFFF; break;
  case MVT::i8:  extendLoMask = 0x000000FF; break;
  }

  SDValue resultLo;

  if (extendLoMask)
    resultLo = DAG.getNode(ISD::ZERO_EXTEND, dbgLoc, MVT::i32, extendFrom);
  else
    resultLo = extendFrom;

  SDValue resultHi = DAG.getConstant(0, dbgLoc, MVT::i32);

  return DAG.getNode(SHAVEISD::I64_CONCAT, dbgLoc, MVT::i64, resultHi, resultLo);
}

SDValue SHAVELowering::SHAVELowerANY_EXTEND(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  SDValue resultLo = op.getOperand(0);
  SDValue resultHi = DAG.getUNDEF(MVT::i32);

  return DAG.getNode(SHAVEISD::I64_CONCAT, dbgLoc, MVT::i64, resultHi, resultLo);
}

SDValue SHAVELowering::SHAVELowerMLOAD(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  MaskedLoadSDNode *N = cast<MaskedLoadSDNode>(op.getNode());
#if 0
  SDValue chain = op.getOperand(0);
  SDValue pointer = op.getOperand(1);
  SDValue mask = op.getOperand(2);
  SDValue passthrough = op.getOperand(3);
#else
  SDValue chain = N->getChain(); // op.getOperand(0);
  SDValue pointer = N->getBasePtr(); //op.getOperand(2);
  SDValue mask = N->getMask(); //op.getOperand(3);
  SDValue passthrough = N->getPassThru();//op.getOperand(1);
#endif
  // Load the full vector
  SDValue load = DAG.getLoad(op.getValueType(), dbgLoc, chain, pointer, cast<MaskedLoadStoreSDNode>(op.getNode())->getMemOperand());

  // Then select which elements we want
  SDValue setcc = DAG.getSetCC(dbgLoc, mask.getValueType(), mask, DAG.getConstant(0, dbgLoc, mask.getValueType()), ISD::SETEQ);
  SDValue select = DAG.getNode(ISD::VSELECT, dbgLoc, op.getValueType(), setcc, passthrough, load);

  return DAG.getNode(ISD::MERGE_VALUES, dbgLoc, DAG.getVTList(op.getValueType(), MVT::Other), select, SDValue(load.getNode(), 1));
}

SDValue SHAVELowering::SHAVELowerMSTORE(SDValue op, SelectionDAG &DAG) const {
  MaskedStoreSDNode *N = cast<MaskedStoreSDNode>(op.getNode());
  SDLoc dbgLoc(op);
  SDValue chain = N->getChain(); // op.getOperand(0);
  SDValue pointer = N->getBasePtr(); //op.getOperand(2);
  SDValue mask = N->getMask(); //op.getOperand(3);
  SDValue value = N->getValue();//op.getOperand(1);

  SDValue compare = DAG.getNode(SHAVEISD::CMU_CM, dbgLoc, MVT::Glue, mask, DAG.getConstant(0, dbgLoc, mask.getValueType()), DAG.getCondCode(ISD::SETNE));
  
  unsigned int ccRegister = 0;
  switch (value.getValueType().getSimpleVT().SimpleTy) {
  case MVT::v4i32: 
  case MVT::v4f32:
  case MVT::v4i8:  ccRegister = SHAVE::C_CMU_0_3; break;
  case MVT::v8i16:
  case MVT::v8f16:
  case MVT::v2i16:
  case MVT::v2f16: ccRegister = SHAVE::C_CMU0; break;
  case MVT::v16i8:ccRegister = SHAVE::C_CMU_0_15; break;
  default: llvm_unreachable("Unsupported masked store type");
  }

  SmallVector<SDValue, 6> operands { 
    chain, value, pointer,
    DAG.getTargetConstant(SHAVECC::NEQ, dbgLoc, MVT::i8),
    DAG.getRegister(ccRegister, MVT::i8),
    compare
  };

  SDValue maskedStore = DAG.getNode(SHAVEISD::MASKED_STORE, dbgLoc, op.getValueType(), operands);

  return maskedStore;
}

SDValue SHAVELowering::SHAVELowerLOAD(SDValue op, SelectionDAG &DAG) const {
  assert(!SHAVEST.hasFeature(SHAVE::HasIRF64_LSU_CMU_Instrs_Feature) &&
         "Only need to lower 64-bit LOADs on NPU without native support");
  SDLoc DbgLoc(op);
  LoadSDNode *Ld = cast<LoadSDNode>(op.getNode());
  EVT MemVT = Ld->getMemoryVT();
  if (MemVT == MVT::i64) {
    Align alignment = commonAlignment(Ld->getAlign(), 4u);
    MachinePointerInfo pointerInfo = Ld->getPointerInfo();
    MachineMemOperand *memOperand = Ld->getMemOperand();
    AAMDNodes AA = Ld->getAAInfo();
    const MDNode *ranges = Ld->getRanges();

    // Do two 32 bit loads
    SDValue LoVal = DAG.getLoad(MVT::i32, DbgLoc, Ld->getChain(), Ld->getBasePtr(),
                                pointerInfo, alignment, memOperand->getFlags(), AA, ranges);

    MachinePointerInfo highPointerInfo = MachinePointerInfo(pointerInfo.V, pointerInfo.Offset + 4);
    SDValue HiPtr =
      DAG.getNode(ISD::ADD, DbgLoc, MVT::i32, Ld->getBasePtr(),
          DAG.getConstant(4, DbgLoc, Ld->getBasePtr().getValueType()));
    SDValue HiVal = DAG.getLoad(MVT::i32, DbgLoc, Ld->getChain(), HiPtr,
                                highPointerInfo, alignment, memOperand->getFlags(), AA, ranges);

    SDValue OutChains[2] = { SDValue(LoVal.getNode(), 1), SDValue(HiVal.getNode(), 1) };
    SDValue OutChain = DAG.getNode(ISD::TokenFactor, DbgLoc, MVT::Other, OutChains);

    SDValue Concat = DAG.getNode(SHAVEISD::I64_CONCAT, DbgLoc, MVT::i64, HiVal, LoVal);

    return DAG.getMergeValues({ Concat, OutChain }, DbgLoc);
  }
  return SDValue();
}

SDValue SHAVELowering::SHAVELowerSTORE(SDValue op, SelectionDAG &DAG) const {
  assert(!SHAVEST.hasFeature(SHAVE::HasIRF64_LSU_CMU_Instrs_Feature) &&
         "Only need to lower 64-bit STOREs on NPU without native support");
  SDLoc DbgLoc(op);
  StoreSDNode *St = cast<StoreSDNode>(op.getNode());
  EVT MemVT = St->getMemoryVT();
  if (MemVT == MVT::i64) {
    Align alignment = commonAlignment(St->getAlign(), 4u);

    MachinePointerInfo MPI = St->getPointerInfo();

    // Do two 32 bit stores
    SDValue LoVal = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, DbgLoc, MVT::i32, St->getValue());
    SDValue HiVal = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, DbgLoc, MVT::i32, St->getValue());

    SDValue LoStore = DAG.getStore(St->getChain(), DbgLoc, LoVal,
                                   St->getBasePtr(), MPI, alignment,
                                   St->getMemOperand()->getFlags(), St->getAAInfo());

    SDValue HiPtr = DAG.getNode(ISD::ADD, DbgLoc, MVT::i32, St->getBasePtr(), DAG.getConstant(4, DbgLoc, St->getBasePtr().getValueType()));
    MachinePointerInfo HiMPI = MachinePointerInfo(MPI.V, MPI.Offset + 4);
    SDValue HiStore = DAG.getStore(St->getChain(), DbgLoc, HiVal,
                                   HiPtr, HiMPI, alignment,
                                   St->getMemOperand()->getFlags(), St->getAAInfo());

    SDValue OutChains[2] = { LoStore, HiStore };
    return DAG.getNode(ISD::TokenFactor, DbgLoc, MVT::Other, OutChains);

    // Bitcast to v2i32
    // This causes a legalization loop with DAGCombiner!
    /* SDValue Val = DAG.getNode(ISD::BITCAST, DbgLoc, MVT::v2i32, St->getValue()); */
    /* return DAG.getStore(St->getChain(), DbgLoc, Val, St->getBasePtr(), */
    /*                     St->getPointerInfo(), St->getAlignment(), */
    /*                     St->getMemOperand()->getFlags(), St->getAAInfo()); */
  }
  return SDValue();
}

SDValue SHAVELowering::SHAVELowerMIN_MAX(SDValue op, SelectionDAG& DAG) const {
  unsigned int newOpcode = ISD::DELETED_NODE;
  unsigned int isUnsigned = std::numeric_limits<unsigned int>::max();
  switch (op.getOpcode()) {
  case ISD::SMIN:
    newOpcode = SHAVEISD::MIN;
    isUnsigned = 0;
    break;
  case ISD::UMIN:
    newOpcode = SHAVEISD::MIN;
    isUnsigned = 1;
    break;
  case ISD::SMAX:
    newOpcode = SHAVEISD::MAX;
    isUnsigned = 0;
    break;
  case ISD::UMAX:
    newOpcode = SHAVEISD::MAX;
    isUnsigned = 1;
    break;
  default:
    llvm_unreachable("Unexpected opcode in MIN/MAX lowering");
  }

  assert(newOpcode != ISD::DELETED_NODE);
  assert(isUnsigned != std::numeric_limits<unsigned int>::max());

  SDLoc dbgLoc(op);
  return DAG.getNode(newOpcode, dbgLoc, op.getValueType(), op.getOperand(0), op.getOperand(1), DAG.getTargetConstant(isUnsigned, dbgLoc, MVT::i32));
}

SDValue SHAVELowering::SHAVELowerFTRUNC(SDValue op, SelectionDAG& DAG) const {
  SDLoc dbgLoc(op);
  SDValue value = op.getOperand(0);
  MVT type = op.getValueType().getSimpleVT();

  assert((type == MVT::f32 || type == MVT::f16) && "Unsupported type for ISD::FTRUNC custom lowering");

  // Ensure the value is positive
  SDValue abs = DAG.getNode(ISD::FABS, dbgLoc, type, value);

  // Extract the "fraction" part of the value
  SDValue opcode = DAG.getTargetConstant(type == MVT::f32 ? Intrinsic::shave_sau_frac_f32_r
                                                          : Intrinsic::shave_sau_frac_f16_r, dbgLoc, MVT::i32);
  SDValue fractionPart = DAG.getNode(ISD::INTRINSIC_WO_CHAIN, dbgLoc, type, opcode, abs);

  // Subtract the fraction part from the value to get the truncated magnitude
  SDValue truncated = DAG.getNode(ISD::FSUB, dbgLoc, type, abs, fractionPart);

  return DAG.getNode(ISD::FCOPYSIGN, dbgLoc, type, truncated, value); // Reset the sign to match the original value
}

static SDValue SHAVELowerMINMAX_NaNChecks(SDValue op, // The original MIN/MAX operation
                                          SDValue minmax, // The lowered SHAVEISD::MIN/MAX
                                          SelectionDAG& DAG) {
  // Add additional checks for NaN when lowering certain min/max operations.
  // If either operand is NaN at runtime, then NaN is returned by this code
  const SDLoc dbgLoc(op);
  const SDValue inA = op.getOperand(0);
  const SDValue inB = op.getOperand(1);
  const MVT type = op.getValueType().getSimpleVT();

  const MVT setCCType = type.isVector() ? MVT::getVectorVT(MVT::getIntegerVT(type.getScalarSizeInBits()), type.getVectorElementCount()) : MVT::i32;

  SDValue setCCA = DAG.getNode(ISD::SETCC, dbgLoc, setCCType,
                               inA, inA, // Compare A with itself
                               DAG.getCondCode(ISD::SETUO));

  const unsigned int selectOpcode = type.isVector() ? ISD::VSELECT : ISD::SELECT;
  SDValue nanCheckA = DAG.getNode(selectOpcode, dbgLoc, type,
                                  setCCA, // If A is NaN
                                  inA, // True value
                                  minmax); // False value

  SDValue setCCB = DAG.getNode(ISD::SETCC, dbgLoc, setCCType,
                               inB, inB, // Compare B with itself
                               DAG.getCondCode(ISD::SETUO));

  SDValue nanCheckB = DAG.getNode(selectOpcode, dbgLoc, type,
                                  setCCB, // If B is NaN
                                  inB, // True value
                                  nanCheckA); // False value

  return nanCheckB;
}

SDValue SHAVELowering::SHAVELowerFMINNUM_FMAXNUM(SDValue op, SelectionDAG& DAG) const {
  // FMINNUM and FMAXNUM match the specifications of C's fminf and fmaxf respectively.
  // MIN/MAX of +/-0 values may return either value,
  // If one value is NaN then the other is returned
  // Only if both values are NaN is NaN returned
  const SDLoc dbgLoc(op);
  const SDValue inA = op.getOperand(0);
  const SDValue inB = op.getOperand(1);
  const MVT type = op.getValueType().getSimpleVT();
  const unsigned int opcode = op.getOpcode() == ISD::FMINNUM ? SHAVEISD::MIN : SHAVEISD::MAX;

  // CMU.[MIN|MAX].[f16|f32]
  return DAG.getNode(opcode, dbgLoc, type, inA, inB, DAG.getTargetConstant(0, dbgLoc, MVT::i32));
}

SDValue SHAVELowering::SHAVELowerFMINIMUM_FMAXIMUM(SDValue op, SelectionDAG& DAG) const {
  // Stricter version of MIN/MAX compared to FMINNUM and FMAXNUM
  // +/-0 values are not considered equal, so the correctly signed value must be returned
  // If at least one value is NaN then NaN is returned
  const SDLoc dbgLoc(op);
  const SDValue inA = op.getOperand(0);
  const SDValue inB = op.getOperand(1);
  const MVT type = op.getValueType().getSimpleVT();
  const bool isMinimum = op.getOpcode() == ISD::FMINIMUM;
  const unsigned int opcode = isMinimum ? SHAVEISD::MIN : SHAVEISD::MAX;

  // CMU.[MIN|MAX].[f16|f32]
  SDValue minmax = DAG.getNode(opcode, dbgLoc, type, inA, inB, DAG.getTargetConstant(0, dbgLoc, MVT::i32));

  if (!DisableNaNChecking && !op->getFlags().hasNoNaNs()) {
    minmax = SHAVELowerMINMAX_NaNChecks(op, minmax, DAG);
  }

  if (!op->getFlags().hasNoSignedZeros()) {
    // Special handling for +/-0 values. SHAVE considers these equal so we must manually inspect both values
    // for +/-0 and fix the output of CMU.MIN/MAX when appropriate
    const MVT intType = type.isVector() ? MVT::getVectorVT(MVT::getIntegerVT(type.getScalarSizeInBits()), type.getVectorElementCount())
                                        : MVT::getIntegerVT(type.getScalarSizeInBits());
    const MVT setCCType = type.isVector() ? intType : MVT::i32;
    const unsigned int selectOpcode = type.isVector() ? ISD::VSELECT : ISD::SELECT;

    SDValue intA = DAG.getNode(ISD::BITCAST, dbgLoc, intType, inA);
    SDValue intB = DAG.getNode(ISD::BITCAST, dbgLoc, intType, inB);

    SDValue isPositiveZeroA = DAG.getNode(ISD::SETCC, dbgLoc, setCCType,
                                          intA, DAG.getConstant(0, dbgLoc, intType), // A is +0
                                          DAG.getCondCode(ISD::SETEQ));

    SDValue isPositiveZeroB = DAG.getNode(ISD::SETCC, dbgLoc, setCCType,
                                          intB, DAG.getConstant(0, dbgLoc, intType), // B is +0
                                          DAG.getCondCode(ISD::SETEQ));

    SDValue isNegativeZeroA = DAG.getNode(ISD::SETCC, dbgLoc, setCCType,
                                          intA, DAG.getConstant(1ull << (type.getScalarSizeInBits() - 1), dbgLoc, intType), // A is -0
                                          DAG.getCondCode(ISD::SETEQ));

    SDValue isNegativeZeroB = DAG.getNode(ISD::SETCC, dbgLoc, setCCType,
                                          intB, DAG.getConstant(1ull << (type.getScalarSizeInBits() - 1), dbgLoc, intType), // B is -0
                                          DAG.getCondCode(ISD::SETEQ));

    // If A is +0 and B is -0, then
    //   - for MAX, return A
    //   - for MIN, return B
    // else return MIN/MAX result
    SDValue positiveANegativeB = DAG.getNode(ISD::AND, dbgLoc, setCCType, isPositiveZeroA, isNegativeZeroB);
    minmax = DAG.getNode(selectOpcode, dbgLoc, type,
                         positiveANegativeB,
                         isMinimum ? inB : inA,
                         minmax);

    // If A is -0 and B is +0, then
    //   - for MAX, return B
    //   - for MIN, return A
    // else return MIN/MAX result
    SDValue negativeApositiveB = DAG.getNode(ISD::AND, dbgLoc, setCCType, isNegativeZeroA, isPositiveZeroB);
    minmax = DAG.getNode(selectOpcode, dbgLoc, type,
                         negativeApositiveB,
                         isMinimum ? inA : inB,
                         minmax);
  }

  return minmax;
}

static SDValue GetConstantSplatValue(SDNode *N) {
  unsigned NumElements = N->getNumOperands();

  // Check if all the operands are constants and if they are all the same.
  bool Broadcast = true;
  SDValue BroadcastVal = N->getOperand(0);

  for (unsigned i = 0; i < NumElements; i++) {
    SDNode *OpNode = N->getOperand(i).getNode();
    unsigned Opc = OpNode->getOpcode();

    switch (Opc) {
    default:
      return SDValue();
    case ISD::Constant:
    case ISD::ConstantFP:
    if (BroadcastVal.isUndef())
       BroadcastVal = N->getOperand(i);
    case ISD::UNDEF:
      break;
    }

    Broadcast &= (OpNode == BroadcastVal.getNode() || OpNode->isUndef());
  }

  return Broadcast ? BroadcastVal : SDValue();
}

SDValue SHAVELowering::OptimizeBUILD_VECTOR(SDNode *N, SelectionDAG &DAG,
                                            bool BeforeLegalize) const {
  SDLoc dbgLoc(N);
  EVT VT = N->getValueType(0);
#if 0 // FIXME: Movidius - the constant packing is totally broken (Bugzilla #22594)
  EVT WordVT = MVT::i32;
#endif
  unsigned NumElements = N->getNumOperands();

  if (!VT.isVector() || !VT.isSimple() || getTypeAction(VT.getSimpleVT().SimpleTy) != TypeLegal)
    return SDValue();

  // Broadcasts are efficiently lowered to a single vector instruction.
  SDValue BroadcastVal = GetConstantSplatValue(N);

  if (BroadcastVal.getNode() && ((hasFeature(SHAVE::HasVRF512_Feature) && (VT.getSizeInBits() == 512 || VT.getSizeInBits() == 256))
                                 || VT.getSizeInBits() == 128 || VT.getSizeInBits() == 64))
    return DAG.getNode(SHAVEISD::SPLAT, dbgLoc, VT, BroadcastVal);

  // Check if all the operands are constants and if they are all the same.
#if 0 // FIXME: Movidius - the constant packing is totally broken (Bugzilla #22594)
  bool CanPack = true;
  EVT EleVT = VT.getVectorElementType();
  const unsigned WordSize = 32;
  const unsigned MaxPackWords = 4;
  unsigned ElementsPerWord = WordSize / EleVT.getSizeInBits();
  bool WordIsDef[MaxPackWords] = {0};
#endif
  unsigned MaxElementBits = 0;

  for (unsigned i = 0; i < NumElements; i++) {
    SDNode *OpNode = N->getOperand(i).getNode();
    unsigned Opc = OpNode->getOpcode();
#if 0 // FIXME: Movidius - the constant packing is totally broken (Bugzilla #22594)
    unsigned WordIndex = i / ElementsPerWord;
#endif

    if (Opc == ISD::Constant) {
      ConstantSDNode *CNode = cast<ConstantSDNode>(OpNode);

      MaxElementBits = std::max((unsigned)CNode->getSimpleValueType(0).getSizeInBits(), MaxElementBits);
#if 0 // FIXME: Movidius - the constant packing is totally broken (Bugzilla #22594)
      WordIsDef[WordIndex] = true;
    } else if (Opc == ISD::ConstantFP) {
      CanPack = false;
      WordIsDef[WordIndex] = true;
#endif
    } else if (Opc == ISD::UNDEF) {
      // Nothing to do for undefined values.
    } else
      return SDValue();
  }

#if 0 // FIXME: Movidius - the constant packing is totally broken (Bugzilla #22594)
  // Determine the packed size of each element, if packing is possible.
  unsigned PackedEleSize = 0;
  unsigned PackedEleSignMask = 0;

  if (CanPack) {
    if (MaxElementBits <= 8) {
      PackedEleSize = 8;
      PackedEleSignMask = 0x80;
    } else if (MaxElementBits <= 16) {
      PackedEleSize = 16;
      PackedEleSignMask = 0x8000;
    }
  }

  // Pack sequences of constant integers into scalars (i32).
  if (PackedEleSize) {
    unsigned ElementsPerPackedWord = (WordSize / PackedEleSize);
    unsigned NumPackedWords = NumElements / ElementsPerPackedWord;

    if (NumPackedWords == 0)
      NumPackedWords = 1;

    bool AllSignBitsZero = true;
    SmallVector<SDValue, MaxPackWords> PackedVals;

    for (unsigned i = 0; i < NumPackedWords; i++) {
      if (WordIsDef[i]) {
        // The source word contains elements, pack them into a scalar word.
        uint32_t PackedVal = 0;

        for (unsigned j = 0; j < ElementsPerPackedWord; j++) {
          unsigned OpIdx = (i * ElementsPerPackedWord) + j;

          if (OpIdx >= NumElements)
            continue;

          SDNode *OpNode = N->getOperand(OpIdx).getNode();

          if (ConstantSDNode *CNode = dyn_cast<ConstantSDNode>(OpNode)) {
            uint32_t OpVal = (uint32_t)CNode->getZExtValue();

            PackedVal |= (OpVal << (PackedEleSize * j));
            AllSignBitsZero &= ((PackedEleSignMask & OpVal) == 0);
          } else {
            // Undefined elements are materialized as zero.
          }
        }

        PackedVals.push_back(DAG.getConstant(PackedVal, dbgLoc, WordVT));
      } else {
        // The whole source word is undefined, do not materialize it.
        PackedVals.push_back(DAG.getUNDEF(WordVT));
      }
    }

    PackedVals.push_back(DAG.getTargetConstant(AllSignBitsZero, dbgLoc, MVT::i8));

    return DAG.getNode(SHAVEISD::UNPACK, dbgLoc, VT, PackedVals);
    }
#endif

  return SDValue();
}

SDValue SHAVELowering::TryConstantPool(SDValue op, SelectionDAG& DAG) const {
  if (SHAVEOptions::DisableConstantPools)
    return SDValue();

  SDLoc dbgLoc(op);
  BuildVectorSDNode *bvNode = cast<BuildVectorSDNode>(op.getNode());
  unsigned numOfElements = bvNode->getNumOperands();

  bool allOperandsAreConstant = true;
  // Check if all the build_vector operands are constants
  for (unsigned i = 0; allOperandsAreConstant && (i < numOfElements); i++)
    allOperandsAreConstant = (((op.getOperand(i).getOpcode() == ISD::Constant) ||
                               (op.getOperand(i).getOpcode() == ISD::ConstantFP)));

  if (!allOperandsAreConstant)
    return SDValue();

  // If the operands are constants, make a load from the constant pool
  std::vector<Constant *> constantList;

  // Return a load from a constant pool
  for (unsigned i = 0; i < numOfElements; i++) {
    if (op.getOperand(i).getOpcode() == ISD::Constant) {
      // Integer constant
      ConstantSDNode *n = cast<ConstantSDNode>(op.getOperand(i).getNode());

      constantList.push_back(const_cast<Constant *>(
          static_cast<const Constant *>(n->getConstantIntValue())));
    } else {
      // Floating-Point constant
      ConstantFPSDNode *n =
          cast<ConstantFPSDNode>(op.getOperand(i).getNode());

      constantList.push_back(const_cast<Constant *>(
          static_cast<const Constant *>(n->getConstantFPValue())));
    }
  }

  Constant *C = ConstantVector::get(constantList);
  Align alignment = DAG.getDataLayout().getPrefTypeAlign(C->getType());
  SDValue CPtr =
      DAG.getConstantPool(C, getPointerTy(DAG.getDataLayout()), alignment);

  auto pointerInfo = MachinePointerInfo::getConstantPool(DAG.getMachineFunction());
  MachineMemOperand * memOperand = DAG.getMachineFunction().getMachineMemOperand(pointerInfo,
                                                                                 MachineMemOperand::MOLoad,
                                                                                 op.getValueType().getSizeInBits(),
                                                                                 alignment);

  return DAG.getLoad(op.getValueType(), dbgLoc, DAG.getEntryNode(), CPtr, memOperand);
}

SDValue SHAVELowering::SHAVELowerBUILD_VECTOR(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  BuildVectorSDNode *bvNode = cast<BuildVectorSDNode>(op.getNode());
  unsigned numOfElements = bvNode->getNumOperands();

  // Try to optimize the operation before emitting a constant pool load.
  SDValue OptVector = OptimizeBUILD_VECTOR(op.getNode(), DAG, false);

  if (OptVector.getNode())
    return OptVector;

  SDValue constantPool = TryConstantPool(op, DAG);
  if (constantPool.getNode())
    return constantPool;

  // if there is at least one non-constant operand, insert the elements into
  // vector one by one
  SDValue temp = DAG.getNode(ISD::INSERT_VECTOR_ELT, dbgLoc, op.getValueType(), DAG.getUNDEF(op.getValueType()),
                             bvNode->getOperand(0), DAG.getConstant(0, dbgLoc, MVT::i8, false));
  SDValue newTemp;

  for(unsigned i = 1; i < numOfElements; i++) {
    SDValue operand = bvNode->getOperand(i);

    // Replace TargetConstant nodes with Constant
    if (operand.getOpcode() == ISD::TargetConstant)
      operand = DAG.getConstant(bvNode->getConstantOperandVal(i), dbgLoc, operand.getSimpleValueType());

    newTemp = DAG.getNode(ISD::INSERT_VECTOR_ELT, dbgLoc, op.getValueType(), temp,
                          operand, DAG.getConstant(i, dbgLoc, MVT::i8, false));
    temp = newTemp;
  }

  return temp;
}

bool IsValid5BitConst(const int val, const bool signedVal) {
  int minVal = 0;
  int maxVal;

  if (signedVal) {
    // Check for the value being a 8bit unsigned immediate
    maxVal = (~(unsigned(0)))>>(8 * sizeof(int) - 5 + 1);
    minVal = - maxVal - 1;;
  } else {
    // Check for the value being a 8bit signed immediate
    maxVal = (~(unsigned(0)))>>(8 * sizeof(int) - 5);
    minVal = 0;
  }

  return (minVal <= val && val <= maxVal);
}

SDValue SHAVELowering::PickConstants(SDValue op, SelectionDAG &DAG, bool signedVal) const {
  SDValue build_vec = op->getOperand(1);

  if (build_vec.getOpcode() != ISD::BUILD_VECTOR)
    return op;

  bool allConsts = true;
  SDLoc dbgLoc(build_vec);
  BuildVectorSDNode *bvNode = cast<BuildVectorSDNode>(build_vec.getNode());
  unsigned numOfElements = bvNode->getNumOperands();

  // Check if all the build_vector operands are constants
  for (unsigned i = 0; (i < numOfElements) && allConsts; i++) {
    unsigned opCode = build_vec.getOperand(i).getOpcode();

    allConsts = (opCode == ISD::Constant) || (opCode == ISD::ConstantFP);
  }

  // if the operands are constants, make a load from the constant pool or a constant itself
  if (!allConsts)
    return op;

  EVT VT = build_vec.getValueType();

  if (VT != op.getValueType()) {
    // Not sure what to do here.
    return op;
  }

  EVT EltVT = VT.getVectorElementType();
  unsigned minSplatBits = EltVT.getSizeInBits();

  if (minSplatBits < 16)
    minSplatBits = 16;

  APInt APSplatBits;
  APInt APSplatUndef;
  unsigned SplatBitSize;
  bool HasAnyUndefs;

  if (bvNode->isConstantSplat(APSplatBits, APSplatUndef, SplatBitSize, HasAnyUndefs, minSplatBits) && (minSplatBits == SplatBitSize)) {
    uint64_t SplatBits = APSplatBits.getZExtValue();
    SDValue buildVecNode;

    switch (VT.getSimpleVT().SimpleTy) {
    default:
      return op;
    case MVT::v16i8: {
        unsigned short Value16 = SplatBits & 0xFF;
        uint64_t sextVal = Value16 & 0xFF;

        if (!IsValid5BitConst((int)(char)sextVal, signedVal))
          return op;

        // The instruction allows this immediate. Create a SPLAT which will match the instruction in tablegen.
        SmallVector<SDValue, 16> Ops;

        Ops.assign(8, DAG.getConstant(Value16, dbgLoc, MVT::i8));
        buildVecNode = DAG.getNode(SHAVEISD::SPLAT, dbgLoc, VT, Ops);
      }
      break;
    case MVT::v8i16: {
        if (!IsValid5BitConst((int)(short)SplatBits, signedVal))
          return op;

        // The instruction allows this immediate. Create a SPLAT which will match the instruction in tablegen.
        SDValue T = DAG.getConstant(SplatBits, dbgLoc, EltVT);
        SmallVector<SDValue, 8> Ops;

        Ops.assign(8, T);
        buildVecNode = DAG.getNode(SHAVEISD::SPLAT, dbgLoc, VT, Ops);
      }
      break;
    case MVT::v4i32:
      if (const ConstantSDNode *int_const = dyn_cast<ConstantSDNode>(build_vec.getOperand(0))) {

        uint64_t sextVal = int_const->getZExtValue();

        if (!IsValid5BitConst((int)sextVal, signedVal))
          return op;

        /* The instruction allows this immediate. Create a SPLAT which will match the instruction in tablegen. */
        SDValue T = DAG.getConstant(unsigned(SplatBits), dbgLoc, VT.getVectorElementType());
        buildVecNode = DAG.getNode(SHAVEISD::SPLAT, dbgLoc, VT, T, T, T, T);
	break;
      }
      return op;
    case MVT::v4f32:
      if (const ConstantFPSDNode* float_const = dyn_cast<ConstantFPSDNode>(build_vec.getOperand(0))) {

        if (SHAVECC::ConstantFPToConstFloat(float_const->getConstantFPValue(), 32) == SHAVECC::CONST_FLOAT_NUM)
          return op;

        SDValue T = DAG.getConstantFP(float_const->getValueAPF(), dbgLoc, VT.getVectorElementType());

        buildVecNode = DAG.getNode(SHAVEISD::SPLAT, dbgLoc, VT, T, T, T, T);
	break;
      }
      return op;
    case MVT::v8f16:
      if (const ConstantFPSDNode* float_const = dyn_cast<ConstantFPSDNode>(build_vec.getOperand(0))) {

        if (SHAVECC::ConstantFPToConstFloat(float_const->getConstantFPValue(), 16) == SHAVECC::CONST_FLOAT_NUM)
          return op;

        SDValue T = DAG.getConstantFP(float_const->getValueAPF(), dbgLoc, VT.getVectorElementType());
        SmallVector<SDValue, 8> Ops;

        Ops.assign(8, T);
        buildVecNode = DAG.getNode(SHAVEISD::SPLAT, dbgLoc, VT, Ops);
	break;
      }
      return op;
    }

    return DAG.getNode(op.getOpcode(), dbgLoc, VT, op->getOperand(0), buildVecNode);
  }

  return op;
}

SDValue SHAVELowering::SHAVELowerSHL_SRL_SRA_64bit(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  unsigned opcode = op.getOpcode();
  SDValue LHS = op.getOperand(0);
  SDValue RHS = op.getOperand(1);
  SDValue shiftAmount;
  SDNode * RHSNode = RHS.getNode();

  if (ConstantSDNode * RHSconst = dyn_cast<ConstantSDNode>(RHSNode))
    shiftAmount = DAG.getConstant(RHSconst->getZExtValue(), dbgLoc, MVT::i32);
  else {
    // FIXME: Movidius - For some reason we can't just extract the low 32 bits from
    // the shift amount and use it directly. Code generation becomes a mess if we
    // don't do *something* with the high 32 bits as well
    SDValue RHS_l = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, MVT::i32, RHS);
    SDValue RHS_h = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, MVT::i32, RHS);

    shiftAmount = DAG.getNode(ISD::OR, dbgLoc, MVT::i32, RHS_l, RHS_h);
  }

  // Extract the high and low parts of the value to be shifted
  SDValue LHS_l = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, DAG.getVTList(MVT::i32, MVT::Glue), LHS);
  SDValue LHS_h = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, DAG.getVTList(MVT::i32, MVT::Glue), LHS);

  // Commonly used constants
  SDValue c0 = DAG.getConstant(0, dbgLoc, MVT::i32);
  SDValue c32 = DAG.getConstant(32, dbgLoc, MVT::i32);

  // Commonly used nodes
  SDValue subRHS32 = DAG.getNode(ISD::SUB, dbgLoc, MVT::i32, shiftAmount, c32);
  SDValue sub32RHS = DAG.getNode(ISD::SUB, dbgLoc, MVT::i32, c32, shiftAmount);

  if (isa<ConstantSDNode>(shiftAmount)) {
    uint64_t constShift = cast<ConstantSDNode>(shiftAmount)->getZExtValue();

    if (constShift == 32) {
      if (opcode == ISD::SRL)
        return DAG.getNode(SHAVEISD::I64_CONCAT, dbgLoc, MVT::i64, c0, LHS_h);
      else if (opcode == ISD::SHL)
        return DAG.getNode(SHAVEISD::I64_CONCAT, dbgLoc, MVT::i64, LHS_l, c0);
      else if (opcode == ISD::SRA)
        return DAG.getNode(SHAVEISD::I64_CONCAT, dbgLoc, MVT::i64, DAG.getNode(ISD::SRA, dbgLoc, MVT::i32, LHS_h, DAG.getConstant(31, dbgLoc, MVT::i32)), LHS_h);
    }
  }

  SDValue result_l_gte32;
  SDValue result_h_gte32;
  SDValue result_l_lt32;
  SDValue result_h_lt32;

  if (opcode == ISD::SHL) {
    // if (num >= 32) {
    //   result_l = 0;
    //   result_h = LHS_l << (RHS - 32);
    // }
    result_l_gte32 = c0;
    result_h_gte32 = DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, LHS_l, subRHS32);

    // else {
    //   result_l = LHS_l << RHS;
    //   result_h = (LHS_h << RHS) | (LHS_l >> (32 - RHS));
    // }
    result_l_lt32 = DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, LHS_l, shiftAmount);
    result_h_lt32 = DAG.getNode(ISD::OR, dbgLoc, MVT::i32,
                                DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, LHS_h, shiftAmount),
                                DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, LHS_l, sub32RHS));
  } else if (opcode == ISD::SRL) {
    // if (num >= 32) {
    //   result_l = LHS_h >> (RHS - 32);
    //   result_h = 0;
    // }
    result_h_gte32 = c0;
    result_l_gte32 = DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, LHS_h, subRHS32);

    // else {
    //   result_h = LHS_h >> RHS;
    //   result_l = (LHS_l >> RHS) | (LHS_h << (32 - RHS));
    // }
    result_h_lt32 = DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, LHS_h, shiftAmount);
    result_l_lt32 = DAG.getNode(ISD::OR, dbgLoc, MVT::i32,
                                DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, LHS_l, shiftAmount),
                                DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, LHS_h, sub32RHS));
  } else if (opcode == ISD::SRA) {
    // if (num >= 32) {
    //   result_h = LHS_h >> 31;
    //   result_l = LHS_h >> (RHS - 32);
    // }
    result_h_gte32 = DAG.getNode(ISD::SRA, dbgLoc, MVT::i32, LHS_h, DAG.getConstant(31, dbgLoc, MVT::i32));
    result_l_gte32 = DAG.getNode(ISD::SRA, dbgLoc, MVT::i32, LHS_h, subRHS32);

    // else {
    //   result_h = LHS_h >> RHS;
    //   result_l = (LHS_l >> RHS) | (LHS_h << (32 - RHS));
    // }
    result_h_lt32 = DAG.getNode(ISD::SRA, dbgLoc, MVT::i32, LHS_h, shiftAmount);
    result_l_lt32 = DAG.getNode(ISD::OR, dbgLoc, MVT::i32,
                                DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, LHS_l, shiftAmount),
                                DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, LHS_h, sub32RHS));
  }

  SDValue result_h = DAG.getSelectCC(dbgLoc, shiftAmount, c32, result_h_gte32, result_h_lt32, ISD::SETGE);
  SDValue result_l = DAG.getSelectCC(dbgLoc, shiftAmount, c32, result_l_gte32, result_l_lt32, ISD::SETGE);

  // if (shiftAmount == 0) return LHS
  result_h = DAG.getSelectCC(dbgLoc, shiftAmount, c0, LHS_h, result_h, ISD::SETEQ);
  result_l = DAG.getSelectCC(dbgLoc, shiftAmount, c0, LHS_l, result_l, ISD::SETEQ);

  return DAG.getNode(SHAVEISD::I64_CONCAT, dbgLoc, MVT::i64, result_h, result_l);
}

SDValue SHAVELowering::SHAVELower64BitOp(SDValue op, SelectionDAG &DAG) const {
  unsigned opcode = op.getOpcode();

  if ((opcode == ISD::SHL) || (opcode == ISD::SRL) || (opcode == ISD::SRA))
    return SHAVELowerSHL_SRL_SRA_64bit(op, DAG);

  SDLoc dbgLoc(op);
  SDValue op0 = op.getOperand(0);
  SDValue op1 = op.getOperand(1);

  SDValue op0_l;
  SDValue op0_h;
  SDValue op1_l;
  SDValue op1_h;

  // Extract the lower 32 bits of op0 and op1
  op0_l = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, EVT(MVT::i32), op0);
  op1_l = DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, EVT(MVT::i32), op1);

  // Extract the upper 32 bits of op0 and op1
  op0_h = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, EVT(MVT::i32), op0);
  op1_h = DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, EVT(MVT::i32), op1);

  // Get the opcodes for calculating the upper and lower halves of the result
  // Also get the result type lists for the operations
  unsigned opcode0 = 0;
  unsigned opcode1 = 0;

  switch (opcode) {
  case ISD::ADD:
    opcode0 = ISD::ADDC;
    opcode1 = ISD::ADDE;
    break;
  case ISD::SUB:
    opcode0 = ISD::SUBC;
    opcode1 = ISD::SUBE;
    break;
  case ISD::AND:
  case ISD::OR:
  case ISD::XOR:
    opcode0 = opcode1 = op.getOpcode();
    break;
  default:
    llvm_unreachable("Attempting to lower unsupported 64 bit operation");
  }

  SDValue lower;
  SDValue upper;

  switch (opcode) {
  case ISD::ADD:
  case ISD::SUB:
    lower = DAG.getNode(opcode0, dbgLoc, DAG.getVTList(MVT::i32, MVT::Glue), op0_l, op1_l);
    upper = DAG.getNode(opcode1, dbgLoc, DAG.getVTList(MVT::i32, MVT::Glue), op0_h, op1_h, lower.getValue(1));
    break;
  case ISD::AND:
  case ISD::OR:
  case ISD::XOR:
    lower = DAG.getNode(opcode0, dbgLoc, MVT::i32, op0_l, op1_l);
    upper = DAG.getNode(opcode1, dbgLoc, MVT::i32, op0_h, op1_h);
    break;
  }

  return DAG.getNode(SHAVEISD::I64_CONCAT, dbgLoc, EVT(MVT::i64), upper, lower);
}

namespace {
static bool isValidExtendOp(unsigned int opcode, bool hasSUMFX512) {
  return opcode == ISD::SIGN_EXTEND ||
         opcode == ISD::ZERO_EXTEND ||
         // FIXME: Movidius - revise if checking for FP_EXTEND affects other targets
         (hasSUMFX512 && opcode == ISD::FP_EXTEND);
}

static unsigned int getMaxSUMFXLength(bool hasSUMFX512, EVT vectorType) {
  // Long SUMFX is only available for v32f16
  if (hasSUMFX512 && vectorType == MVT::v32f16)
    return 512;
  return 128;
}

static bool checkHorizontalOpSize(bool hasVRF512, unsigned int vectorSize, unsigned int opcode) {
  if (hasVRF512) {
    if (opcode == ISD::FADD) {
      return (vectorSize == 128 || vectorSize == 256 || vectorSize == 512);
    } else
      return vectorSize == 512;
  }
  return vectorSize == 128 || vectorSize == 32;
}

static bool needToBreakIntoSubvectors(bool hasVRF512, unsigned maxSUMFXLength) {
  assert((maxSUMFXLength == 128 || maxSUMFXLength == 512) && "Unsupported SAU.SUMFX vector length");
  if (!hasVRF512)
    return false;
  return maxSUMFXLength < 512;
}
} // End of anonymous namespace

SDValue SHAVELowering::SHAVEMatchHorizontalVectorOp(SDValue op, SDNode *& inputChain) const {
  unsigned int opcode = op->getOpcode();

  SDValue op0 = op->getOperand(0);
  SDValue op1 = op->getOperand(1);
  SDNode * currentOp = nullptr;
  SDNode * extract = nullptr;
  SDNode * extend = nullptr;
  bool hasSUMFX512 = SHAVEST.hasFeature(SHAVE::HasSUMFX512_Feature);

  if (op0->getOpcode() == opcode) {
    if (op1->getOpcode() != ISD::EXTRACT_VECTOR_ELT &&
        !isValidExtendOp(op1->getOpcode(), hasSUMFX512))
      return op;

    currentOp = op0.getNode();

    if (op1->getOpcode() == ISD::EXTRACT_VECTOR_ELT) {
      extract = op1.getNode();
    }
    else {
      extend = op1.getNode();

      if (extend->getOperand(0)->getOpcode() != ISD::EXTRACT_VECTOR_ELT)
        return op;
      else
        extract = extend->getOperand(0).getNode();
    }
  }
  else {
    if ((op0->getOpcode() != ISD::EXTRACT_VECTOR_ELT &&
         !isValidExtendOp(op0->getOpcode(), hasSUMFX512)) ||
        op1->getOpcode() != opcode)
      return op;

    currentOp = op1.getNode();

    if (op0->getOpcode() == ISD::EXTRACT_VECTOR_ELT) {
      extract = op0.getNode();
    }
    else {
      extend = op0.getNode();

      if (extend->getOperand(0)->getOpcode() != ISD::EXTRACT_VECTOR_ELT)
        return op;
      else
        extract = extend->getOperand(0).getNode();

    }
  }

  // We can only lower a chain of i32 adds to a horizontal add if the source
  // operands are extended from i8 or i16
  if (op.getOpcode() == ISD::ADD && op.getValueType() == MVT::i32 &&
      extend == nullptr)
    return op;

  SDValue vectorSource = extract->getOperand(0);
  unsigned int numberOfElements = vectorSource->getValueType(0).getVectorNumElements();
  std::vector<SDValue> extracts;
  extracts.push_back(SDValue(extract, 0));

  while (currentOp->getOpcode() == opcode && extracts.size() != numberOfElements) {
    SDValue operand0 = currentOp->getOperand(0);
    SDValue operand1 = currentOp->getOperand(1);
    SDNode * nextOp = nullptr;
    SDNode * otherExtract = nullptr;
    inputChain = nullptr;

    if (operand0->getOpcode() == opcode) {
      if (operand1->getOpcode() != ISD::EXTRACT_VECTOR_ELT &&
          !isValidExtendOp(operand1->getOpcode(), hasSUMFX512))
        return op;

      nextOp = operand0.getNode();

      if (operand1.getOpcode() == ISD::EXTRACT_VECTOR_ELT) {
        extract = operand1.getNode();
      }
      else {
        if (extend == nullptr)
          return op;
        // Ensure they are the same type of extend
        if (extend->getOpcode() != operand1.getOpcode())
          return op;

        extend = operand1.getNode();

        if (extend->getOperand(0).getOpcode() != ISD::EXTRACT_VECTOR_ELT)
          return op;
        else
          extract = extend->getOperand(0).getNode();
      }

      if (extracts.size() == (numberOfElements - 1)) {
        inputChain = operand0.getNode();
      }
    }
    else {
      if (operand0->getOpcode() != ISD::EXTRACT_VECTOR_ELT &&
          !isValidExtendOp(operand0->getOpcode(), hasSUMFX512))
        return op;

      if (operand1->getOpcode() == ISD::EXTRACT_VECTOR_ELT) {
        otherExtract = operand1.getNode();
      }
      else if (extracts.size() == (numberOfElements - 1)) {
        inputChain = operand1.getNode();
      } else if (isValidExtendOp(operand1->getOpcode(), hasSUMFX512)) {
        if (extend == nullptr)
          return op;
        if (extend->getOpcode() != operand1->getOpcode())
          return op;

        if (operand1->getOperand(0).getOpcode() != ISD::EXTRACT_VECTOR_ELT)
          return op;

        otherExtract = operand1->getOperand(0).getNode();
      }
      else if (operand1->getOpcode() != opcode) {
        return op;
      }

      nextOp = operand1.getNode();
      if (operand0.getOpcode() == ISD::EXTRACT_VECTOR_ELT) {
        extract = operand0.getNode();
      }
      else {
        if (extend == nullptr)
          return op;
        // Ensure they are the same type of extend
        if (extend->getOpcode() != operand0.getOpcode())
          return op;

        extend = operand0.getNode();

        if (extend->getOperand(0).getOpcode() != ISD::EXTRACT_VECTOR_ELT)
          return op;
        else
          extract = extend->getOperand(0).getNode();
      }
    }

    if (extract->getOperand(0) != vectorSource)
      return op;

    if (otherExtract != nullptr) {
      if (otherExtract->getOperand(0) != vectorSource)
        return op;

      extracts.push_back(SDValue(otherExtract, 0));
    }

    extracts.push_back(SDValue(extract, 0));
    currentOp = nextOp;
  }

  if (extracts.size() != numberOfElements)
    return op;

  std::vector<bool> elementsFound(numberOfElements, false);
  for (SDValue currentExtract : extracts) {
    SDValue element = currentExtract->getOperand(1);

    if (!isa<ConstantSDNode>(element))
      return op;

    unsigned int constantValue = currentExtract->getConstantOperandVal(1);

    if (constantValue > elementsFound.size() || elementsFound[constantValue])
      return op;

    elementsFound[constantValue] = true;
  }

  for (bool found : elementsFound)
    if (!found)
      return op;

  unsigned int vectorSize = vectorSource->getValueSizeInBits(0);
  if (!checkHorizontalOpSize(hasFeature(SHAVE::HasVRF512_Feature), vectorSize, opcode))
    return op;

  return vectorSource;
}

SDValue SHAVELowering::SHAVELowerHorizontalOp(SDValue op, SelectionDAG &DAG) const {
  SDNode * inputChain = nullptr;
  SDValue vectorSource = SHAVEMatchHorizontalVectorOp(op, inputChain);

  if (vectorSource == op)
    return op;

  unsigned int opcode = op->getOpcode();
  SDNode * result = nullptr;

  if (opcode == ISD::ADD) {
    SDValue add = DAG.getNode(SHAVEISD::HORIZONTAL_ADD, SDLoc(op), MVT::i32, vectorSource);
    if (op->getValueType(0) == MVT::i32)
      result = add.getNode();
    else
      result = DAG.getNode(ISD::TRUNCATE, SDLoc(op), op->getValueType(0), add).getNode();
  } else if (opcode == ISD::FADD) {
    unsigned int vectorSize = vectorSource->getValueSizeInBits(0);
    unsigned int maxVectorSizeForOp =
        getMaxSUMFXLength(SHAVEST.hasFeature(SHAVE::HasSUMFX512_Feature), vectorSource.getValueType());
    unsigned int subVectorCount = vectorSize/maxVectorSizeForOp;

    if (needToBreakIntoSubvectors(hasFeature(SHAVE::HasVRF512_Feature), maxVectorSizeForOp)) {
      assert((subVectorCount == 1 || subVectorCount == 2 || subVectorCount == 4) &&
             "Incorrect vector split for SUMFX");
      SDLoc dbgLoc(op);
      EVT floatType = vectorSource.getValueType().getVectorElementType();
      SmallVector<SDValue> subVectors;

      for (unsigned i = 0; i < subVectorCount; ++i)
      {
        subVectors.push_back(DAG.getNode(SHAVEISD::HORIZONTAL_FADD_SUBVECTOR,
            SDLoc(op), op->getValueType(0), vectorSource,
            DAG.getConstant(i, dbgLoc, MVT::i16)));
      }

      SDValue rootAdd = subVectors[subVectors.size() - 1];
      for (int i = subVectors.size() - 2; i >= 0; --i) {
        rootAdd =
            DAG.getNode(ISD::FADD, dbgLoc, floatType, subVectors[i], rootAdd);
      }
      result = rootAdd.getNode();
    }
    else {
      result = DAG.getNode(SHAVEISD::HORIZONTAL_FADD, SDLoc(op),
                           op->getValueType(0), vectorSource)
                   .getNode();
    }
  }
  else {
    unsigned int hOpcode = ISD::DELETED_NODE;

    switch (opcode) {
    case ISD::AND:  hOpcode = SHAVEISD::HORIZONTAL_AND;  break;
    case ISD::OR:   hOpcode = SHAVEISD::HORIZONTAL_OR;   break;
    case ISD::XOR:  hOpcode = SHAVEISD::HORIZONTAL_XOR;  break;
    }

    assert(hOpcode != ISD::DELETED_NODE && "Invalid opcode found in horizontal vector operation lowering");

    result = DAG.getNode(hOpcode, SDLoc(op), op->getValueType(0), vectorSource).getNode();
  }

  if (inputChain != nullptr)
    return DAG.getNode(opcode, SDLoc(op), op->getValueType(0), SDValue(inputChain, 0), SDValue(result, 0));
  else
    return SDValue(result, 0);
}

SDValue SHAVELowering::SHAVELowerADD_SUB_ROTL_SHL_SRA_SRL(SDValue op, SelectionDAG &DAG) const {
  if (op->getValueType(0) == EVT(MVT::i64))
    return SHAVELower64BitOp(op, DAG);

  return PickConstants(op, DAG, false);
}

SDValue SHAVELowering::SHAVELowerTRUNCATE(SDValue op, SelectionDAG &DAG) const {
  if (op->getSimpleValueType(0) != MVT::i32 && op->getOperand(0)->getSimpleValueType(0) != MVT::i64)
    return op;

  SDLoc dbgLoc(op);
  SDValue truncatedValue = op->getOperand(0);

  if (truncatedValue->getOpcode() == ISD::SRL) {
    SDValue shiftAmountValue = truncatedValue->getOperand(1);
    if (isa<ConstantSDNode>(shiftAmountValue) && truncatedValue->getConstantOperandVal(1) == 32ull)
      return DAG.getNode(SHAVEISD::I64_EXTRACT_HIGH, dbgLoc, EVT(MVT::i32), truncatedValue->getOperand(0));
  }

  return DAG.getNode(SHAVEISD::I64_EXTRACT_LOW, dbgLoc, EVT(MVT::i32), truncatedValue);
}

SDValue SHAVELowering::SHAVELowerMUL(SDValue op, SelectionDAG &DAG) const {
  return PickConstants(op, DAG, true);
}

SDValue SHAVELowering::SHAVELowerEXTRACT_VECTOR_ELT(SDValue op, SelectionDAG &DAG) const {
  // This function handles lowering for  extraction from 16/32 bit vectors with a variable index
  //
  // Extraction from 16/32 bit vectors with a constant index are handled directly by patterns in tablegen
  // Extraction from 64/128 bit vectors with a variable index are handled in handled in SHAVEISelDAGtoDAG::SelectEXTRACT_VECTOR_ELT(SDNode *N)

  SDLoc dbgLoc(op);
  SDValue src = op.getOperand(0);
  EVT srcType = src.getValueType();

  // Start with 16/32 bit vectors
  if (src->getValueSizeInBits(0) <= 32 && !dyn_cast<ConstantSDNode>(op->getOperand(1).getNode())) {
    MVT castType = op->getOperand(0)->getValueSizeInBits(0) == 32 ? MVT::i32 : MVT::i16;
    unsigned shiftAmount = op->getValueSizeInBits(0) == 16 ? 4 : 3; // Multiply x8 index by 8, x16 index by 16

    SDValue bitcast = DAG.getBitcast(castType, src);
    SDValue shiftedIndex = DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, op->getOperand(1), DAG.getConstant(shiftAmount, dbgLoc, MVT::i32));

    SDValue extractedValue = DAG.getNode(ISD::SRL, dbgLoc, castType, bitcast, shiftedIndex);

    return DAG.getNode(ISD::TRUNCATE, dbgLoc, op->getValueType(0), extractedValue);
  }

  return op;
}

void SHAVELowering::GenerateShuffleMaskForCONCAT_VECTORS(SmallVector<SmallVector<int, 8>, 2> &shuffleVectors,
  EVT shuffleOutType, unsigned numOfInputVectors, unsigned numElementsInInputVectors) const {
  unsigned outSize = shuffleOutType.getVectorNumElements();

  for (unsigned i = 2; i < numOfInputVectors; ++i) {
    SmallVector<int, 8> newMask;

    for (unsigned j = 0; j < numElementsInInputVectors * numOfInputVectors; ++j) {
      unsigned alreadyShuffled = (outSize / numOfInputVectors)*i;

      if (j < alreadyShuffled)
        newMask.push_back(j);
      else if (j <= (i + 1)*(numElementsInInputVectors)) {
        unsigned maskElement = (j - alreadyShuffled) + outSize;

        newMask.push_back(maskElement);
      } else
        newMask.push_back(0);
    }

    shuffleVectors.push_back(newMask);
  }
}

SDValue SHAVELowering::SHAVELowerCONCAT_VECTORS(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  bool allTruncs = op.getNumOperands() > 0;

  for (unsigned i = 0; i < op.getNumOperands(); ++i) {
      SDValue operand = op.getOperand(i);
      allTruncs &= (operand.getOpcode() == ISD::TRUNCATE);
  }

  // If each operand of op is a truncate
  if (allTruncs) {
    EVT toType = op.getValueType();
    EVT fromType = op.getOperand(0).getOperand(0).getValueType();
    bool allSameType = true;

    for (unsigned i = 1; i < op.getNumOperands(); ++i) {
      allSameType &= (op.getOperand(i).getOperand(0).getValueType().getSizeInBits() == toType.getSizeInBits());
      allSameType &= (op.getOperand(i).getOperand(0).getValueType() == fromType);
    }

    // If all truncate instructions have the same type signatures
    // (i.e. they are all truncates from the same type, to the same type)
    // and the truncates' source type is the same as the concat_vectors' type
    // NB: we can't handle the shuffle of 8 bit vectors efficiently in this way
    if (allSameType && toType.getScalarSizeInBits() != 8) {
      // Create bitcasts for the truncates' operands to the concat's type
      SmallVector<SDValue, 4> bitcasts;

      for (unsigned i = 0; i < op.getNumOperands(); ++i)
        bitcasts.push_back(DAG.getNode(ISD::BITCAST, dbgLoc, toType, op.getOperand(i).getOperand(0)));

      // Create a mask for the shuffle instructions
      SmallVector<int, 8> shuffleMask;

      for (unsigned i = 0; i < toType.getVectorNumElements(); ++i) {
        int mask = i * op.getNumOperands();

        if (mask < static_cast<int>(toType.getVectorNumElements() * 2))
          shuffleMask.push_back(mask);
        else
          shuffleMask.push_back(0);
      }

      // The above mask can only be used for the first shuffle instruction
      // we need to build additional masks for when we are concatenating >2 vectors
      SmallVector<SmallVector<int, 8>, 2> shuffleMasks;

      shuffleMasks.push_back(shuffleMask);
      GenerateShuffleMaskForCONCAT_VECTORS(shuffleMasks, fromType, op.getNumOperands(), fromType.getVectorNumElements());

      // Shuffle those bitcasts together to form the result
      SDValue shuffle = DAG.getVectorShuffle(toType, dbgLoc, bitcasts[0], bitcasts[1], shuffleMasks[0]);

      for (unsigned i = 2; i < op.getNumOperands(); ++i)
          shuffle = DAG.getVectorShuffle(toType, dbgLoc, shuffle, bitcasts[i], shuffleMasks[i-1]);

      return shuffle;
    }
  }

  //Disabling this bit for now, since it generates incorrect code in the following scenario
  //       %1 = loadhi <2xfloat> memaddr1
  //       store different value to memaddr1
  //       %2 = loadlo <2xfloat> memaddr2
  //       concat_vector %1, %2
  // the loadhi gets moved over the scalar store, thereby putting wrong value in the concat_vector
#if 0
  // If both operands are loads and the destination is a full vector register, then we can emit
  // a load low and load high on different addresses to the same register
  if (op.getNumOperands() == 2 &&
      op.getOperand(0).getOpcode() == ISD::LOAD && op.getOperand(1).getOpcode() == ISD::LOAD &&
      op.getValueSizeInBits() == 128) {
    const LoadSDNode *originalLoadLow = cast<LoadSDNode>(op.getOperand(0).getNode());
    const LoadSDNode *originalLoadHigh = cast<LoadSDNode>(op.getOperand(1).getNode());
    
    // Double the vector size so the register allocator will allocate a single, full VRF128 for both instructions
    SDVTList types = DAG.getVTList(originalLoadLow->getValueType(0).getDoubleNumVectorElementsVT(*DAG.getContext()), MVT::Other);
    
    const SDValue lowOperands[2] = { originalLoadLow->getChain(), originalLoadLow->getBasePtr() };
    SDValue loadLow = DAG.getMemIntrinsicNode(SHAVEISD::LOAD64_LOW, dbgLoc, types, lowOperands, originalLoadLow->getMemoryVT(), originalLoadLow->getMemOperand());

    const SDValue highOperands[3] = { originalLoadHigh->getChain(), originalLoadHigh->getBasePtr(), loadLow };
    SDValue loadHigh = DAG.getMemIntrinsicNode(SHAVEISD::LOAD64_HIGH, dbgLoc, types, highOperands, originalLoadHigh->getMemoryVT(), originalLoadHigh->getMemOperand());

    return loadHigh;
  }
#endif


  // General case
  EVT toType = op.getValueType();
  SmallVector<SDValue, 2> vectors;
//  EVT scalarType = op.getOperand(0).getValueType().getVectorElementType();

  bool isVectorExtend = true;
  // Extend each input operand to the target vector size
  for (unsigned i = 0; i < op.getNumOperands(); ++i) {
      SDValue vector = DAG.getNode(SHAVEISD::VECTOR_EXTEND, dbgLoc, toType, op.getOperand(i));

      isVectorExtend &= ((i == 0) || op.getOperand(i).isUndef());
      vectors.push_back(vector);
  }

  // Don't create shuffles if a simple VECTOR_EXTEND suffices
  if (isVectorExtend)
    return vectors[0];

  // Create a mask for the shuffle instructions
  SmallVector<int, 8> shuffleMask;
  unsigned sourceNumElements = op.getOperand(0).getValueType().getVectorNumElements();

  for (unsigned i = 0; i < toType.getVectorNumElements(); ++i)
    if (i >= sourceNumElements)
      shuffleMask.push_back(i - sourceNumElements + toType.getVectorNumElements());
    else if (i < sourceNumElements)
      shuffleMask.push_back(i);

  // The above mask can only be used for the first shuffle instruction
  // we need to build additional masks for when we are concatenating >2 vectors
  SmallVector<SmallVector<int, 8>, 2> shuffleMasks;

  shuffleMasks.push_back(shuffleMask);
  GenerateShuffleMaskForCONCAT_VECTORS(shuffleMasks, toType, op.getNumOperands(),
                                        op.getOperand(0).getValueType().getVectorNumElements());

  // Shuffle the vectors together to form the result
  SDValue shuffle = DAG.getVectorShuffle(toType, dbgLoc, vectors[0], vectors[1], shuffleMasks[0]);

  for (unsigned i = 2; i < op.getNumOperands(); ++i)
    shuffle = DAG.getVectorShuffle(toType, dbgLoc, shuffle, vectors[i], shuffleMasks[i - 1]);

  return shuffle;
}

SDValue SHAVELowering::findScalarInsert(SDValue Src, int &laneIdx) {
  if ((Src.getOpcode() == ISD::INSERT_VECTOR_ELT) &&
      (Src.getOperand(0).getOpcode() == ISD::UNDEF)) {
    ConstantSDNode *InsertPos = dyn_cast<ConstantSDNode>(Src.getOperand(2).getNode());

    if (InsertPos) {
      laneIdx = InsertPos->getSExtValue();
      return Src.getOperand(1);
    }
  }

  return SDValue();
}

SDValue SHAVELowering::SHAVELowerEXTRACT_SUBVECTOR(SDValue op, SelectionDAG &DAG) const {
  // Get the range of elements, extract them into Ops vector, and build vector with these elements
  SDNode *Node = op.getNode();
  SDLoc dbgLoc = SDLoc(op);
  SmallVector<SDValue, 8> Ops;
  SDValue SubOp = Node->getOperand(0);
  EVT VVT = SubOp.getNode()->getValueType(0);
  EVT EltVT = VVT.getVectorElementType();
  unsigned idx = Node->getConstantOperandVal(1);
  EVT VecVT = op.getValueType();
  unsigned  NumExtractElements = VecVT.getVectorNumElements();

  for (unsigned i = 0; i < NumExtractElements; i++)
    Ops.push_back(DAG.getNode(ISD::EXTRACT_VECTOR_ELT, dbgLoc, EltVT, SubOp, DAG.getConstant(idx + i, dbgLoc, MVT::i32, false)));


  return DAG.getNode(ISD::BUILD_VECTOR, dbgLoc, op.getValueType(), Ops);
}

SDValue SHAVELowering::SHAVELowerVECTOR_SHUFFLE(SDValue op, SelectionDAG &DAG) const {
    ShuffleVectorSDNode *SHV = cast<ShuffleVectorSDNode>(op.getNode());

    EVT VecVT = op.getValueType();
    SDValue LHS = SHV->getOperand(0);
    SDValue RHS = SHV->getOperand(1);
    SDLoc dbgLoc = SDLoc(op);

    DEBUG(dbgs() << "SHAVELowering::SHAVELowerVECTOR_SHUFFLE() on " << VecVT.getEVTString() << "\n");

    // FIXME: Movidius - TODO: - Simplify
    // v4xyy and v8x16 should be handled directly by the instruction-selection
    //  as we have short patterns for those
    //
    // v2xyy should probably just be expanded as it's at most a few operations
    //  anyway though it could be handled as a partial v4xyy in some cases
    ShuffleAnalysis SA(SHV->getMask(), VecVT, LHS, RHS, SHAVEST);

    if (SA.analyse()) {
      // Analysis succeeded, the shuffle will be lowered during ISel.
      DEBUG(dbgs() << "Found shuffle pattern\n");

      // Lowerings that can be done to other IR level constructs should be done
      // here already as this allows for more cleanup and improves combining
      switch(SA.Action) {
      case ShuffleAnalysis::LowerIdentityV0:
        return LHS;

      case ShuffleAnalysis::LowerUndefined:
        return DAG.getNode(ISD::UNDEF, dbgLoc, VecVT);

      case ShuffleAnalysis::LowerSwapVectors: {
        // Construct new shuffle mask with swapped inputs
        ArrayRef<int> OldMask = SHV->getMask();
        SmallVector<int, 16> NewMask;
        unsigned Nelem = VecVT.getVectorNumElements();

        for (unsigned i = 0; i < Nelem; i++)
          NewMask.push_back(OldMask[Nelem+i]);

        for (unsigned i = 0; i < Nelem; i++)
          NewMask.push_back(OldMask[i]);

        return DAG.getVectorShuffle(VecVT, dbgLoc, RHS, LHS, NewMask);
      }

      case ShuffleAnalysis::LowerSplatV0: {
        // Find the input index
        EVT EleVT = VecVT.getVectorElementType();

        SDValue SplatValue = DAG.getNode(ISD::EXTRACT_VECTOR_ELT, dbgLoc, EleVT, LHS,
                                          DAG.getConstant(SA.SplatLaneV0, dbgLoc, MVT::i32, false));

        return DAG.getNode(SHAVEISD::SPLAT, dbgLoc, VecVT, SplatValue);
      }

      case ShuffleAnalysis::LowerSubvectorInsert: {
        SDValue insertIndex = DAG.getConstant(SA.SubvectorInsertIndex, dbgLoc, MVT::i32);
        SDValue extractIndex = DAG.getConstant(SA.SubvectorExtractIndex, dbgLoc, MVT::i32);

        // This is just a vector element insert, not a subvector insert
        if (SA.SubvectorInsertSize == 1) {
          SDValue elementExtract = DAG.getNode(ISD::EXTRACT_VECTOR_ELT, dbgLoc, VecVT.getScalarType(), RHS, extractIndex);
          return DAG.getNode(ISD::INSERT_VECTOR_ELT, dbgLoc, VecVT, LHS, elementExtract, insertIndex);
        }

        // If the source vector is created by a concat_vectors node, we may be able to just
        // take the subvector directly from the concat_vectors' operands rather than having
        // to emit an EXTRACT_SUBVECTOR
        if (RHS.getOpcode() == ISD::CONCAT_VECTORS) {
          unsigned sourceSizeInBytes = RHS.getOperand(0).getValueSizeInBits() / 8;
          int sourceNumElements = (int) RHS.getOperand(0).getValueType().getVectorNumElements();
          SDValue source;

          if (sourceNumElements == SA.SubvectorInsertSize) {
            if (SA.SubvectorExtractIndex % sourceSizeInBytes == 0) {
              unsigned sourceOperandIndex = SA.SubvectorExtractIndex / sourceSizeInBytes;
              if (sourceOperandIndex < RHS.getNumOperands())
                source = RHS.getOperand(sourceOperandIndex);
            }

            if (source.getNode() != nullptr) {
              return DAG.getNode(SHAVEISD::INSERT_SUBVECTOR, dbgLoc, VecVT, LHS, source, insertIndex);
            }
          }
        }

        MVT extractType = MVT::getVectorVT(VecVT.getScalarType().getSimpleVT(), SA.SubvectorInsertSize);

        SDValue subvectorExtract = DAG.getNode(SHAVEISD::EXTRACT_SUBVECTOR, dbgLoc, extractType, RHS, extractIndex);

        return DAG.getNode(SHAVEISD::INSERT_SUBVECTOR, dbgLoc, VecVT, LHS, subvectorExtract, insertIndex);
      }

      default:
        return op;
      }
    }

    // Change v8i8 into v8i16 shuffle which can be handled efficiently using
    // trunc_to_v8i8(shuffle(anyext_to_v8i16(LHS), anyext_to_v8i16(RHS), mask))
    if (VecVT == MVT::v8i8) {
      DEBUG(dbgs() << "Extending v8i8 shuffle to use v8i16\n");

      SDValue newLHS = DAG.getNode(ISD::ANY_EXTEND, dbgLoc, MVT::v8i16, LHS);
      SDValue newRHS = DAG.getNode(ISD::ANY_EXTEND, dbgLoc, MVT::v8i16, RHS);

      SDValue newShuffle =
        DAG.getVectorShuffle(MVT::v8i16, dbgLoc, newLHS, newRHS, SHV->getMask());

      SDValue truncated = DAG.getNode(ISD::TRUNCATE, dbgLoc, MVT::v8i8, newShuffle);
      return truncated;
    }

    // The shuffle operation is not supported and needs expanding.
    DEBUG(dbgs() << "Shuffle expand selected for " << VecVT.getEVTString() << "\n");
    return SDValue();
}

SDValue SHAVELowering::SHAVELowerConstantPool(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  EVT valType = op.getValueType();
  ConstantPoolSDNode *CPN = cast<ConstantPoolSDNode>(op.getNode());
  SDValue CP;

  if (CPN->isMachineConstantPoolEntry())
    CP = DAG.getTargetConstantPool(CPN->getMachineCPVal(), CPN->getValueType(0), CPN->getAlign ());
  else
    CP = DAG.getTargetConstantPool(CPN->getConstVal(), CPN->getValueType(0), CPN->getAlign ());

  return DAG.getNode(SHAVEISD::LDISym, dbgLoc, valType, CP);
}

// There are no immediate Global Addresses in SHAVE. All addresses get loaded into
// registers first
bool SHAVELowering::isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const {
  return false;
}

SDValue SHAVELowering::LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(Op);
  GlobalAddressSDNode *GA = cast<GlobalAddressSDNode>(Op);
  SDValue address = DAG.getTargetGlobalAddress(GA->getGlobal(), dbgLoc, MVT::i32);
  SDValue GALoad = DAG.getNode(SHAVEISD::LDISym, dbgLoc, MVT::i32, address);
  unsigned OffsetOpc = 0;
  uint64_t Offset = 0;

  if (GA->getOffset() > 0) {
    OffsetOpc = ISD::ADD;
    Offset = (uint64_t)GA->getOffset();
  } else if (GA->getOffset() < 0) {
    OffsetOpc = ISD::SUB;
    Offset = (uint64_t)-GA->getOffset();
  }

  if (OffsetOpc) {
    SDValue OffsetVal = DAG.getConstant(Offset, dbgLoc, MVT::i32);
    GALoad = DAG.getNode(OffsetOpc, dbgLoc, MVT::i32, GALoad, OffsetVal);
  }

  return GALoad;
}

SDValue SHAVELowering::SHAVELowerBlockAddress(SDValue Op, SelectionDAG &DAG) const {
    SDLoc dbgLoc(Op);
    const BlockAddress *BA = cast<BlockAddressSDNode>(Op)->getBlockAddress();
    SDValue address = DAG.getTargetBlockAddress(BA, MVT::i32);

    return DAG.getNode(SHAVEISD::LDISym, dbgLoc, MVT::i32, address);
}

SDValue SHAVELowering::LowerJumpTable(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(Op);
  JumpTableSDNode *jtNode = cast<JumpTableSDNode>(Op);
  assert(jtNode && "Unexpected nullptr jump table node");

  unsigned jumpTableIndex = jtNode->getIndex();
  SDValue jumpTableEntry = DAG.getTargetJumpTable(jumpTableIndex, MVT::i32);

  // Ensure that the labels in the jump-table are marked as "address taken" or
  // they will not be emitted during assembly printing
  // FIXME: Movidius - would this be better put in 'SHAVELowerBR_JT()'?
  MachineFunction &MF = DAG.getMachineFunction();
  const MachineJumpTableInfo *jumpTableInfo = MF.getJumpTableInfo();
  assert(jumpTableInfo && "Unexpected nullptr jump table info");

  const std::vector<MachineJumpTableEntry> &jumpTableEntries = jumpTableInfo->getJumpTables();
  const std::vector<MachineBasicBlock*> &jumpTableBasicBlocks = jumpTableEntries[jumpTableIndex].MBBs;

  for (MachineBasicBlock *MBB : jumpTableBasicBlocks) {
    MBB->setMachineBlockAddressTaken();

    // This line is needed to set the hasAddressTaken flag on the BasicBlockobject
    BlockAddress::get(const_cast<BasicBlock*>(MBB->getBasicBlock()));
  }

  return DAG.getNode(SHAVEISD::LDISym, dbgLoc, jumpTableEntry.getValueType(), jumpTableEntry);
}

SDValue SHAVELowering::SHAVELowerSHR(SDValue op, SelectionDAG &DAG) const {
  assert(op.getValueType() == MVT::i32 && "Only MVT::i32 value type of SR[A|L]_PARTS must be lowered");

  SDLoc dbgLoc(op);
  unsigned opcode = 0;

  if (op.getOpcode() == ISD::SRA_PARTS)
    opcode = ISD::SRA;
  else if (op.getOpcode() == ISD::SRL_PARTS)
    opcode = ISD::SRL;
  else
    llvm_unreachable("unknown SRx_PARTS opcode");

  // opcode = ISD::SRA_PARTS or ISD::SRL_PARTS
  SDValue LoLSH = op.getOperand(0);
  SDValue HiLSH = op.getOperand(1);
  SDValue shamt = op.getOperand(2);

  // Shift the high part
  SDValue HiResult = DAG.getNode(opcode, dbgLoc, MVT::i32, HiLSH, shamt);

  // shamt >= 32
  SDValue shamt1 = DAG.getNode(ISD::SUB, dbgLoc, MVT::i32, shamt, DAG.getConstant(op.getValueSizeInBits(), dbgLoc, MVT::i32, false));
  // Compute the shamt that remains shamt1 = shamt - 32
  SDValue LoResult1 = DAG.getNode(opcode, dbgLoc, MVT::i32, HiLSH, shamt1);

  // shamt < 32
  SDValue shamt2 = DAG.getNode(ISD::SUB, dbgLoc, MVT::i32, DAG.getConstant(op.getValueSizeInBits(), dbgLoc, MVT::i32, false), shamt);
  // Shift left the high part to recover the bits that must be moved in the lower part
  SDValue shlHi = DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, HiLSH, shamt2);
  // Shift right the low part, to make space for the bits from the high part
  SDValue shrLo = DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, LoLSH, shamt);
  // Merge what's left from the low part with what comes from the high part
  SDValue LoResult2 = DAG.getNode(ISD::OR, dbgLoc, MVT::i32, shlHi, shrLo);

  // Now merge the both paths, depending on shamt using select_cc pseudo-instruction
  SDValue ops1[] = {shamt, DAG.getConstant(op.getValueSizeInBits(), dbgLoc, MVT::i32, false),
                    LoResult1, LoResult2, DAG.getTargetConstant(SHAVEISD::GTE, dbgLoc, MVT::i8)};
  SDValue LoResult = DAG.getNode(ISD::SELECT_CC, dbgLoc, MVT::i32, ArrayRef(ops1, 5));
  SDValue ops2[] = {LoResult, HiResult};
  SDValue retVal =  DAG.getMergeValues(ArrayRef(ops2, 2), dbgLoc);

  return retVal;
}

SDValue SHAVELowering::SHAVELowerSHL(SDValue op, SelectionDAG &DAG) const {
  assert(op.getValueType() == MVT::i32 && "Only MVT::i32 value type of SHL_PARTS must be lowered");

  SDLoc dbgLoc(op);
  SDValue LoLSH = op.getOperand(0);
  SDValue HiLSH = op.getOperand(1);
  SDValue shamt = op.getOperand(2);

  // Shift the low part
  SDValue LoResult = DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, LoLSH, shamt);

  // shamt >= 32
  SDValue shamt1 = DAG.getNode(ISD::SUB, dbgLoc, MVT::i32, shamt, DAG.getConstant(op.getValueSizeInBits(), dbgLoc, MVT::i32, false));
  // Compute the shamt that remains shamt1 = shamt - 32
  SDValue HiResult1 = DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, LoLSH, shamt1);

  // shamt < 32
  SDValue shamt2 = DAG.getNode(ISD::SUB, dbgLoc, MVT::i32, DAG.getConstant(op.getValueSizeInBits(), dbgLoc, MVT::i32, false), shamt);
  // Shift right the low part to get the outgoing bits
  SDValue shrLo = DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, LoLSH, shamt2);
  // Shift left the high part to make space for the incoming bits from the low part
  SDValue shlHi = DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, HiLSH, shamt);
  // Merge what's left from the high part with what comes from the lowPart
  SDValue HiResult2 = DAG.getNode(ISD::OR, dbgLoc, MVT::i32, shrLo, shlHi);

  // now merge the both paths, depending on shamt using select_cc pseudo-instruction
  SDValue ops1[] = {shamt, DAG.getConstant(op.getValueSizeInBits(), dbgLoc, MVT::i32, false),
    HiResult1, HiResult2, DAG.getTargetConstant(SHAVEISD::GTE, dbgLoc, MVT::i8)};
  SDValue HiResult = DAG.getNode(ISD::SELECT_CC, dbgLoc, MVT::i32, ArrayRef(ops1, 5));
  SDValue ops2[] = {LoResult, HiResult};
  SDValue retVal =  DAG.getMergeValues(ArrayRef(ops2, 2), dbgLoc);

  return retVal;
}

SDValue SHAVELowering::SHAVELowerSMUL_LOHI(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(Op);
  SDValue term1 = Op.getOperand(0);
  SDValue term2 = Op.getOperand(1);
  SDValue mulhs = DAG.getNode(ISD::MULHS, dbgLoc, MVT::i32, term1, term2);
  SDValue mulls = DAG.getNode(ISD::MUL, dbgLoc, MVT::i32, term1, term2);
  SDValue valuesToMerge[] = {mulls, mulhs};

  return DAG.getMergeValues(ArrayRef(valuesToMerge, 2), dbgLoc);
}

SDValue SHAVELowering::SHAVELowerUMUL_LOHI(SDValue Op, SelectionDAG &DAG) const {
  // Let op1 = 2 * shiftOp1 + lsb1
  // Nnd op2 = 2 * shiftOp2 + lsb2 where 0 <= lsb1, lsb2 <= 1
  // It comes that
  //  op1 * op2 = 4 * shiftOp1 * shiftOp2 + 2 * (lsb1 * shiftOp2 + lsb2 * shiftOp1) + lsb1 * lsb2
  // So
  //  op1 * op2 = (shiftOp1 * shiftOp2) << 4 + (lsb1 * shiftOp2 + lsb2 * shiftOp1) << 1 + lsb1 * lsb2

  SDValue op1 = Op.getOperand(0);
  SDValue op2 = Op.getOperand(1);
  SDLoc dbgLoc(Op);

  assert((op1.getValueType() == MVT::i32) &&
          (op2.getValueType() == MVT::i32) && "Expecting i32 operands!");


  // lsb1 = op1 % 2
  SDValue lsb1 = DAG.getNode(ISD::AND, dbgLoc, MVT::i32, op1,
                             DAG.getConstant(1, dbgLoc, MVT::i32));
  // lsb2 = op2 % 2
  SDValue lsb2 = DAG.getNode(ISD::AND, dbgLoc, MVT::i32, op2,
                             DAG.getConstant(1, dbgLoc, MVT::i32));

  // lsb12 = lsb1 * lsb2
  SDValue lsb12 = DAG.getNode(ISD::AND, dbgLoc, MVT::i32, lsb1, lsb2);

  // shiftOp1 = op1 / 2
  SDValue shiftOp1 = DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, op1,
                                 DAG.getConstant(1, dbgLoc, MVT::i32));
  // shiftOp2 = op2 / 2
  SDValue shiftOp2 = DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, op2,
                                 DAG.getConstant(1, dbgLoc, MVT::i32));

  SDValue mulh = DAG.getNode(ISD::MULHS, dbgLoc, MVT::i32, shiftOp1, shiftOp2);
  SDValue mull = DAG.getNode(ISD::MUL, dbgLoc, MVT::i32, shiftOp1, shiftOp2);

  SDValue shl2MulLoPart = DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, mull,
                                      DAG.getConstant(2, dbgLoc, MVT::i32));
  SDValue shl2MulHiPart = DAG.getNode(ISD::OR, dbgLoc, MVT::i32,
                                      DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, mulh,
                                                  DAG.getConstant(2, dbgLoc, MVT::i32)),
                                                  DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, mull,
                                                              DAG.getConstant(30, dbgLoc, MVT::i32)));

    SDValue low1 = DAG.getNode(ISD::MUL, dbgLoc, MVT::i32, shiftOp1, lsb2); // <= 2^31 - 1
    SDValue low2 = DAG.getNode(ISD::MUL, dbgLoc, MVT::i32, shiftOp2, lsb1); // <= 2^31 - 1

    // shiftOp1 * lsb2 + shiftOp2 * lsb1
    SDValue addLow12 = DAG.getNode(ISD::ADD, dbgLoc, MVT::i32, low1, low2); // <= 2^32 - 2

    SDValue shl1AddLowLoPart = DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, addLow12,
                                           DAG.getConstant(1, dbgLoc, MVT::i32));
    SDValue shl1AddLowHiPart = DAG.getNode(ISD::SRL, dbgLoc, MVT::i32, addLow12,
                                           DAG.getConstant(31, dbgLoc, MVT::i32));

    SDValue addLowCarryOut = DAG.getNode(ISD::ADDC, dbgLoc, DAG.getVTList(MVT::i32, MVT::Glue),
                                         shl2MulLoPart, shl1AddLowLoPart);
    SDValue hiPart = DAG.getNode(ISD::ADDE, dbgLoc, DAG.getVTList(MVT::i32, MVT::Glue),
                                 shl2MulHiPart, shl1AddLowHiPart, addLowCarryOut.getValue(1));

    SDValue loPart = DAG.getNode(ISD::ADD, dbgLoc, MVT::i32, addLowCarryOut.getValue(0), lsb12);

    SDValue valuesToMerge[] = {loPart, hiPart};
    SDValue mergeValues = DAG.getMergeValues(ArrayRef(valuesToMerge, 2), dbgLoc);

    return mergeValues;
}

SDValue SHAVELowering::SHAVELowerBR_JT(SDValue Op, SelectionDAG &DAG) const {
  // FIXME: Movidius - need to rewrite this as 'PseudoSourceValue::getJumpTable()' has been
  //        obsoleted in v3.8; also no other target uses this approach anymore
  SDLoc dbgLoc(Op);
  SDValue Chain = Op.getOperand(0);
  SDValue Table = Op.getOperand(1);
  SDValue Index = Op.getOperand(2);

  Index = DAG.getNode(ISD::SHL, dbgLoc, MVT::i32, Index, DAG.getConstant(2, dbgLoc, MVT::i32));

  SDValue Addr = DAG.getNode(ISD::ADD, dbgLoc, MVT::i32, Table, Index);
  SDValue Load = DAG.getExtLoad(ISD::SEXTLOAD, dbgLoc, MVT::i32, Chain, Addr,
                                MachinePointerInfo::getJumpTable(DAG.getMachineFunction()), MVT::i32);
  SDValue Brind = DAG.getNode(ISD::BRIND, dbgLoc, MVT::Other, Load.getValue(1), Load);

  return Brind;
}


SDValue SHAVELowering::SHAVELowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  // 'va_start' stores the address of the VarArgsFrameIndex slot into the memory location argument
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  SHAVEMachineFunctionInfo *machineFnInfo = MF.getInfo<SHAVEMachineFunctionInfo>();
  SDLoc dbgLoc(Op);
  Align align = SHAVEST.getFrameLowering()->getStackAlign();

  // Create a stack slot to save the value of the stack pointer at entry
  assert((machineFnInfo->getVarArgsStackOffset() > -1ll) && "Location for 'va_list' has not been initialised");

  int spAtEntrySlot = MFI.CreateFixedObject(getPointerTy(DAG.getDataLayout()).getStoreSize(),
                                             machineFnInfo->getVarArgsStackOffset(), true);

  // Set the alignment for the stack slot
  MFI.setObjectAlignment(spAtEntrySlot, align);

  // Get the frame index for the saved stack pointer
  SDValue spAtEntry = DAG.getFrameIndex(spAtEntrySlot, getPointerTy(DAG.getDataLayout()));
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();

  return DAG.getStore(Op.getOperand(0), dbgLoc, spAtEntry, Op.getOperand(1),
                      MachinePointerInfo(SV, 0), align);
}

SDValue SHAVELowering::SHAVELowerVAARG(SDValue Op, SelectionDAG &DAG) const {
#ifdef FIXME_VEC_VAARG
  dbgs() << "SHAVELowering::SHAVELowerVAARG: ";
  Op->dump(&DAG);
  dbgs() << "\n";
  SDValue opArg0 = Op.getOperand(0);
  SDValue opArg1 = Op.getOperand(1);
  SDValue opArg2 = Op.getOperand(2);
  SDValue opArg3 = Op.getOperand(3);

  dbgs() << "    Operand #0: "; opArg0->dump(); dbgs() << "\n";
  dbgs() << "    Operand #1: "; opArg1->dump(); dbgs() << "\n";
  dbgs() << "    Operand #2: "; opArg2->dump(); dbgs() << "\n";
  dbgs() << "    Operand #3: "; opArg3->dump(); dbgs() << "\n";
#endif // FIXME_VEC_VAARG
  SDLoc dbgLoc(Op);

  // Location where the 'va_list' pointer is stored
  SDValue vaListPtrLoc = Op.getOperand(1);
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  Align alignment = SHAVEST.getFrameLowering()->getStackAlign();
  const DataLayout dataLayout = DAG.getDataLayout();

  // Load the 'va_list' pointer, which points to the argument to be loaded
  SDValue vaListPtr = DAG.getLoad(getPointerTy(dataLayout), dbgLoc, Op.getOperand(0), vaListPtrLoc,
                                  MachinePointerInfo(SV, 0));

  // Get the type of the value being loaded
  EVT vaargVT = Op.getValue(0).getValueType();

  // This is the offset to next vararg, relative to the argument that is currently being loaded
  int offset = (vaargVT.getStoreSize() + (alignment.value() - 1)) &
               (~(alignment.value() - 1));

  // Compute the value where the next argument is located
  SDValue nextVAListPtr = DAG.getNode(ISD::ADD, dbgLoc, getPointerTy(dataLayout), vaListPtr, DAG.getConstant(offset, dbgLoc, getPointerTy(dataLayout)));

  // Store it to the requested location on the stack
  SDValue Chain = DAG.getStore(vaListPtr.getValue(1), dbgLoc, nextVAListPtr,
                          vaListPtrLoc, MachinePointerInfo(SV, 0));

  // Load the requested value from the variable argument list
  return DAG.getLoad(vaargVT, dbgLoc, Chain, vaListPtr, MachinePointerInfo((const Value*)nullptr, 0), alignment);
}

SDValue SHAVELowering::SHAVELowerVACOPY(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(Op);
  SDValue Chain = Op.getOperand(0);
  SDValue DestPtr = Op.getOperand(1);
  SDValue SrcPtr = Op.getOperand(2);
  const Value *DestSV = cast<SrcValueSDNode>(Op.getOperand(3))->getValue();
  const Value *SrcSV = cast<SrcValueSDNode>(Op.getOperand(4))->getValue();

  SDValue VAPtr = DAG.getLoad(MVT::i32, dbgLoc, Chain, SrcPtr, MachinePointerInfo(SrcSV, 0));
  Chain = VAPtr.getValue(1);
  Chain = DAG.getStore(Chain, dbgLoc, VAPtr, DestPtr, MachinePointerInfo(DestSV, 0));

  return Chain;
}


SDValue SHAVELowering::SHAVELowerXADDR(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(Op);
  unsigned reg = 0;

  if (Op.getOpcode() == ISD::RETURNADDR)
    reg = SHAVEST.getRegisterInfo()->getRARegister();
  else
    reg = SHAVERegisterInfo::getSPReg();

  return DAG.getCopyFromReg(DAG.getUNDEF(MVT::Other), dbgLoc, reg, getPointerTy(DAG.getDataLayout()));
}

// Performs in the similar way as the default SelectionDAGLegalize::ExpandDYNAMIC_STACKALLOC()
// with only difference that it creates a SHAVE::DYNALLOC pseudo instruction
// that it's been erased afterwards on EmitCustomDYNALLOC() where we can track the DYNALLOC
// and replace it with instructions for stack overflow checking.
SDValue SHAVELowering::SHAVELowerDYNAMIC_STACKALLOC(SDValue Op, SelectionDAG &DAG) const {
  unsigned SPReg = SHAVERegisterInfo::getSPReg();
  assert(SPReg && "SHAVE cannot require DYNAMIC_STACKALLOC expansion and"
    " not tell us which reg is the stack pointer!");

  // Get the inputs.
  EVT VT = Op.getValueType();
  SDValue chain = Op.getOperand(0);
  SDValue size = Op.getOperand(1);
  SDValue align = Op.getOperand(2);
  SDValue newSP = SDValue(Op.getNode(), 0);  // copy chain
  SDValue copyChain = SDValue(Op.getNode(), 1);
  SDLoc dbgLoc(Op);

  // The alignment must be a constant, and '0' or a power-of-2
  // FIXME: Movidius - TODO: we don't yet handle alignments other than the default, ignore for now
  ConstantSDNode *alignConstant = cast<ConstantSDNode>(align);
  //  assert(alignConstant != nullptr && "Non-constant Align in 'SHAVELowerDYNAMIC_STACKALLOC'");
  uint64_t alignment = alignConstant->getZExtValue();
  assert(((alignment == 0) || isPowerOf2_64((alignment))) &&
          "Alignment must be zero or a power-of-2");

  unsigned stackAlignment =
    DAG.getSubtarget().getFrameLowering()->getStackAlignment();

  // Chain the dynamic stack allocation so that it doesn't modify the stack
  // pointer when other instructions are using the stack.
  chain = DAG.getCALLSEQ_START(chain, 0, 0, dbgLoc);

  SDValue SP = DAG.getCopyFromReg(chain, dbgLoc, SPReg, VT);

  chain = SP.getValue(1);
  newSP = DAG.getNode(ISD::SUB, dbgLoc, VT, SP, size);       // Value

  if (alignment > stackAlignment) {
    newSP = DAG.getNode(ISD::AND, dbgLoc, VT, newSP,
      DAG.getConstant(-(int64_t)alignment, dbgLoc, VT));
  }
  chain = DAG.getCopyToReg(chain, dbgLoc, SPReg, newSP);     // Output chain

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  chain = DAG.getNode(SHAVEISD::DYNALLOC, dbgLoc, NodeTys, chain, newSP);

  copyChain = DAG.getCALLSEQ_END(chain, DAG.getIntPtrConstant(0, dbgLoc, true),
                                 DAG.getIntPtrConstant(0, dbgLoc, true), SDValue(), dbgLoc);

  SDValue Ops[2] = {newSP, copyChain};
  return DAG.getMergeValues(ArrayRef(Ops, 2), dbgLoc);
}

SDValue SHAVELowering::SHAVELowerFROUND(SDValue op, SelectionDAG &DAG) const {
  SDValue operand = op.getOperand(0);

  // We support f32 -> f16 fround
  if (operand.getValueType() != op.getValueType())
    return op;

  SDLoc dbgLoc(op);
  ArgListTy Args;
  const char *symbolName = getLibcallName(RTLIB::ROUND_F32);

  SDValue Callee = DAG.getExternalSymbol(symbolName, getPointerTy(DAG.getDataLayout()));
  Type *FloatTy = Type::getFloatTy(*DAG.getContext());
  SDValue Chain = DAG.getEntryNode();
  ArgListEntry Arg;

  Arg.Node = op.getOperand(0);
  Arg.Ty = FloatTy;
  Args.push_back(Arg);

  TargetLowering::CallLoweringInfo CLI(DAG);

  CLI.setDebugLoc(dbgLoc).setChain(Chain)
      .setCallee(CallingConv::C, FloatTy, Callee, std::move(Args));

  std::pair<SDValue, SDValue> CallInfo = LowerCallTo(CLI);

  return CallInfo.first;
}

SDValue SHAVELowering::LowerOperation(SDValue op, SelectionDAG &DAG) const {
  switch(op.getOpcode()) {
  case ISD::UINT_TO_FP :
  case ISD::SINT_TO_FP :
    return SHAVELowerINT_TO_FP(op, DAG);

  case ISD::FP_TO_UINT :
  case ISD::FP_TO_SINT :
    return SHAVELowerFP_TO_INT(op, DAG);

  case ISD::BR_CC :
    return SHAVELowerBR_CC(op, DAG);

  case ISD::BR_JT :
    return SHAVELowerBR_JT(op, DAG);

  case ISD::SELECT :
  case ISD::VSELECT :
    return SHAVELowerSELECT(op, DAG);

  case ISD::SETCC :
    return SHAVELowerSETCC(op, DAG);

  case ISD::ConstantFP :
    return SHAVELowerConstantFP(op, DAG);

  case ISD::BlockAddress :
    return SHAVELowerBlockAddress(op, DAG);

  case ISD::GlobalAddress :
    return LowerGlobalAddress(op, DAG);

  case ISD::JumpTable :
    return LowerJumpTable(op, DAG);

  case ISD::ConstantPool :
    return SHAVELowerConstantPool(op, DAG);

  case ISD::EXTRACT_SUBVECTOR:
    if (SHAVEST.hasFeature(SHAVE::HasVRF128_Feature))
      return SHAVELowerEXTRACT_SUBVECTOR(op, DAG);
    else
      return SHAVEv3LowerEXTRACT_SUBVECTOR(op, DAG);

  case ISD::VECTOR_SHUFFLE :
    if (hasFeature(SHAVE::HasVRF512_Feature))
      return SHAVEv3LowerVECTOR_SHUFFLE(op, DAG);
    else
      return SHAVELowerVECTOR_SHUFFLE(op, DAG);

  case ISD::BUILD_VECTOR :
    if (hasFeature(SHAVE::HasVRF512_Feature))
      return SHAVEv3LowerBUILD_VECTOR(op, DAG);
    else
      return SHAVELowerBUILD_VECTOR(op, DAG);

  case ISD::EXTRACT_VECTOR_ELT :
    return SHAVELowerEXTRACT_VECTOR_ELT(op, DAG);

  case ISD::CONCAT_VECTORS :
    if (hasFeature(SHAVE::HasVRF512_Feature))
      return SHAVEv3LowerCONCAT_VECTORS(op, DAG);
    else
      return SHAVELowerCONCAT_VECTORS(op, DAG);

  case ISD::SCALAR_TO_VECTOR :
    return SHAVELowerSCALAR_TO_VECTOR(op, DAG);

  case ISD::CTLZ:
  case ISD::CTLZ_ZERO_UNDEF:
    return SHAVELowerCTLZ(op, DAG);

  case ISD::CTTZ:
  case ISD::CTTZ_ZERO_UNDEF:
    return SHAVELowerCTTZ(op, DAG);

  case ISD::SIGN_EXTEND:
    return SHAVELowerSIGN_EXTEND(op, DAG);

  case ISD::ZERO_EXTEND:
    return SHAVELowerZERO_EXTEND(op, DAG);

  case ISD::ANY_EXTEND:
    return SHAVELowerANY_EXTEND(op, DAG);

  case ISD::SRA_PARTS :
  case ISD::SRL_PARTS :
    return SHAVELowerSHR(op, DAG);

  case ISD::SHL_PARTS :
    return SHAVELowerSHL(op, DAG);

  case ISD::SMUL_LOHI:
    return SHAVELowerSMUL_LOHI(op, DAG);

  case ISD::UMUL_LOHI:
    return SHAVELowerUMUL_LOHI(op, DAG);

  case ISD::VASTART:
    return SHAVELowerVASTART(op, DAG);

  case ISD::VAARG:
    return SHAVELowerVAARG(op, DAG);

  case ISD::VACOPY:
    return SHAVELowerVACOPY(op, DAG);

  case ISD::RETURNADDR:
  case ISD::FRAMEADDR:
    return SHAVELowerXADDR(op, DAG);

  case ISD::DYNAMIC_STACKALLOC:
    return SHAVELowerDYNAMIC_STACKALLOC(op, DAG);

  case ISD::ADD:
  case ISD::FADD: {
    EVT type = op->getValueType(0);
    if (type == MVT::i32 || type == MVT::i16 || type == MVT::i8 ||
        ((type == MVT::f32 || type == MVT::f16) && (op->getFlags().hasAllowContract() || op->getFlags().hasAllowReassociation()))) {
      SDValue horizontalOp = SHAVELowerHorizontalOp(op, DAG);
      if (horizontalOp != op)
        return horizontalOp;
    }
  }
    // else fallthrough
  case ISD::SUB:
  case ISD::ROTL:
  case ISD::SHL:
  case ISD::SRA:
  case ISD::SRL:
  case ISD::FSUB:
    return SHAVELowerADD_SUB_ROTL_SHL_SRA_SRL(op, DAG);

  case ISD::MUL:
  case ISD::FMUL:
    return SHAVELowerMUL(op, DAG);

  case ISD::AND:
  case ISD::XOR:
  case ISD::OR:
    if (op->getValueType(0) == MVT::i64)
      return SHAVELower64BitOp(op, DAG);
    else
      return SHAVELowerHorizontalOp(op, DAG);

  case ISD::FROUND:
    return SHAVELowerFROUND(op, DAG);

  case ISD::TRUNCATE:
    return SHAVELowerTRUNCATE(op, DAG);

  case ISD::MLOAD:
    return SHAVELowerMLOAD(op, DAG);

  case ISD::MSTORE:
    if (hasFeature(SHAVE::HasVRF512_Feature)) {
      return SHAVEv3LowerMSTORE(op, DAG);
    } else {
      return SHAVELowerMSTORE(op, DAG);
    }

  case ISD::LOAD:
    return SHAVELowerLOAD(op, DAG);

  case ISD::STORE:
    return SHAVELowerSTORE(op, DAG);

  case ISD::SMIN:
  case ISD::UMIN:
  case ISD::SMAX:
  case ISD::UMAX:
    return SHAVELowerMIN_MAX(op, DAG);

  case ISD::FTRUNC:
    return SHAVELowerFTRUNC(op, DAG);

  case ISD::FCOPYSIGN:
    if (hasFeature(SHAVE::HasVRF512_Feature))
      return SHAVEv3LowerFCOPYSIGN(op, DAG);
    break;

  case ISD::FMINNUM:
  case ISD::FMAXNUM:
    return SHAVELowerFMINNUM_FMAXNUM(op, DAG);

  case ISD::FMINIMUM:
  case ISD::FMAXIMUM:
    return SHAVELowerFMINIMUM_FMAXIMUM(op, DAG);

  case ISD::FSQRT:
    // If denormals are to be preserved, then we cannot use this lowering as all input and output denormals are flushed to zero
    if (hasFeature(SHAVE::HasVRF512_Feature) && DAG.getDenormalMode(MVT::f16).outputsAreZero())
      return op;
    return SDValue();

  case ISD::FLOG:
  case ISD::FEXP:
    if (hasFeature(SHAVE::HasVRF512_Feature))
      return SHAVEv3LowerMathOperation(op, DAG);
    return SDValue(); // Otherwise expand
  }

  return op;
}

/// isVectorLoadExtDesirable - Return true if folding a vector load into
/// ExtVal (a sign, zero, or any extend node) is profitable.
bool SHAVELowering::isVectorLoadExtDesirable(SDValue ExtVal) const {
  EVT VT = ExtVal.getValueType();
  if (!isTypeLegal(VT))
    return false;

  bool isValidExt = false;

  // FIXME: Movidius - Should this be extended to VRF512?
  if (hasFeature(SHAVE::HasVRF128_Feature)){
    // For now, only generate extload for these conversions
    isValidExt |= (ExtVal.getSimpleValueType() == MVT::v4i32 &&
                   ExtVal.getOperand(0).getSimpleValueType() == MVT::v4i16);

    isValidExt |= (ExtVal.getSimpleValueType() == MVT::v8i16 &&
                   ExtVal.getOperand(0).getSimpleValueType() == MVT::v8i8);

    isValidExt |= (ExtVal.getSimpleValueType() == MVT::v4i32 &&
                   ExtVal.getOperand(0).getSimpleValueType() == MVT::v4i8);
  }
  return isValidExt;
}

/// isLegalAddressingMode - Return true if the addressing mode represented
/// by AM is legal for this target, for a load/store of the specified type.
bool SHAVELowering::isLegalAddressingMode(const DataLayout &DL,
                                          const AddrMode &AM,
                                          Type *Ty,
                                          unsigned AddrSpace,
                                          Instruction *I) const {
  if (SHAVEST.getLDOSTO_OffsetBits() == 13u) {
    // Allows a sign-extended 13-bit immediate field.
    if (AM.BaseOffs <= -(1LL << 12) || AM.BaseOffs >= (1LL << 12)-1)
      return false;
  }

  // Allows a sign-extended 15-bit immediate field.
  if (AM.BaseOffs <= -(1LL << 15) || AM.BaseOffs >= (1LL << 15)-1)
    return false;

  // No global is ever allowed as a base.
  if (AM.BaseGV)
    return false;

  // Support r+i and r+r (Myriad 2 only)
  switch (AM.Scale) {
  default:
    return false;
  case 0:  // "r+i" or just "i", depending on HasBaseReg.
    break;
  case 1:
    if (AM.HasBaseReg)  // "r+r" is only allowed on Myriad2.
      return !AM.BaseOffs;
    break;
  }

  return true;
}

bool SHAVELowering::getPostIndexedAddressParts(SDNode *N, SDNode * Op, SDValue &Base,
           SDValue &Offset, ISD::MemIndexedMode &AM, SelectionDAG &DAG) const {
  // Fetch the opcode of the supposed incrementing/decrementing node
  int opcode = Op->getOpcode();
  LSBaseSDNode *lsNode = cast<LSBaseSDNode>(N);

  // Fetch the in-memory size of the value to store
  unsigned size = lsNode->getMemoryVT().getStoreSize();

  if (Op->getOperand(1).getOpcode() == ISD::Constant) {
    // Fetch the value of the potential increment
    if (ConstantSDNode *incrementNode = dyn_cast<ConstantSDNode>(Op->getOperand(1).getNode())) {
      int64_t increment = incrementNode->getSExtValue();

      AM = ISD::UNINDEXED;

      if ((increment == -size && opcode == ISD::ADD) || (increment == size && opcode == ISD::SUB))
	AM = ISD::POST_DEC;
      else if ((increment == size && opcode == ISD::ADD) || (increment == -size && opcode == ISD::SUB))
	AM = ISD::POST_INC;

      if (AM != ISD::UNINDEXED) {
	// We know what type of post inc/dec operations is this
	Base = Op->getOperand(0);
	Offset = Op->getOperand(1);
	return true;
      }
    }
  }

  // Conservatively...
  return false;
}

SDValue SHAVELowering::PerformDAGCombine(SDNode *N,
                                         DAGCombinerInfo &DCI) const {
  switch (N->getOpcode()) {
  default:
    // Default implementation: no optimization.
    return SDValue();
  case ISD::EXTRACT_VECTOR_ELT:
    return CombineEXTRACT_VECTOR_ELT(N, DCI);
  case ISD::INSERT_VECTOR_ELT:
    return CombineINSERT_VECTOR_ELT(N, DCI);
  case ISD::BUILD_VECTOR:
    return CombineBUILD_VECTOR(N, DCI);
  case ISD::SELECT_CC:
    return CombineSELECT_CC(N, DCI);
  case ISD::VSELECT:
    return CombineVSELECT(N, DCI);
  case ISD::SDIV:
    return CombineSDIV(N, DCI);
  case ISD::SREM:
    return CombineSREM(N, DCI);
  case ISD::SINT_TO_FP:
    return CombineSINT_TO_FP(N, DCI);
  case SHAVEISD::MIN:
  case SHAVEISD::MAX:
    return CombineMIN_MAX(N, DCI);
  case ISD::FSUB:
  case ISD::FADD:
    return CombineFADD(N, DCI);
  case ISD::SUB:
  case ISD::ADD:
    return CombineADD(N, DCI);
  }
}

SDValue SHAVELowering::CombineEXTRACT_VECTOR_ELT(SDNode *N, DAGCombinerInfo &DCI) const {
  ConstantSDNode *Index = dyn_cast<ConstantSDNode>(N->getOperand(1).getNode());

  if(!Index)
    return SDValue();

  // Walk into the definition chain of the vector to find a possible insert operation
  SDValue Node(N, 0);
  SDLoc dbgLoc(N);

  while (true) {
    Node = Node->getOperand(0);

    switch(Node->getOpcode()) {
    case ISD::UNDEF: {
      // Walking up a chain of insert operations may end in an undefined node
      EVT VT = N->getValueType(0);
      return DCI.DAG.getNode(ISD::UNDEF, dbgLoc, VT);
    }
    case ISD::INSERT_VECTOR_ELT: {
      // Combine extract(insert(V, val, i), i) -> val
      ConstantSDNode *InsertIndex = dyn_cast<ConstantSDNode>(Node->getOperand(2));
      if( !InsertIndex )
        return SDValue();

      if( Index->getZExtValue() == InsertIndex->getZExtValue() )
        return Node->getOperand(1);

      continue;
    }
    case ISD::BUILD_VECTOR:
      // Combine extract(buildvector(val_0, val_1, val_2, ...), i) -> val_i
      return Node->getOperand(Index->getZExtValue());

    case SHAVEISD::SPLAT:
      // Combine extract(splat(val), i) -> val
      return Node->getOperand(0);

    default:
      return SDValue();
    }
  }
}

SDValue SHAVELowering::CombineINSERT_VECTOR_ELTIntoSPLAT(SDNode * N, DAGCombinerInfo &DCI) const {
  EVT VT = N->getValueType(0);
  SDValue Root(N, 0);
  SDValue FirstVal = N->getOperand(1);
  ConstantSDNode *PosNode = dyn_cast<ConstantSDNode>(N->getOperand(2));

  // FIXME: Movidius - Don't attempt to combine insert_vector_elt into a Shuffle for 16/32 bit
  //                  vectors. Shuffle of 16/32 bit vectors is being expanded as a temporary
  //                  workaround for bug #23317
  if (VT.getSizeInBits() <= 32)
    return SDValue();

  if (!PosNode)
    return SDValue();

  unsigned Pos = PosNode->getZExtValue();

  if ((Pos + 1) != VT.getVectorNumElements())
    return SDValue();

  unsigned PrevPos = Pos + 1;

  while (true) {
    // Make sure that all insert_vector_elt nodes use the same scalar value.
    SDValue Val = Root.getOperand(1);

    if (!Val || (Val != FirstVal))
      return SDValue();

    // Make sure that the insertion positions are sequential.
    PosNode = cast<ConstantSDNode>(Root.getOperand(2));
    Pos = PosNode->getZExtValue();

    if ((Pos + 1) != PrevPos)
      return SDValue();

    PrevPos = Pos;

    // Make sure the next source is an insert_vector_elt node or undef.
    SDValue Source = Root.getOperand(0);

    if (Pos == 0) {
      if (Source->getOpcode() == ISD::UNDEF)
        break;
      else
        return SDValue();
    }
    else if (Source->getOpcode() != ISD::INSERT_VECTOR_ELT)
      return SDValue();

    Root = Source;
  }

  // Replace N-1 inserts by a shuffle vector instruction.
  SmallVector<int, 16> SplatMask(VT.getVectorNumElements(), 0);

  return DCI.DAG.getVectorShuffle(VT, SDLoc(N), Root, Root,
    SplatMask);
}

static bool getExtractVectorElementInfo(SDValue start, SDValue &source, SDValue &index, SelectionDAG &DAG) {
  // This function helps to match the pattern emitted by SHAVELowering::SHAVELowerEXTRACT_VECTOR_ELT for certain
  // vector types. It returns true if it has successfully found the source and index of the vector element extract
  if (start.getOpcode() == ISD::EXTRACT_VECTOR_ELT) {
    source = start.getOperand(0);
    index = start.getOperand(1);
    return true;
  }

  if (start.getValueType() == MVT::i8) {
    if (start.getOpcode() == ISD::TRUNCATE && start.getOperand(0).getValueType() == MVT::i16) {
      SDValue extract = start.getOperand(0);
      unsigned offset = 0;

      if (extract.getOpcode() == ISD::SRL) {
        if (!isa<ConstantSDNode>(extract.getOperand(1)) || extract.getConstantOperandVal(1) != 8)
          return false;
        extract = extract.getOperand(0);
        offset = 1;
      }

      if (extract.getOpcode() != ISD::EXTRACT_VECTOR_ELT || !isa<ConstantSDNode>(extract.getOperand(1)))
        return false;

      SDValue bitcast = extract.getOperand(0);
      if (bitcast.getOpcode() != ISD::BITCAST)
        return false;

      offset += extract.getConstantOperandVal(1)*2;

      source = bitcast.getOperand(0);
      index = DAG.getConstant(offset, SDLoc(start), MVT::i32);

      return true;
    }
  }

  return false;
}

// FIXME: Movidius - We currently don't have an implementation for subvector insert
//                   with a variable index. When there is one, this block can be re-enabled
#if 0
SDValue SHAVELowering::CombineINSERT_VECTOR_ELTIntoSHAVE_INSERT_SUBVECTOR(SDNode *N, DAGCombinerInfo &DCI) const {
  // This function attempts to match the following pattern
  //
  //   insertelement
  //       insertelement
  //           ...
  //               insertelement
  //                   %anything
  //                   extractelement
  //                       %src
  //                       n-1
  //                   add
  //                       %base_index
  //                       n-1
  //           extractelement
  //               %src
  //               1
  //           add
  //               %base_index
  //               1
  //       extractelement
  //           %src
  //           0
  //       %base_index
  //
  // and then reduces it to a single
  // SHAVEISD::INSERT_SUBVECTOR %anything, %src, %base_index

  SDValue currentInsert = SDValue(N, 0);
  SDValue vector1, vector2, index;

  bool foundExtract;

  EVT sourceVT = currentInsert.getValueType();
  bool hasConstantIndex = isa<ConstantSDNode>(currentInsert.getOperand(2));

  if (!hasConstantIndex) {
    if (currentInsert.getOperand(2).getOpcode() != ISD::ADD)
      return SDValue();

    SDValue currentSource, currentIndex, currentExtract;
    SDValue addBase = currentInsert.getOperand(2).getOperand(0);

    currentExtract = currentInsert.getOperand(1);
    foundExtract = getExtractVectorElementInfo(currentExtract, currentSource, currentIndex, DCI.DAG);

    if (!foundExtract)
      return SDValue();

    vector2 = currentSource;
    unsigned currentOffset = vector2.getSimpleValueType().getVectorNumElements();

    while (currentOffset > 0) {
      currentOffset--;

      if (currentInsert.getOpcode() != ISD::INSERT_VECTOR_ELT)
        return SDValue();

      currentExtract = currentInsert.getOperand(1);
      foundExtract = getExtractVectorElementInfo(currentExtract, currentSource, currentIndex, DCI.DAG);

      if (!foundExtract)
        return SDValue();

      if (currentSource != vector2)
        return SDValue();

      if (!isa<ConstantSDNode>(currentIndex))
        return SDValue();

      if (cast<ConstantSDNode>(currentIndex)->getZExtValue() != currentOffset)
        return SDValue();

      SDValue currentAdd = currentInsert.getOperand(2);

      if (currentOffset == 0) {
        if (currentAdd != addBase)
          return SDValue();

        vector1 = currentInsert.getOperand(0);
      }
      else {
        if (currentAdd.getOpcode() != ISD::ADD)
          return SDValue();

        if (currentAdd.getOperand(0) != addBase)
          return SDValue();

        if (!isa<ConstantSDNode>(currentAdd.getOperand(1)))
          return SDValue();

        if (currentAdd.getConstantOperandVal(1) != currentOffset)
          return SDValue();
      }

      currentInsert = currentInsert.getOperand(0);
    }

    if (vector1.getNode() == nullptr || vector2.getNode() == nullptr || addBase.getNode() == nullptr)
      return SDValue();

    return DCI.DAG.getNode(SHAVEISD::INSERT_SUBVECTOR, SDLoc(N), N->getValueType(0), vector1, vector2, addBase);
  }

  return SDValue();
}
#endif

SDValue SHAVELowering::CombineINSERT_VECTOR_ELTIntoSHAVE_EXTRACT_SUBVECTOR(SDNode * N, DAGCombinerInfo &DCI) const {
  // This function attempts to match the following pattern
  //
  //   insertelement
  //       insertelement
  //           ...
  //               insertelement
  //                   undef
  //                   extractelement
  //                       %src
  //                       %base_index
  //                   0
  //           extractelement
  //               %src
  //               add
  //                   %base_index
  //                   n - 2
  //           n - 2
  //       extractelement
  //           %src
  //           add
  //               %base_index
  //               n - 1
  //       n - 1
  //
  // and then reduces it to a single
  // SHAVEISD::EXTRACT_SUBVECTOR %src

  SDValue currentInsert = SDValue(N, 0);
  SDValue currentExtract = N->getOperand(1);
  SDValue source, index, baseIndex, currentAdd;
  SDValue extractSubvectorIndex;

  bool foundExtract = getExtractVectorElementInfo(currentExtract, source, index, DCI.DAG);

  if (!foundExtract)
    return SDValue();

  if (source.getValueSizeInBits() <= currentInsert.getValueSizeInBits())
    return SDValue();

  // Only produce EXTRACT_SUBVECTOR for 128-bit and smaller vectors with 32-bit and smaller elements
  if (source.getValueSizeInBits() > 128 || source.getValueType().getScalarSizeInBits() > 32)
    return SDValue();

  if (!isa<ConstantSDNode>(currentInsert.getOperand(2).getNode()))
    return SDValue();

  bool hasConstantBaseIndex = false;
  unsigned constantBaseIndex = 0;
  unsigned currentOffset, lowestOffset;
  unsigned insertIndex = currentInsert.getConstantOperandVal(2);

  if (isa<ConstantSDNode>(index)) {
    hasConstantBaseIndex = true;
    constantBaseIndex = cast<ConstantSDNode>(index)->getZExtValue() - (N->getValueType(0).getVectorNumElements() - 1);
    currentOffset = cast<ConstantSDNode>(index)->getZExtValue() - constantBaseIndex;
  }
  else {
    currentAdd = index;
    if (currentAdd.getOpcode() != ISD::ADD || !isa<ConstantSDNode>(currentAdd.getOperand(1)))
      return SDValue();

    currentOffset = currentAdd.getConstantOperandVal(1);
    if (currentOffset >= source.getValueType().getVectorNumElements())
      return SDValue();

    baseIndex = currentAdd.getOperand(0);
  }

  if (insertIndex != currentOffset)
    return SDValue();

  if (currentOffset < (N->getValueType(0).getVectorNumElements() - 1))
    return SDValue();

  lowestOffset = currentOffset - (N->getValueType(0).getVectorNumElements() - 1);

  while (currentOffset > lowestOffset) {
    --currentOffset;

    if (currentInsert.getOperand(0).getOpcode() != ISD::INSERT_VECTOR_ELT)
      return SDValue();

    currentInsert = currentInsert.getOperand(0);

    if (!isa<ConstantSDNode>(currentInsert.getOperand(2).getNode()))
      return SDValue();

    insertIndex = currentInsert.getConstantOperandVal(2);

    if (insertIndex != currentOffset)
      return SDValue();

    SDValue currentSource, currentIndex;
    foundExtract = getExtractVectorElementInfo(currentInsert.getOperand(1), currentSource, currentIndex, DCI.DAG);

    if (!foundExtract)
      return SDValue();

    if (currentSource != source)
      return SDValue();

    if (hasConstantBaseIndex) {
      if (!isa<ConstantSDNode>(currentIndex))
        return SDValue();

      if (cast<ConstantSDNode>(currentIndex)->getZExtValue() != (constantBaseIndex + currentOffset))
        return SDValue();
    }
    else {
      if (currentOffset == lowestOffset) {
        if (currentIndex != baseIndex)
          return SDValue();
        if (currentInsert.getOperand(0).getOpcode() != ISD::UNDEF)
          return SDValue();

        extractSubvectorIndex = currentIndex;
      }
      else {
        if (currentIndex.getOpcode() != ISD::ADD)
          return SDValue();

        currentAdd = currentIndex;

        if (currentAdd.getOperand(0) != baseIndex)
          return SDValue();
        if (!isa<ConstantSDNode>(currentAdd.getOperand(1)) || currentAdd.getConstantOperandVal(1) != currentOffset)
          return SDValue();
      }
    }
  }

  if (hasConstantBaseIndex)
    extractSubvectorIndex = DCI.DAG.getConstant(constantBaseIndex, SDLoc(N), MVT::i32);

  // If we get here, then the current ISD::INSERT_VECTOR_ELT node is the root of a chain that performs
  // a subvector extract
  return DCI.DAG.getNode(SHAVEISD::EXTRACT_SUBVECTOR, SDLoc(N), N->getValueType(0), source, extractSubvectorIndex);
}

SDValue SHAVELowering::CombineINSERT_VECTOR_ELT(SDNode *N, DAGCombinerInfo &DCI) const {
  SDValue splat = CombineINSERT_VECTOR_ELTIntoSPLAT(N, DCI);
  if (splat.getNode() != nullptr)
    return splat;

  // FIXME: Movidius - We currently don't have an implementation for subvector insert
  //                   with a variable index. When there is one, this block can be re-enabled
#if 0
  SDValue insert_subvector = CombineINSERT_VECTOR_ELTIntoSHAVE_INSERT_SUBVECTOR(N, DCI);
  if (insert_subvector.getNode() != nullptr)
    return insert_subvector;
#endif

  SDValue extract_subvector = CombineINSERT_VECTOR_ELTIntoSHAVE_EXTRACT_SUBVECTOR(N, DCI);
  if (extract_subvector.getNode() != nullptr)
    return extract_subvector;

  return SDValue();
}

SDValue SHAVELowering::CombineBUILD_VECTOR(SDNode *N,
                                           DAGCombinerInfo &DCI) const {
  return OptimizeBUILD_VECTOR(N, DCI.DAG, DCI.isBeforeLegalize());
}


namespace {
  // Check if two SDValues are equivalent constants after casting WideConst to CastType using operation OpCode
  bool IsCastEquivalentConstant(SDValue WideConst, SDValue ShortConst, Type *CastType, unsigned OpCode) {
    const Constant *C1 = nullptr;
    const Constant *C2 = nullptr;

    // Translate SDNode OpCode to Instruction OpCode
    unsigned InstOpCode = 0;

    switch (OpCode) {
    case ISD::TRUNCATE:   InstOpCode = Instruction::Trunc; break;
    case ISD::FP_TO_UINT: InstOpCode = Instruction::FPToUI; break;
    case ISD::FP_TO_SINT: InstOpCode = Instruction::FPToSI; break;

    default:
      return false; // Unsupported conversion type
    }

    // Normal constant nodes
    if ((isa<ConstantSDNode>(WideConst.getNode()) || isa<ConstantFPSDNode>(WideConst.getNode()))
          && isa<ConstantSDNode>(ShortConst.getNode())) {

      if (isa<ConstantSDNode>(WideConst.getNode()))
        C1 = cast<ConstantSDNode>(WideConst.getNode())->getConstantIntValue();
      else
        C1 = cast<ConstantFPSDNode>(WideConst.getNode())->getConstantFPValue();

      C2 = cast<ConstantSDNode>(ShortConst.getNode())->getConstantIntValue();

      // FIXME: Movidius - const_cast is nasty but it seems that there is no const Constant* version of this interface...
      APInt V1 = cast<ConstantInt>(ConstantFoldCastInstruction(InstOpCode, const_cast<Constant*>(C1), CastType))->getValue();
      APInt V2 = cast<ConstantInt>(C2)->getValue();

      return V1 == V2;
    }

    // Vector constant nodes are presented as BUILD_VECTOR nodes, check all operands for equality
    if ((WideConst.getOpcode() == ISD::BUILD_VECTOR) && (ShortConst.getOpcode() == ISD::BUILD_VECTOR)) {
      unsigned NumElements = WideConst.getNumOperands();
      bool IsCastEquivalent = true;

      for (unsigned i = 0; i < NumElements; i++) {
        SDValue WideConstI = WideConst.getOperand(i);
        SDValue ShortConstI = ShortConst.getOperand(i);

        if ((isa<ConstantSDNode>(WideConstI.getNode()) || isa<ConstantFPSDNode>(WideConstI.getNode()))
              && isa<ConstantSDNode>(ShortConstI.getNode())) {
          if (isa<ConstantSDNode>(WideConstI.getNode()))
            C1 = cast<ConstantSDNode>(WideConstI.getNode())->getConstantIntValue();
          else
            C1 = cast<ConstantFPSDNode>(WideConstI.getNode())->getConstantFPValue();

          C2 = cast<ConstantSDNode>(ShortConstI.getNode())->getConstantIntValue();

          // FIXME: Movidius - const_cast is nasty but it seems that there is no const Constant* version of this interface...
          APInt V1 = cast<ConstantInt>(ConstantFoldCastInstruction(InstOpCode, const_cast<Constant*>(C1), CastType))->getValue();
          APInt V2 = cast<ConstantInt>(C2)->getValue();

          IsCastEquivalent &= (V1 == V2);
        } else
          IsCastEquivalent = false;
      }

      return IsCastEquivalent;
    }

    return false;
  }

  // Try to combine a select_cc or vselect+setcc node into a min/max node.
  SDValue SelectMinOrMax(SDValue LHS, SDValue RHS, SDValue TrueVal,
                         SDValue FalseVal, ISD::CondCode CC,
                         SDLoc dbgLoc, SelectionDAG &DAG) {
    EVT VT = TrueVal.getValueType();
    EVT CompareVT = LHS.getValueType();

    if ((VT == EVT(MVT::i64)) || (CompareVT == EVT(MVT::i64)))
      return SDValue();

    // Detect min/max patterns.
    bool MinMaxFormat1 = (LHS.getNode() == TrueVal.getNode()) && (RHS.getNode() == FalseVal.getNode());
    bool MinMaxFormat2 = (LHS.getNode() == FalseVal.getNode()) && (RHS.getNode() == TrueVal.getNode());

    // Typecast compare-and-select gets normalized to have the typecast between
    //  the compare and the select.  Match this pattern to typecast min/max
    //  operations.
    //
    // This checks for the following kind of pattern:
    //
    // %cc = icmp ult %a, %C
    // %t = trunc to X %a
    // %r = select %cc, %a, trunc to X %C
    //
    // which can be rewritten to
    // %cc = icmp ult %a, %C
    // %rl = select %cc, %a, %C
    // %r  = trunc to X %rl
    //
    // such that it reduces to
    // %rl = min %a, %C
    // %r  = trunc to X %rl
    //
    // Alternative typecasts such as ftoui and ftosi are also considered.

    bool NeedsCast = false;
    unsigned CastOperation = ISD::TRUNCATE;

    if (!MinMaxFormat1 && !MinMaxFormat2) {
      bool TrueValIsCast = TrueVal.getOpcode() == ISD::TRUNCATE
                              || TrueVal.getOpcode() == ISD::FP_TO_UINT
                              || TrueVal.getOpcode() == ISD::FP_TO_SINT;
      bool FalseValIsCast = FalseVal.getOpcode() == ISD::TRUNCATE
                              || FalseVal.getOpcode() == ISD::FP_TO_UINT
                              || FalseVal.getOpcode() == ISD::FP_TO_SINT;

      NeedsCast = true;

      Type *Ty = VT.getScalarType().getTypeForEVT(*DAG.getContext());

      if (TrueValIsCast && FalseValIsCast) {
        // Not sure if this needed but just to be sure.
        //
        // Match (for example)
        // %cc = icmp COND %a, %b
        // %ta = trunc to X %a
        // %tb = trunc to X %b
        // %r = select %cc, %ta, %tb
        //
        // Also match patterns with swapped inputs
        SDValue LongTrueVal = TrueVal->getOperand(0);
        SDValue LongFalseVal = FalseVal->getOperand(0);

        CastOperation = TrueVal.getOpcode();

        MinMaxFormat1 = (LHS.getNode() == LongTrueVal.getNode()) && (RHS.getNode() == LongFalseVal.getNode());
        MinMaxFormat2 = (LHS.getNode() == LongFalseVal.getNode()) && (RHS.getNode() == LongTrueVal.getNode());
      } else if (TrueValIsCast) {
        // Match
        // %cc = icmp COND %a, CONST
        // %ta = trunc to X %a
        // %r = select %cc, %ta, trunc to X CONST
        SDValue LongValue = TrueVal->getOperand(0);

        CastOperation = TrueVal.getOpcode();

        if (LongValue.getNode() == LHS.getNode())
          MinMaxFormat1 = IsCastEquivalentConstant(RHS, FalseVal, Ty, CastOperation);

        if (LongValue.getNode() == RHS.getNode())
          MinMaxFormat2 = IsCastEquivalentConstant(LHS, FalseVal, Ty, CastOperation);
      } else if (FalseValIsCast) {
        // Match
        // %cc = icmp COND %a, CONST
        // %ta = trunc to X %a
        // %r = select %cc, trunc to X CONST, %ta
        SDValue LongValue = FalseVal->getOperand(0);

        CastOperation = FalseVal.getOpcode();

        if (LongValue.getNode() == LHS.getNode())
          MinMaxFormat2 = IsCastEquivalentConstant(RHS, TrueVal, Ty, CastOperation);

        if (LongValue.getNode() == RHS.getNode())
          MinMaxFormat1 = IsCastEquivalentConstant(LHS, TrueVal, Ty, CastOperation);
      }
    }

    if (MinMaxFormat1 || MinMaxFormat2) {
      // Select the min/max instruction based on the condition code.
      unsigned MinMaxOpc = 0;

      switch (CC) {
      default:
        MinMaxOpc = 0;
        break;
      case ISD::SETULT:
      case ISD::SETOLT:
      case ISD::SETLT:
      case ISD::SETULE:
      case ISD::SETOLE:
      case ISD::SETLE:
        MinMaxOpc = MinMaxFormat1 ? SHAVEISD::MIN : SHAVEISD::MAX;
        break;
      case ISD::SETUGT:
      case ISD::SETOGT:
      case ISD::SETGT:
      case ISD::SETUGE:
      case ISD::SETOGE:
      case ISD::SETGE:
        MinMaxOpc = MinMaxFormat1 ? SHAVEISD::MAX : SHAVEISD::MIN;
        break;
      }

      // Emit the min/max instruction if we matched the pattern.
      if (MinMaxOpc) {
        bool IsUnsignedInt = !SHAVECC::isSignedComparison(CC) && !CompareVT.isFloatingPoint();
        SDValue SignedFlag = DAG.getTargetConstant(IsUnsignedInt, dbgLoc, MVT::i32);

        // Canonicalise the operand order: any min/max should be in LHS.
        if ((RHS.getOpcode() == SHAVEISD::MIN) || (RHS.getOpcode() == SHAVEISD::MAX))
          std::swap(LHS, RHS);

        SDValue MinMaxValue = DAG.getNode(MinMaxOpc, dbgLoc, CompareVT, LHS, RHS, SignedFlag);

        if (NeedsCast)
          return DAG.getNode(CastOperation, dbgLoc, VT, MinMaxValue);
        else
          return MinMaxValue;
      }
    }

    return SDValue();
  }
} // End of anonymous namespace


SDValue SHAVELowering::CombineSELECT_CC(SDNode *N, DAGCombinerInfo &DCI) const {
  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);
  SDValue TrueVal = N->getOperand(2);
  SDValue FalseVal = N->getOperand(3);
  SDValue Cond = N->getOperand(4);
  ISD::CondCode CC = cast<CondCodeSDNode>(Cond)->get();
  EVT CompareVT = LHS.getValueType();

  if (!isTypeLegal(CompareVT))
    return SDValue();

  return SelectMinOrMax(LHS, RHS, TrueVal, FalseVal, CC, SDLoc(N), DCI.DAG);
}

SDValue SHAVELowering::CombineMIN_MAX(SDNode *N, DAGCombinerInfo &DCI) const {
  SDLoc dbgLoc(N);
  EVT VT = N->getValueType(0);
  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);

  // Combine (MAX (MIN v high) low) -> CLAMP(v, low, high)
  // Combine (MIN (MAX v low) high) -> CLAMP(v, low, high)
  bool MaxMinPattern = (N->getOpcode() == SHAVEISD::MAX) && (LHS.getOpcode() == SHAVEISD::MIN);
  bool MinMaxPattern = (N->getOpcode() == SHAVEISD::MIN) && (LHS.getOpcode() == SHAVEISD::MAX);

  if (!MinMaxPattern && !MaxMinPattern)
    return SDValue();

  // Types of low and high must match
  EVT OtherVT = LHS.getValueType();
  if (VT != OtherVT)
    return SDValue();

  ConstantSDNode *IsUint = cast<ConstantSDNode>(N->getOperand(2));
  ConstantSDNode *OtherIsUint = cast<ConstantSDNode>(LHS.getOperand(2));
  SDValue ClampSrc = LHS.getOperand(0);

  // Get low and high values
  SDValue ClampLowValue, ClampHighValue;
  ConstantSDNode * HighIsUint, * LowIsUint;
  if (MaxMinPattern) {
    ClampLowValue = RHS;
    ClampHighValue = LHS.getOperand(1);
    LowIsUint = IsUint;
    HighIsUint = OtherIsUint;
  }
  else if (MinMaxPattern) {
    ClampLowValue = LHS.getOperand(1);
    ClampHighValue = RHS;
    LowIsUint = OtherIsUint;
    HighIsUint = IsUint;
  }

  SDValue ClampLow = ClampLowValue;
  SDValue ClampHigh = ClampHighValue;

  // Account for vector types with constant splat high/low values
  if (ClampLow.getOpcode() == SHAVEISD::SPLAT)
    ClampLowValue = ClampLow.getOperand(0);
  if (ClampHigh.getOpcode() == SHAVEISD::SPLAT)
    ClampHighValue = ClampHigh.getOperand(0);

  if (isa<ConstantSDNode>(ClampLowValue) && isa<ConstantSDNode>(ClampHighValue)) {
    // Integer constant values
    ConstantSDNode *LowValue = cast<ConstantSDNode>(ClampLowValue);
    ConstantSDNode *HighValue = cast<ConstantSDNode>(ClampHighValue);

    bool unsignedLow = LowIsUint->getZExtValue();
    bool unsignedHigh = HighIsUint->getZExtValue();

    int64_t low = unsignedLow ? (int64_t) LowValue->getZExtValue() : LowValue->getSExtValue();
    int64_t high = unsignedHigh ? (int64_t) HighValue->getZExtValue() : HighValue->getSExtValue();

    if (unsignedLow == unsignedHigh) {
      // If LowValue > HighValue, then (MAX (MIN v high) low) will always evaluate to the "low" value,
      //                               (MIN (MAX v low) high) will always evaluate to the "high" value.
      // If LowValue == HighValue, then both patterns will always evaluate to this value
      // Opt does this optimisation long before now, but we might catch some late optimisation
      // opportunities with this
      if (low >= high)
        return SDValue(MaxMinPattern ? LowValue : HighValue, 0);

      // Signs match and high > low equates to a CLAMP for both patterns
      return DCI.DAG.getNode(SHAVEISD::CLAMP, dbgLoc, VT, ClampSrc, ClampLow,
                             ClampHigh, SDValue(LowIsUint, 0));
    }
    else if (!unsignedLow && unsignedHigh && MinMaxPattern) {
      // For two positive constant values, LLVM can produce a signed MAX, with an unsigned MIN.
      // Since the constant operand to the MAX is a positive value, the output from the operation is
      // guaranteed to be unsigned, hence the unsigned MIN. We need to account for this specifically

      // For (unsigned_MIN (signed_MAX v low) high), when low >= high the signed_MAX operation is guaranteed
      // to always produce a value which is >= "high", so the result of the unsigned_MIN is always "high"
      if (low >= high)
        return SDValue(HighValue, 0);

      // (unsigned_MIN (signed_MAX v low) high)
      // In this case when high > low, we cannot produce a clamp if low is less than zero or high is greater than INT_MAX
      // For example, (unsigned_MIN (signed_MAX -1 -2) 128) produces 128, but CLAMP.i32 produces -1
      // And          (unsigned_MIN (signed_MAX 3 -2) 0xFFFFFFFF) produces 3, but CLAMP.i32 produces -1
      // But
      //              (unsigned_MIN (signed_MAX -1 2) 128) produces 2, and CLAMP.i32 also produces 2
      // And          (unsigned_MIN (signed_MAX 130 2) 128) produces 128, and CLAMP.i32 also produces 128
      if (low >= 0 && HighValue->getAPIntValue().isSignedIntN(32))
        return DCI.DAG.getNode(SHAVEISD::CLAMP, dbgLoc, VT, ClampSrc, ClampLow, ClampHigh,
                               DCI.DAG.getTargetConstant(0, dbgLoc, MVT::i32)); // Signed CLAMP
    }
  }
  else if (isa<ConstantFPSDNode>(ClampLowValue) && isa<ConstantFPSDNode>(ClampHighValue)) {
    // Floating-point constant values
    ConstantFPSDNode *LowValue = cast<ConstantFPSDNode>(ClampLowValue);
    ConstantFPSDNode *HighValue = cast<ConstantFPSDNode>(ClampHighValue);
    bool noNaNs = N->getFlags().hasNoNaNs() && LHS->getFlags().hasNoNaNs();

    // If LowValue > HighValue, then (MAX (MIN v high) low) will always evaluate to the "low" value,
    //                               (MIN (MAX v low) high) will always evaluate to the "high" value.
    // But only when we don't care about NaNs (e.g. -ffast-math)
    if (LowValue->getValueAPF() > HighValue->getValueAPF()) {
      if (noNaNs)
        return SDValue(MaxMinPattern ? LowValue : HighValue, 0);
      return SDValue();
    }

    // If LowValue == HighValue, then both patterns will always evaluate to this value
    if (LowValue->getValueAPF() == HighValue->getValueAPF() && noNaNs)
      return SDValue(LowValue, 0);

    return DCI.DAG.getNode(SHAVEISD::CLAMP, dbgLoc, VT, ClampSrc, ClampLow, ClampHigh, SDValue(IsUint, 0));
  }

  return SDValue();
}

SDValue SHAVELowering::CombineVSELECT(SDNode *N, DAGCombinerInfo &DCI) const {
  // Combine (vselect (v*i1 (setcc lhs rhs cond)) x y) to min/max.
  SDNode *SelectNode = N->getOperand(0).getNode();
  SDValue TrueVal = N->getOperand(1);
  SDValue FalseVal = N->getOperand(2);

  if (SelectNode->getOpcode() != ISD::SETCC)
    return SDValue();

  SDValue LHS = SelectNode->getOperand(0);
  SDValue RHS = SelectNode->getOperand(1);
  SDValue Cond = SelectNode->getOperand(2);
  ISD::CondCode CC = cast<CondCodeSDNode>(Cond)->get();
  EVT CompareVT = LHS.getValueType();

  if (!isTypeLegal(CompareVT))
    return SDValue();

  return SelectMinOrMax(LHS, RHS, TrueVal, FalseVal, CC, SDLoc(N), DCI.DAG);
}


namespace {
  unsigned GetConstantPowerOfTwo(SDValue V) {
    EVT VT = V.getValueType();
    ConstantSDNode *ConstVal = nullptr;

    if (VT.isVector()) {
      if (V.getOpcode() == ISD::BUILD_VECTOR) {
        SDValue SplatVal = GetConstantSplatValue(V.getNode());

        ConstVal = dyn_cast_or_null<ConstantSDNode>(SplatVal.getNode());
      } else if (V.getOpcode() == SHAVEISD::SPLAT) {
        SDValue SplatVal = V.getOperand(0);

        ConstVal = dyn_cast<ConstantSDNode>(SplatVal.getNode());
      }
    } else
      ConstVal = dyn_cast<ConstantSDNode>(V.getNode());

    // Determine whether the divisor is a positive power-of-two.
    if (ConstVal && (ConstVal->getSExtValue() > 0) && isPowerOf2_32(ConstVal->getZExtValue()))
      return ConstVal->getZExtValue();

    return 0;
  }
} // End of anonymous namespace


SDValue SHAVELowering::CombineSDIV(SDNode *N, DAGCombinerInfo &DCI) const {
  SDLoc dbgLoc(N);
  EVT VT = N->getValueType(0);
  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);

  // Determine whether the divisor is a positive power-of-two.
  unsigned DivisorVal = GetConstantPowerOfTwo(RHS);

  if (DivisorVal && isTypeLegal(VT)) {
    // Replace 'x / N' by 'x >> log N', with rounding towards zero.
    unsigned EleBitWidth = VT.isVector() ? VT.getVectorElementType().getSizeInBits()
                                         : VT.getSizeInBits();
    unsigned ShiftAmount = Log2_32(DivisorVal);
    unsigned RoundAmount = (1 << ShiftAmount) - 1;
    SDValue MaskVal = DCI.DAG.getConstant(EleBitWidth - 1, dbgLoc, VT);
    SDValue RoundedVal = DCI.DAG.getConstant(RoundAmount, dbgLoc, VT);

    MaskVal = DCI.DAG.getNode(ISD::SRA, dbgLoc, VT, LHS, MaskVal);
    RoundedVal = DCI.DAG.getNode(ISD::AND, dbgLoc, VT, MaskVal, RoundedVal);
    RoundedVal = DCI.DAG.getNode(ISD::ADD, dbgLoc, VT, LHS, RoundedVal);

    SDValue ShiftVal = DCI.DAG.getConstant(ShiftAmount, dbgLoc, VT);

    return DCI.DAG.getNode(ISD::SRA, dbgLoc, VT, RoundedVal, ShiftVal);
  }

  return SDValue();
}

SDValue SHAVELowering::CombineSREM(SDNode *N, DAGCombinerInfo &DCI) const {
  SDLoc dbgLoc(N);
  EVT VT = N->getValueType(0);
  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);

  // Determine whether the divisor is a positive power-of-two.
  unsigned DivisorVal = GetConstantPowerOfTwo(RHS);

  if (DivisorVal && isTypeLegal(VT)) {
    // Replace (srem x N) by (sub x (round x down to be logN-aligned))
    unsigned EleBitWidth = VT.isVector() ? VT.getVectorElementType().getSizeInBits()
                                         : VT.getSizeInBits();
    unsigned ShiftAmount = Log2_32(DivisorVal);
    unsigned RoundAmount = (1 << ShiftAmount) - 1;
    SDValue RoundAmountVal = DCI.DAG.getConstant(RoundAmount, dbgLoc, VT);
    SDValue TruncVal = DCI.DAG.getNode(ISD::XOR, dbgLoc, VT, LHS, RoundAmountVal);
    SDValue MaskVal = DCI.DAG.getConstant(EleBitWidth - 1, dbgLoc, VT);

    MaskVal = DCI.DAG.getNode(ISD::SRA, dbgLoc, VT, LHS, MaskVal);

    SDValue RoundedVal = DCI.DAG.getNode(ISD::AND, dbgLoc, VT, MaskVal, RoundAmountVal);

    RoundedVal = DCI.DAG.getNode(ISD::ADD, dbgLoc, VT, TruncVal, RoundedVal);

    return DCI.DAG.getNode(ISD::SUB, dbgLoc, VT, LHS, RoundedVal);
  }

  return SDValue();
}

SDValue SHAVELowering::CombineSINT_TO_FP(SDNode *N, DAGCombinerInfo &DCI) const {
  SDLoc dbgLoc(N);
  EVT VT = N->getValueType(0);
  SDValue Src = N->getOperand(0);

  // Replace (v8f32 (sitofp (v8i8 (load Ptr))))
  // by (v8f32 (sitofp (v8i16 (extload Ptr from v8i8)))).
  LoadSDNode *Load = dyn_cast<LoadSDNode>(Src.getNode());

  if (Load && (VT == MVT::v8f32)) {
    SDValue Chain = Load->getOperand(0);
    SDValue Base = Load->getOperand(1);
    SDValue Offset = Load->getOperand(2);

    if ((Offset.getOpcode() == ISD::UNDEF) && (Load->getMemoryVT() == MVT::v8i8)) {
      MachineMemOperand::Flags memLoadOpFlags = static_cast<MachineMemOperand::Flags>(
                                                    (Load->isVolatile() ? MachineMemOperand::MOVolatile : MachineMemOperand::MONone)
                                                    | (Load->isInvariant() ? MachineMemOperand::MOInvariant : MachineMemOperand::MONone)
                                                    | (Load->isNonTemporal() ? MachineMemOperand::MONonTemporal : MachineMemOperand::MONone)
                                                    | MachineMemOperand::MOLoad);

      SDValue NewLoad = DCI.DAG.getExtLoad(ISD::SEXTLOAD, dbgLoc, MVT::v8i16,
                                           Chain, Base, Load->getPointerInfo(),
                                           Load->getMemoryVT(),
                                           Load->getAlign(),
                                           memLoadOpFlags,
                                           Load->getAAInfo());

      return DCI.DAG.getNode(N->getOpcode(), dbgLoc, VT, NewLoad);
    }
  }

  return SDValue();
}


namespace {
  void CheckAccuADD(SDNode *N, SmallVector<std::pair<SDNode*, char>, 8> &WorkList) {
    SDValue Op0 = N->getOperand(0);
    SDValue Op1 = N->getOperand(1);
    char AddOpIdx = -1;

    unsigned Opcode0 = Op0.getNode()->getOpcode();
    unsigned Opcode1 = Op1.getNode()->getOpcode();

    // Skip if both subtrees have Operators

    if ((Opcode0 == ISD::ADD) || (Opcode0 == ISD::SUB))
       if ((Opcode1 == ISD::ADD) || (Opcode1 == ISD::SUB))
           return;

    // Skip if the current node N is a sub with a right skew
    // ACCN/MACN cannot capture this
    if (N->getOpcode() == ISD::SUB && (Opcode1 == ISD::ADD || Opcode1 == ISD::SUB))
      return;

    if ((Opcode0 == ISD::SUB || Opcode0 == ISD::ADD) && Op0.hasOneUse()) {
      CheckAccuADD(Op0.getNode(), WorkList);
      AddOpIdx = 0;
    } else if ((Opcode1 == ISD::SUB || Opcode1 == ISD::ADD) && Op1->hasOneUse()) {
      CheckAccuADD(Op1.getNode(), WorkList);
      AddOpIdx = 1;
    }

    WorkList.push_back(std::make_pair(N, AddOpIdx));

    return;
  }

  bool ACCConflictsWithDP4A(MachineFunction &MF, const EVT &NVT) {
    SHAVEMachineFunctionInfo *SMFI = MF.getInfo<SHAVEMachineFunctionInfo>();
    return (SMFI->hasSAUDP4A() && (!NVT.isVector() || NVT.getSizeInBits() <= 32)) ||
           (SMFI->hasVAUDP4A() && NVT.isVector());
  }
} // End of anonymous namespace


SDValue SHAVELowering::CombineADD(SDNode *N, DAGCombinerInfo &DCI) const {
  if (!EnableINTAccMac)
    return SDValue();

  EVT NVT = N->getValueType(0);

  if (NVT.isInteger() && !NVT.isVector()) {
    // This combine does not support over 32-bit scalar integer.
    if (NVT.getSizeInBits() > 32)
      return SDValue();
  }

  // No scalar ACC/MACs if SAU is missing
  if (!NVT.isVector() && !SHAVEST.hasSAU())
    return SDValue();

  // No vector ACC/MACs if VAU is missing
  if (NVT.isVector() && !SHAVEST.hasVAU())
    return SDValue();

  if (ACCConflictsWithDP4A(DCI.DAG.getMachineFunction(), NVT))
    return SDValue();

  SmallVector<std::pair<SDNode*, char>, 8> WorkList;
  // For integer ACCs, we need the minops to be 3, but we are disabling ACC
  // since it does not give any benefit for ints
  const unsigned MinOps = 2; // FIXME: should be set to 1 , for macs only, was 5
  const unsigned MaxOps = 16; // To be tuned heuristically

  // Check the candidates for accumulation.
  CheckAccuADD(N, WorkList);

  // Check the lower operation threshold.
  if (WorkList.size() < MinOps)
    return SDValue();

  // The transformation is as follows:
  // - WorkList[0] --> ACCPZ and ACCP
  // - WorkList[1] ~ WorkList[size-2] --> ACCP
  // - WorkList[size-1] --> ACCP and ACCPW
  SmallVector<SDValue, 16> Ops;
  bool isMac = true;

  // Check whether insructions of worklist are MAC or ACC.
  for (unsigned i = 0; i < WorkList.size(); i++) {
    SDValue CurOp0 = WorkList[i].first->getOperand(0);
    SDValue CurOp1 = WorkList[i].first->getOperand(1);

    // If there are not MUL operands on a instruction
    // ACC instructions will be generated.
    if ((CurOp0.getNode()->getOpcode() != ISD::MUL) && (CurOp1.getNode()->getOpcode() != ISD::MUL)) {
      isMac = false;
      //////// DISABLE SCALAR INTEGER ACCS - THEY JUST INCREASE REG PRESSURE /////
     if (!NVT.isVector())
        return SDValue();

     break;
    }
  }

  // Check the upper operation threshold.
  unsigned NumOps = WorkList.size();

  if (isMac)
    NumOps *= 2;

  if (NumOps > MaxOps && hasFeature(SHAVE::HasVRF512_Feature))
    return SDValue();

  // ----------------- Start creating the ACC/MAC sequence ----------------- //
  //
  // Build ACCPZ and ACCP nodes.
  SDNode *CandACCPZ = WorkList[0].first;
  SDLoc dbgLoc(CandACCPZ);
  EVT VT = CandACCPZ->getValueType(0);

  // allow only v4i32,v8i16 and v16i8 vectors
  if (VT.isVector()) {
     int8_t simpleTy = VT.getSimpleVT().SimpleTy;
     if (hasFeature(SHAVE::HasVRF128_Feature) && simpleTy != MVT::v4i32 &&
         simpleTy != MVT::v8i16 &&
         simpleTy != MVT::v16i8)
       return SDValue();
     else if (hasFeature(SHAVE::HasVRF512_Feature) && simpleTy != MVT::v16i32 &&
              simpleTy != MVT::v32i16 &&
              simpleTy != MVT::v64i8)
       return SDValue();
  }

  // Check mul operand.
  SDValue FirstLHS = CandACCPZ->getOperand(0);
  SDValue FirstRHS = CandACCPZ->getOperand(1);


  if (isMac) {
    bool LHSIsMUL = (FirstLHS.getOpcode() == ISD::MUL);
    bool RHSIsMUL = (FirstRHS.getOpcode() == ISD::MUL);

    // Handle the first operand.
    if (LHSIsMUL) {
      Ops.push_back(FirstLHS.getOperand(0));
      Ops.push_back(FirstLHS.getOperand(1));
    } else{
      // Lone addition before the MUL-ADD chain.
      // Convert it to a trivial MUL-ADD.
      Ops.push_back(DCI.DAG.getConstant(1, dbgLoc, VT));
      Ops.push_back(FirstLHS);
    }

    // Push the negation markers before the next operand if needed
    // MAC logic relies on each operand pair starting at even index, hence we
    // push it twice here
    if (WorkList[0].first->getOpcode() == ISD::SUB){
        Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));
        Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));
       }

    // Handle the second operand
    if (RHSIsMUL) {
      Ops.push_back(FirstRHS.getOperand(0));
      Ops.push_back(FirstRHS.getOperand(1));
    } else {
      // Lone addition before the MUL-ADD chain.
      // Convert it to a trivial MUL-ADD.
      Ops.push_back(DCI.DAG.getConstant(1, dbgLoc, VT));
      Ops.push_back(FirstRHS);
    }
  } else {
    // Build ACCPZ and ACCP nodes.
    // ACC : acc = acc + A
    // This is for ACCPZ. We should use this operands 2 times
    // when this node is expanded on PseudoExpansion.
    Ops.push_back(CandACCPZ->getOperand(0));

    if (WorkList[0].first->getOpcode() == ISD::SUB){
        Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));
       }

    // This is for ACCP.
    Ops.push_back(CandACCPZ->getOperand(1));
  }

  // Build ACCP and MACP nodes.
  for (unsigned ACCPIdx = 1; ACCPIdx < WorkList.size()-1; ACCPIdx++) {
    SDNode *CandACCP = WorkList[ACCPIdx].first;
    unsigned ACCOpIdx = WorkList[ACCPIdx].second;
    unsigned OtherOpIdx = ACCOpIdx ? 0 : 1;

    // Push the negation markers before the next operand if needed
    if (CandACCP->getOpcode() == ISD::SUB)  {
         Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));
         if (isMac) Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));

         // The code for handling the negated node relies on the fact that the
         // ACCnode cannot be the RHS of a SUB node. Adding check here
         assert (OtherOpIdx == 1 && "Broken assumption about operand ordering in MAC combiner");
    }

    if (isMac) {
      // Build MACP nodes.
      // These are for MACP.
      SDNode *Mul = CandACCP->getOperand(OtherOpIdx).getNode();

      Ops.push_back(Mul->getOperand(0));
      Ops.push_back(Mul->getOperand(1));
    }
    else {
      // Build ACCP nodes.
      Ops.push_back(CandACCP->getOperand(OtherOpIdx));
    }
  }

  unsigned MulIdx = 0;

  // FIXME: Saurabh: We don't need to specialize for writebacks
  // Extending the above loop till end of worklist should suffice
  // Build ACCPW and MACPW nodes.
  unsigned ACCPWIdx = WorkList.size() - 1;
  SDNode *CandACCPW = WorkList[ACCPWIdx].first;

  SDValue Op0 = CandACCPW->getOperand(0);
  SDValue Op1 = CandACCPW->getOperand(1);

  // Check FMUL operand.
  if (Op0.getNode()->getOpcode() == ISD::MUL) {
    MulIdx = 0;
  } else if (Op1.getNode()->getOpcode() == ISD::MUL) {
    MulIdx = 1;
  }

  // Build ACCPW and MACPW node next

  if (isMac) {
    // These are for MACPW.
    SDNode *Mul = CandACCPW->getOperand(MulIdx).getNode();

    // Push the negation markers before the next operand if needed
    // MAC logic relies on each operand pair starting at even index, hence we
    // push it twice here
    if (CandACCPW->getOpcode() == ISD::SUB)  {
         Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));
         Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));

         // The code for handling the negated node relies on the fact that the
         // ACCnode cannot be the RHS of a SUB node. Adding check here
         assert (MulIdx == 1 && "Broken assumption about operand ordering in MAC combiner");
    }

    Ops.push_back(Mul->getOperand(0));
    Ops.push_back(Mul->getOperand(1));

    // Generate MAC Pseudo node.
    SDValue MAC = DCI.DAG.getNode(SHAVEISD::MACP_SEQ, dbgLoc, VT, Ops);

    return MAC;
  } else {
    // Build ACCP and ACCPW nodes.
    // This is for ACCPW.

    // Push the negation markers before the next operand if needed
    if (CandACCPW->getOpcode() == ISD::SUB)  {
         Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));
    }

    unsigned ACCPWOpIdx = WorkList[ACCPWIdx].second;
    unsigned OtherOpIdx = ACCPWOpIdx ? 0 : 1;
    Ops.push_back(CandACCPW->getOperand(OtherOpIdx));

    // Generate ACC Pseudo node.
    SDValue ACC = DCI.DAG.getNode(SHAVEISD::ACCP_SEQ, dbgLoc, VT, Ops);

    return ACC;
  }
}

namespace {
  void CheckAccuFADD(SDNode *N, SmallVector<std::pair<SDNode*, char>, 8> &WorkList) {
    SDValue Op0 = N->getOperand(0);
    SDValue Op1 = N->getOperand(1);
    char FaddOpIdx = -1;

    unsigned Opcode0 = Op0.getNode()->getOpcode();
    unsigned Opcode1 = Op1.getNode()->getOpcode();

    // Skip if both subtrees have Operators
    if ((Opcode0 == ISD::FADD) || (Opcode0 == ISD::FSUB))
       if ((Opcode1 == ISD::FADD) || (Opcode1 == ISD::FSUB))
           return;

    // Skip if the current node N is a sub with a right skew
    // ACCN/MACN cannot capture this
    if (N->getOpcode() == ISD::FSUB && (Opcode1 == ISD::FADD || Opcode1 == ISD::FSUB))
      return;

    if ((Opcode0 == ISD::FSUB || Opcode0 == ISD::FADD) && Op0.hasOneUse()) {
      CheckAccuFADD(Op0.getNode(), WorkList);
      FaddOpIdx = 0;
    } else if ((Opcode1 == ISD::FSUB || Opcode1 == ISD::FADD) && Op1->hasOneUse()) {
      CheckAccuFADD(Op1.getNode(), WorkList);
      FaddOpIdx = 1;
    }
    WorkList.push_back(std::make_pair(N, FaddOpIdx));

    return;
  }

  bool isKnownZero(SDValue V) {
    if (V.getValueType().isVector() && (V.getOpcode() == SHAVEISD::SPLAT))
      return isKnownZero(V.getOperand(0));
    else if (V.getValueType().isVector() && (V.getOpcode() == ISD::BUILD_VECTOR)) {
      bool result = true;
      for (int i = V.getNumOperands() - 1; i >= 0; i--) {
    SDValue operandI= V.getOperand(i);
    result &= (isKnownZero(operandI) || operandI.isUndef());
      }
      return result;
    }
    else if (ConstantSDNode *CSD = dyn_cast<ConstantSDNode>(V.getNode()))
      return CSD->isZero();
    else if (ConstantFPSDNode *FPCSD = dyn_cast<ConstantFPSDNode>(V.getNode()))
      return FPCSD->isZero();
    else
      return false;
  }
} // End of anonymous namespace

/// Whether or not this node is a part of a larger horizontal vector chain.
bool SHAVELowering::isPartOfHorizontalChain(SDNode *N) const {
  SDNode * inputChain = nullptr;
  SDValue vectorSource = SHAVEMatchHorizontalVectorOp(SDValue(N, 0), inputChain);
  if (vectorSource != SDValue(N, 0))
    return true;
  for (SDNode* use: N->uses()) {
    if (use->getOpcode() != N->getOpcode())
      continue;
    if (isPartOfHorizontalChain(use))
      return true;
  }
  return false;
}

SDValue SHAVELowering::CombineFADD(SDNode *N, DAGCombinerInfo &DCI) const {
  unsigned MinOps;
  unsigned MaxOps;

  SmallVector<std::pair<SDNode*, char>, 8> WorkList;
  // FIXME: Movidius - Need to use this temp reg instead of I0
  // MachineRegisterInfo &MRI = DCI.DAG.getMachineFunction().getRegInfo();
  // unsigned TempReg = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);

  if (!EnableFPAccMac)
    return SDValue();

  EVT NVT = N->getValueType(0);

  if (ACCConflictsWithDP4A(DCI.DAG.getMachineFunction(), NVT))
    return SDValue();

  // No scalar fp ACC/MACs if SAU is missing
  if (!NVT.isVector() && !SHAVEST.hasSAU())
    return SDValue();

  // No vector ACC/MACs if VAU is missing
  if (NVT.isVector() && !SHAVEST.hasVAU())
    return SDValue();

  // Check the candidates for accumulation.
  CheckAccuFADD(N, WorkList);

  // The transformation is as follows:
  // - WorkList[0] --> ACCPZ and ACCP
  // - WorkList[1] ~ WorkList[size-2] --> ACCP
  // - WorkList[size-1] --> ACCP and ACCPW
  SmallVector<SDValue, 16> Ops;
  bool isMac = true;

  // Check whether insructions of worklist are MAC or ACC.
  for (unsigned i = 0; i < WorkList.size(); i++) {
    SDValue CurOp0 = WorkList[i].first->getOperand(0);
    SDValue CurOp1 = WorkList[i].first->getOperand(1);

    // If there are not FMUL operands on a instruction
    // ACC instructions will be generated.
    if ((CurOp0.getOpcode() != ISD::FMUL) && (CurOp1.getOpcode() != ISD::FMUL)) {
      isMac = false;
      break;
    }
  }

  // Min,Max ops values based on instruction latencies
  if (isMac) {
                MinOps = 4;
                MaxOps = 7;
  } else {
                MinOps = 4;
                MaxOps = 9;
  }

  unsigned NumOps = WorkList.size();

  // Enforce the minimum operand threshold.
  if (WorkList.size() < MinOps)
    return SDValue();

  // Enforce the upper operation threshold.
  if (NumOps > MaxOps)
    return SDValue();

  // ----------------- Start creating the ACC/MAC sequence ----------------- //
  //
  // Get operands from the first 'add' instruction.
  SDNode *FirstAdd = WorkList[0].first;
  SDLoc dbgLoc(FirstAdd);
  EVT VT = FirstAdd->getValueType(0);

  SDVTList VTList = DCI.DAG.getVTList(VT, VT);

  // allow only v4f32 and v8f16 vectors
  if (VT.isVector()) {
     int8_t simpleTy = VT.getSimpleVT().SimpleTy;
     if (hasFeature(SHAVE::HasVRF128_Feature) && simpleTy != MVT::v4f32 && simpleTy != MVT::v8f16)
       return SDValue();
     else if (hasFeature(SHAVE::HasVRF512_Feature) && simpleTy != MVT::v16f32 && simpleTy != MVT::v32f16)
       return SDValue();
  }
  else {
    // For scalar accumulation chains which operate on all elements of a vector value,
    // don't use the accumulator so we can lower to a horizontal vector add later
    if (isPartOfHorizontalChain(N))
      return SDValue();
  }

  SDValue FirstLHS = FirstAdd->getOperand(0);
  SDValue FirstRHS = FirstAdd->getOperand(1);

  // push the tmpreg for fp ACC consolidation
  //SDValue tmpRegSDValue = DCI.DAG.getRegister(TempReg, VT);
 // Ops.push_back(tmpRegSDValue);
  //tmpRegSDValue.setIsKill(true);

  if (isMac) {
    bool LHSIsMUL = (FirstLHS.getOpcode() == ISD::FMUL);
    bool RHSIsMUL = (FirstRHS.getOpcode() == ISD::FMUL);

    // Handle the first operand.
    if (LHSIsMUL) {
      Ops.push_back(FirstLHS.getOperand(0));
      Ops.push_back(FirstLHS.getOperand(1));
    } else if (!isKnownZero(FirstLHS)) {
      // Lone addition before the MUL-ADD chain.
      // Convert it to a trivial MUL-ADD.
      Ops.push_back(DCI.DAG.getConstantFP(1.0, dbgLoc, VT));
      Ops.push_back(FirstLHS);
    }

    if (WorkList[0].first->getOpcode() == ISD::FSUB && !isKnownZero(FirstRHS)){
      Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));
      Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));
     }

    // Handle the second operand.
    if (RHSIsMUL) {
      Ops.push_back(FirstRHS.getOperand(0));
      Ops.push_back(FirstRHS.getOperand(1));
    } else if (!isKnownZero(FirstRHS)) {
      // Lone addition before the MUL-ADD chain.
      // Convert it to a trivial MUL-ADD.
      Ops.push_back(DCI.DAG.getConstantFP(1.0, dbgLoc, VT));
      Ops.push_back(FirstRHS);
    }
  } else {
    if (!isKnownZero(FirstLHS))
      Ops.push_back(FirstLHS);

    if (!isKnownZero(FirstRHS)) {
      if (WorkList[0].first->getOpcode() == ISD::FSUB)
        Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));

      Ops.push_back(FirstRHS);
    }
  }

  // Gather the remaining ACC/MAC operands.
  for (unsigned InstIdx = 1; InstIdx < WorkList.size(); InstIdx++) {
    SDNode *Add = WorkList[InstIdx].first;
    unsigned ACCOpIdx = WorkList[InstIdx].second;
    unsigned OtherOpIdx = ACCOpIdx ? 0 : 1;

    if (Add->getOpcode() == ISD::FSUB)  {
         Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));
         if (isMac) Ops.push_back(DCI.DAG.getTargetConstant(SHAVE_ACCN_MACN_MARKER, dbgLoc, MVT::i32));

         // The code for handling the negated node relies on the fact that the
         // ACCnode cannot be the RHS of a SUB node. Adding check here
         assert (OtherOpIdx == 1 && "Broken assumption about operand ordering in MAC combiner");
    }

    if (isMac) {
      SDValue Mul = Add->getOperand(OtherOpIdx);
      Ops.push_back(Mul.getOperand(0));
      Ops.push_back(Mul.getOperand(1));
    } else {
      Ops.push_back(Add->getOperand(OtherOpIdx));
    }

  }

  // Build MAC/ACCP pseudo nodes.
  unsigned PseudoOpc = isMac ? SHAVEISD::MACP_SEQ : SHAVEISD::ACCP_SEQ;

  return DCI.DAG.getNode(PseudoOpc, dbgLoc, VTList, Ops);
}

MachineBasicBlock *SHAVELowering::EmitCustomCall(MachineInstr *MI, MachineBasicBlock *BB) const {
  DebugLoc dbgLoc = MI->getDebugLoc();
  const SHAVEInstrInfo *TII = SHAVEST.getInstrInfo();

  // Get the Link Reg (I30, but it may change)
  unsigned linkReg = SHAVERegisterInfo::getLinkReg();

  // insert the call sequence after the SHAVE_CALL instruction
  MachineBasicBlock::iterator it = MI;
  ++it;

  MachineOperand callTarget = MI->getOperand(0);

  if (callTarget.isReg())
    TII->finaliseMI(BuildMI(*BB, it, dbgLoc, TII->get(SHAVE::CMU_CPII_32_Raw), linkReg)
        .add(callTarget));
  else if(callTarget.isSymbol() || callTarget.isGlobal()) {
    BuildMI(*BB, it, dbgLoc, TII->get(SHAVE::LSU_LDILSym), linkReg)
        .add(callTarget)
        .addImm(SHAVECC::AL).addReg(0)
        .addImm(SHAVE::LSU1);
    TII->finaliseMI(BuildMI(*BB, it, dbgLoc, TII->get(SHAVE::LSU_LDIHSym), linkReg)
       .addReg(linkReg).add(callTarget));
  } else
    llvm_unreachable("Unknown call target operand type!");

  MachineInstrBuilder MIB = BuildMI(*BB, it, dbgLoc, TII->get(SHAVE::BRU_SWP), linkReg);

  MIB.addReg(linkReg);
  TII->finaliseMI(MIB);

  // Find and copy the register mask.
  for (unsigned i = 0; i < MI->getNumOperands(); i++) {
    MachineOperand &MO = MI->getOperand(i);

    if (MO.isRegMask()) {
      MIB.add(MO);
      break;
    }
  }

  // Copy implicitely defined registers (i.e. return values).
  for (unsigned i = 0; i < MI->getNumOperands(); i++) {
    MachineOperand &MO = MI->getOperand(i);

    if (!MO.isReg() || !MO.isDef() || !MO.isImplicit())
      continue;

    MIB.add(MO);
  }

  // Add the regs that are live-out at the moment of the call to the BRU.SWP instruction;
  // this is needed in order to disable Dead Code Elimination of this regs
  for(unsigned regIdx = 1; regIdx < MI->getNumOperands(); regIdx++)
    if (MI->getOperand(regIdx).isReg() && MI->getOperand(regIdx).isUse())
      MIB.addReg(MI->getOperand(regIdx).getReg(), RegState::Implicit);

  // Now remove the SHAVE_CALL pseudo-instruction
  MI->eraseFromParent();

  return BB;
}


MachineBasicBlock *SHAVELowering::EmitCustomReturn(MachineInstr *MI, MachineBasicBlock *BB) const {
  const SHAVEInstrInfo *TII = SHAVEST.getInstrInfo();
  MachineInstrBuilder MIB;
  MachineBasicBlock::iterator it = MI;
  DebugLoc dbgLoc = MI->getDebugLoc();

  // Insert the return sequence before the SHAVE_RET instruction.  The attribute
  // 'dllexport' is used to identify Entry-Points callable from the Leon, and
  // these must return using the 'BRU.SWIH' instruction rather than the regular
  // 'BRU.JMP I30' return sequence
  if (BB->getParent()->getFunction().hasDLLExportStorageClass())
    MIB = BuildMI(*BB, it, dbgLoc, TII->get(SHAVE::BRU_SWIH_Ret));
  else {
    MIB = BuildMI(*BB, it, dbgLoc, TII->get(SHAVE::BRU_JMP_Ret));
    MIB.addReg(SHAVERegisterInfo::getLinkReg());
  }

  TII->finaliseMI(MIB);

  // Copy return registers to the return instruction. This ensures that they are
  // alive since 'live-outs' are not supported.
  for (unsigned i = 0; i < MI->getNumOperands(); i++) {
    MachineOperand &MO = MI->getOperand(i);
    assert(MO.isReg() && "All SHAVE_RET operands should be registers.");

    MIB.addReg(MO.getReg(), RegState::Implicit);
  }

  // Erase the SHAVE_RET pseudo instruction
  MI->eraseFromParent();

  return BB;
}

MachineBasicBlock *SHAVELowering::EmitCustomEXTRACT_SUBVECTOR(MachineInstr *MI, MachineBasicBlock *MBB) const {
  // Expand the pseudo instruction
  //   destination = SHAVE::EXTRACT_SUBVECTOR source, index

  DebugLoc dbgLoc = MI->getDebugLoc();
  MachineFunction *MF = MI->getParent()->getParent();
  const SHAVEInstrInfo *TII = SHAVEST.getInstrInfo();
  MachineRegisterInfo &MRI = MF->getRegInfo();

  unsigned opcode = MI->getOpcode();
  MachineOperand &destination = MI->getOperand(0);
  MachineOperand &source      = MI->getOperand(1);
  MachineOperand &index       = MI->getOperand(2);

  switch (opcode) {
  case SHAVE::EXTRACT_SUBVECTOR_v2i32_v4i32:
  case SHAVE::EXTRACT_SUBVECTOR_v4i16_v8i16:
  case SHAVE::EXTRACT_SUBVECTOR_v2i16_v8i16:
  case SHAVE::EXTRACT_SUBVECTOR_v2i16_v4i16:
  case SHAVE::EXTRACT_SUBVECTOR_v8i8_v16i8:
  case SHAVE::EXTRACT_SUBVECTOR_v4i8_v16i8:
  case SHAVE::EXTRACT_SUBVECTOR_v4i8_v8i8:
  case SHAVE::EXTRACT_SUBVECTOR_v2f32_v4f32:
  case SHAVE::EXTRACT_SUBVECTOR_v4f16_v8f16:
  case SHAVE::EXTRACT_SUBVECTOR_v2f16_v8f16:
  case SHAVE::EXTRACT_SUBVECTOR_v2f16_v4f16: {
    const BasicBlock *BB = MBB->getBasicBlock();
    MachineFunction::iterator it = ++MBB->getIterator();

    // Create two new basic blocks and insert them into the function after BB
    // We are going to create the following structure to perform the subvector extract:
    //
    //        +-----------+
    //        |    BB     |
    //        +-----+-----+
    //              |  +------+
    //              |  |      |
    //              V  V      |
    //        +-----------+   |
    //        |  rpiMBB   |   |
    //        +--+-----+--+   |
    //           |     |      |
    //           |     +------+
    //           V
    //        +------------+
    //        | trailerMBB |
    //        +------------+
    //
    MachineBasicBlock *rpiMBB = MF->CreateMachineBasicBlock(BB);
    MachineBasicBlock *trailerMBB = MF->CreateMachineBasicBlock(BB);
    MF->insert(it, rpiMBB);
    MF->insert(it, trailerMBB);

    // Remove all instructions after the subvector extract from BB and move them into the trailer basic block
    trailerMBB->splice(trailerMBB->begin(), MBB, std::next(MachineBasicBlock::iterator(MI)), MBB->end());

    // Setup the successors for each block
    trailerMBB->transferSuccessorsAndUpdatePHIs(MBB);
    MBB->addSuccessor(rpiMBB);
    rpiMBB->addSuccessor(trailerMBB);
    rpiMBB->addSuccessor(rpiMBB);

    // These instructions are added to rpiMBB in reverse order. The following is the produced instruction sequence:
    //   CMU.CPVV destination source
    //   CMU.CMZ index
    //   PEU.PC1C NEQ
    //     || BRU.RPI index
    //     || CMU.ALIGNVEC destination destination destination alignAmount
    //
    // where alignAmount is either 4, 2, or 1 depending on the source vector type
    //
    // When the destination is an IRF register there is a slightly different instruction sequence produced:
    //   CMU.CPVV alignvecDstReg source
    //   CMU.CMZ index
    //   PEU.PC1C NEQ
    //     || BRU.RPI index
    //     || CMU.ALIGNVEC alignvecDstReg alignvecDstReg alignvecDstReg alignAmount
    //   CMU.CPVI.x32 destination alignvecDstReg
    //

    unsigned alignvecDstReg;

    if (opcode == SHAVE::EXTRACT_SUBVECTOR_v2i16_v8i16 || opcode == SHAVE::EXTRACT_SUBVECTOR_v2i16_v4i16
       || opcode == SHAVE::EXTRACT_SUBVECTOR_v4i8_v16i8 || opcode == SHAVE::EXTRACT_SUBVECTOR_v4i8_v8i8) {
      alignvecDstReg = MRI.createVirtualRegister(&SHAVE::VRF64_lRegClass);
      TII->finaliseMI(BuildMI(*rpiMBB, rpiMBB->begin(), dbgLoc, TII->get(SHAVE::CMU_CPVI_x32), destination.getReg()).addReg(alignvecDstReg).addImm(0));
    }
    else if (opcode == SHAVE::EXTRACT_SUBVECTOR_v2f16_v8f16 || opcode == SHAVE::EXTRACT_SUBVECTOR_v2f16_v4f16) {
      alignvecDstReg = MRI.createVirtualRegister(&SHAVE::VRF64_lRegClass);
      TII->finaliseMI(BuildMI(*rpiMBB, rpiMBB->begin(), dbgLoc, TII->get(SHAVE::CMU_CPVI_x32), destination.getReg()).addReg(alignvecDstReg).addImm(0));
    }
    else {
      alignvecDstReg = destination.getReg();
    }

    BuildMI(*rpiMBB, rpiMBB->begin(), dbgLoc, TII->get(SHAVE::BRU_RPI)).addMBB(rpiMBB)
           .add(index).addImm(SHAVECC::NEQ).addReg(SHAVE::CC_CMU0);

    unsigned srcReg = MRI.createVirtualRegister(&SHAVE::VRF64_lRegClass);
    unsigned alignAmount;

    switch (opcode) {
    case SHAVE::EXTRACT_SUBVECTOR_v2i32_v4i32: alignAmount = 4; break;
    case SHAVE::EXTRACT_SUBVECTOR_v2i16_v8i16:
    case SHAVE::EXTRACT_SUBVECTOR_v2i16_v4i16:
    case SHAVE::EXTRACT_SUBVECTOR_v4i16_v8i16: alignAmount = 2; break;
    case SHAVE::EXTRACT_SUBVECTOR_v8i8_v16i8:
    case SHAVE::EXTRACT_SUBVECTOR_v4i8_v16i8:
    case SHAVE::EXTRACT_SUBVECTOR_v4i8_v8i8:   alignAmount = 1; break;
    default: llvm_unreachable("Unknown alignAmount for instruction!"); break;
    }

    MachineInstrBuilder alignvec = BuildMI(*rpiMBB, rpiMBB->begin(), dbgLoc, TII->get(SHAVE::CMU_ALIGNVEC_imm_vrf64_SUBVECTOR_EXTRACT), alignvecDstReg)
                                          .addReg(srcReg).addReg(srcReg).addImm(alignAmount)
                                          .addImm(SHAVECC::NEQ).addReg(SHAVE::CC_CMU0);

    alignvec.addImm(SHAVE::CMU);
    alignvec.getInstr()->bundleWithSucc();

    TII->finaliseMI(BuildMI(*rpiMBB, rpiMBB->begin(), dbgLoc, TII->get(SHAVE::CMU_CMZ_i32)).add(index));
    TII->finaliseMI(BuildMI(*rpiMBB, rpiMBB->begin(), dbgLoc, TII->get(SHAVE::CMU_CPVV_128_64), srcReg).add(source));

    MI->eraseFromParent();

    return trailerMBB;
  }
  case SHAVE::EXTRACT_SUBVECTOR_v2i8_v4i8: {
    unsigned shiftAmount = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);

    // IAU.SHL shiftAmount index 3 ; shiftAmount = index * 8
    TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::IAU_SHL_32_imm), shiftAmount).add(index).addImm(3));

    // IAU.SHR.u32 destination source shiftAmount
    TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::IAU_SHR_u16_u32), destination.getReg()).add(source).addReg(shiftAmount));

    MI->eraseFromParent();

    return MBB;
  }
  case SHAVE::EXTRACT_SUBVECTOR_v2i8_v16i8:
  case SHAVE::EXTRACT_SUBVECTOR_v2i8_v8i8: {
    unsigned align = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);
    unsigned vtemp = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);
    unsigned vtemp2 = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);
    unsigned itemp = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);
    unsigned itemp2 = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);
    unsigned undef = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);

    // IAU.FEXTU align index 0 1
    TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::IAU_FEXTU_imm), align).add(index).addImm(0).addImm(1));

    // CMU.CPVV vtemp source
    TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_CPVV), vtemp).add(source));

    // CMU.CPII itemp index
    TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_CPII_32_Raw), itemp).add(index));

    // CMU.CMZ align
    TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_CMZ_i32)).addReg(align));

    // PEU.PC1C NEQ
    //   || CMU.ALIGNVEC vtemp temp temp 1
    //   || IAU.INCS itemp -1
    MachineInstrBuilder alignvec = BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_ALIGNVEC_imm_vrf), vtemp2).addReg(vtemp).addReg(vtemp).addImm(1)
                                          .addImm(SHAVECC::NEQ).addReg(SHAVE::CC_CMU0);

    alignvec.addImm(SHAVE::CMU);

    BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::IAU_INCS_i32), itemp2).addReg(itemp).addImm(-1)
           .addImm(SHAVECC::NEQ).addReg(SHAVE::CC_CMU0);

    // CMU.LUT.x16 destination itemp 0x0
    BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_LUT_i16_v8i16_pseudo), destination.getReg()).addReg(undef, RegState::Define).addReg(vtemp2).addReg(itemp2).addReg(undef, RegState::Undef);

    MI->eraseFromParent();

    return MBB;
  }
  default:
    llvm_unreachable("Unrecognised EXTRACT_SUBVECTOR pseudo opcode during custom emission");
  }

  return MBB;
}

MachineBasicBlock *SHAVELowering::EmitCustomINSERT_SUBVECTOR(MachineInstr *MI, MachineBasicBlock *MBB) const {
  // Expand the pseudo instruction
  //   destination = SHAVE::INSERT_SUBVECTOR vector1, vector2, index

  DebugLoc dbgLoc = MI->getDebugLoc();
  MachineFunction *MF = MI->getParent()->getParent();
  const SHAVEInstrInfo *TII = SHAVEST.getInstrInfo();
  MachineRegisterInfo &MRI = MF->getRegInfo();

  unsigned opcode = MI->getOpcode();
  MachineOperand &destination = MI->getOperand(0);
  MachineOperand &vector1 = MI->getOperand(1);
  MachineOperand &vector2 = MI->getOperand(2);
  int64_t index = MI->getOperand(3).getImm();

  switch (opcode) {
  case SHAVE::INSERT_SUBVECTOR_imm_v2i32_v4i32:
  case SHAVE::INSERT_SUBVECTOR_imm_v2f32_v4f32: {
    // CMU.VSZM.WORD destination vector2 [MASK]
    //   destination is the same register as vector1
    MachineInstrBuilder buildVSZM = BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_VSZM_WORD_SUBVECTOR_INSERT), destination.getReg()).add(vector1).add(vector2);

    // Add the MASK
    switch (index) {
    case 0: buildVSZM = buildVSZM.addImm(SHAVEVSZM::VSZM_LANE0).addImm(SHAVEVSZM::VSZM_LANE0+1).addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_DISABLE); break; // [DD10]
    case 1: buildVSZM = buildVSZM.addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_LANE0).addImm(SHAVEVSZM::VSZM_LANE0+1).addImm(SHAVEVSZM::VSZM_DISABLE); break; // [D10D]
    case 2: buildVSZM = buildVSZM.addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_LANE0).addImm(SHAVEVSZM::VSZM_LANE0+1); break; // [10DD]
    default: llvm_unreachable("INSERT_SUBVECTOR constant index is out of range");
    }

    TII->finaliseMI(buildVSZM);

    break;
  }
  case SHAVE::INSERT_SUBVECTOR_imm_v4i16_v8i16:
  case SHAVE::INSERT_SUBVECTOR_imm_v4f16_v8f16: {
    // We can't emit a CMU.ALIGNVEC with a constant 0, so use CMU.CPVV for this case
    if (index != 0) {
      // || CMU.ALIGNVEC destination vector1 vector2 (16 - (2 * index))
      MachineInstrBuilder alignvec = BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_ALIGNVEC_imm_vrf64_vrf_insert), destination.getReg())
                                            .add(vector1).add(vector2).addImm(16 - (2 * index)).addImm(SHAVECC::AL).addReg(0);
      alignvec.addImm(SHAVE::CMU);
    }
    else {
      // || CMU.CPVV destination vector2
      //   destination is the same register as vector1
      BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_CPVV_64_128_insert), destination.getReg())
             .add(vector1).add(vector2).addImm(SHAVECC::AL).addReg(0).addImm(SHAVE::CMU);
    }

    // This should really be just a CP.64 Vd.dcol Vs.scol
    // We don't have a testcase for this scenario. Bail out with an error for now since PVEN8C is not available on myriad4.0
    if (hasFeature(SHAVE::HasVRF512_Feature))
      llvm_unreachable("Unhandled subvector insert for SHAVE512");

    // PEU.PVEN8C (0xF << index)
    MachineInstrBuilder pven8c = BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::PEU_PVEN8C)).addImm(0xF << index);
    pven8c.getInstr()->bundleWithPred();

    break;
  }
  case SHAVE::INSERT_SUBVECTOR_imm_v8i8_v16i8: {
    if (index % 4 == 0) {
      // CMU.VSZM.WORD destination vector2 [MASK]
      //   destination is the same register as vector1
      MachineInstrBuilder buildVSZM = BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_VSZM_WORD_SUBVECTOR_INSERT), destination.getReg()).add(vector1).add(vector2);

      // Add the MASK
      switch (index) {
      case 0: buildVSZM = buildVSZM.addImm(SHAVEVSZM::VSZM_LANE0).addImm(SHAVEVSZM::VSZM_LANE0+1).addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_DISABLE); break; // [DD10]
      case 4: buildVSZM = buildVSZM.addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_LANE0).addImm(SHAVEVSZM::VSZM_LANE0+1).addImm(SHAVEVSZM::VSZM_DISABLE); break; // [D10D]
      case 8: buildVSZM = buildVSZM.addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_LANE0).addImm(SHAVEVSZM::VSZM_LANE0+1); break; // [10DD]
      default: llvm_unreachable("INSERT_SUBVECTOR constant index is out of range");
      }

      TII->finaliseMI(buildVSZM);
    }
    else if (index % 2 == 0) {
      // We can't emit a CMU.ALIGNVEC with a constant 0, so use CMU.CPVV for this case
      if (index != 0) {
        // || CMU.ALIGNVEC destination vector1 vector2 (16 - index)
        MachineInstrBuilder alignvec = BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_ALIGNVEC_imm_vrf64_vrf_insert), destination.getReg())
                                              .add(vector1).add(vector2).addImm(16 - index).addImm(SHAVECC::AL).addReg(0);

        alignvec.addImm(SHAVE::CMU);
      }
      else {
        // || CMU.CPVV destination vector2
        //   destination is the same register as vector1
        BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_CPVV_64_128_insert), destination.getReg())
               .add(vector1).add(vector2).addImm(SHAVECC::AL).addReg(0).addImm(SHAVE::CMU);
      }

      // This should really be just a CP.64 Vd.dcol Vs.scol
      // We don't have a testcase for this scenario. Bail out with an error for now since PVEN8C is not available on myriad4.0
      if (hasFeature(SHAVE::HasVRF512_Feature))
        llvm_unreachable("Unhandled subvector insert for SHAVE512");

      // PEU.PVEN8C (0xF << (index/2))

    MachineInstrBuilder pven8c = BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::PEU_PVEN8C)).addImm(0xF << (index / 2));
    pven8c.getInstr()->bundleWithPred();
    }
    else {
      unsigned vector1MaskIRF = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);
      unsigned vector2MaskIRF = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);
      unsigned vector2Aligned = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);
      unsigned vector1MaskVRF = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);
      unsigned vector2MaskVRF = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);
      unsigned vector1AndMask = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);
      unsigned vector2AndMask = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);

      // LSU0.LDIL vector1_mask_irf ((0xFF << index) ^ 0xFFFF)
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::LDImm32), vector1MaskIRF).addImm((0xFF << index) ^ 0xFFFF));

      // LSU1.LDIL vector2_mask_irf (0xFF << index)
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::LDImm32), vector2MaskIRF).addImm(0xFF << index));

      // CMU.ALIGNVEC vector2 vector2 vector2 (16 - index)
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_ALIGNVEC_imm_vrf64_vrf), vector2Aligned).add(vector2).add(vector2).addImm(16 - index));

      // CMU.CMASK vector1_mask_vrf vector1_mask_irf 0x0
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_CMASK_v16i8), vector1MaskVRF).addReg(vector1MaskIRF));

      // CMU.CMASK vector2_mask_vrf vector2_mask_irf 0x0
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_CMASK_v16i8), vector2MaskVRF).addReg(vector2MaskIRF));

      // VAU.AND vector1_and_mask vector1 vector1_mask_vrf
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::VAU_AND_16), vector1AndMask).add(vector1).addReg(vector1MaskVRF));

      // VAU.AND vector2_and_mask vector2 vector2_mask_vrf
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::VAU_AND_16), vector2AndMask).addReg(vector2Aligned).addReg(vector2MaskVRF));

      // VAU.OR destination vector1_and_mask vector2_and_mask
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::VAU_OR_16), destination.getReg()).addReg(vector1AndMask).addReg(vector2AndMask));
    }
    break;
  }
  case SHAVE::INSERT_SUBVECTOR_imm_v2i8_v4i8: {
    // CMU.VSZM.BYTE destination vector2 [MASK]
    //   destination is the same register as vector1
    MachineInstrBuilder buildVSZM = BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_VSZM_BYTE_SUBVECTOR_INSERT), destination.getReg()).add(vector1).add(vector2);

    // Add the MASK
    switch (index) {
    case 0: buildVSZM = buildVSZM.addImm(SHAVEVSZM::VSZM_LANE0).addImm(SHAVEVSZM::VSZM_LANE0+1).addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_DISABLE); break; // [DD10]
    case 1: buildVSZM = buildVSZM.addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_LANE0).addImm(SHAVEVSZM::VSZM_LANE0+1).addImm(SHAVEVSZM::VSZM_DISABLE); break; // [D10D]
    case 2: buildVSZM = buildVSZM.addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_LANE0).addImm(SHAVEVSZM::VSZM_LANE0+1); break; // [10DD]
    default: llvm_unreachable("INSERT_SUBVECTOR constant index is out of range");
    }

    TII->finaliseMI(buildVSZM);
    break;
  }
  case SHAVE::INSERT_SUBVECTOR_imm_v2i16_v4i16:
  case SHAVE::INSERT_SUBVECTOR_imm_v2i16_v8i16:
  case SHAVE::INSERT_SUBVECTOR_imm_v2f16_v4f16:
  case SHAVE::INSERT_SUBVECTOR_imm_v2f16_v8f16: {
    if (index % 2 == 0) {
      // CMU.CPIV destination.(index/2) vector2
      //   destination is the same register as vector1
      unsigned cpivOpcode;
      if (opcode == SHAVE::INSERT_SUBVECTOR_imm_v2i16_v4i16)      cpivOpcode = SHAVE::CMU_CPIV_x32_v2i32;
      else if (opcode == SHAVE::INSERT_SUBVECTOR_imm_v2i16_v8i16) cpivOpcode = SHAVE::CMU_CPIV_x32_v4i32;
      else if (opcode == SHAVE::INSERT_SUBVECTOR_imm_v2f16_v4f16) cpivOpcode = SHAVE::CMU_CPIV_x32_v2i32;
      else /* SHAVE::INSERT_SUBVECTOR_imm_v2f16_v8f16 */          cpivOpcode = SHAVE::CMU_CPIV_x32_v4i32;

      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(cpivOpcode), destination.getReg()).add(vector1).add(vector2).addImm(index/2));
    }
    else {
      unsigned cpivOpcode_l, cpivOpcode_h;
      unsigned tempDestReg;

       // Myriad2v3
    if (SHAVE::VRF64_lRegClass.contains(destination.getReg())) {
      cpivOpcode_l = SHAVE::CMU_CPIV_x16_v4i16_l_Myr2v3;
      cpivOpcode_h = SHAVE::CMU_CPIV_x16_v4i16_h_Myr2v3;
    }
    else {
      cpivOpcode_l = SHAVE::CMU_CPIV_x16_v8i16_l_Myr2v3;
      cpivOpcode_h = SHAVE::CMU_CPIV_x16_v8i16_h_Myr2v3;
    }

      if (SHAVE::VRF64_lRegClass.contains(destination.getReg()))
        tempDestReg = MRI.createVirtualRegister(&SHAVE::VRF64_lRegClass);
      else
        tempDestReg = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);

      // CMU.CPIV.x16 destination.(index) vector2.l
      MachineInstrBuilder cpiv_l = BuildMI(*MBB, MI, dbgLoc, TII->get(cpivOpcode_l), tempDestReg).add(vector1).add(vector2).addImm(index);
      // CMU.CPIV.x16 destinaton.(index+1) vector2.h
      //   destination is the same register as vector 1
      MachineInstrBuilder cpiv_h = BuildMI(*MBB, MI, dbgLoc, TII->get(cpivOpcode_h), destination.getReg()).addReg(tempDestReg).add(vector2).addImm(index+1);

      TII->finaliseMI(cpiv_l);
      TII->finaliseMI(cpiv_h);

    }
    break;
  }
  case SHAVE::INSERT_SUBVECTOR_imm_v4i8_v8i8:
  case SHAVE::INSERT_SUBVECTOR_imm_v4i8_v16i8: {
    if (index % 4 == 0) {
      // CMU.CPIV destination vector2
      //   destination is the same register as vector1
      unsigned cpivOpcode = opcode == SHAVE::INSERT_SUBVECTOR_imm_v4i8_v16i8 ? SHAVE::CMU_CPIV_x32_v4i32 : SHAVE::CMU_CPIV_x32_v2i32;
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(cpivOpcode), destination.getReg()).add(vector1).add(vector2).addImm(index/4));
    }
    else if (index % 2 == 0) {
      unsigned cpivOpcode_l, cpivOpcode_h;
      unsigned tempDestReg;
      if (opcode == SHAVE::INSERT_SUBVECTOR_imm_v4i8_v8i8) {
        cpivOpcode_l = SHAVE::CMU_CPIV_x16_v4i16_l_Myr2v3;
        cpivOpcode_h = SHAVE::CMU_CPIV_x16_v4i16_h_Myr2v3;
        tempDestReg = MRI.createVirtualRegister(&SHAVE::VRF64_lRegClass);
      }
      else {
        cpivOpcode_l = SHAVE::CMU_CPIV_x16_v8i16_l_Myr2v3;
        cpivOpcode_h = SHAVE::CMU_CPIV_x16_v8i16_h_Myr2v3;
        tempDestReg = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);
      }

      // CMU.CPIV.x16 destination.(index/2) vector2.l
      MachineInstrBuilder cpiv_l = BuildMI(*MBB, MI, dbgLoc, TII->get(cpivOpcode_l), tempDestReg).add(vector1).add(vector2).addImm(index/2);
      // CMU.CPIV.x16 destinaton.((index/2)+1) vector2.h
      //   destination is the same register as vector 1
      MachineInstrBuilder cpiv_h = BuildMI(*MBB, MI, dbgLoc, TII->get(cpivOpcode_h), destination.getReg()).addReg(tempDestReg).add(vector2).addImm((index/2)+1);
      TII->finaliseMI(cpiv_l);
      TII->finaliseMI(cpiv_h);
    }
    else {
      unsigned temp1_1 = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);
      unsigned temp1_2 = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);
      unsigned temp2_1 = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);
      unsigned temp2_2 = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);
      unsigned tempDestReg;

      unsigned cpviOpcode, cpivOpcode;

      if (opcode == SHAVE::INSERT_SUBVECTOR_imm_v4i8_v16i8) {
        tempDestReg = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);
        cpviOpcode = SHAVE::CMU_CPVI_x32;
        cpivOpcode = SHAVE::CMU_CPIV_x32_v4i32;
      }
      else {
        tempDestReg = MRI.createVirtualRegister(&SHAVE::VRF64_lRegClass);
        cpviOpcode = SHAVE::CMU_CPVI_x32_v2i32;
        cpivOpcode = SHAVE::CMU_CPIV_x32_v2i32;
      }

      // CMU.CPVI.x32 temp1 vector1.(index/4)
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(cpviOpcode), temp1_1).add(vector1).addImm(index/4));

      // CMU.VSZM.BYTE temp1 vector2 [MASK]
      unsigned vszmOpcode = SHAVE::CMU_VSZM_BYTE_IRF32;
      MachineInstrBuilder vszm1 = BuildMI(*MBB, MI, dbgLoc, TII->get(vszmOpcode), temp1_2).addReg(temp1_1).add(vector2);

      if (index % 4 == 1)
        vszm1 = vszm1.addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_LANE0).addImm(SHAVEVSZM::VSZM_LANE0+1).addImm(SHAVEVSZM::VSZM_LANE0+2); // [210D]
      else
        vszm1 = vszm1.addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_LANE0); // [0DDD]

      TII->finaliseMI(vszm1);

      // CMU.CPIV.x32 vector1.(index/4) temp1
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(cpivOpcode), tempDestReg).add(vector1).addReg(temp1_2).addImm(index/4));

      // CMU.CPVI.x32 temp2 vector1.((index/4)+1)
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(cpviOpcode), temp2_1).addReg(tempDestReg).addImm((index/4)+1));

      // CMU.VSZM.BYTE temp2 vector2 [MASK]
      MachineInstrBuilder vszm2 = BuildMI(*MBB, MI, dbgLoc, TII->get(vszmOpcode), temp2_2).addReg(temp2_1).add(vector2);

      if (index % 4 == 1)
        vszm2 = vszm2.addImm(SHAVEVSZM::VSZM_LANE0+3).addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_DISABLE); // [DDD3]
      else
        vszm2 = vszm2.addImm(SHAVEVSZM::VSZM_LANE0+1).addImm(SHAVEVSZM::VSZM_LANE0+2).addImm(SHAVEVSZM::VSZM_LANE0+3).addImm(SHAVEVSZM::VSZM_DISABLE); // [D321]

      TII->finaliseMI(vszm2);

      // CMU.CPIV.x32 vector1.((index/4)+1) temp2
      TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(cpivOpcode), destination.getReg()).addReg(tempDestReg).addReg(temp2_2).addImm((index/4)+1));
    }
    break;
  }
  case SHAVE::INSERT_SUBVECTOR_imm_v2i8_v8i8:
  case SHAVE::INSERT_SUBVECTOR_imm_v2i8_v16i8: {
    if (index % 2 == 0) {
      unsigned cpivOpcode;
      if (opcode == SHAVE::INSERT_SUBVECTOR_imm_v2i8_v8i8)
        cpivOpcode = SHAVE::CMU_CPIV_x16_v4i16_l_Myr2v3;
      else
        cpivOpcode = SHAVE::CMU_CPIV_x16_v8i16_l_Myr2v3;

      // CMU.CPIV.x16 destination.(index/2) vector2.l
      //   destination is the same register as vector1
      MachineInstrBuilder cpiv_l = BuildMI(*MBB, MI, dbgLoc, TII->get(cpivOpcode), destination.getReg()).add(vector1).add(vector2).addImm(index/2);
      TII->finaliseMI(cpiv_l);
    }
    else {
      if (index % 4 == 1) {
        unsigned cpviOpcode, cpivOpcode;
        unsigned temp1 = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);
        unsigned temp2 = MRI.createVirtualRegister(&SHAVE::IRF32RegClass);

        if (opcode == SHAVE::INSERT_SUBVECTOR_imm_v2i8_v8i8) {
          cpviOpcode = SHAVE::CMU_CPVI_x32_v2i32;
          cpivOpcode = SHAVE::CMU_CPIV_x32_v2i32;
        }
        else {
          cpviOpcode = SHAVE::CMU_CPVI_x32;
          cpivOpcode = SHAVE::CMU_CPIV_x32_v4i32;
        }

         // CMU.CPVI temp vector1.(index/4)
        TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(cpviOpcode), temp1).add(vector1).addImm(index/4));

        // CMU.VSZM.BYTE temp vector2 [D10D]
        TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(SHAVE::CMU_VSZM_BYTE_IRF32), temp2).addReg(temp1).add(vector2)
                                                                              .addImm(SHAVEVSZM::VSZM_DISABLE).addImm(SHAVEVSZM::VSZM_LANE0)
                                                                              .addImm(SHAVEVSZM::VSZM_LANE0+1).addImm(SHAVEVSZM::VSZM_DISABLE));

        // CMU.CPIV destination.(index/4) temp
        //   destination is the same register as vector1
        TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(cpivOpcode), destination.getReg()).add(vector1).addReg(temp2).addImm(index/4));
      }
      else {
        unsigned alignvecOpcode, cpivOpcode;
        unsigned temp1, temp2;

        if (opcode == SHAVE::INSERT_SUBVECTOR_imm_v2i8_v8i8) {
          alignvecOpcode = SHAVE::CMU_ALIGNVEC_imm_vrf64;
          cpivOpcode = SHAVE::CMU_CPIV_x16_v4i16_l_Myr2v3;
          temp1 = MRI.createVirtualRegister(&SHAVE::VRF64_lRegClass);
          temp2 = MRI.createVirtualRegister(&SHAVE::VRF64_lRegClass);
        }
        else {
          alignvecOpcode = SHAVE::CMU_ALIGNVEC_imm_vrf;
          cpivOpcode = SHAVE::CMU_CPIV_x16_v8i16_l_Myr2v3;
          temp1 = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);
          temp2 = MRI.createVirtualRegister(&SHAVE::VRF128RegClass);
        }

        // CMU.ALIGNVEC temp vector1 vector1 1
        TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(alignvecOpcode), temp1).add(vector1).add(vector1).addImm(1));

        // CMU.CPIV.x16 temp.(index/2) vector2.l
        MachineInstrBuilder cpiv = BuildMI(*MBB, MI, dbgLoc, TII->get(cpivOpcode), temp2).addReg(temp1).add(vector2).addImm(index/2);

        TII->finaliseMI(cpiv);

        // CMU.ALIGNVEC destination temp temp 15
        TII->finaliseMI(BuildMI(*MBB, MI, dbgLoc, TII->get(alignvecOpcode), destination.getReg()).addReg(temp2).addReg(temp2).addImm(15));
      }
    }
    break;
  }
  default:
    llvm_unreachable("Unrecognised INSERT_SUBVECTOR pseudo opcode during custom emission");
  }

  MI->eraseFromParent();
  return MBB;
}

MachineBasicBlock *SHAVELowering::EmitCustomDYNALLOC
(MachineInstr *MI, MachineBasicBlock *MBB) const {

  // Expand the pseudo instruction DYNALLOC:
  //  SHAVE::DYNALLOC $newSP

  if (SHAVEOptions::TrackStackUsage || SHAVEOptions::StackOverflowChecking) {
    const SHAVEInstrInfo *TII = SHAVEST.getInstrInfo();
    MachineFunction* MF = MBB->getParent();
    MachineRegisterInfo& regInfo = MF->getRegInfo();
    DebugLoc dbgLoc = MI->getDebugLoc();

    // insert the STACK PROBES sequence after the DYNALLOC instruction
    MachineBasicBlock::iterator it = MI;

    // The value of the newSP is being hold on the DYNALLOC pseudo:
    unsigned newSP = MI->getOperand(0).getReg();

    // symbol := "__stackMaximumExtent"
    MCSymbol *symbol = MF->getMMI().getContext().getOrCreateSymbol("__stackMaximumExtent");

    unsigned maxExtentReg = regInfo.createVirtualRegister(&SHAVE::IRF32RegClass);
    unsigned maxExtentReg2 = regInfo.createVirtualRegister(&SHAVE::IRF32RegClass);

    // maxExtentReg : = &__stackMaximumExtent
    TII->finaliseMI(BuildMI(*MBB, it, dbgLoc, TII->get(SHAVE::LSU_LDILSym), maxExtentReg2)
      .addExternalSymbol(symbol->getName().data()));

    TII->finaliseMI(BuildMI(*MBB, it, dbgLoc, TII->get(SHAVE::LSU_LDIHSym), maxExtentReg)
      .addReg(maxExtentReg2)
      .addExternalSymbol(symbol->getName().data()));

    if (SHAVEOptions::TrackStackUsage) {

      unsigned tmp1Reg = regInfo.createVirtualRegister(&SHAVE::IRF32RegClass);

      MachinePointerInfo memInfo;
      MachineMemOperand *MMO = MF->getMachineMemOperand(memInfo, MachineMemOperand::MOLoad, 4u, Align(16));
      MMO->setFlags(MachineMemOperand::MOVolatile);

      // tmp1Reg := LDO.32 maxExtentReg, 8  //__highWaterMark
      MachineInstrBuilder MIBLd = BuildMI(*MBB, it, dbgLoc, TII->get(SHAVE::LSU_LDO_i32), tmp1Reg)
        .addReg(maxExtentReg)
        .addImm(8)
        .addMemOperand(MMO);
      TII->finaliseMI(MIBLd);

      // IF newSP < tmp1Reg
      //   STO.32 newSP, maxExtentReg 8

      TII->finaliseMI(BuildMI(*MBB, it, dbgLoc, TII->get(SHAVE::CMU_CMII_i32))
        .addReg(newSP)
        .addReg(tmp1Reg));

      // *tmpReg := newSP  // __stackHighWater := newSP
      MachineMemOperand *MMOSt = MF->getMachineMemOperand(memInfo, MachineMemOperand::MOStore, 4u, Align(16));
      MMOSt->setFlags(MachineMemOperand::MOVolatile);

      int64_t useThisLSU = (SHAVEOptions::LSUVolatileLoadStorePolicy == SHAVEOptions::AlwaysUseLSU0) ? SHAVE::LSU0 : SHAVE::LSU1;
      BuildMI(*MBB, it, dbgLoc, TII->get(SHAVE::LSU_STO_i32))
        .addReg(newSP)
        .addReg(maxExtentReg)
        .addImm(8)
        .add(MachineOperand::CreateImm(SHAVECC::LT))
        .addReg(SHAVE::CC_CMU0)
        .addImm(useThisLSU)
        .addMemOperand(MMOSt);  // FIXME: Movidius - should this not precede adding the predicate?  Perhaps this is why the policy is ignored?
    }

    if (SHAVEOptions::StackOverflowChecking) {

      MachinePointerInfo memInfo;
      MachineMemOperand *MMO = MF->getMachineMemOperand(memInfo, MachineMemOperand::MOLoad, 4u, Align(16));
      MMO->setFlags(MachineMemOperand::MOVolatile);

      unsigned tmp2Reg = regInfo.createVirtualRegister(&SHAVE::IRF32RegClass);

      TII->finaliseMI(BuildMI(*MBB, it, dbgLoc, TII->get(SHAVE::LSU_LD32_Raw), tmp2Reg)
        .addReg(maxExtentReg)
        .addMemOperand(MMO));

      // IF SP < __stackMaximumExtent
      //   I18 := -3
      //   SWIH 0x3

      TII->finaliseMI(BuildMI(*MBB, it, dbgLoc, TII->get(SHAVE::CMU_CMII_i32))
        .addReg(newSP)
        .addReg(tmp2Reg));

      // If I19 is less than the top-of-stack, then an overflow has occurred so abort
      // with 'SHAVEExitCodes_t::SHAVEExitStackOverflow' and set 'IRF18' to '-3'
      BuildMI(*MBB, it, dbgLoc, TII->get(SHAVE::LSU_LDIL), SHAVE::I18)
        .addImm(-3)
        .add(MachineOperand::CreateImm(SHAVECC::LT))
        .addReg(SHAVE::CC_CMU0)
        .addImm(SHAVE::LSU0);

      BuildMI(*MBB, it, dbgLoc, TII->get(SHAVE::LSU_LDIH), SHAVE::I18)
        .addReg(SHAVE::I18)
        .addImm(-3)
        .add(MachineOperand::CreateImm(SHAVECC::LT))
        .addReg(SHAVE::CC_CMU0)
        .addImm(SHAVE::LSU0);

      BuildMI(*MBB, it, dbgLoc, TII->get(SHAVE::BRU_SWIH_imm))
        .addImm(0x3)    // SHAVEProcessExitStackOverflow
        .addReg(SHAVE::I18, RegState::Implicit)
        .add(MachineOperand::CreateImm(SHAVECC::LT))
        .addReg(SHAVE::CC_CMU0);
    }
  }

  // Erase the SHAVE_DYNALLOC pseudo instruction
  MI->eraseFromBundle();

  return MBB;
}

MachineBasicBlock *SHAVELowering::EmitInstrWithCustomInserter(MachineInstr &MI, MachineBasicBlock *BB) const {
  switch (MI.getOpcode()) {
  case SHAVE::DYNALLOC:
    return EmitCustomDYNALLOC(&MI, BB);
  case SHAVE::SHAVE_CALL:
    return EmitCustomCall(&MI, BB);
  case SHAVE::SHAVE_RETURN:
    return EmitCustomReturn(&MI, BB);
  case SHAVE::EXTRACT_SUBVECTOR_v2i32_v4i32:
  case SHAVE::EXTRACT_SUBVECTOR_v4i16_v8i16:
  case SHAVE::EXTRACT_SUBVECTOR_v2i16_v8i16:
  case SHAVE::EXTRACT_SUBVECTOR_v2i16_v4i16:
  case SHAVE::EXTRACT_SUBVECTOR_v8i8_v16i8:
  case SHAVE::EXTRACT_SUBVECTOR_v4i8_v16i8:
  case SHAVE::EXTRACT_SUBVECTOR_v2i8_v16i8:
  case SHAVE::EXTRACT_SUBVECTOR_v4i8_v8i8:
  case SHAVE::EXTRACT_SUBVECTOR_v2i8_v8i8:
  case SHAVE::EXTRACT_SUBVECTOR_v2i8_v4i8:
  case SHAVE::EXTRACT_SUBVECTOR_v2f32_v4f32:
  case SHAVE::EXTRACT_SUBVECTOR_v4f16_v8f16:
  case SHAVE::EXTRACT_SUBVECTOR_v2f16_v8f16:
  case SHAVE::EXTRACT_SUBVECTOR_v2f16_v4f16:
    return EmitCustomEXTRACT_SUBVECTOR(&MI, BB);
  case SHAVE::INSERT_SUBVECTOR_imm_v2i32_v4i32:
  case SHAVE::INSERT_SUBVECTOR_imm_v4i16_v8i16:
  case SHAVE::INSERT_SUBVECTOR_imm_v2i16_v8i16:
  case SHAVE::INSERT_SUBVECTOR_imm_v2i16_v4i16:
  case SHAVE::INSERT_SUBVECTOR_imm_v8i8_v16i8:
  case SHAVE::INSERT_SUBVECTOR_imm_v4i8_v16i8:
  case SHAVE::INSERT_SUBVECTOR_imm_v2i8_v16i8:
  case SHAVE::INSERT_SUBVECTOR_imm_v4i8_v8i8:
  case SHAVE::INSERT_SUBVECTOR_imm_v2i8_v8i8:
  case SHAVE::INSERT_SUBVECTOR_imm_v2i8_v4i8:
  case SHAVE::INSERT_SUBVECTOR_imm_v2f32_v4f32:
  case SHAVE::INSERT_SUBVECTOR_imm_v4f16_v8f16:
  case SHAVE::INSERT_SUBVECTOR_imm_v2f16_v8f16:
  case SHAVE::INSERT_SUBVECTOR_imm_v2f16_v4f16:
    return EmitCustomINSERT_SUBVECTOR(&MI, BB);
  default:
    llvm_unreachable("Don't know how to custom insert this instruction");
    return BB;
  }
}



//===----------------------------------------------------------------------===//
// Calling Convention Implementation
//===----------------------------------------------------------------------===//

// FIXME: Movidius - why do we need to do this, can LLVM not deduce it from the CallingConv in the TD?
SDValue SHAVELowering::LowerFormalArguments(SDValue Chain,
                                            CallingConv::ID CallConv, bool isVarArg,
                                            const SmallVectorImpl<ISD::InputArg>  &Ins,
                                            const SDLoc &dbgLoc, SelectionDAG &DAG,
                                            SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction& MF = DAG.getMachineFunction();
  MachineRegisterInfo& regInfo = MF.getRegInfo();
  SHAVEMachineFunctionInfo* shaveFuncInfo = MF.getInfo<SHAVEMachineFunctionInfo>();
  LLVMContext& Ctx = *DAG.getContext();

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16>  argLocs;
  CCState CCInfo(CallConv, isVarArg, MF, argLocs, Ctx);

  // Check the calling convention
  // FIXME: Movidius - This should be a function of the calling-convention ('cdecl' or 'stdcall')
  CCInfo.AnalyzeFormalArguments(Ins, hasFeature(SHAVE::HasVRF512_Feature) ? shaveV3_CallingConv : Shave_CallingConv);
  SmallVector<SDValue, 16> outChains;
  SDValue glueValue;

  for (unsigned argIdx = 0; argIdx < argLocs.size(); argIdx++) {
    CCValAssign& VA = argLocs[argIdx];

    // Handle arguments passed in registers
    if (VA.isRegLoc()) {
      if (VA.needsCustom())
        llvm_unreachable("Not expecting custom argument register handling");

      // FIXME: Movidius - can we not just use 'getRegClassFor(VA.getLocVT())'?
      const TargetRegisterClass * regClass = nullptr;

      switch (VA.getLocVT().SimpleTy) {
      case MVT::i8:
        regClass = &SHAVE::IRF8_q0RegClass;
        break;
      case MVT::i16:
      case MVT::f16:
        regClass = &SHAVE::IRF16_lRegClass;
        break;
      case MVT::i32:
      case MVT::f32:
        regClass = &SHAVE::IRF32RegClass;
        break;
      case MVT::i64:
        regClass = &SHAVE::IRF64RegClass;
        break;
      case MVT::v2i8:
        if (hasFeature(SHAVE::HasVRF512_Feature)) regClass = &SHAVE::WVRF16_0RegClass;
        else                                      regClass = &SHAVE::VRF16_e0RegClass;
        break;
      case MVT::v4i8:
      case MVT::v2i16:
      case MVT::v2f16:
        if (hasFeature(SHAVE::HasVRF512_Feature)) regClass = &SHAVE::WVRF32_0RegClass;
        else                                      regClass = &SHAVE::VRF32_q0RegClass;
        break;
      case MVT::v8i8:
      case MVT::v4i16:
      case MVT::v4f16:
      case MVT::v2i32:
      case MVT::v2f32:
        if (hasFeature(SHAVE::HasVRF512_Feature)) regClass = &SHAVE::WVRF64_0RegClass;
        else                                      regClass = &SHAVE::VRF64_lRegClass;
        break;
      case MVT::v16i8:
      case MVT::v8i16:
      case MVT::v8f16:
      case MVT::v4i32:
      case MVT::v4f32:
        if (hasFeature(SHAVE::HasVRF512_Feature)) regClass = &SHAVE::WVRF128_0RegClass;
        else                                      regClass = &SHAVE::VRF128RegClass;
        break;
      case MVT::v8i32:
      case MVT::v16i16:
      case MVT::v32i8:
      case MVT::v8f32:
      case MVT::v16f16:
        assert(hasFeature(SHAVE::HasVRF512_Feature) && "256-bit vectors not supported on this platform");
        regClass = &SHAVE::WVRF256_0RegClass;
        break;
      case MVT::v16i32:
      case MVT::v32i16:
      case MVT::v64i8:
      case MVT::v16f32:
      case MVT::v32f16:
        assert(hasFeature(SHAVE::HasVRF512_Feature) && "512-bit vectors not supported on this platform");
        regClass = &SHAVE::WVRF512RegClass;
        break;
      default:
        llvm_unreachable("Unrecognized type found during formal argument lowering");
      }

      // Add the assigned register location as a LiveIn to the function
      unsigned int vreg = regInfo.createVirtualRegister(regClass);
      regInfo.addLiveIn(VA.getLocReg(), vreg);

      // Extract the value from the reg
      SDValue arg = DAG.getCopyFromReg(Chain, dbgLoc, vreg, VA.getLocVT(), glueValue);

      glueValue = arg.getValue(1);
      outChains.push_back(glueValue);
      InVals.push_back(arg);
      Chain = arg.getValue(1);
    } else if (VA.isMemLoc()) {
      // Create a stack slot from where to load the incoming argument
      int FI = MF.getFrameInfo().CreateFixedObject(VA.getLocVT().getStoreSize(), VA.getLocMemOffset(), true);

      // Issue a load from the stack slot
      SDValue frameIdxPtr = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
      SDValue load = DAG.getLoad(VA.getValVT(), dbgLoc, Chain, frameIdxPtr,
                                 MachinePointerInfo::getFixedStack(MF, FI));

      outChains.push_back(load.getValue(1));
      InVals.push_back(load);
    } else
      llvm_unreachable("Unable to pass arguments that are not in either a register or on the stack");
  }

  for (unsigned argIdx = 0; argIdx < argLocs.size(); argIdx++) {
    // If this register contains a pointer to an argument on the stack
    // which is being passed by value, then we need to issue a memcpy
    // to copy the argument over
    ISD::InputArg currentArg = Ins[argIdx];

    if (currentArg.Flags.isByVal()) {
      unsigned int size = currentArg.Flags.getByValSize();
      Align align = currentArg.Flags.getNonZeroByValAlign();

      // We can't copy 0 bytes
      if (size == 0)
        size = 1;

      // Allocate a slot on the stack to copy the variable into (destination)
      int FI = MF.getFrameInfo().CreateStackObject(size, align, false);
      SDValue frameIdxPtr = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));

      // Retrieve the pointer that was previously loaded into a register (source)
      SDValue arg = InVals[argIdx];
      SDValue memcpy = DAG.getMemcpy(Chain, dbgLoc, frameIdxPtr,
                                     arg.getValue(0), DAG.getConstant(size, dbgLoc, MVT::i32), align,
                                     false, false, false, MachinePointerInfo(), MachinePointerInfo());

      // Remove the pointer to the source from the list of arguments and put
      // the newly copied pointer in its place
      InVals.erase(InVals.begin() + argIdx);
      InVals.insert(InVals.begin() + argIdx, frameIdxPtr);
      outChains.erase(outChains.begin() + argIdx);
      outChains.insert(outChains.begin() + argIdx, memcpy);
    }
  }

  if (isVarArg) {
    // save the stack offset where the variable arguments begin
    unsigned varArgOffset = CCInfo.getStackSize();
    unsigned alignment = SHAVEST.getFrameLowering()->getStackAlignment();

    // adjust the stack offset to the next aligned position
    varArgOffset = ((varArgOffset + alignment - 1) & ~(alignment - 1));

    // save it for further use by va_start lowering
    shaveFuncInfo->setVarArgsStackOffset(varArgOffset);
  }

  if (!outChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, dbgLoc, MVT::Other, outChains);

  return  Chain;
}

// Check if we have sufficient registers to accommodate the return value
// else return false so that the return value may be sret demoted
bool SHAVELowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool isVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, hasFeature(SHAVE::HasVRF512_Feature) ? shaveV3_ReturnConv : origShave_ReturnConv);
}

// FIXME: Movidius - need to check for 'noreturn'
SDValue SHAVELowering::LowerReturn(SDValue Chain,
                                   CallingConv::ID CallConv, bool isVarArg,
                                   const SmallVectorImpl<ISD::OutputArg> &Outs,
                                   const SmallVectorImpl<SDValue> &OutVals,
                                   const SDLoc &dbgLoc, SelectionDAG &DAG) const {
  // Assign locations to all of the incoming arguments.
  assert (( 16 >= OutVals.size ()) && "Unexpectedly large number of output parameters" );
  SmallVector<CCValAssign, 16> retLocs;

  // Use an LLVM specified return convention
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
                 retLocs, *DAG.getContext());

  CCInfo.AnalyzeReturn(Outs, hasFeature(SHAVE::HasVRF512_Feature) ? shaveV3_ReturnConv : origShave_ReturnConv);

  SDValue flag;  // Initially empty

  // Make a collections of the regs that are known to be live-out
  SmallVector<SDValue, 4> retOps;
  retOps.push_back(Chain);

  // Copy the results to the output registers
  for (unsigned rvIdx = 0, size = retLocs.size(); rvIdx < size; rvIdx++) {
    CCValAssign& VA = retLocs[rvIdx];

    // Extract the value from the reg and add it to the chain
    Chain = DAG.getCopyToReg(Chain, dbgLoc, VA.getLocReg(), OutVals[rvIdx], flag);

    // Guarantee that all emitted copies are stuck together with flags
    flag = Chain.getValue(1);
    retOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  // Update the chain to include the output registers
  retOps[0] = Chain;

  // If a 'flag' exists, push it back too
  if (flag.getNode())
    retOps.push_back(flag);

  // insert a return flag that will be transformed later into an indirect jump
  Chain = DAG.getNode(SHAVEISD::RET_FLAG, dbgLoc, MVT::Other, retOps);

  // DAG.viewGraph();
  return Chain;
}

// FIXME: Movidius - use doesNotRet!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
SDValue SHAVELowering::LowerCall(CallLoweringInfo &CLI, SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG                     = CLI.DAG;
  SDLoc &dbgLoc                         = CLI.DL;
  SmallVector<ISD::OutputArg, 32> &Outs = CLI.Outs;
  SmallVector<SDValue, 32> &OutVals     = CLI.OutVals;
  SmallVector<ISD::InputArg, 32> &Ins   = CLI.Ins;
  SDValue Chain                         = CLI.Chain;
  SDValue Callee                        = CLI.Callee;
  bool &isTailCall                      = CLI.IsTailCall;
  CallingConv::ID CallConv              = CLI.CallConv;
  bool isVarArg                         = CLI.IsVarArg;
  DataLayout dataLayout                 = DAG.getDataLayout();

  // Use an LLVM specified calling convention
  // Assign locations to the outgoing operands
  SmallVector<CCValAssign, 16> argLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), argLocs, *DAG.getContext());

  // Analyze the call operands
  CCInfo.AnalyzeCallOperands(Outs, hasFeature(SHAVE::HasVRF512_Feature) ? shaveV3_CallingConv : Shave_CallingConv);

  assert(argLocs.size() == Outs.size());
  assert(argLocs.size() == OutVals.size());

  SmallVector<SDValue, 16> outChains;
  MachineFunction &machineFn = DAG.getMachineFunction();
//  SHAVEMachineFunctionInfo *funcInfo = machineFn.getInfo<SHAVEMachineFunctionInfo>();

  // FIXME: Movidius - TODO: SHAVE doesn't handle tail calls for now
  isTailCall = false;

  // Get the stack requirement for passing the outbound arguments
  unsigned argsSize = CCInfo.getStackSize();
  unsigned stackAlign = SHAVEST.getFrameLowering()->getStackAlignment();

  // Convert the callee node to a target node, just to make sure that lowering doesn't lower it
  if (GlobalAddressSDNode *globalAddr = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(globalAddr->getGlobal(), dbgLoc, getPointerTy(dataLayout));
  else if (ExternalSymbolSDNode *extSym = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(extSym->getSymbol(), getPointerTy(dataLayout));

  // Adjust the argsSize to ensure that the stack is aligned
  argsSize = (argsSize + stackAlign) & (~stackAlign);

  // Mark the start of the call sequence
  Chain = DAG.getCALLSEQ_START(Chain, argsSize, 0, dbgLoc);

  // Keep a record of store chains
  SmallVector<SDValue, 16> storeChains;

  // Before writing the CC registers, store the values (if any) that need to be passed by stack
  for (unsigned argIdx = 0; argIdx < argLocs.size(); argIdx++) {
    CCValAssign &VA = argLocs[argIdx];

    if (VA.isMemLoc()) {
      // Create a stack slot to hold the value to pass
      int frameIndex = machineFn.getFrameInfo().
              CreateFixedObject(VA.getLocVT().getStoreSize(), VA.getLocMemOffset(), true);
      // Fetch the LLVM Value Type
      Type *type = EVT(VA.getLocVT()).getTypeForEVT(*DAG.getContext());
      // and its alignment
      Align alignment = dataLayout.getABITypeAlign(type);;

      if (isVarArg) {
          // override the type specific alignment and
          // use a unique one for passing variable arguments
          alignment = SHAVEST.getFrameLowering()->getStackAlign();
      }

      // Issue a load from the stack slot
      SDValue frameIdxPtr = DAG.getFrameIndex(frameIndex, getPointerTy(dataLayout));
      SDValue store = DAG.getStore(Chain, dbgLoc, OutVals[argIdx], frameIdxPtr,
              MachinePointerInfo(), alignment);

      // Keep track of the chain produced by this store
      storeChains.push_back(store);
    }
  }

  // Merge all the chains produced by the stores (if there are any) and update the chain
  if (!storeChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, dbgLoc, MVT::Other, storeChains);

  // Keep a list of registers that contain the args to pass
  SmallVector<SDValue, 16> valuesToPass;
  // This flag will stuck together all the copies to regs
  SDValue currentFlag;
  bool hasRegOuts = false;

  for (unsigned argIdx = 0; argIdx < argLocs.size(); argIdx++) {
    CCValAssign &VA = argLocs[argIdx];

    if (VA.isRegLoc()) {
      // Insert the value into reg
      SDValue argVal = DAG.getCopyToReg(Chain, dbgLoc, VA.getLocReg(), OutVals[argIdx], currentFlag);

      // Collect the chain produced by the copy to reg
      currentFlag = argVal.getValue(1);
      // Keep a track of the registers that need to be passed to the call
      valuesToPass.push_back(DAG.getRegister(VA.getLocReg(), OutVals[argIdx].getValueType()));
      // Update the running chain
      Chain = argVal;
      hasRegOuts = true;
    }
  }

  // Set the preserved register mask.
  const TargetRegisterInfo *TRI = SHAVEST.getRegisterInfo();
  const uint32_t* PreservedMask = TRI->getCallPreservedMask(machineFn, CallConv);

  valuesToPass.push_back(DAG.getRegisterMask(PreservedMask));

  // Update the flag in case any copy to regs were issued
  if (hasRegOuts) {
    // Update the current flag
    currentFlag = Chain.getValue(1);
    // Append the flag
    valuesToPass.push_back(currentFlag);
  }

  // Prepend the Chain and the Call Target
  valuesToPass.insert(valuesToPass.begin(), Callee);
  valuesToPass.insert(valuesToPass.begin(), Chain);

  // Insert a SHAVEISD::CALL to represent the SHAVE call instruction (BRU.SWP)
  // this will be custom lowered and inserted
  SDValue callNode = DAG.getNode(SHAVEISD::CALL, dbgLoc, DAG.getVTList(MVT::Other, MVT::Glue), valuesToPass);

  // Update the running chain and flag
  Chain = callNode.getValue(0);
  currentFlag = callNode.getValue(1);

  // Insert this node to mark the end of the call sequence
  Chain = DAG.getCALLSEQ_END(Chain, DAG.getIntPtrConstant(argsSize, dbgLoc, true),
#if 0
                             Callee /* Others have: DAG.getIntPtrConstant(0, dbgLoc, true) */, currentFlag, dbgLoc);
#else
                             DAG.getIntPtrConstant(0, dbgLoc, true), currentFlag, dbgLoc);
#endif
  Chain = Chain.getValue(0);
  currentFlag = Chain.getValue(1);

  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> retLocs;
  // Use an LLVM specified return convention
  CCState ReturnInfo(CallConv, isVarArg, DAG.getMachineFunction(),
                     retLocs, *DAG.getContext());

  ReturnInfo.AnalyzeCallResult(Ins, hasFeature(SHAVE::HasVRF512_Feature) ? shaveV3_ReturnConv : origShave_ReturnConv);

  for (unsigned retIdx = 0; retIdx < retLocs.size(); retIdx++) {
    CCValAssign retInfo = retLocs[retIdx];
    unsigned retReg = retInfo.getLocReg();

    // Get a copy from the register assigned by the return convention
    SDValue retVal = DAG.getCopyFromReg(Chain, dbgLoc, retReg, retInfo.getLocVT(), currentFlag);

    Chain = retVal.getValue(1);
    // Update the running flag, that glues the nodes that make the call and return sequence
    currentFlag = retVal.getValue(2);

    // Add the value to the incoming list
    // i.e. the value enters the calling function at this point
    InVals.push_back(retVal.getValue(0));
  }

  return Chain;
}


//===----------------------------------------------------------------------===//
//                       SHAVE Inline Assembly Support
//===----------------------------------------------------------------------===//

/// getConstraintType - Given a constraint letter, return the type of
/// constraint it is for this target.
TargetLowering::ConstraintType
SHAVELowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'e':  // Sparc uses this, so handle as a register to keep 'scan-build' happy
    case 'f':  // Sparc uses this, so handle as a register to keep 'scan-build' happy
    case 'm':  // For SHAVE, map 'm' to a register
      return C_RegisterClass;
    // The base-class can handle everything else
    default:
      break;
    }
  }

  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass*> SHAVELowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                                                            StringRef Constraint,
                                                                                            MVT VT) const {
  if (Constraint.size() == 1) {
    // GCC Constraint Letters
    switch (Constraint[0]) {
    case 'r':
      switch (VT.SimpleTy) {
      case MVT::i64:  return std::make_pair(0U, &SHAVE::IRF64RegClass);
      case MVT::i32:  return std::make_pair(0U, &SHAVE::IRF32RegClass);
      case MVT::i16:  return std::make_pair(0U, &SHAVE::IRF16_lRegClass);
      case MVT::i8:   return std::make_pair(0U, &SHAVE::IRF8_q0RegClass);

      case MVT::f32:  return std::make_pair(0U, &SHAVE::IRF32RegClass);
      case MVT::f16:  return std::make_pair(0U, &SHAVE::IRF16_lRegClass);

      case MVT::v4i32:
      case MVT::v8i16:
      case MVT::v16i8:

      case MVT::v4f32:
      case MVT::v8f16:
	if (hasFeature(SHAVE::HasVRF512_Feature))
	  return std::make_pair(0U, &SHAVE::WVRF128_0RegClass);
	else
	  return std::make_pair(0U, &SHAVE::VRF128RegClass);

      case MVT::v2i32:
      case MVT::v4i16:
      case MVT::v8i8:

      case MVT::v2f32:
      case MVT::v4f16:
	if (hasFeature(SHAVE::HasVRF512_Feature))
	  return std::make_pair(0U, &SHAVE::WVRF64_0RegClass);
	else
	  return std::make_pair(0U, &SHAVE::VRF64_lRegClass);

      case MVT::v16i32:
      case MVT::v32i16:
      case MVT::v64i8:

      case MVT::v16f32:
      case MVT::v32f16:
	if (hasFeature(SHAVE::HasVRF512_Feature))
	  return std::make_pair(0U, &SHAVE::WVRF512RegClass);

      default:
        break;
      }
      break;

    case 'm':
      return std::make_pair(0U, &SHAVE::IRF32RegClass);

    default:
      break;
    }
  }

  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}


std::vector<unsigned> SHAVELowering::getRegClassForInlineAsmConstraint(const std::string &Constraint, EVT VT) const {
// FIXME - Movidius - This could do with a bit of refactoring
// FIXME - Movidius - The IRF register selections permit SP, LR and FP to be used! For VRF it excluded V0
  if (Constraint.size() != 1)
    return std::vector<unsigned>();

  switch (Constraint[0]) {
  case 'r':
    if ((VT == MVT::i64)) {
      static const unsigned RFVec[] = {
                       SHAVE::I1_2,   SHAVE::I2_3,   SHAVE::I3_4,   SHAVE::I4_5,
        SHAVE::I5_6,   SHAVE::I6_7,   SHAVE::I7_8,   SHAVE::I8_9,   SHAVE::I9_10,
        SHAVE::I10_11, SHAVE::I11_12, SHAVE::I12_13, SHAVE::I13_14, SHAVE::I14_15,
        SHAVE::I15_16, SHAVE::I16_17, SHAVE::I17_18,
        SHAVE::I20_21, SHAVE::I21_22, SHAVE::I22_23, SHAVE::I23_24, SHAVE::I24_25,
        SHAVE::I25_26, SHAVE::I26_27, SHAVE::I27_28, SHAVE::I28_29,
        0
      };
      return std::vector<unsigned>(RFVec, RFVec + sizeof(RFVec) / sizeof(unsigned));
    } else if ((VT == MVT::i32) || (VT == MVT::f32)) {
      static const unsigned RFVec[] = {
                    SHAVE::I1,  SHAVE::I2,  SHAVE::I3,  SHAVE::I4,
        SHAVE::I5,  SHAVE::I6,  SHAVE::I7,  SHAVE::I8,  SHAVE::I9,
        SHAVE::I10, SHAVE::I11, SHAVE::I12, SHAVE::I13, SHAVE::I14,
        SHAVE::I15, SHAVE::I16, SHAVE::I17, SHAVE::I18,
        SHAVE::I20, SHAVE::I21, SHAVE::I22, SHAVE::I23, SHAVE::I24,
        SHAVE::I25, SHAVE::I26, SHAVE::I27, SHAVE::I28, SHAVE::I29,
        0
      };
      return std::vector<unsigned>(RFVec, RFVec + sizeof(RFVec) / sizeof(unsigned));
    } else if ((VT == MVT::i16) || (VT == MVT::f16)) {
      static const unsigned RFVec[] = {
                      SHAVE::I1_l,  SHAVE::I2_l,  SHAVE::I3_l,  SHAVE::I4_l,
        SHAVE::I5_l,  SHAVE::I6_l,  SHAVE::I7_l,  SHAVE::I8_l,  SHAVE::I9_l,
        SHAVE::I10_l, SHAVE::I11_l, SHAVE::I12_l, SHAVE::I13_l, SHAVE::I14_l,
        SHAVE::I15_l, SHAVE::I16_l, SHAVE::I17_l, SHAVE::I18_l,
        SHAVE::I20_l, SHAVE::I21_l, SHAVE::I22_l, SHAVE::I23_l, SHAVE::I24_l,
        SHAVE::I25_l, SHAVE::I26_l, SHAVE::I27_l, SHAVE::I28_l, SHAVE::I29_l,
        0
      };
      return std::vector<unsigned>(RFVec, RFVec + sizeof(RFVec) / sizeof(unsigned));
    } else if ((VT == MVT::v16i8) || (VT == MVT::v8i16) || (VT == MVT::v4i32) ||
               (VT == MVT::v8f16) || (VT == MVT::v4f32)) {
      static const unsigned RFVec_Myr2[] = {
        SHAVE::V0,  SHAVE::V1,  SHAVE::V2,  SHAVE::V3,  SHAVE::V4,
        SHAVE::V5,  SHAVE::V6,  SHAVE::V7,  SHAVE::V8,  SHAVE::V9,
        SHAVE::V10, SHAVE::V11, SHAVE::V12, SHAVE::V13, SHAVE::V14,
        SHAVE::V15, SHAVE::V16, SHAVE::V17, SHAVE::V18, SHAVE::V19,
        SHAVE::V20, SHAVE::V21, SHAVE::V22, SHAVE::V23, SHAVE::V24,
        SHAVE::V25, SHAVE::V26, SHAVE::V27, SHAVE::V28, SHAVE::V29,
        SHAVE::V30, SHAVE::V31,
        0
      };
      static const unsigned RFVec_Myr4[] = {
        SHAVE::W0_128_0,  SHAVE::W1_128_0,  SHAVE::W2_128_0,  SHAVE::W3_128_0,  SHAVE::W4_128_0,
        SHAVE::W5_128_0,  SHAVE::W6_128_0,  SHAVE::W7_128_0,  SHAVE::W8_128_0,  SHAVE::W9_128_0,
        SHAVE::W10_128_0, SHAVE::W11_128_0, SHAVE::W12_128_0, SHAVE::W13_128_0, SHAVE::W14_128_0,
        SHAVE::W15_128_0, SHAVE::W16_128_0, SHAVE::W17_128_0, SHAVE::W18_128_0, SHAVE::W19_128_0,
        SHAVE::W20_128_0, SHAVE::W21_128_0, SHAVE::W22_128_0, SHAVE::W23_128_0, SHAVE::W24_128_0,
        SHAVE::W25_128_0, SHAVE::W26_128_0, SHAVE::W27_128_0, SHAVE::W28_128_0, SHAVE::W29_128_0,
        SHAVE::W30_128_0, SHAVE::W31_128_0,
        0
      };
      if (hasFeature(SHAVE::HasVRF128_Feature))
        return std::vector<unsigned>(RFVec_Myr2, RFVec_Myr2 + sizeof(RFVec_Myr2) / sizeof(unsigned));
      else
        return std::vector<unsigned>(RFVec_Myr4, RFVec_Myr4 + sizeof(RFVec_Myr4) / sizeof(unsigned));
    } else if ((VT == MVT::v8i8)  || (VT == MVT::v4i16) || (VT == MVT::v2i32) ||
               (VT == MVT::v4f16) || (VT == MVT::v2f32)) {
      static const unsigned RFVec_Myr2[] = {
        SHAVE::V0_l,  SHAVE::V1_l,  SHAVE::V2_l,  SHAVE::V3_l,  SHAVE::V4_l,
        SHAVE::V5_l,  SHAVE::V6_l,  SHAVE::V7_l,  SHAVE::V8_l,  SHAVE::V9_l,
        SHAVE::V10_l, SHAVE::V11_l, SHAVE::V12_l, SHAVE::V13_l, SHAVE::V14_l,
        SHAVE::V15_l, SHAVE::V16_l, SHAVE::V17_l, SHAVE::V18_l, SHAVE::V19_l,
        SHAVE::V20_l, SHAVE::V21_l, SHAVE::V22_l, SHAVE::V23_l, SHAVE::V24_l,
        SHAVE::V25_l, SHAVE::V26_l, SHAVE::V27_l, SHAVE::V28_l, SHAVE::V29_l,
        SHAVE::V30_l, SHAVE::V31_l,
        0
      };
      static const unsigned RFVec_Myr4[] = {
        SHAVE::W0_64_0,  SHAVE::W1_64_0,  SHAVE::W2_64_0,  SHAVE::W3_64_0,  SHAVE::W4_64_0,
        SHAVE::W5_64_0,  SHAVE::W6_64_0,  SHAVE::W7_64_0,  SHAVE::W8_64_0,  SHAVE::W9_64_0,
        SHAVE::W10_64_0, SHAVE::W11_64_0, SHAVE::W12_64_0, SHAVE::W13_64_0, SHAVE::W14_64_0,
        SHAVE::W15_64_0, SHAVE::W16_64_0, SHAVE::W17_64_0, SHAVE::W18_64_0, SHAVE::W19_64_0,
        SHAVE::W20_64_0, SHAVE::W21_64_0, SHAVE::W22_64_0, SHAVE::W23_64_0, SHAVE::W24_64_0,
        SHAVE::W25_64_0, SHAVE::W26_64_0, SHAVE::W27_64_0, SHAVE::W28_64_0, SHAVE::W29_64_0,
        SHAVE::W30_64_0, SHAVE::W31_64_0,
        0
      };
      if (hasFeature(SHAVE::HasVRF128_Feature))
        return std::vector<unsigned>(RFVec_Myr2, RFVec_Myr2 + sizeof(RFVec_Myr2) / sizeof(unsigned));
      else
        return std::vector<unsigned>(RFVec_Myr4, RFVec_Myr4 + sizeof(RFVec_Myr4) / sizeof(unsigned));
    } else if ((VT == MVT::v4i8)  || (VT == MVT::v2i16) || (VT == MVT::v2f16)) {
      // FIXME: Movidius - Need to use adapt for small vectors
      static const unsigned RFVec[] = {
                    SHAVE::I1,  SHAVE::I2,  SHAVE::I3,  SHAVE::I4,
        SHAVE::I5,  SHAVE::I6,  SHAVE::I7,  SHAVE::I8,  SHAVE::I9,
        SHAVE::I10, SHAVE::I11, SHAVE::I12, SHAVE::I13, SHAVE::I14,
        SHAVE::I15, SHAVE::I16, SHAVE::I17, SHAVE::I18,
        SHAVE::I20, SHAVE::I21, SHAVE::I22, SHAVE::I23, SHAVE::I24,
        SHAVE::I25, SHAVE::I26, SHAVE::I27, SHAVE::I28, SHAVE::I29,
        0
      };
      return std::vector<unsigned>(RFVec, RFVec + sizeof(RFVec) / sizeof(unsigned));
    }
    else if (VT == MVT::v2i8) {
      // FIXME: Movidius - Need to use adapt for small vectors
      static const unsigned RFVec[] = {
                      SHAVE::I1_l,  SHAVE::I2_l,  SHAVE::I3_l,  SHAVE::I4_l,
        SHAVE::I5_l,  SHAVE::I6_l,  SHAVE::I7_l,  SHAVE::I8_l,  SHAVE::I9_l,
        SHAVE::I10_l, SHAVE::I11_l, SHAVE::I12_l, SHAVE::I13_l, SHAVE::I14_l,
        SHAVE::I15_l, SHAVE::I16_l, SHAVE::I17_l, SHAVE::I18_l,
        SHAVE::I20_l, SHAVE::I21_l, SHAVE::I22_l, SHAVE::I23_l, SHAVE::I24_l,
        SHAVE::I25_l, SHAVE::I26_l, SHAVE::I27_l, SHAVE::I28_l, SHAVE::I29_l,
        0
      };
      return std::vector<unsigned>(RFVec, RFVec + sizeof(RFVec) / sizeof(unsigned));
    } else if (((VT == MVT::v64i8) || (VT == MVT::v32i16) || (VT == MVT::v16i32) ||
		(VT == MVT::v32f16) || (VT == MVT::v16f32)) && hasFeature(SHAVE::HasVRF512_Feature)) {
      static const unsigned RFVec[] = {
        SHAVE::W0,  SHAVE::W1,  SHAVE::W2,  SHAVE::W3,  SHAVE::W4,
        SHAVE::W5,  SHAVE::W6,  SHAVE::W7,  SHAVE::W8,  SHAVE::W9,
        SHAVE::W10, SHAVE::W11, SHAVE::W12, SHAVE::W13, SHAVE::W14,
        SHAVE::W15, SHAVE::W16, SHAVE::W17, SHAVE::W18, SHAVE::W19,
        SHAVE::W20, SHAVE::W21, SHAVE::W22, SHAVE::W23, SHAVE::W24,
        SHAVE::W25, SHAVE::W26, SHAVE::W27, SHAVE::W28, SHAVE::W29,
        SHAVE::W30, SHAVE::W31,
        0
      };
      return std::vector<unsigned>(RFVec, RFVec + sizeof(RFVec) / sizeof(unsigned));
    } else if (((VT == MVT::v32i8)  || (VT == MVT::v16i16) || (VT == MVT::v8i32) ||
		(VT == MVT::v16f16) || (VT == MVT::v8f32)) && hasFeature(SHAVE::HasVRF512_Feature)) {
      static const unsigned RFVec[] = {
        SHAVE::W0_256_0,  SHAVE::W1_256_0,  SHAVE::W2_256_0,  SHAVE::W3_256_0,  SHAVE::W4_256_0,
        SHAVE::W5_256_0,  SHAVE::W6_256_0,  SHAVE::W7_256_0,  SHAVE::W8_256_0,  SHAVE::W9_256_0,
        SHAVE::W10_256_0, SHAVE::W11_256_0, SHAVE::W12_256_0, SHAVE::W13_256_0, SHAVE::W14_256_0,
        SHAVE::W15_256_0, SHAVE::W16_256_0, SHAVE::W17_256_0, SHAVE::W18_256_0, SHAVE::W19_256_0,
        SHAVE::W20_256_0, SHAVE::W21_256_0, SHAVE::W22_256_0, SHAVE::W23_256_0, SHAVE::W24_256_0,
        SHAVE::W25_256_0, SHAVE::W26_256_0, SHAVE::W27_256_0, SHAVE::W28_256_0, SHAVE::W29_256_0,
        SHAVE::W30_256_0, SHAVE::W31_256_0,
        0
      };
      return std::vector<unsigned>(RFVec, RFVec + sizeof(RFVec) / sizeof(unsigned));
    }
    break;

    case 'm':
      {
        static const unsigned RFVec[] = {
                      SHAVE::I1,  SHAVE::I2,  SHAVE::I3,  SHAVE::I4,
          SHAVE::I5,  SHAVE::I6,  SHAVE::I7,  SHAVE::I8,  SHAVE::I9,
          SHAVE::I10, SHAVE::I11, SHAVE::I12, SHAVE::I13, SHAVE::I14,
          SHAVE::I15, SHAVE::I16, SHAVE::I17, SHAVE::I18,
          SHAVE::I20, SHAVE::I21, SHAVE::I22, SHAVE::I23, SHAVE::I24,
          SHAVE::I25, SHAVE::I26, SHAVE::I27, SHAVE::I28, SHAVE::I29,
          0
        };
        return std::vector<unsigned>(RFVec, RFVec + sizeof(RFVec) / sizeof(unsigned));
      }

  default:
    break;
  }

  return std::vector<unsigned>();
}

// FIXME: Movidius - the following functions could do with some careful rework to handle both 64-bit and 32-bit vectors
std::pair<const TargetRegisterClass*, uint8_t>
SHAVELowering::findRepresentativeClass(const TargetRegisterInfo *TRI, MVT VT) const {
  const TargetRegisterClass *RRC = 0;
  uint8_t Cost = 1;

  switch (VT.SimpleTy) {
  default:
    return TargetLowering::findRepresentativeClass(TRI, VT);
  case MVT::v2i8:
  case MVT::v4i8:
  case MVT::v8i8:
    if (hasFeature(SHAVE::HasVRF512_Feature))
      RRC =  &SHAVE::WVRF128_0RegClass;
    else
      RRC = &SHAVE::VRF128RegClass;

    break;
  }

  return std::make_pair(RRC, Cost);
}

bool SHAVELowering::isTruncateFree(Type * Ty1, Type * Ty2) const {
  return isTruncateFree(EVT::getEVT(Ty1), EVT::getEVT(Ty2));
}

bool SHAVELowering::isTruncateFree(EVT VT1, EVT VT2) const {
  if (!VT1.isSimple() || !VT2.isSimple())
    return false;

  return (VT1 == MVT::i32) && ((VT2 == MVT::i16) || (VT2 == MVT::i8));
}

TargetLoweringBase::LegalizeTypeAction SHAVELowering::getPreferredVectorAction(MVT VT) const {
  switch (VT.SimpleTy) {
  default:
    return TargetLoweringBase::getPreferredVectorAction(VT);
#ifdef FIXME_VEC_VAARG
  case MVT::v2i32:
  case MVT::v4i16:
#endif // FIXME_VEC_VAARG
  case MVT::v2i16:
#ifdef FIXME_VEC_VAARG
  case MVT::v8i8:
  case MVT::v4i8:
#endif // FIXME_VEC_VAARG
  case MVT::v2i8:
#ifdef FIXME_VEC_VAARG
  case MVT::v2f32:
  case MVT::v2f16:
  case MVT::v4f16:
#endif // FIXME_VEC_VAARG
    return TypeWidenVector;
#ifdef FIXME_VEC_VAARG
  case MVT::v16i32:
  case MVT::v8i32:
  case MVT::v16i16:
  case MVT::v16f32:
  case MVT::v8f32:
    return TypeSplitVector;
#endif // FIXME_VEC_VAARG
  case MVT::v16f16:
  case MVT::v2i1:
    return TypeSplitVector;
  }
}


//===----------------------------------------------------------------------===//
//                       SHAVE Vector Shuffle Analysis
//===----------------------------------------------------------------------===//
ShuffleAnalysis::ShuffleAnalysis(ArrayRef<int> mask, EVT vecVT, SDValue v0, SDValue v1, const SHAVESubtarget& shavest)
: VecVT(vecVT), V0(v0), V1(v1), SHAVEST(shavest)
{
  EVT EltVT = VecVT.getVectorElementType();
  assert(VecVT.getSizeInBits() > 0 &&
         "Cannot shuffle a vector with size in bits of 0");
  assert(EltVT.getSizeInBits() > 0 &&
         "Cannot shuffle a vector with element size in bits of 0");
  unsigned NumVecElts = VecVT.getSizeInBits() / EltVT.getSizeInBits();

  if (mask.size() == NumVecElts && VecVT.getSizeInBits() != 128) {
    // Change and pad mask according to 128bit.
    // For example, v4i16 vector's mask [0, 1, 2, 5]
    // will be [0, 1, 2, 9, -1, -1, -1, -1].
    int NumPadMask = (128 - VecVT.getSizeInBits()) / EltVT.getSizeInBits();

    for (unsigned i = 0; i < mask.size(); i++) {
      if (mask[i] >= NumPadMask)
        // From above example, '5' is changed '9';
        TmpMask.push_back(mask[i] + NumPadMask);
      else
        TmpMask.push_back(mask[i]);
    }

    for (int i = 0; i < NumPadMask; i++)
      TmpMask.push_back(-1);

    Mask = ArrayRef(TmpMask);
  }
  else
    Mask = mask;

  Status = Expand;
  Cost = 0;
  V0BeforeV1 = V1BeforeV0 = false;
  V0Identity = true;
  SplatV0 = true;
  AllHighV0 = true;
  IsSwizzle8V0 = true;
  IsAnyExt16V0 = IsAnyExt32V0 = true;
  IsLinear1 = IsLinear2 = IsLinear4 = true;
  IsShiftCandidate = false;
  IsLinear1WithWrapAround = IsLinear2WithWrapAround = IsLinear4WithWrapAround = true;
  IsSubvectorInsert = true;
  Singleton = true;
  SplatLaneV0 = -1;
  MinIdxV0 = MinIdxV1 = mask.size();
  MaxIdxV0 = MaxIdxV1 = -1;
  SubvectorInsertIndex = SubvectorExtractIndex = -1;
  SubvectorInsertSize = 0;
  NumUndef = 0;
  FirstDefV0 = -1;
  LastDefV0 = -1;
  ConcatOffset = 0;
  ShiftOffset = 0;
  InterleaveType = NoInterleave;
  Action = ShuffleAnalysis::LowerInvalid;
}


#ifndef NDEBUG
namespace {
  const char *getActionName(ShuffleAnalysis::ShuffleLoweringAction action) {
    switch (action) {
    default:
      return "Unknown";
    case ShuffleAnalysis::LowerInvalid:
      return "Invalid";
    case ShuffleAnalysis::LowerUndefined:
      return "Undefined value";
    case ShuffleAnalysis::LowerIdentityV0:
      return "Identity (V0)";
    case ShuffleAnalysis::LowerSplatV0:
      return "Splat (V0)";
    case ShuffleAnalysis::LowerSwapLowHighV0:
      return "Low/high swap (V0)";
    case ShuffleAnalysis::LowerTruncateV0:
      return "Truncate (V0)";
    case ShuffleAnalysis::LowerAnyExt16V0:
      return "AnyExt16 (V0)";
    case ShuffleAnalysis::LowerAnyExt32V0:
      return "AnyExt32 (V0)";
    case ShuffleAnalysis::LowerSwizzleWordV0:
      return "SwizzleWord (V0)";
    case ShuffleAnalysis::LowerSwizzleHalfWordV0:
      return "ShuffleHalfWord (V0)";
    case ShuffleAnalysis::LowerSwizzleByteV0:
      return "SwizzleByte (V0)";
    case ShuffleAnalysis::LowerRotateV0:
      return "Rotate (V0)";
    case ShuffleAnalysis::LowerSwapVectors:
      return "Swap vectors";
    case ShuffleAnalysis::LowerConcatVectors:
      return "Concatenate vectors";
    case ShuffleAnalysis::LowerInterleaveVectors:
      return "Interleave vectors";
    case ShuffleAnalysis::LowerDeinterleaveVectors:
      return "Deinterleave vectors";
    case ShuffleAnalysis::LowerSubvectorInsert:
      return "Subvector insert";
    case ShuffleAnalysis::LowerShlVector:
      return "Vector 128-bit shift left";
    case ShuffleAnalysis::LowerShrVector:
      return "Vector 128-bit shift right";
    }
  }
}
#endif // NDEBUG


bool ShuffleAnalysis::assignLeafAction(ShuffleLoweringAction action, int cost) {
  Action = action;
  Status = Leaf;
  Cost += cost;
  DEBUG(dbgs() << "Leaf action: " << getActionName(action) << "\n");
  return (action != LowerInvalid);
}

bool ShuffleAnalysis::assignTransformAction(ShuffleLoweringAction action,
                                            int cost, ArrayRef<int> NewMask,
                                            SDValue LHS, SDValue RHS) {
  ShuffleAnalysis NewSA(NewMask, VecVT, LHS, RHS, SHAVEST);

  if ((action != LowerInvalid) && NewSA.analyse()) {
    Status = TransformMask;
    Action = action;
    Cost += cost;
    Cost += NewSA.Cost;
    DEBUG(dbgs() << "Transform action: " << getActionName(action) << "\n");
    return true;
  }

  return false;
}

bool ShuffleAnalysis::canLowerAsSwizzleByte(int offset) {
  if (VecVT != MVT::v16i8)
    return false;

  // Make sure the swizzle does not cross word boundaries, since we cannot
  // efficiently handle it.
  EVT VT = VecVT.getVectorElementType();
  assert(VT.getSizeInBits() > 0 &&
         "v16i8 element size in bits should not be 0!");
  unsigned ElePerWord = (32 / VT.getSizeInBits());
  unsigned NumParts = (VecVT.getVectorNumElements() / ElePerWord);
  int MaskPos = 0;
  bool InsideWordBounds = true;

  for (unsigned i = 0; i < NumParts; i++) {
    for(unsigned j = 0; j < ElePerWord; j++) {
      int Lane = (Mask[MaskPos + j] - offset);
      int MaskEnd = (MaskPos + ElePerWord);

      InsideWordBounds &= (Lane < 0) | ((Lane >= MaskPos) & (Lane < MaskEnd));
    }

    MaskPos += ElePerWord;
  }

  return InsideWordBounds;
}

void ShuffleAnalysis::dumpMask(raw_ostream &O) {
  unsigned MaskSize = Mask.size();

  O << "<";

  for (unsigned i = 0; i < MaskSize; i++) {
    int lane = Mask[i];

    if (i > 0)
      dbgs() << ",";

    if (lane >= 0)
      O << lane;
    else
      O << "u";
  }

  O << ">";
}

bool ShuffleAnalysis::analyse() {
  DEBUG(dbgs() << "ShuffleAnalysis::analyse(");
  DEBUG(dumpMask(dbgs()));
  DEBUG(dbgs() << ")\n");

  // Analyze the shuffle mask, looking for these special cases:
  // - elements only come from one of the vector.
  // - elements of one vector follow the elements of the other vector.
  // - identities.
  // - splats.
  // - extensions.

  // Separate the mask elements into two lists, one for each vector.
  unsigned MaskSize = Mask.size();
  int FirstHighLane = (int)MaskSize / 2;
  int i = 0;
  bool subvectorInsert = true;
  bool subvectorError = false;
  bool foundSecond = false;
  bool isFirstHalfInterleave = true;
  bool isLastHalfInterleave = true;

  while(i < (int)MaskSize && subvectorInsert) {
    int lane = Mask[i];
    int previousLane = lane - 1;

    if (lane >= (int)MaskSize) {
      if (foundSecond)
        subvectorInsert = false;
      foundSecond = true;

      int j = 0;
      SubvectorInsertIndex = i;
      SubvectorExtractIndex = lane - (int)MaskSize;

      while (lane >= (int)MaskSize && i < (int)MaskSize && subvectorInsert) {
        int realLane = lane - (int)MaskSize;
        if (realLane != j && j > 0)
          subvectorInsert = false;
        if (lane != (previousLane + 1)) {
          subvectorInsert = false;
        }
        j++; i++;
        if (i != (int)MaskSize) {
          previousLane = lane;
          lane = Mask[i];
        }
      }

      if (!isPowerOf2_32(j))
        subvectorInsert = false;
      else
        SubvectorInsertSize = j;
    }
    else {
      if (lane != i)
        subvectorInsert = false;
      i++;
    }
  }

  if (subvectorInsert && subvectorError)
    return false;

  if (!foundSecond || !subvectorInsert)
    IsSubvectorInsert = false;

  for (i = 0; i < (int)MaskSize; i++) {
    int lane = Mask[i];
    bool LaneUndef = (lane < 0);

    if (lane >= (int)MaskSize) {
      int realLane = (lane - (int)MaskSize);

      MinIdxV1 = std::min(MinIdxV1, i);
      MaxIdxV1 = std::max(MaxIdxV1, i);
      MaskV1.push_back(realLane);
      V0Identity = false;
      IsSwizzle8V0 = false;
      IsAnyExt16V0 = false;
      IsAnyExt32V0 = false;
    } else if (lane >= 0) {
      MinIdxV0 = std::min(MinIdxV0, i);
      MaxIdxV0 = std::max(MaxIdxV0, i);
      MaskV0.push_back(lane);
      V0Identity &= (lane == i);

      if (SplatV0 && (SplatLaneV0 < 0))
        SplatLaneV0 = lane;
      else  if (SplatLaneV0 != lane)
        SplatV0 = false;

      if (lane < FirstHighLane)
        AllHighV0 = false;

      // Look for extension patterns.
      int ZExt32Element = (i & 3);
      int ZExt16Element = (i & 1);

      IsAnyExt16V0 &= (ZExt16Element == 0);
      IsAnyExt32V0 &= (ZExt32Element == 0);

      if (IsAnyExt16V0)
        AnyExt16Mask.push_back(lane);

      if (IsAnyExt32V0)
        AnyExt32Mask.push_back(lane);

      // Look for 8-element swizzle patterns.
      if (MaskSize == 16) {
        // Match: <a, a+1, b, b+1, ...> where a, b, ... are 2-aligned.
        if (i & 1)
          IsSwizzle8V0 &= ((lane - Mask[i-1]) == 1);
        else
          IsSwizzle8V0 &= ((lane & 1) == 0);
      }
    } else
      NumUndef++;

    // Look for simple linear patterns.
    if ((FirstDefV0 < 0) && !LaneUndef)
      FirstDefV0 = lane;

    if (!LaneUndef)
      LastDefV0 = lane;

    IsLinear1 &= (FirstDefV0 < 0) | (lane == (FirstDefV0 + (1 * (i - MinIdxV0)))) | LaneUndef;
    IsLinear2 &= (FirstDefV0 < 0) | (lane == (FirstDefV0 + (2 * (i - MinIdxV0)))) | LaneUndef;
    IsLinear4 &= (FirstDefV0 < 0) | (lane == (FirstDefV0 + (4 * (i - MinIdxV0)))) | LaneUndef;

    IsLinear1WithWrapAround &= (FirstDefV0 < 0) | (lane == (FirstDefV0 + (1 * (i - MinIdxV0))) % (int)MaskSize) | LaneUndef;
    IsLinear2WithWrapAround &= (FirstDefV0 < 0) | (lane == (FirstDefV0 + (2 * (i - MinIdxV0))) % (int)MaskSize) | LaneUndef;
    IsLinear4WithWrapAround &= (FirstDefV0 < 0) | (lane == (FirstDefV0 + (4 * (i - MinIdxV0))) % (int)MaskSize) | LaneUndef;

    // Look for the 'singleton' pattern.
    if (i == 0)
      Singleton &= !LaneUndef;
    else
      Singleton &= LaneUndef;

    if (LaneUndef) {
      isFirstHalfInterleave = false;
      isLastHalfInterleave = false;
    }

    // Look for interleaves
    int interleaveIndex = i / 2;
    // Need to account for both first half and last half interleaves
    // [0, 8, 1, 9, 2, 10, 3, 11]
    if (isFirstHalfInterleave) {
      if (i % 2 == 0)
        isFirstHalfInterleave &= lane == interleaveIndex;
      // The second vector lanes should be MaskSize higher than the first vector
      // lanes
      else
        isFirstHalfInterleave &= lane == (int)MaskSize + interleaveIndex;
    }
    // [4, 12, 5, 13, 6, 14, 7, 15]
    if (isLastHalfInterleave) {
      // Same thing but offset by MaskSize / 2
      int offset = MaskSize / 2;
      if (i % 2 == 0)
        isLastHalfInterleave &= lane == offset + interleaveIndex;
      else
        isLastHalfInterleave &= lane == offset + (int)MaskSize + interleaveIndex;
    }
  }

  if (isFirstHalfInterleave && isLastHalfInterleave)
    llvm_unreachable("Shuffle cannot both interleave first and last half");

  if (isFirstHalfInterleave)
    InterleaveType = FirstHalfInterleave;
  else if (isLastHalfInterleave)
    InterleaveType = LastHalfInterleave;
  else
    InterleaveType = NoInterleave;

  if (IsLinear1 && LastDefV0 == (int) MaskSize - 1 && MaxIdxV0 == (int) MaskSize - 1)
    IsShiftCandidate = true;
  else if (IsLinear1 && FirstDefV0 == 0 && MinIdxV0 == 0)
    IsShiftCandidate = true;

  V0BeforeV1 = (MaxIdxV0 >= 0) && (MaxIdxV0 < MinIdxV1);
  V1BeforeV0 = (MaxIdxV1 >= 0) && (MaxIdxV1 < MinIdxV0);

  if (Singleton) {
    IsLinear1 = IsLinear2 = IsLinear4;
    IsLinear1WithWrapAround = IsLinear2WithWrapAround = IsLinear4WithWrapAround;
  }

  IsSwizzle8V0 &= ((MaskSize == 8) || (MaskSize == 4) || (MaskSize == 16));
  IsAnyExt16V0 &= ((AnyExt16Mask.size() * 2 * (128 / VecVT.getSizeInBits())) == MaskSize);
  IsAnyExt32V0 &= ((AnyExt32Mask.size() * 4 * (128 / VecVT.getSizeInBits())) == MaskSize);

  if (NumUndef == MaskSize)
    return assignLeafAction(LowerUndefined, 0);
  else if (MaskV0.size() && !MaskV1.size())
    return analyseV0Only();
  else if (MaskV1.size() && !MaskV0.size())
    return analyseV1Only();
  else
    return analyseV0V1Mix();
}

bool ShuffleAnalysis::analyseV0Only() {
  // Elements come from v0 only, ex: 2 1 0 3.
  EVT VT = VecVT.getScalarType();
  unsigned LaneWidth = VT.getSizeInBits();
  bool WordLane = (LaneWidth == 32);
  unsigned MaskSize = Mask.size();

  if (V0Identity)
    return assignLeafAction(LowerIdentityV0, 0);

  // Try splat patterns.
  if (SplatV0) {
    int InsertLane = 0;
    SDValue Scalar = SHAVELowering::findScalarInsert(V0, InsertLane);
    bool ScalarIdiom = Scalar.getNode() && (InsertLane == SplatLaneV0);

    if (!Singleton || ScalarIdiom) {
      // Only lower <1,u,u,u,...> as a splat if the source is a scalar.
      // It is more efficient to rotate the vector than to do an extraction,
      // insertion and splat.
      return assignLeafAction(LowerSplatV0, 1);
    }
  }

  if (WordLane)
    return assignLeafAction(LowerSwizzleWordV0, 1);

  // Try rotation patterns.
  if (IsLinear1 || IsLinear1WithWrapAround) {
    int ConcatIndex = FirstDefV0 - MinIdxV0;

    if (ConcatIndex < 0)
      ConcatIndex += (int)MaskSize;

    ConcatOffset = ConcatIndex * (VT.getSizeInBits() / 8);

    return assignLeafAction(LowerRotateV0, 1);
  }

  // Try extension patterns.
  if (IsAnyExt16V0) {
    while (AnyExt16Mask.size() < MaskSize)
      AnyExt16Mask.push_back(-1);

    if (assignTransformAction(LowerAnyExt16V0, 1, AnyExt16Mask, V0, V0))
      return true;
  }

  if (IsAnyExt32V0) {
    while (AnyExt32Mask.size() < MaskSize)
      AnyExt32Mask.push_back(-1);

    if (assignTransformAction(LowerAnyExt32V0, 1, AnyExt32Mask, V0, V0))
      return true;
  }

  // Check for SwizzleHalfWordV0 patterns
  if ((VecVT == MVT::v8i16) || (VecVT == MVT::v8f16)) {
    assignLeafAction(LowerSwizzleHalfWordV0, 1);
    return true;
  }

  // Try byte swizzle patterns.
  if (canLowerAsSwizzleByte(0))
    return assignLeafAction(LowerSwizzleByteV0, 1);

  // If all the elements are from the high part of the first vector, few of the
  // patterns above can be matched. Try to swap the low and high part of the
  // first vector and retry selecting a shuffle instruction.
  if (AllHighV0) {
    int FirstHighLane = (int)MaskSize / 2;

    MaskV0.clear();
    for (unsigned i = 0; i < MaskSize; i++) {
      int lane = Mask[i];

      if (lane >= 0)
        MaskV0.push_back(lane - FirstHighLane);
      else
        MaskV0.push_back(lane);
    }

    if (assignTransformAction(LowerSwapLowHighV0, 1, MaskV0, V0, V0))
      return true;
  }

  return false;
}

bool ShuffleAnalysis::analyseV1Only() {
  // Elements come from v1 only, ex: 5 4 7 6.
  // Canonicalise this by swapping v0 and v1.
  unsigned MaskSize = Mask.size();

  MaskV0.clear();

  for (unsigned i = 0; i < MaskSize; i++) {
    int lane = Mask[i];

    if (lane >= 0)
      MaskV0.push_back(lane - MaskSize);
    else
      MaskV0.push_back(lane);
  }

  return assignTransformAction(LowerSwapVectors, 0, MaskV0, V1, V1);
}

bool ShuffleAnalysis::analyseV0V1Mix() {
  // Elements come from v0 then v1 or the other way around, ex: 2 1 7 4.
  // This will be lowered as a concatenation, which can deal with byte-size
  // lanes.
  EVT VT = VecVT.getScalarType();
  unsigned LaneWidth = VT.getSizeInBits();
  unsigned MaskSize = Mask.size();

  // FIXME: This should be lower priority than 128bit shift for myriad2.3
  if (IsSubvectorInsert) {
    Action = LowerSubvectorInsert;
    DEBUG(dbgs() << "Transform action: " << getActionName(Action) << "\n");
    return true;
  }

  if (UseVILV && InterleaveType != NoInterleave)
    return assignLeafAction(LowerInterleaveVectors, 1);
  else if ((MinIdxV0 == 0) && (FirstDefV0 == 0 || FirstDefV0 == 1) &&
           (IsLinear2 || IsLinear2WithWrapAround))
    return assignLeafAction(LowerDeinterleaveVectors, 1);
  else if (V0BeforeV1 || V1BeforeV0) {
    // Sort out the left and right vectors.
    SmallVector<int, 16> &LHSMask = V0BeforeV1 ? MaskV0 : MaskV1;
    SmallVector<int, 16> &RHSMask = V0BeforeV1 ? MaskV1 : MaskV0;

    ConcatOffset = (RHSMask.size() * (LaneWidth / 8));

    if (NumUndef > 0) {
      // Undefined values force us to rebuild the LHS and RHS masks.
      int ConcatIndex = V0BeforeV1 ? MaxIdxV0 + 1 : MaxIdxV1 + 1;

      LHSMask.clear();

      for (int i = 0; i < ConcatIndex; i++) {
        int lane = Mask[i];

        if (lane < 0)
          LHSMask.push_back(-1);
        else if(lane < (int)MaskSize) {
          if (!V0BeforeV1)
            llvm_unreachable("RHS lane found before ConcatOffset");

          LHSMask.push_back(lane);
        } else {
          if (!V1BeforeV0)
            llvm_unreachable("RHS lane found before ConcatOffset");

          LHSMask.push_back(lane - (int)MaskSize);
        }
      }

      RHSMask.clear();

      for (int i = ConcatIndex; i < (int)MaskSize; i++) {
        int lane = Mask[i];

        if(lane < 0)
          RHSMask.push_back(-1);
        else if(lane < (int)MaskSize) {
          if (V0BeforeV1)
            llvm_unreachable("LHS lane found after ConcatOffset");

          RHSMask.push_back(lane);
        } else {
          if (V1BeforeV0)
            llvm_unreachable("LHS lane found after ConcatOffset");

          RHSMask.push_back(lane - (int)MaskSize);
        }
      }

      // Recompute the concatenation offset with the new masks.
      ConcatOffset = (RHSMask.size() * (LaneWidth / 8));
    }

    // Pad the mask element lists with undefined values. This is done either
    // at the end of the list or beginning, depending on the order of mask elements.
    while(LHSMask.size() < MaskSize)
      LHSMask.insert(LHSMask.begin(), -1);

    while(RHSMask.size() < MaskSize)
      RHSMask.insert(RHSMask.end(), -1);

    // Swizzle the source vectors if needed.
    ShuffleAnalysis LHSSA(LHSMask, VecVT, V0, V0, SHAVEST);
    ShuffleAnalysis RHSSA(RHSMask, VecVT, V1, V1, SHAVEST);

    if (LHSSA.analyse() && RHSSA.analyse()) {
      // Try the 128bit shift patterns
      if (VecVT.is128BitVector()) {
        if (RHSSA.IsShiftCandidate && isKnownZero(V0)) {
          Action = LowerShrVector;
          Cost += 1;
          ShiftOffset = 16 - ConcatOffset;
         return assignLeafAction(LowerShrVector, 1);
        }else if (LHSSA.IsShiftCandidate && isKnownZero(V1)) {
           Action = LowerShlVector;
           Cost += 1;
           ShiftOffset = ConcatOffset;
           return assignLeafAction(LowerShlVector, 1);;
        }
      }
      Status = TransformMask;
      Action = LowerConcatVectors;
      Cost += 1; // For the vector concatenation.
      Cost += LHSSA.Cost;
      Cost += RHSSA.Cost;
      DEBUG(dbgs() << "Transform action: " << getActionName(Action) << "\n");
      return true;
    }
  }

  return false;
}

unsigned ShuffleAnalysis::getTruncOpcode(EVT &TruncVT) const {
  EVT VT = VecVT.getVectorElementType();
  unsigned Opc = 0;

  TruncVT = MVT::Other;

  if ((FirstDefV0 - MinIdxV0) == 0 && VecVT.getSizeInBits() == 128) {
    if ((VT == MVT::i8) && (IsLinear2 || IsLinear2WithWrapAround)) {
      Opc = SHAVE::CMU_CPVV_v8u16_v8u8_conv;
      TruncVT = MVT::v8i8;
    } else if ((VT == MVT::i8) && (IsLinear4 || IsLinear4WithWrapAround)) {
      Opc = SHAVE::CMU_CPVV_v4u32_v4u8_conv;
      TruncVT = MVT::v4i8;
    } else if ((VT == MVT::i16) && (IsLinear2 || IsLinear2WithWrapAround)) {
      Opc = SHAVE::CMU_CPVV_v4u32_v4u16_conv;
      TruncVT = MVT::v4i16;
    }
  }

  return Opc;
}
