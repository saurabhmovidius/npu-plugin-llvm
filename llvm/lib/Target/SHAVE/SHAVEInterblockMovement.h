//===-- SHAVEInterblockMovement.h - Pre-Scheduling Pass ------*- C++ -*----===//
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

#pragma once

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/CodeGen/MachineBranchProbabilityInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

#include "SHAVE.h"
#include "SHAVEInstrInfo.h"
#include "SHAVELiveRanges.h"
#include "SHAVERegisterInfo.h"
#include "SHAVETargetMachine.h"
#include "SHAVEUtilities.h"

#include <list>
#include <unordered_map>

using namespace llvm;
using namespace SHAVE;
using namespace SHAVELiveRanges;

namespace SHAVEInterblockMovementNS {
  typedef std::unordered_map<unsigned int, VerboseLiveRanges> BlockLiveRanges;

  class InterblockMovement {
  private:
    struct MoveInfo {
      bool canMove = false;
      bool forcePredication = false;
      MachineInstr * instruction = nullptr;
      struct PredData {
        MachineBasicBlock::iterator position;
        SHAVECC::CondCode predicate;
        unsigned int ccReg;
      };
      std::list<PredData> predecessorData;
    };

    MachineBasicBlock &block;
    const SHAVEInstrInfo *SII = nullptr;
    const SHAVERegisterInfo *SRI;
    MachineLoopInfo *MLI = nullptr;
    SHAVEUtilities utilities;
    BlockLiveRanges &liveRanges;
    SmallVector<MachineBasicBlock::iterator, 4u> insertPoints;
    // Private member variables for the transformation cost model
    static constexpr size_t UnitCountsSize = SHAVE::FU_COUNT + 1; // +1 as functional units are indexed from 1
    typedef SmallVector<unsigned int, UnitCountsSize> UnitCounts;
    SmallVector<UnitCounts, 4u> usedUnits; // Number of instructions using each functional unit per predecessor
    SmallVector<int, 4u> addedCost; // Number of instruction cycles already added per predecessor
    SmallVector<bool, 4u> conservativeLimits; // Use conservative limits per predecessor
    bool bypassCostModel = false;
    const int addedCostBase = 0;
    SmallVector<BitVector, 4u> impactedByPredicated; // Per predecessor instructions that are potentially moved by predicated instructions

    SmallVector<MachineInstr *, 4u> movedInstructions;

    void moveInstruction(const MoveInfo &moveInfo);
    bool mustPredicate(MachineInstr &instruction) const;
    bool canMoveLoadStore(MachineInstr &instruction);
    bool tooExpensive(MoveInfo &info) const;
    bool generatePositionsAndPredicates(MoveInfo &moveInfo) const;
    MoveInfo getMoveInfo(MachineInstr &instruction);
    bool canMoveFromBlock() const;

  public:
    InterblockMovement(MachineBasicBlock &block,
                       const SHAVEInstrInfo *SII,
                       MachineLoopInfo *MLI,
                       AliasAnalysis *AA,
                       MachineBranchProbabilityInfo *BPI,
                       BlockLiveRanges &liveRanges);

    bool run();
  };
} // SHAVEInterblockMovement
