//===-- SHAVELowering.h - Instruction Selection -----------------*- C++ -*-===//
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

#ifndef TARGET_SHAVE_LOWERING_H
#define TARGET_SHAVE_LOWERING_H

#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/Constants.h"

namespace llvm {

  class SHAVETargetMachine;
  class SHAVESubtarget;

  namespace SHAVEISD {
    enum NodeType {
      FIRST_NUMBER = ISD::BUILTIN_OP_END,
      CMU_CM,

      // calling convention
      CALL,
      RETURN,

      // FIXME: Movidius - TODO: legacy, to be removed
      RET_FLAG,

      // for SHAVE, basic block nodes have no use, load symbolics
      LDISym,
      CMU_CPII,
      SELECT,
      VSELECT,
      SETCC,
      MIN,
      MAX,
      CLAMP,

      // u8 <--> f16 load and store helpers
      UITOFP_INREG,
      FPTOUI_INREG,

      // ACC and MAC nodes that take sequences of values.
      ACCP_SEQ,
      MACP_SEQ,
      ACCN_SEQ,
      MACN_SEQ,

      // i64 helpers
      I64_EXTRACT_LOW,
      I64_EXTRACT_HIGH,
      I64_CONCAT,

      // vector pseudo-instructions
      TRUNCATE_INREG,
      SHD,
      SHLV,
      SPLAT,
      LANESPLAT,
      ALIGNVEC,
      ROTATE_VECTOR,
      INTERLEAVE_VECTORS,
      INTERLEAVE_VECTORS_COMBINE,
      DEINTERLEAVE_VECTORS,
      COMBINE_VECTORS,
      COMPRESS_VECTOR,
      PERMUTE_VECTOR,
      PERMUTE_BLEND_VECTORS,
      UNPACK,
      VECTOR_EXTEND,
      EXTRACT_SUBVECTOR, 
      INSERT_SUBVECTOR,
      MASKED_STORE,
      MASKED_STORE_L,
      MASKED_STORE_H,

      // horiztontal vector instructions
      // The horizontal add instruction must ALWAYS be followed by a truncate to the original type. The SAU.SUMX
      // instructions generate a 32-bit result for all integer input types. LLVM doesn't provide a way to distinguish
      // between signed and unsigned integer add instructions. If we use SAU.SUMX.iX for unsigned X-bit values then
      // only the first X-bits of the result are correct. The same is true if we use SAU.SUMX.uX for signed X-bit values.
      HORIZONTAL_ADD,
      HORIZONTAL_FADD,
      HORIZONTAL_FADD_SUBVECTOR,
      HORIZONTAL_AND,
      HORIZONTAL_OR,
      HORIZONTAL_XOR,

      // dynamic allocations - used mainly for stack overflows check
      DYNALLOC,
#if 0
      // FIXME: Movidius - TODO: implementation of Memset. See SHAVESelectionDAGInfo.cpp for full details
      MEMSET_BLOCK
#endif

      // Vector loads
      LOAD64_LOW = ISD::FIRST_TARGET_MEMORY_OPCODE,
      LOAD64_HIGH
    };

    enum SHAVECondCode {
      NONE,
      EQ,
      GT,
      GTE,
      LT,
      LTE,
      NEQ
    };
  } // end namespace SHAVEISD

  struct ShuffleAnalysis {
    enum ShuffleLoweringStatus {
      Expand,                   // Cannot select the shuffle mask any better than LLVM's default implementation.
      Leaf,                     // This shuffle mask can be lowered in one step.
      TransformMask             // This shuffle mask needs to be transformed further before being lowered.
    };

    enum ShuffleLoweringAction {
      LowerInvalid,
      LowerUndefined,           // The result value is undefined.
      LowerIdentityV0,          // The result value is V0.
      LowerSplatV0,             // The result value is a splat from V0.
      LowerSwapLowHighV0,       // The result value is a swap of low/high parts of V0.
      LowerRotateV0,            // The result value is a rotation of V0.
      LowerTruncateV0,          // The result value is a truncation of V0.
      LowerAnyExt16V0,          // The result value is a 16-bit extension of V0.
      LowerAnyExt32V0,          // The result value is a 32-bit extension of V0.
      LowerSwizzleWordV0,       // The result value is a word swizzle of V0.
      LowerSwizzleHalfWordV0,   // The result value is a 16-bit swizzle of V0.
      LowerSwizzleByteV0,       // The result value is a byte swizzle of V0.
      LowerSwapVectors,         // The result value is the same shuffle but with V0 and V1 swapped.
      LowerConcatVectors,       // The result value is an alignment of V0 and V1.
      LowerInterleaveVectors,   // The result value is V0 and V1 interleaved.
      LowerDeinterleaveVectors, // The result value is V0 and V1 deinterleaved.
      LowerSubvectorInsert,     // The result value is V0 with a subvector of V1 inserted
      LowerShrVector,           // The result value is a right shifted value of V1
      LowerShlVector            // The result value is a left shifted value of V0
    };

    // Shuffle mask to analyse.
    ArrayRef<int> Mask;
    // Temporary mask.
    SmallVector<int, 16> TmpMask;
    // Return type for the shuffle operation.
    EVT VecVT;
    // Result of the analysis.
    ShuffleLoweringStatus Status;
    // Current lowering action to perform.
    ShuffleLoweringAction Action;
    // Total cost to lower this shuffle mask.
    int Cost;

    // Source vectors.
    SDValue V0;
    SDValue V1;

    // Elements of the mask that belong to V0.
    SmallVector<int, 16> MaskV0;
    // Elements of the mask that belong to V1.
    SmallVector<int, 16> MaskV1;

    // Whether elements from V0 come before elements from V1 or vice versa.
    bool V0BeforeV1;
    bool V1BeforeV0;
    // Whether elements from V0 follow the 'identity' pattern.
    bool V0Identity;
    // Whether the same element from V0 is repeated in the mask.
    bool SplatV0;
    // Whether all elements from V0/V1 come from the high part of the vector.
    bool AllHighV0;
    // Whether the shuffle can be lowered as a 8-element swizzle from V0.
    bool IsSwizzle8V0;
    // Whether the shuffle can be lowered to a subvector insert
    bool IsSubvectorInsert;
    // Which lane is repeated in the mask.
    int SplatLaneV0;
    // Smallest element index from V0/V1.
    int MinIdxV0;
    int MinIdxV1;
    // Largets element index from V0/V1.
    int MaxIdxV0;
    int MaxIdxV1;
    // Starting index for a subvector insert
    int SubvectorInsertIndex;
    // Size of the subvector to insert
    int SubvectorInsertSize;
    // There may need to be a subvector extract from the source associated with a subvector insert
    int SubvectorExtractIndex;
    // Number of undefined elements in the mask.
    unsigned NumUndef;

    // Resulting masks after applying 16/32-bit extension to V0.
    SmallVector<int, 8> AnyExt16Mask;
    SmallVector<int, 4> AnyExt32Mask;
    // Whether elements from V0 are extended using a 16-bit stride.
    bool IsAnyExt16V0;
    // Whether elements from V0 are extended using a 32-bit stride.
    bool IsAnyExt32V0;

  // Whether element indices form a linear sequence with an increment of 1/2/4.
    bool IsLinear1;
    bool IsLinear2;
    bool IsLinear4;
    // Whether element indixes form a linear sequence with an increment of 1/2/4
    // with potential wrap around
    bool IsLinear1WithWrapAround;
    bool IsLinear2WithWrapAround;
    bool IsLinear4WithWrapAround;
    // Whether the element indices are aligned to indicate a potential shift 
    // pattern (Myriad2.3)
    bool IsShiftCandidate;
    // First defined element in the mask.
    int FirstDefV0;
    // Last defined element in the mask.
    int LastDefV0;
    // Whether the first element is the only defined one.
    bool Singleton;

    // Whether or not the shuffle is a V0/V1 interleave
    enum InterleaveType {
      NoInterleave,
      FirstHalfInterleave,
      LastHalfInterleave
    };
    InterleaveType InterleaveType;

    // Concatenation offset used to align vectors.
    int ConcatOffset;
    // Offset used in 128-bit shifts (myriad2.3)
    int ShiftOffset;

    // Subtarget Object provided by Lowering
    const SHAVESubtarget& SHAVEST;

    ShuffleAnalysis(ArrayRef<int> mask, EVT VecVT, SDValue v0, SDValue v1, const SHAVESubtarget& shavest);
    bool analyse();

   unsigned getTruncOpcode(EVT &TruncVT) const;

  private:
    bool canLowerAsSwizzleByte(int offset);
    bool assignLeafAction(ShuffleLoweringAction action, int cost);
    bool assignTransformAction(ShuffleLoweringAction action, int cost,
                               ArrayRef<int> NewMask, SDValue LHS, SDValue RHS);
    bool analyseV0Only();
    bool analyseV1Only();
    bool analyseV0V1Mix();

    void dumpMask(raw_ostream &O);
  };

  class SHAVELowering : public TargetLowering {
    const SHAVESubtarget& SHAVEST;

  public:
    explicit SHAVELowering(const SHAVETargetMachine &TM);
    ~SHAVELowering();

    void initialiseLoweringActions(const SHAVETargetMachine &STM);

    bool hasFeature(unsigned int feature) const;

    void SHAVEv3Lowering();

    unsigned int getFunctionAlignment(const Function *f) const;

    EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context, EVT VT) const override;

    bool allowsMisalignedMemoryAccesses(
        EVT, unsigned AddrSpace = 0, Align Alignment = Align(1),
        MachineMemOperand::Flags Flags = MachineMemOperand::MONone,
        unsigned * /*Fast*/ = nullptr) const override;

    SDValue LowerFormalArguments(SDValue Chain,
            CallingConv::ID CallConv, bool isVarArg,
            const SmallVectorImpl<ISD::InputArg> &Ins,
            const SDLoc &dl, SelectionDAG &DAG,
            SmallVectorImpl<SDValue> &InVals) const override;

    bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
         bool isVarArg,
         const SmallVectorImpl<ISD::OutputArg> &Outs,
         LLVMContext &Context) const override;

    SDValue LowerReturn(SDValue Chain,
            CallingConv::ID CallConv, bool isVarArg,
            const SmallVectorImpl<ISD::OutputArg> &Outs,
            const SmallVectorImpl<SDValue> &OutVals,
            const SDLoc &dl, SelectionDAG &DAG) const override;

    SDValue LowerCall(CallLoweringInfo &/*CLI*/, SmallVectorImpl<SDValue> &/*InVals*/) const override;
    SDValue LowerOperation(SDValue op, SelectionDAG &DAG) const override;
    SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

    SDValue CombineINSERT_VECTOR_ELTIntoSPLAT(SDNode *N, DAGCombinerInfo &DCI) const;
    SDValue CombineINSERT_VECTOR_ELTIntoSHAVE_EXTRACT_SUBVECTOR(SDNode *N, DAGCombinerInfo &DCI) const;
// FIXME: Movidius - We currently don't have an implementation for subvector insert
//                   with a variable index. When there is one, this block can be re-enabled
#if 0
    SDValue CombineINSERT_VECTOR_ELTIntoSHAVE_INSERT_SUBVECTOR(SDNode *N, DAGCombinerInfo &DCI) const;
#endif
    SDValue CombineEXTRACT_VECTOR_ELT(SDNode *N, DAGCombinerInfo &DCI) const;
    SDValue CombineINSERT_VECTOR_ELT(SDNode *N, DAGCombinerInfo &DCI) const;
    SDValue CombineBUILD_VECTOR(SDNode *N, DAGCombinerInfo &DCI) const;
    SDValue CombineSELECT_CC(SDNode *N, DAGCombinerInfo &DCI) const;
    SDValue CombineVSELECT(SDNode *N, DAGCombinerInfo &DCI) const;
    SDValue CombineMIN_MAX(SDNode *N, DAGCombinerInfo &DCI) const;
    SDValue CombineSDIV(SDNode *N, DAGCombinerInfo &DCI) const;
    SDValue CombineSREM(SDNode *N, DAGCombinerInfo &DCI) const;
    SDValue CombineSINT_TO_FP(SDNode *N, DAGCombinerInfo &DCI) const;
    SDValue CombineFADD(SDNode *N, DAGCombinerInfo &DCI) const;
    SDValue CombineADD(SDNode *N, DAGCombinerInfo &DCI) const;

    bool isPartOfHorizontalChain(SDNode *N) const;

    void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results, SelectionDAG &DAG) const override;

    bool isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const override;
    SDValue LowerGlobalAddress(SDValue op, SelectionDAG &DAG) const;
    SDValue LowerJumpTable(SDValue op, SelectionDAG &DAG) const;

    const char *getTargetNodeName(unsigned op) const override;

    /// findRepresentativeClass - Return the largest legal super-reg register class
    /// of the register class for the specified type and its associated "cost".
    std::pair<const TargetRegisterClass*, uint8_t>
    findRepresentativeClass(const TargetRegisterInfo *TRI, MVT VT) const override;

    bool isTruncateFree(Type * /*Ty1*/, Type * /*Ty2*/) const override;
    bool isTruncateFree(EVT /*VT1*/, EVT /*VT2*/) const override;

    // Inline asm support
    std::pair<unsigned, const TargetRegisterClass*>
    getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                 StringRef Constraint,
                                 MVT VT) const override;

    TargetLowering::ConstraintType
    getConstraintType(StringRef Constraint) const override;

    std::vector<unsigned>
    getRegClassForInlineAsmConstraint(const std::string &Constraint, EVT VT) const;
    bool isVectorLoadExtDesirable(SDValue ExtVal) const override;

    bool isLegalAddressingMode(const DataLayout &DL, const AddrMode &AM, Type *Ty, unsigned AddrSpace, Instruction *I = nullptr) const override;
    bool getPostIndexedAddressParts(SDNode *N, SDNode * Op, SDValue &Base,
            SDValue &Offset, ISD::MemIndexedMode &AM, SelectionDAG &DAG) const override;

    static SDValue findScalarInsert(SDValue Src, int &laneIdx);

    TargetLoweringBase::LegalizeTypeAction
    getPreferredVectorAction(MVT VT) const override;

    MVT getScalarShiftAmountTy(const DataLayout &DL, EVT LHSTy) const override;

    bool isFNegFree(EVT VT) const override;
    bool isFAbsFree(EVT VT) const override;


  private:
    void InitializeScalarLoweringActions();
    void InitializeVectorLoweringActions();
    void InitializeLibCallLoweringActions();
    void InitializeSHAVEv3LoweringActions();

    void GenerateShuffleMaskForCONCAT_VECTORS(SmallVector<SmallVector<int, 8>, 2> &shuffleVectors, 
                                              EVT shuffleOutType, unsigned numOfInputVectors, 
                                              unsigned numElementsInInputVectors) const;
    SDValue SHAVEComparei64(SDValue LHS, SDValue RHS, ISD::CondCode CC, 
                            SDLoc Loc, SelectionDAG &DAG) const;

    SDValue SHAVELowerSELECT(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerSETCC_i64(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerSETCC(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerCM_SELECT(SDValue LHS, SDValue RHS, SDValue TrueVal,
                                SDValue FalseVal, ISD::CondCode CC,
                                SDLoc Loc, unsigned Opc,
                                SelectionDAG &DAG) const;
    SDValue SHAVELowerCM_SELECT_i64(SDValue LHS, SDValue RHS, SDValue TrueVal,
                                    SDValue FalseVal, ISD::CondCode CC,
                                    SDLoc Loc, SelectionDAG &DAG) const;
    SDValue SHAVELowerBR_CC_i64(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerBR_CC(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerBR_JT(SDValue Op, SelectionDAG &DAG) const;

    SDValue SHAVELowerConstantFP(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerConstantPool(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerINT_TO_FP(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerUINT_TO_FP(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerFP_TO_INT(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerFP_TO_UINT(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerSHR(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerSHL(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerSMUL_LOHI(SDValue Op, SelectionDAG &DAG) const;
    SDValue SHAVELowerUMUL_LOHI(SDValue Op, SelectionDAG &DAG) const;
    SDValue SHAVELowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
    SDValue SHAVELowerFROUND(SDValue Op, SelectionDAG &DAG) const;

    SDValue SHAVELowerEXTRACT_SUBVECTOR(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerVECTOR_SHUFFLE(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVEv3LowerVECTOR_SHUFFLE(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVEv3OptimizeBUILD_VECTOR(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVEv3LowerBUILD_VECTOR(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVEv3LowerEXTRACT_SUBVECTOR(SDValue op, SelectionDAG &DAG) const;
    SDValue OptimizeBUILD_VECTOR(SDNode *N, SelectionDAG &DAG,
                                 bool BeforeLegalize) const;
    SDValue TryConstantPool(SDValue, SelectionDAG &DAG) const;
    SDValue SHAVELowerBUILD_VECTOR(SDValue op, SelectionDAG &DAG) const;
    SDValue PickConstants(SDValue op, SelectionDAG &DAG, bool signedVal) const;
    SDValue SHAVELowerSHL_SRL_SRA_64bit(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELower64BitOp(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVEMatchHorizontalVectorOp(SDValue op, SDNode *& inputChain) const;
    SDValue SHAVELowerHorizontalOp(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerADD_SUB_ROTL_SHL_SRA_SRL(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerTRUNCATE(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerMUL(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerEXTRACT_VECTOR_ELT(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerCONCAT_VECTORS(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVEv3LowerSimpleCONCAT_VECTORS(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVEv3LowerCONCAT_VECTORS(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerSCALAR_TO_VECTOR(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerCTLZ_64bit(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerCTLZ(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerCTTZ(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerSIGN_EXTEND(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerZERO_EXTEND(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerANY_EXTEND(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerMLOAD(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerMSTORE(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVEv3LowerMSTORE(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerLOAD(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerSTORE(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerMIN_MAX(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerFTRUNC(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVEv3LowerFCOPYSIGN(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerFMINNUM_FMAXNUM(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVELowerFMINIMUM_FMAXIMUM(SDValue op, SelectionDAG &DAG) const;
    SDValue SHAVEv3LowerMathOperation(SDValue op, SelectionDAG &DAG) const;

    SDValue SHAVELowerVASTART(SDValue Op, SelectionDAG &DAG) const;
    SDValue SHAVELowerVAARG(SDValue Op, SelectionDAG &DAG) const;
    SDValue SHAVELowerVACOPY(SDValue Op, SelectionDAG &DAG) const;

    /// lower ISD::RETURNADDR and ISD::FRAMEADDR builtins
    /// only the 0 value for the argument is supported by this target
    SDValue SHAVELowerXADDR(SDValue Op, SelectionDAG &DAG) const;

    // lower ISD::DYNAMIC_STACKALLOC: used for handling variable size arrays or allocas
    SDValue SHAVELowerDYNAMIC_STACKALLOC(SDValue Op, SelectionDAG &DAG) const;

    MachineBasicBlock *EmitInstrWithCustomInserter(MachineInstr &MI, MachineBasicBlock *BB) const override;
    
    MachineBasicBlock *EmitCustomCall(MachineInstr *MI, MachineBasicBlock *BB) const;
    MachineBasicBlock *EmitCustomReturn(MachineInstr *MI, MachineBasicBlock *BB) const;
    MachineBasicBlock *EmitCustomEXTRACT_SUBVECTOR(MachineInstr *MI, MachineBasicBlock *BB) const;
    MachineBasicBlock *EmitCustomINSERT_SUBVECTOR(MachineInstr *MI, MachineBasicBlock *BB) const;
    MachineBasicBlock *EmitCustomDYNALLOC(MachineInstr *MI, MachineBasicBlock *MBB) const;
  };


  struct SHAVEArgument {
      // Argument position
      int Number;

      // Calling convention argument position
      int CCNumber;

      // Abstract argument number
      int AbstractNumber;

      // True if this argument is part from an abstract type
      bool HasAbstractParent;

      // Position inside the parent
      int Index;

      // Parent element size
      int ParentSize;

      // Registers allocated to this abstract element
      SmallVector<unsigned, 0> Registers;

      // Position of allocated register from the above list
      int RegisterIndex;

      // Size in bytes
      int Size;

      // Extended value type
      EVT BaseType;

      // Is pass by value (e.g. a struct)
      bool ByValue;

      // Size in bytes for by-value args
      unsigned ByValueSize;

      // Alignment for by-value args
      unsigned ByValueAlignment;

      SHAVEArgument() : ByValue(false) {}
  };

  //
  // SHAVEv3 Shuffle Lowering
  //

  class SHAVEv3ShuffleLowering {
  private:
    enum SourceVector {
      Left = 0,
      Right,

      NumSources
    };

    // Ordering is important here, the order of this enum defines the order of precedence for shuffle operations
    enum ShufflePattern {
      Identity = 0,
      Splat,
      ShiftLeft,
      ShiftRight,
      Rotate,
      Interleave,
      DeInterleave,
      Alignvec,
      Reverse,
      Combine,
      Swizzle,
      Compress,
      Permute,
      PermuteBlend,

      NumPatterns
    };

    struct ShuffleInfo {
      unsigned int splatIndex = -1, offset = -1, interleaveOutput = -1;
      uint64_t compressMask = 0;

      std::vector<bool> combineMask;
      unsigned int combineSize = 0;

      bool interleaveBothLeft = false;
    };

    const SHAVESubtarget &SHAVEST;

    SDValue sourceA, sourceB;
    const ArrayRef<int> mask;
    int inputASize, inputBSize;
    EVT outputType;
    SDLoc dbgLoc;
    SelectionDAG &DAG;
    int threshold;

    ShufflePattern usedPattern;
    ShuffleInfo usedInfo;

    SDValue getSource(SourceVector source) const { return source == SourceVector::Left ? sourceA : sourceB; }
    SDValue getOtherSource(SourceVector source) const { return source == SourceVector::Left ? sourceB : sourceA; }

    // Pattern matching functions
    void checkIdentity(std::vector<bool> &match, ShuffleInfo &info, SourceVector source);
    void checkSplat(std::vector<bool> &match, ShuffleInfo &info, SourceVector source);
    void checkRotate(std::vector<bool> &match, ShuffleInfo &info, SourceVector source);
    void checkInterleave(std::vector<bool> &match, ShuffleInfo &info, SourceVector source);
    void checkDeInterleave(std::vector<bool> &match, ShuffleInfo &info, SourceVector source);
    void checkAlignvec(std::vector<bool> &match, ShuffleInfo &info, SourceVector source);
    void checkCombine(std::vector<bool> &match, ShuffleInfo &info, SourceVector source);
    void checkCompress(std::vector<bool>& match, ShuffleInfo& info, SourceVector source);
    void checkPermuteBlend(std::vector<bool>& match, ShuffleInfo& info, SourceVector source);
    void checkPermute(std::vector<bool> &match, ShuffleInfo &info, SourceVector source);

    // Lowering helper functions
    SDValue getInterleaveOtherSource(SourceVector source) const;

    // Lowering functions
    SDValue lowerSplat(const std::vector<bool> &match, SourceVector source);
    SDValue lowerRotate(const std::vector<bool> &match, SourceVector source);
    SDValue lowerInterleave(const std::vector<bool> &match, SourceVector source);
    SDValue lowerDeInterleave(const std::vector<bool> &match, SourceVector source);
    SDValue lowerAlignvec(const std::vector<bool> &match, SourceVector source);
    SDValue lowerCombine(const std::vector<bool> &match, SourceVector source);
    SDValue lowerCompress(const std::vector<bool>& match, SourceVector source);
    SDValue lowerPermute(const std::vector<bool>& match, SourceVector source, bool blend = false);

    SDValue insertCopies(SDValue destVector, std::vector<bool> &match);

  public:
    SHAVEv3ShuffleLowering(const SHAVESubtarget& SHAVEST, ShuffleVectorSDNode * op, SelectionDAG &DAG) :
      SHAVEST(SHAVEST),
      sourceA(op->getOperand(0)), sourceB(op->getOperand(1)), mask(op->getMask()),
      inputASize((int) op->getOperand(0).getValueType().getVectorNumElements()),
      inputBSize((int) op->getOperand(1).getValueType().getVectorNumElements()),
      outputType(op->getValueType(0)),
      dbgLoc(SDValue(op, 0)), DAG(DAG),
      threshold(-1), usedPattern(NumPatterns) {}

    SHAVEv3ShuffleLowering(const SHAVESubtarget& SHAVEST, 
                           SDValue sourceA, SDValue sourceB, const ArrayRef<int> mask,
                           const EVT &outputType, SDLoc &dbgLoc,
                           SelectionDAG &DAG, int threshold) :
      SHAVEST(SHAVEST),
      sourceA(sourceA), sourceB(sourceB), mask(mask),
      inputASize(sourceA.getValueType().getVectorNumElements()),
      inputBSize(sourceB.getValueType().getVectorNumElements()),
      outputType(outputType),
      dbgLoc(dbgLoc), DAG(DAG),
      threshold(threshold), usedPattern(NumPatterns) {}

    SDValue lower();
    bool isCompress() { return usedPattern == ShufflePattern::Compress; }
    uint64_t getCompressMask() { return usedInfo.compressMask; }
  };

} //namespace llvm

#endif
