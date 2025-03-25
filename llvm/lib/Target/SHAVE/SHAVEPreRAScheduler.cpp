//===-- SHAVEPreRAScheduler.cpp - Pre-RA Scheduler Pass ---------*- C++ -*-===//
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

#define DEBUG_TYPE "shave-prera-scheduler"

#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/CodeGen/LiveVariables.h"

#include "SHAVE.h"
#include "SHAVEPreRASchedPeephole.h"
#include "SHAVEPreRAScheduler.h"

using namespace llvm;

//
// Statistics for scheduling methods
//

STATISTIC(ListCount, "SHAVEPreRAScheduler - Number of blocks scheduling using the list method");
STATISTIC(DepthFirstCount, "SHAVEPreRAScheduler - Number of blocks scheduling using the depth-first method");
STATISTIC(BreadthFirstCount, "SHAVEPreRAScheduler - Number of blocks scheduling using the breadth-first method");

//
// Initialisation code required by the Pass Manager
//

char SHAVEPreRAScheduler::ID = 0;
char &llvm::SHAVEExperimentalPreRAScheduleID = SHAVEPreRAScheduler::ID;

INITIALIZE_PASS_BEGIN(SHAVEPreRAScheduler, "shavepreraschedulerpass", "SHAVE Pre-RA Scheduler Pass", false, false)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(SlotIndexes)
INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_DEPENDENCY(SHAVEPreRASchedPeephole)  // FIXME: Movidius - not sure this is necessary to ensure it runs before 'SHAVEPreRAScheduler'
INITIALIZE_PASS_END(SHAVEPreRAScheduler, "shavepreraschedulerpass", "SHAVE Pre-RA Scheduler Pass", false, false)

MachineFunctionPass *llvm::createSHAVEPreRASchedulerPass() {
  return new SHAVEPreRAScheduler();
}

//
// Definition of private member functions of SHAVEPreRALiveRegisterSets
//

const TargetRegisterClass* SHAVEPreRALiveRegisterSets::availableRegisterClasses[SHAVEPreRALiveRegisterSets::numAvailableRegisterClasses] =
{ &SHAVE::IRF32RegClass, &SHAVE::VRF128RegClass, &SHAVE::WVRF512RegClass };

SHAVEPreRALiveRegisterSets::SHAVERegisterCost SHAVEPreRALiveRegisterSets::getRegisterCost(unsigned int reg) const {
  const TargetRegisterClass* regClass = nullptr;

  if (Register::isPhysicalRegister(reg))
    regClass = SRI->getMinimalPhysRegClassLLT(reg);
  else if (Register::isVirtualRegister(reg))
    regClass = MRI->getRegClass(reg);

  if (regClass == nullptr)
    return SHAVERegisterCost(-1, 0);

  switch (regClass->getID()) {
  default:
    return SHAVERegisterCost(-1, 0);
  case SHAVE::IRF8_q0RegClassID:
  case SHAVE::IRF8_q1RegClassID:
  case SHAVE::IRF8_q2RegClassID:
  case SHAVE::IRF8_q3RegClassID:
  case SHAVE::IRF16_hRegClassID:
  case SHAVE::IRF16_lRegClassID:
  case SHAVE::IRF32RegClassID:
    return SHAVERegisterCost(SHAVE::IRF32RegClassID, 1);
  case SHAVE::IRF64RegClassID:
    return SHAVERegisterCost(SHAVE::IRF32RegClassID, 2);
  case SHAVE::VRF16_e0RegClassID:
  case SHAVE::VRF16_e1RegClassID:
  case SHAVE::VRF16_e2RegClassID:
  case SHAVE::VRF16_e3RegClassID:
  case SHAVE::VRF16_e4RegClassID:
  case SHAVE::VRF16_e5RegClassID:
  case SHAVE::VRF16_e6RegClassID:
  case SHAVE::VRF16_e7RegClassID:
  case SHAVE::VRF32_q0RegClassID:
  case SHAVE::VRF32_q1RegClassID:
  case SHAVE::VRF32_q2RegClassID:
  case SHAVE::VRF32_q3RegClassID:
  case SHAVE::VRF64_lRegClassID:
  case SHAVE::VRF64_hRegClassID:
  case SHAVE::VRF128RegClassID:
    return SHAVERegisterCost(SHAVE::VRF128RegClassID, 1);
  case SHAVE::WVRF16_0RegClassID:
  case SHAVE::WVRF32_0RegClassID:
  case SHAVE::WVRF64_0RegClassID:
  case SHAVE::WVRF128_0RegClassID:
  case SHAVE::WVRF256_0RegClassID:
  case SHAVE::WVRF256_1RegClassID:
  case SHAVE::WVRF512RegClassID:
    return SHAVERegisterCost(SHAVE::WVRF512RegClassID, 1);
  }
}

//
// Definition of public member functions of SHAVEPreRALiveRegisterSets
//

SHAVEPreRALiveRegisterSets::SHAVEPreRALiveRegisterSets(MachineBasicBlock * MBB, const SHAVEInstrInfo * SII, const SHAVERegisterInfo* SRI, LiveIntervals * LIS)
  : SII(SII), SRI(SRI), LIS(LIS) {
  MRI = &MBB->getParent()->getRegInfo();

  for (unsigned int i = 0; i < numAvailableRegisterClasses; ++i) {
    unsigned int id = availableRegisterClasses[i]->getID();
    numberOfAvailableRegisters[id] = id == SHAVE::IRF32RegClassID ? 28 : 32; // Exclude reserved regs for IRF32
    liveIns[id] = 0;
  }

  SlotIndexes* indexes = LIS->getSlotIndexes();

  std::vector<unsigned int> definedRegs;
  std::vector<unsigned int> usedRegs;
  std::unordered_map<unsigned int, std::set<MachineInstr *>> users;
  std::vector<unsigned int> killedRegs;

  // If a register is used without being defined before-hand in this block
  // then that register is live-in on entry to the block. We must account
  // for these live-in registers in the live register sets.
  for (MachineInstr &instr : *MBB) {
    if (instr.isDebugInstr() || instr.isPosition())
      continue;

    for (MachineOperand &operand : instr.operands()) {
      if (operand.isReg() && operand.isUse() && !operand.isUndef()) {
        unsigned reg = operand.getReg();
        users[reg].insert(&instr);

        if (std::find(definedRegs.begin(), definedRegs.end(), reg) == definedRegs.end() &&
            std::find(usedRegs.begin(), usedRegs.end(), reg) == usedRegs.end()) {
          SHAVERegisterCost cost = getRegisterCost(reg);

          // .first is the register class ID, .second is the cost associated with that register
          if (cost.first != -1 && cost.second != 0)
            liveIns[cost.first] += cost.second;

          usedRegs.push_back(reg);
        }

        if (operand.getReg().isVirtual()) {
          LiveInterval& interval = LIS->getInterval(operand.getReg());
          if (indexes->hasIndex(instr)) {
            SlotIndex index = indexes->getInstructionIndex(instr);
            index = indexes->getNextNonNullIndex(index);
            if (!interval.isLiveAtIndexes(ArrayRef<SlotIndex>(index)))
              killedRegs.push_back(reg);
          }
        }
      }
    }

    for (MachineOperand &operand : instr.operands()) {
      if (operand.isReg() && operand.isDef())
        definedRegs.push_back(operand.getReg());
    }
  }

  // Save for later
  for (const auto reg : killedRegs)
    killedRegisterUsers[reg] = users[reg];
}

void SHAVEPreRALiveRegisterSets::insertCycle(SHAVECycle cycle) {
  assert(cycle == (SHAVECycle) liveRegisterSets.size() && "SHAVEPreRAScheduler can only insert cycles at the end of the schedule");

  SHAVELiveRegisterCounter liveRegs;
  if (cycle == 0)
    liveRegs = liveIns;
  else
    liveRegs = liveRegisterSets.back();

  for (const auto &adjustment : newCycleAdjustment)
    liveRegs[adjustment.first] += adjustment.second;
  newCycleAdjustment.clear();

  liveRegisterSets.push_back(liveRegs);
}

void SHAVEPreRALiveRegisterSets::insertNode(SHAVEDependencyNodePtr node, SHAVECycle cycle) {
  const auto counters = getNodeCounters(node);

  bool useKills = false;
  for (MachineInstr * instr : node->instructions) {
    for (const auto &use : instr->uses()) {
      if (use.isReg()) {
        auto reg = use.getReg();
        auto it = killedRegisterUsers.find(reg);
        if (it != killedRegisterUsers.end()) {
          it->second.erase(instr);
          if (it->second.empty()) {
            useKills = true;
            killedRegisterUsers.erase(it);
          }
          lastUser[reg] = std::max(cycle, lastUser[reg]);
        }
        if (instr->definesRegister(reg))
          useKills = true;
      }
    }
  }

  for (const auto &counter : counters) {
    const auto registerClass = counter.first;
    const auto &registerClassCounter = counter.second;
    if (!registerClassCounter.defs.empty()) {
      for (const auto &def : registerClassCounter.defs) {
        for (unsigned int i = cycle; i < liveRegisterSets.size(); ++i) {
          liveRegisterSets[i][registerClass] += def.cost;
          if (def.isEarlyClobber) { // Only add early-clobber for one cycle
            if ((i + 1) == liveRegisterSets.size())
              newCycleAdjustment[registerClass] -= def.cost;
            break;
          }
        }
      }
    }

    if (useKills) {
      for (unsigned int i = cycle; i < liveRegisterSets.size(); ++i) {
        for (const auto &kill : registerClassCounter.kills) {
          if (lastUser[kill.reg] > cycle)
            continue;

          liveRegisterSets[i][registerClass] -= kill.cost;
          if (liveRegisterSets[i][registerClass] < 0)
            liveRegisterSets[i][registerClass] = 0;
        }
      }
    }
  }
}

SHAVEInstructionRegisterCounter SHAVEPreRALiveRegisterSets::getNodeCounters(SHAVEDependencyNodePtr node) const {
  SHAVEInstructionRegisterCounter instrCounter;
  std::set<Register> defRegs;
  for (MachineInstr* instr : node->instructions) {
    for (unsigned int i = 0; i < instr->getNumOperands(); ++i) {
      MachineOperand& operand = instr->getOperand(i);

      if (!operand.isReg())
        continue;

      const Register reg = operand.getReg();

      if (!Register::isVirtualRegister(reg))
        continue;

      SHAVERegisterCost cost = getRegisterCost(reg);

      // .first is the register class ID, .second is the cost associated with that register
      if (cost.first == -1 || cost.second == 0)
        continue;

      if (operand.isDef()) {
        defRegs.insert(reg);
        if (!operand.isDead() || operand.isEarlyClobber())
          instrCounter[cost.first].defs.push_back(SHAVERegisterDef(reg, (int) cost.second, operand.isEarlyClobber()));
      }
      else {
        if (defRegs.find(reg) != defRegs.end()) {
          // Register defined by this instruction does not increase pressure as it is an input as well
          if (!operand.isUndef()) // Unless it is an undef in which case it was never live to begin with
            instrCounter[cost.first].kills.push_back(SHAVERegisterKill(reg, (int) cost.second));
        }
        else {
          auto it = killedRegisterUsers.find(reg);
          // If this is the last user of a killed register to be inserted into the schedule
          if (it != killedRegisterUsers.end() && it->second.size() == 1)
            instrCounter[cost.first].kills.push_back(SHAVERegisterKill(reg, (int) cost.second));
        }
      }
    }
  }

  return instrCounter;
}

SHAVESpillStatus SHAVEPreRALiveRegisterSets::willSpill(SHAVEDependencyNodePtr node, SHAVECycle cycle) {
  auto instrCounter = getNodeCounters(node);

  if (liveRegisterSets.empty())
    return SHAVESpillStatus::NoSpill;

  for (const auto &registerClassCounters : instrCounter) {
    const auto registerClass = registerClassCounters.first;
    const auto &defs = registerClassCounters.second.defs;
    const auto &kills = registerClassCounters.second.kills;
    if (!defs.empty()) {
      // If checking for a potential new cycle, then use the last available
      if (cycle >= (SHAVECycle) liveRegisterSets.size())
        cycle = (SHAVECycle) liveRegisterSets.size() - 1;

      for (SHAVECycle checkCycle = cycle; checkCycle < (SHAVECycle) liveRegisterSets.size(); ++checkCycle) {
        auto totalCost = 0;
        for (const auto &def : defs)
          if (!def.isEarlyClobber || checkCycle == cycle)
            totalCost += def.cost;

        for (const auto &kill : kills) {
          auto killCycle = std::max(checkCycle, lastUser[kill.reg]);
          if (killCycle <= checkCycle)
            totalCost -= kill.cost;
        }

        if (totalCost <= 0)
          break;

        if (liveRegisterSets[checkCycle][registerClass] + totalCost > numberOfAvailableRegisters[registerClass]) {
          return registerClass == SHAVE::IRF32RegClassID ? SHAVESpillStatus::IRF : SHAVESpillStatus::VRF;
        }
      }
    }
  }

  return SHAVESpillStatus::NoSpill;
}

SHAVERegisterCosts SHAVEPreRALiveRegisterSets::getNodeCost(SHAVEDependencyNodePtr node) const {
  auto counters = getNodeCounters(node);

  auto sumCosts = [&](const auto &counter) {
    return std::accumulate(counter.begin(), counter.end(), 0,
                           [](int sum, const SHAVEPreRARegister& reg) { return sum + reg.cost; });
  };

  if (SII->hasFeature(SHAVE::HasVRF128_Feature))
    return { sumCosts(counters[SHAVE::IRF32RegClassID].defs) - sumCosts(counters[SHAVE::IRF32RegClassID].kills),
             sumCosts(counters[SHAVE::VRF128RegClassID].defs) - sumCosts(counters[SHAVE::VRF128RegClassID].kills) };
  else
    return { sumCosts(counters[SHAVE::IRF32RegClassID].defs) - sumCosts(counters[SHAVE::IRF32RegClassID].kills),
             sumCosts(counters[SHAVE::WVRF512RegClassID].defs) - sumCosts(counters[SHAVE::WVRF512RegClassID].kills) };
}

SHAVERegisterCosts SHAVEPreRALiveRegisterSets::getCycle(SHAVECycle cycle) const {
  auto counters = liveRegisterSets.at(cycle);

  if (SII->hasFeature(SHAVE::HasVRF128_Feature))
    return { counters[SHAVE::IRF32RegClassID], counters[SHAVE::VRF128RegClassID] };
  else
    return { counters[SHAVE::IRF32RegClassID], counters[SHAVE::WVRF512RegClassID] };
}

SHAVERegisterCosts SHAVEPreRALiveRegisterSets::getLimits() const {
  if (SII->hasFeature(SHAVE::HasVRF128_Feature))
    return { numberOfAvailableRegisters.at(SHAVE::IRF32RegClassID), numberOfAvailableRegisters.at(SHAVE::VRF128RegClassID) };
  else
    return { numberOfAvailableRegisters.at(SHAVE::IRF32RegClassID), numberOfAvailableRegisters.at(SHAVE::WVRF512RegClassID) };
}

bool SHAVEPreRALiveRegisterSets::atOrBeyondLimits(SHAVECycle cycle) const {
  const auto &counters = liveRegisterSets.at(cycle);
  for (const auto &counter : counters)
    if (counter.second >= numberOfAvailableRegisters.at(counter.first))
      return true;
  return false;
}

SHAVERegisterCosts SHAVEPreRALiveRegisterSets::getLiveIns() const {
  if (SII->hasFeature(SHAVE::HasVRF128_Feature))
    return { liveIns.at(SHAVE::IRF32RegClassID), liveIns.at(SHAVE::VRF128RegClassID) };
  else
    return { liveIns.at(SHAVE::IRF32RegClassID), liveIns.at(SHAVE::WVRF512RegClassID) };
}

#ifndef NDEBUG
void SHAVEPreRALiveRegisterSets::dump(raw_ostream &out, SHAVECycle cycle) {
  for (std::pair<const unsigned int, int> &counter : liveRegisterSets[cycle])
    out << counter.first << ":" << counter.second << ", ";
}
#endif // NDEBUG

//
// Definition of private member functions of SHAVEPreRASchedule
//

SHAVEConflictSet SHAVEPreRASchedule::getModifiedConflictSet(unsigned int opcode, SHAVEConflictSet conflicts1, SHAVEConflictSet conflicts2) {
  if (conflicts1[usesLSU1] && conflicts2[usesLSU1] && !conflicts1[usesLSU0] && !conflicts2[usesLSU0]) {
    conflicts1[usesLSU1] = false;
    conflicts1[usesLSU0] = true;
  }

  if (opcode == SHAVE::COPY)
    conflicts1[usesCMU] = true;
  else if (opcode == SHAVE::FLUSH_LSU1)
    conflicts1[usesLSU1] = true;
  else if (opcode == SHAVE::FLUSH_LSU0)
    conflicts1[usesLSU0] = true;
  else if (opcode <= SHAVE::ADJCALLSTACKUP)
    conflicts1[usesAllFUs] = true;

  return conflicts1 | conflicts2;
}

SHAVEConflictSet SHAVEPreRASchedule::getFunctionalUnits(MachineInstr* instr) {
  SHAVEConflictSet conflictSet;
  unsigned int opcode = instr->getOpcode();

  conflictSet.set(usesAllFUs, instr->isInlineAsm());
  conflictSet.set(usesBRU, check_usesBRU(opcode));
  conflictSet.set(usesCMU, check_usesCMU(opcode));
  conflictSet.set(usesIAU, check_usesIAU(opcode));
  conflictSet.set(usesPEU, check_usesPEU(opcode));
  conflictSet.set(usesSAU, check_usesSAU(opcode));
  conflictSet.set(usesVAU, check_usesVAU(opcode));
  conflictSet.set(usesLSU0, check_usesLSU0(opcode));
  conflictSet.set(usesLSU1, check_usesLSU1(opcode));

  return conflictSet;
}


void SHAVEPreRASchedule::reorderSchedule() {
  std::vector<SHAVEPreRACycle> reorderedSchedule(schedule.size(), SHAVEPreRACycle());

  for (unsigned int i = 0; i < schedule.size(); ++i) {
    for (SHAVEDependencyNodePtr node : schedule[i].nodes) {
      MachineInstr* instr = node->instructions[0];
      // All PHIs must be at the very beginning of the basic block
      if (instr->isPHI()) {
        reorderedSchedule[0].nodes.insert(reorderedSchedule[0].nodes.begin(), node);
      }
      else {
        reorderedSchedule[i].nodes.push_back(node);
      }
    }
  }

  schedule.clear();

  // A use and def of the same register can occur in the same cycle. In these cases, we must ensure the use is re-inserted into the
  // basic block before the def
  for (unsigned int i = 0; i < reorderedSchedule.size(); ++i) {
    SHAVEPreRACycle cycleNodes;
    SHAVEPreRACycle PHIs;

    for (SHAVEDependencyNodePtr currentNode : reorderedSchedule[i].nodes) {
      std::vector<SHAVEDependencyNodePtr>::iterator insertPoint = cycleNodes.nodes.begin();

      if (i == 0 && currentNode->instructions[0]->isPHI()) {
        PHIs.nodes.push_back(currentNode);
      }
      else {
        for (std::vector<SHAVEDependencyNodePtr>::iterator i = cycleNodes.nodes.begin(); i != cycleNodes.nodes.end(); i++) {
          SHAVEDependencyNodePtr completedNode = *i;

          if (std::find(completedNode->successors.begin(), completedNode->successors.end(), currentNode) != completedNode->successors.end()) {
            std::vector<SHAVEDependencyNodePtr>::iterator newInsertPoint = i;

            if (++newInsertPoint > insertPoint)
              insertPoint = newInsertPoint;
          }
        }
        cycleNodes.nodes.insert(insertPoint, currentNode);
      }
    }

    if (i == 0) {
      for (SHAVEDependencyNodePtr phi : PHIs.nodes)
        cycleNodes.nodes.insert(cycleNodes.nodes.begin(), phi);
    }

    // New cycle at the end of the schedule
    if (schedule.empty())
      schedule[0] = cycleNodes;
    else
      schedule[schedule.rbegin()->first + 1] = cycleNodes;
  }
}

bool SHAVEPreRASchedule::isBundled(SHAVECycle cycle) {
  if (cycle < (SHAVECycle) schedule.size())
    if (!schedule[cycle].nodes.empty() && schedule[cycle].nodes[0]->isBundled)
      return true;
  return false;
}

static bool hasConflict(const SHAVEConflictPredicate& pred, const SHAVEConflictSet& set) {
  SHAVEConflictSet conflict = pred.conflicts & set;

  switch (pred.condition) {
  case anySet:
    if (conflict.any())
      return true;
    break;
  case allSet:
    if (conflict == pred.conflicts)
      return true;
    break;
  case noneSet:
    if (conflict.none())
      return true;
    break;
  default:
    llvm_unreachable("SHAVESchedulerBase: Unrecognised conflict type");
  }

  return false;
}

SHAVEConflictTypeSet SHAVEPreRASchedule::findConflicts(SHAVEDependencyNodePtr node, SHAVECycle cycle) {
  SHAVEConflictTypeSet result;
  SHAVEConflictSet conflicts;
  SHAVEConflictSet nodeFunctionalUnits;

  // Get the conflict set for this node. The conflict set for a node is the conflict
  // sets for each individual instruction in the node combined
  for (unsigned int i = 0; i < node->instructions.size(); ++i) {
    unsigned int opcode = node->instructions[i]->getOpcode();
    conflicts = getModifiedConflictSet(opcode, getConflictSet(opcode), conflicts);
    nodeFunctionalUnits = getModifiedConflictSet(opcode, getFunctionalUnits(node->instructions[i]), nodeFunctionalUnits);
  }

  // Get the functional units used at this cycle in the schedule
  SHAVEConflictSet scheduleFunctionalUnits;
  if (cycle < (SHAVECycle) schedule.size()) {
    std::vector<SHAVEDependencyNodePtr> &scheduleNodes = schedule[cycle].nodes;
    for (unsigned int j = 0; j < scheduleNodes.size(); ++j)
      for (MachineInstr * instr : scheduleNodes[j]->instructions)
        scheduleFunctionalUnits = getModifiedConflictSet(instr->getOpcode(), getFunctionalUnits(instr), scheduleFunctionalUnits);
  }

  if (nodeFunctionalUnits[usesLSU1] && scheduleFunctionalUnits[usesLSU1] && !nodeFunctionalUnits[usesLSU0] && !scheduleFunctionalUnits[usesLSU0]) {
    nodeFunctionalUnits[usesLSU0] = true;
    nodeFunctionalUnits[usesLSU1] = false;
  }

  if ((nodeFunctionalUnits & scheduleFunctionalUnits).any())
    result.set(SHAVEConflictType::functionalUnit);

  if ((nodeFunctionalUnits.test(SHAVEConflict::usesPEU) && scheduleFunctionalUnits.any()) ||
      scheduleFunctionalUnits.test(SHAVEConflict::usesPEU))
    result.set(SHAVEConflictType::predication);

  if (result.any())
    return result;

  // Only targets pre-NPU4 use the conflict table, so we can exit early here for all others
  if (!targetUsesConflicts)
    return result;

  for (unsigned int i = 0; i < conflictTableSize && result.none(); ++i) {
    SHAVEConflictDescription description = conflictTable[i];

    // Check for conflicts with instructions after the insertion point
    if (hasConflict(description.pred1, conflicts)) {
      SHAVECycle checkCycle = cycle + description.minDelay;
      while (checkCycle < (int)schedule.size() && checkCycle < (cycle + (SHAVECycle) description.maxDelay + 1)) {
        SHAVEConflictSet cycleConflicts;

        // Get the conflict sets for instructions at this cycle in the schedule
        std::vector<SHAVEDependencyNodePtr> &scheduleNodes = schedule[checkCycle].nodes;
        for (unsigned int j = 0; j < scheduleNodes.size(); ++j)
          for (MachineInstr * instr : scheduleNodes[j]->instructions)
            cycleConflicts = getModifiedConflictSet(instr->getOpcode(), getConflictSet(instr->getOpcode()), cycleConflicts);

        // If a conflict has been detected then include it in the result conflict type set
        if (hasConflict(description.pred2, cycleConflicts)) {
          result |= description.types;
          return result;
        }

        checkCycle++;
      }
    }

    // Check for conflicts with instructions before the insertion point
    if (hasConflict(description.pred2, conflicts)) {
      int checkCycle = cycle - description.minDelay;
      while (checkCycle > 0 && checkCycle > (int)(cycle - description.maxDelay - 1)) {
        SHAVEConflictSet cycleConflicts;

        if (checkCycle < (int)schedule.size()) {
          // Get the conflict sets for instructions at this cycle in the actualSchedule
          std::vector<SHAVEDependencyNodePtr> &scheduleNodes = schedule[checkCycle].nodes;
          for (unsigned int j = 0; j < scheduleNodes.size(); ++j)
            for (MachineInstr * instr : scheduleNodes[j]->instructions)
              cycleConflicts = getModifiedConflictSet(instr->getOpcode(), getConflictSet(instr->getOpcode()), cycleConflicts);
        }

        // If a conflict has been detected then include it in the result conflict type set
        if (hasConflict(description.pred1, cycleConflicts)) {
          result |= description.types;
          return result;
        }

        checkCycle--;
      }
    }
  }

  return result;
}

static unsigned int getNodeLatency(SHAVEDependencyNodePtr node, const SHAVEInstrInfo * SII) {
  unsigned int latency = 0;

  for (MachineInstr * instr : node->instructions) {
    if (instr->isPseudo()) {
      if (instr->mayLoad())
        latency = std::max(latency, SII->getLoadLatency());
    }
    else {
      latency = std::max(latency, SII->GetSchedMaxLatency(instr));
    }
  }

  return latency;
}

void SHAVEPreRASchedule::scheduleNode(SHAVEDependencyNodePtr node, SHAVECycle cycle) {
  // Insert extra NOPs at the end of the schedule if the cycle to insert
  // at is after the end of the existing schedule
  if (cycle >= (SHAVECycle)schedule.size()) {
    SHAVECycle insertPoint = schedule.size() == 0 ? 0 : schedule.rbegin()->first + 1;
    do {
      schedule[insertPoint] = SHAVEPreRACycle();
      liveRegisterSets.insertCycle(insertPoint);
      insertPoint += 1;
    } while (cycle >= (SHAVECycle)schedule.size());
  }

  schedule[cycle].nodes.push_back(node);

  // Insert the node's instructions into the schedule
  liveRegisterSets.insertNode(node, cycle);

  SHAVECycle maxLatency = (SHAVECycle) getNodeLatency(node, SII);

  // If any of the instructions just inserted will write to a register after this
  // basic block has exited, then insert enough NOPs to ensure the writeback occurs
  // on the last cycle of the block
  if ((cycle + maxLatency) >= (SHAVECycle) schedule.size()) {
    SHAVECycle insertPoint = schedule.rbegin()->first + 1;
    do {
      schedule[insertPoint] = SHAVEPreRACycle();
      liveRegisterSets.insertCycle(insertPoint);
      insertPoint += 1;
    } while ((cycle + maxLatency) >= (SHAVECycle) schedule.size());
  }

  cycles[node->number] = { 0, cycle };
  completedNodes[node->number] = true;
}

//
// Definition of public member functions of SHAVEPreRASchedule
//

void SHAVEPreRASchedule::scheduleList(const std::vector<SHAVEDependencyNodePtr>& allNodes) {
  // This version of the schedule does not move any instructions, instead it is used to determine
  // the register costs of the original instruction order so that we do not produce a schedule
  // which might spill more than this
  isList = true;

  SHAVECycle cycle = 0;
  for (SHAVEDependencyNodePtr node : allNodes) {
    scheduleNode(node, cycle);
    cycle += node->latency + 1;
  }
}

SHAVESpillStatus SHAVEPreRASchedule::scheduleNode(SHAVEDependencyNodePtr node, SHAVESpillStatus allowSpills) {
  int cycle = 0;
  for (SHAVEDependencyNodePtr successor : node->successors) {
    assert(completedNodes.at(successor->number) && "All successors must be scheduled first");
    int latency = (int)getNodeLatency(successor, SII);
    cycle = std::max(cycle, cycles[successor->number].cycle + latency + 1);
  }

  SHAVECycle currentCycle = cycle;
  SHAVECycle startingCycle = currentCycle;

  DEBUG(dbgs() << "SHAVEPreRAScheduler: Scheduling node " << node->number << " with instructions:\n");
  DEBUG(for (MachineInstr* instr : node->instructions) { dbgs() << "  "; instr->dump(); });
  DEBUG(dbgs() << "SHAVEPreRAScheduler: Starting at cycle: " << cycle << "\n");

  while (!completedNodes[node->number] && currentCycle < (SHAVECycle)schedule.size()) {
    if (!isBundled(currentCycle)) {
      auto spill = liveRegisterSets.willSpill(node, currentCycle);
      if (spill == SHAVESpillStatus::NoSpill) {
        if (findConflicts(node, currentCycle).none()) {
          // Schedule the node at the current cycle
          scheduleNode(node, currentCycle);
          DEBUG(dbgs() << "SHAVEPreRAScheduler: Scheduled node at cycle " << currentCycle << "\n");
          DEBUG(dbgs() << "  Register Pressure: "; liveRegisterSets.dump(dbgs(), currentCycle); dbgs() << "\n");
        }
      }
    }
    ++currentCycle;
  }

  currentCycle = startingCycle;
  SHAVESpillStatus status = SHAVESpillStatus::NoSpill;

  while (!completedNodes[node->number]) {
    auto spillStatus = liveRegisterSets.willSpill(node, currentCycle);
    if (spillStatus != SHAVESpillStatus::NoSpill && allowSpills != spillStatus && allowSpills != SHAVESpillStatus::Both) {
      ++currentCycle;
      if (currentCycle > (SHAVECycle)schedule.size())
        return spillStatus;
      continue;
    }

    if (!isBundled(currentCycle) && findConflicts(node, currentCycle).none()) {
      // Schedule the node at the current cycle
      scheduleNode(node, currentCycle);
      status = spillStatus;
      DEBUG(dbgs() << "SHAVEPreRAScheduler: Scheduled node at cycle " << currentCycle << "\n");
      DEBUG(dbgs() << "  Register Pressure: "; liveRegisterSets.dump(dbgs(), currentCycle); dbgs() << "\n");
    }
    ++currentCycle;
  }

  return status;
}

SHAVERegisterCosts SHAVEPreRASchedule::getMaxCost() {
  SHAVERegisterCosts costs;
  for (SHAVECycle cycle = 0; cycle < (SHAVECycle) schedule.size(); ++ cycle) {
    const auto cycleCost = liveRegisterSets.getCycle(cycle);
    costs.IRFcost = std::max(costs.IRFcost, cycleCost.IRFcost);
    costs.VRFcost = std::max(costs.VRFcost, cycleCost.VRFcost);
  }
  return costs;
}

void SHAVEPreRASchedule::emitSchedule() {
  // Schedule is in list order so there is nothing to do here
  if (isList)
    return;

  reorderSchedule();

  MachineBasicBlock::instr_iterator insertPoint = currentBasicBlock->instr_begin();

  while (insertPoint != currentBasicBlock->instr_end() && (insertPoint->isDebugInstr() || insertPoint->isPosition() || insertPoint->isCFIInstruction()))
    insertPoint++;

  auto compareNodes = [&](SHAVEDependencyNodePtr node1, SHAVEDependencyNodePtr node2) {
    const auto cost1 = liveRegisterSets.getNodeCounters(node1);
    const auto cost2 = liveRegisterSets.getNodeCounters(node2);

    auto sumCosts = [](const auto &counter) {
      return std::accumulate(counter.begin(), counter.end(), 0,
                             [](int sum, const SHAVEPreRARegister &kill) { return sum + kill.cost; });
    };

    unsigned int defs1 = 0;
    unsigned int kills1 = 0;
    for (const auto &cost : cost1) {
      defs1 += sumCosts(cost.second.defs);
      kills1 += sumCosts(cost.second.kills);
    }

    unsigned int defs2 = 0;
    unsigned int kills2 = 0;
    for (const auto& cost : cost2) {
      defs2 += sumCosts(cost.second.defs);
      kills2 += sumCosts(cost.second.kills);
    }

    return kills1 > kills2 || (kills1 == kills2 && defs1 < defs2);
  };

  for (auto& cycle : schedule) {
    SHAVEPreRACycle& nodes = cycle.second;

    // Sort the nodes in a cycle by latency, lowest first
    // By sorting this way, if a lower latency instruction kills a register then the higher latency one
    // can re-use that physical register and the post-RA scheduler can put the lower latency instruction
    // into the delay slots of the higher one, despite the register re-use.
    if (!liveRegisterSets.atOrBeyondLimits(cycle.first))
      std::stable_sort(nodes.nodes.begin(), nodes.nodes.end(),
                       [](auto node1, auto node2) { return node1->latency < node2->latency; });
    // Unless the register pressure at this cycle is at or beyond the limits
    // then sort the nodes such that the node which kills the most registers is first
    // to relieve the register pressure as much as possible before any new defs
    else
      std::stable_sort(nodes.nodes.begin(), nodes.nodes.end(), compareNodes);
    for (const SHAVEDependencyNodePtr& node : nodes.nodes) {
      MachineInstr* bundleHead = node->instructions[0];

      if (insertPoint != bundleHead->getIterator()) {
        currentBasicBlock->splice(insertPoint, currentBasicBlock, bundleHead->getIterator());

        if (!bundleHead->isDebugInstr() && !bundleHead->isPosition() && !bundleHead->isCFIInstruction())
          LIS->handleMove(*bundleHead, true);
      }

      insertPoint = std::next(bundleHead->getIterator());

      for (unsigned int k = 1; k < node->instructions.size(); ++k) {
        MachineInstr* instr = node->instructions[k];

        if (insertPoint != instr->getIterator()) {
          currentBasicBlock->splice(insertPoint, currentBasicBlock, instr->getIterator());

          if (!instr->isDebugInstr() && !instr->isPosition() && !instr->isCFIInstruction())
            if (!node->isBundled)
              LIS->handleMove(*instr, true);
        }
        insertPoint = std::next(instr->getIterator());
      }

      // Re-insert any debug instructions that were attached to this node
      for (MachineInstr* debugInstr : node->debugInstructions) {
        if (insertPoint != debugInstr->getIterator())
          currentBasicBlock->splice(insertPoint, currentBasicBlock, debugInstr->getIterator());

        insertPoint = std::next(debugInstr->getIterator());
      }
    }
  }
}

#ifndef NDEBUG
void SHAVEPreRASchedule::dumpSchedule(raw_ostream& out) {
  for (SHAVECycle i = 0; i < (SHAVECycle)schedule.size(); ++i) {
    out << "Cycle " << i << ":\n";
    out << "Live registers: ";
    liveRegisterSets.dump(out, i);
    out << "\n";

    for (SHAVEDependencyNodePtr node : schedule[i].nodes)
      for (MachineInstr* instr : node->instructions)
        instr->dump();
  }
}
#endif // NDEBUG

//
// Definition of private member functions of SHAVEPreRAScheduler
//

void SHAVEPreRAScheduler::generateNodeCosts() {
  nodeCost = NodeCosts(allGeneratedNodes.size(), SHAVERegisterCosts());
  maxSubgraphCost = NodeCosts(allGeneratedNodes.size(), SHAVERegisterCosts());

  std::unordered_map<SHAVEDependencyNodePtr, std::vector<uint64_t> > includedMaxNodes;
  std::unordered_map<SHAVEDependencyNodePtr, unsigned int> predecessorsComplete;

  auto getSet = [&]() {
    return std::vector<uint64_t>(allGeneratedNodes.size() / 64 + 1, 0ull);
  };

  auto getNodeSet = [&](SHAVEDependencyNodePtr node) {
    auto it = includedMaxNodes.find(node);
    if (it != includedMaxNodes.end())
      return it;
    auto inserted = includedMaxNodes.insert(std::make_pair(node, getSet()));
    return inserted.first; // inserted.iterator->value
  };

  auto setBit = [](std::vector<uint64_t>& nodes, unsigned int number) {
    nodes[number / 64] |= 1ull << (number % 64ull);
  };

  auto getBit = [](std::vector<uint64_t>& nodes, unsigned int number) {
    return nodes[number / 64] & (1ull << (number % 64ull));
  };

  auto combine = [](std::vector<uint64_t>& first, std::vector<uint64_t>& second) {
    for (unsigned int i = 0; i < first.size(); ++i)
      first[i] |= second[i];
  };

  DEBUG(dbgs() << "\nNode costs:\n");
  for (SHAVEDependencyNodePtr checkNode : allGeneratedNodes) {
    auto cost = liveRegisterSets->getNodeCost(checkNode);
    nodeCost[checkNode->number] = cost;
    DEBUG(dbgs() << "  Node " << checkNode->number << ": " << cost.to_string() << "\n");

    auto& bitset = getNodeSet(checkNode)->second;

    auto successorNodes = checkNode->successors;
    std::stable_sort(successorNodes.begin(), successorNodes.end(), [](auto a, auto b) { return a->number > b->number; });
    for (SHAVEDependencyNodePtr successor : successorNodes) {
      if (!getBit(bitset, successor->number)) {
        setBit(bitset, successor->number);
        combine(bitset, getNodeSet(successor)->second);
      }
    }
  }

  for (SHAVEDependencyNodePtr checkNode : allGeneratedNodes) {
    auto maxCost = nodeCost.at(checkNode->number);
    // Registers live into this instruction must still be counted for subgraph cost,
    // even if they are killed by this instruction
    maxCost.IRFcost = std::max(maxCost.IRFcost, 0);
    maxCost.VRFcost = std::max(maxCost.VRFcost, 0);

    auto &bitset = getNodeSet(checkNode)->second;

    auto successorsBitset = getSet();
    for (auto successor : checkNode->successors)
      setBit(successorsBitset, successor->number);

    for (unsigned int i = 0; i < allGeneratedNodes.size(); ++i) {
      if (getBit(bitset, i)) {
        const auto &cost = nodeCost.at(i);
        if (!getBit(successorsBitset, i)) {
          maxCost += cost;
        }
        else {
          maxCost.IRFcost += std::max(cost.IRFcost, 0);
          maxCost.VRFcost += std::max(cost.VRFcost, 0);
        }
      }
    }

    maxSubgraphCost.at(checkNode->number) = maxCost;
  }

  DEBUG(dbgs() << "\nSubgraph costs:\n");
  DEBUG(for (unsigned int i = 0; i < allGeneratedNodes.size(); ++i) dbgs() << "  Node " << i << ": " << maxSubgraphCost[i].to_string() << "\n";);
}

std::vector<SHAVEDependencyNodePtr> SHAVEPreRAScheduler::generateScheduleOrder() {
  std::vector<SHAVEDependencyNodePtr> workList;
  for (SHAVEDependencyNodePtr graph : dependencyGraphs)
    workList.push_back(graph);

  std::vector<SHAVEDependencyNodePtr> scheduleOrder;
  scheduleOrder.reserve(allGeneratedNodes.size());
  std::vector<bool> addedToSchedule(allGeneratedNodes.size(), false);

  SHAVERegisterCosts limits = liveRegisterSets->getLimits();
  limits -= liveRegisterSets->getLiveIns();

  while (!workList.empty()) {
    SHAVEDependencyNodePtr currentNode = workList.back();
    workList.pop_back();

    // There may be duplicates in the workList, if this node has already been scheduled then
    // simply skip over it and move onto the next node
    if (addedToSchedule[currentNode->number])
      continue;

    bool canAdd = true;
    for (SHAVEDependencyNodePtr predecessor : currentNode->predecessors)
      canAdd &= addedToSchedule[predecessor->number];

    if (!canAdd) {
      workList.insert(workList.begin(), currentNode);
      continue;
    }

    scheduleOrder.insert(scheduleOrder.begin(), currentNode);
    addedToSchedule[currentNode->number] = true;

    std::vector<SHAVEDependencyNodePtr> breadthFirstNodes;
    SHAVERegisterCosts cost = nodeCost.at(currentNode->number);

    auto scheduleBreadthFirst = [&]() {
      while (!breadthFirstNodes.empty()) {
        SHAVEDependencyNodePtr nextNode = breadthFirstNodes.front();
        breadthFirstNodes.erase(breadthFirstNodes.begin());

        if (cost >= limits) {
          // If one node on its own will break the limits then use depth-first scheduling
          // to reduce the amount of spilling
          workList.insert(workList.begin(), nextNode);
        }
        else {
          if (addedToSchedule.at(nextNode->number))
            continue;

          bool canAddNext = true;
          for (SHAVEDependencyNodePtr predecessor : nextNode->predecessors)
            canAddNext &= addedToSchedule[predecessor->number];

          if (canAddNext) {
            scheduleOrder.insert(scheduleOrder.begin(), nextNode);
            addedToSchedule.at(nextNode->number) = true;

            // Nodes are added to breadthFirstNodes in the reverse order to their eventual scheduling order
            // So we put the lowest weight nodes onto the scheduleOrder first so they are scheduled last
            auto successors = nextNode->successors;
            std::stable_sort(successors.begin(), successors.end(),
                             [](auto node1, auto node2) { return node1->weight < node2->weight; });
            for (SHAVEDependencyNodePtr nextSuccessor : successors)
              breadthFirstNodes.push_back(nextSuccessor);
          }
          else {
            workList.insert(workList.begin(), currentNode);
          }
        }
      }
    };

    // When traversing the graph breadth-frist, we want to group as many different paths
    // together as we can. To aid this, we sort the successors here by register cost
    // lowest usage to highest. This allows the highest number of independent paths to
    // be overlapped first
    auto compareNodes = [&](SHAVEDependencyNodePtr node1, SHAVEDependencyNodePtr node2) {
      const auto& cost1 = maxSubgraphCost.at(node1->number);
      const auto& cost2 = maxSubgraphCost.at(node2->number);

      const auto combined1 = cost1.IRFcost + cost1.VRFcost;
      const auto combined2 = cost2.IRFcost + cost2.VRFcost;

      if (combined1 == combined2)
        return node1->weight > node2->weight;

      return combined1 < combined2;
    };

    auto successors = currentNode->successors;
    std::stable_sort(successors.begin(), successors.end(), compareNodes);

    for (SHAVEDependencyNodePtr successor : successors) {
      if (cost + maxSubgraphCost[successor->number] >= limits) {
        scheduleBreadthFirst();
        cost = nodeCost[currentNode->number];
      }

      cost += maxSubgraphCost[successor->number];
      breadthFirstNodes.push_back(successor);
    }

    scheduleBreadthFirst(); // Any remaining nodes
  }

  return scheduleOrder;
}

SHAVEDependencyNodePtr SHAVEPreRAScheduler::getSpillReliefNode(const SHAVEPreRASchedule &schedule,
                                                               SHAVEDependencyNodePtr node,
                                                               const std::vector<SHAVEDependencyNodePtr> &toBeScheduled,
                                                               SHAVESpillStatus spillStatus) const {
  auto allSuccessorsComplete = [&](const SHAVEDependencyNodePtr node) {
    for (SHAVEDependencyNodePtr successor : node->successors)
      if (!schedule.isComplete(successor))
        return false;
    return true;
  };

  auto getRegCost = [&](const SHAVERegisterCosts& nodeCost) {
    return spillStatus == SHAVESpillStatus::IRF ? nodeCost.IRFcost : nodeCost.VRFcost;
  };

  auto getOtherRegCost = [&](const SHAVERegisterCosts& nodeCost) {
    return spillStatus == SHAVESpillStatus::IRF ? nodeCost.VRFcost : nodeCost.IRFcost;
  };

  SHAVEDependencyNodePtr reliefNode = node;
  auto nodeCost = liveRegisterSets->getNodeCost(reliefNode);

  for (auto nextNode : toBeScheduled) {
    if (nodeCost.IRFcost <= 0 && nodeCost.VRFcost <= 0)
      break;

    if (!allSuccessorsComplete(nextNode))
      continue;

    const auto nextCost = liveRegisterSets->getNodeCost(nextNode);

    if (spillStatus == SHAVESpillStatus::Both) {
      // If this node only reduces the register pressure then schedule it
      if (nextCost.IRFcost <= 0 && nextCost.VRFcost <= 0) {
        reliefNode = nextNode;
        break;
      }

      // The lesser of two evils
      if ((nextCost.IRFcost < nodeCost.IRFcost && nextCost.VRFcost <= nodeCost.VRFcost) ||
          (nextCost.IRFcost <= nodeCost.IRFcost && nextCost.VRFcost < nodeCost.VRFcost)) {
        reliefNode = nextNode;
        nodeCost = nextCost;
      }
      // Decrease pressure on one at the expense of the other
      else if ((nextCost.IRFcost + nextCost.VRFcost) < (nodeCost.IRFcost + nodeCost.VRFcost)) {
        reliefNode = nextNode;
        nodeCost = nextCost;
      }
    }
    // If we can find one, prefer a node which reduces the pressure on the register class which will spill
    else if (getRegCost(nextCost) <= getRegCost(nodeCost)) {
      if (getRegCost(nextCost) < getRegCost(nodeCost) || getOtherRegCost(nextCost) < getOtherRegCost(nodeCost)) {
        reliefNode = nextNode;
        nodeCost = nextCost;
      }
      if (getRegCost(nodeCost) < 0 && getOtherRegCost(nodeCost) <= 0)
        break;
    }
  }

  return reliefNode;
}

void SHAVEPreRAScheduler::scheduleGraphs(SHAVEPreRASchedule &schedule) {
  // First track the number of registers that are needed for each subgraph so that we can
  // schedule breadth-first as much as possible, without spilling registers. If a register
  // spill is likely, then we switch to depth-first for that sub-graph
  generateNodeCosts();

  auto scheduleOrder = generateScheduleOrder();

  DEBUG(dbgs() << "  Schedule Order:\n");
  DEBUG(for (auto node : scheduleOrder) dbgs() << "    " << node->number << "\n");

  auto toBeScheduled = allGeneratedNodes;
  while (!toBeScheduled.empty()) {
    auto currentNode = scheduleOrder.front();

    auto spillStatus = schedule.scheduleNode(currentNode, /*allowSpills=*/ SHAVESpillStatus::NoSpill);
    if (spillStatus == SHAVESpillStatus::NoSpill) { // Node has been scheduled successfully
      scheduleOrder.erase(scheduleOrder.begin());
      toBeScheduled.erase(std::find(toBeScheduled.begin(), toBeScheduled.end(), currentNode));
    }
    else {
      // If a spill is unavoidable for this node, then revert to source order until pressure is relieved
      DEBUG(dbgs() << "  WARNING: Unavoidable spill on " << (spillStatus == SHAVESpillStatus::IRF ? "IRF" : "VRF") << ", reverting to source order\n");

      SHAVEDependencyNodePtr reliefNode = getSpillReliefNode(schedule, currentNode, toBeScheduled, spillStatus);

      // Try again but only allow a spill to happen if it is on the same register class as the original spill
      // We don't want to trade an IRF spill for a VRF spill (or vice-versa) without knowing about it
      auto secondAttemptStatus = schedule.scheduleNode(reliefNode, /*allowSpills=*/ spillStatus);

      if (secondAttemptStatus != SHAVESpillStatus::NoSpill && secondAttemptStatus != SHAVESpillStatus::Both && secondAttemptStatus != spillStatus) {
        DEBUG(dbgs() << "  WARNING: Unavoidable spill on both IRF and VRF, trying again\n");
        // Both register classes are spilling but with different nodes, try again to find a node
        // which causes the least amount of spilling across both classes
        reliefNode = getSpillReliefNode(schedule, reliefNode, toBeScheduled, SHAVESpillStatus::Both);
        schedule.scheduleNode(reliefNode);
      }

      scheduleOrder.erase(std::find(scheduleOrder.begin(), scheduleOrder.end(), reliefNode));
      toBeScheduled.erase(std::find(toBeScheduled.begin(), toBeScheduled.end(), reliefNode));
    }
  }
}

void SHAVEPreRAScheduler::scheduleGraphDepthFirst(SHAVEPreRASchedule &schedule, SHAVEDependencyNodePtr node) {
  std::vector<SHAVEDependencyNodePtr> workList(1, node);

  while (!workList.empty()) {
    SHAVEDependencyNodePtr currentNode = workList.back();

    // There may be duplicates in the workList, if this node has already been scheduled then
    // simply skip over it and move onto the next node
    if (schedule.isComplete(currentNode)) {
      workList.pop_back();
      continue;
    }

    // Sort the successor nodes of this node by weight (lowest to highest)
    std::stable_sort(currentNode->successors.begin(), currentNode->successors.end(),
                     [](auto node1, auto node2) { return node1->weight < node2->weight; });

    bool incompleteSuccessor = false;
    // Schedule each of this node's successors in the generated order
    for (SHAVEDependencyNodePtr successor : currentNode->successors) {
      if (!schedule.isComplete(successor)) {
        workList.push_back(successor);
        incompleteSuccessor = true;
      }
    }

    if (incompleteSuccessor)
      continue;

    schedule.scheduleNode(currentNode);

    // Finally remove the node from the workList
    workList.pop_back();
  }
}

#ifndef NDEBUG
//
// Definition of overridden protected member functions from SHAVESchedulerBase
//

void SHAVEPreRAScheduler::generateAndDumpInstrIDs(raw_ostream &out) {
  unsigned int id = 0;
  for (MachineInstr &MI : *currentBasicBlock) {
    out << id << ": "; MI.print(out);
    instrIDs[&MI] = id++;
  }
}
#endif // NDEBUG

//
// Definition of public member functions of SHAVEPreRAScheduler
//

void SHAVEPreRAScheduler::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  AU.addRequired<LiveIntervals>();
  AU.addPreserved<LiveIntervals>();
  AU.addRequired<SlotIndexes>();
  AU.addPreserved<SlotIndexes>();
  AU.addRequired<AAResultsWrapperPass>();
  MachineFunctionPass::getAnalysisUsage(AU);
}

enum {
  overIRF1 = 1,
  overIRF2,
  overVRF1,
  overVRF2,
  overNone
};

static constexpr int hashOvers(int index) {
  return index == overNone ? 0 : 1 << index;
}

template<typename ...Indexes>
static constexpr int hashOvers(int index, Indexes... indexes) {
  return hashOvers(index) | hashOvers(indexes...);
}

bool SHAVEPreRAScheduler::isBetterSchedule(const std::unique_ptr<SHAVEPreRASchedule> &schedule1,
                                           const std::unique_ptr<SHAVEPreRASchedule> &schedule2) const {
  // Returns true if schedule2 is a "better" choice than schedule1
  if (schedule1 == nullptr)
    return true;

  const auto cost1 = schedule1->getMaxCost();
  const auto cost2 = schedule2->getMaxCost();
  const auto limits = liveRegisterSets->getLimits();

  auto key = hashOvers(cost1.IRFcost > limits.IRFcost ? overIRF1 : overNone,
                       cost1.VRFcost > limits.VRFcost ? overVRF1 : overNone,
                       cost2.IRFcost > limits.IRFcost ? overIRF2 : overNone,
                       cost2.VRFcost > limits.VRFcost ? overVRF2 : overNone);

  const bool lowerCycleCount = schedule2->numCycles() < schedule1->numCycles();

  switch (key) {
  //
  // Best case, nothing is spilling
  //
  default:
    // pick whichever uses the most registers on both RFs, or if they are the same
    // then pick the schedule which has the lowest cycle count
    return (cost1.sum() < cost2.sum()) || (cost1.sum() == cost2.sum() && lowerCycleCount);
  //
  // Special cases
  //
  case hashOvers(overIRF1, overIRF2, overVRF1, overVRF2): // Everything is spilling
    // Choose whichever is spilling the least on both RFs
    return cost1.sum() > cost2.sum();
  case hashOvers(overIRF1, overIRF2                    ): // Both schedules spill IRF and neither spill VRF
    // schedule1 spills more, pick schedule2
    if (cost1.IRFcost > cost2.IRFcost)
      return true;
    // schedule2 spills more, pick schedule1
    if (cost1.IRFcost < cost2.IRFcost)
      return false;
    // both spill the same IRFs, pick whichever uses more VRFs
    return cost1.VRFcost < cost2.VRFcost;
  case hashOvers(overIRF1,                     overVRF2): // schedule1 spills IRFs only and schedule2 spills VRFs only
    // pick whichever spills the fewest of their RF
    return cost1.IRFcost > cost2.VRFcost;
  case hashOvers(          overIRF2, overVRF1          ): // schedule1 spills only VRF and schedule2 spills only IRF
    // pick whichever spills the fewest of their RF
    return cost1.VRFcost > cost2.IRFcost;
  case hashOvers(                    overVRF1, overVRF2): // Both schedules spill VRF and neither spills IRF
    // schedule1 spills more VRF, pick schedule2
    if (cost1.VRFcost > cost2.VRFcost)
      return true;
    // schedule2 spills more VRF, pick schedule1
    if (cost1.VRFcost < cost2.VRFcost)
      return false;
    // both spill the same VRFs, pick whichever uses more IRFs
    return cost1.IRFcost < cost2.IRFcost;
  //
  // Pick schedule1
  //
  case hashOvers(overIRF1, overIRF2,           overVRF2): // schedule2 spills both and schedule1 spills only VRF
  case hashOvers(          overIRF2, overVRF1, overVRF2): // schedule2 spills both and schedule1 spills only IRF
  case hashOvers(          overIRF2,           overVRF2): // schedule2 spills both and schedule1 spills neither
  case hashOvers(          overIRF2                    ): // schedule1 spills neither and schedule2 spills only IRF
  case hashOvers(                              overVRF2): // schedule2 spills VRF and schedule1 spills nothing
    return false;
  //
  // Pick schedule2
  //
  case hashOvers(overIRF1, overIRF2, overVRF1          ): // schedule1 spills both and schedule2 spills only VRF
  case hashOvers(overIRF1,           overVRF1, overVRF2): // schedule1 spills both and schedule2 spills only IRF
  case hashOvers(overIRF1,           overVRF1          ): // schedule1 spills both and schedule2 spills neither
  case hashOvers(overIRF1                              ): // schedule1 spills only IRF and schedule2 spills neither
  case hashOvers(                    overVRF1          ): // schedule1 spills VRF and schedule2 spills nothing
    return true;
  }

  assert(0 && "SHAVEPreRAScheduler could not pick between schedules");
  return false;
}

bool SHAVEPreRAScheduler::runOnMachineFunction(MachineFunction &MF) {
  DEBUG(dbgs() << "SHAVEPreRAScheduler: Starting scheduling for machine function " << MF.getName() << "\n");

  const SHAVETargetMachine &TM = static_cast<const SHAVETargetMachine&>(MF.getTarget());
  SII = TM.getSubtargetImpl()->getInstrInfo();
  SRI = SII->getSHAVERegisterInfo();

  bool targetUsesConflicts = !SII->hasFeature(SHAVE::HasSharedResources_Feature);

  initConflictTable();
  LiveIntervals *LIS = &getAnalysis<LiveIntervals>();
  AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();

  for (MachineBasicBlock &MBB : MF) {
    DEBUG(dbgs() << "SHAVEPreRAScheduler: Starting scheduling for machine basic block " << MBB.getName() << "\n");
    DEBUG(dbgs() << "SHAVEPreRAScheduler: Machine basic block before scheduling: \n");
    DEBUG(MBB.dump());
    DEBUG(dbgs() << "\n");

    currentBasicBlock = &MBB;

    liveRegisterSets = std::make_unique<SHAVEPreRALiveRegisterSets>(&MBB, SII, SII->getSHAVERegisterInfo(), LIS);

    DEBUG(dbgs() << "SHAVEPreRAScheduler: Generating dependency graphs\n");
    generateDependencyGraphs(MBB);
    DEBUG(dumpDependencyGraphs(dbgs(), true));

    std::unique_ptr<SHAVEPreRASchedule> bestSchedule = nullptr;
    SchedulingMethod chosenMethod = SchedulingMethod::First;

    DEBUG(dbgs() << "SHAVEPreRAScheduler: Starting instruction scheduling\n");

    for (int method = (int)SchedulingMethod::First; method < (int)SchedulingMethod::NumMethods; ++method) {
      switch ((SchedulingMethod)method) {
      case SchedulingMethod::List: {
        if (SHAVEOptions::EnableSchedulingMethod == SHAVEOptions::Scheduling::LegacyPreRA ||
            SHAVEOptions::EnableSchedulingMethod == SHAVEOptions::Scheduling::LegacyBoth)
          break;
        auto schedule = std::make_unique<SHAVEPreRASchedule>(targetUsesConflicts, currentBasicBlock, SII, LIS, *(liveRegisterSets.get()), allGeneratedNodes.size());
        DEBUG(dbgs() << "SHAVEPreRAScheduler: Starting list scheduling\n");
        schedule->scheduleList(allGeneratedNodes);
        DEBUG(dbgs() << "SHAVEPreRAScheduler: Completed list scheduling\n");
        DEBUG(dbgs() << "SHAVEPreRAScheduler:   Costs: " << schedule->getMaxCost().to_string() << "\n");
        DEBUG(schedule->dumpSchedule(dbgs()));
        if (isBetterSchedule(bestSchedule, schedule)) {
          bestSchedule.swap(schedule);
          chosenMethod = (SchedulingMethod) method;
        }
        break;
      }
      case SchedulingMethod::DepthFirst: {
        auto schedule = std::make_unique<SHAVEPreRASchedule>(targetUsesConflicts, currentBasicBlock, SII, LIS, *(liveRegisterSets.get()), allGeneratedNodes.size());
        DEBUG(dbgs() << "SHAVEPreRAScheduler: Starting depth-first scheduling\n");
        for (SHAVEDependencyNodePtr graph : dependencyGraphs)
          scheduleGraphDepthFirst(*(schedule.get()), graph);
        DEBUG(dbgs() << "SHAVEPreRAScheduler: Completed depth-first scheduling\n");
        DEBUG(dbgs() << "SHAVEPreRAScheduler:   Costs: " << schedule->getMaxCost().to_string() << "\n");
        DEBUG(schedule->dumpSchedule(dbgs()));
        if (isBetterSchedule(bestSchedule, schedule)) {
          bestSchedule.swap(schedule);
          chosenMethod = (SchedulingMethod)method;
        }
        break;
      }
      case SchedulingMethod::BreadthFirst: {
        if (SHAVEOptions::EnableSchedulingMethod == SHAVEOptions::Scheduling::LegacyPreRA ||
            SHAVEOptions::EnableSchedulingMethod == SHAVEOptions::Scheduling::LegacyBoth)
          break;
        DEBUG(dbgs() << "SHAVEPreRAScheduler: Starting breadth-first scheduling\n");
        auto schedule = std::make_unique<SHAVEPreRASchedule>(targetUsesConflicts, currentBasicBlock, SII, LIS, *(liveRegisterSets.get()), allGeneratedNodes.size());
        scheduleGraphs(*(schedule.get()));
        DEBUG(dbgs() << "SHAVEPreRAScheduler: Completed breadth-first scheduling\n");
        DEBUG(dbgs() << "SHAVEPreRAScheduler:   Costs: " << schedule->getMaxCost().to_string() << "\n");
        DEBUG(schedule->dumpSchedule(dbgs()));
        if (isBetterSchedule(bestSchedule, schedule)) {
          bestSchedule.swap(schedule);
          chosenMethod = (SchedulingMethod)method;
        }
        break;
      }
      default:
        llvm_unreachable("Unrecognised scheduling method");
      }
    }

    switch (chosenMethod) {
    case SchedulingMethod::List:
      ++ListCount;
      DEBUG(dbgs() << "SHAVEPreRAScheduler: Chose List Scheduling\n");
      break;
    case SchedulingMethod::DepthFirst:
      ++DepthFirstCount;
      DEBUG(dbgs() << "SHAVEPreRAScheduler: Chose Depth-First Scheduling\n");
      break;
    case SchedulingMethod::BreadthFirst:
      ++BreadthFirstCount;
      DEBUG(dbgs() << "SHAVEPreRAScheduler: Chose Breadth-First Scheduling\n");
      break;
    default:
      llvm_unreachable("Unrecognised scheduling method");
    }

    DEBUG(dbgs() << "SHAVEPreRAScheduler: Dependency graphs scheduled. Machine basic block schedule is now:\n");
    DEBUG(bestSchedule->dumpSchedule(dbgs()));
    DEBUG(dbgs() << "\n");

    bestSchedule->emitSchedule();

    cleanup();

    DEBUG(dbgs() << "SHAVEPreRAScheduler: Finished scheduling for machine basic block " << MBB.getName() << "\n");
    DEBUG(dbgs() << "SHAVEPreRAScheduler: Machine basic block after scheduling: \n");
    DEBUG(MBB.dump());
    DEBUG(dbgs() << "\n");
  }

  DEBUG(dbgs() << "SHAVEPreRAScheduler: Finished scheduling for machine function " << MF.getName() << "\n");

  return true;
}
