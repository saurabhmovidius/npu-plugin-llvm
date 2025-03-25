//===-- SHAVEInstInfo.h - Instruction Information ---------------*- C++ -*-===//
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

#ifndef SHAVEINSTRINFO_H
#define SHAVEINSTRINFO_H (1)


#include "llvm/ADT/SmallBitVector.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSchedule.h"
#include "llvm/Support/BranchProbability.h"

#include "MCTargetDesc/SHAVEBaseInfo.h"
#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVE.h"
#include "SHAVERegisterInfo.h"

#include <bitset>

#define GET_INSTRINFO_HEADER
#include "SHAVEGenInstrInfo.inc"
#include "SHAVEGenConflicts.inc"

namespace llvm {
  struct MCSchedClassDesc;
  class MachineInstrBuilder;

  namespace SHAVE {
    // This must be kept in sync with SHAVESchedule.td
    enum SHAVEFuncUnit {
      NONE = 0,
      IAU = 1,
      SAU = 2,
      VAU = 3,
      PEU = 4,
      CMU = 5,
      BRU = 6,
      LSU0 = 7,
      LSU1 = 8,
      FU_COUNT = LSU1
    };

    // this must be kept in sync with SHAVESchedule.td
    enum SHAVEPort {
      NONE_RW = 0,
      VRF_R0,
      VRF_R1,
      VRF_R2,
      VRF_R3,
      VRF_R4,
      VRF_R5,

      VRF_W0,
      VRF_W1,
      VRF_W2,
      VRF_W3,
      VRF_W4,
      VRF_W5,

      IRF_R0,
      IRF_R1,
      IRF_R2,
      IRF_R3,
      IRF_R4,
      IRF_R5,
      IRF_R6,
      IRF_R7,
      IRF_R8,
      IRF_R9,
      IRF_R10,
      IRF_R11,

      IRF_W0,
      IRF_W1,
      IRF_W2,
      IRF_W3,
      IRF_W4,
      IRF_W5,

      MEM_R,
      MEM_W
    };
  }

  struct SHAVEOpSchedInfo {
    unsigned Latency;
    unsigned Reg;
    bool IsImplicit;
    bool IsDef;

    void Clear() {
      Latency = Reg = 0;
      IsImplicit = IsDef = false;
    }
  };

  // Bit-mask representing functional units
  typedef uint16_t FUnitMask;
  const FUnitMask FmNone = 0;
  const FUnitMask FmIAU = (1u << 0);
  const FUnitMask FmSAU = (1u << 1);
  const FUnitMask FmVAU = (1u << 2);
  const FUnitMask FmPEU = (1u << 3);
  const FUnitMask FmCMU = (1u << 4);
  const FUnitMask FmBRU = (1u << 5);
  const FUnitMask FmLSU0 = (1u << 6);
  const FUnitMask FmLSU1 = (1u << 7);
  const FUnitMask FmAllUnits = (FmIAU | FmSAU | FmVAU | FmPEU | FmCMU | FmBRU | FmLSU0 | FmLSU1);

  // Bitfield with one bit per scheduling resource (functional unit, ports,
  // etc). Given two instructions, if any bit is set in both bitfields then the
  // instructions cannot be scheduled in the same cycle.
  // The lower 8 bits are for functional units, higher bits are for ports or
  // scheduling resources, depending on the model used.
  class ResourceField : public SmallBitVector {
    // This is the maximum number of bits that SmallBitVector can store inline
    // on 64-bit systems. This should be enough for the 8 functional units and
    // the 44 ports (inlcuding MEM_R/MEM_W which might not be used any more).
    const static unsigned TOTAL_BITS = 57;
    const static unsigned FUNIT_BITS = 8;

  public:
    ResourceField() : SmallBitVector(TOTAL_BITS) {}

    bool IsUnitUsed(unsigned FUnit) const {
      if (!FUnit || FUnit > FUNIT_BITS)
        return false;
      return (*this)[FUnit - 1];
    }

    bool IsPortUsed(unsigned Port) const {
      if (!Port)
        return false;
      return (*this)[FUNIT_BITS + Port - 1];
    }

    void addUnit(unsigned FUnit) {
      if (!FUnit || FUnit > FUNIT_BITS)
        return;
      this->set(FUnit - 1);
    }

    void addPort(unsigned Port) {
      if (!Port)
        return;
      this->set(FUNIT_BITS + Port - 1);
    }

    void clearUnit(unsigned FUnit) {
      if (!FUnit || FUnit > FUNIT_BITS)
        return;
      this->reset(FUnit - 1);
    }

    void clearPort(unsigned Port) {
      if (!Port)
        return;
      this->reset(FUNIT_BITS + Port - 1);
    }
  };

  struct SHAVEPortUse {
    unsigned Latency;
    uint64_t Port;

    void Clear() {
      Latency = Port = 0;
    }
  };

  struct SHAVEResUse {
    unsigned Latency;
    ResourceField Res;
  };

  class SHAVEInstrInfo : public SHAVEGenInstrInfo {
    const SHAVESubtarget &SHAVEST;
    const SHAVERegisterInfo SHAVERegInfo;

    // FIXME: Movidius - this is a wild guess...
    // constants used by the branch predictor, may change...
    float AvgILP; // Estimate for average ILP
    float MinILP; // Estimate for minimum ILP
    // Number of instruction in a basic block that can be scheduled to obtain the AvgILP
    unsigned AvgILPThreshold;
    // instr | ILP
    //  1    | 0.64
    //  2    | 0.68
    // 	3    | 0.72
    // ............
    // 10    | 1.0
    // 11    | 1.04
    // ............
    // 19 	 | 1.36
    // 20    | 1.4
    // > 20  | 1.4

    float predictAverageILP(unsigned numOfInstructions) const {
      float ILP = (numOfInstructions >= AvgILPThreshold) ? AvgILP
              : (numOfInstructions / AvgILPThreshold) * (AvgILP - MinILP)
                      + MinILP;
      return ILP;
    }

    int nDelaySlots;
    unsigned loadLatency;

  public:
    enum FrameContext_t {
      inFunctionBody,
      inFunctionPrologue,
      inFunctionEpilogue
    };

    explicit SHAVEInstrInfo(SHAVESubtarget &ST);

    void initialiseLatencies();

    const SHAVERegisterInfo *getSHAVERegisterInfo() const {
      return &SHAVERegInfo;
    }

    static bool isReadPort(unsigned port) {
      return ((SHAVE::VRF_R0 <= port) && (SHAVE::VRF_R5 >= port))
                || ((SHAVE::IRF_R0 <= port) && (SHAVE::IRF_R11 >= port));
    }

    static bool isWritePort(unsigned port) {
        return ((SHAVE::VRF_W0 <= port) && (SHAVE::VRF_W5 >= port))
                || ((SHAVE::IRF_W0 <= port) && (SHAVE::IRF_W5 >= port));
    }

    static bool isMem(unsigned port) {
      return ((SHAVE::MEM_R == port) || (SHAVE::MEM_W == port));
    }

    MachineBasicBlock::instr_iterator FindFirstUser(MachineBasicBlock &BB,
                                              MachineBasicBlock::iterator MI,
                                              MachineOperand* &UseMO) const;

    unsigned isLoadFromStackSlot(const MachineInstr &MI, int &FrameIndex) const override;
    unsigned isStoreToStackSlot(const MachineInstr &MI, int &FrameIndex) const override;

    void storeRegToStackSlot(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MI, Register SrcReg,
                             bool isKill, int FrameIndex,
                             const TargetRegisterClass *RC,
                             const TargetRegisterInfo *TRI,
                             Register VReg) const override;
    void loadRegFromStackSlot(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MI, Register DestReg,
                              int FrameIndex, const TargetRegisterClass *RC,
                              const TargetRegisterInfo *TRI,
                              Register VReg) const override;

    void storeRegToStashReg(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator I, Register SrcReg, bool isKill,
                            unsigned VirtReg, Register StashReg, const TargetRegisterClass *RC,
                            const TargetRegisterInfo *TRI) const override;
    void loadRegFromStashReg(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator I, Register DestReg, unsigned VirtReg, Register StashReg,
                             const TargetRegisterClass *RC, const TargetRegisterInfo *TRI) const override;
    void copyPhysReg(MachineBasicBlock &MBB,
                     MachineBasicBlock::iterator MI, const DebugLoc &DL, MCRegister DestReg,
                     MCRegister SrcReg, bool KillSrc) const override;

    void finaliseMI(const MachineInstrBuilder &MIB, FrameContext_t frameInstructionType = inFunctionBody) const;

    SHAVECC::CondCode getPredicateCC(const MachineInstr& MI) const;
    void setPredicateCC(MachineInstr &MI, SHAVECC::CondCode newCC) const;

    // If Conversion methods:
    bool isPredicated(const MachineInstr &MI) const override;

    unsigned getPredicationCost(const MachineInstr &MI) const override;

    bool PredicateInstruction(MachineInstr &MI, ArrayRef<MachineOperand> Pred) const override;

    bool SubsumesPredicate(ArrayRef<MachineOperand> Pred1,
                           ArrayRef<MachineOperand> Pred2) const override;

    bool ClobbersPredicate(MachineInstr &MI, std::vector<MachineOperand> &Pred, bool SkipDead) const override;

    bool isLegalToIfCvt(MachineBasicBlock &MBB) const;

    bool isProfitableToIfCvt(MachineBasicBlock &MBB, unsigned NumCyles,
                             unsigned ExtraPredCycles, BranchProbability Probability) const override;

    bool isProfitableToIfCvt(MachineBasicBlock &TMBB, unsigned NumTCycles,
                             unsigned ExtraTCycles, MachineBasicBlock &FMBB,
                             unsigned NumFCycles, unsigned ExtraFCycles,
                             BranchProbability Probability) const override;

    bool isProfitableToDupForIfCvt(MachineBasicBlock &MBB, unsigned NumCyles,
                                   BranchProbability Probability) const override;

    bool reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const override;

    // Branch Folder methods:
    bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                       MachineBasicBlock *&FBB, SmallVectorImpl<MachineOperand> &Cond,
                       bool AllowModify = false) const override;

    unsigned removeBranch(MachineBasicBlock &MBB, int *BytesRemoved) const override;

    unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                          MachineBasicBlock *FBB,
                          ArrayRef<MachineOperand> Cond,
                          const DebugLoc &DL, int *BytesRemoved) const override;

    bool isLegalToSplitMBBAt(MachineBasicBlock &MBB,
      MachineBasicBlock::iterator MBBI) const override;

    void ExpandMaskedStore(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    bool ExpandSelect(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandSetCC(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandSetCCVector(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;

    void ExpandJmpToLabel(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandLOAD64_LOW_HIGH(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandLDV128(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandSTV128(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandLDV512(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandSTV512(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandLDOI64(MachineBasicBlock &BB,
                      MachineBasicBlock::iterator MI) const;
    void ExpandSTOI64(MachineBasicBlock &BB,
                      MachineBasicBlock::iterator MI) const;
    void ExpandSTO_VRF(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandLDImm32(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandUnpack(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandZeroHi(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandVECTOR_EXTEND(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    bool ExpandInputSwizzle8(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
    void ExpandACCSequence(MachineBasicBlock &BB, MachineBasicBlock::iterator MI, bool isFloatType, bool isVectorType,
                           LiveIntervals * LIS = nullptr) const;
    void ExpandMACPSequence(MachineBasicBlock &BB, MachineBasicBlock::iterator MI, bool isFloatType, bool isVectorType,
                            LiveIntervals * LIS = nullptr) const;

#if 0
    // FIXME: Movidius - TODO: implementation of Memset.  See 'SHAVESelectionDAGInfo.cpp' for full details
    void ExpandMemsetBlock(MachineBasicBlock &BB, MachineBasicBlock::iterator MI) const;
#endif

    bool expandPostRAPseudo(MachineInstr &MI) const override;

    bool isReallyTriviallyReMaterializable(const MachineInstr &MI) const override;

    // Branch instruction delay slot
    int getDelaySlots() const { return nDelaySlots; }
    unsigned int getLoadLatency() const { return loadLatency; }

    unsigned getISAIntrinsicOpcode(unsigned intrinsicID) const;

    bool findLoadStoreBase(const MachineInstr *MI, unsigned int *Reg, bool isHigh) const;
    bool findLoadStoreOffset(const MachineInstr *MI, int *Offset = nullptr) const;
    bool isBranchInstruction(const MachineInstr *MI) const;
    unsigned GetACCPZOpcode(unsigned Opc) const;
    unsigned GetACCNZOpcode(unsigned Opc) const;
    unsigned GetACCPOpcode(unsigned Opc) const;
    unsigned GetACCPWOpcode(unsigned Opc) const;
    unsigned GetACCNOpcode(unsigned Opc) const;
    unsigned GetACCNWOpcode(unsigned Opc) const;
    unsigned GetMACPZOpcode(unsigned Opc) const;
    unsigned GetMACNZOpcode(unsigned Opc) const;
    unsigned GetMACPOpcode(unsigned Opc) const;
    unsigned GetMACPWOpcode(unsigned Opc) const;
    unsigned GetMACNOpcode(unsigned Opc) const;
    unsigned GetMACNWOpcode(unsigned Opc) const;

    // Return the index of the operand that contains the 'dynamic'
    // functional unit ID or a negative value if not present.
    int getFUnitOperandIndex(const MachineInstr *MI) const;

    // Return the instruction's functional unit.
    unsigned GetFunctionalUnit(const MachineInstr *MI) const;

    // Return the instruction's functional unit. This will return SHAVE::NONE
    // for LSU instructions which have 'dynamic' functional unit operands.
    unsigned GetFunctionalUnit(unsigned Opc) const;
    FUnitMask GetFunctionalUnitMask(const MachineInstr *MI) const;
    FUnitMask GetFunctionalUnitMask(unsigned Opc) const;
    unsigned FUnitsToPCXXMask(FUnitMask FUnits) const;
    FUnitMask PCXXMaskToFUnits(int64_t Mask) const;
    unsigned GetSchedMaxLatency(const MachineInstr *MI) const;

    void GetSchedPortRange(const MachineInstr * MI, TargetSchedModel::ProcResIter &begin, TargetSchedModel::ProcResIter &end) const;

    unsigned GetSchedLatency(const MachineInstr *MI, unsigned OpIdx, bool *OpFound = nullptr) const;

    unsigned getInstrLatency(const InstrItineraryData *ItinData,
                             const MachineInstr &MI,
                             unsigned *PredCost = nullptr) const override;
    unsigned getInstrLatency(const InstrItineraryData *ItinData,
                             SDNode *Node) const override;

    bool isPredicate(MachineInstr *MI, SHAVECC::CondCode &CC,
                     FUnitMask &PredUnits) const;
    bool setPredicate(MachineInstr *MI, SHAVECC::CondCode CC,
                      FUnitMask PredUnits) const;
    bool hasFeature(unsigned int feature) const;
    bool hasSAU() const;
    bool hasVAU() const;
    bool isStashRetrieveEnabledForArch() const;
    unsigned int getCMXCutSize() const;
    unsigned int getCMXNumberOfCuts() const;
    unsigned int getBRARange() const;
    unsigned int getLDOSTO_OffsetBits() const;
    bool GetSchedOperand(const MachineInstr *MI, unsigned OpIdx, SHAVEOpSchedInfo &Info) const;

    bool tryUseRPI(MachineBasicBlock &BB, MachineInstr * branchInstruction) const;

  private:
    const MCSchedClassDesc *GetSchedClass(const MachineInstr *MI) const;
    bool isBranchInstruction(const MCInstrDesc &Desc) const;
    unsigned TranslateLatency(const MachineInstr *MI, int latency) const;

    // The upper and lower bounds for the permitted number of branch-delay slots
    const int minBranchDelaySlots = 0;
    const int maxBranchDelaySlots = INT_MAX;

    // The upper and lower bounds for the permitted memory load latency
    const unsigned minLoadLatencyPreNPU4 = 6;
    const unsigned minLoadLatencyPostNPU4 = 8;
    const unsigned maxLoadLatency = UINT_MAX;
  };
} // end namespace llvm


#endif // SHAVEINSTRINFO_H
