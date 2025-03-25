//===-- SHAVEISelDAGtoDAG.cpp - Instruction Selection -----------*- C++ -*-===//
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
// SHAVE Instruction selector and custom instruction builder
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "shave-isel"

#include <sstream>

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsShave.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/raw_ostream.h"

#include "MCTargetDesc/SHAVEMCTargetDesc.h"
#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVE.h"
#include "SHAVEISelDAGtoDAG.h"
#include "SHAVELowering.h"
#include "SHAVETargetMachine.h"
#include "SHAVEMachineFunctionInfo.h"

using namespace llvm;
using namespace SHAVEOptions;


FunctionPass *llvm::createSHAVEISelDAGtoDAG(SHAVETargetMachine &TM) {
  return new SHAVEISelDAGtoDAG(TM);
}

char SHAVEISelDAGtoDAG::ID;

SHAVEISelDAGtoDAG::SHAVEISelDAGtoDAG(SHAVETargetMachine &TM)
: SelectionDAGISel(ID, TM, TM.getOptLevel()),
  Subtarget(*TM.getSubtargetImpl())
{}

SHAVEISelDAGtoDAG::~SHAVEISelDAGtoDAG()
{}

void SHAVEISelDAGtoDAG::finaliseOperands(SmallVectorImpl<SDValue> &Ops, SDLoc dbgLoc,
                                         unsigned Opc, SDValue Chain) const {
  // Add default predication operands for predicable instructions.
  const SHAVEInstrInfo *SII = Subtarget.getInstrInfo();

  assert(Opc && "Unexpected Phi node in SHAVE DAG to DAG");

  if (SII->get(Opc).isPredicable()) {
    Ops.push_back(CurDAG->getTargetConstant(SHAVECC::AL, dbgLoc, MVT::i8));
    Ops.push_back(CurDAG->getRegister(0, MVT::i8));
  }

  // Add functional unit ID for LSU and CMU instructions.
  if (SHAVEConflicts::check_usesLSU1(Opc) && !SHAVEConflicts::check_isPseudoInstr(Opc))
    Ops.push_back(CurDAG->getTargetConstant(SHAVE::LSU0, dbgLoc, MVT::i8));
  else if (SHAVEConflicts::check_usesCMU(Opc) && !SHAVEConflicts::check_isPseudoInstr(Opc))
    Ops.push_back(CurDAG->getTargetConstant(SHAVE::CMU, dbgLoc, MVT::i8));

  // Add optional chain.
  if (Chain.getNode())
    Ops.push_back(Chain);
}

MachineSDNode *SHAVEISelDAGtoDAG::getMachineNodeWithDefaultOps(unsigned Opc, SDLoc dbgLoc, EVT VT,
                                                               SmallVectorImpl<SDValue> &Ops,
                                                               SDValue Chain) const {
  finaliseOperands(Ops, dbgLoc, Opc, Chain);
  return CurDAG->getMachineNode(Opc, dbgLoc, VT, Ops);
}

MachineSDNode *SHAVEISelDAGtoDAG::getMachineNodeWithDefaultOps(unsigned Opc, SDLoc dbgLoc,
                                                               SDVTList VTList,
                                                               SmallVectorImpl<SDValue> &Ops,
                                                               SDValue Chain) const {
  finaliseOperands(Ops, dbgLoc, Opc, Chain);
  return CurDAG->getMachineNode(Opc, dbgLoc, VTList, Ops);
}

 void SHAVEISelDAGtoDAG::SelectNodeWithDefaultOps(SDNode *N, unsigned Opc, EVT VT,
                                                    SmallVectorImpl<SDValue> &Ops,
                                                    SDValue Chain) const {
  finaliseOperands(Ops, SDLoc(N), Opc, Chain);
  CurDAG->SelectNodeTo(N, Opc, VT, Ops);
}

void SHAVEISelDAGtoDAG::SelectNodeWithDefaultOps(SDNode *N, unsigned Opc,
                                                    SDVTList VTList,
                                                    SmallVectorImpl<SDValue> &Ops,
                                                    SDValue Chain) const {
  finaliseOperands(Ops, SDLoc(N), Opc, Chain);
  CurDAG->SelectNodeTo(N, Opc, VTList, Ops);
}


namespace {
  SDNode *getLDImm64Node(SelectionDAG *DAG, SDLoc dbgLoc, uint64_t imm) {
    uint64_t imm_h = (imm & 0xFFFFFFFF00000000ull) >> 32;
    SDValue immHVal = DAG->getTargetConstant(imm_h, dbgLoc, MVT::i32);
    SDNode *loadH = DAG->getMachineNode(SHAVE::LDImm32, dbgLoc, MVT::i32, MVT::Glue, immHVal);

    uint64_t imm_l = imm & 0x00000000FFFFFFFFull;
    SDValue immLVal = DAG->getTargetConstant(imm_l, dbgLoc, MVT::i32);
    SDNode *loadL = DAG->getMachineNode(SHAVE::LDImm32, dbgLoc, MVT::i32, MVT::Glue, immLVal);

    SDNode *def = DAG->getMachineNode(SHAVE::IMPLICIT_DEF, dbgLoc, DAG->getVTList(MVT::i64, MVT::Glue), std::nullopt);
    SDNode *insertHigh = DAG->getMachineNode(SHAVE::INSERT_SUBREG, dbgLoc, MVT::i64, MVT::Glue, SDValue(def, 0), SDValue(loadH, 0), DAG->getTargetConstant(SHAVE::vsub32_1, dbgLoc, MVT::i32));
    SDNode *result = DAG->getMachineNode(SHAVE::INSERT_SUBREG, dbgLoc, MVT::i64, MVT::Glue, SDValue(insertHigh, 0), SDValue(loadL, 0), DAG->getTargetConstant(SHAVE::vsub32_0, dbgLoc, MVT::i32));

    return result;
  }

  SDNode *getLDImm32Node(SelectionDAG *DAG, EVT VT, SDLoc dbgLoc, uint64_t Imm) {
    EVT NativeVT = VT.isFloatingPoint() ? MVT::f32 : MVT::i32;

    if (VT.bitsLE(MVT::i16))
      Imm &= 0x0000FFFFu;
    else if (VT.bitsLE(MVT::i32))
      Imm &= 0xFFFFFFFFu;

    SDValue ImmVal = DAG->getTargetConstant(Imm, dbgLoc, MVT::i32);
    SDNode *Load = DAG->getMachineNode(SHAVE::LDImm32, dbgLoc, NativeVT, ImmVal);

    if (VT != NativeVT) {
      unsigned SubRegIdx = 0;

      switch (VT.getSimpleVT().SimpleTy) {
      default:
        llvm_unreachable("This type of constant is not supported.");
        break;
      case MVT::i8:
        SubRegIdx = SHAVE::qsub_0;
        break;
      case MVT::i16:
      case MVT::f16:
        SubRegIdx = SHAVE::hsub_0;
        break;
      }

      Load = DAG->getTargetExtractSubreg(SubRegIdx, dbgLoc, VT, SDValue(Load, 0)).getNode();
    }

    return Load;
  }
} // End of anonymous namespace


void SHAVEISelDAGtoDAG::SelectImmediateLoadFP(SDNode *N) {
  ConstantFPSDNode *imm = cast<ConstantFPSDNode>(N);
  APFloat Val = imm->getValueAPF();
  APInt ValAsInt = Val.bitcastToAPInt();

  SDNode * result = getLDImm32Node(CurDAG, imm->getValueType(0), SDLoc(N),
                                   ValAsInt.getZExtValue());

  ReplaceNode(N, result);
}

void SHAVEISelDAGtoDAG::SelectImmediateLoad(SDNode *N) {
  ConstantSDNode *imm = cast<ConstantSDNode>(N);
  SDNode * result = nullptr;

  if (imm->getValueType(0).getSizeInBits() == 64)
    result = getLDImm64Node(CurDAG, SDLoc(N), imm->getZExtValue());
  else
    result = getLDImm32Node(CurDAG, imm->getValueType(0), SDLoc(N), imm->getZExtValue());

  ReplaceNode(N, result);
}

bool SHAVEISelDAGtoDAG::SelectADDRriImpl(SDValue &Addr, SDValue &Base, SDValue &Offset, unsigned int shift) {
  SDLoc dbgLoc(Addr.getNode());

  // FIXME: Movidius - why can we not select for non-frame-indices which are of the form 'reg + immediate'?
  //        Why not any indexed memory?
  if (FrameIndexSDNode *Fin = dyn_cast<FrameIndexSDNode>(Addr)) {
    // The offset must fit in the N-bit signed offset field of LDO or STO
    if (isIntN(Subtarget.getLDOSTO_OffsetBits(), Fin->getIndex())) {
      Base = CurDAG->getTargetFrameIndex(Fin->getIndex(), MVT::i32);
      Offset = CurDAG->getTargetConstant(0, dbgLoc, MVT::i32);

      return true;
    }
  }

  if (shift && Addr->getOpcode() == ISD::ADD && isa<ConstantSDNode>(Addr->getOperand(1))) {
    int64_t index = cast<ConstantSDNode>(Addr->getOperand(1))->getSExtValue();
    
    if (index >= -4096 && index <= 4064 && (index & ~(((uint64_t) -1) << shift)) == 0) {
      Base = Addr->getOperand(0);
      // The compiler does not do the shifting since it is handled by the assembler.
      // Output the original immediate value here
      Offset = CurDAG->getTargetConstant(index, dbgLoc, MVT::i32);

      return true;
    }
  }

  return false;
}

bool SHAVEISelDAGtoDAG::SelectADDRri(SDValue &Addr, SDValue &Base, SDValue &Offset) {
  return SelectADDRriImpl(Addr, Base, Offset);
}

bool SHAVEISelDAGtoDAG::SelectADDRri5(SDValue &Addr, SDValue &Base, SDValue &Offset) {
  return SelectADDRriImpl(Addr, Base, Offset, 5u);
}

bool SHAVEISelDAGtoDAG::SelectADDRri4(SDValue &Addr, SDValue &Base, SDValue &Offset) {
  return SelectADDRriImpl(Addr, Base, Offset, 4u);
}

bool SHAVEISelDAGtoDAG::SelectADDRri3(SDValue &Addr, SDValue &Base, SDValue &Offset) {
  return SelectADDRriImpl(Addr, Base, Offset, 3u);
}

bool SHAVEISelDAGtoDAG::SelectADDRri2(SDValue &Addr, SDValue &Base, SDValue &Offset) {
  return SelectADDRriImpl(Addr, Base, Offset, 2u);
}

bool SHAVEISelDAGtoDAG::SelectADDRrr(SDValue &Addr, SDValue &Base, SDValue &Offset) {
  if (Addr.getOpcode() == ISD::ADD) {
    const SDValue &lhs = Addr.getOperand(0);
    const SDValue &rhs = Addr.getOperand(1);

    if (EnableLDXandSTXInstructions) {
      // Don't bother loading constants into a register first. At best we get the same number of instructions
      if (rhs.getOpcode() == ISD::Constant)
        return false;

      Base = lhs;
      Offset = rhs;

      return true;
    }
  }

  return false;
}

bool SHAVEISelDAGtoDAG::SelectADDRr(SDValue &Addr, SDValue &Base) {
  if(FrameIndexSDNode *Fin = dyn_cast<FrameIndexSDNode>(Addr)) {
    // just pass the stack operation further to eliminateFrameIndex method
    // where it will be handled
    Base = CurDAG->getTargetFrameIndex(Fin->getIndex(), MVT::i32);
    return true;
  } else if(Addr.getOpcode() == ISD::ADD) {
    // gelementptr load/store
    Base = Addr;
    return true;
  } else if(Addr.getOpcode() == ISD::TargetGlobalAddress) {
    Base = CurDAG->getNode(SHAVEISD::LDISym, SDLoc(Addr), Addr.getValueType(), Addr);
    return true;
  } else if(Addr.getOpcode() == SHAVEISD::LDISym) {
    Base = Addr;
    return true;
  } else if(Addr.getValueType() == MVT::i32 || Addr.getValueType() == MVT::iPTR) {
    // in fact we can use any i32 as address to load from
    Base = Addr;
    return true;
  }

  return false;
}

namespace {
  unsigned getVectorBytes(EVT VT) {
    if (VT.isVector())
      return VT.getSizeInBits() / 8;

    return 0;
  }

  int64_t ExtractMemoryOffset(SDValue &Base, int64_t CurrentOffset = 0) {
    unsigned BaseOpc = Base.getOpcode();

    if ((BaseOpc != ISD::ADD) && (BaseOpc != ISD::SUB))
      return CurrentOffset;

    SDValue BaseLHS = Base.getOperand(0);
    ConstantSDNode *BaseRHS = dyn_cast<ConstantSDNode>(Base.getOperand(1).getNode());

    if (!BaseRHS)
      return CurrentOffset;

    const int64_t MaxOffset = 0x3FFF;

    int64_t BaseOffset = BaseRHS->getSExtValue();

    BaseOffset = (BaseOpc == ISD::SUB) ? -BaseOffset : BaseOffset;

    int64_t NewOffset = CurrentOffset + BaseOffset;

    if ((NewOffset > MaxOffset) || (NewOffset < -MaxOffset))
      return CurrentOffset;

    // Try to extract another offset with the new base.
    Base = BaseLHS;
    return ExtractMemoryOffset(Base, NewOffset);
  }
} // End of anonymous namespace

void SHAVEISelDAGtoDAG::SelectCLAMP(SDNode *N) {
  SDLoc dbgLoc(N);
  EVT VT = N->getValueType(0);
  SDValue Src = N->getOperand(0);
  SDValue Low = N->getOperand(1);
  SDValue High = N->getOperand(2);
  ConstantSDNode *IsUint = cast<ConstantSDNode>(N->getOperand(3));
  unsigned ClampOpc = 0;

  // Try to select CLAMP(0), which only operate on signed types.
  if (IsUint->getZExtValue() == 0) {
    bool LowIsSplat = (Low.getOpcode() == SHAVEISD::SPLAT);
    bool HighIsSplat = (Low.getOpcode() == SHAVEISD::SPLAT);
    bool LowIsZero = false;
    bool LowIsMinusHigh = false;

    // Try to extract constants from the low and high operands.
    ConstantSDNode *LowVal = dyn_cast<ConstantSDNode>(Low);
    ConstantFPSDNode *LowValFP = dyn_cast<ConstantFPSDNode>(Low);
    ConstantSDNode *HighVal = dyn_cast<ConstantSDNode>(High);
    ConstantFPSDNode *HighValFP = dyn_cast<ConstantFPSDNode>(High);

    if (LowIsSplat) {
      LowVal = dyn_cast<ConstantSDNode>(Low.getOperand(0));
      LowValFP = dyn_cast<ConstantFPSDNode>(Low.getOperand(0));
    }

    if (HighIsSplat) {
      HighVal = dyn_cast<ConstantSDNode>(High.getOperand(0));
      HighValFP = dyn_cast<ConstantFPSDNode>(High.getOperand(0));
    }

    // Try to match the '0, max' pattern.
    if (LowVal && (LowVal->getSExtValue() == 0))
      LowIsZero = true;

    if (LowValFP && LowValFP->isZero())
      LowIsZero = true;

    // Try to match the '-x, x' pattern (where x is constant).
    if (HighVal && LowVal)
      LowIsMinusHigh = (-HighVal->getSExtValue() == LowVal->getSExtValue());
    else if (HighValFP && LowValFP) {
      APFloat LowAPF = LowValFP->getValueAPF();
      APFloat HighAPF = HighValFP->getValueAPF();

      LowAPF.changeSign();
      LowIsMinusHigh = (LowAPF.compare(HighAPF) == APFloat::cmpEqual);
    }

    // Try to match the '-x, x' pattern (where x is not constant).
    if (!LowIsMinusHigh && (Low.getOpcode() == ISD::FNEG)) {
      LowIsMinusHigh = (Low.getOperand(0) == High);
    }

    // Now try to select these variants.
    if (LowIsZero || LowIsMinusHigh) {
      switch (VT.getSimpleVT().SimpleTy) {
      default:
        ClampOpc = 0;
        break;
      // 128-bit vectors
      case MVT::v4f32:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_f32_v4f32 : SHAVE::CMU_CLAMP_f32_v4f32;
        break;
      case MVT::v8f16:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_f16_v8f16 : SHAVE::CMU_CLAMP_f16_v8f16;
        break;
      case MVT::v4i32:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i32_v4i32 : SHAVE::CMU_CLAMP_i32_v4i32;
        break;
      case MVT::v8i16:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i16_v8i16 : SHAVE::CMU_CLAMP_i16_v8i16;
        break;
      case MVT::v16i8:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i8_v16i8 : SHAVE::CMU_CLAMP_i8_v16i8;
        break;
      // 64-bit vectors
      case MVT::v2f32:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_f32_v2f32 : SHAVE::CMU_CLAMP_f32_v2f32;
        break;
      case MVT::v4f16:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_f16_v4f16 : SHAVE::CMU_CLAMP_f16_v4f16;
        break;
      case MVT::v2i32:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i32_v2i32 : SHAVE::CMU_CLAMP_i32_v2i32;
        break;
      case MVT::v4i16:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i16_v4i16 : SHAVE::CMU_CLAMP_i16_v4i16;
        break;
      case MVT::v8i8:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i8_v8i8 : SHAVE::CMU_CLAMP_i8_v8i8;
        break;
      // 32-bit vectors
      case MVT::v2f16:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_f16_v2f16 : SHAVE::CMU_CLAMP_f16_v2f16;
        break;
      case MVT::v2i16:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i16_v2i16 : SHAVE::CMU_CLAMP_i16_v2i16;
        break;
      case MVT::v4i8:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i8_v4i8 : SHAVE::CMU_CLAMP_i8_v4i8;
        break;
      // 16-bit vectors
      case MVT::v2i8:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i8_v2i8 : SHAVE::CMU_CLAMP_i8_v2i8;
        break;
      // Scalars
      case MVT::f32:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_f32 : SHAVE::CMU_CLAMP_f32;
        break;
      case MVT::f16:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_f16 : SHAVE::CMU_CLAMP_f16;
        break;
      case MVT::i32:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i32 : SHAVE::CMU_CLAMP_i32;
        break;
      case MVT::i16:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i16 : SHAVE::CMU_CLAMP_i16;
        break;
      case MVT::i8:
        ClampOpc = LowIsZero ? SHAVE::CMU_CLAMP0_i8 : SHAVE::CMU_CLAMP_i8;
        break;
      }

      if (ClampOpc) {
        SmallVector<SDValue, 4> Ops;

        Ops.push_back(Src);
        Ops.push_back(High);

        SDNode * result = getMachineNodeWithDefaultOps(ClampOpc, dbgLoc, VT, Ops);
        ReplaceNode(N, result);
        return;
      }
    }
  }

  SelectCode(N);
}

void SHAVEISelDAGtoDAG::SelectScalarLOAD(SDNode *N) {
  SDLoc dbgLoc(N);
  LoadSDNode *Load = cast<LoadSDNode>(N);
  SDValue Chain = N->getOperand(0);
  SDValue Base = N->getOperand(1);
  MachineMemOperand *MMO = Load->getMemOperand();
  unsigned BaseOpc = Base.getOpcode();

  if ((BaseOpc != ISD::ADD) && (BaseOpc != ISD::SUB)) {
    SelectCode(N);
    return;
  }

  int64_t BaseOffset = ExtractMemoryOffset(Base);

  if (!BaseOffset || (Subtarget.getLDOSTO_OffsetBits() == 13u && (BaseOffset <= -4096 || BaseOffset > 4095))) {
    SelectCode(N);
    return;
  }

  if (Load->getExtensionType() != ISD::NON_EXTLOAD) {
    SelectCode(N);
    return;
  }

  {
    // FIXME: Movidius - adapt this code to use a register if the range of the immediate exceeds the range
    //        of a signed 15-bit integer: save the immediate to a register and use 'LDX'.
    int64_t offsetFromBase = 0;

    // Determine the instruction needed to perform the load.
    FrameIndexSDNode *fi = dyn_cast<FrameIndexSDNode>(Base.getNode());

    if (fi)
      offsetFromBase = fi->getIndex();
    else
      offsetFromBase = ExtractMemoryOffset(Base);

    if (!isIntN(Subtarget.getLDOSTO_OffsetBits(), offsetFromBase))
      llvm_unreachable("Offset to LDO exceeds range supported by platform");
  }

  // Determine the load opcode.
  MVT VT = N->getValueType(0).getSimpleVT();
  unsigned Opc = 0;

  switch (VT.SimpleTy) {
  default:
    SelectCode(N);
    return;
  case MVT::i8:
    Opc = SHAVE::LSU_LDO_i8;
    break;
  case MVT::i16:
    Opc = SHAVE::LSU_LDO_i16;
    break;
  case MVT::i32:
    Opc = SHAVE::LSU_LDO_i32;
    break;
  case MVT::f16:
    Opc = SHAVE::LSU_LDO_f16;
    break;
  case MVT::f32:
    Opc = SHAVE::LSU_LDO_f32;
    break;
  }

  // Create the load machine node.
  SmallVector<SDValue, 8> Ops;

  if (FrameIndexSDNode *FINode = dyn_cast<FrameIndexSDNode>(Base))
    Base = CurDAG->getTargetFrameIndex(FINode->getIndex(), Base.getValueType());

  Ops.push_back(Base);
  Ops.push_back(CurDAG->getTargetConstant(BaseOffset, dbgLoc, MVT::i32));

  MachineSDNode *NewLoad = getMachineNodeWithDefaultOps(Opc, dbgLoc, N->getVTList(),
                                                        Ops, Chain);

  // Set the memory operand.
  CurDAG->setNodeMemRefs(NewLoad, {MMO});

  ReplaceNode(N, NewLoad);
}

void SHAVEISelDAGtoDAG::SelectScalarSTORE(SDNode *N) {
  SDLoc dbgLoc(N);
  StoreSDNode *Store = cast<StoreSDNode>(N);
  SDValue Chain = N->getOperand(0);
  SDValue Val = N->getOperand(1);
  SDValue Base = N->getOperand(2);
  MachineMemOperand *MMO = Store->getMemOperand();
  unsigned BaseOpc = Base.getOpcode();

  if ((BaseOpc != ISD::ADD) && (BaseOpc != ISD::SUB)) {
    SelectCode(N);
    return;
  }

  int64_t BaseOffset = ExtractMemoryOffset(Base);

  if (!BaseOffset || (Subtarget.getLDOSTO_OffsetBits() == 13u && (BaseOffset <= -4096 || BaseOffset > 4095))) {
    SelectCode(N);
    return;
  }

  if (Store->isTruncatingStore()) {
    SelectCode(N);
    return;
  }

  {
    // FIXME: Movidius - adapt this code to use a register if the range of the immediate exceeds the range
    //        of a signed 15-bit integer: save the immediate to a register and use 'STX'.
    int64_t offsetFromBase = 0;

    // Determine the instruction needed to perform the load.
    FrameIndexSDNode *fi = dyn_cast<FrameIndexSDNode>(Base.getNode());

    if (fi)
      offsetFromBase = fi->getIndex();
    else
      offsetFromBase = ExtractMemoryOffset(Base);

    if (!isIntN(Subtarget.getLDOSTO_OffsetBits(), offsetFromBase))
      llvm_unreachable("Offset to STO exceeds range supported by platform");
  }

  // Determine the store opcode.
  MVT VT = Val.getValueType().getSimpleVT();
  unsigned Opc = 0;

  switch (VT.SimpleTy) {
  default:
    SelectCode(N);
    return;
  case MVT::i8:
    Opc = SHAVE::LSU_STO_i8;
    break;
  case MVT::i16:
    Opc = SHAVE::LSU_STO_i16;
    break;
  case MVT::i32:
    Opc = SHAVE::LSU_STO_i32;
    break;
  case MVT::f16:
    Opc = SHAVE::LSU_STO_f16;
    break;
  case MVT::f32:
    Opc = SHAVE::LSU_STO_f32;
    break;
  }

  // Create the store machine node.
  SmallVector<SDValue, 8> Ops;

  Ops.push_back(Val);

  if (FrameIndexSDNode *FINode = dyn_cast<FrameIndexSDNode>(Base))
    Base = CurDAG->getTargetFrameIndex(FINode->getIndex(), Base.getValueType());

  Ops.push_back(Base);
  Ops.push_back(CurDAG->getTargetConstant(BaseOffset, dbgLoc, MVT::i32));

  MachineSDNode *NewStore = getMachineNodeWithDefaultOps(Opc, dbgLoc, MVT::Other,
                                                         Ops, Chain);

  // Set the memory operand.
  CurDAG->setNodeMemRefs(NewStore, {MMO});

  ReplaceNode(N, NewStore);
}

void SHAVEISelDAGtoDAG::SelectLOAD64_LOW_HIGH(SDNode *N) {
  SDLoc dbgLoc(N);
  SDValue chain = N->getOperand(0);
  SDValue baseAddress = N->getOperand(1);
  bool isHigh = N->getOpcode() == SHAVEISD::LOAD64_HIGH;

  FrameIndexSDNode *frameIndex = dyn_cast<FrameIndexSDNode>(baseAddress.getNode());
  int64_t baseOffset = 0;
  bool useOffset = false;

  if (frameIndex) {
    baseAddress = CurDAG->getTargetFrameIndex(frameIndex->getIndex(), baseAddress.getValueType());
    useOffset = true;
    if (!isInt<15u>(frameIndex->getIndex()))
      llvm_unreachable("Offset to LDO exceeds range of a signed 15-bit integer");
  }
  else {
    baseOffset = ExtractMemoryOffset(baseAddress);
    useOffset = (baseOffset != 0);
    if (!isInt<15u>(baseOffset))
      llvm_unreachable("Offset to LDO exceeds range of a signed 15-bit integer");
  }

  SDVTList typeList = CurDAG->getVTList(N->getValueType(0), MVT::Other);
  
  SmallVector<SDValue, 3> operands;
  if (isHigh)
    operands.push_back(N->getOperand(2));
  operands.push_back(baseAddress);
  if (useOffset)
    operands.push_back(CurDAG->getTargetConstant(baseOffset, dbgLoc, MVT::i32));

  unsigned int opcode;
  if (isHigh)
    opcode = useOffset ? SHAVE::LOAD64_HIGH_Imm : SHAVE::LOAD64_HIGH;
  else
    opcode = useOffset ? SHAVE::LOAD64_LOW_Imm : SHAVE::LOAD64_LOW;

  MachineSDNode * load = getMachineNodeWithDefaultOps(opcode, dbgLoc, typeList, operands, chain);

  CurDAG->setNodeMemRefs(load, {cast<MemSDNode>(N)->getMemOperand()});

  ReplaceNode(N, load);
}

// FIXME: Movidius - TODO: try to make all vector types legal. is there any problem with that?
// if not then where possible do conversions like LD64.l + CPVV.i16.i32 -> LD128.i16.i32!!!!!!
void SHAVEISelDAGtoDAG::SelectVectorLOAD(SDNode *N) {
  LoadSDNode *loadNode = dyn_cast<LoadSDNode>(N);

  if (!loadNode)
    llvm_unreachable("Not a load node.");

  SDLoc dbgLoc(N);
  SDValue chain = N->getOperand(0);
  SDValue base = N->getOperand(1);
  EVT memoryVT = loadNode->getMemoryVT();
  unsigned memBytes = getVectorBytes(memoryVT);
  EVT resultVT = N->getValueType(0);
  unsigned resultBytes = getVectorBytes(resultVT);
  MachineMemOperand *MMO = loadNode->getMemOperand();
  ISD::LoadExtType ExtTy = loadNode->getExtensionType();
  bool withExt = (ExtTy != ISD::NON_EXTLOAD);

  if (!memBytes || !resultBytes)
    llvm_unreachable("The source or memory types are not vector types.");
  else if (!withExt && (memBytes != resultBytes))
    llvm_unreachable("Source and memory type mismatch for non-extending load.");
  else if (withExt && (memBytes >= resultBytes))
    llvm_unreachable("Source and memory size mismatch for extending load.");
  else if ((memBytes > 64) || (resultBytes > 64))
    llvm_unreachable("Cannot store more than 16 words (512 bits).");

  // Sign- and zero-extending vector loads can simply be selected by tablegen.
  if (withExt) {
    SelectCode(N);
    return;
  }

  if (hasFeature(SHAVE::HasVRF512_Feature) && memBytes < 64) {
    SelectCode(N);
    return;
  }

  // Determine the instruction needed to perform the load.
  FrameIndexSDNode *fi = dyn_cast<FrameIndexSDNode>(base.getNode());
  int64_t BaseOffset = 0;
  bool UseOffset = false;
  unsigned LoadOpc = 0;

  // FIXME: Movidius - adapt this code to use a register if the range of the immediate exceeds the range
  //        of a signed 15-bit integer: save the immediate to a register and use 'LDX'.
  if (fi) {
    base = CurDAG->getTargetFrameIndex(fi->getIndex(), base.getValueType());
    UseOffset = true;
    if (!isInt<15u>(fi->getIndex())) llvm_unreachable("Offset to LDO exceeds range of a signed 15-bit integer");
//  assert(isInt<15u>(fi->getIndex()) && "Offset to LDO exceeds range of a signed 15-bit integer");
  } else {
    BaseOffset = ExtractMemoryOffset(base);
    UseOffset = (BaseOffset != 0);
    if (!isInt<15u>(BaseOffset)) llvm_unreachable("Offset to LDO exceeds range of a signed 15-bit integer");
//  assert(isInt<15u>(BaseOffset) && "Offset to LDO exceeds range of a signed 15-bit integer");
  }

  switch (memBytes) {
  default:
    llvm_unreachable("Load size not supported.");
  case 64:
    assert(hasFeature(SHAVE::HasVRF512_Feature) && "512-bit vectors not supported on Myriad2");
    LoadOpc = UseOffset ? SHAVE::LDOV512 : SHAVE::LDV512;
    break;
  case 16:
    LoadOpc = UseOffset ? SHAVE::LDOV128 : SHAVE::LDV128;
    break;
  case 8:
    LoadOpc = UseOffset ? SHAVE::LSU_LDO64_l_Raw : SHAVE::LSU_LD64_l_Raw;
    break;
  case 4:
    // FIXME: Movidius - Use tablegen for this case
    {
      MVT VT = N->getValueType(0).getSimpleVT();

      switch (VT.SimpleTy) {
      case MVT::v2f16:  // FIXME: Movidius - Need to adapt for true small-vectors
        LoadOpc = UseOffset ? SHAVE::LSU_LDO_v2f16 : SHAVE::LSU_LD_v2f16;
        break;
      case MVT::v4i8:  // FIXME: Movidius - Need to adapt for true small-vectors
        LoadOpc = UseOffset ? SHAVE::LSU_LDO_v4i8 : SHAVE::LSU_LD_v4i8;
        break;
      case MVT::v2i16:  // FIXME: Movidius - Need to adapt for true small-vectors
        LoadOpc = UseOffset ? SHAVE::LSU_LDO_v2i16 : SHAVE::LSU_LD_v2i16;
        break;
      default:
        LoadOpc = UseOffset ? SHAVE::LSU_LDO_i32 : SHAVE::LSU_LD_i32;
        break;
      }
    }
    break;
  case 2:
    LoadOpc = UseOffset ? SHAVE::LSU_LDO_i16 : SHAVE::LSU_LD_i16;
    break;
  }

  // Load the the vector.
  SDVTList typeList = CurDAG->getVTList(memoryVT, MVT::Other);
  MachineSDNode *Load = nullptr;
  SmallVector<SDValue, 6> Ops;

  if (UseOffset) {
    // we're loading from a frame location or pointer + known offset
    // use the addressing mode [Reg + ImmOffset]
    Ops.push_back(base);
    Ops.push_back(CurDAG->getTargetConstant(BaseOffset, dbgLoc, MVT::i32));
  } else {
    // we're loading from an unknown location
    // use the addressing mode [Reg]
    Ops.push_back(base);
  }

  // FIXME: Movidius - need to verify the 'size'
  Load = getMachineNodeWithDefaultOps(LoadOpc, dbgLoc, typeList, Ops, chain);

  CurDAG->setNodeMemRefs(Load,{MMO});
  ReplaceNode(N, Load);
}

unsigned SHAVEISelDAGtoDAG::SelectCompareZero(SDNode *N, SDValue &LHS,
                                              SDValue RHS) {
  // Try to match integer comparisons against zero.
  MVT VT = LHS.getValueType().getSimpleVT();
  unsigned Opc = 0;
  ConstantSDNode *ConstRHS = dyn_cast<ConstantSDNode>(RHS.getNode());

  if (ConstRHS && ConstRHS->getConstantIntValue()->isZero()) {
    switch (VT.SimpleTy) {
    case MVT::i8:
      Opc = SHAVE::CMU_CMZ_i8;
      break;
    case MVT::i16:
      Opc = SHAVE::CMU_CMZ_i16;
      break;
    case MVT::i32:
      Opc = SHAVE::CMU_CMZ_i32;
      break;
    default:
      return 0;
    }
    return Opc;
  }

  // Try to match floating-point comparisons against zero.
  ConstantFPSDNode *ConstFPRHS = dyn_cast<ConstantFPSDNode>(RHS.getNode());

  if (ConstFPRHS && ConstFPRHS->isZero()) {
    // the comparison is against zero
    switch (VT.SimpleTy) {
    case MVT::f16:
      Opc = SHAVE::CMU_CMZ_f16;
      break;
    case MVT::f32:
      Opc = SHAVE::CMU_CMZ_f32;
      break;
    default:
      return 0;
    }

    return Opc;
  }

  bool isSplat0 = false;
  if (RHS.getOpcode() == SHAVEISD::SPLAT) {
    if (ConstantSDNode * constant = dyn_cast<ConstantSDNode>(RHS.getOperand(0)))
      isSplat0 = constant->getConstantIntValue()->isZero();

    if (ConstantFPSDNode * constantfp = dyn_cast<ConstantFPSDNode>(RHS.getOperand(0)))
      isSplat0 = constantfp->getConstantFPValue()->isZero();
  }

  if (isSplat0) {
    if (hasFeature(SHAVE::HasVRF128_Feature)) {
      switch (VT.SimpleTy) {
      case MVT::v4i32: Opc = SHAVE::CMU_CMZ_v4i32; break;
      case MVT::v8i16: Opc = SHAVE::CMU_CMZ_v8i16; break;
      case MVT::v16i8: Opc = SHAVE::CMU_CMZ_v16i8; break;
      case MVT::v4f32: Opc = SHAVE::CMU_CMZ_v4f32; break;
      case MVT::v8f16: Opc = SHAVE::CMU_CMZ_v8f16; break;
      default: return 0;
      }
      return Opc;
    }
    else {
      switch (VT.SimpleTy) {
      case MVT::v16i32: Opc = SHAVE::CMU_CMZ_v16i32; break;
      case MVT::v32i16: Opc = SHAVE::CMU_CMZ_v32i16; break;
      case MVT::v64i8:  Opc = SHAVE::CMU_CMZ_v64i8;  break;
      case MVT::v16f32: Opc = SHAVE::CMU_CMZ_v16f32; break;
      case MVT::v32f16: Opc = SHAVE::CMU_CMZ_v32f16; break;
      default: return 0;
      }
      return Opc;
    }
  }

  return 0;
}

void SHAVEISelDAGtoDAG::SelectCompare(SDNode *N) {
  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);

  // Fetch the condition code and the value type
  ISD::CondCode CC = cast<CondCodeSDNode>(N->getOperand(2))->get();
  MVT::SimpleValueType VT = LHS.getValueType().getSimpleVT().SimpleTy;
  assert(LHS.getValueType() == RHS.getValueType() && LHS.getValueType().isSimple()
         && "Compared values should have the same simple type.");

  SmallVector<SDValue, 2> ops;
  unsigned opcode = 0; //!(ISD::isUnsignedIntSetCC(CC)) ? SelectCompareZero(N, LHS, RHS) : 0;
  bool isCMZ = false;

  if (ISD::isUnsignedIntSetCC(CC) && LHS.getValueType().isInteger()) {
    // This is an unsigned-integer comparison
    switch (VT) {
      // Scalars
    case MVT::i32:    opcode = SHAVE::CMU_CMII_u32; break;
    case MVT::i16:    opcode = SHAVE::CMU_CMII_u16; break;
    case MVT::i8:     opcode = SHAVE::CMU_CMII_u8;  break;

    // 512-bit vectors
    case MVT::v16i32: opcode = SHAVE::CMU_CMVV_v16i32_unsigned_Myr4; break;
    case MVT::v32i16: opcode = SHAVE::CMU_CMVV_v32i16_unsigned_Myr4; break;
    case MVT::v64i8:  opcode = SHAVE::CMU_CMVV_v64i8_unsigned_Myr4; break;

    // 256-bit vectors
    case MVT::v8i32:  opcode = SHAVE::CMU_CMVV_v8i32_unsigned_Myr4; break;
    case MVT::v16i16: opcode = SHAVE::CMU_CMVV_v16i16_unsigned_Myr4; break;
    case MVT::v32i8:  opcode = SHAVE::CMU_CMVV_v32i8_unsigned_Myr4; break;

    // 128-bit vectors
    case MVT::v4i32:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v4i32_unsigned_Myr4 : SHAVE::CMU_CMVV_u32; break;
    case MVT::v8i16:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v8i16_unsigned_Myr4 : SHAVE::CMU_CMVV_u16; break;
    case MVT::v16i8:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v16i8_unsigned_Myr4 : SHAVE::CMU_CMVV_u8; break;

    // 64-bit vectors
    case MVT::v2i32:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v2i32_unsigned_Myr4 : SHAVE::CMU_CMVV_v2u32; break;
    case MVT::v4i16:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v4i16_unsigned_Myr4 : SHAVE::CMU_CMVV_v4u16; break;
    case MVT::v8i8:   opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v8i8_unsigned_Myr4  : SHAVE::CMU_CMVV_v8u8; break;

    // 32-bit vectors
    case MVT::v2i16:  opcode = SHAVE::CMU_CMII_v2u16; break;
    case MVT::v4i8:   opcode = SHAVE::CMU_CMII_v4u8; break;

    // 16-bit vectors
    case MVT::v2i8:  opcode = SHAVE::CMU_CMII_v2u8; break;

    default: llvm_unreachable("Unknown comparison type!");
    }
  } else {
    // This is a signed-integer or floating-point comparison
    // Check for compare with zero first
    opcode = SelectCompareZero(N, LHS, RHS);
    if (opcode)
      isCMZ = true;
    else {
      switch (VT) {
        // Scalars
      case MVT::i32:
        opcode = SHAVE::CMU_CMII_i32;
        if (RHS->getOpcode() == ISD::Constant) {
          int64_t constVal = cast<ConstantSDNode>(RHS)->getSExtValue();
          // CMPI takes only signed 16-bit immediates
          if (isInt<16u>(constVal))
            opcode = SHAVE::CMU_CMPI_Raw;
        }
        break;

      case MVT::i16:    opcode = SHAVE::CMU_CMII_i16; break;
      case MVT::i8:     opcode = SHAVE::CMU_CMII_i8;  break;
      case MVT::f32:    opcode = SHAVE::CMU_CMII_f32; break;
      case MVT::f16:    opcode =  SHAVE::CMU_CMII_f16; break;

      // 512-bit vectors
      case MVT::v16i32: opcode = SHAVE::CMU_CMVV_v16i32_signed_Myr4; break;
      case MVT::v32i16: opcode = SHAVE::CMU_CMVV_v32i16_signed_Myr4; break;
      case MVT::v64i8:  opcode = SHAVE::CMU_CMVV_v64i8_signed_Myr4; break;
      case MVT::v16f32: opcode = SHAVE::CMU_CMVV_v16f32_Myr4; break;
      case MVT::v32f16: opcode = SHAVE::CMU_CMVV_v32f16_Myr4; break;

      // 256-bit vectors
      case MVT::v8i32:  opcode = SHAVE::CMU_CMVV_v8i32_signed_Myr4; break;
      case MVT::v16i16: opcode = SHAVE::CMU_CMVV_v16i16_signed_Myr4; break;
      case MVT::v32i8:  opcode = SHAVE::CMU_CMVV_v32i8_signed_Myr4; break;
      case MVT::v8f32:  opcode = SHAVE::CMU_CMVV_v8f32_Myr4; break;
      case MVT::v16f16: opcode = SHAVE::CMU_CMVV_v16f16_Myr4; break;

      // 128-bit vectors
      case MVT::v4i32:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v4i32_signed_Myr4 : SHAVE::CMU_CMVV_i32; break;
      case MVT::v8i16:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v8i16_signed_Myr4 : SHAVE::CMU_CMVV_i16; break;
      case MVT::v16i8:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v16i8_signed_Myr4 : SHAVE::CMU_CMVV_i8; break;
      case MVT::v4f32:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v4f32_Myr4 : SHAVE::CMU_CMVV_f32; break;
      case MVT::v8f16:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v8f16_Myr4 : SHAVE::CMU_CMVV_f16; break;

      // 64-bit vectors
      case MVT::v2i32:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v2i32_signed_Myr4 : SHAVE::CMU_CMVV_v2i32; break;
      case MVT::v4i16:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v4i16_signed_Myr4 : SHAVE::CMU_CMVV_v4i16; break;
      case MVT::v8i8:   opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v8i8_signed_Myr4 : SHAVE::CMU_CMVV_v8i8; break;
      case MVT::v2f32:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v2f32_Myr4 : SHAVE::CMU_CMVV_v2f32; break;
      case MVT::v4f16:  opcode = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::CMU_CMVV_v4f16_Myr4 : SHAVE::CMU_CMVV_v4f16; break;

      // 32-bit vectors
      case MVT::v2i16:  opcode = SHAVE::CMU_CMII_v2i16; break;
      case MVT::v4i8:   opcode = SHAVE::CMU_CMII_v4i8; break;
      case MVT::v2f16:  opcode = SHAVE::CMU_CMII_v2f16; break;

      // 16-bit vectors
      case MVT::v2i8:   opcode = SHAVE::CMU_CMII_v2i8; break;

      default: llvm_unreachable("Unknown comparison type!");
      }
    }
  }

  // push only the LHS in case it is a CMU.CMZ
  if (isCMZ)
    ops.push_back(LHS);
  else {
    // Add both comparison operands
    ops.push_back(LHS);
    if (opcode == SHAVE::CMU_CMPI_Raw) {
      ConstantSDNode *ConstRHS = cast<ConstantSDNode>(RHS);
      ops.push_back(CurDAG->getTargetConstant(ConstRHS->getSExtValue() & 0xFFFF, SDLoc(N), MVT::i32));
    } else
      ops.push_back(RHS);
  }

  assert(N->getValueType(0) == MVT::Glue && "The compare node must produce a flag!");

  if (N->hasOneUse()) {
    SelectNodeWithDefaultOps(N, opcode, MVT::Glue, ops);
  } else {
    SDNode *result = getMachineNodeWithDefaultOps(opcode, SDLoc(N), MVT::Glue, ops);
    ReplaceNode(N, result);
  }
}

SDValue SHAVEISelDAGtoDAG::ZeroHighBits(SDValue Src, SDLoc dbgLoc, EVT DestVT) {
  unsigned Opc = 0;

  switch(DestVT.getSimpleVT().SimpleTy) {
  default:
    return SDValue();
  case MVT::v4f16:
  case MVT::v2f16:
    // Do not touch floating point values.
    return Src;
  case MVT::v4i8:
    if (Src.getValueSizeInBits() == 64)
      Opc = SHAVE::ZeroHi_v4i8_VRF64_l;
    else
      Opc = SHAVE::ZeroHi_v4i8;
    break;
  case MVT::v8i8:
    if(Src.getValueSizeInBits() == 64)
      return Src; // v8i8 fits perfectly into a VRF64_l, no need to zero
    else
      Opc = SHAVE::ZeroHi_v8i8;
    break;
  case MVT::v4i16:
    if (Src.getValueSizeInBits() == 64)
      return Src; // v4i16 fits perfectly into a VRF64_l, no need to zero
    else
      Opc = SHAVE::ZeroHi_v4i16;
    break;
  case MVT::v2i16:
    if (Src.getValueSizeInBits() == 64)
      Opc = SHAVE::ZeroHi_v2i16_VRF64_l;
    else
      Opc = SHAVE::ZeroHi_v4i16;
    break;
  case MVT::v2i8:
    if(Src.getValueSizeInBits() == 64)
      Opc = SHAVE::ZeroHi_v2i8_VRF64_l;
    else
      Opc = SHAVE::ZeroHi_v2i8;
    break;
  }

  return SDValue(CurDAG->getMachineNode(Opc, dbgLoc, Src.getValueType(), Src), 0);
}

SDValue SHAVEISelDAGtoDAG::VectorTRUNCATE(SDValue SrcVal, SDLoc dbgLoc, EVT DestVT) {
  // Clear the high bits to prevent interference with saturation.
  EVT SrcVT = SrcVal.getValueType();
  unsigned DestBytes = getVectorBytes(DestVT);
  SDValue ClearVal = ZeroHighBits(SrcVal, dbgLoc, DestVT);

  // Truncate the source. This is done with saturation but at this point
  // the high bits should be all zeros.
  unsigned ConvOpc = 0;

  switch (SrcVT.getSimpleVT().SimpleTy) {
  default:
    llvm_unreachable("Don't know how to convert this vector type.");
    break;
  case MVT::v4f32:
    ConvOpc = SHAVE::CMU_CPVV_v4f32_v4f16_conv;
    break;
  case MVT::v2f32:
    ConvOpc = SHAVE::CMU_CPVV_v2f32_v2f16_conv;
    break;
  case MVT::v4i32:
    switch(DestBytes) {
    default:
      llvm_unreachable("Don't know how to convert this vector type.");
      break;
    case 8:
      ConvOpc = SHAVE::CMU_CPVV_v4u32_v4u16_conv;
      break;
    case 4:
      ConvOpc = SHAVE::CMU_CPVV_v4u32_v4u8_conv;
      break;
    }
    break;
  case MVT::v2i32:
    switch(DestBytes) {
    default:
      llvm_unreachable("Don't know how to convert this vector type.");
      break;
    case 4:
      ConvOpc = SHAVE::CMU_CPVV_v2u32_v2u16_conv;
      break;
    case 2:
      ConvOpc = SHAVE::CMU_CPVV_v2u32_v2u8_conv;
      break;
    }
    break;
  case MVT::v4i16:
    ConvOpc = SHAVE::CMU_CPVV_v4u16_v4u8_conv;
    break;
  case MVT::v8i16:
    ConvOpc = SHAVE::CMU_CPVV_v8u16_v8u8_conv;
    break;
  case MVT::v2i16: {
      SmallVector<SDValue, 5> Ops;

      Ops.push_back(SrcVal);
      Ops.push_back(CurDAG->getTargetConstant(VSZM_LANE0, dbgLoc, MVT::i8));
      Ops.push_back(CurDAG->getTargetConstant(VSZM_LANE0 + 2, dbgLoc, MVT::i8));
      Ops.push_back(CurDAG->getTargetConstant(VSZM_CLEAR, dbgLoc, MVT::i8));
      Ops.push_back(CurDAG->getTargetConstant(VSZM_CLEAR, dbgLoc, MVT::i8));

      unsigned VSZMOpCode = SHAVE::CMU_VSZMBYTE_imm_irf;

      ClearVal = SDValue(getMachineNodeWithDefaultOps(VSZMOpCode, dbgLoc, SrcVT, Ops), 0);
      ConvOpc = SHAVE::CMU_CPII_32_16_trunc;
    }
    break;
  }

  assert(ClearVal && "Could not zero the high bits of this vector type");

  SmallVector<SDValue, 4> Ops;
  Ops.push_back(ClearVal);

  return SDValue(getMachineNodeWithDefaultOps(ConvOpc, dbgLoc, DestVT, Ops), 0);
}

namespace {
  SDValue ExtractHalfVectorFromTruncStore(SDValue StoreVal) {
    // Match (AssertZext x, i8)
    if (StoreVal.getOpcode() != ISD::AssertZext)
      return SDValue();

    VTSDNode *ZextType = dyn_cast<VTSDNode>(StoreVal.getOperand(1).getNode());

    if (!ZextType || (ZextType->getVT() != MVT::i8))
      return SDValue();

    StoreVal = StoreVal.getOperand(0);

    // Match (fptosi x)
    if (StoreVal.getOpcode() != ISD::FP_TO_SINT)
      return SDValue();

    StoreVal = StoreVal.getOperand(0);

    // Match (v8f16 x)
    if (StoreVal.getValueType() != MVT::v8f16)
      return SDValue();

    return StoreVal;
  }
}

bool SHAVEISelDAGtoDAG::isSupportedVRF128Store(SDNode *N) {
  StoreSDNode *storeNode = dyn_cast<StoreSDNode>(N);
  if (storeNode == nullptr)
    return false;
  SDValue value = N->getOperand(1);
  EVT srcVT = value.getValueType();
  EVT memoryVT = storeNode->getMemoryVT();

  if (srcVT == MVT::v8i8 &&
      value.getOpcode() == ISD::FP_TO_UINT &&
      value.getOperand(0).getValueType() == MVT::v8f16)
    return true;

  // Supported STI64/STI32 patterns
  // FIXME: Do we need to check both types?
  if ( (storeNode->getAddressingMode() == ISD::POST_INC) &&
       ((srcVT == MVT::v4f16 && memoryVT == MVT::v4f16) ||
        (srcVT == MVT::v4i16 && memoryVT == MVT::v4i16) ||
        (srcVT == MVT::v8i8  && memoryVT == MVT::v8i8 ) ||
        (srcVT == MVT::v4i8  && memoryVT == MVT::v4i8 )))
    return true;

  // Converting stores
  if ((srcVT == MVT::v4i32 && memoryVT == MVT::v4i8)  ||
      (srcVT == MVT::v4f32 && memoryVT == MVT::v4f16) ||
      (srcVT == MVT::v8i16 && memoryVT == MVT::v8i8)  ||
      (srcVT == MVT::v2i16 && memoryVT == MVT::v2i8)  ||
      (srcVT == MVT::v4i32 && memoryVT == MVT::v4i16) ||
      (srcVT == MVT::v4i16 && memoryVT == MVT::v4i8))
    return true;

  return false;
}

void SHAVEISelDAGtoDAG::SelectVectorSTORE(SDNode *N) {
  StoreSDNode *storeNode = dyn_cast<StoreSDNode>(N);

  if (!storeNode)
    llvm_unreachable("Not a store node.");

  if (hasFeature(SHAVE::HasVRF128_Feature) && isSupportedVRF128Store(N)) {
    SelectCode(N);
    return;
  }

  SDLoc dbgLoc(N);
  SDValue chain = N->getOperand(0);
  SDValue value = N->getOperand(1);
  SDValue base = N->getOperand(2);
  EVT memoryVT = storeNode->getMemoryVT();
  MachineMemOperand *MMO = storeNode->getMemOperand();
  unsigned memBytes = getVectorBytes(memoryVT);
  bool isTrunc = storeNode->isTruncatingStore();
  EVT srcVT = value.getValueType();
  unsigned srcBytes = getVectorBytes(srcVT);

  if (!memBytes || !srcBytes)
    llvm_unreachable("The source or memory types are not vector types.");
  else if (!isTrunc && (memBytes != srcBytes))
    llvm_unreachable("Source and memory type mismatch for non-truncating store.");
  else if (isTrunc && (memBytes >= srcBytes))
    llvm_unreachable("Source and memory size mismatch for truncating store.");
  else if ((memBytes > 64) || (srcBytes > 64))
    llvm_unreachable("Cannot store more than 16 words (512 bits).");

  if (hasFeature(SHAVE::HasVRF512_Feature) && memBytes < 64) {
    SelectCode(N);
    return;
  }

  // Look for special 'half' truncation patterns.
  SDValue memValue = value;
  bool FoundHalfTrunc = false;

  if ((memBytes == 8) && (srcBytes == 16)) {
    SDValue HalfTruncVal = ExtractHalfVectorFromTruncStore(value);
    if (HalfTruncVal.getNode()) {
      memValue = HalfTruncVal;
      FoundHalfTrunc = true;
    }
  }

  // Peform the vector truncation before the store.
  if(isTrunc && !FoundHalfTrunc)
    memValue = VectorTRUNCATE(memValue, dbgLoc, memoryVT);

  // Determine the instruction(s) needed to perform the store.
  FrameIndexSDNode *fi = dyn_cast<FrameIndexSDNode>(base.getNode());
  int64_t BaseOffset = 0;
  bool UseOffset = false;
  unsigned StoreOpc = 0;

  // FIXME: Movidius - adapt this code to use a register if the range of the immediate exceeds the range
  //        of a signed 15-bit integer: save the immediate to a register and use 'STX'.
  if (!FoundHalfTrunc) {
    if (fi) {
      base = CurDAG->getTargetFrameIndex(fi->getIndex(), base.getValueType());
      UseOffset = true;
      if (!isInt<15u>(fi->getIndex())) llvm_unreachable("Offset to STO exceeds range of a signed 15-bit integer");
//    assert(isInt<15u>(fi->getIndex()) && "Offset to STO exceeds range of a signed 15-bit integer");
    } else {
      BaseOffset = ExtractMemoryOffset(base);
      UseOffset = (BaseOffset != 0);
      if (!isInt<15u>(BaseOffset)) llvm_unreachable("Offset to STO exceeds range of a signed 15-bit integer");
//    assert(isInt<15u>(BaseOffset) && "Offset to STO exceeds range of a signed 15-bit integer");
    }
  }
  // Check if an STX instruction may be generated
  SDValue STXBase, STXOffset;
  bool useSTX = false;

  switch (memBytes) {
  default:
    llvm_unreachable("Store size not supported.");
  case 64:
    assert(hasFeature(SHAVE::HasVRF512_Feature) && "512-bit vectors not supported on Myriad2");
    StoreOpc = UseOffset ? SHAVE::STOV512 : SHAVE::STV512;
    break;
  case 16:
    assert(!FoundHalfTrunc);
    StoreOpc = UseOffset ? SHAVE::STOV128 : SHAVE::STV128;
    break;
  case 8:
      if (FoundHalfTrunc)
        StoreOpc = SHAVE::LSU_ST128_f16_u8_Raw;
      else if (UseOffset)
        StoreOpc = SHAVE::LSU_STOV64_l;
      else if (SelectADDRrr(base, STXBase, STXOffset)){
        useSTX = true;
        StoreOpc = SHAVE::LSU_STX64_Raw;
      }
      else
        StoreOpc = SHAVE::LSU_ST64_l_Raw;
    break;
  case 4: {
      // Copy to scalar register if doing a scalar store.
      SDValue Lane = CurDAG->getTargetConstant(0, dbgLoc, MVT::i32);
      SmallVector<SDValue, 4> Ops;
      Ops.push_back(memValue);
      Ops.push_back(Lane);

      MVT VT = memValue.getValueType().getSimpleVT();

      switch (VT.SimpleTy) {
      case MVT::v2f16:
        StoreOpc = UseOffset ? SHAVE::LSU_STO_v2f16 : SHAVE::LSU_ST_v2f16;
        break;
      case MVT::v4i8:
        StoreOpc = UseOffset ? SHAVE::LSU_STO_v4i8 : SHAVE::LSU_ST_v4i8;
        break;
      case MVT::v2i16:
        StoreOpc = UseOffset ? SHAVE::LSU_STO_v2i16 : SHAVE::LSU_ST_v2i16;
        break;
      default:
        memValue = SDValue(getMachineNodeWithDefaultOps(SHAVE::CMU_CPVI_x32, dbgLoc,
          MVT::i32, Ops), 0);
        StoreOpc = UseOffset ? SHAVE::LSU_STO_i32 : SHAVE::LSU_ST_i32;
        break;
      }
    }
    break;
  case 2:
    StoreOpc = UseOffset ? SHAVE::LSU_STO_i16 : SHAVE::LSU_ST_i16;
    break;
  }

  SDVTList typeList = CurDAG->getVTList(MVT::Other);

  // Store the vector.
  SmallVector<SDValue, 6> Ops;

  Ops.push_back(memValue);

  if (UseOffset) {
    // we're storing to a frame location or pointer + known offset
    // use the addressing mode [Reg + ImmOffset]
    Ops.push_back(base);
    Ops.push_back(CurDAG->getTargetConstant(BaseOffset, dbgLoc, MVT::i32));
  } else if (useSTX) {
    // we're storing to an unknown location
    // use the addressing mode [Reg]
    Ops.push_back(STXBase);
    Ops.push_back(STXOffset);
  } else {
    // we're storing to an unknown location
    // use the addressing mode [Reg]
    Ops.push_back(base);
  }

  // FIXME: Movidius - need to verify the 'size'
  MachineSDNode *Store = getMachineNodeWithDefaultOps(StoreOpc, dbgLoc, typeList,
                                                      Ops, chain);
  CurDAG->setNodeMemRefs(Store, {MMO});
  ReplaceNode(N, Store);
}

void SHAVEISelDAGtoDAG::SelectVectorUNPACK(SDNode *N) {
  // Determine the MI opcode.
  unsigned Opc = 0;
  unsigned NumSrcWords = N->getNumOperands() - 1;
  EVT VT = N->getValueType(0);

  switch (NumSrcWords) {
  default:
    llvm_unreachable("Unsupported number of operands for UNPACK.");
#if 0 // FIXME: Movidius - the constant packing is totally broken (Bugzilla #22594)
  case 1:
    if (VT.is32BitVector())
      Opc = SHAVE::Unpack1_32bit;
    else if (VT.is16BitVector())
      Opc = SHAVE::Unpack1_16bit;
    else
      Opc = SHAVE::Unpack1;
    break;
#endif
  case 2:
    Opc = SHAVE::Unpack2;
    break;
  case 4:
    Opc = SHAVE::Unpack4;
    break;
  }

  // Encode the destination type.
  SmallVector<SDValue, 5> Ops;
  SDValue DestTy = CurDAG->getTargetConstant(VT.getSimpleVT().SimpleTy,
                                             SDLoc(N), MVT::i32);

  Ops.push_back(DestTy);
  Ops.push_back(N->getOperand(NumSrcWords)); // 'AllSignBitsZero' flag.

  // Copy the node operands.
  for (unsigned i = 0; i < NumSrcWords; i++)
    Ops.push_back(N->getOperand(i));

  // Select the machine node.
  CurDAG->SelectNodeTo(N, Opc, VT, Ops);
}

SDNode *SHAVEISelDAGtoDAG::createSwizzle(SDLoc dbgLoc, int Opcode, SDValue src,
                                          llvm::ArrayRef<int> mask, int offset) {
  SmallVector<SDValue, 8> Ops;
  int SwizzleBase = 0;

  switch (Opcode) {
  case SHAVE::CMU_VSZMWORD_imm_vrf:
  case SHAVE::CMU_VSZMWORD_imm_vrf64:
  case SHAVE::CMU_VSZMBYTE_imm_vrf:
  case SHAVE::CMU_VSZMBYTE_imm_irf:
    SwizzleBase = VSZM_LANE0;
    break;

  default:
    SwizzleBase = 0;
    break;
  }

  Ops.push_back(src);

  for (unsigned i = 0; i < mask.size(); i++) {
    int LaneIdx = (mask[i] >= 0) ? (mask[i] + offset) : i;
    Ops.push_back(CurDAG->getTargetConstant(SwizzleBase + LaneIdx, dbgLoc, MVT::i8));
  }
  return getMachineNodeWithDefaultOps(Opcode, dbgLoc, src.getValueType(), Ops);
}

SDNode*SHAVEISelDAGtoDAG::createHalfWordSwizzle(SDLoc dbgLoc, SDValue src,
                                                ArrayRef<int> Mask) {
  DEBUG(dbgs() << "SHAVEISelDAGtoDAG::createHalfWordSwizzle()\n");

  SmallVector<SDValue, 9> Ops;
  EVT VecVT = src.getValueType();

  Ops.push_back(src);

  for (unsigned i = 0; i < 8; i++) {
    int lane = Mask[i];

    if (lane < 0 || lane >= 8)
      Ops.push_back(CurDAG->getTargetConstant(i, dbgLoc, MVT::i8));
    else
      Ops.push_back(CurDAG->getTargetConstant(lane, dbgLoc, MVT::i8));
  }

  SDNode *SwizzleNode = nullptr;

  if (VecVT.getSizeInBits() == 128)
    SwizzleNode = CurDAG->getMachineNode(SHAVE::INPUT_SWIZZLE8, dbgLoc, VecVT, Ops);
  else // VecVT.getSizeInBits() == 64
    SwizzleNode = CurDAG->getMachineNode(SHAVE::INPUT_SWIZZLE8_vrf64, dbgLoc, VecVT, Ops);

  // Myriad 2.3 does not allow LSU.SWZ8C to be used in conjunction with a CMU.CPVV
  // so we use CMU.ALIGNVEC with immediate 0 instead
  SmallVector<SDValue, 3> alignvecOperands;

  alignvecOperands.push_back(SDValue(SwizzleNode, 0));
  alignvecOperands.push_back(SDValue(SwizzleNode, 0));
  alignvecOperands.push_back(CurDAG->getTargetConstant(0, dbgLoc, MVT::i32));

  return getMachineNodeWithDefaultOps(SHAVE::CMU_ALIGNVEC_imm_vrf, dbgLoc, VecVT, alignvecOperands);
}

SDNode *SHAVEISelDAGtoDAG::createByteSwizzle(SDLoc dbgLoc, SDValue src,
                                             ArrayRef<int> Mask) {
  DEBUG(dbgs() << "SHAVEISelDAGtoDAG::createByteSwizzle()\n");

  // SWZBYTE swizzles within a word and uses the same mask for all words.
  // Therefore we need to do as many swizzle as there are words, each using
  // a different chunk of the mask and outputing a different 'part'.
  EVT VecVT = src.getValueType();
  EVT VT = VecVT.getVectorElementType();
  unsigned Opc = SHAVE::CMU_VSZMBYTE_imm_vrf;
  assert(VT.getSizeInBits() > 0 &&
         "Cannot swizzle a vector with size in bits of 0");
  unsigned ElePerWord = (32 / VT.getSizeInBits());
  unsigned NumParts = (VecVT.getVectorNumElements() / ElePerWord);
  SmallVector<SDNode *, 4> Parts;
  unsigned MaskPos = 0;

  for (unsigned i = 0; i < NumParts; i++) {
    ArrayRef<int> MaskPart(Mask.data() + MaskPos, ElePerWord);
    SDNode *Part = createSwizzle(dbgLoc, Opc, src, MaskPart, -MaskPos);
    Parts.push_back(Part);
    MaskPos += ElePerWord;
  }

  // Combine all parts by successively masking them.
  SDValue ByteEnable = CurDAG->getTargetConstant(1, dbgLoc, MVT::i8);
  SDValue ByteDisable = CurDAG->getTargetConstant(0, dbgLoc, MVT::i8);
  SDNode *Combined = Parts[0];
  SmallVector<SDValue, 8> Ops;

  Opc = SHAVE::VAU_CMBWORD_imm_vrf;

  for (unsigned i = 1; i < NumParts; i++) {
    Ops.clear();
    Ops.push_back(SDValue(Combined, 0));
    Ops.push_back(SDValue(Parts[i], 0));

    for (unsigned j = 0; j < NumParts; j++)
      Ops.push_back((i == j) ? ByteEnable : ByteDisable);

    Combined = getMachineNodeWithDefaultOps(Opc, dbgLoc, VecVT, Ops);
  }

  return Combined;
}

SDNode *SHAVEISelDAGtoDAG::VectorSHUFFLE(ShuffleAnalysis &SA, SDLoc dbgLoc) {
  DEBUG(dbgs() << "SHAVEISelDAGtoDAG::VectorSHUFFLE: ");

  unsigned SwizzleWordOpc;

  if (SA.VecVT.getSizeInBits() == 128)
    SwizzleWordOpc = SHAVE::CMU_VSZMWORD_imm_vrf;
  else // SA.VecVT.getSizeInBits() == 64
    SwizzleWordOpc = SHAVE::CMU_VSZMWORD_imm_vrf64;

  switch (SA.Action) {
  default:
    DEBUG(dbgs() << "default\n");
    break;
  case ShuffleAnalysis::LowerUndefined:
    DEBUG(dbgs() << "LowerUndefined\n");
    return CurDAG->getUNDEF(SA.VecVT).getNode();
  case ShuffleAnalysis::LowerIdentityV0:
    DEBUG(dbgs() << "LowerIdentityV0\n");
    return SA.V0.getNode();
  case ShuffleAnalysis::LowerSplatV0:
    DEBUG(dbgs() << "LowerSplatV0\n");
    return VectorSHUFFLE_Splat(SA, dbgLoc);
  case ShuffleAnalysis::LowerSwizzleWordV0:
    DEBUG(dbgs() << "LowerSwizzleWordV0\n");
    return createSwizzle(dbgLoc, SwizzleWordOpc, SA.V0, SA.Mask);
  case ShuffleAnalysis::LowerSwizzleHalfWordV0:
    DEBUG(dbgs() << "LowerSwizzleHalfWordV0\n");
    return createHalfWordSwizzle(dbgLoc, SA.V0, SA.Mask);
  case ShuffleAnalysis::LowerSwizzleByteV0:
    DEBUG(dbgs() << "LowerSwizzleByteV0\n");
    return createByteSwizzle(dbgLoc, SA.V0, SA.Mask);
  case ShuffleAnalysis::LowerSwapLowHighV0:
    DEBUG(dbgs() << "LowerSwapLowHighV0\n");
    return VectorSHUFFLE_LowHighSwap(SA, dbgLoc);
  case ShuffleAnalysis::LowerSwapVectors:
    DEBUG(dbgs() << "LowerSwapVectors\n");
    return VectorSHUFFLE_SourceSwap(SA, dbgLoc);
  case ShuffleAnalysis::LowerAnyExt16V0:
  case ShuffleAnalysis::LowerAnyExt32V0:
    DEBUG(dbgs() << "AnyExt\n");
    return VectorSHUFFLE_AnyExt(SA, dbgLoc, SA.VecVT);
  case ShuffleAnalysis::LowerTruncateV0:
    DEBUG(dbgs() << "TruncateV0\n");
    return VectorSHUFFLE_Truncate(SA, dbgLoc);
  case ShuffleAnalysis::LowerRotateV0:
    DEBUG(dbgs() << "Rotate\n");
    return VectorSHUFFLE_Rotate(SA, dbgLoc);
  case ShuffleAnalysis::LowerInterleaveVectors:
    DEBUG(dbgs() << "Interleave\n");
    return VectorSHUFFLE_Interleave(SA, dbgLoc);
  case ShuffleAnalysis::LowerDeinterleaveVectors:
    DEBUG(dbgs() << "Deinterleave\n");
    return VectorSHUFFLE_Deinterleave(SA, dbgLoc);
  case ShuffleAnalysis::LowerConcatVectors:
    DEBUG(dbgs() << "Concat\n");
    return VectorSHUFFLE_Concat(SA, dbgLoc);
  case ShuffleAnalysis::LowerShlVector:
    DEBUG(dbgs() << "128-bit shift left\n");
    return VectorSHUFFLE_Shift(SA, dbgLoc, true);
  case ShuffleAnalysis::LowerShrVector:
    DEBUG(dbgs() << "128-bit shift right\n");
    return VectorSHUFFLE_Shift(SA, dbgLoc, false);
  }

  return nullptr;
}

SDNode *SHAVEISelDAGtoDAG::VectorSHUFFLE_Shift(ShuffleAnalysis &SA,
	SDLoc dbgLoc, bool isLeftShift) {
	SmallVector<SDValue, 8> Ops;
	unsigned int Opc;
  // The shuffle analysis in moviCompile is big-endian so the logic is backwards for the little-endian SHAVE
  // This is why we use shiftright for left shifting and shiftleft for right shifting
	if (isLeftShift) {
		Ops.push_back(SA.V0);
		Opc = SHAVE::CMU_SHIFTRIGHT_imm;
	}
	else {
		Ops.push_back(SA.V1);
		Opc = SHAVE::CMU_SHIFTLEFT_imm;
	}

	Ops.push_back(CurDAG->getTargetConstant(SA.ShiftOffset, dbgLoc, MVT::i8));
	return getMachineNodeWithDefaultOps(Opc, dbgLoc, SA.VecVT, Ops);
}
SDNode *SHAVEISelDAGtoDAG::VectorSHUFFLE_Concat(ShuffleAnalysis &SA,
                                                SDLoc dbgLoc) {
  // Sort out the left and right vectors.
  SmallVector<int, 16> &LHSMask = SA.V0BeforeV1 ? SA.MaskV0 : SA.MaskV1;
  SmallVector<int, 16> &RHSMask = SA.V0BeforeV1 ? SA.MaskV1 : SA.MaskV0;
  SDValue LHSVec = SA.V0BeforeV1 ? SA.V0 : SA.V1;
  SDValue RHSVec = SA.V0BeforeV1 ? SA.V1 : SA.V0;

  // Lower the source vectors as needed.
  ShuffleAnalysis LHSSA(LHSMask, SA.VecVT, LHSVec, LHSVec, Subtarget);
  ShuffleAnalysis RHSSA(RHSMask, SA.VecVT, RHSVec, RHSVec, Subtarget);

  if (LHSSA.analyse() && RHSSA.analyse()) {
    SDNode *NewLHS = VectorSHUFFLE(LHSSA, dbgLoc);
    SDNode *NewRHS = VectorSHUFFLE(RHSSA, dbgLoc);

    if (NewLHS && NewRHS) {
      // Concatenate the two vectors.
      unsigned ConcatOpc;

      if (SA.VecVT.getSizeInBits() == 128)
        ConcatOpc = SHAVE::CMU_ALIGNVEC_imm_vrf;
      else // SA.VecVT.getSizeInBits() == 64
        ConcatOpc = SHAVE::CMU_ALIGNVEC_imm_vrf64;

      SmallVector<SDValue, 8> Ops;

      Ops.push_back(SDValue(NewLHS, 0));
      Ops.push_back(SDValue(NewRHS, 0));
      Ops.push_back(CurDAG->getTargetConstant(SA.ConcatOffset, dbgLoc, MVT::i8));

      return getMachineNodeWithDefaultOps(ConcatOpc, dbgLoc, SA.VecVT, Ops);
    }
  }

  return nullptr;
}

SDNode *SHAVEISelDAGtoDAG::VectorSHUFFLE_LowHighSwap(ShuffleAnalysis &SA,
                                                     SDLoc dbgLoc) {
  ShuffleAnalysis InnerSA(SA.MaskV0, SA.VecVT, SA.V0, SA.V1, Subtarget);

  if (InnerSA.analyse()) {
    unsigned ConcatPos = (SA.VecVT.getSizeInBits() / 8) / 2;
    unsigned ConcatOpc;

    if (SA.VecVT.getSizeInBits() == 128)
      ConcatOpc = SHAVE::CMU_ALIGNVEC_imm_vrf;
    else // SA.VecVT.getSizeInBits() == 64
      ConcatOpc = SHAVE::CMU_ALIGNVEC_imm_vrf64;

    SmallVector<SDValue, 8> Ops;

    Ops.push_back(SA.V0);
    Ops.push_back(SA.V1);

    Ops.push_back(CurDAG->getTargetConstant(ConcatPos, dbgLoc, MVT::i8));

    SDNode *SwappedV0 = getMachineNodeWithDefaultOps(ConcatOpc, dbgLoc, SA.VecVT, Ops);

    InnerSA.V0 = SDValue(SwappedV0, 0);

    return VectorSHUFFLE(InnerSA, dbgLoc);
  }

  return nullptr;
}

SDNode *SHAVEISelDAGtoDAG::VectorSHUFFLE_SourceSwap(ShuffleAnalysis &SA,
                                                    SDLoc dbgLoc) {
  ShuffleAnalysis NewSA(SA.MaskV0, SA.VecVT, SA.V1, SA.V0, Subtarget);

  if (NewSA.analyse())
    return VectorSHUFFLE(NewSA, dbgLoc);
  else
    return nullptr;
}

SDNode *SHAVEISelDAGtoDAG::VectorSHUFFLE_AnyExt(ShuffleAnalysis &SA,
                                                SDLoc dbgLoc, EVT VT,
                                                unsigned ExtOpc) {
  unsigned AnyExtOpc = 0;
  ArrayRef<int> AnyExtMask;
  MVT SrcVT;
  unsigned SubRegIdx = 0;

  if (SA.IsAnyExt16V0) {
    SubRegIdx = SHAVE:: hsub_0;
    if (ExtOpc)
      AnyExtOpc = ExtOpc;
    else if (VT == MVT::v8i16)
      AnyExtOpc = SHAVE::CMU_CPVV_v4i16_v4i32_conv;
    else
      AnyExtOpc = SHAVE::CMU_CPVV_v8i8_v8i16_conv;

    AnyExtMask = SA.AnyExt16Mask;
    // FIXME: Movidius - Was set to a 128-bit type due to test failures, reverting to correct type
    // was - SrcVT = MVT::v4i32;
    SrcVT = MVT::v4i16;
  } else if (SA.IsAnyExt32V0) {
    SubRegIdx = SHAVE:: qsub_0;
    AnyExtOpc = ExtOpc ? ExtOpc : static_cast<unsigned>(SHAVE::CMU_CPVV_v4i8_v4i32_conv);
    AnyExtMask = SA.AnyExt32Mask;
    // FIXME: Movidius - Was set to a 128-bit type due to test failures, reverting to correct type
    // was - SrcVT = MVT::v4i32;
    SrcVT = MVT::v4i8;
  }
  if (AnyExtOpc) {
    ShuffleAnalysis InnerSA(AnyExtMask, SA.VecVT, SA.V0, SA.V1, Subtarget);

    if (InnerSA.analyse()) {
      SmallVector<SDValue, 4> Ops;
      SDNode *InnerResult = VectorShuffleOrInputSwizzle(InnerSA, dbgLoc, nullptr);
#if 1
      // Enabling this fragment to convert the VRF128 result of swizzle into the VRF32_q0/VRF64_l
      // operand of the converting (extending) copy instruction. Else, llvm introduces register copies
      // which are not supported in our COPY-expansion handler.
      // Need to look out for cases where the register coalescer might generate the wrong registers when a
      // register is extracted then inserted. This was earlier reported in sextload_v8i32.ll
      Ops.push_back(SDValue(InnerResult, 0));
      Ops.push_back(CurDAG->getTargetConstant(SubRegIdx, dbgLoc, MVT::i32));

      SDNode *Extract = CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG,
                                               dbgLoc, SrcVT, Ops);

      Ops.clear();
      Ops.push_back(SDValue(Extract, 0));
#else
      Ops.push_back(SDValue(InnerResult, 0));
#endif
      return getMachineNodeWithDefaultOps(AnyExtOpc, dbgLoc, VT, Ops);
    }
  }

  return nullptr;
}

SDNode *SHAVEISelDAGtoDAG::VectorSHUFFLE_Truncate(ShuffleAnalysis &SA,
                                                  SDLoc dbgLoc) {
  EVT TruncVT;
  unsigned Opc = SA.getTruncOpcode(TruncVT);

  if (!Opc)
    return nullptr;

  SDValue Cleared = ZeroHighBits(SA.V0, dbgLoc, TruncVT);

  assert(Cleared && "Could not zero the high bits of this vector type");

  SmallVector<SDValue, 4> Ops;

  Ops.push_back(Cleared);

  SDNode *Truncation = getMachineNodeWithDefaultOps(Opc, dbgLoc, TruncVT, Ops);
  SDNode *ID = CurDAG->getMachineNode(TargetOpcode::IMPLICIT_DEF, dbgLoc, SA.VecVT);

  Ops.clear();
  Ops.push_back(SDValue(ID, 0));
  Ops.push_back(SDValue(Truncation, 0));
  Ops.push_back(CurDAG->getTargetConstant(SHAVE::hsub_0, dbgLoc, MVT::i32));

  return CurDAG->getMachineNode(TargetOpcode::INSERT_SUBREG, dbgLoc, SA.VecVT, Ops);
}

SDNode *SHAVEISelDAGtoDAG::VectorSHUFFLE_Rotate(ShuffleAnalysis &SA,
                                                SDLoc dbgLoc) {
  unsigned RotateOpc;

  if (SA.VecVT.getSizeInBits() == 128)
    RotateOpc = SHAVE::CMU_ALIGNVEC_imm_vrf;
  else // SA.VecVT.getSizeInBits() == 64
    RotateOpc = SHAVE::CMU_ALIGNVEC_imm_vrf64;

  SmallVector<SDValue, 8> Ops;

  Ops.push_back(SA.V0);
  Ops.push_back(SA.V0);
  Ops.push_back(CurDAG->getTargetConstant(SA.ConcatOffset, dbgLoc, MVT::i8));

  return getMachineNodeWithDefaultOps(RotateOpc, dbgLoc, SA.VecVT, Ops);
}

SDNode *SHAVEISelDAGtoDAG::VectorSHUFFLE_Interleave(ShuffleAnalysis &SA,
                                                    SDLoc dbgLoc) {
  unsigned Opc = 0;

  switch (SA.VecVT.getSimpleVT().SimpleTy) {
  // 64 bit types
  case MVT::v2i32:
  case MVT::v2f32:
    Opc = SHAVE::CMU_VILV_x32_v2i32;
    break;
  case MVT::v4i16:
  case MVT::v4f16:
    Opc = SHAVE::CMU_VILV_x16_v4i16;
    break;
  case MVT::v8i8:
    Opc = SHAVE::CMU_VILV_x8_v8i8;
    break;
  // 128 bit types
  case MVT::v4i32:
  case MVT::v4f32:
    Opc = SHAVE::CMU_VILV_x32;
    break;
  case MVT::v8i16:
  case MVT::v8f16:
    Opc = SHAVE::CMU_VILV_x16;
    break;
  case MVT::v16i8:
    Opc = SHAVE::CMU_VILV_x8;
    break;
  default:
    llvm_unreachable("Unsupported vector type for interleave");
  }

  if (!Opc)
    return nullptr;

  SmallVector<SDValue, 2> Ops;
  SDVTList VTList = CurDAG->getVTList(SA.VecVT, SA.VecVT);
  Ops.push_back(SA.V1);
  Ops.push_back(SA.V0);
  SDNode *InterleaveNode =
      getMachineNodeWithDefaultOps(Opc, dbgLoc, VTList, Ops);

  Ops.clear();

  switch (SA.InterleaveType) {
  case ShuffleAnalysis::NoInterleave:
    llvm_unreachable("Interleaving a NoInterleave");
  case ShuffleAnalysis::FirstHalfInterleave:
    Ops.push_back(SDValue(InterleaveNode, 0));
    break;
  case ShuffleAnalysis::LastHalfInterleave:
    Ops.push_back(SDValue(InterleaveNode, 1));
    break;
  }

  const TargetRegisterClass *DstClass =
      getTargetLowering()->getRegClassFor(SA.VecVT.getSimpleVT());
  Ops.push_back(CurDAG->getTargetConstant(DstClass->getID(), dbgLoc, MVT::i32));
  return CurDAG->getMachineNode(TargetOpcode::COPY_TO_REGCLASS, dbgLoc,
                                SA.VecVT, Ops);
}

SDNode *SHAVEISelDAGtoDAG::VectorSHUFFLE_Deinterleave(ShuffleAnalysis &SA,
                                                      SDLoc dbgLoc) {
  unsigned MaskSize = SA.Mask.size();
  unsigned DeinterleaveOpc = 0;

  if (MaskSize == 4)
    DeinterleaveOpc = SHAVE::CMU_VDILV_x32;
  else if (MaskSize == 8)
    DeinterleaveOpc = SHAVE::CMU_VDILV_x16;
  else if (MaskSize == 16)
    DeinterleaveOpc = SHAVE::CMU_VDILV_x8;

  if (DeinterleaveOpc) {
    SmallVector<SDValue, 4> Ops;
    SDVTList VTList = CurDAG->getVTList(SA.VecVT, SA.VecVT);

    Ops.push_back(SA.V0);
    Ops.push_back(SA.V1);

    SDNode *DeinterleaveNode = getMachineNodeWithDefaultOps(DeinterleaveOpc, dbgLoc,
                                                            VTList, Ops);

    Ops.clear();
    // The second destination register holds the value we want when the first index is 0 (i.e. evens)
    // The first destination register holds the value we want when the first index is 1 (i.e. odds)
    if (SA.FirstDefV0 == 0)
      Ops.push_back(SDValue(DeinterleaveNode, 1));
    else // SA.FirstDefV0 == 1
      Ops.push_back(SDValue(DeinterleaveNode, 0));
    Ops.push_back(CurDAG->getTargetConstant(SHAVE::VRF128RegClassID, dbgLoc, MVT::i32));

    return CurDAG->getMachineNode(TargetOpcode::COPY_TO_REGCLASS, dbgLoc, SA.VecVT, Ops);
  }

  return nullptr;
}

SDNode *SHAVEISelDAGtoDAG::VectorSHUFFLE_Splat(ShuffleAnalysis &SA,
                                               SDLoc dbgLoc) {
  EVT VT = SA.VecVT.getVectorElementType();
  SDNode *ExtractedVal = nullptr;
  SmallVector<SDValue, 4> Ops;
  unsigned SplatOpc = 0;
  unsigned ExtractOpc = 0;
  bool Truncate = false;
  int SplatLane  = SA.SplatLaneV0;

  switch (VT.getSimpleVT().SimpleTy) {
  default:
    return nullptr;
  case MVT::i8:
    if (SA.VecVT.getSizeInBits() == 128) {
      SplatOpc = (SplatLane % 2 == 0) ? SHAVE::CMU_CPIVR_i8 : SHAVE::CMU_CPIVR_i8_q1;
      ExtractOpc = SHAVE::CMU_CPVI_x16_l;
    }
    else { // SA.VecVT.getSizeInBits() == 64
      SplatOpc = (SplatLane % 2 == 0) ? SHAVE::CMU_CPIVR_v8i8 : SHAVE::CMU_CPIVR_v8i8_q1;
      ExtractOpc = SHAVE::CMU_CPVI_v4i16_l;
    }
    SplatLane >>= 1;
    Truncate = true;
    break;
  case MVT::i16:
    if (SA.VecVT.getSizeInBits() == 128) {
      SplatOpc = SHAVE::CMU_CPIVR_i16;
      ExtractOpc = SHAVE::CMU_CPVI_x16_l;
    }
    else { // SA.VecVT.getSizeInBits() == 64
      SplatOpc = SHAVE::CMU_CPIVR_v4i16;
      ExtractOpc = SHAVE::CMU_CPVI_v4i16_l;
    }
    break;
  case MVT::f16:
    if (SA.VecVT.getSizeInBits() == 128) {
      SplatOpc = SHAVE::CMU_CPIVR_i16;
      ExtractOpc = SHAVE::CMU_CPVI_x16_l;
    }
    else { // SA.VecVT.getSizeInBits() == 64
      SplatOpc = SHAVE::CMU_CPIVR_v4f16;
      ExtractOpc = SHAVE::CMU_CPVI_v4f16_l;
    }
    break;
  case MVT::i32:
    if (SA.VecVT.getSizeInBits() == 128) {
      SplatOpc = SHAVE::CMU_CPIVR_i32;
      ExtractOpc = SHAVE::CMU_CPVI_x32;
    }
    else { // SA.VecVT.getSizeInBits() == 64
      SplatOpc = SHAVE::CMU_CPIVR_v2i32;
      ExtractOpc = SHAVE::CMU_CPVI_x32_v2i32;
    }
    break;
  case MVT::f32:
    if (SA.VecVT.getSizeInBits() == 128) {
      SplatOpc = SHAVE::CMU_CPIVR_i32;
      ExtractOpc = SHAVE::CMU_CPVI_x32;
    }
    else { // SA.VecVT.getSizeInBits() == 64
      SplatOpc = SHAVE::CMU_CPIVR_v2f32;
      ExtractOpc = SHAVE::CMU_CPVI_f32_v2f32;
    }
    break;
  }

  // Optimise common constant splat idiom.
  int InsertLane = 0;

  SDValue Scalar = SHAVELowering::findScalarInsert(SA.V0, InsertLane);
  if (Scalar.getNode() && (InsertLane == SA.SplatLaneV0))
    ExtractedVal = Scalar.getNode();

  // Extract the value to splat.
  if (!ExtractedVal) {
    Ops.push_back(SA.V0);
    Ops.push_back(CurDAG->getTargetConstant(SplatLane, dbgLoc, MVT::i32));
    ExtractedVal = getMachineNodeWithDefaultOps(ExtractOpc, dbgLoc, VT, Ops);

    if (Truncate) {
      SDValue SubReg;
      if (SA.SplatLaneV0 % 2 == 0)
        SubReg = CurDAG->getTargetConstant(SHAVE::qsub_0, dbgLoc, MVT::i32);
      else
        SubReg = CurDAG->getTargetConstant(SHAVE::qsub_1, dbgLoc, MVT::i32);
      ExtractedVal = CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG, dbgLoc,
                                            MVT::i8, SDValue(ExtractedVal, 0),
                                            SubReg);
    }
  }

  // Splat the value.
  Ops.clear();
  Ops.push_back(SDValue(ExtractedVal, 0));

  return getMachineNodeWithDefaultOps(SplatOpc, dbgLoc, SA.VecVT, Ops);
}

// Find a SD node that uses N as its LHS operand.
// The node must be allowed to have an input swizzle.
SDNode *SHAVEISelDAGtoDAG::FindLHSUserForSwizzle(SDNode *N) const {
  int UseIdx = -1;
  unsigned Users = 0;
  SDNode *LastUser = nullptr;

  for (SDNode::use_iterator I = N->use_begin(), E = N->use_end(); I != E; ++I) {
    SDNode *User = *I;

    for (unsigned i = 0; i < User->getNumOperands(); i++) {
      SDValue Op = User->getOperand(i);

      if (Op.getNode() == N) {
        UseIdx = (int)i;
        Users++;
        LastUser = User;
      }
    }
  }

  // Make sure this is the only user of N.
  if (Users != 1)
    return nullptr;

  // Make sure the user is an actual SHAVE instruction.
  const SHAVEInstrInfo *SII = Subtarget.getInstrInfo();

  if (!LastUser->isMachineOpcode())
    return nullptr;

  unsigned Opc = LastUser->getMachineOpcode();
  const MCInstrDesc &Desc = SII->get(Opc);

  if ((Opc == TargetOpcode::COPY) || (Opc == TargetOpcode::COPY_TO_REGCLASS))
    return FindLHSUserForSwizzle(LastUser);
  else if (SHAVEConflicts::check_isACCPSeq(Opc))
    return (UseIdx > 0) ? LastUser : nullptr;
  else if (SHAVEConflicts::check_isMACPSeq(Opc))
    return ((UseIdx > 0) && !((UseIdx - 1) & 1)) ? LastUser : nullptr;
  else if ((Opc <= SHAVE::ADJCALLSTACKUP) || Desc.isPseudo())
    return nullptr;

  // Make sure the user has the right functional unit.
  unsigned FUnit = SII->GetFunctionalUnit(Opc);

  switch (FUnit) {
  default:
    return nullptr;
  case SHAVE::CMU:
  case SHAVE::SAU:
  case SHAVE::VAU:
    break;
  }

  // Make sure the operand that uses the shuffle is the first input.
  const unsigned FirstInputOpIdx = 0;

  if ((unsigned)UseIdx != FirstInputOpIdx)
    return nullptr;

  return LastUser;
}

SDNode *SHAVEISelDAGtoDAG::VectorShuffleOrInputSwizzle(ShuffleAnalysis &SA,
                                                       SDLoc dbgLoc,
                                                       SDNode *N) {
  if (SA.Action == ShuffleAnalysis::LowerIdentityV0)
    return VectorSHUFFLE(SA, dbgLoc);

  // Make sure there is a suitable user for the input swizzle.
  // If N is null the caller guarantees that there is one.
  if (SA.IsSwizzle8V0 && (!N || FindLHSUserForSwizzle(N))) {
    SmallVector<SDValue, 12> Ops;
    unsigned MaskSize = SA.Mask.size();

    Ops.push_back(SA.V0);

    for (unsigned i = 0; i < 8; i++) {
      int Lane = -1;

      if (MaskSize == 8) {
        Lane = SA.Mask[i];
      } else if (MaskSize == 16) {
        Lane = SA.Mask[i*2];

        if (Lane >= 0)
          Lane /= 2;
      } else if (MaskSize == 4) {
        Lane = SA.Mask[i/2];

        if (Lane >= 0)
          Lane = (Lane * 2) + (i & 1);
      } else
        llvm_unreachable("Unsupported shuffle mask length");

      if (Lane < 0)
        Lane = i;

      Ops.push_back(CurDAG->getTargetConstant(Lane, dbgLoc, MVT::i8));
    }

    if (SA.VecVT.getSizeInBits() == 128)
      return CurDAG->getMachineNode(SHAVE::INPUT_SWIZZLE8, dbgLoc, SA.VecVT, Ops);

    else // SA.VecVT.getSizeInBits() == 64
      return CurDAG->getMachineNode(SHAVE::INPUT_SWIZZLE8_vrf64, dbgLoc, SA.VecVT, Ops);
  }

  return VectorSHUFFLE(SA, dbgLoc);
}

void SHAVEISelDAGtoDAG::SelectVectorSHUFFLE(SDNode *N) {
  DEBUG(dbgs() << "SHAVEISelDAGtoDAG::SelectVectorShuffle(SDNode *N)\n");

  ShuffleVectorSDNode *SHV = cast<ShuffleVectorSDNode>(N);
  EVT VecVT = N->getValueType(0);
  SDValue LHS = SHV->getOperand(0);
  SDValue RHS = SHV->getOperand(1);
  ShuffleAnalysis SA(SHV->getMask(), VecVT, LHS, RHS, Subtarget);

  if (SA.analyse()) {
    // Look for shuffles that can be lowered as swizzles and
    // which are LHS operands of VAU/SAU/CMU instructions.
    SDNode * Result = VectorShuffleOrInputSwizzle(SA, SDLoc(N), N);

    if (Result) {
      ReplaceNode(N, Result);
      return;
    }
  }

  SelectCode(N);
}

// Try to match (sint_to_fp (sign_extend_inreg (vector_shuffle <anyext>)))
//      or just (sign_extend_inreg (vector_shuffle <anyext>))
//      or just (sint_to_fp (vector_shuffle <anyext>))
// and combine it into a simpler sequence of instructions.
SDNode *SHAVEISelDAGtoDAG::CombineVectorExtensions(SDNode *N) {
  SDLoc dbgLoc(N);
  EVT FPDstVT;
  EVT InregSrcVT;
  EVT InregDstVT;
  SDNode *SIntToFP = nullptr;
  SDNode *SExtInreg = nullptr;

  // Try to find the original value before any extension. Hopefully we can
  // combine any intermediate extension with the SIGN_EXTEND_INREG.
  SDValue Orig(N, 0);

  if (Orig.getOpcode() == ISD::SINT_TO_FP) {
    SIntToFP = Orig.getNode();
    FPDstVT = Orig.getValueType();
    Orig = Orig.getOperand(0);
  }

  if (Orig.getOpcode() == ISD::SIGN_EXTEND_INREG) {
    SExtInreg = Orig.getNode();
    InregSrcVT = cast<VTSDNode>(Orig.getOperand(1))->getVT();
    InregDstVT = Orig.getValueType();
    Orig = Orig.getOperand(0);
  }

  while (Orig.getOpcode() == ISD::BITCAST)
    Orig = Orig.getOperand(0);

  // Look for a VECTOR_SHUFFLE node that implements 'ANY_EXTEND'.
  if (Orig.getOpcode() != ISD::VECTOR_SHUFFLE)
    return nullptr;

  ShuffleVectorSDNode *SHV = cast<ShuffleVectorSDNode>(Orig.getNode());
  EVT SVT = SHV->getValueType(0);
  SDValue LHS = SHV->getOperand(0);
  SDValue RHS = SHV->getOperand(1);
  ShuffleAnalysis SA(SHV->getMask(), Orig.getValueType(), LHS, RHS, Subtarget);

  if (!SA.analyse() || (!SA.IsAnyExt16V0 && !SA.IsAnyExt32V0))
    return nullptr;

  // Identify any in-register extension operation.
  EVT ExtVT1;
  unsigned SExtInregOpc = 0;

  if (SExtInreg) {
    ExtVT1 = SExtInreg->getValueType(0);

    if (SA.IsAnyExt32V0) {
      if ((InregSrcVT == MVT::v4i8) && (InregDstVT == MVT::v4i32))
        SExtInregOpc = SHAVE::CMU_CPVV_v4i8_v4i32_conv;
    } else if (SA.IsAnyExt16V0) {
      if ((InregSrcVT == MVT::v4i8) && (InregDstVT == MVT::v4i32)) {
        // FIXME: Movidius - This requires truncation of the input and fiddling with the
        // shuffle mask. Support this case later.
        return nullptr;
      } else if ((InregSrcVT == MVT::v4i16) && (InregDstVT == MVT::v4i32))
        SExtInregOpc = SHAVE::CMU_CPVV_v4i16_v4i32_conv;
      else if ((InregSrcVT == MVT::v8i8) && (InregDstVT == MVT::v8i16))
        SExtInregOpc = SHAVE::CMU_CPVV_v8i8_v8i16_conv;
    }

    SVT = SExtInreg->getValueType(0);
  }

  // Identify any int-to-float conversion operation.
  EVT ExtVT2;
  unsigned SIntToFPOpc = 0;

  if (SIntToFP) {
    ExtVT2 = SIntToFP->getValueType(0);

    if ((SVT == MVT::v4i16) && (FPDstVT == MVT::v4f16))
      SIntToFPOpc = SHAVE::CMU_CPVV_v4i16_v4f16_conv;
    else if ((SVT == MVT::v4i32) && (FPDstVT == MVT::v4f32))
      SIntToFPOpc = SHAVE::CMU_CPVV_v4i32_v4f32_conv;
    else if ((SVT == MVT::v8i16) && (FPDstVT == MVT::v8f16))
      SIntToFPOpc = SHAVE::CMU_CPVV_v8i16_v8f16_conv;
  }

  // Combine the two operations, if possible.
  unsigned ExtOpc1 = 0;
  unsigned ExtOpc2 = 0;

  if (!SExtInregOpc && !SIntToFPOpc)
    return nullptr;
  else if (SExtInregOpc && !SIntToFPOpc)
    ExtOpc1 = SExtInregOpc;
  else if (!SExtInregOpc && SIntToFPOpc) {
    ExtOpc1 = SIntToFPOpc;
    ExtVT1 = ExtVT2;
  } else if ((SExtInregOpc == SHAVE::CMU_CPVV_v4i16_v4i32_conv) &&
             (SIntToFPOpc == SHAVE::CMU_CPVV_v4i32_v4f32_conv)) {
    ExtOpc1 = SHAVE::CMU_CPVV_v4i16_v4f32_conv;
    ExtVT1 = ExtVT2;
  } else if ((SExtInregOpc == SHAVE::CMU_CPVV_v8i8_v8i16_conv) &&
             (SIntToFPOpc == SHAVE::CMU_CPVV_v8i16_v8f16_conv)) {
    ExtOpc1 = SHAVE::CMU_CPVV_v8i8_v8f16_conv;
    ExtVT1 = ExtVT2;
  } else {
    ExtOpc1 = SExtInregOpc;
    ExtOpc2 = SIntToFPOpc;
  }

  // Emit the combined conversion instruction.
  SDNode *Ext = VectorSHUFFLE_AnyExt(SA, dbgLoc, ExtVT1, ExtOpc1);

  // Emit any necessary second conversion instruction.
  if (ExtOpc2) {
    SmallVector<SDValue, 4> Ops;

    Ops.push_back(SDValue(Ext, 0));

    Ext = getMachineNodeWithDefaultOps(ExtOpc2, dbgLoc, ExtVT2, Ops);
  }

  return Ext;
}

void SHAVEISelDAGtoDAG::SelectVectorSIGN_EXTEND_INREG(SDNode *N) {
  SDNode * result = CombineVectorExtensions(N);

  if (result)
    ReplaceNode(N, result);
  else
    SelectCode(N);
}

void SHAVEISelDAGtoDAG::SelectVectorANY_EXTEND(SDNode *N) {
  EVT VT = N->getValueType(0);
  EVT OpVT = N->getOperand(0).getValueType();

  // Temporarily, process ANY_EXTEND with vector type.
  // ANY_EXTEND ==> CMU.CPVV
  unsigned opcode = 0;

  if (VT == MVT::v4i32 && OpVT == MVT::v4i16)
    opcode = SHAVE::CMU_CPVV_v4i16_v4i32_conv;
  else if (VT == MVT::v8i16 && OpVT == MVT::v8i8)
    opcode = SHAVE::CMU_CPVV_v8i8_v8i16_conv;
  else if (VT == MVT::v4i16 && OpVT == MVT::v4i8)
    opcode = SHAVE::CMU_CPVV_v4i8_v4i16_conv;
  else if (VT == MVT::v4i32 && OpVT == MVT::v4i8)
    opcode = SHAVE::CMU_CPVV_v4i8_v4i32_conv;
  else if (VT == MVT::v2i32 && OpVT == MVT::v2i16)
    opcode = SHAVE::CMU_CPVV_v2i16_v2i32_conv;
  else if (VT == MVT::v2i32 && OpVT == MVT::v2i8)
    opcode = SHAVE::CMU_CPVV_v2i8_v2i32_conv;

  if (opcode != 0) {
    SmallVector<SDValue, 4> Ops;

    Ops.push_back(N->getOperand(0));

    SDNode * result = getMachineNodeWithDefaultOps(opcode, SDLoc(N), VT, Ops);
    ReplaceNode(N, result);
  }
  else {
    SelectCode(N);
  }
}

void SHAVEISelDAGtoDAG::SelectVectorTRUNCATE(SDNode *N) {
  SDLoc dbgLoc(N);
  EVT DstVT = N->getValueType(0);
  SDValue Trunc = VectorTRUNCATE(N->getOperand(0), dbgLoc, DstVT);

  if (Trunc.getNode() != 0) {
    ReplaceNode(N, Trunc.getNode());
  }
  else {
    SelectCode(N);
  }
}

void SHAVEISelDAGtoDAG::SelectVectorSINT_TO_FP(SDNode *N) {
  SDNode * result = CombineVectorExtensions(N);

  if (result)
    ReplaceNode(N, result);
  else
    SelectCode(N);
}

void SHAVEISelDAGtoDAG::SelectEXTRACT_SUBVECTOR(SDNode *N) {
  SDLoc dbgLoc(N);
  SDValue Src = N->getOperand(0);
  MVT DstVT = N->getValueType(0).getSimpleVT();
  MVT SrcVT = Src.getValueType().getSimpleVT();
  unsigned NumSrcEle = Src.getValueType().getVectorNumElements();
  unsigned Offset = cast<ConstantSDNode>(N->getOperand(1))->getZExtValue();
  const TargetRegisterClass *SrcRC = getTargetLowering()->getRegClassFor(SrcVT);
  const TargetRegisterClass *DstRC = getTargetLowering()->getRegClassFor(DstVT);
  unsigned SubRegIdx = 0;

  if (SrcRC == &SHAVE::VRF128RegClass && DstRC == &SHAVE::VRF64_lRegClass) {
    SubRegIdx = SHAVE::hsub_0;
  }
  else if ((SrcRC == &SHAVE::VRF128RegClass || SrcRC == &SHAVE::VRF64_lRegClass) && DstRC == &SHAVE::VRF32_q0RegClass) {
    SubRegIdx = SHAVE::qsub_0;
  }
  else if (SrcRC == &SHAVE::VRF64_lRegClass && DstRC == &SHAVE::VRF32_q0RegClass) {
    SubRegIdx = SHAVE::hsub_0;
  }
  else {
    SelectCode(N);
    return;
  }

  // Turn the extraction into a shuffle.
  if (Offset != 0) {
    SmallVector<int, 16> Mask;

    for (unsigned i = 0; i < NumSrcEle; i++) {
      unsigned Lane = Offset + i;

      if (Lane >= NumSrcEle)
        Lane -= NumSrcEle;

      Mask.push_back(Lane);
    }

    ShuffleAnalysis SA(Mask, Src.getValueType(), Src, Src, Subtarget);

    SA.analyse();

    SDNode *Extraction = VectorShuffleOrInputSwizzle(SA, dbgLoc, N);

    assert(Extraction && "Could not lower EXTRACT_SUBVECTOR as a shuffle!");
    Src = SDValue(Extraction, 0);
  }

  SmallVector<SDValue, 2> Ops;

  Ops.push_back(Src);
  Ops.push_back(CurDAG->getTargetConstant(SubRegIdx, dbgLoc, MVT::i32));

  SDNode * result = CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG, dbgLoc, DstVT, Ops);
  ReplaceNode(N, result);
}

void SHAVEISelDAGtoDAG::SelectBUILD_VECTOR(SDNode *N) {
  unsigned NumOperands = N->getNumOperands();
  EVT VecVT = N->getValueType(0);
  EVT EleVT = VecVT.getVectorElementType();
  SDLoc dbgLoc(N);

  // Test for vector with all elements equal and use SPLAT if possible
  bool isSplat = true;
  SDValue firstOperand = N->getOperand(0);

  for(unsigned i = 1; i < NumOperands; i++) {
    if( firstOperand != N->getOperand(i) ) {
      isSplat = false;
      break;
    }
  }

  if (isSplat) {
    unsigned opcode = 0;

    switch (VecVT.getSimpleVT().SimpleTy) {
    default:
      assert("Unrecognised vector type for BUILD_VECTOR splat");
      break;
    // 128 bit vectors
    case MVT::v4i32: opcode = SHAVE::CMU_CPIVR_i32; break;
    case MVT::v8i16: opcode = SHAVE::CMU_CPIVR_i16; break;
    case MVT::v16i8: opcode = SHAVE::CMU_CPIVR_i8; break;
    case MVT::v4f32: opcode = SHAVE::CMU_CPIVR_f32; break;
    case MVT::v8f16: opcode = SHAVE::CMU_CPIVR_f16; break;
    // 64 bit vectors
    case MVT::v2i32: opcode = SHAVE::CMU_CPIVR_v2i32; break;
    case MVT::v4i16: opcode = SHAVE::CMU_CPIVR_v4i16; break;
    case MVT::v8i8:  opcode = SHAVE::CMU_CPIVR_v8i8; break;
    case MVT::v2f32: opcode = SHAVE::CMU_CPIVR_v2f32; break;
    case MVT::v4f16: opcode = SHAVE::CMU_CPIVR_v4f16; break;
    // 32 bit vectors
    case MVT::v2i16: opcode = SHAVE::CMU_VSZM_SPLAT_i16_v2i16; break;
    case MVT::v4i8:  opcode = SHAVE::CMU_VSZM_SPLAT_i8_v4i8; break;
    case MVT::v2f16: opcode = SHAVE::CMU_VSZM_SPLAT_f16_v2f16; break;
    // 16 bit vectors
    case MVT::v2i8:  opcode = SHAVE::CMU_VSZM_SPLAT_i8_v2i8; break;
    }

    SmallVector<SDValue, 1> ops;
    ops.push_back(firstOperand);

    SDNode *result = getMachineNodeWithDefaultOps(opcode, dbgLoc, VecVT, ops);
    ReplaceNode(N, result);
    return;
  }

  // If Myriad4v0, we can use a sequence of CPIV instructions to insert the
  // vector elements.

  if (hasFeature(SHAVE::HasVRF512_Feature)) {
    SDNode * result = CurDAG->getMachineNode(SHAVE::IMPLICIT_DEF, dbgLoc, VecVT);

    for (unsigned i = 0; i < NumOperands; i++) {
      // No need to insert anything for undefined elements
      if (N->getOperand(i)->isUndef())
        continue;

      unsigned opcode = 0;

      switch (VecVT.getSimpleVT().SimpleTy) {
      default:
        llvm_unreachable("Unrecognised vector type for BUILD_VECTOR");
      // 512-bit vectors
      case MVT::v16i32: opcode = SHAVE::CMU_CPIV_i32_v16i32_Myr4; break;
      case MVT::v32i16: opcode = SHAVE::CMU_CPIV_i16_v32i16_Myr4; break;
      case MVT::v64i8:  opcode = SHAVE::CMU_CPIV_i8_v64i8_Myr4;   break;
      case MVT::v16f32: opcode = SHAVE::CMU_CPIV_f32_v16f32_Myr4; break;
      case MVT::v32f16: opcode = SHAVE::CMU_CPIV_f16_v32f16_Myr4; break;
      // 256-bit vectors
      case MVT::v8i32:  opcode = SHAVE::CMU_CPIV_i32_v8i32_Myr4;  break;
      case MVT::v16i16: opcode = SHAVE::CMU_CPIV_i16_v16i16_Myr4; break;
      case MVT::v32i8:  opcode = SHAVE::CMU_CPIV_i8_v32i8_Myr4;   break;
      case MVT::v8f32:  opcode = SHAVE::CMU_CPIV_f32_v8f32_Myr4;  break;
      case MVT::v16f16: opcode = SHAVE::CMU_CPIV_f16_v16f16_Myr4; break;
      // 128-bit vectors
      case MVT::v4i32:  opcode = SHAVE::CMU_CPIV_i32_v4i32_Myr4;  break;
      case MVT::v8i16:  opcode = SHAVE::CMU_CPIV_i16_v8i16_Myr4;  break;
      case MVT::v16i8:  opcode = SHAVE::CMU_CPIV_i8_v16i8_Myr4;   break;
      case MVT::v4f32:  opcode = SHAVE::CMU_CPIV_f32_v4f32_Myr4;  break;
      case MVT::v8f16:  opcode = SHAVE::CMU_CPIV_f16_v8f16_Myr4;  break;
      // 64-bit vectors
      case MVT::v2i32:  opcode = SHAVE::CMU_CPIV_i32_v2i32_Myr4;  break;
      case MVT::v4i16:  opcode = SHAVE::CMU_CPIV_i16_v4i16_Myr4;  break;
      case MVT::v8i8:   opcode = SHAVE::CMU_CPIV_i8_v8i8_Myr4;    break;
      case MVT::v2f32:  opcode = SHAVE::CMU_CPIV_f32_v2f32_Myr4;  break;
      case MVT::v4f16:  opcode = SHAVE::CMU_CPIV_f16_v4f16_Myr4;  break;
      // 32-bit vectors
      case MVT::v2i16:  opcode = SHAVE::CMU_CPIV_i16_v2i16_Myr4;  break;
      case MVT::v4i8:   opcode = SHAVE::CMU_CPIV_i8_v4i8_Myr4;    break;
      case MVT::v2f16:  opcode = SHAVE::CMU_CPIV_f16_v2f16_Myr4;  break;
      // 16-bit vectors
      case MVT::v2i8:   opcode = SHAVE::CMU_CPIV_i8_v2i8_Myr4;    break;
      }

      SmallVector<SDValue, 2> ops;
      ops.push_back(SDValue(result, 0));
      ops.push_back(N->getOperand(i));
      ops.push_back(CurDAG->getTargetConstant(i, dbgLoc, MVT::i32));

      result = getMachineNodeWithDefaultOps(opcode, dbgLoc, VecVT, ops);
    }

    ReplaceNode(N, result);
    return;
  }

  // Alternatively, construct using a sequence of vector shift-in element

  // Prepare type list for SHAVEISD::SHLV
  SmallVector<EVT, 4> Types;

  Types.push_back(VecVT);
  Types.push_back(EleVT);
  Types.push_back(MVT::i32);
  Types.push_back(MVT::i32);

  SDNode * result = CurDAG->getMachineNode(SHAVE::IMPLICIT_DEF, dbgLoc, VecVT);

  for(unsigned i = 0; i < NumOperands; i++) {
    unsigned opcode = 0;

    // FIXME: Movidius - This builds 32 and 16 bit vectors in VRF registers, which then
    //                   need to be copied to IRF to be used. There is a better way to
    //                   do this.
    switch (VecVT.getSimpleVT().SimpleTy) {
    default:
      llvm_unreachable("Unrecognised vector type for BUILD_VECTOR");
    // 128 bit vectors
    case MVT::v4i32: opcode = SHAVE::CMU_SHLIV_x32; break;
    case MVT::v8i16: opcode = SHAVE::CMU_SHLIV_x16; break;
    case MVT::v16i8: opcode = SHAVE::CMU_SHLIV_x8; break;
    case MVT::v4f32: opcode = SHAVE::CMU_SHLIV_x32; break;
    case MVT::v8f16: opcode = SHAVE::CMU_SHLIV_x16; break;
    // 64 bit vectors
    case MVT::v2i32: opcode = SHAVE::CMU_SHLIV_x32_v2i32; break;
    case MVT::v4i16: opcode = SHAVE::CMU_SHLIV_x16_v4i16; break;
    case MVT::v8i8:  opcode = SHAVE::CMU_SHLIV_x8_v8i8; break;
    case MVT::v2f32: opcode = SHAVE::CMU_SHLIV_x32_v2i32; break;
    case MVT::v4f16: opcode = SHAVE::CMU_SHLIV_x16_v4i16; break;
    // 32 bit vectors
    case MVT::v2i16: opcode = SHAVE::CMU_SHLIV_x16_v2i16; break;
    case MVT::v4i8:  opcode = SHAVE::CMU_SHLIV_x8_v4i8; break;
    case MVT::v2f16: opcode = SHAVE::CMU_SHLIV_x16_v2i16; break;
    // 16 bit vectors
    case MVT::v2i8:  opcode = SHAVE::CMU_SHLIV_x8_v2i8; break;
    }

    SmallVector<SDValue, 2> ops;
    ops.push_back(SDValue(result, 0));
    ops.push_back(N->getOperand(NumOperands - i - 1));

    result = getMachineNodeWithDefaultOps(opcode, dbgLoc, VecVT, ops);
  }

  ReplaceNode(N, result);
}

SDNode *SHAVEISelDAGtoDAG::getLastCopyFromReg(SDNode *N) {
  for(SDNode::use_iterator it = N->use_begin(); it != N->use_end(); it++)
    if(it->getOperand(it.getOperandNo()).getValueType() == MVT::Other
          && it->getOpcode() == ISD::CopyFromReg)
    return getLastCopyFromReg(*it);

  return N;
}

void SHAVEISelDAGtoDAG::fixExtensionNode(SelectionDAG::allnodes_iterator &nodeIt) {
  SDValue operand = nodeIt->getOperand(0);
  SDNode *N = &(*nodeIt); // FIXME: Movidius - I always get nervous about expression like '&*'; it usually means trouble

  if (nodeIt->getValueType(0) == operand.getValueType()) {
    // This extension is pointless...
    nodeIt++;
    ReplaceUses(N, operand.getNode());
    CurDAG->RemoveDeadNode(N);
  }
}

void SHAVEISelDAGtoDAG::PostprocessISelDAG() {
  //  DEBUG(CurDAG->viewGraph());
  // CurDAG->viewGraph("PostSelection DAG");
}

void SHAVEISelDAGtoDAG::PreprocessISelDAG() {
  // iterate over DAG nodes and fix what's needed to preserve correctness
  for (SelectionDAG::allnodes_iterator nodeIt = CurDAG->allnodes_begin(); nodeIt != CurDAG->allnodes_end(); nodeIt++) {
    if ((nodeIt->getOpcode() == ISD::ZERO_EXTEND)
                  || (nodeIt->getOpcode() == ISD::SIGN_EXTEND)
                  || (nodeIt->getOpcode() == ISD::ANY_EXTEND)) {
      // FIXME: Movidius - Do we need to remove these nodes?
      // fix integer extensions
      fixExtensionNode(nodeIt);
    }
  }
}

void SHAVEISelDAGtoDAG::SelectIndexedLOAD(SDNode *N) {
  SmallVector<SDValue, 4> ops;

  // Fetch the chain
  SDValue chain = N->getOperand(0);
  LoadSDNode *loadNode = cast<LoadSDNode>(N);

  // Get a list of the operands of the node skipping the chain and the offset
  for (unsigned opIdx = 1; opIdx < N->getNumOperands() - 1; opIdx++)
    ops.push_back(N->getOperand(opIdx));

  // fetch the in-memory type
  MVT::SimpleValueType valType = cast<LoadSDNode>(N)->getMemoryVT().getSimpleVT().SimpleTy;

  // choose the opcode depending on the value type to be loaded
  unsigned opcode = 0;

  if (loadNode->getAddressingMode() == ISD::POST_INC) {
    // check if the load is to be w/o extension
    // FIXME: Movidius - There's something fishy about the instruction selection here
    if (loadNode->getExtensionType() == ISD::NON_EXTLOAD) {
      switch (valType) {
      case MVT::i8:    opcode = SHAVE::LSU_LDI_x8_Raw;  break;
      case MVT::i16:   opcode = SHAVE::LSU_LDI_x16_Raw; break;
      case MVT::i32:   opcode = SHAVE::LSU_LDI_x32_Raw; break;
      case MVT::v4i16: opcode = SHAVE::LSU_LDI64_l_Raw; break;
      case MVT::v4f16: opcode = SHAVE::LSU_LDI64_l_Raw; break;
      case MVT::v8i8:  opcode = SHAVE::LSU_LDI64_l_Raw; break;
      case MVT::v4i8:  opcode = SHAVE::LSU_LDI_x32_Raw; break;

      // FIXME: Movidius - Not 100% sure what to do with 'f16' and 'f32' on Myriad2, does non-extending load mean anything for FP?
      case MVT::f16:   opcode = SHAVE::LSU_LDI_x16_Raw; break;
      case MVT::f32:   opcode = SHAVE::LSU_LDI_x32_Raw; break;

      default:
        llvm_unreachable("Cannot select post-incrementing load!");
      }
    } else if (loadNode->getExtensionType() == ISD::SEXTLOAD) {
      // FIXME: Movidius - There's something fishy about the instruction selection here
      switch (valType) {
      case MVT::i8:   opcode = SHAVE::LSU_LDI32_i8_i32_Raw;  break;
      case MVT::i16:  opcode = SHAVE::LSU_LDI32_i16_i32_Raw; break;

      // These are the only vector ext loads allowed by isVectorLoadExtDesirable
      case MVT::v4i16: opcode = SHAVE::LSU_LDI128_i16_i32_Raw; break;
      case MVT::v8i8:  opcode = SHAVE::LSU_LDI128_i8_i16_Raw; break;
      case MVT::v4i8:  opcode = SHAVE::LSU_LDI128_i8_i32_Raw; break;

      default:
        llvm_unreachable("Cannot select post-incrementing load!");
      }
    } else if ((loadNode->getExtensionType() == ISD::ZEXTLOAD) || (loadNode->getExtensionType() == ISD::EXTLOAD)) {
      // FIXME: Movidius - There's something fishy about the instruction selection here
      switch (valType) {
      case MVT::i8:   opcode = SHAVE::LSU_LDI32_u8_u32_Raw;  break;
      case MVT::i16:  opcode = SHAVE::LSU_LDI32_u16_u32_Raw; break;
      case MVT::f16:  opcode = SHAVE::LSU_LDI32_f16_f32_Raw; break;

      // These are the only vector ext loads allowed by isVectorLoadExtDesirable
      case MVT::v4i16: opcode = SHAVE::LSU_LDI128_u16_u32_Raw; break;
      case MVT::v8i8:  opcode = SHAVE::LSU_LDI128_u8_u16_Raw; break;
      case MVT::v4i8:  opcode = SHAVE::LSU_LDI128_u8_u32_Raw; break;

      case MVT::v4f16: opcode = SHAVE::LSU_LDI128_f16_f32_Raw; break;

      default:
        llvm_unreachable("Cannot select post-incrementing load!");
      }
    }
  } else
    llvm_unreachable("Cannot select indexed load!");

  // select the node to the chosen opcode
  SelectNodeWithDefaultOps(N, opcode, N->getVTList(), ops, chain);
}


namespace {
  void GetIntrinsicInfo(unsigned intrinsicID, int &firstImmIdx, int &lastImmIdx) {
    // Initialise the intrinsic indices
    firstImmIdx = -1;
    lastImmIdx = -1;

    // Handle each intrinsic
    switch (intrinsicID) {
    // BRU.SWIC/SWIH
    case Intrinsic::shave_bru_swih_i:
    case Intrinsic::shave_bru_swic_i:
      firstImmIdx = 0;
      lastImmIdx = 0;
      break;
    // CMU.ALIGNVEC/VNZ
    case Intrinsic::shave_cmu_alignvec_rri_float4:
    case Intrinsic::shave_cmu_alignvec_rri_half8:
    case Intrinsic::shave_cmu_alignvec_rri_int4:
    case Intrinsic::shave_cmu_alignvec_rri_short8:
    case Intrinsic::shave_cmu_alignvec_rri_char16:
    case Intrinsic::shave_cmu_alignvec_rri_float16:
    case Intrinsic::shave_cmu_alignvec_rri_half32:
    case Intrinsic::shave_cmu_alignvec_rri_int16:
    case Intrinsic::shave_cmu_alignvec_rri_short32:
    case Intrinsic::shave_cmu_alignvec_rri_char64:
    case Intrinsic::shave_cmu_vnz_x32_rri_int4:
    case Intrinsic::shave_cmu_vnz_x32_rri_uint4:
    case Intrinsic::shave_cmu_vnz_x16_rri_short8:
    case Intrinsic::shave_cmu_vnz_x16_rri_ushort8:
    case Intrinsic::shave_cmu_vnz_x8_rri_char16:
    case Intrinsic::shave_cmu_vnz_x8_rri_uchar16:
      firstImmIdx = 2;
      lastImmIdx = 2;
      break;
    // CMU.VSZM
    case Intrinsic::shave_cmu_vszmword_riiii_int4:
    case Intrinsic::shave_cmu_vszmword_riiii_float4:
    case Intrinsic::shave_cmu_vszmword_riiii_short8:
    case Intrinsic::shave_cmu_vszmword_riiii_char16:
    case Intrinsic::shave_cmu_vszmbyte_riiii_int4:
    case Intrinsic::shave_cmu_vszmbyte_riiii_short8:
    case Intrinsic::shave_cmu_vszmbyte_riiii_char16:
      firstImmIdx = 1;
      lastImmIdx = 4;
      break;
    // IAU.ADDSI/ADDSU
    case Intrinsic::shave_iau_addsi_ri:
    case Intrinsic::shave_iau_addsu_ri:
      firstImmIdx = 1;
      lastImmIdx = 1;
      break;
    // IAU.ALIGN
    case Intrinsic::shave_iau_align_rri:
      firstImmIdx = 2;
      lastImmIdx = 2;
      break;
    // IAU.FEXTI/FEXTU
    case Intrinsic::shave_iau_fexti_rii:
    case Intrinsic::shave_iau_fextu_rii:
      firstImmIdx = 1;
      lastImmIdx = 2;
      break;
    // IAU.FINS
    case Intrinsic::shave_iau_fins_rrii:
      firstImmIdx = 2;
      lastImmIdx = 3;
      break;
    // IAU/SAU/VAU register-immediate instructions
    case Intrinsic::shave_iau_mulsi_ri:
    case Intrinsic::shave_iau_mulsu_ri:
    case Intrinsic::shave_iau_subsi_ri:
    case Intrinsic::shave_iau_subsu_ri:
    case Intrinsic::shave_sau_exp2_f16_ri:
    case Intrinsic::shave_sau_tanh_f16_ri:
    case Intrinsic::shave_sau_rol_x32_ri:
    case Intrinsic::shave_sau_rol_x16_ri:
    case Intrinsic::shave_sau_rol_x8_ri:
    case Intrinsic::shave_sau_shl_v2x16_ri:
    case Intrinsic::shave_sau_shl_v4x8_ri:
    case Intrinsic::shave_sau_shr_i32_ri:
    case Intrinsic::shave_sau_shr_u32_ri:
    case Intrinsic::shave_sau_shr_i16_ri:
    case Intrinsic::shave_sau_shr_u16_ri:
    case Intrinsic::shave_sau_shr_i8_ri:
    case Intrinsic::shave_sau_shr_u8_ri:
    case Intrinsic::shave_sau_shr_v2i16_ri:
    case Intrinsic::shave_sau_shr_v2u16_ri:
    case Intrinsic::shave_sau_shr_v4i8_ri:
    case Intrinsic::shave_sau_shr_v4u8_ri:
    case Intrinsic::shave_sau_sqt_f16_ri:
    case Intrinsic::shave_sau_onesx_x128_ri:
    case Intrinsic::shave_sau_onesx_x64_ri:
    case Intrinsic::shave_sau_onesx_x32_ri:
    case Intrinsic::shave_sau_zerosx_x128_ri:
    case Intrinsic::shave_sau_zerosx_x64_ri:
    case Intrinsic::shave_sau_zerosx_x32_ri:
    case Intrinsic::shave_sau_sumfx_f32_ri_float16:
    case Intrinsic::shave_sau_sumfx_f16_ri_half32:
    case Intrinsic::shave_sau_logv_ri:
    case Intrinsic::shave_sau_sigmv_ri:
    case Intrinsic::shave_sau_tanhv_ri:
    case Intrinsic::shave_sau_expv_ri:
    case Intrinsic::shave_sau_exp2v_ri:
    case Intrinsic::shave_sau_sqtv_ri:
    case Intrinsic::shave_vau_add_i32_ri_int16:
    case Intrinsic::shave_vau_add_i16_ri_short32:
    case Intrinsic::shave_vau_add_i8_ri_char64:
    case Intrinsic::shave_vau_add_i32_ri:
    case Intrinsic::shave_vau_add_i16_ri:
    case Intrinsic::shave_vau_add_i8_ri:
    case Intrinsic::shave_vau_iadds_i32_ri:
    case Intrinsic::shave_vau_iadds_u32_ri:
    case Intrinsic::shave_vau_iadds_i16_ri:
    case Intrinsic::shave_vau_iadds_u16_ri:
    case Intrinsic::shave_vau_iadds_i8_ri:
    case Intrinsic::shave_vau_iadds_u8_ri:
    case Intrinsic::shave_vau_imuls_i32_ri:
    case Intrinsic::shave_vau_imuls_u32_ri:
    case Intrinsic::shave_vau_imuls_i16_ri:
    case Intrinsic::shave_vau_imuls_u16_ri:
    case Intrinsic::shave_vau_imuls_i8_ri:
    case Intrinsic::shave_vau_imuls_u8_ri:
    case Intrinsic::shave_vau_isubs_i32_ri:
    case Intrinsic::shave_vau_isubs_u32_ri:
    case Intrinsic::shave_vau_isubs_i16_ri:
    case Intrinsic::shave_vau_isubs_u16_ri:
    case Intrinsic::shave_vau_isubs_i8_ri:
    case Intrinsic::shave_vau_isubs_u8_ri:
    case Intrinsic::shave_vau_mul_i32_ri:
    case Intrinsic::shave_vau_mul_i16_ri:
    case Intrinsic::shave_vau_mul_i8_ri:
    case Intrinsic::shave_vau_rol_x32_ri:
    case Intrinsic::shave_vau_rol_x16_ri:
    case Intrinsic::shave_vau_rol_x8_ri:
    case Intrinsic::shave_vau_shd_i32_ri:
    case Intrinsic::shave_vau_shd_i16_ri:
    case Intrinsic::shave_vau_shd_i8_ri:
    case Intrinsic::shave_vau_shl_x32_ri:
    case Intrinsic::shave_vau_shl_x16_ri:
    case Intrinsic::shave_vau_shl_x8_ri:
    case Intrinsic::shave_vau_shr_i32_ri:
    case Intrinsic::shave_vau_shr_u32_ri:
    case Intrinsic::shave_vau_shr_i16_ri:
    case Intrinsic::shave_vau_shr_u16_ri:
    case Intrinsic::shave_vau_shr_i8_ri:
    case Intrinsic::shave_vau_shr_u8_ri:
    case Intrinsic::shave_vau_sub_i32_ri:
    case Intrinsic::shave_vau_sub_i16_ri:
    case Intrinsic::shave_vau_sub_i8_ri:
    case Intrinsic::shave_vau_add_i32_ri_int2:
    case Intrinsic::shave_vau_add_i16_ri_short4:
    case Intrinsic::shave_vau_add_i8_ri_schar8:
    case Intrinsic::shave_vau_mul_i32_ri_int2:
    case Intrinsic::shave_vau_mul_i16_ri_short4:
    case Intrinsic::shave_vau_mul_i8_ri_schar8:
    case Intrinsic::shave_vau_rol_x32_ri_uint2:
    case Intrinsic::shave_vau_rol_x16_ri_ushort4:
    case Intrinsic::shave_vau_rol_x8_ri_uchar8:
    case Intrinsic::shave_vau_shd_i32_ri_int2:
    case Intrinsic::shave_vau_shd_i16_ri_short4:
    case Intrinsic::shave_vau_shd_i8_ri_schar8:
    case Intrinsic::shave_vau_shl_x32_ri_int2:
    case Intrinsic::shave_vau_shl_x16_ri_short4:
    case Intrinsic::shave_vau_shl_x8_ri_schar8:
    case Intrinsic::shave_vau_shr_i32_ri_int2:
    case Intrinsic::shave_vau_shr_u32_ri_int2:
    case Intrinsic::shave_vau_shr_i16_ri_short4:
    case Intrinsic::shave_vau_shr_u16_ri_short4:
    case Intrinsic::shave_vau_shr_i8_ri_schar8:
    case Intrinsic::shave_vau_shr_u8_ri_schar8:
    case Intrinsic::shave_vau_sub_i32_ri_int2:
    case Intrinsic::shave_vau_sub_i16_ri_short4:
    case Intrinsic::shave_vau_sub_i8_ri_schar8:
    case Intrinsic::shave_vau_iadds_i32_ri_int16:
    case Intrinsic::shave_vau_iadds_u32_ri_uint16:
    case Intrinsic::shave_vau_iadds_i16_ri_short32:
    case Intrinsic::shave_vau_iadds_u16_ri_ushort32:
    case Intrinsic::shave_vau_iadds_i8_ri_char64:
    case Intrinsic::shave_vau_iadds_u8_ri_uchar64:
    case Intrinsic::shave_vau_imuls_i32_ri_int16:
    case Intrinsic::shave_vau_imuls_u32_ri_uint16:
    case Intrinsic::shave_vau_imuls_i16_ri_short32:
    case Intrinsic::shave_vau_imuls_u16_ri_ushort32:
    case Intrinsic::shave_vau_imuls_i8_ri_char64:
    case Intrinsic::shave_vau_imuls_u8_ri_uchar64:
    case Intrinsic::shave_vau_isubs_i32_ri_int16:
    case Intrinsic::shave_vau_isubs_u32_ri_uint16:
    case Intrinsic::shave_vau_isubs_i16_ri_short32:
    case Intrinsic::shave_vau_isubs_u16_ri_ushort32:
    case Intrinsic::shave_vau_isubs_i8_ri_char64:
    case Intrinsic::shave_vau_isubs_u8_ri_uchar64:
    case Intrinsic::shave_vau_mul_i32_ri_int16:
    case Intrinsic::shave_vau_mul_i16_ri_short32:
    case Intrinsic::shave_vau_mul_i8_ri_char64:
    case Intrinsic::shave_vau_rol_x32_ri_int16:
    case Intrinsic::shave_vau_rol_x16_ri_short32:
    case Intrinsic::shave_vau_rol_x8_ri_char64:
    case Intrinsic::shave_vau_shd_i32_ri_int16:
    case Intrinsic::shave_vau_shd_i16_ri_short32:
    case Intrinsic::shave_vau_shd_i8_ri_char64:
    case Intrinsic::shave_vau_shl_x32_ri_uint16:
    case Intrinsic::shave_vau_shl_x16_ri_ushort32:
    case Intrinsic::shave_vau_shl_x8_ri_uchar64:
    case Intrinsic::shave_vau_shr_i32_ri_int16:
    case Intrinsic::shave_vau_shr_u32_ri_uint16:
    case Intrinsic::shave_vau_shr_i16_ri_short32:
    case Intrinsic::shave_vau_shr_u16_ri_ushort32:
    case Intrinsic::shave_vau_shr_i8_ri_char64:
    case Intrinsic::shave_vau_shr_u8_ri_uchar64:
    case Intrinsic::shave_vau_sub_i32_ri_int16:
    case Intrinsic::shave_vau_sub_i16_ri_short32:
    case Intrinsic::shave_vau_sub_i8_ri_char64:
      firstImmIdx = 1;
      lastImmIdx = 1;
      break;
    // VAU.ALIGNBYTE
    case Intrinsic::shave_vau_alignbyte_rri_int4:
    case Intrinsic::shave_vau_alignbyte_rri_short8:
    case Intrinsic::shave_vau_alignbyte_rri_char16:
    case Intrinsic::shave_vau_alignbyte_rri_int16:
    case Intrinsic::shave_vau_alignbyte_rri_short32:
    case Intrinsic::shave_vau_alignbyte_rri_char64:
      firstImmIdx = 2;
      lastImmIdx = 2;
      break;
    // VAU.CMB
    case Intrinsic::shave_vau_cmbbyte_rriiii_int4:
    case Intrinsic::shave_vau_cmbbyte_rriiii_short8:
    case Intrinsic::shave_vau_cmbbyte_rriiii_char16:
    case Intrinsic::shave_vau_cmbword_rriiii_float4:
    case Intrinsic::shave_vau_cmbword_rriiii_int4:
    case Intrinsic::shave_vau_cmbword_rriiii_short8:
    case Intrinsic::shave_vau_cmbword_rriiii_char16:
      firstImmIdx = 2;
      lastImmIdx = 5;
      break;
    default:
      break;
    }
  }
} // End of anonymous namespace

void SHAVEISelDAGtoDAG::SelectIntrinsicGetcpuid(SDNode *N) {
  SDValue svidReg;

  // Before NPU4, the SHAVE ID were allocated linearly and stored in P_SVID TRF
  if (!hasFeature(SHAVE::HasLegacySHAVEIDs_Feature)) {
    // For NPU4, the cpu id for the activation shaves is calculated as below
    // tile_id * shave_num + shave_inst_id
    // where:
    //     tile_id       = Tile Number the SHAVE is assigned to
    //     shave_num     = Number of SHAVES in one NCE Tile
    //     shave_inst_id = current SHAVE's ID within the Tile

    struct {
      unsigned tileIdStartPos, tileIdBits;
      unsigned shaveNumStartPos, shaveNumBits;
      unsigned shaveInstIdStartPos, shaveInstIdBits;
    } shaveDesc[2] = {
      { 5, 3, 10, 2, 4, 1 }, // 2 SHAVEs per Tile
      { 5, 3,  9, 3, 3, 2 }  // 4 SHAVEs per Tile
    };
    const unsigned descIdx = hasFeature(SHAVE::Has_4_SHAVEs_per_tile);

    // Ensure that the SHAVES per TILE count has been set in the processor model
    assert(Subtarget.getNumShavesPerTile() && "Unexpected number of SHAVEs per tile");

    svidReg = CurDAG->getRegister(SHAVE::P_ID_Myr4, MVT::i32);

    SDLoc dbgLoc(N);
    SmallVector<SDValue, 1> ops;
    ops.clear();
    ops.push_back(svidReg);

    SDNode *cptiNode = getMachineNodeWithDefaultOps(SHAVE::CMU_CPTI, dbgLoc,
						    N->getVTList(), ops);
    ops.clear();
    ops.push_back(SDValue(cptiNode,0));
    ops.push_back(CurDAG->getTargetConstant(shaveDesc[descIdx].tileIdStartPos, dbgLoc, MVT::i32));
    ops.push_back(CurDAG->getTargetConstant(shaveDesc[descIdx].tileIdBits, dbgLoc, MVT::i32));
    SDNode *tile_id =
      getMachineNodeWithDefaultOps(SHAVE::IAU_FEXTU_imm, dbgLoc, MVT::i32, ops);

    ops.clear();
    ops.push_back(SDValue(cptiNode,0));
    ops.push_back(CurDAG->getTargetConstant(shaveDesc[descIdx].shaveNumStartPos, dbgLoc, MVT::i32));
    ops.push_back(CurDAG->getTargetConstant(shaveDesc[descIdx].shaveNumBits, dbgLoc, MVT::i32));
    SDNode *shave_num =
      getMachineNodeWithDefaultOps(SHAVE::IAU_FEXTU_imm, dbgLoc, MVT::i32, ops);

    ops.clear();
    ops.push_back(SDValue(cptiNode,0));
    ops.push_back(CurDAG->getTargetConstant(shaveDesc[descIdx].shaveInstIdStartPos, dbgLoc, MVT::i32));
    ops.push_back(CurDAG->getTargetConstant(shaveDesc[descIdx].shaveInstIdBits, dbgLoc, MVT::i32));
    SDNode *shave_inst_id =
      getMachineNodeWithDefaultOps(SHAVE::IAU_FEXTU_imm, dbgLoc, MVT::i32, ops);

    ops.clear();
    ops.push_back(SDValue(tile_id,0));
    ops.push_back(SDValue(shave_num,0));
    SDNode *result =
      getMachineNodeWithDefaultOps(SHAVE::IAU_MUL_32, dbgLoc, MVT::i32, ops);

    ops.clear();
    ops.push_back(SDValue(result,0));
    ops.push_back(SDValue(shave_inst_id,0));

    result = getMachineNodeWithDefaultOps(SHAVE::IAU_ADD_32, dbgLoc,
					  MVT::i32, ops);

    ReplaceNode(N,result);
  } else {
    svidReg = CurDAG->getRegister(SHAVE::P_SVID, MVT::i32);

    SDLoc dbgLoc(N);
    SmallVector<SDValue, 1> ops;
    ops.clear();
    ops.push_back(svidReg);

    SDNode *cptiNode = getMachineNodeWithDefaultOps(SHAVE::CMU_CPTI, dbgLoc,
						    N->getVTList(), ops);

    ops.clear();
    ops.push_back(SDValue(cptiNode, 0));
    ops.push_back(SDValue(getLDImm32Node(CurDAG, MVT::i32, dbgLoc, 0x1f), 0));

    SDNode *result =
      getMachineNodeWithDefaultOps(SHAVE::IAU_AND_32, dbgLoc, MVT::i32, ops);

    ReplaceNode(N, result);
  }
}

void SHAVEISelDAGtoDAG::SelectIntrinsicTailWriteback(SDNode *N) {
  SDLoc dbgLoc(N);

  const SDValue &chain = N->getOperand(0);
  const SDValue &value = N->getOperand(2);
  const SDValue &pointer = N->getOperand(3);
  const SDValue &size = N->getOperand(4);

  MVT valueType = value.getValueType().getSimpleVT();

  MVT maskType = MVT::INVALID_SIMPLE_VALUE_TYPE;
  switch (valueType.getScalarSizeInBits()) {
  case 32u: maskType = MVT::v16i32; break;
  case 16u: maskType = MVT::v32i16; break;
  case 8u:  maskType = MVT::v64i8;  break;
  }
  assert(maskType != MVT::INVALID_SIMPLE_VALUE_TYPE);

  SmallVector<SDValue, 3u> ops;
  ops.push_back(CurDAG->getTargetConstant(1, dbgLoc, MVT::i16));
  SDValue one = SDValue(getMachineNodeWithDefaultOps(SHAVE::LSU_LDIL, dbgLoc, MVT::i32, ops), 0);

  // Generate IRF mask
  SDNode *irfMask = nullptr;
  if (maskType.getVectorElementType().getSizeInBits() > 8u) {
    // For 32-bit and 16-bit element types, the mask is (1 << size) - 1
    ops.clear();
    ops.push_back(one);
    ops.push_back(size);
    SDNode *shift = getMachineNodeWithDefaultOps(SHAVE::IAU_SHL_i32, dbgLoc, MVT::i32, ops); // 1 << size

    ops.clear();
    ops.push_back(SDValue(shift, 0));
    ops.push_back(CurDAG->getTargetConstant(1, dbgLoc, MVT::i32));
    irfMask = getMachineNodeWithDefaultOps(SHAVE::IAU_SUB_32_imm, dbgLoc, MVT::i32, ops); // (1 << size) - 1
  }
  else {
    // For 8-bit element types, the mask is (1 << size) - 1 but split over two IRFs as there are 64 elements (and therefore 64-bits)
    ops.clear();
    ops.push_back(size);
    ops.push_back(CurDAG->getTargetConstant(32, dbgLoc, MVT::i32));
    SDNode *upperSize = getMachineNodeWithDefaultOps(SHAVE::IAU_SUBSU_imm, dbgLoc, MVT::i32, ops); // size - 32 (saturated so the result is 0 when size < 32)

    ops.clear();
    ops.push_back(one);
    ops.push_back(SDValue(upperSize, 0));
    SDNode *upperBase = getMachineNodeWithDefaultOps(SHAVE::IAU_SHL_i32, dbgLoc, MVT::i32, ops); // 1 << upperSize

    ops.clear();
    ops.push_back(SDValue(upperBase, 0));
    ops.push_back(CurDAG->getTargetConstant(1, dbgLoc, MVT::i32));
    SDNode *upperMask = getMachineNodeWithDefaultOps(SHAVE::IAU_SUB_32_imm, dbgLoc, MVT::i32, ops); // (1 << upperSize) - 1;

    ops.clear();
    ops.push_back(size);
    ops.push_back(SDValue(upperSize, 0));
    SDNode *lowerSize = getMachineNodeWithDefaultOps(SHAVE::IAU_SUB_32, dbgLoc, MVT::i32, ops); // size - upperSize

    // If the size is greater than 32 then lowerSize is 32, which will not shift the 1 at all
    // In this case we produce a predicated XOR to zero the register so the -1 will set all bits
    ops.clear();
    ops.push_back(size);
    ops.push_back(CurDAG->getTargetConstant(32, dbgLoc, MVT::i32));
    SDNode* cmpi = getMachineNodeWithDefaultOps(SHAVE::CMU_CMPI_Raw, dbgLoc, MVT::Glue, ops);

    ops.clear();
    ops.push_back(one);
    ops.push_back(CurDAG->getTargetConstant(SHAVECC::GTE, dbgLoc, MVT::i8));
    ops.push_back(CurDAG->getRegister(SHAVE::CC_CMU0, MVT::i8));
    ops.push_back(SDValue(cmpi, 0)); // chain
    SDValue shiftBase = SDValue(CurDAG->getMachineNode(SHAVE::SAU_XOR_tied, dbgLoc, MVT::i32, ops), 0);

    ops.clear();
    ops.push_back(shiftBase);
    ops.push_back(SDValue(lowerSize, 0));
    SDNode *lowerBase = getMachineNodeWithDefaultOps(SHAVE::IAU_SHL_i32, dbgLoc, MVT::i32, ops); // 1 << lowerSize

    ops.clear();
    ops.push_back(SDValue(lowerBase, 0));
    ops.push_back(CurDAG->getTargetConstant(1, dbgLoc, MVT::i32));
    SDNode *lowerMask = getMachineNodeWithDefaultOps(SHAVE::IAU_SUB_32_imm, dbgLoc, MVT::i32, ops); // (1 << size) - 1

    irfMask = CurDAG->getMachineNode(SHAVE::IMPLICIT_DEF, dbgLoc, MVT::i64);
    irfMask = CurDAG->getTargetInsertSubreg(SHAVE::vsub32_0, dbgLoc, MVT::i64, SDValue(irfMask, 0), SDValue(lowerMask, 0)).getNode();
    irfMask = CurDAG->getTargetInsertSubreg(SHAVE::vsub32_1, dbgLoc, MVT::i64, SDValue(irfMask, 0), SDValue(upperMask, 0)).getNode();
  }
  assert(irfMask != nullptr);

  // Convert to VRF mask
  unsigned int splatOpcode = SHAVE::PHI;
  SDNode * splatSource = nullptr;
  switch(maskType.SimpleTy) {
  case MVT::v16i32:
    splatOpcode = SHAVE::CMU_CPR_v16i32_splat;
    splatSource = one.getNode();
    break;
  case MVT::v32i16:
    splatOpcode = SHAVE::CMU_CPR_v32i16_splat;
    splatSource = CurDAG->getTargetExtractSubreg(SHAVE::hsub_0, dbgLoc, MVT::i16, one).getNode();
    break;
  case MVT::v64i8:
    splatOpcode = SHAVE::CMU_CPR_v64i8_splat;
    splatSource = CurDAG->getTargetExtractSubreg(SHAVE::qsub_0, dbgLoc, MVT::i16, one).getNode();
    break;
  default:
    llvm_unreachable("Unsupported mask type for splat in tail-writeback lowering");
  }

  ops.clear();
  ops.push_back(SDValue(splatSource, 0));
  SDNode *maskBase = getMachineNodeWithDefaultOps(splatOpcode, dbgLoc, maskType, ops); // SPLAT 1

  unsigned int expandOpcode = SHAVE::PHI;
  switch (maskType.SimpleTy) {
  case MVT::v16i32: expandOpcode = SHAVE::CMU_EXPAND_x32_VRF512; break;
  case MVT::v32i16: expandOpcode = SHAVE::CMU_EXPAND_x16_VRF512; break;
  case MVT::v64i8:  expandOpcode = SHAVE::CMU_EXPAND_x8_VRF512;  break;
  default: llvm_unreachable("Unsupported mask type for expand in tail-writeback lowering");
  }

  ops.clear();
  ops.push_back(SDValue(maskBase, 0));
  ops.push_back(SDValue(irfMask, 0));
  SDNode *mask = getMachineNodeWithDefaultOps(expandOpcode, dbgLoc, maskType, ops); // CMU.EXPAND (SPLAT 1) (mask)

  // Compare mask with zero
  unsigned int cmzOpcode = SHAVE::PHI;
  switch (maskType.SimpleTy) {
  case MVT::v16i32: cmzOpcode = SHAVE::CMU_CMZ_v16i32; break;
  case MVT::v32i16: cmzOpcode = SHAVE::CMU_CMZ_v32i16; break;
  case MVT::v64i8:  cmzOpcode = SHAVE::CMU_CMZ_v64i8;  break;
  default: llvm_unreachable("Unsupported mask type for cmz in tail-writeback lowering");
  }

  ops.clear();
  ops.push_back(SDValue(mask, 0));
  SDNode *cmzLow = getMachineNodeWithDefaultOps(cmzOpcode, dbgLoc, MVT::Glue, ops);

  // Predicated store for low-half
  SDValue shaveCC = CurDAG->getTargetConstant(SHAVECC::NEQ, dbgLoc, MVT::i8);
  unsigned int ccRegister = SHAVE::NoRegister;
  switch (valueType.getScalarSizeInBits()) {
  case 32u: ccRegister = SHAVE::C_CMU_0_3;  break;
  case 16u: ccRegister = SHAVE::C_CMU0;     break;
  case 8u:  ccRegister = SHAVE::C_CMU_0_15; break;
  }
  assert(ccRegister != SHAVE::NoRegister);

  SDValue ccReg = CurDAG->getRegister(ccRegister, MVT::i8);

  MachineSDNode *storeLow = CurDAG->getMachineNode(SHAVE::MASKED_STORE_WVRF_l, dbgLoc, MVT::Other, { value, pointer, shaveCC, ccReg, chain, SDValue(cmzLow, 0) });

  // New compare with zero for high half
  ops.clear();
  ops.push_back(SDValue(mask, 0));
  ops.push_back(CurDAG->getTargetConstant(0, dbgLoc, MVT::i32));
  ops.push_back(CurDAG->getTargetConstant(1, dbgLoc, MVT::i32));
  SDNode* alignedMask = getMachineNodeWithDefaultOps(SHAVE::CMU_CPVV_256_extract_512_Myr4, dbgLoc, maskType, ops); // CMU.CP.256 alignedMask.0 mask.1

  ops.clear();
  ops.push_back(SDValue(alignedMask, 0));
  SDNode* cmzHigh = getMachineNodeWithDefaultOps(cmzOpcode, dbgLoc, MVT::Glue, ops);

  // Predicated store for high-half
  MachineSDNode* storeHigh = CurDAG->getMachineNode(SHAVE::MASKED_STORE_WVRF_h, dbgLoc, MVT::Other, { value, pointer, shaveCC, ccReg, SDValue(storeLow, 0), SDValue(cmzHigh, 0) });

  ReplaceNode(N, storeHigh);
}

namespace {
void UpdateDP4AUsage(unsigned Opc, MachineFunction &MF) {
  SHAVEMachineFunctionInfo *SMFI = MF.getInfo<SHAVEMachineFunctionInfo>();
  if (SHAVEConflicts::check_isDP4A(Opc)) {
    if (SHAVEConflicts::check_usesSAU(Opc)) {
      SMFI->setSAUDP4AUsage(true);
    }
    if (SHAVEConflicts::check_usesVAU(Opc)) {
      SMFI->setVAUDP4AUsage(true);
    }
  }
}
} // End of Anonymous namespace

void SHAVEISelDAGtoDAG::SelectIntrinsic(SDNode *N) {
  unsigned OpIdx = 0;
  SDValue Chain;

  switch (N->getOpcode()) {
  default:
    llvm_unreachable("Unrecognised intrinsic opcode during selection");
  case ISD::INTRINSIC_VOID:
    Chain = N->getOperand(OpIdx++);
    break;
  case ISD::INTRINSIC_W_CHAIN:
    Chain = N->getOperand(OpIdx++);
    break;
  case ISD::INTRINSIC_WO_CHAIN:
    break;
  }

  // Determine the new opcode for the instruction.
  SmallVector<SDValue, 5> Ops;
  unsigned intrinsicID = N->getConstantOperandVal(OpIdx++);
  unsigned Opc = 0;

  // Handle custom instrinsics
  switch (intrinsicID) {
  case Intrinsic::shave_getcpuid: {
    SelectIntrinsicGetcpuid(N);
    return;
  }
  case Intrinsic::shave_tail_writeback_v16f32:
  case Intrinsic::shave_tail_writeback_v32f16:
  case Intrinsic::shave_tail_writeback_v16i32:
  case Intrinsic::shave_tail_writeback_v32i16:
  case Intrinsic::shave_tail_writeback_v64i8:
  case Intrinsic::shave_tail_writeback_v16u32:
  case Intrinsic::shave_tail_writeback_v32u16:
  case Intrinsic::shave_tail_writeback_v64u8:
    SelectIntrinsicTailWriteback(N);
    return;
  case Intrinsic::shave_setP_CFG: {
      Opc = SHAVE::CMU_CPIT;

      // Copy the argument from the '__builtin_shave_setP_CFG' interface
      assert((N->getNumOperands() == 3) && "Incorrect number of arguments provided to '__builtin_shave_setP_CFG");

      SDValue pcfgReg = CurDAG->getRegister(SHAVE::P_CFG, MVT::i32);
      Ops.push_back(pcfgReg);

      for (unsigned opIdx = OpIdx; opIdx < N->getNumOperands(); opIdx++) {
        SDValue Op = N->getOperand(opIdx);

        Ops.push_back(Op);
      }

    }
    break;
  case Intrinsic::shave_getP_CFG: {
      Opc = SHAVE::CMU_CPTI;

      // Copy the argument from the '__builtin_shave_getP_CFG' interface
      assert((N->getNumOperands() == 2) && "Too many arguments provided to '__builtin_shave_getP_CFG");

      for (unsigned opIdx = OpIdx; opIdx < N->getNumOperands(); opIdx++) {
        SDValue Op = N->getOperand(opIdx);

        Ops.push_back(Op);
      }

      SDValue pcfgReg = CurDAG->getRegister(SHAVE::P_CFG, MVT::i32);
      Ops.push_back(pcfgReg);
    }
    break;

  default: {
      const SHAVEInstrInfo *SII = static_cast<SHAVETargetMachine&>(TM).getSubtargetImpl()->getInstrInfo();
      Opc = SII->getISAIntrinsicOpcode(intrinsicID);

      if (!Opc) {
        bool hasInputChain = N->getOperand(0).getValueType() == MVT::Other;
        unsigned iid = N->getConstantOperandVal(hasInputChain);

        std::string errorMessageStr;
        raw_string_ostream errorMessage(errorMessageStr);
        errorMessage << "SHAVE Intrinsic: " << TII->getName(iid) << " not supported on this SHAVE variant";
        report_fatal_error(Twine(errorMessage.str()));
      }

      // Determine if the intrinsic has immediate operands or not.
      int firstImmIdx = -1;
      int lastImmIdx = -1;

      GetIntrinsicInfo(intrinsicID, firstImmIdx, lastImmIdx);

      // Fetch the operands, translating immediates to target immediates.
      bool hasImm = (firstImmIdx >= 0) && (lastImmIdx >= firstImmIdx);
      int argIdx = 0;

      for (unsigned opIdx = OpIdx; opIdx < N->getNumOperands(); opIdx++, argIdx++) {
        SDValue Op = N->getOperand(opIdx);

        if (hasImm && (argIdx >= firstImmIdx) && (argIdx <= lastImmIdx)) {
          ConstantSDNode *Const = dyn_cast<ConstantSDNode>(Op.getNode());

          if (!Const) {
            errs() << "error: could not translate intrinsic operand ";
            errs() << opIdx << " (expected constant) for node:\n";
#ifndef NDEBUG
            N->dump(CurDAG);
#endif // NDEBUG
            llvm_unreachable("Invalid operand type");
          }

          // Translate the constant to a target constant.
          uint64_t immVal = Const->getZExtValue();
          Op = CurDAG->getTargetConstant(immVal, SDLoc(N), Op.getValueType());
        }

        Ops.push_back(Op);
      }
    }
    break;
  }

  UpdateDP4AUsage(Opc, CurDAG->getMachineFunction());
  SelectNodeWithDefaultOps(N, Opc, N->getVTList(), Ops, Chain);
}

void SHAVEISelDAGtoDAG::SelectACCP_SEQ(SDNode *N) {
  SDLoc dbgLoc(N);
  EVT VT = N->getValueType(0);
  unsigned SDOpc = N->getOpcode();
  unsigned MIOpc = 0;

  if (SDOpc == SHAVEISD::ACCP_SEQ) {
    switch (VT.getSimpleVT().SimpleTy) {
    default:
      llvm_unreachable("Invalid element type.");
    case MVT::i8:
      MIOpc = SHAVE::SAU_ACCP_SEQ_i8;
      break;
    case MVT::i16:
      MIOpc = SHAVE::SAU_ACCP_SEQ_i16;
      break;
    case MVT::i32:
      MIOpc = SHAVE::SAU_ACCP_SEQ_i32;
      break;
    case MVT::f16:
      MIOpc = SHAVE::SAU_ACCP_SEQ_f16;
      break;
    case MVT::f32:
      MIOpc = SHAVE::SAU_ACCP_SEQ_f32;
      break;
    case MVT::v16i8:
      MIOpc = SHAVE::VAU_ACCP_SEQ_i8;
      break;
    case MVT::v8i16:
      MIOpc = SHAVE::VAU_ACCP_SEQ_i16;
      break;
    case MVT::v4i32:
      MIOpc = SHAVE::VAU_ACCP_SEQ_i32;
      break;
    case MVT::v8f16:
      MIOpc = SHAVE::VAU_ACCP_SEQ_f16;
      break;
    case MVT::v4f32:
      MIOpc = SHAVE::VAU_ACCP_SEQ_f32;
      break;
    case MVT::v64i8:
      MIOpc = SHAVE::VAU_ACCP_SEQ_i8_Myr4;
      break;
    case MVT::v32i16:
      MIOpc = SHAVE::VAU_ACCP_SEQ_i16_Myr4;
      break;
    case MVT::v16i32:
      MIOpc = SHAVE::VAU_ACCP_SEQ_i32_Myr4;
      break;
    case MVT::v32f16:
      MIOpc = SHAVE::VAU_ACCP_SEQ_f16_Myr4;
      break;
    case MVT::v16f32:
      MIOpc = SHAVE::VAU_ACCP_SEQ_f32_Myr4;
      break;
    }
  } else if (SDOpc == SHAVEISD::MACP_SEQ) {
    switch (VT.getSimpleVT().SimpleTy) {
    default:
      llvm_unreachable("Invalid element type.");
    case MVT::i8:
      MIOpc = SHAVE::SAU_MACP_SEQ_i8;
      break;
    case MVT::i16:
      MIOpc = SHAVE::SAU_MACP_SEQ_i16;
      break;
    case MVT::i32:
      MIOpc = SHAVE::SAU_MACP_SEQ_i32;
      break;
    case MVT::f16:
      MIOpc = SHAVE::SAU_MACP_SEQ_f16;
      break;
    case MVT::f32:
      MIOpc = SHAVE::SAU_MACP_SEQ_f32;
      break;
    case MVT::v16i8:
      MIOpc = SHAVE::VAU_MACP_SEQ_i8;
      break;
    case MVT::v8i16:
      MIOpc = SHAVE::VAU_MACP_SEQ_i16;
      break;
    case MVT::v4i32:
      MIOpc = SHAVE::VAU_MACP_SEQ_i32;
      break;
    case MVT::v8f16:
      MIOpc = SHAVE::VAU_MACP_SEQ_f16;
      break;
    case MVT::v4f32:
      MIOpc = SHAVE::VAU_MACP_SEQ_f32;
      break;
    case MVT::v64i8:
      MIOpc = SHAVE::VAU_MACP_SEQ_i8_Myr4;
      break;
    case MVT::v32i16:
      MIOpc = SHAVE::VAU_MACP_SEQ_i16_Myr4;
      break;
    case MVT::v16i32:
      MIOpc = SHAVE::VAU_MACP_SEQ_i32_Myr4;
      break;
    case MVT::v32f16:
      MIOpc = SHAVE::VAU_MACP_SEQ_f16_Myr4;
      break;
    case MVT::v16f32:
      MIOpc = SHAVE::VAU_MACP_SEQ_f32_Myr4;
      break;
    }
  }

  // Store the number of operands to operand(1).
  SmallVector<SDValue, 16> Ops;

  Ops.push_back(CurDAG->getTargetConstant(N->getNumOperands(), dbgLoc, MVT::i32));

  for (unsigned i = 0; i < N->getNumOperands(); i++)
    Ops.push_back(N->getOperand(i));

  SDNode * result = CurDAG->getMachineNode(MIOpc, dbgLoc, VT, Ops);

  ReplaceNode(N, result);
}

SDNode * SHAVEISelDAGtoDAG::SelectINSERT_VECTOR_ELT_VariableIndexImpl(SDValue vector, SDValue value, SDValue index, MVT vectorType, MVT valueType, SDLoc dbgLoc) {

  SmallVector<SDValue, 8> ops;
  bool is8bit = (valueType.SimpleTy == MVT::i8);
  unsigned opcode = 0;

  if (vectorType.getSizeInBits() == 32 || vectorType.getSizeInBits() == 16)
  {
    SDNode * posLen = nullptr;

    // Add the position specifier to posLen
    ops.clear();
    ops.push_back(index);
    // We need to multiply the index by 8 for 8 bit values, and by 16 for 16 bit values
    ops.push_back(SDValue(getLDImm32Node(CurDAG, MVT::i32, dbgLoc, 16 + (is8bit ? 3 : 4)), 0));
    posLen = getMachineNodeWithDefaultOps(SHAVE::IAU_SHL_i32, dbgLoc, MVT::i32, ops);

    // Add the length specifier to posLen
    ops.clear();
    ops.push_back(CurDAG->getTargetConstant(valueType.getSizeInBits(), dbgLoc, MVT::i16));
    SDNode * len = getMachineNodeWithDefaultOps(SHAVE::LSU_LDIL, dbgLoc, MVT::i32, ops);

    ops.clear();
    ops.push_back(SDValue(len, 0));
    ops.push_back(SDValue(posLen, 0));
    posLen = getMachineNodeWithDefaultOps(SHAVE::IAU_OR_32, dbgLoc, MVT::i32, ops);

    // Get the correct opcode for this bitsize and target
    if (valueType.getSizeInBits() == 16)
      opcode = SHAVE::IAU_FINSJ_i16_v2i16;
    else if (vectorType.getSizeInBits() == 32)
      opcode = SHAVE::IAU_FINSJ_i8_v4i8;
    else
      opcode = SHAVE::IAU_FINSJ_i8_v2i8;

    // Use IAU.FINSJ to insert vector element into IRF
    ops.clear();
    ops.push_back(vector);
    ops.push_back(value);
    ops.push_back(SDValue(posLen, 0));
    return getMachineNodeWithDefaultOps(opcode, dbgLoc, vectorType, ops);
  }

  // Find the right opcode
  if (hasFeature(SHAVE::HasVRF128_Feature)) {
    switch (vectorType.SimpleTy) {
    // i32 vector element insert
    case MVT::v4i32: opcode = SHAVE::CMU_LUTW_i32_v4i32_pseudo; break;
    case MVT::v2i32: opcode = SHAVE::CMU_LUTW_i32_v2i32_pseudo; break;

    // i16 vector element insert
    case MVT::v8i16: opcode = SHAVE::CMU_LUTW_i16_v8i16_pseudo; break;
    case MVT::v4i16: opcode = SHAVE::CMU_LUTW_i16_v4i16_pseudo; break;

    // i8 vector element insert
    case MVT::v16i8: opcode = SHAVE::CMU_LUTW_i8_v16i8_pseudo; break;
    case MVT::v8i8: opcode = SHAVE::CMU_LUTW_i8_v8i8_pseudo; break;

    // f32 vector element insert
    case MVT::v4f32: opcode = SHAVE::CMU_LUTW_f32_v4f32_pseudo; break;
    case MVT::v2f32: opcode = SHAVE::CMU_LUTW_f32_v2f32_pseudo; break;

    // f16 vector element insert
    case MVT::v8f16: opcode = SHAVE::CMU_LUTW_f16_v8f16_pseudo; break;
    case MVT::v4f16: opcode = SHAVE::CMU_LUTW_f16_v4f16_pseudo; break;

    default:
      llvm_unreachable("Invalid vector type found when selecting CMU.LUTW");
    }
  }
  else {
    switch (vectorType.SimpleTy) {
    // i32 vector element insert
    case MVT::v16i32: opcode = SHAVE::CMU_LUTW_i32_v16i32_Myr4_pseudo; break;
    case MVT::v8i32: opcode = SHAVE::CMU_LUTW_i32_v8i32_Myr4_pseudo; break;
    case MVT::v4i32: opcode = SHAVE::CMU_LUTW_i32_v4i32_Myr4_pseudo; break;
    case MVT::v2i32: opcode = SHAVE::CMU_LUTW_i32_v2i32_Myr4_pseudo; break;

    // i16 vector element insert
    case MVT::v32i16: opcode = SHAVE::CMU_LUTW_i16_v32i16_Myr4_pseudo; break;
    case MVT::v16i16: opcode = SHAVE::CMU_LUTW_i16_v16i16_Myr4_pseudo; break;
    case MVT::v8i16: opcode = SHAVE::CMU_LUTW_i16_v8i16_Myr4_pseudo; break;
    case MVT::v4i16: opcode = SHAVE::CMU_LUTW_i16_v4i16_Myr4_pseudo; break;
    case MVT::v2i16: opcode = SHAVE::CMU_LUTW_i16_v2i16_Myr4_pseudo; break;

    // i8 vector element insert
    case MVT::v64i8: opcode = SHAVE::CMU_LUTW_i8_v64i8_Myr4_pseudo; break;
    case MVT::v32i8: opcode = SHAVE::CMU_LUTW_i8_v32i8_Myr4_pseudo; break;
    case MVT::v16i8: opcode = SHAVE::CMU_LUTW_i8_v16i8_Myr4_pseudo; break;
    case MVT::v8i8: opcode = SHAVE::CMU_LUTW_i8_v8i8_Myr4_pseudo; break;
    case MVT::v4i8: opcode = SHAVE::CMU_LUTW_i8_v4i8_Myr4_pseudo; break;

    // f32 vector element insert
    case MVT::v16f32: opcode = SHAVE::CMU_LUTW_f32_v16f32_Myr4_pseudo; break;
    case MVT::v8f32: opcode = SHAVE::CMU_LUTW_f32_v8f32_Myr4_pseudo; break;
    case MVT::v4f32: opcode = SHAVE::CMU_LUTW_f32_v4f32_Myr4_pseudo; break;
    case MVT::v2f32: opcode = SHAVE::CMU_LUTW_f32_v2f32_Myr4_pseudo; break;

    // f16 vector element insert
    case MVT::v32f16: opcode = SHAVE::CMU_LUTW_f16_v32f16_Myr4_pseudo; break;
    case MVT::v16f16: opcode = SHAVE::CMU_LUTW_f16_v16f16_Myr4_pseudo; break;
    case MVT::v8f16: opcode = SHAVE::CMU_LUTW_f16_v8f16_Myr4_pseudo; break;
    case MVT::v4f16: opcode = SHAVE::CMU_LUTW_f16_v4f16_Myr4_pseudo; break;
    case MVT::v2f16: opcode = SHAVE::CMU_LUTW_f16_v2f16_Myr4_pseudo; break;

    default:
      llvm_unreachable("Invalid vector type found when selecting CMU.LUTW");
    }
  }

  SDValue undef = SDValue(CurDAG->getMachineNode(SHAVE::IMPLICIT_DEF, dbgLoc, MVT::i32), 0);

  ops.push_back(undef);
  ops.push_back(vector);
  ops.push_back(value);
  ops.push_back(index);
  ops.push_back(undef);

  return CurDAG->getMachineNode(opcode, dbgLoc, vectorType, ops);
}

void SHAVEISelDAGtoDAG::SelectINSERT_VECTOR_ELT_VariableIndex(SDNode *N) {
  SDNode * result = SelectINSERT_VECTOR_ELT_VariableIndexImpl(N->getOperand(0), N->getOperand(1), N->getOperand(2), N->getValueType(0).getSimpleVT(),
                                                              N->getValueType(0).getScalarType().getSimpleVT(), SDLoc(N));
  ReplaceNode(N, result);
}

SDNode * SHAVEISelDAGtoDAG::SelectINSERT_VECTOR_ELT_ShiftInto(SDNode *N) {
  MVT vectorType = N->getValueType(0).getSimpleVT();
  if (vectorType != MVT::v4i32 && vectorType != MVT::v8i16 && vectorType != MVT::v16i8 && 
      vectorType != MVT::v4f32 && vectorType != MVT::v8f16)
    return nullptr;

  SDValue inputVector = N->getOperand(0);
  if (inputVector.getOpcode() != ISD::VECTOR_SHUFFLE)
    return nullptr;

  ShuffleVectorSDNode * shuffle = cast<ShuffleVectorSDNode>(inputVector.getNode());
  ArrayRef<int> mask = shuffle->getMask();

  ConstantSDNode * index = cast<ConstantSDNode>(N->getOperand(2).getNode());
  if (index->getZExtValue() == (vectorType.getVectorNumElements() - 1)) {
    for (unsigned int i = 0; i < (mask.size() - 1); ++i)
      if (mask[i] != (int) (i + 1))
        return nullptr;

    unsigned int opcode = 0;
    switch (vectorType.SimpleTy) {
    case MVT::v4i32: 
    case MVT::v4f32: 
      opcode = SHAVE::CMU_SHRIV_x32;
      break;
    case MVT::v8i16: 
    case MVT::v8f16:
      opcode = SHAVE::CMU_SHRIV_x16;
      break;
    case MVT::v16i8:
      opcode = SHAVE::CMU_SHRIV_x8;
      break;
    default:
      return nullptr;
    }

    SmallVector<SDValue, 2> ops;
    ops.push_back(inputVector.getOperand(0));
    ops.push_back(N->getOperand(1));

    return getMachineNodeWithDefaultOps(opcode, SDLoc(N), vectorType, ops);
  }
  else if (index->getZExtValue() == 0) {
    for (unsigned int i = 1; i < mask.size(); ++i)
      if (mask[i] != (int) (i - 1))
        return nullptr;

    unsigned int opcode = 0;
    switch (vectorType.SimpleTy) {
    case MVT::v4i32:
    case MVT::v4f32:
      opcode = SHAVE::CMU_SHLIV_x32;
      break;
    case MVT::v8i16:
    case MVT::v8f16:
      opcode = SHAVE::CMU_SHLIV_x16;
      break;
    case MVT::v16i8:
      opcode = SHAVE::CMU_SHLIV_x8;
      break;
    default:
      return nullptr;
    }

    SmallVector<SDValue, 2> ops;
    ops.push_back(inputVector.getOperand(0));
    ops.push_back(N->getOperand(1));

    return getMachineNodeWithDefaultOps(opcode, SDLoc(N), vectorType, ops);
  }

  return nullptr;
}

SDNode * SHAVEISelDAGtoDAG::SelectEXTRACT_VECTOR_ELTImpl(SDValue vector, SDValue index, MVT vectorType, SDLoc dbgLoc) {
  SmallVector<SDValue, 4> ops;
  unsigned opcode = 0;

  // Find the right opcode
  if (hasFeature(SHAVE::HasVRF128_Feature)) {
    switch (vectorType.SimpleTy) {
      // i32 vector element extract
    case MVT::v4i32: opcode = SHAVE::CMU_LUT_i32_v4i32_pseudo; break;
    case MVT::v2i32: opcode = SHAVE::CMU_LUT_i32_v2i32_pseudo; break;

      // i16 vector element extract
    case MVT::v8i16: opcode = SHAVE::CMU_LUT_i16_v8i16_pseudo; break;
    case MVT::v4i16: opcode = SHAVE::CMU_LUT_i16_v4i16_pseudo; break;
    case MVT::v2i16: opcode = SHAVE::CMU_LUT_i16_v2i16_pseudo; break;

      // i8 vector element extract
    case MVT::v16i8: opcode = SHAVE::CMU_LUT_i8_v16i8_pseudo; break;
    case MVT::v8i8:  opcode = SHAVE::CMU_LUT_i8_v8i8_pseudo; break;
    case MVT::v4i8:  opcode = SHAVE::CMU_LUT_i8_v4i8_pseudo; break;
    case MVT::v2i8:  opcode = SHAVE::CMU_LUT_i8_v2i8_pseudo; break;

      // f32 vector element extract
    case MVT::v4f32: opcode = SHAVE::CMU_LUT_f32_v4f32_pseudo; break;
    case MVT::v2f32: opcode = SHAVE::CMU_LUT_f32_v2f32_pseudo; break;

      // f16 vector element extract
    case MVT::v8f16: opcode = SHAVE::CMU_LUT_f16_v8f16_pseudo; break;
    case MVT::v4f16: opcode = SHAVE::CMU_LUT_f16_v4f16_pseudo; break;
    case MVT::v2f16: opcode = SHAVE::CMU_LUT_f16_v2f16_pseudo; break;

    default:
      llvm_unreachable("Invalid vector type found when selecting CMU.LUT");
    }
  }
  else {
    switch (vectorType.SimpleTy) {
      // i32 vector element extract
    case MVT::v16i32: opcode = SHAVE::CMU_LUT_i32_v16i32_Myr4_pseudo; break;
    case MVT::v8i32: opcode = SHAVE::CMU_LUT_i32_v8i32_Myr4_pseudo; break;
    case MVT::v4i32: opcode = SHAVE::CMU_LUT_i32_v4i32_Myr4_pseudo; break;
    case MVT::v2i32: opcode = SHAVE::CMU_LUT_i32_v2i32_Myr4_pseudo; break;

      // i16 vector element extract
    case MVT::v32i16: opcode = SHAVE::CMU_LUT_i16_v32i16_Myr4_pseudo; break;
    case MVT::v16i16: opcode = SHAVE::CMU_LUT_i16_v16i16_Myr4_pseudo; break;
    case MVT::v8i16: opcode = SHAVE::CMU_LUT_i16_v8i16_Myr4_pseudo; break;
    case MVT::v4i16: opcode = SHAVE::CMU_LUT_i16_v4i16_Myr4_pseudo; break;
    case MVT::v2i16: opcode = SHAVE::CMU_LUT_i16_v2i16_Myr4_pseudo; break;

      // i8 vector element extract
    case MVT::v64i8: opcode = SHAVE::CMU_LUT_i8_v64i8_Myr4_pseudo; break;
    case MVT::v32i8: opcode = SHAVE::CMU_LUT_i8_v32i8_Myr4_pseudo; break;
    case MVT::v16i8: opcode = SHAVE::CMU_LUT_i8_v16i8_Myr4_pseudo; break;
    case MVT::v8i8:  opcode = SHAVE::CMU_LUT_i8_v8i8_Myr4_pseudo; break;
    case MVT::v4i8:  opcode = SHAVE::CMU_LUT_i8_v4i8_Myr4_pseudo; break;

      // f32 vector element extract
    case MVT::v16f32: opcode = SHAVE::CMU_LUT_f32_v16f32_Myr4_pseudo; break;
    case MVT::v8f32: opcode = SHAVE::CMU_LUT_f32_v8f32_Myr4_pseudo; break;
    case MVT::v4f32: opcode = SHAVE::CMU_LUT_f32_v4f32_Myr4_pseudo; break;
    case MVT::v2f32: opcode = SHAVE::CMU_LUT_f32_v2f32_Myr4_pseudo; break;

      // f16 vector element extract
    case MVT::v32f16: opcode = SHAVE::CMU_LUT_f16_v32f16_Myr4_pseudo; break;
    case MVT::v16f16: opcode = SHAVE::CMU_LUT_f16_v16f16_Myr4_pseudo; break;
    case MVT::v8f16: opcode = SHAVE::CMU_LUT_f16_v8f16_Myr4_pseudo; break;
    case MVT::v4f16: opcode = SHAVE::CMU_LUT_f16_v4f16_Myr4_pseudo; break;
    case MVT::v2f16: opcode = SHAVE::CMU_LUT_f16_v2f16_Myr4_pseudo; break;

    default:
      llvm_unreachable("Invalid vector type found when selecting CMU.LUT");
    }
  }

  SDValue undef = SDValue(CurDAG->getMachineNode(SHAVE::IMPLICIT_DEF, dbgLoc, MVT::i32), 0);

  ops.push_back(undef);
  ops.push_back(vector);
  ops.push_back(index);
  ops.push_back(undef);

  return CurDAG->getMachineNode(opcode, dbgLoc, vectorType.getScalarType(), ops);
}

void SHAVEISelDAGtoDAG::SelectEXTRACT_VECTOR_ELT(SDNode *N) {
  SDNode * result = SelectEXTRACT_VECTOR_ELTImpl(N->getOperand(0), N->getOperand(1), N->getOperand(0)->getValueType(0).getSimpleVT(), SDLoc(N));
  ReplaceNode(N, result);
}

void SHAVEISelDAGtoDAG::SelectSHAVE_EXTRACT_SUBVECTOR_ConstantIndex(SDNode *N) {
  // Select SHAVEISD::EXTRACT_SUBVECTOR with a constant index
  //   destination = EXTRACT_SUBVECTOR source, index
  SDLoc dbgLoc(N);
  SmallVector<SDValue, 4> ops;

  SDValue source = N->getOperand(0);
  SDValue index = N->getOperand(1);
  MVT returnType = N->getValueType(0).getSimpleVT();
  SDNode * result = nullptr;

  unsigned constIndex = cast<ConstantSDNode>(index)->getZExtValue();
  switch (returnType.SimpleTy) {
  default:
    llvm_unreachable("Invalid EXTRACT_SUBVECTOR type encountered during selection");
  // Extract v2i32 from v4i32
  case MVT::v2i32:
  case MVT::v2f32: {
    ops.push_back(source);
    ops.push_back(source);
    ops.push_back(CurDAG->getTargetConstant(constIndex * 4, dbgLoc, MVT::i8));

    // CMU.ALIGNVEC destination source source index*4
    unsigned alignvecOpcode = SHAVE::CMU_ALIGNVEC_imm_vrf_vrf64;
    result = getMachineNodeWithDefaultOps(alignvecOpcode, dbgLoc, returnType, ops);
    break;
  }
  // Extract v4i16 from v8i16
  case MVT::v4i16:
  case MVT::v4f16: {
    ops.push_back(source);
    ops.push_back(source);
    ops.push_back(CurDAG->getTargetConstant(constIndex * 2, dbgLoc, MVT::i8));

    // CMU.ALIGNVEC destination source source index*2
    unsigned alignvecOpcode = SHAVE::CMU_ALIGNVEC_imm_vrf_vrf64;
    result = getMachineNodeWithDefaultOps(alignvecOpcode, dbgLoc, returnType, ops);
    break;
  }
  // Extract v8i8 from v16i8
  case MVT::v8i8: {
    ops.push_back(source);
    ops.push_back(source);
    ops.push_back(CurDAG->getTargetConstant(constIndex, dbgLoc, MVT::i8));

    // CMU.ALIGNVEC destination source source index
    unsigned alignvecOpcode = SHAVE::CMU_ALIGNVEC_imm_vrf_vrf64;
    result = getMachineNodeWithDefaultOps(alignvecOpcode, dbgLoc, returnType, ops);
    break;
  }
  // Extract v2i16 from v4i16/v8i16
  case MVT::v2i16:
  case MVT::v2f16: {
    if (constIndex % 2 == 0) {
      unsigned opcode = source->getValueSizeInBits(0) == 128 ? SHAVE::CMU_CPVI_x32 : SHAVE::CMU_CPVI_x32_v2i32;

      ops.push_back(source);
      ops.push_back(CurDAG->getTargetConstant(constIndex / 2, dbgLoc, MVT::i32));

      // CMU.CPVI destination source.(index/2)
      result = getMachineNodeWithDefaultOps(opcode, dbgLoc, returnType, ops);
      break;
    }
    else {
      unsigned cpvi = source->getValueSizeInBits(0) == 128 ? SHAVE::CMU_CPVI_x32 : SHAVE::CMU_CPVI_x32_v2i32;

      unsigned alignvec = source->getValueSizeInBits(0) == 128 ? SHAVE::CMU_ALIGNVEC_imm_vrf : SHAVE::CMU_ALIGNVEC_imm_vrf64;

      ops.push_back(source);
      ops.push_back(source);
      ops.push_back(CurDAG->getTargetConstant(constIndex * 2, dbgLoc, MVT::i8));

      // CMU.ALIGNVEC align source source index*2
      MachineSDNode * align = getMachineNodeWithDefaultOps(alignvec, dbgLoc, source->getValueType(0), ops);

      ops.clear();
      ops.push_back(SDValue(align, 0));
      ops.push_back(CurDAG->getTargetConstant(0, dbgLoc, MVT::i32));

      // CMU.CPVI destination align.0
      result = getMachineNodeWithDefaultOps(cpvi, dbgLoc, returnType, ops);
      break;
    }
  }
  // Extract v4i8 from v8i8/v16i8
  case MVT::v4i8: {
    if (constIndex % 4 == 0) {
      unsigned opcode = source->getValueSizeInBits(0) == 128 ? SHAVE::CMU_CPVI_x32 : SHAVE::CMU_CPVI_x32_v2i32;

      ops.push_back(source);
      ops.push_back(CurDAG->getTargetConstant(constIndex / 4, dbgLoc, MVT::i32));

      // CMU.CPVI destination source.(index/4)
      result = getMachineNodeWithDefaultOps(opcode, dbgLoc, returnType, ops);
      break;
    }
    else {
      unsigned cpvi = source->getValueSizeInBits(0) == 128 ? SHAVE::CMU_CPVI_x32 : SHAVE::CMU_CPVI_x32_v2i32;
      unsigned alignvec = source->getValueSizeInBits(0) == 128 ? SHAVE::CMU_ALIGNVEC_imm_vrf : SHAVE::CMU_ALIGNVEC_imm_vrf64;

      ops.push_back(source);
      ops.push_back(source);
      ops.push_back(CurDAG->getTargetConstant(constIndex, dbgLoc, MVT::i8));

      // CMU.ALIGNVEC align source source index
      MachineSDNode * align = getMachineNodeWithDefaultOps(alignvec, dbgLoc, source->getValueType(0), ops);

      ops.clear();
      ops.push_back(SDValue(align, 0));
      ops.push_back(CurDAG->getTargetConstant(0, dbgLoc, MVT::i32));

      // CMU.CPVI destination align.0
      result = getMachineNodeWithDefaultOps(cpvi, dbgLoc, returnType, ops);
      break;
    }
  }
  // Extract v2i8 from v4i8/v8i8/v16i8
  case MVT::v2i8: {
    if (source.getValueType() == MVT::v4i8) {
      // destination.l = source
      if (constIndex == 0) {
        result = CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG, dbgLoc, MVT::v2i8, source,
                                        CurDAG->getTargetConstant(SHAVE::hsub_0, dbgLoc, MVT::i32));
      }
      else {
        ops.push_back(source);
        ops.push_back(CurDAG->getTargetConstant(constIndex * 8, dbgLoc, MVT::i32));

        // IAU.SHR.u32 destination source index*8
        result = getMachineNodeWithDefaultOps(SHAVE::IAU_SHR_u16_u32, dbgLoc, returnType, ops);
      }
    }
    else if (constIndex % 2 == 0) {
      unsigned opcode = source->getValueSizeInBits(0) == 128 ? SHAVE::CMU_CPVI_x16_l : SHAVE::CMU_CPVI_v4i16_l;

      ops.push_back(source);
      ops.push_back(CurDAG->getTargetConstant(constIndex / 2, dbgLoc, MVT::i32));

      // CMU.CPVI.x16 destination.l source.(index/2)
      result = getMachineNodeWithDefaultOps(opcode, dbgLoc, returnType, ops);
    }
    else {
      unsigned cpvi = source->getValueSizeInBits(0) == 128 ? SHAVE::CMU_CPVI_x16_l : SHAVE::CMU_CPVI_v4i16_l;
      unsigned alignvec = source->getValueSizeInBits(0) == 128 ? SHAVE::CMU_ALIGNVEC_imm_vrf : SHAVE::CMU_ALIGNVEC_imm_vrf64;

      ops.push_back(source);
      ops.push_back(source);
      ops.push_back(CurDAG->getTargetConstant(constIndex, dbgLoc, MVT::i8));

      // CMU.ALIGNVEC align source source index
      MachineSDNode * align = getMachineNodeWithDefaultOps(alignvec, dbgLoc, source->getValueType(0), ops);

      ops.clear();
      ops.push_back(SDValue(align, 0));
      ops.push_back(CurDAG->getTargetConstant(0, dbgLoc, MVT::i32));

      // CMU.CPVI.x16 destination.l source.0
      result = getMachineNodeWithDefaultOps(cpvi, dbgLoc, returnType, ops);
    }
    break;
  }
  }

  assert(result != nullptr && "Unable to select EXTRACT_SUBVECTOR with a constant index");

  ReplaceNode(N, result);
}

void SHAVEISelDAGtoDAG::SelectMASKED_STORE(SDNode *N) {
  SDLoc dbgLoc(N);
  SmallVector<SDValue, 6> operands = {
    N->getOperand(1), // value
    N->getOperand(2), // pointer
    N->getOperand(3), // SHAVECC
    N->getOperand(4), // CC register
    N->getOperand(0), // chain
    N->getOperand(5)  // compare
  };

  auto n = N->getOperand(1)->getValueSizeInBits(0);
  auto sizeInBits = N->getOperand(1)->getValueSizeInBits(0);
  unsigned int opcode = 0;

  switch (sizeInBits) {
  case 128:
    opcode = SHAVE::MASKED_STORE_VRF;
    break;
  case 256:
    opcode = SHAVE::MASKED_STORE_WVRF;
    break;
  case 512:
    if (N->getOpcode() == SHAVEISD::MASKED_STORE_H)
      opcode = SHAVE::MASKED_STORE_WVRF_h;
    else if (N->getOpcode() == SHAVEISD::MASKED_STORE_L)
      opcode = SHAVE::MASKED_STORE_WVRF_l;
    break;
  case 32:
    opcode = SHAVE::MASKED_STORE_IRF;
    break;
  }
  assert(opcode != 0 && "Ubsupported masked store value size");

  MachineSDNode * store = CurDAG->getMachineNode(opcode, dbgLoc, CurDAG->getVTList(MVT::Other), operands);
  ReplaceNode(N, store);
}

void SHAVEISelDAGtoDAG::SelectCompressVector(SDNode *N) {
  MVT outputType = N->getValueType(0).getSimpleVT();
  MVT inputType = N->getOperand(0).getValueType().getSimpleVT();
  unsigned int opcode = SHAVE::NONE;

#define getOpcode(DestinationRC, SourceRC) \
  switch(outputType.getScalarSizeInBits()) { \
    case 32: opcode = SHAVE::CMU_COMPRESS_x32_##DestinationRC##_##SourceRC; break; \
    case 16: opcode = SHAVE::CMU_COMPRESS_x16_##DestinationRC##_##SourceRC; break; \
    case 8: opcode = SHAVE::CMU_COMPRESS_x8_##DestinationRC##_##SourceRC; break; \
    default: llvm_unreachable("Unsupported scalar size for SHAVEISD::COMPRESS_VECTOR"); \
  }

#define getOpcodes(DestinationRC) \
  switch(inputType.getSizeInBits()) { \
    case 512: getOpcode(DestinationRC, WVRF512); break; \
    case 256: getOpcode(DestinationRC, WVRF256_0); break; \
    case 128: getOpcode(DestinationRC, WVRF128_0); break; \
    case 64:  getOpcode(DestinationRC, WVRF64_0); break; \
    case 32:  getOpcode(DestinationRC, WVRF32_0); break; \
    case 16:  getOpcode(DestinationRC, WVRF16_0); break; \
    default: llvm_unreachable("Unsupported input type for SHAVEISD::COMPRESS_VECTOR"); \
  }

  switch (outputType.getSizeInBits()) {
  case 512: getOpcodes(WVRF512); break;
  case 256: getOpcodes(WVRF256_0); break;
  case 128: getOpcodes(WVRF128_0); break;
  case 64:  getOpcodes(WVRF64_0); break;
  case 32:  getOpcodes(WVRF32_0); break;
  case 16:  getOpcodes(WVRF16_0); break;
  default:
    llvm_unreachable("Unsupported output type for SHAVEISD::COMPRESS_VECTOR");
  }

#undef getOpcodes
#undef getOpcode

  assert(opcode != SHAVE::NONE && "Unsupported types for SHAVEISD::COMPRESS_VECTOR");

  SDLoc dbgLoc(N);

  SmallVector<SDValue, 2> ops;
  ops.push_back(N->getOperand(0));
  ops.push_back(N->getOperand(1));

  SDNode * compress = getMachineNodeWithDefaultOps(opcode, dbgLoc, outputType, ops);
  ReplaceNode(N, compress);
}

void SHAVEISelDAGtoDAG::SelectCTPOP(SDNode *N) {
  // FIXME: Movidius - This only handles v4i32 vectors for now. This needs to be expanded
  //                   to work with all vector types. All other types have been set to expand
  //                   in SHAVELowering
  SDLoc dbgLoc(N);
  SmallVector<SDValue, 1> ops;

  ops.clear();
  ops.push_back(N->getOperand(0));

  SDNode * ones = getMachineNodeWithDefaultOps(SHAVE::SAU_ONESX_x32, dbgLoc, MVT::v4i8, ops);

  ops.clear();
  ops.push_back(SDValue(ones, 0));

  SDNode * result = getMachineNodeWithDefaultOps(SHAVE::CMU_CPVV_v4i8_v4i32_conv, dbgLoc, MVT::v4i32, ops);
  ReplaceNode(N, result);
}

void SHAVEISelDAGtoDAG::SelectFDIV(SDNode *N) {
  // ISD::FDIV for vectors isn't supported but we need to set the lowering for v8f16 to Legal for u8f load support
  // FIXME: Movidius - It's possible to lower the load with a Tablegen pattern, can we generate the fallback BUILD_VECTOR
  //                   sequence using Tablegen too?
  bool isSplat255 = false;
  SDValue source;
  SDLoc dbgLoc(N);

  SDValue operand0 = N->getOperand(0);
  SDValue operand1 = N->getOperand(1);
  
  if (operand1.getOpcode() == SHAVEISD::SPLAT)
    if (ConstantFPSDNode * constant = dyn_cast<ConstantFPSDNode>(operand1.getOperand(0)))
      isSplat255 = constant->isExactlyValue(255.0);

  if (operand0.getOpcode() == ISD::UINT_TO_FP && operand0.getOperand(0).getOpcode() == ISD::LOAD) {
    SDValue load = operand0.getOperand(0);
    source = load.getOperand(1);
  }
  
  if (isSplat255 && source != SDValue()) {
    SelectCode(N);
  }
  else {
    // Fallback
    SmallVector<SDValue, 8> divs;

    for (unsigned int i = 0; i < 8; ++i) {
      SmallVector<SDValue, 2> operands0 = { operand0, CurDAG->getTargetConstant(i, dbgLoc, MVT::i32) };
      SDNode * extract0 = getMachineNodeWithDefaultOps(SHAVE::CMU_CPVI_f16_l, dbgLoc, MVT::f16, operands0);

      SmallVector<SDValue, 2> operands1 = { operand1, CurDAG->getTargetConstant(i, dbgLoc, MVT::i32) };
      SDNode * extract1 = getMachineNodeWithDefaultOps(SHAVE::CMU_CPVI_f16_l, dbgLoc, MVT::f16, operands1);

      SmallVector<SDValue, 2> divOperands = { SDValue(extract0, 0), SDValue(extract1, 0) };
      divs.push_back(SDValue(getMachineNodeWithDefaultOps(SHAVE::SAU_DIV_f16, dbgLoc, MVT::f16, divOperands), 0));
    }

    SDNode * buildVector = CurDAG->getNode(ISD::BUILD_VECTOR, dbgLoc, MVT::v8f16, divs).getNode();
    ReplaceNode(N, buildVector);
    SelectBUILD_VECTOR(buildVector);
  }
}

SDNode * SHAVEISelDAGtoDAG::SelectBITCASTImpl(SDValue source, MVT destinationType, SDLoc dbgLoc) {
  // Implement full-vector bitcasts as copies to the relevant register class.
  // The copy should have the same register class for both the source and the
  // destination. LLVM will eliminate such copies as redundant.
  EVT sourceType = source.getValueType();

  unsigned RegClassID = 0;

  switch (sourceType.getSizeInBits()) {
  default:
    llvm_unreachable("Unsupported bitcast width");
  case 16:
    RegClassID = SHAVE::IRF16_lRegClassID;
    break;
  case 32:
    RegClassID = SHAVE::IRF32RegClassID;
    break;
  case 64:
    if (destinationType.isVector()) RegClassID = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::WVRF64_0RegClassID : SHAVE::VRF64_lRegClassID;
    else                            RegClassID = SHAVE::IRF64RegClassID;
    break;
  case 128:
    RegClassID = hasFeature(SHAVE::HasVRF512_Feature) ? SHAVE::WVRF128_0RegClassID : SHAVE::VRF128RegClassID;
    break;
  case 256:
    RegClassID = SHAVE::WVRF256_0RegClassID;
    break;
  case 512:
    RegClassID = SHAVE::WVRF512RegClassID;
    break;
  }

  SmallVector<SDValue, 2> ops;

  ops.push_back(source);
  ops.push_back(CurDAG->getTargetConstant(RegClassID, dbgLoc, MVT::i32));

  return CurDAG->getMachineNode(TargetOpcode::COPY_TO_REGCLASS, dbgLoc, destinationType, ops);
}

void SHAVEISelDAGtoDAG::Select(SDNode *N) {
  if(N->isMachineOpcode())
    return; // Node already selected

  SDLoc dbgLoc(N);

  // ISD and SHAVEISD don't overlap: see FIRST_NUMBER = ISD::BUILTIN_OP_END
  switch (N->getOpcode()) {
  case ISD::INTRINSIC_WO_CHAIN:
  case ISD::INTRINSIC_W_CHAIN:
  case ISD::INTRINSIC_VOID:
    SelectIntrinsic(N);
    return;

  case ISD::Constant:
    SelectImmediateLoad(N);
    return;

  case ISD::ConstantFP:
    SelectImmediateLoadFP(N);
    return;

  case SHAVEISD::ACCP_SEQ:
  case SHAVEISD::MACP_SEQ:
    SelectACCP_SEQ(N);
    return;

  case SHAVEISD::CLAMP:
    if (hasFeature(SHAVE::HasVRF128_Feature))
      SelectCLAMP(N);
    else
      SelectCode(N);
    return;

  case SHAVEISD::LOAD64_LOW:
  case SHAVEISD::LOAD64_HIGH:
    SelectLOAD64_LOW_HIGH(N);
    return;

  case ISD::LOAD: {
    LoadSDNode *LN = dyn_cast<LoadSDNode>(N);
    if (LN != nullptr && LN->isIndexed())
      SelectIndexedLOAD(N);
    else if (N->getValueType(0).isVector())
      SelectVectorLOAD(N);
    else
      SelectScalarLOAD(N);
    return;
  }

  case ISD::STORE:
    if (N->getOperand(1).getValueType().isVector())
      SelectVectorSTORE(N);
    else
      SelectScalarSTORE(N);
    return;

  case SHAVEISD::UNPACK:
    SelectVectorUNPACK(N);
    return;

  case ISD::VECTOR_SHUFFLE:
    SelectVectorSHUFFLE(N);
    return;

  case ISD::EXTRACT_SUBVECTOR:
    if (hasFeature(SHAVE::HasVRF128_Feature)) {
      SelectEXTRACT_SUBVECTOR(N);
      return;
    }
    break;

  case ISD::BUILD_VECTOR:
    SelectBUILD_VECTOR(N);
    return;

  case ISD::SIGN_EXTEND_INREG:
    if (N->getOperand(0).getValueType().isVector()) {
      SelectVectorSIGN_EXTEND_INREG(N);
      return;
    }
    break;

  case ISD::ANY_EXTEND:
    if (hasFeature(SHAVE::HasVRF128_Feature) && N->getOperand(0).getValueType().isVector()) {
      SelectVectorANY_EXTEND(N);
      return;
    }
    break;

  case ISD::TRUNCATE:
    if (hasFeature(SHAVE::HasVRF128_Feature) && N->getOperand(0).getValueType().isVector()) {
      SelectVectorTRUNCATE(N);
      return;
    }
    break;

  case ISD::SINT_TO_FP:
    if (hasFeature(SHAVE::HasVRF128_Feature) && N->getOperand(0).getValueType().isVector()) {
      SelectVectorSINT_TO_FP(N);
      return;
    }
    break;

  case ISD::BITCAST: {
    SDNode * result = SelectBITCASTImpl(N->getOperand(0), N->getValueType(0).getSimpleVT(), SDLoc(N));
    ReplaceNode(N, result);
    return;
  }

  case SHAVEISD::LDISym: {
    SmallVector<SDValue, 1> ops;

    ops.push_back(N->getOperand(0));

    SDNode * loadLo = getMachineNodeWithDefaultOps(SHAVE::LSU_LDILSym, dbgLoc, MVT::i32, ops);

    ops.insert(ops.begin(), SDValue(loadLo, 0));

    CurDAG->SelectNodeTo(N, SHAVE::LSU_LDIHSym, MVT::i32, ops);
    return;
  }

  case ISD::BRIND: {
    SDValue chain = N->getOperand(0);
    SDValue target = N->getOperand(1);
    SmallVector<SDValue, 1> ops;

    ops.push_back(target); // register containing the branch address

    SelectNodeWithDefaultOps(N, SHAVE::BRU_JMP, MVT::Other, ops, chain);
    return;
  }

  case ISD::FrameIndex: {
    // If frame indices reach this point, promote them to target frame indices and
    // append and IAU.ADD instruction to compute the address
    SDValue fi= CurDAG->getTargetFrameIndex(cast<FrameIndexSDNode>(N)->getIndex(), MVT::i32);
    SDValue imm = CurDAG->getTargetConstant(0, dbgLoc, MVT::i32);
    SmallVector<SDValue, 4> ops;

    ops.push_back (fi);
    ops.push_back (imm);

    unsigned Opc = SHAVE::IAU_ADD_32_imm;

    if (N->hasOneUse()) {
      SelectNodeWithDefaultOps(N, Opc, MVT::i32, ops);
    }
    else {
      SDNode * result = getMachineNodeWithDefaultOps(Opc, SDLoc(N), MVT::i32, ops);
      ReplaceNode(N, result);
    }
    return;
  }

  case SHAVEISD::CMU_CM:
    SelectCompare(N);
    return;

  case ISD::BSWAP: {
    // Coming from __builtin_bswap32
    SDValue value = N->getOperand(0);
    EVT VT = value.getValueType();
    assert((VT == MVT::i32 || VT == MVT::i16) && "Only __builtin_bswap32() and __builtin_bswap16() are supported!");
    unsigned opcode = 0;
    if (VT == MVT::i32)
      opcode = SHAVE::SAU_SWZ;
    else // VT == MVT::i16
      opcode = SHAVE::SAU_SWZ_i16;
    SmallVector<SDValue, 8> ops;

    ops.push_back(value);

    if (VT == MVT::i32) {
      ops.push_back(CurDAG->getTargetConstant(0, dbgLoc, MVT::i8));
      ops.push_back(CurDAG->getTargetConstant(1, dbgLoc, MVT::i8));
      ops.push_back(CurDAG->getTargetConstant(2, dbgLoc, MVT::i8));
      ops.push_back(CurDAG->getTargetConstant(3, dbgLoc, MVT::i8));
    } else if (VT == MVT::i16) {
      ops.push_back(CurDAG->getTargetConstant(0, dbgLoc, MVT::i8));
      ops.push_back(CurDAG->getTargetConstant(0, dbgLoc, MVT::i8));
      ops.push_back(CurDAG->getTargetConstant(0, dbgLoc, MVT::i8));
      ops.push_back(CurDAG->getTargetConstant(1, dbgLoc, MVT::i8));
    }

    SelectNodeWithDefaultOps(N, opcode, VT, ops);
    return;
  }

  case ISD::BR: {
    SDValue chain = N->getOperand(0);
    SDValue target = N->getOperand(1);

    if (N->getNumOperands() == 5) {
      // This is a flagged BR node and probably predicated.
      SDValue predMask = N->getOperand(2);
      SDValue predReg = N->getOperand(3);
      SDValue glue = N->getOperand(4);

      unsigned Opcode = SHAVE::JMP_TO_LABEL;
      SHAVECC::CondCode CC = (SHAVECC::CondCode)cast<ConstantSDNode>(predMask)->getZExtValue();

      // Do not use PREEMPTION branch if the target has Code Protection support, this is handled independently by SHAVEPostSchedulingPreemption
      if (SHAVEOptions::EnablePreemption == Preemption::Restore &&
          CC != SHAVECC::AL &&
          SHAVECC::canReverse(CC) && // Generic loop preemption requires the CC be reversible
          !hasFeature(SHAVE::HasCodeProtection_Feature)) {
        // Get an extra register which is used by the expanded code in SHAVEPreemptionHandler to signal
        // an interrupt should be raised. The register is then reused to check the loop status when
        // control has returned to this function
        SDValue internalReg = SDValue(getLDImm32Node(CurDAG, MVT::i32, dbgLoc, 0), 0); // Def/used register in expanded code, starting value 0

        SDValue ops[] = {
          target,
          internalReg,
          predMask,
          predReg,
          chain,
          glue
        };

        CurDAG->SelectNodeTo(N, SHAVE::JMPcc_TO_LABEL_PREEMPTION, CurDAG->getVTList(MVT::Other, MVT::i32), ArrayRef(ops, 6));
      }
      else {
        SDValue ops[] = {
          target,
          predMask,
          predReg,
          chain,
          glue
        };
        if (CC != SHAVECC::AL)
          Opcode = SHAVE::JMPcc_TO_LABEL;
        CurDAG->SelectNodeTo(N, Opcode, MVT::Other, ArrayRef(ops, 5));
      }
    }
    else {
      assert(N->getNumOperands() == 2 && "These are the only accepted flavors of BR");
      SmallVector<SDValue, 4> Ops;

      Ops.push_back(target);

      SelectNodeWithDefaultOps(N, SHAVE::JMP_TO_LABEL, MVT::Other, Ops, chain);
    }
    return;
  }

  case ISD::INSERT_VECTOR_ELT: {
    MVT resultType = N->getValueType(0).getSimpleVT();

    // If the index if not constant replace with LUTW instruction
    if (!isa<ConstantSDNode>(N->getOperand(2).getNode())) {
      // Myriad2.3 supports indirect indexes for CP.32 insertions
      if (hasFeature(SHAVE::HasVRF512_Feature) || (resultType != MVT::v4i32 && resultType != MVT::v2i32 && resultType != MVT::v4f32 && resultType != MVT::v2f32)) {
        SelectINSERT_VECTOR_ELT_VariableIndex(N);
        return;
      }
    }
    else {
      SDNode * result = SelectINSERT_VECTOR_ELT_ShiftInto(N);
      if (result) {
        ReplaceNode(N, result);
        return;
      }
    }
    break;
  }

  case ISD::EXTRACT_VECTOR_ELT: {
    MVT resultType = N->getValueType(0).getSimpleVT();
    // If the index if not constant replace with LUT instruction
    if (!isa<ConstantSDNode>(N->getOperand(1).getNode())) {
      if (hasFeature(SHAVE::HasVRF512_Feature) || (resultType != MVT::f32 && resultType != MVT::i32)) {
        SelectEXTRACT_VECTOR_ELT(N);
        return;
      }
    }
    break;
  }

  case SHAVEISD::EXTRACT_SUBVECTOR:
    if (hasFeature(SHAVE::HasVRF128_Feature) && isa<ConstantSDNode>(N->getOperand(1))) {
      SelectSHAVE_EXTRACT_SUBVECTOR_ConstantIndex(N);
      return;
    }
    break;

  case SHAVEISD::MASKED_STORE:
  case SHAVEISD::MASKED_STORE_L:
  case SHAVEISD::MASKED_STORE_H:
    SelectMASKED_STORE(N);
    return;

  case ISD::CTPOP:
    if (N->getValueType(0) == MVT::v4i32) {
      SelectCTPOP(N);
      return;
    }
    break;

  case ISD::FDIV:
    if (N->getValueType(0) == MVT::v8f16) {
      SelectFDIV(N);
      return;
    }
    break;

  case SHAVEISD::COMPRESS_VECTOR:
    assert(hasFeature(SHAVE::HasVRF512_Feature) && "SHAVEISD::COMPRESS_VECTOR is only supported with VRF512");
    SelectCompressVector(N);
    return;
#if 0
// FIXME: Movidius - TODO: implementation of Memset.  See 'SHAVESelectionDAGInfo.cpp' for full details
  case SHAVEISD::MEMSET_BLOCK: {
      SDValue ops[4];
      ops[0] = N->getOperand(0);
      ops[1] = N->getOperand(1);
      ops[2] = N->getOperand(2);
      ops[3] = N->getOperand(3);
      CurDAG->SelectNodeTo(N, SHAVE::SHAVE_MEMSET_BLOCK, MVT::Other, ops, array_lengthof(ops));
    }
#endif
  }

  SelectCode(N);
}

bool SHAVEISelDAGtoDAG::hasFeature(unsigned int feature) const {
  return static_cast<const SHAVETargetMachine&>(TM).getSubtargetImpl()->hasFeature(feature);
}
