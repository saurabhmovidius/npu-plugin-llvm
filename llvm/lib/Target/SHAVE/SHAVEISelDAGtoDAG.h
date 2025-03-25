//===-- SHAVEISelDAGtoDAG.h - Instruction Selection -------------*- C++ -*-===//
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

#ifndef SHAVEISELDAGTODAG_H
#define SHAVEISELDAGTODAG_H (1)


#define DEBUG_TYPE "shave-isel"

#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "SHAVE.h"
#include "SHAVEInstrInfo.h"
#include "SHAVELowering.h"
#include "SHAVETargetMachine.h"

using namespace llvm;


namespace {
  class SHAVEISelDAGtoDAG : public SelectionDAGISel {
    const SHAVESubtarget &Subtarget;

  public:
    static char ID;

    explicit SHAVEISelDAGtoDAG(SHAVETargetMachine &TM);
    ~SHAVEISelDAGtoDAG();

    StringRef getPassName() const override {
      return "SHAVE DAG->DAG Pattern Instruction Selection";
    }

    bool hasFeature(unsigned int feature) const;

#include "SHAVEGenDAGISel.inc"

    void Select(SDNode *N) override;
    void PreprocessISelDAG() override;
    void PostprocessISelDAG() override;

  private:
    // Check for Myriad2.3 stores node to be selected through the tablegen patterns
    bool isSupportedVRF128Store(SDNode *N);

    /// getLastCopyFromReg - find the end of the chaining copies from regs
    /// that use node N; if no copy is found, return N
    static SDNode *getLastCopyFromReg(SDNode *N);

    /// fixExtensionNode - remove SIGN_EXTEND, ANY_EXTEND and ZERO_EXTEND nodes
    /// which extend from a value type to the same type (so there's no extension)
    void fixExtensionNode(SelectionDAG::allnodes_iterator &nodeIt);

    // Add default operands for the instruction.
    void finaliseOperands(SmallVectorImpl<SDValue> &Ops,
                          SDLoc dbgLoc,
                          unsigned Opc = 0,
                          SDValue Chain = SDValue()) const;

    // Create a machine node with default operands added.
    MachineSDNode *getMachineNodeWithDefaultOps(unsigned Opc, SDLoc dbgLoc, EVT VT,
                                                SmallVectorImpl<SDValue> &Ops,
                                                SDValue Chain = SDValue()) const;
    MachineSDNode *getMachineNodeWithDefaultOps(unsigned Opc, SDLoc dbgLoc,
                                                SDVTList VTList,
                                                SmallVectorImpl<SDValue> &Ops,
                                                SDValue Chain = SDValue()) const;

    // Morph a machine node with default operands added.
    void SelectNodeWithDefaultOps(SDNode *N, unsigned Opc, EVT VT,
                                  SmallVectorImpl<SDValue> &Ops,
                                  SDValue Chain = SDValue()) const;
    void SelectNodeWithDefaultOps(SDNode *N, unsigned Opc, SDVTList VTList,
                                  SmallVectorImpl<SDValue> &Ops,
                                  SDValue Chain = SDValue()) const;

    bool SelectADDRriImpl(SDValue &Addr, SDValue &Base, SDValue &Offset, unsigned int shift = 0);
    bool SelectADDRri(SDValue &Addr, SDValue &Base, SDValue &Offset);
    bool SelectADDRri5(SDValue &Addr, SDValue &Base, SDValue &Offset);
    bool SelectADDRri4(SDValue &Addr, SDValue &Base, SDValue &Offset);
    bool SelectADDRri3(SDValue &Addr, SDValue &Base, SDValue &Offset);
    bool SelectADDRri2(SDValue &Addr, SDValue &Base, SDValue &Offset);
    bool SelectADDRrr(SDValue &Addr, SDValue &Base, SDValue &Offset);
    bool SelectADDRr(SDValue &Addr, SDValue &Base);

    void SelectCLAMP(SDNode *N);
    void SelectScalarLOAD(SDNode *N);
    void SelectScalarSTORE(SDNode *N);
    void SelectLOAD64_LOW_HIGH(SDNode *N);
    void SelectVectorLOAD(SDNode *N);
    void SelectVectorSTORE(SDNode *N);
    void SelectIndexedLOAD(SDNode *N);
    void SelectVectorUNPACK(SDNode *N);
    void SelectVectorSHUFFLE(SDNode *N);
    SDNode *VectorSHUFFLE(ShuffleAnalysis &SA, SDLoc dbgLoc);
    SDNode *VectorSHUFFLE_Concat(ShuffleAnalysis &SA, SDLoc dbgLoc);
    SDNode *VectorSHUFFLE_Shift(ShuffleAnalysis &SA, SDLoc dbgLoc, bool isLeftShift);
    SDNode *VectorSHUFFLE_LowHighSwap(ShuffleAnalysis &SA, SDLoc dbgLoc);
    SDNode *VectorSHUFFLE_AnyExt(ShuffleAnalysis &SA, SDLoc dbgLoc,
                                 EVT VT, unsigned ExtOpc = 0);
    SDNode *VectorSHUFFLE_Truncate(ShuffleAnalysis &SA, SDLoc dbgLoc);
    SDNode *VectorSHUFFLE_Interleave(ShuffleAnalysis &SA, SDLoc dbgLoc);
    SDNode *VectorSHUFFLE_Deinterleave(ShuffleAnalysis &SA, SDLoc dbgLoc);
    SDNode *VectorSHUFFLE_Rotate(ShuffleAnalysis &SA, SDLoc dbgLoc);
    SDNode *VectorSHUFFLE_SourceSwap(ShuffleAnalysis &SA, SDLoc dbgLoc);
    SDNode *VectorSHUFFLE_Splat(ShuffleAnalysis &SA, SDLoc dbgLoc);
    SDNode *VectorShuffleOrInputSwizzle(ShuffleAnalysis &SA, SDLoc dbgLoc, SDNode *N);
    void SelectEXTRACT_SUBVECTOR(SDNode *N);
    void SelectBUILD_VECTOR(SDNode *N);
    void SelectVectorSIGN_EXTEND_INREG(SDNode *N);
    void SelectVectorSINT_TO_FP(SDNode *N);
    SDNode *CombineVectorExtensions(SDNode *N);
    void SelectVectorANY_EXTEND(SDNode *N);
    void SelectVectorTRUNCATE(SDNode *N);

    SDNode *createSwizzle(SDLoc dbgLoc, int Opcode, SDValue src,
                          llvm::ArrayRef<int> mask, int offset=0);
    SDNode *createHalfWordSwizzle(SDLoc dbgLoc, SDValue src, ArrayRef<int> Mask);
    SDNode *createByteSwizzle(SDLoc dbgLoc, SDValue src, ArrayRef<int> Mask);

    SDValue ZeroHighBits(SDValue Src, SDLoc dbgLoc, EVT DestVT);
    SDValue VectorTRUNCATE(SDValue SrcVal, SDLoc dbgLoc, EVT DestVT);

    void SelectImmediateLoad(SDNode *N);
    void SelectImmediateLoadFP(SDNode *N);
    void SelectCompare(SDNode *N);
    unsigned SelectCompareZero(SDNode *N, SDValue &LHS, SDValue RHS);

    SDNode *FindLHSUserForSwizzle(SDNode *N) const;

    void SelectIntrinsic(SDNode *N);
    void SelectIntrinsicGetcpuid(SDNode *N);
    void SelectIntrinsicTailWriteback(SDNode *N);

    void SelectACCP_SEQ(SDNode *N);

    SDNode *SelectINSERT_VECTOR_ELT_ShiftInto(SDNode *N);
    SDNode *SelectEXTRACT_VECTOR_ELTImpl(SDValue vector, SDValue index, MVT vectorType, SDLoc dbgLoc);
    void SelectEXTRACT_VECTOR_ELT(SDNode *N);
    SDNode *SelectINSERT_VECTOR_ELT_VariableIndexImpl(SDValue vector, SDValue value, SDValue index, MVT vectorType, MVT valueType, SDLoc dbgLoc);
    void SelectINSERT_VECTOR_ELT_VariableIndex(SDNode *N);
    void SelectSHAVE_EXTRACT_SUBVECTOR_ConstantIndex(SDNode *N);
    void SelectMASKED_STORE(SDNode *N);
    void SelectCompressVector(SDNode *N);

    void SelectCTPOP(SDNode *N);
    void SelectFDIV(SDNode *N);

    SDNode *SelectBITCASTImpl(SDValue source, MVT destinationType, SDLoc dbgLoc);
  };
} // End of anonymous namespace


#endif // SHAVEISELDAGTODAG_H
