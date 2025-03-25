//===-- SHAVETargetTransformInfo.h - SHAVE TTI Implementation ---*- C++ -*-===//
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

#ifndef SHAVETARGETTRANSFORMINFO_H
#define SHAVETARGETTRANSFORMINFO_H (1)


#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"

#include "SHAVETargetMachine.h"

namespace llvm {

  class SHAVETargetTransformInfo : public BasicTTIImplBase<SHAVETargetTransformInfo> {
    typedef BasicTTIImplBase<SHAVETargetTransformInfo> BaseT;
    friend BaseT;
    using TTI = TargetTransformInfo;

  public:

    SHAVETargetTransformInfo(const SHAVETargetMachine *TM, const Function &F)
      : BaseT(TM, F.getParent()->getDataLayout()),
      ST(TM->getSubtargetImpl(F)), TLI(TM->getSubtargetImpl()->getTargetLowering()) {}

    // Provide value semantics. MSVC requires that we spell all of these out.
    SHAVETargetTransformInfo(const SHAVETargetTransformInfo &Arg)
      : BaseT(static_cast<const BaseT &>(Arg)), ST(Arg.ST), TLI(Arg.TLI) {}

    SHAVETargetTransformInfo(SHAVETargetTransformInfo &&Arg)
      : BaseT(std::move(static_cast<BaseT &>(Arg))), ST(std::move(Arg.ST)), TLI(std::move(Arg.TLI)) {}

    // Klocwork requires that if we define a copy constructor, we must define an
    // assignment operator
    SHAVETargetTransformInfo operator=(const SHAVETargetTransformInfo &Arg);

    unsigned getNumberOfRegisters(unsigned ClassID) const;

    TypeSize getRegisterBitWidth(TTI::RegisterKind K) const;

    unsigned getMaxInterleaveFactor(ElementCount VF) const;

    InstructionCost
    getCastInstrCost(unsigned Opcode, Type *Dst, Type *Src,
                     TTI::CastContextHint CCH,
                     TTI::TargetCostKind CostKind = TTI::TCK_SizeAndLatency,
                     const Instruction *I = nullptr) const;

    using BaseT::getVectorInstrCost;
    InstructionCost getVectorInstrCost(unsigned Opcode, Type *Val,
                                       TTI::TargetCostKind CostKind,
                                       unsigned Index, Value *Op0, Value *Op1);

    InstructionCost getArithmeticInstrCost(
        unsigned Opcode, Type *Ty, TTI::TargetCostKind CostKind,
        TTI::OperandValueInfo Op1Info = {TTI::OK_AnyValue, TTI::OP_None},
        TTI::OperandValueInfo Op2Info = {TTI::OK_AnyValue, TTI::OP_None},
        ArrayRef<const Value *> Args = ArrayRef<const Value *>(),
        const Instruction *CxtI = nullptr) const;

    InstructionCost getAddressComputationCost(Type *Ty,
                                              ScalarEvolution *SE = nullptr,
                                              const SCEV *Ptr = nullptr);

    InstructionCost getInstructionCost(const User *U,
                                       ArrayRef<const Value *> Operands,
                                       TTI::TargetCostKind CostKind);

    bool isLegalMaskedLoad(Type *DataType, Align Alignment) const;

    bool isLegalMaskedStore(Type *DataType, Align Alignment) const;

    InstructionCost getArithmeticReductionCost(
        unsigned Opcode, VectorType *Ty, std::optional<FastMathFlags> FMF,
        TTI::TargetCostKind CostKind = TTI::TCK_RecipThroughput);

    bool shouldExpandReduction(const IntrinsicInst *II) const {
      switch (II->getIntrinsicID()) {
      case Intrinsic::vector_reduce_add:
      case Intrinsic::vector_reduce_fadd:
      case Intrinsic::vector_reduce_and:
      case Intrinsic::vector_reduce_or:
      case Intrinsic::vector_reduce_xor:
        return false;
      }
      return true;
    }

  private:

    const SHAVESubtarget *ST;
    const SHAVELowering *TLI;

    const SHAVESubtarget *getST() const { return ST; }

    const SHAVELowering *getTLI() const { return TLI; }
  };

} // namespace llvm


#endif // SHAVETARGETTRANSFORMINFO_H
