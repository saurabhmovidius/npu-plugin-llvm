//===-- SHAVETargetTransformInfo.cpp - SHAVE TTI Implementation -*- C++ -*-===//
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
// This is the SHAVE implementation of 'llvm::TargetTransformInfo'
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "SHAVETargetTransformInfo"

#include "llvm/CodeGen/CostTable.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "SHAVE.h"
#include "SHAVETargetTransformInfo.h"

#include <set>

using namespace llvm;


namespace {
  // Look for a phi node that defines a value used by V
  typedef std::set<const User *> TrackFindPhiUseSet;

  // Returns 'true' when the PHI node is found, or the search completed without finding it
  const PHINode * FindPhiUse(const User *V, TrackFindPhiUseSet &trackSet) {
    // Has this value already been searched?
    if (trackSet.find(V) != trackSet.end()) {
      DEBUG(dbgs() << "FindPhiUse - V already seen: ");
      DEBUG(V->dump());
//      assert(false && "Element already visited");
      return nullptr;
    }

    // Othewise add this to the set of previously visited values
    trackSet.insert(V);

    const PHINode *Phi = dyn_cast<PHINode>(V);

    // If this is a PHI node, we're done
    if (!Phi) {
      DEBUG(dbgs() << "FindPhiUse - V : ");
      DEBUG(V->dump());

      // And search each of it's operands
      for (User::const_op_iterator I = V->op_begin(), E = V->op_end(); I != E; ++I) {
        if (User *V2 = dyn_cast<User>(*I)) {
          if ((Phi = FindPhiUse(V2, trackSet)))
            break;
        }
      }
    }

    return Phi;
  }

  inline const PHINode *FindPhiUse(const User *V) {
    DEBUG(dbgs() << "Entering top-level 'FindPhiUse'\n");
    TrackFindPhiUseSet trackSet;
    return FindPhiUse(V, trackSet);
  }

  // Determine whether V directly or indirectly uses 'Use'
  bool FindUse(const Value *V, const Value *Use) {
    if (Use == V)
      return true;
    else if (isa<PHINode>(V))
      return false;

    if (const User *VUser = dyn_cast<User>(V)) {
      for (User::const_op_iterator I = VUser->op_begin(), E = VUser->op_end();
        I != E; ++I) {
        if (isa<User>(*I)) {
          if (dyn_cast<Value>(V) == Use)
            return true;
        }
      }
    }

    return false;
  }

  // Determine whether V is both defined through a phi node and used by the same
  // phi node.
  static bool IsReduction(const User *V) {
    const PHINode *Phi = FindPhiUse(V);
    if (!Phi)
      return false;

    for (unsigned i = 0; i < Phi->getNumIncomingValues(); i++) {
      Value *IncVal = Phi->getIncomingValue(i);
      if (FindUse(IncVal, V))
        return true;
    }

    return false;
  }
}

unsigned SHAVETargetTransformInfo::getNumberOfRegisters(unsigned ClassID) const {
  // Note: ClassID here refers to abstract register classes, not the target
  // register classes provided in the backend td file. See
  // 'TargetTransformInfoImpl.h'.
  //
  // Current default abstract classes are:
  //   "Generic::ScalarRC" : 0
  //   "Generic::VectorRC" : 1

  bool Vector = (ClassID == 1);

  // 32 VRF registers.
  if (Vector)
    return 32;

  // 32 IRF registers.
  return 32;
}

TypeSize SHAVETargetTransformInfo::getRegisterBitWidth(TTI::RegisterKind K) const {
  switch (K) {
  case TargetTransformInfo::RGK_Scalar:
    // 32-bit IRF registers.
    return TypeSize::getFixed(32);
  case TargetTransformInfo::RGK_FixedWidthVector:
    // 512-bit VRF registers on SHAVE512, 128-bit otherwise.
    return TypeSize::getFixed(ST->hasFeature(SHAVE::HasVRF512_Feature) ? 512 : 128);
  case TargetTransformInfo::RGK_ScalableVector:
    return TypeSize::getScalable(0);
  }

  llvm_unreachable("Unsupported register kind");
}

unsigned
SHAVETargetTransformInfo::getMaxInterleaveFactor(ElementCount VF) const {
  // FIXME: Movidius - need to tune this
  return 4;
}

InstructionCost SHAVETargetTransformInfo::getCastInstrCost(
    unsigned Opcode, Type *Dst, Type *Src, TTI::CastContextHint CCH,
    TTI::TargetCostKind CostKind, const Instruction *I) const {
  // FIXME: Movidius - this needs a lot of refinement, it is very crude
  std::pair<InstructionCost, MVT> SrcLT = getTypeLegalizationCost(Src);
  std::pair<InstructionCost, MVT> DstLT = getTypeLegalizationCost(Dst);

  return SrcLT.first * DstLT.first;
}

InstructionCost SHAVETargetTransformInfo::getVectorInstrCost(
    unsigned Opcode, Type *Val, TTI::TargetCostKind CostKind, unsigned Index,
    Value *Op0, Value *Op1) {
  // FIXME: Movidius - this needs a lot of refinement, it is very crude
  InstructionCost Cost =
      BaseT::getVectorInstrCost(Opcode, Val, CostKind, Index, Op0, Op1);

  // Vector insertions and extractions are really expensive for i8 elements.
  IntegerType *ElTy = dyn_cast<IntegerType>(Val->getScalarType());
  if (ElTy && ElTy->getBitWidth() == 8) {
    switch (Opcode) {
    case Instruction::InsertElement:
    case Instruction::ExtractElement:
        Cost *= 3;
        break;
    }
  }

  return Cost;
}

InstructionCost SHAVETargetTransformInfo::getArithmeticInstrCost(
    unsigned Opcode, Type *Ty, TTI::TargetCostKind CostKind,
    TTI::OperandValueInfo Op1Info, TTI::OperandValueInfo Op2Info,
    ArrayRef<const Value *> Args, const Instruction *CxtI) const {
  // FIXME: Movidius - This implementatin is over simplistic
  std::pair<InstructionCost, MVT> LT = getTypeLegalizationCost(Ty);

  bool IsFloat = Ty->getScalarType()->isFloatingPointTy();
  unsigned OpCost = (IsFloat ? 2 : 1);
  InstructionCost Cost = LT.first * OpCost;

  if(Ty->isVectorTy()) {
    switch (Opcode) {
    default:
      break;
    case Instruction::UDiv:
    case Instruction::SDiv:
    case Instruction::FDiv:
    case Instruction::URem:
    case Instruction::SRem:
    case Instruction::FRem:
      // LLVM doesn't seem to provide a way to say "never vectorise this operation" so we set it to a
      // high number here in hopes it will never get vectorised. Scale by the number of elements so that
      // if it does get selected then it should at least be a small VF
      Cost *= 32;
      if (auto * VTy = dyn_cast<FixedVectorType>(Ty)) // For SHAVE, this should always be the case
        Cost *= VTy->getNumElements();
      break;
    }
  }

  return Cost;
}

InstructionCost SHAVETargetTransformInfo::getAddressComputationCost(
    Type *Ty, ScalarEvolution *SE, const SCEV *Ptr) {
  // Address computations in vectorized code with non-consecutive addresses will
  // likely result in more unused cycles compared to scalar code where the
  // computation can more often be hidden away in branch delay slots. 
  // Empirically, assigning an overhead of 10 here gives best performance results
  unsigned NumVectorInstToHideOverhead = 10;
  int MaxMergeDistance = 64;

  if (Ty->isVectorTy() && SE &&
    !BaseT::isConstantStridedAccessLessThan(SE, Ptr, MaxMergeDistance + 1))
    return NumVectorInstToHideOverhead;

  // Other computations work better with the base cost 
  return BaseT::getAddressComputationCost(Ty, SE, Ptr);
}

InstructionCost
SHAVETargetTransformInfo::getInstructionCost(const User *U,
                                             ArrayRef<const Value *> Operands,
                                             TTI::TargetCostKind CostKind) {
  // Floating-point reduction will introduce latency even after loop unrolling.
  // That's because each (for example) add depends on the result of the
  // previous add.
  if (const BinaryOperator *BinOp = dyn_cast<BinaryOperator>(U)) {
    if (BinOp->getType()->isFloatingPointTy()) {
      if (IsReduction(BinOp)) {
        DEBUG(dbgs() << "FP reduction: " << *BinOp << "\n");
        return TargetTransformInfo::TCC_Basic * 2;
      }
    }
  }

  return BaseT::getInstructionCost(U, Operands, CostKind);
}

static bool isLegalMaskedLoadStore(Type *DataType, unsigned maxStoreSize) {
  if (DataType->isVectorTy() &&
      ((DataType->getPrimitiveSizeInBits() % maxStoreSize == 0) ||
       (DataType->getPrimitiveSizeInBits() == 32 && DataType->getScalarSizeInBits() == 8))) {
    Type * scalarType = DataType->getScalarType();

    if (scalarType->isIntegerTy() || scalarType->isFloatingPointTy()) {
      switch (scalarType->getScalarSizeInBits()) {
      case 8:
      case 16:
      case 32:
        return true;
      }
    }
  }

  return false;
}

bool SHAVETargetTransformInfo::isLegalMaskedLoad(Type *DataType, Align Alignment) const {
  if (ST->hasFeature(SHAVE::HasVRF512_Feature))
    return false;
  return isLegalMaskedLoadStore(DataType, 128);
}

bool SHAVETargetTransformInfo::isLegalMaskedStore(Type *DataType, Align Alignment) const {
  unsigned maxStoreSize =
      (ST->hasFeature(SHAVE::HasVRF512_Feature)) ? 256 : 128;
  return isLegalMaskedLoadStore(DataType, maxStoreSize);
}

InstructionCost SHAVETargetTransformInfo::getArithmeticReductionCost(
    unsigned Opcode, VectorType *Ty, std::optional<FastMathFlags> FMF,
    TTI::TargetCostKind CostKind) {
  // We can use SAU horizontal instructions on these operations, don't let SLP
  // reduce them so that we can detect the pattern in SHAVELowering later
  if (Opcode == Instruction::And || Opcode == Instruction::Or ||
      Opcode == Instruction::Xor || Opcode == Instruction::Add ||
      Opcode == Instruction::FAdd) {

    assert(isa<FixedVectorType>(Ty) &&
           "Scalable vectors are not supported in the SHAVE backend");

    unsigned numElems = cast<FixedVectorType>(Ty)->getNumElements();
    unsigned size = Ty->getScalarSizeInBits();

    // Check if size/numElems is divisible by one of the supported SAU ranges
    // 16 x 8  bits
    // 8  x 16 bits
    // 4  x 32 bits
    if (Ty->getElementType()->isFloatingPointTy()) {
      if ((size % 16 == 0 && numElems % 8 == 0) ||
          (size % 32 == 0 && numElems % 4 == 0))
        return INT_MAX;
    } else {
      if ((size % 8 == 0 && numElems % 16 == 0) ||
          (size % 16 == 0 && numElems % 8 == 0) ||
          (size % 32 == 0 && numElems % 4 == 0))
        return INT_MAX;
    }
  }
  // ToDo doublecheck (dead code/exception)
  return BaseT::getArithmeticReductionCost(Opcode, Ty, FMF, CostKind);
}
