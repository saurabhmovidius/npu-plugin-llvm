//===-- SHAVEPostRASchedule.cpp - Post-RA Schedule --------------*- C++ -*-===//
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

#define DEBUG_TYPE "shave-postra-scheduler"

#include "SHAVEPostRASchedule.h"

#include <optional>

using namespace llvm;

//
// Definition of private member functions of SHAVEPostRASchedule for finding load low and load high instructions
//

SHAVEPostRASchedule::LoadType SHAVEPostRASchedule::isLoadLowInstruction(unsigned opcode) const {
  if (check_isLoadImmediateLow(opcode)) return loadImmediate;
  else if (check_isLD128Low(opcode)) return LD128;
  else if (check_isLD512Low(opcode)) return LD512;
  else return noLoad;
}

SHAVEPostRASchedule::LoadType SHAVEPostRASchedule::isLoadHighInstruction(unsigned opcode) const {
  if (check_isLoadImmediateHigh(opcode)) return loadImmediate;
  else if (check_isLD128High(opcode)) return LD128;
  else if (check_isLD512High(opcode)) return LD512;
  else return noLoad;
}

bool SHAVEPostRASchedule::isStoreLowInstruction(unsigned opcode) const {
  return check_isST128Low(opcode) | check_isST512Low(opcode);
}

bool SHAVEPostRASchedule::isStoreHighInstruction(unsigned opcode) const {
  return check_isST128High(opcode) | check_isST512High(opcode);
}


int SHAVEPostRASchedule::getLSUInstructionIndex(ConstNodePtr node) const {
  int lsuIndex = -1;

  if (node->instructions.size() == 2) {
    unsigned int opcode0 = node->instructions[0]->getOpcode();
    unsigned int opcode1 = node->instructions[1]->getOpcode();

    if (check_usesPEU(opcode0))
      lsuIndex = 1;
    else if (check_usesPEU(opcode1))
      lsuIndex = 0;
  }
  else if (node->instructions.size() == 1) {
    lsuIndex = 0;
  }

  return lsuIndex;
}

bool SHAVEPostRASchedule::isLowHighLoadSequence(ConstNodePtr node,
                                                MachineInstr* nodeLoadInstruction,
                                                ConstNodePtr& foundPredecessor) const {
  unsigned int lowOpcode = nodeLoadInstruction->getOpcode();
  LoadType lowType = isLoadLowInstruction(lowOpcode);

  if (lowType == noLoad)
    return false;

  SHAVESchedulePosition startingCycle = getStartingCycle(node);

  for (ConstNodePtr predecessor : node->predecessors) {
    int predecessorLsuIndex = getLSUInstructionIndex(predecessor);

    if (predecessorLsuIndex != -1) {
      unsigned int highOpcode = predecessor->instructions[predecessorLsuIndex]->getOpcode();
      LoadType highType = isLoadHighInstruction(highOpcode);
      const SHAVESchedulePosition& predecessorCycle = cycles.at(predecessor->number);

      // Ensure that the low and high instructions can be issued in parallel, by checking they are the same type
      // This is to prevent incorrect movement of a genuine def-use case. For example, if an LDIL loads the offset
      // operand for an LDXV_h, they cannot be issued in the same cycle. A dependency should only exist between
      // instructions of the same type if they can be issued together.
      if (highType == lowType &&
          startingCycle.branchPosition == predecessorCycle.branchPosition &&
          startingCycle.cycle <= predecessorCycle.cycle) {
        // Ensure both instructions are writing to the same register
        unsigned int predReg = predecessor->instructions[predecessorLsuIndex]->getOperand(0).getReg();
        unsigned int reg = nodeLoadInstruction->getOperand(0).getReg();
        if (predReg != reg)
          return false;

        // If this is a load immediate high (LDIH) and it is bundled with a predicate instruction
        // then we cannot safely run it in parallel with the corresponding LDIL instruction.
        // See bug #32693 for details
        if (predecessor->instructions.size() > 1 && check_isLoadImmediateHigh(highOpcode)) {
          // If this is an LDIL with the same predicate then it is okay to bundle them together
          if (node->instructions.size() > 1 && check_isLoadImmediateLow(lowOpcode)) {
            int predecessorPeuIndex = predecessorLsuIndex == 0 ? 1 : 0;
            unsigned int predecessorPeuOpcode = predecessor->instructions[predecessorPeuIndex]->getOpcode();

            int peuIndex = -1;
            for (unsigned int i = 0; i < node->instructions.size(); ++i)
              if (node->instructions[i] != nodeLoadInstruction && check_usesPEU(node->instructions[i]->getOpcode()))
                peuIndex = i;

            // Load is bundled with something other than a PEU instruction
            if (peuIndex == -1)
              return false;

            unsigned int peuOpcode = node->instructions[peuIndex]->getOpcode();

            if (peuOpcode == predecessorPeuOpcode && conflicts.predicateCanBeModified(peuOpcode)) {
              unsigned ldihCC = predecessor->instructions[predecessorPeuIndex]->getOperand(0).getImm();
              unsigned ldilCC = node->instructions[peuIndex]->getOperand(0).getImm();

              if (ldilCC == ldihCC) {
                foundPredecessor = predecessor;
                return true;
              }
            }
          }
        }
        else {
          foundPredecessor = predecessor;
          return true;
        }
      }
    }
  }

  return false;
}

bool SHAVEPostRASchedule::isLoadVectorElementPair(ConstNodePtr node,
                                                  MachineInstr* nodeLoadInstruction,
                                                  ConstNodePtr& foundPredecessor) const {
  for (ConstNodePtr predecessor : node->predecessors) {
    int predecessorLsuIndex = getLSUInstructionIndex(predecessor);
    if (predecessorLsuIndex == -1)
      return false;

    MachineInstr* predLoadInstruction = predecessor->instructions[predecessorLsuIndex];
    if (check_isLoadElement(predLoadInstruction->getOpcode())) {
      unsigned int predReg = predLoadInstruction->getOperand(0).getReg();
      unsigned int reg = nodeLoadInstruction->getOperand(0).getReg();
      if (predReg != reg)
        return false;

      MachineMemOperand* predMemOperand = nullptr;
      if (predLoadInstruction->hasOneMemOperand())
        predMemOperand = *predLoadInstruction->memoperands_begin();

      MachineMemOperand* nodeMemOperand = nullptr;
      if (nodeLoadInstruction->hasOneMemOperand())
        nodeMemOperand = *nodeLoadInstruction->memoperands_begin();

      if (predMemOperand == nullptr || nodeMemOperand == nullptr)
        return false;

      unsigned int predSize = predMemOperand->getSize();
      unsigned int nodeSize = nodeMemOperand->getSize();

      unsigned int predIdx = predLoadInstruction->getOperand(3).getImm();
      unsigned int idx = nodeLoadInstruction->getOperand(3).getImm();
      if (schedulerInfo.doMemoryRangesOverlap(predIdx * predSize, idx * nodeSize, predSize, nodeSize))
        return false;

      foundPredecessor = predecessor;
      return true;
    }
  }

  return false;
}

bool SHAVEPostRASchedule::isIRF64LoadPair(ConstNodePtr node,
                                          MachineInstr* nodeLoadInstruction,
                                          ConstNodePtr& foundPredecessor) const {
  auto getImplicitIRF64 = [](MachineInstr* instruction) {
    for (MachineOperand& operand : instruction->operands()) {
      if (operand.isReg() && operand.isImplicit() && operand.isDef() && SHAVE::IRF64RegClass.contains(operand.getReg())) {
        return (unsigned)operand.getReg();
      }
    }
    return 0u;
    };

  unsigned int implicitIRF64 = getImplicitIRF64(nodeLoadInstruction);
  if (implicitIRF64 == 0)
    return false;

  for (ConstNodePtr predecessor : node->predecessors) {
    int predecessorLsuIndex = getLSUInstructionIndex(predecessor);
    if (predecessorLsuIndex == -1)
      return false;

    MachineInstr* predLoadInstruction = predecessor->instructions[predecessorLsuIndex];
    if (check_isLoad(predLoadInstruction->getOpcode())) {
      unsigned int predReg = predLoadInstruction->getOperand(0).getReg();
      unsigned int reg = nodeLoadInstruction->getOperand(0).getReg();
      // If the instructions define the same register then skip
      if (SRI->regsOverlap(predReg, reg))
        continue;

      unsigned int predImplicitIRF64 = getImplicitIRF64(predLoadInstruction);
      if (predImplicitIRF64 != implicitIRF64)
        continue;

      foundPredecessor = predecessor;
      return true;
    }
  }

  return false;
}

bool SHAVEPostRASchedule::isElementCopyPair(ConstNodePtr node,
                                            ConstNodePtr& foundPredecessor) const {
  // Element copy instructions have the form
  //   $dst = CMU_CPVV_element_* $vdst, $src, $dstidx
  //   $dst = CMU_CPIV_* $vdst, $src, $dstidx
  auto getDestinationReg = [](const MachineInstr* instruction) {
    return instruction->getOperand(0).getReg();
  };

  auto getSourceReg = [](const MachineInstr* instruction) {
    return instruction->getOperand(2).getReg();
  };

  auto getDestinationIndex = [](const MachineInstr* instruction) {
    return instruction->getOperand(3).getImm();
  };

  auto isElementCopy = [&](ConstNodePtr node,
                           std::optional<unsigned int> dstReg = std::nullopt,
                           std::optional<unsigned int> srcReg = std::nullopt,
                           std::optional<int64_t> index = std::nullopt,
                           std::optional<int> size = std::nullopt) {
    // Do not accept any predicated instructions
    if (node->instructions.size() != 1)
      return false;

    const auto& instruction = node->instructions.front();
    if (!check_isElementCopy(instruction->getOpcode()))
      return false;

    if (dstReg.has_value() && dstReg != getDestinationReg(instruction))
      return false;

    // Do not accept this node if there is a legitimate read-after-write dependency
    // between this node and its predecessor node, i.e. this instruction writes a
    // register which is read by the instruction that depends on it (other than the
    // tied-def vdst register)
    if (srcReg.has_value() && srcReg == getDestinationReg(instruction))
      return false;

    // Make sure the indexes don't overlap
    if (index.has_value()) {
      // If index is set but size isn't then assume the indexes overlap
      if (!size.has_value())
        return false;

      int instructionSize = getValueFor_isElementCopy(instruction->getOpcode());
      if (schedulerInfo.doMemoryRangesOverlap(index.value() * size.value(),
                                              getDestinationIndex(instruction) * instructionSize,
                                              size.value(), instructionSize))
        return false;
    }

    return true;
  };

  if (isElementCopy(node)) {
    MachineInstr * instruction = node->instructions.front();
    unsigned int dstReg = getDestinationReg(instruction);
    unsigned int srcReg = getSourceReg(instruction);
    int64_t index = getDestinationIndex(instruction);
    int size = getValueFor_isElementCopy(instruction->getOpcode());
    for (auto& predecessor : node->predecessors) {
      if (isElementCopy(predecessor, dstReg, srcReg, index, size)) {
        foundPredecessor = predecessor;
        return true;
      }
    }
  }

  return false;
}

bool SHAVEPostRASchedule::isMemLowHighSequence(ConstNodePtr node,
                                               ConstNodePtr& foundPredecessor) const {
  if (isElementCopyPair(node, foundPredecessor))
    return true;

  int lsuIndex = getLSUInstructionIndex(node);
  if (lsuIndex == -1)
    return false;

  MachineInstr* nodeInstruction = node->instructions[lsuIndex];

  if (isLowHighLoadSequence(node, nodeInstruction, foundPredecessor)) {
    return true;
  }
  else if (isStoreLowInstruction(nodeInstruction->getOpcode())) {
    SHAVESchedulePosition startingCycle = getStartingCycle(node);
    for (ConstNodePtr predecessor : node->predecessors) {
      int predecessorLsuIndex = getLSUInstructionIndex(predecessor);
      if (predecessorLsuIndex != -1 && isStoreHighInstruction(predecessor->instructions[predecessorLsuIndex]->getOpcode()) && startingCycle == cycles.at(predecessor->number)) {
        foundPredecessor = predecessor;
        return true;
      }
    }
  }
  else if (check_isLoadElement(nodeInstruction->getOpcode())) {
    return isLoadVectorElementPair(node, nodeInstruction, foundPredecessor);
  }
  else if (check_isLoad(nodeInstruction->getOpcode())) {
    return isIRF64LoadPair(node, nodeInstruction, foundPredecessor);
  }

  return false;
}

//
// Definition of private member functions of SHAVEPostRASchedule for finding the start/end of the schedule
//

SHAVESchedulePosition SHAVEPostRASchedule::getStartOfBlock() {
  return SHAVESchedulePosition(0, schedules.first.empty() ? 1 : schedules.first.begin()->first);
}

SHAVESchedulePosition SHAVEPostRASchedule::getEndOfBlock() {
  const auto& schedule = schedules.second.empty() ? schedules.first : schedules.second;
  const int branchPosition = schedules.second.empty() ? 0 : 1;
  return schedule.empty() ? SHAVESchedulePosition(branchPosition, 1)
    : SHAVESchedulePosition(branchPosition, schedule.rbegin()->first + 1);
}

//
// Definition of private member functions of SHAVEPostRASchedule for scheduling a node at a given cycle
//

void SHAVEPostRASchedule::markAsComplete(const ConstNodePtr& node) {
  completedNodes[node->number] = true;
  startingCycles.erase(node->number); // Starting cycle is no longer needed
}

void SHAVEPostRASchedule::addPorts(const std::set<uint64_t>& ports,
                                   const SHAVESchedulePosition& cycle) {
  auto& schedulePorts = getCycle(cycle).ports;
  for (uint64_t port : ports)
    schedulePorts.push_back(port);
}

void SHAVEPostRASchedule::removePorts(const std::set<uint64_t>& ports,
                                      const SHAVESchedulePosition& cycle) {
  auto& schedulePorts = getCycle(cycle).ports;
  for (uint64_t port : ports)
    schedulePorts.erase(std::find(schedulePorts.begin(), schedulePorts.end(), port));
}

void SHAVEPostRASchedule::scheduleNodeAtCycle(ConstNodePtr node,
                                              const SHAVESchedulePosition& cycle,
                                              const std::vector<SHAVEModification>& modificationActions) {
  DEBUG(dbgs() << "SHAVEPostRAScheduler: At cycle " << cycle.branchPosition << ", " << cycle.cycle << "\n");

  auto& cycleInfo = getCycle(cycle); // get implicitly intialises if it does not exist already

  // Apply the modification actions first
  SHAVEInstructionBundle instructions = node->instructions;
  for (const SHAVEModification& modification : modificationActions) {
    if (modification.applyToSchedule) {
      removePorts(modification.ignorePorts, cycle);
      addPorts(modification.additionalPorts, cycle);

      // Reset the conflict set and update with the new instructions, but not the removed
      cycleInfo.conflicts.reset();
      for (auto& node : cycleInfo.nodes) {
        for (MachineInstr* instruction : node->instructions)
          if (std::find(modification.removeInstructions.begin(), modification.removeInstructions.end(), instruction) == modification.removeInstructions.end())
            cycleInfo.conflicts |= conflicts.getModifiedConflictSet(instruction);
      }
      for (MachineInstr* instruction : modification.insertInstructions) {
        cycleInfo.conflicts |= conflicts.getModifiedConflictSet(instruction);
        cycleInfo.instructions.push_back(instruction);
      }

      // Remove instructions from the list as per the modification
      for (MachineInstr* instruction : modification.removeInstructions)
        cycleInfo.instructions.erase(std::find(cycleInfo.instructions.begin(), cycleInfo.instructions.end(), instruction));
    }
    else {
      for (MachineInstr* instruction : modification.removeInstructions)
        instructions.erase(std::find(instructions.begin(), instructions.end(), instruction));
      for (MachineInstr* instruction : modification.insertInstructions)
        instructions.push_back(instruction);
    }
  }

  bool shouldInsert = true;
  for (MachineInstr* instruction : instructions)
    if (instruction->getOpcode() <= SHAVE::ADJCALLSTACKUP && instruction->getOpcode() != SHAVE::INLINEASM)
      shouldInsert = false;

  // Insert the node into the specified cycle
  if (shouldInsert) {
    cycleInfo.nodes.push_back(node);
    cycleInfo.instructions.insert(cycleInfo.instructions.end(), instructions.begin(), instructions.end());
  }

  // Update the node metadata
  cycles[node->number] = cycle;
  markAsComplete(node);

  // Update the schedule to include any ports that are used by this/these instruction(s)
  setPortUsage(instructions, cycle);

  // Update the conflict set for this cycle
  for (MachineInstr* instruction : instructions) {
    cycleInfo.conflicts |= conflicts.getModifiedConflictSet(instruction);
    if (instruction->isInlineAsm())
      cycleInfo.conflicts.set(SHAVEConflict::usesAllFUs);
  }
}

void SHAVEPostRASchedule::setPortUsage(const SHAVEInstructionBundle& instructions,
                                       const SHAVESchedulePosition& cycle) {
  for (MachineInstr* instr : instructions) {
    if (instr->getOpcode() <= SHAVE::ADJCALLSTACKUP)
      continue;

    TargetSchedModel::ProcResIter begin, end;
    SII->GetSchedPortRange(instr, begin, end);

    for (TargetSchedModel::ProcResIter p = begin; p != end; ++p) {
      uint64_t port = p->ProcResourceIdx;
      int minDelay = p->AcquireAtCycle;
      int maxDelay = p->ReleaseAtCycle - 1;

      int branchDelay = SII->getDelaySlots();

      for (int delay = minDelay; delay <= maxDelay; ++delay) {
        SHAVESchedulePosition portCycle = cycle;
        portCycle.cycle += delay;

        // If the instruction has been moved across a branch position (and predicated on the opposite
        // condition code as the conditional branch) then writebacks may occur in the delay slots of the
        // unconditional branch.
        // Since the instruction is predicated on the opposite condition of the first branch, we only need
        // to consider the second branch position here, and not the desination block of the first branch.
        if (portCycle.cycle > branchDelay) {
          portCycle.cycle -= branchDelay + 1; // + 1 to account for the second branch itself
          portCycle.branchPosition += 1;
        }

        assert(portCycle.branchPosition < 2);
        assert(portCycle.cycle <= branchDelay);

        if (hasCycle(portCycle))
          getCycle(portCycle).ports.push_back(port);
      }
    }
  }
}

//
// Definition of private member functions of SHAVEPostRASchedule for working with node starting cycle metadata
//

SHAVESchedulePosition SHAVEPostRASchedule::calculateStartingCycle(ConstNodePtr node) {
  // Find the maximum latency for this instruction bundle
  unsigned int latency = getNodeLatency(node);

  SHAVESchedulePosition startingCycle;
  SHAVESchedulePosition& cachedStartingCycle = startingCycles[node->number];

  if (node->isCall) {
    SHAVESchedulePosition startOfBlock = getStartOfBlock();
    if (!cachedStartingCycle.isInitialised())
      startingCycle = startOfBlock;
    else if (cachedStartingCycle.branchPosition != 0 || cachedStartingCycle.cycle > 0) // Don't put a call into branch delay slots
      startingCycle = startOfBlock;
    else
      startingCycle = cachedStartingCycle;
  }
  else if (!cachedStartingCycle.isInitialised()) {
    // If this node has no starting cycle specified then we just start at the end of the block
    startingCycle = getEndOfBlock();

    // When inserting into an empty basic block, set the starting cycle to one after cycle 0,0
    if (startingCycle.cycle == 0 && startingCycle.branchPosition == 0 && !hasCycle(startingCycle))
      startingCycle.cycle = 1;
  }
  else {
    startingCycle = cachedStartingCycle;
  }

  SHAVESchedulePosition cycle = startingCycle;

  if (node->isPrefetch) {
    // Move prefetch instructions as far up the schedule as we can
    cycle = getStartOfBlock();
  }
  else {
    unsigned int offsetLatency = latency + 1;
    ConstNodePtr lowHighPredecessor = nullptr;
    if (isMemLowHighSequence(node, lowHighPredecessor)) {
      offsetLatency = 0;
      // We need to account for any anti-dependent predecessors whose cycle is less than the cycle of the low/high counterpart
      // to this high/low instruction. We only want to overlap the high and low halves as much as the predecessor allows.
      for (ConstNodePtr predecessor : node->predecessors) {
        if (predecessor != lowHighPredecessor) {
          const SHAVESchedulePosition& predecessorCycle = cycles.at(predecessor->number);
          if (predecessorCycle.branchPosition == cycle.branchPosition) {
            if (std::find(predecessor->outputSuccessors.begin(), predecessor->outputSuccessors.end(), node) == predecessor->outputSuccessors.end()) {
              int cycleOffset = cycle.cycle - predecessorCycle.cycle;
              int predLatency = getNodeLatency(predecessor);
              if (cycleOffset > predLatency)
                offsetLatency = std::max(offsetLatency, (unsigned int)(cycleOffset - predLatency));
            }
            else {
              int cycleOffset = predecessorCycle.cycle - cycle.cycle;
              if (cycleOffset <= (int)latency + 1)
                offsetLatency = std::max(offsetLatency, latency + 1 - cycleOffset);
            }
          }
        }
      }
    }

    cachedStartingCycle = SHAVESchedulePosition(); // Invalidate the starting cycle when adjusting for latency

    for (unsigned int i = 0; i < offsetLatency; ++i)
      cycle = getNextCycleForNode(node, cycle, latency);

    // Ensure that a node never overlaps with the execution of a branch
    cycle.cycle = std::min(((int)SII->getDelaySlots()) - (int)latency, cycle.cycle);

    // Create the extra cycles if they don't exist
    auto startOfBlock = getStartOfBlock();
    while (cycle.cycle < startOfBlock.cycle) {
      --startOfBlock.cycle;
      getCycle(startOfBlock); // get implicitly initialises
    }
  }

  cachedStartingCycle = cycle;
  return cycle;
}

void SHAVEPostRASchedule::updateSuccessorStartingCycles(ConstNodePtr node,
                                                        const SHAVESchedulePosition& cycle,
                                                        unsigned int latency) {
  for (ConstNodePtr successor : node->successors) {
    if (std::find(schedulerInfo.branchNodes.begin(), schedulerInfo.branchNodes.end(), successor) != schedulerInfo.branchNodes.end()) {
      updateSuccessorStartingCycles(successor, cycle, latency);
      continue;
    }

    SHAVESchedulePosition& successorStartingCycle = startingCycles[successor->number];
    // If the starting cycle has not yet been set, then set it
    if (!successorStartingCycle.isInitialised()) {
      if (std::find(node->outputSuccessors.begin(), node->outputSuccessors.end(), successor) == node->outputSuccessors.end()
          && !SHAVEOptions::EnableInterruptFriendlyScheduling)
        successorStartingCycle = getOffsetCycleForNode(successor, node, cycle, latency);
      else
        successorStartingCycle = cycle;
      continue;
    }

    bool isLabelLoad = false;
    for (MachineInstr* instruction : node->instructions)
      if (instruction->getOpcode() == SHAVE::LSU_LDIL_Label || instruction->getOpcode() == SHAVE::LSU_LDIH_Label)
        isLabelLoad = true;

    // If this successor does not rely on the output value of the current node (e.g. if this is an anti-dependency) then the successor's lifetime
    // may overlap with the current node's
    if (std::find(node->outputSuccessors.begin(), node->outputSuccessors.end(), successor) == node->outputSuccessors.end()
        && !SHAVEOptions::EnableInterruptFriendlyScheduling) {
      SHAVESchedulePosition newCycle = getOffsetCycleForNode(successor, node, cycle, latency);

      if (newCycle.branchPosition < successorStartingCycle.branchPosition) {
        // FIXME: Movidius - We need to check for reg dependencies with branchNodes at all intermediate branchPositions
        //                   At present they all use I0, so this suffices
        if (!successor->definesI0 &&
            std::find(schedulerInfo.branchNodes.begin(), schedulerInfo.branchNodes.end(), node) == schedulerInfo.branchNodes.end() &&
            !isLabelLoad)
          successorStartingCycle = newCycle;
      }
      else if (newCycle.branchPosition == successorStartingCycle.branchPosition && newCycle.cycle <= successorStartingCycle.cycle)
        successorStartingCycle = newCycle;

      continue;
    }

    if (cycle.branchPosition < successorStartingCycle.branchPosition) {
      // FIXME: Movidius - We need to check for reg dependencies with branchNodes at all intermediate branchPositions
      //                   At present they all use I0, so this suffices
      if (!successor->definesI0 &&
          std::find(schedulerInfo.branchNodes.begin(), schedulerInfo.branchNodes.end(), node) == schedulerInfo.branchNodes.end() &&
          !isLabelLoad)
        successorStartingCycle = cycle;
    }
    else if (cycle.branchPosition == successorStartingCycle.branchPosition && cycle.cycle < successorStartingCycle.cycle)
      successorStartingCycle = cycle;
  }
}

//
// Definition of private member functions of SHAVEPostRASchedule for calculating node cycles
//

unsigned int SHAVEPostRASchedule::getNodeLatency(ConstNodePtr node) {
  unsigned int latency = 0;
  for (MachineInstr* instr : node->instructions) {
    unsigned int instrLatency = SII->GetSchedMaxLatency(instr);
    if (instrLatency > latency)
      latency = instrLatency;
  }
  return latency;
}

SHAVESchedulePosition SHAVEPostRASchedule::getOffsetCycleForNode(ConstNodePtr node,
                                                                 ConstNodePtr predecessor,
                                                                 const SHAVESchedulePosition& cycle,
                                                                 unsigned int latency) {
  // successors should finish execution before performing an inline asm or CPIT/CPTI instruction
  for (MachineInstr* instr : predecessor->instructions) {
    unsigned int opcode = instr->getOpcode();
    if (instr->isInlineAsm() || opcode == SHAVE::CMU_CPTI || opcode == SHAVE::CMU_CPIT)
      return cycle;
  }

  for (MachineInstr* instr : node->instructions)
    if (instr->isInlineAsm())
      return cycle;

  SHAVESchedulePosition newCycle = cycle;
  newCycle.cycle += latency + 1;

  // The CMU.LUTR instructions read the VRF input reg after 1 cycle
  // Ideally we would like to base this on read latencies specified in the itinerary rather than
  // for one instruction, but this will require change in the way SHAVE latencies are implemented
  for (MachineInstr* instruction : node->instructions)
    if (SHAVEConflicts::check_isCMU_LUTR(instruction->getOpcode()))
      newCycle.cycle -= 1;

  bool isInCallDelaySlots = false;
  SHAVESchedulePosition checkCycle = newCycle;
  for (unsigned int i = 0; i <= (SII->getDelaySlots() + latency + 1) && !isInCallDelaySlots; ++i) {
    if (hasCycle(checkCycle)) {
      for (ConstNodePtr checkNode : getCycle(checkCycle).nodes) {
        isInCallDelaySlots |= checkNode->isCall;
      }
    }

    checkCycle.cycle -= 1;
  }

  if (!node->isCall && !predecessor->isCall && !isInCallDelaySlots)
    newCycle.cycle += getNodeLatency(node);

  while (!hasCycle(newCycle) && newCycle != cycle)
    newCycle.cycle -= 1;

  return newCycle;
}

SHAVESchedulePosition SHAVEPostRASchedule::getNextCycleForNode(ConstNodePtr currentNode,
                                                               const SHAVESchedulePosition& cycle,
                                                               unsigned int latency) {
  SHAVESchedulePosition nextCycle = cycle;
  nextCycle.cycle -= 1;

  // If the next cycle exists, then return it
  if (hasCycle(nextCycle))
    return nextCycle;

  // Otherwise move to the last cycle of the next branch position
  if (nextCycle.branchPosition == 1) {
    nextCycle.branchPosition--;
    nextCycle.cycle = SII->getDelaySlots();

    // Since the new branchPosition target might use the result of this node, we must pull the cycle back
    // to accommodate the node's latency
    nextCycle.cycle -= latency;

    // If this cycle exists, then return it
    if (hasCycle(nextCycle))
      return nextCycle;
  }

  // Otherwise we need to insert a new cycle into the block
  SHAVESchedulePosition newCycle = getStartOfBlock();

  newCycle.cycle -= 1;
  getCycle(newCycle); // get implicitly initialises

  return newCycle;
}

//
// Definition of public member functions of SHAVEPostRASchedule used by SHAVEPostRAScheduler
//

SHAVEPostRASchedule::SHAVEPostRASchedule(SHAVESchedulerBase& schedulerInfo,
                                         const SHAVEInstrInfo* SII,
                                         MachineBasicBlock* block)
  : block(block),
    function(block->getParent()),
    SII(SII),
    SRI(SII->getSHAVERegisterInfo()),
    schedulerInfo(schedulerInfo),
    conflicts(*this, schedulerInfo, SII, function) {
  for (MachineInstr& instr : block->instrs()) {
    if ((instr.getOpcode() == SHAVE::BRU_JMP || instr.getOpcode() == SHAVE::BRU_JMPcc) && instr.getOperand(0).getReg() == SHAVE::I0) {
      blockUsesI0Jump = true;
      break;
    }
  }
}

void SHAVEPostRASchedule::initialiseSchedule() {
  assert(schedulerInfo.branchNodes.size() <= 2u && "Expected maximum of two branches per block");
  schedules.first.clear();
  schedules.second.clear();
  completedNodes = std::vector<bool>(schedulerInfo.allGeneratedNodes.size(), false);

  bool first = true;

  for (ConstNodePtr branchNode : schedulerInfo.branchNodes) {
    if (!isComplete(branchNode)) {
      // Insert the branch instruction into the schedule
      SHAVESchedulePosition cycle = { first ? 0 : 1, 0 };
      SHAVEPostRACycle& cycleNodes = getCycle(cycle);
      cycleNodes = SHAVEPostRACycle(branchNode);

      for (MachineInstr* instr : branchNode->instructions) {
        cycleNodes.conflicts |= conflicts.getModifiedConflictSet(instr);
        if (instr->isInlineAsm())
          cycleNodes.conflicts.set(SHAVEConflict::usesAllFUs);
      }

      cycles[branchNode->number] = cycle;
      markAsComplete(branchNode);

      SHAVESchedulePosition successorCycle = cycle;
      successorCycle.cycle += SII->getDelaySlots() + 1;

      // Set the starting cycle for each of this node's successors
      for (ConstNodePtr successor : branchNode->successors) {
        SHAVESchedulePosition newCycle = cycle;
        if (std::find(branchNode->outputSuccessors.begin(), branchNode->outputSuccessors.end(), successor) == branchNode->outputSuccessors.end())
          newCycle = successorCycle;

        SHAVESchedulePosition& startingCycle = startingCycles[successor->number];
        if (!startingCycle.isInitialised() ||
            (newCycle.branchPosition == startingCycle.branchPosition && newCycle.cycle < startingCycle.cycle))
          startingCycle = newCycle;

        if (newCycle.branchPosition < startingCycle.branchPosition) {
          // FIXME: Movidius - We need to check for reg dependencies with branchNodes at all intermediate branchPositions
          //                   At present they all use I0, so this suffices
          // Move to earlier branch position unless this results in moving a "def i0" over a "JMPcc i0"
          if (!(blockUsesI0Jump && successor->hasDownstreamI0))
            startingCycle = newCycle;
        }
      }

      SHAVESchedulePosition nopCycle = cycle;
      // Insert NOPs for the branch into the schedule
      for (int i = 0; i < SII->getDelaySlots(); ++i) {
        nopCycle.cycle++;
        getCycle(nopCycle); // get implicitly initialises
      }

      // Set the ports used by this node
      setPortUsage(branchNode->instructions, cycle);
    }

    first = false;
  }

  DEBUG(dbgs() << "SHAVEPostRAScheduler: Initialised schedule:\n");
  DEBUG(dumpSchedule(dbgs()));
  DEBUG(dbgs() << "\n");
}

void SHAVEPostRASchedule::scheduleNode(ConstNodePtr node) {
  SHAVESchedulePosition cycle = calculateStartingCycle(node);

  unsigned int latency = getNodeLatency(node);

  DEBUG(dbgs() << "SHAVEPostRAScheduler: Starting at cycle " << cycle.branchPosition << ", " << cycle.cycle << "\n");

  std::vector<SHAVEModification> modificationSet;
  bool canSchedule = false;

  while (!canSchedule) {
    canSchedule = true;
    conflicts.clearModificationActions(modificationSet);

    SHAVEConflictTypeSet conflictsSet(0);

    conflictsSet = conflicts.findConflicts(node, cycle, modificationSet);

    if (conflictsSet.any()) {
      conflicts.handleConflicts(node, cycle, conflictsSet, modificationSet);

      if (conflictsSet.none())
        conflictsSet = conflicts.findConflicts(node, cycle, modificationSet, true);
    }

    canSchedule &= conflictsSet.none();

    if (!canSchedule)
      cycle = getNextCycleForNode(node, cycle, latency);
  }

  // Insert the node at cycle
  scheduleNodeAtCycle(node, cycle, modificationSet);

  // Update the starting cycle for each of this node's successors
  // FIXME: Using a single latency value creates a problem for instructions with multiple
  // outputs (like LSU.LDI). We want to update the successor cycle based on the specific
  // latency for the operand which causes the dependency. Presently the latency is the
  // maximum of all operand latencies. See bug #28451
  updateSuccessorStartingCycles(node, cycle, latency);
}

void SHAVEPostRASchedule::emitSchedule() {
  DEBUG(dbgs() << "SHAVEPostRAScheduler: Dependency graph scheduled. Machine function schedule is now:\n");
  DEBUG(dumpSchedule(dbgs()));
  DEBUG(dbgs() << "\n");

  DebugLoc dbgLoc;

  assert(std::all_of(completedNodes.begin(), completedNodes.end(), [](bool complete) { return complete; }) && "Some nodes have not been scheduled");

  unsigned int size = block->size();
  for (unsigned int i = 0; i < size; ++i) {
    MachineInstr* instr = &*(block->instr_begin());
    instr->removeFromBundle();
  }

  for (const auto& schedule : { schedules.first, schedules.second }) {
    for (const auto& cycle : schedule) {
      const SHAVEPostRACycle& cycleInfo = cycle.second;

      MachineBasicBlock::iterator insertPosition = block->end();

      if (cycleInfo.instructions.empty()) {
        BuildMI(*block, insertPosition, dbgLoc, SII->get(SHAVE::NOP));
      }
      else {
        MachineInstr* firstInstr = cycleInfo.instructions[0];
        MachineInstr* firstCopy = function->CloneMachineInstr(firstInstr);

        block->insert(block->end(), firstCopy);

        for (unsigned int i = 1; i < cycleInfo.instructions.size(); ++i) {
          MachineInstr* copy = function->CloneMachineInstr(cycleInfo.instructions[i]);
          block->insert(block->end(), copy);
          copy->bundleWithPred();
        }

        for (ConstNodePtr node : cycleInfo.nodes) {
          for (MachineInstr* debugInstr : node->debugInstructions) {
            MachineInstr* copy = function->CloneMachineInstr(debugInstr);
            block->insert(block->end(), copy);
          }
        }
      }
    }
  }

  for (MachineInstr* debugInstr : schedulerInfo.entryDebugInstructions) {
    MachineInstr* copy = function->CloneMachineInstr(debugInstr);
    block->insert(block->begin(), copy);
  }
}

//
// Definition of public member functions of SHAVEPostRASchedule used by SHAVEConflicts
//


bool SHAVEPostRASchedule::hasCycle(const SHAVESchedulePosition& position) const {
  if (position.branchPosition == 0)
    return schedules.first.find(position.cycle) != schedules.first.end();
  else
    return schedules.second.find(position.cycle) != schedules.second.end();
}

SHAVEPostRACycle& SHAVEPostRASchedule::getCycle(const SHAVESchedulePosition& position) {
  return position.branchPosition == 0 ? schedules.first[position.cycle]
                                      : schedules.second[position.cycle];
}

SHAVESchedulePosition SHAVEPostRASchedule::getStartingCycle(const ConstNodePtr& node) const {
  SHAVESchedulePosition startingCycle;
  auto it = startingCycles.find(node->number);
  if (it != startingCycles.end())
    startingCycle = it->second;
  return startingCycle;
}

bool SHAVEPostRASchedule::isMemLowHighSequence(ConstNodePtr node) const {
  ConstNodePtr dummyPred;
  return isMemLowHighSequence(node, dummyPred);
}

//
// Definition of member functions of SHAVEPostRASchedule used for debugging
//

#ifndef NDEBUG
void SHAVEPostRASchedule::dumpSchedule(raw_ostream& out) {
  for (const auto& schedule : { schedules.first, schedules.second }) {
    for (const auto& cycle : schedule) {
      out << "Cycle " << cycle.first << ":\n";

      for (ConstNodePtr node : cycle.second.nodes)
        for (MachineInstr* instr : node->instructions)
          instr->dump();

      if (cycle.second.nodes.empty())
        out << "NOP\n";
    }
  }
}
#endif // NDEBUG
