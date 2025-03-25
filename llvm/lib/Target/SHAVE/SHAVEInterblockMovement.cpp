//===-- SHAVEInterblockMovement.cpp - Pre-Scheduling Pass ----*- C++ -*----===//
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

#define DEBUG_TYPE "shave-interblock-movement"

#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"

#include "SHAVE.h"
#include "SHAVEInterblockMovement.h"

using namespace llvm;
using namespace SHAVEInterblockMovementNS;

namespace {
  class SHAVEInterblockMovement : public MachineFunctionPass {
  public:
    static char ID;

    SHAVEInterblockMovement() : MachineFunctionPass(ID) {}

    StringRef getPassName() const override {
      return "SHAVE Interblock Instruction Movement Pass";
    }

    bool runOnMachineFunction(MachineFunction& MF) override {
      const SHAVETargetMachine &TM = static_cast<const SHAVETargetMachine&>(MF.getTarget());
      const SHAVEInstrInfo *SII = TM.getSubtargetImpl()->getInstrInfo();
      MachineLoopInfo *MLI = &getAnalysis<MachineLoopInfo>();
      AliasAnalysis *AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();
      MachineBranchProbabilityInfo *BPI = &getAnalysis<MachineBranchProbabilityInfo>();

      BlockLiveRanges liveRanges;
      for (MachineBasicBlock& block : MF)
        liveRanges.emplace(block.getNumber(), VerboseLiveRanges(block, SII));

      bool modified = false;

      // Traverse the blocks in the function bottom-up
      for (auto it = MF.rbegin(); it != MF.rend(); ++it) {
        // Regenerate the live ranges for this block to account for any new instructions hoisted from
        // a successor block in a previous run
        auto &block = *it;
        liveRanges.erase(block.getNumber());
        liveRanges.emplace(block.getNumber(), VerboseLiveRanges(block, SII));

        SHAVEInterblockMovementNS::InterblockMovement mover(block, SII, MLI, AA, BPI, liveRanges);
        modified = mover.run();
      }

      return modified;
    }

    void getAnalysisUsage(AnalysisUsage& AU) const override {
      AU.addRequired<MachineLoopInfo>();
      AU.addRequired<AAResultsWrapperPass>();
      AU.addRequired<MachineBranchProbabilityInfo>();
      MachineFunctionPass::getAnalysisUsage(AU);
    }
  };
}

char SHAVEInterblockMovement::ID = 0;

INITIALIZE_PASS_BEGIN(SHAVEInterblockMovement, "shaveinterblockmovementpass", "SHAVE Interblock Movement Pass", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(MachineBranchProbabilityInfo)
INITIALIZE_PASS_END(SHAVEInterblockMovement, "shaveinterblockmovementpass", "SHAVE Interblock Movement Pass", false, false)

MachineFunctionPass* llvm::createSHAVEInterblockMovementPass() {
  return new SHAVEInterblockMovement();
}

//
// Definition of private member functions of InterblockMovement
//

void InterblockMovement::moveInstruction(const MoveInfo& moveInfo) {
  auto predecessorIt = block.pred_begin();
  unsigned int predecessorIdx = 0;
  MachineFunction &function = *block.getParent();

  for (const auto &predData : moveInfo.predecessorData) {
    MachineBasicBlock *predecessor = *predecessorIt;
    DEBUG(dbgs() << "    Cloning instruction into BB#" << predecessor->getNumber() << " (" << predecessor->getName() << ")");
    DEBUG(dbgs() << " with" << (predData.predicate == SHAVECC::AL ? "out" : "") << " predication\n");
    MachineInstr *branchInstruction = predData.position == predecessor->end() ? nullptr : &*(predData.position);

    MachineInstr *clone = function.CloneMachineInstr(moveInfo.instruction);
    if (predData.predicate != SHAVECC::AL) {
      SmallVector<MachineOperand, 2> conditions;
      conditions.push_back(MachineOperand::CreateImm(predData.predicate));
      conditions.push_back(MachineOperand::CreateReg(predData.ccReg, false));
      SII->PredicateInstruction(*clone, conditions);
    }
    predecessor->insert(predData.position, clone);

    for (auto &operand : clone->operands()) {
      if (operand.isReg() && operand.isKill()) {
        // If this operand is marked as isKill but the register is live-in
        // to another successor block of this predecessor then remove the flag
        // as it is still live at this point
        for (const auto &successor : predecessor->successors())
          if (successor != &block && liveRanges.at(successor->getNumber()).isLiveIn(operand.getReg()))
            operand.setIsKill(false);

        // The jump instruction we are inserting before may use this register
        if (branchInstruction != nullptr && branchInstruction->readsRegister(operand.getReg(), SRI))
          operand.setIsKill(false);
      }
    }

    liveRanges.at(predecessor->getNumber()).addInstructionToLiveOuts(*clone);

    unsigned int unit = SII->GetFunctionalUnit(moveInfo.instruction->getOpcode());
    usedUnits[predecessorIdx][unit] += 1;

    // Only track the extra cycles once we have seen a predicated instruction
    BitVector effectedInstructions = liveRanges.at(predecessor->getNumber()).getUsers(*(moveInfo.instruction));
    if (predData.predicate != SHAVECC::AL)
      impactedByPredicated[predecessorIdx] |= effectedInstructions;

    if (impactedByPredicated[predecessorIdx].anyCommon(effectedInstructions)) {
      auto opcode = moveInfo.instruction->getOpcode();
      if (opcode != SHAVE::LSU_LDIH) // Don't count LDIL/LDIH pairs twice
        addedCost[predecessorIdx] += SII->GetSchedMaxLatency(moveInfo.instruction) + 1;
    }

    ++predecessorIt;
    ++predecessorIdx;
  }

  movedInstructions.push_back(moveInfo.instruction);

  // This instruction's defined registers are now live into this block from all predecessors
  for (const auto &def : moveInfo.instruction->defs())
    block.addLiveIn(def.getReg());

  // Finally, update the live ranges for this block so that other instructions which use the registers
  // defined by this one can also be moved if it is safe to do so
  liveRanges.at(block.getNumber()).addInstructionToLiveIns(*(moveInfo.instruction));
}

bool InterblockMovement::mustPredicate(MachineInstr &instruction) const {
  if (instruction.mayLoad() || instruction.mayStore())
    return true;

  // If a register defined by this instruction is live-in for a successor to one
  // of this block's predecessors then it must be predicated for the move to be safe
  for (const auto& predecessor : block.predecessors()) {
    if (predecessor->isReturnBlock())
      return true;

    for (const auto& def : instruction.defs()) {
      auto registers = SRI->getPhysicalSHAVERegisters(def.getReg());

      bool onlyLiveIntoThisBlock = true;
      for (const auto& successor : predecessor->successors())
        if (successor != &block && liveRanges.at(successor->getNumber()).isLiveIn(def.getReg()))
          onlyLiveIntoThisBlock = false;

      for (unsigned int reg : registers)
        if (liveRanges.at(predecessor->getNumber()).isLiveOut(reg) && !onlyLiveIntoThisBlock)
          return true;
    }
  }

  return false;
}

bool InterblockMovement::canMoveLoadStore(MachineInstr &instruction) {
  if (!instruction.mayLoadOrStore())
    return true;

  if (instruction.mayStore()) {
    MachineBasicBlock::instr_iterator it = instruction.getIterator();
    while (it != instruction.getParent()->instr_begin()) {
      --it;
      // Don't move a store past a potentially aliasing load or store
      if (it->mayLoadOrStore() && utilities.compareMemoryAccesses(instruction, *it) != AliasResult::NoAlias)
        return false;
    }
  }

  if (instruction.mayLoad()) {
    MachineBasicBlock::instr_iterator it = instruction.getIterator();
    while (it != instruction.getParent()->instr_begin()) {
      --it;
      // Don't move a load past a potentially aliasing store
      if (it->mayStore() && utilities.compareMemoryAccesses(instruction, *it) != AliasResult::NoAlias)
        return false;
    }
  }

  return true;
}

bool InterblockMovement::tooExpensive(InterblockMovement::MoveInfo &info) const {
  if (bypassCostModel)
    return false;

  unsigned int unit = SII->GetFunctionalUnit(info.instruction->getOpcode());
  unsigned int unitLimit = (unsigned int) SII->getDelaySlots();
  if (unit == SHAVE::LSU1 || unit == SHAVE::LSU0)
    unitLimit *= 2;

  int latency = (int) SII->GetSchedMaxLatency(info.instruction) + 1;

  if (unit != SHAVE::NONE) {
    auto predecessor = block.pred_begin();
    for (unsigned int i = 0; i < usedUnits.size(); ++i) {
      // Don't move any instructions that need predicating if this block
      // is not taken at least 50% of the time from predecessor
      if (info.forcePredication && conservativeLimits[i])
        return true;
      if (info.forcePredication || conservativeLimits[i]) {
        if (usedUnits[i][unit] >= unitLimit)
          return true;
        if (addedCost[i] + latency >= latency)
          return true;
      }
      // Check unpredicated instructions if we have already seen a predicated instruction
      // (i.e. addedCost[i] has been modified), but let an LDIH through to its LDIL partner
      BitVector effectedInstructions = liveRanges.at((*predecessor)->getNumber()).getUsers(*(info.instruction));
      if (effectedInstructions.anyCommon(impactedByPredicated[i]) && addedCost[i] + latency >= latency &&
          info.instruction->getOpcode() != SHAVE::LSU_LDIH)
        return true;
    }

    ++predecessor;
  }

  for (const auto &predecessor : block.predecessors()) {
    const auto &ranges = liveRanges.at(predecessor->getNumber());
    const auto users = ranges.getUsers(*(info.instruction));
    const double threshold = 0.4f;
    // If this register would cause >40% of instructions in the block to be moved upwards
    // then don't move it as it is likely to increase the number of cycles in the block
    if (users.count() > (users.size() * threshold))
      return true;
  }

  return false;
}

bool InterblockMovement::generatePositionsAndPredicates(InterblockMovement::MoveInfo &info) const {
  unsigned int predecessorPosition = 0;

  auto instructionMustBePredicated = [&](const MachineBasicBlock *predecessor) {
    return (info.forcePredication ||
           (insertPoints[predecessorPosition] != predecessor->end() &&
            liveRanges.at(predecessor->getNumber()).isLiveAt(*(info.instruction),
                                                             *(insertPoints[predecessorPosition]))));
  };

  for (const auto& predecessor : block.predecessors()) {
    SmallVector<MachineOperand, 4> conditions;
    MachineBasicBlock *TBB = nullptr, *FBB = nullptr;
    bool noInfo = SII->analyzeBranch(*predecessor, TBB, FBB, conditions, false);

    SHAVECC::CondCode predicate = SHAVECC::AL;
    unsigned int ccReg = SHAVE::NoRegister;
    if (!noInfo && !conditions.empty()) {
      unsigned int conditionIdx = conditions.size() - 2;
      if (TBB == &block) {
        // predecessor has a predicated branch to this block
        predicate = (SHAVECC::CondCode) conditions[conditionIdx].getImm();
        ccReg = conditions[conditionIdx + 1].getReg();
      }
      else if (TBB != nullptr) {
        // predecessor has a predicated branch to its other successor, and then branches/falls-through to this block
        auto condCode = (SHAVECC::CondCode) conditions[conditionIdx].getImm();
        if (instructionMustBePredicated(predecessor)) {
          if (!SHAVECC::canReverse(condCode))
            return false;
          predicate = SHAVECC::getOppositeCondition(condCode);
          ccReg = conditions[conditionIdx + 1].getReg();
        }
      }
    }

    if (instructionMustBePredicated(predecessor)) {
      if (noInfo) // The instruction needs to be predicated but we cannot find the predicate
        return false;
      if (predicate == SHAVECC::AL) // The instruction needs to be predicated but there is no predicate
        return false;
      info.predecessorData.push_back({ insertPoints[predecessorPosition], predicate, ccReg });
    }
    else {
      info.predecessorData.push_back({ insertPoints[predecessorPosition], SHAVECC::AL, SHAVE::NoRegister });
    }

    predecessorPosition += 1;
  }

  assert((int) info.predecessorData.size() == std::distance(block.pred_begin(), block.pred_end()));
  assert(!info.forcePredication || std::all_of(info.predecessorData.begin(),
                                               info.predecessorData.end(),
                                               [](const auto &data) { return data.predicate != SHAVECC::AL; }));

  return true;
}

InterblockMovement::MoveInfo InterblockMovement::getMoveInfo(MachineInstr& instruction) {
  MoveInfo info;
  info.canMove = false;
  info.instruction = &instruction;

  if (instruction.isBundled())
    return info;

  if (instruction.isInlineAsm())
    return info;

  if (instruction.isBarrier() || instruction.isBranch())
    return info;

  // We cannot have CC registers live across basic block boundaries
  if (instruction.isCompare())
    return info;

  // Check for the definition of any CMU CC registers (Overlap=true)
  if (instruction.findRegisterDefOperandIdx(SHAVE::C_CMU_0_15, false, true, SRI) != -1)
    return info;

  // If the instruction is predicated, it cannot be moved
  if (SII->isPredicated(instruction))
    return info;

  // Backend assumes that I0 is never live across block boundaries
  if (instruction.definesRegister(SHAVE::I0))
    return info;

  // Check if hoisting this instruction would move it past a potentially aliasing load/store
  if (!canMoveLoadStore(instruction))
    return info;

  // Make sure the register uses/defs allow the move to happen safely
  if (!liveRanges.at(block.getNumber()).isHoistable(instruction))
    return info;

  info.forcePredication = mustPredicate(instruction);

  if (info.forcePredication && !SII->isPredicable(instruction))
    return info;

  // Check the cost model if it is considered to be too expensive to move this instruction
  if (tooExpensive(info))
    return info;

  // Special case for jump pseudos which use a register internally. This won't get caught
  // by the live ranges
  for (unsigned int i = 0; i < block.pred_size(); ++i) {
    auto predecessor = *(block.pred_begin() + i);
    if (insertPoints[i] != predecessor->end()) {
      for (auto &operand : instruction.operands()) {
        if (!operand.isReg())
          continue;

        if (insertPoints[i]->definesRegister(operand.getReg(), SRI))
          return info;
      }
    }
  }

  if (generatePositionsAndPredicates(info))
    info.canMove = true;

  return info;
}

bool InterblockMovement::canMoveFromBlock() const {
  // returns true if this block is a candidate for inter-block instruction movement
  if (block.isEntryBlock())
    return false;

  const MachineLoop* loopInfo = MLI->getLoopFor(&block);

  auto getOutermostLoop = [](const MachineLoop * loop) {
    // FIXME: LoopInfo has a getOutermostLoop function in LLVM18
    while (loop->getParentLoop())
      loop = loop->getParentLoop();
    return loop;
  };

  if (loopInfo != nullptr && loopInfo->getLoopDepth() != 0) {
    for (const auto &predecessor : block.predecessors()) {
      const MachineLoop* predecessorInfo = MLI->getLoopFor(predecessor);
      if (predecessorInfo == nullptr)
        return false;

      // Blocks must be in the same loop structure
      if (getOutermostLoop(predecessorInfo) != getOutermostLoop(loopInfo))
        return false;

      // Blocks must be at the same depth, or the predecessors must be a lower depth
      // We should never move an instruction into a higher depth (i.e. a "more inner" loop)
      if (predecessorInfo->getLoopDepth() <= loopInfo->getLoopDepth())
        return false;
    }
  }
  else {
    // Should not move instructions from outside a loop into one
    for (const auto &predecessor : block.predecessors()) {
      const MachineLoop* predecessorInfo = MLI->getLoopFor(predecessor);
      if (predecessorInfo != nullptr && predecessorInfo->getLoopDepth() != 0)
        return false;
    }
  }

  const unsigned int predecessorLimit = 8u;
  if (block.pred_size() > predecessorLimit)
    return false;

  return true;
}

//
// Definition of public member functions of InterblockMovement
//

InterblockMovement::InterblockMovement(MachineBasicBlock &block,
                                       const SHAVEInstrInfo *SII,
                                       MachineLoopInfo *MLI,
                                       AliasAnalysis *AA,
                                       MachineBranchProbabilityInfo *BPI,
                                       BlockLiveRanges &liveRanges)
  : block(block),
    SII(SII),
    SRI(SII->getSHAVERegisterInfo()),
    MLI(MLI),
    utilities(AA, SII, true),
    liveRanges(liveRanges),
    bypassCostModel(true),
    addedCostBase(-SII->getDelaySlots()) {
  auto findBranch = [](MachineBasicBlock &block) {
    // We can't use block.getFirstTerminator() at this stage of the compilation because we have inserted
    // LDIL/LDIH label load pairs inbetween the two terminators
    for (auto it = block.begin(); it != block.end(); ++it)
      if (it->isTerminator())
        return it;
    return block.end();
  };

  for (const auto &predecessor : block.predecessors()) {
    insertPoints.push_back(findBranch(*predecessor));
    addedCost.push_back(addedCostBase); // Allow for delay slots worth of instructions to be inserted

    // Track the number of each functional unit that is used in the predecessor
    // This is used later on when deciding whether to move an instruction which needs
    // to be predicated in the predecessor block. We only want to move it if there is likely
    // to be a slot available in the branch delay slots, otherwise we risk pushing the comparison
    // for the branch up the schedule
    auto &unitCounts = usedUnits.emplace_back(UnitCounts(UnitCountsSize, 0));
    for (const auto &instruction : *predecessor) {
      unsigned int unit = SII->GetFunctionalUnit(instruction.getOpcode());
      assert(unit < UnitCountsSize);
      unitCounts[unit] += 1;
    }

    // If the probability this block will be executed from the predecessor is < 50% then use more conservative
    // limits for unpredicated instructions so we don't harm the more likely path
    auto edgeProbability = BPI->getEdgeProbability(predecessor, &block);
    double probability = ((double)edgeProbability.getNumerator() / (double)edgeProbability.getDenominator());
    conservativeLimits.push_back(probability < 0.5f);

    // Only bypass the cost model if the probability of this block executing for all its predecessors is >= 99%
    if (probability < 0.99f)
      bypassCostModel = false;

    impactedByPredicated.push_back(BitVector());
  }
}

bool InterblockMovement::run() {
  if (!canMoveFromBlock())
    return false;

  DEBUG(dbgs() << "SHAVEInterblockMovement: BB#" << block.getNumber() << " (" << block.getName() << ") is a candidate for interblock movement\n");
  DEBUG(if (bypassCostModel) dbgs() << "  Bypassing cost model for this block.\n");
  bool modified = false;

  // Traverse the instructions in the block top-down
  for (MachineInstr& instruction : block) {
    // Skip debug instructions (FIXME: Save to move with the next instruction)
    if (instruction.isDebugInstr() || instruction.isPosition())
      continue;

    // We cannot move an instruction past a call as there is no guarantee the registers
    // it uses/defines are not clobbered by the callee function
    if (instruction.isCall())
      break;

    auto moveInfo = getMoveInfo(instruction);
    if (!moveInfo.canMove)
      continue;

    DEBUG(dbgs() << "  Candidate instruction:\n");
    DEBUG(instruction.dump());

    // Here, it is safe to move this instruction to all predecessors
    moveInstruction(moveInfo);
    modified = true;
  }

  for (auto instruction : movedInstructions)
    instruction->eraseFromParent();

  return modified;
}
