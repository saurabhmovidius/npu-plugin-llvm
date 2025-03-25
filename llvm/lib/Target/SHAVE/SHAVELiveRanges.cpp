//===-- SHAVELiveRanges.cpp - Live Ranges Helper ----------------*- C++ -*-===//
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

#define DEBUG_TYPE "shave-live-ranges"

#include "SHAVELiveRanges.h"

#include <map>

using namespace SHAVELiveRanges;

static bool skipInstruction(const MachineInstr &instruction) {
  return instruction.isDebugValue() || instruction.isCFIInstruction() || instruction.isPosition();
}

//
// LiveRanges private functions
//

void LiveRangesBase::addRangeToRegister(unsigned int reg,
                                        const LiveRange &range,
                                        bool isDef) {
  liveRegisters[reg].push_back(range);
  updateMetadata(reg, range, isDef);
}

void LiveRangesBase::calculateWeights() {
  for (auto &registers : allRegisters) {
    for (unsigned int reg : registers.second) {
      if (liveRegisters.find(reg) == liveRegisters.end()) {
        // Register is not used in this block so give it lowest weight
        registerWeights[registers.first].insert(RegisterWeight(reg, 0));
        registerWeightLookup[reg] = 0;
      }
      else {
        auto &ranges = liveRegisters.at(reg);
        uint64_t weight = 0;
        for (auto &range : ranges) {
          weight += (range.second - range.first) + 1;
        }
        registerWeights[registers.first].insert(RegisterWeight(reg, weight));
        registerWeightLookup[reg] = weight;
      }
    }
  }
}

bool LiveRangesBase::rangesOverlap(const LiveRange &liveRange0, const LiveRange &liveRange1) const {
  if (liveRange0.first >= liveRange1.first && liveRange0.first <= liveRange1.second)
    return true;
    
  if (liveRange0.second >= liveRange1.first && liveRange0.second <= liveRange1.second)
    return true;

  if (liveRange1.first >= liveRange0.first && liveRange1.first <= liveRange0.second)
    return true;
    
  if (liveRange1.second >= liveRange0.first && liveRange1.second <= liveRange0.second)
    return true;

  return false;
}

bool LiveRangesBase::rangeOverlapsWithReg(const unsigned int reg, const LiveRange &liveRange) const {
  // Register is unused so there can be no overlap
  if (liveRegisters.find(reg) == liveRegisters.end())
    return false;
  
  for (const LiveRange &registerRange : liveRegisters.at(reg))
    if (rangesOverlap(registerRange, liveRange))
      return true;

  return false;
}

void LiveRangesBase::removeRangeFromReg(const LiveRange &range, unsigned int reg) {
  auto &ranges = liveRegisters[reg];
  bool moreToProcess;
  uint64_t weightAdjustment = 0;
  do {
    moreToProcess = false;
    for (auto it = ranges.begin(); it != ranges.end(); ++it) {
      if (rangesOverlap(*it, range)) {
        weightAdjustment += (it->second - it->first) + 1u;

        if (range.first == it->first) {
          if (range.second == it->second) {
            ranges.erase(it);
            // Don't set moreToProcess, we don't need to re-enter for this case
            break;
          }
          else if (range.second < it->second) {
            LiveRange newRange(range.second, it->second);
            *it = newRange;
            // Don't set moreToProcess, we don't need to re-enter for this case
            break;
          }
          else { // range.second > it->second
            ranges.erase(it);
            moreToProcess = true;
            break;
          }
        }
        else if (range.first > it->first) {
          if (range.second >= it->second) {
            LiveRange newRange(it->first, range.first);
            *it = newRange;
            if (range.second == it->second)
              break; // Only need to continue if there is some of the range leftover
          }
          else { // range.second < it->second
            LiveRange startRange(it->first, range.first);
            LiveRange endRange(range.second, it->second);
            *it = endRange;
            ranges.insert(it, startRange);
            // Don't set moreToProcess, we don't need to re-enter for this case
            break;
          }
        }
        else { // range.first < it->first
          if (range.second == it->second) {
            ranges.erase(it);
            // Don't set moreToProcess, we don't need to re-enter for this case. The first part of the range
            // has already been handled
            break;
          }
          else if (range.second > it->second) {
            ranges.erase(it);
            moreToProcess = true;
            break;
          }
          else { // range.second < it->second
            LiveRange newRange(range.second, it->second);
            *it = newRange;
            // Don't set moreToProcess, we don't need to re-enter for this case
            break;
          }
        }
      }
    }
  } while (moreToProcess);

  DEBUG(dbgs() << "Removed range " << range.first << "->" << range.second << " from ranges for " << SRI->getName(reg) << "\n");

  const TargetRegisterClass * regClass = SRI->getMinimalPhysRegClass(reg);
  auto &weights = registerWeights[regClass->getID()];
  uint64_t originalWeight = 0;
  for (auto it = weights.begin(); it != weights.end(); ++it) {
    if (it->reg == reg) {
      originalWeight = it->weight;
      weights.erase(it);
      break;
    }
  }

  uint64_t newWeight = originalWeight - weightAdjustment;
  RegisterWeight weight(reg, newWeight);
  registerWeightLookup[reg] = newWeight;

  weights.insert(RegisterWeight(reg, newWeight));

  DEBUG(dbgs() << "Register " << SRI->getName(reg) << " weight has been adjusted from " << originalWeight << " to " << newWeight << "\n");
}

void LiveRangesBase::updateRangesForRegister(const LiveRange &range, unsigned int reg) {
  auto &ranges = liveRegisters[reg]; // Insert if it doesn't exist already
  
  bool inserted = false;
  for (auto it = ranges.begin(); it != ranges.end(); ++it) {
    if (range.second < it->first) {
      ranges.insert(it, range);
      inserted = true;
      break;
    }
  }

  // First use of this register
  if (!inserted)
    ranges.push_back(range);

  DEBUG(dbgs() << "Added range " << range.first << "->" << range.second << " to ranges for " << SRI->getName(reg) << "\n");

  const TargetRegisterClass * regClass = SRI->getMinimalPhysRegClass(reg);
  auto &weights = registerWeights[regClass->getID()];
  uint64_t originalWeight = 0;
  for (auto it = weights.begin(); it != weights.end(); ++it) {
    if (it->reg == reg) {
      originalWeight = it->weight;
      weights.erase(it);
      break;
    }
  }

  uint64_t newWeight = originalWeight + ((range.second - range.first) + 1);
  RegisterWeight weight(reg, newWeight);
  registerWeightLookup[reg] = newWeight;

  weights.insert(RegisterWeight(reg, newWeight));

  DEBUG(dbgs() << "Register " << SRI->getName(reg) << " weight has been adjusted from " << originalWeight << " to " << newWeight << "\n");
}

void LiveRangesBase::generateLiveRegisters() {
  std::unordered_map<unsigned int, unsigned int> definedRegisters; // Map registers to the node number that defines them
  unsigned int number = 1;

  // Start by getting all live-in registers in the classes we care about
  for (auto &liveIn : block.liveins()) {
    auto registers = SRI->getPhysicalSHAVERegisters(liveIn.PhysReg);
    for (unsigned int reg : registers) {
      if (reg != SHAVE::NoRegister) {
        addLiveIn(reg);
        definedRegisters[reg] = 0;
      }
    }
  }

  addExtraTrackedRegisters(definedRegisters);

  liveOutNumber = number;
  for (auto &instruction : block.instrs()) {
    if (!skipInstruction(instruction) && !instruction.isBundledWithSucc())
      liveOutNumber += 1;
  }

  for (auto &instruction : block.instrs()) {
    if (skipInstruction(instruction))
      continue;

    // Sometimes an implicit def can appear in the operands list after a kill. In these cases,
    // we do not want to mark the register as killed
    std::set<unsigned int> instructionDefs;

    // Iterate over operands in reverse order so we can easily catch a kill of a register
    // before a new definition
    for (int i = instruction.getNumOperands() - 1; i >= 0; --i) {
      auto &operand = instruction.getOperand(i);

      if (!operand.isReg())
        continue;

      auto registers = SRI->getPhysicalSHAVERegisters(operand.getReg());

      for (unsigned int reg : registers) {
        if (reg == SHAVE::NoRegister)
          continue;

        if (operand.isDef()) {
          unsigned int latency = getInstructionLatency(instruction);
          instructionDefs.insert(reg);
          if (definedRegisters.find(reg) == definedRegisters.end()) {
            definedRegisters[reg] = number + latency;
          }
          else if (operand.isDead()) {
            addRangeToRegister(reg, LiveRange(definedRegisters[reg], number + latency), true);
            definedRegisters.erase(reg);
          }
          else {
            handleUnprocessedDef(reg, number + latency, definedRegisters);
          }
        }
        else if (operand.isKill() && !ignoreKills()) {
          if (definedRegisters.find(reg) != definedRegisters.end() && instructionDefs.find(reg) == instructionDefs.end()) {
            addRangeToRegister(reg, LiveRange(definedRegisters[reg], number));
            definedRegisters.erase(reg);
          }
          else if (!liveRegisters[reg].empty()) {
            liveRegisters[reg].back().second = number;
          }
        }
        else {
          handleUnprocessedUse(reg, number, definedRegisters, /*isAlsoDef=*/ instructionDefs.find(reg) != instructionDefs.end());
        }
      }
    }

    addInstruction(instruction, number);

    if (!instruction.isBundledWithSucc())
      ++number;
  }

  assert(liveOutNumber == number && "SHAVELiveRanges: Mismatch between instruction processing and live-out number generation");

  // Any remaining registers are presumed live-out
  for (auto &liveOut : definedRegisters) {
    liveRegisters[liveOut.first].push_back(LiveRange(liveOut.second, liveOutNumber));
  }

  // Calculate weights associated with each register
  calculateWeights();
}

//
// LiveRanges public functions
//

LiveRangesBase::LiveRangesBase(const MachineBasicBlock &block,
                               const SHAVEInstrInfo * SII,
                               const std::set<unsigned int> availableCalleeSaved,
                               bool trackAllRegisters,
                               bool useI0)
    : SII(SII), SRI(SII->getSHAVERegisterInfo()), block(block), liveOutNumber(0) {
  const MCPhysReg *calleeSavedRegs = block.getParent()->getRegInfo().getCalleeSavedRegs();
  const BitVector &reservedRegs = block.getParent()->getRegInfo().getReservedRegs();

  // Add all registers to the available sets for each supported register class which are not reserved
  for (unsigned int reg : SHAVE::IRF32RegClass)
    if (!reservedRegs.test(reg) || trackAllRegisters)
      availableRegisters[SHAVE::IRF32RegClassID].insert(reg);
  for (unsigned int reg : SHAVE::VRF128RegClass)
    if (!reservedRegs.test(reg) || trackAllRegisters)
      availableRegisters[SHAVE::VRF128RegClassID].insert(reg);
  for (unsigned int reg : SHAVE::WVRF512RegClass)
    if (!reservedRegs.test(reg) || trackAllRegisters)
      availableRegisters[SHAVE::WVRF512RegClassID].insert(reg);

  if (useI0)
    availableRegisters[SHAVE::IRF32RegClassID].insert(SHAVE::I0);

  allRegisters = availableRegisters;

  // FIXME: Movidius - Remove this once we can safely spill and restore any extra callee-saved registers
  //                   which are used a result of the anti-dep breaking
  for (unsigned int i = 0; calleeSavedRegs[i] != 0; ++i) {
    if (availableCalleeSaved.find(calleeSavedRegs[i]) == availableCalleeSaved.end()) {
      availableRegisters[SHAVE::IRF32RegClassID].erase(calleeSavedRegs[i]);
      availableRegisters[SHAVE::VRF128RegClassID].erase(calleeSavedRegs[i]);
      availableRegisters[SHAVE::WVRF512RegClassID].erase(calleeSavedRegs[i]);
    }
  }
}

bool LiveRangesBase::isLiveOut(unsigned int reg) const {
  auto liveRegister = liveRegisters.find(reg);
  if (liveRegister == liveRegisters.end() || liveRegister->second.empty())
    return false;
  return liveRegister->second.back().second == liveOutNumber;
}

unsigned int LiveRangesBase::getReplacementRegister(unsigned int reg, LiveRange &range) {
  const TargetRegisterClass * regClass = SRI->getMinimalPhysRegClass(reg);
  if (regClass == nullptr)
    return SHAVE::NoRegister;

  return getRegister(regClass, reg, range);
}

unsigned int LiveRangesBase::getRegister(const TargetRegisterClass * regClass, unsigned int reg, LiveRange &range) {
  const uint64_t regWeight = registerWeightLookup[reg];
  const uint64_t addedWeight = (range.second - range.first) + 1;

  int leftThreshold = range.first;
  if (reg != SHAVE::NoRegister) {
    for (auto &liveRange : liveRegisters[reg]) {
      if (range.first > liveRange.second) { // Incoming range is after this liveRange
        leftThreshold = range.first - liveRange.second;
      }
      else if (range.first >= liveRange.first && range.first <= liveRange.second) { // Start of incoming range is inside this liveRange
        leftThreshold = 0;
        break;
      }
      else if (range.second <= liveRange.first) { // liveRange is passed the end of the incoming range
        break;
      }
    }
  }

  int rightThreshold = liveOutNumber - range.second;
  if (reg != SHAVE::NoRegister) {
    for (auto &liveRange : liveRegisters[reg]) {
      if (range.second >= liveRange.first && range.second <= liveRange.second) { // End of incoming range in inside this liveRange
        rightThreshold = 0;
        break;
      }
      else if (range.second < liveRange.first) { // End of incoming range is before this liveRange
        rightThreshold = liveRange.first - range.second;
        break;
      }
    }
  }

  struct ReplacementInfo {
    unsigned int reg;
    int leftRange, rightRange;
  };
  // Used during post-RA to find the longest partial availability
  unsigned int partialReg = SHAVE::NoRegister;
  unsigned int partialRange = 0;

  ReplacementInfo candidate = { SHAVE::NoRegister, leftThreshold, rightThreshold };
  const auto &availableRegistersForClass = availableRegisters[regClass->getID()];

  for (auto &newReg : registerWeights[regClass->getID()]) {
    if (availableRegistersForClass.find(newReg.reg) == availableRegistersForClass.end())
      continue;

    if (newReg.reg == reg)
      continue;

    if (newReg.weight == 0 && liveRegisters.find(newReg.reg) == liveRegisters.end()) {
      DEBUG(dbgs() << "Found unused register replacement for sub-graph:\n");
      DEBUG(dbgs() << "  " << SRI->getName(reg) << " -> " << SRI->getName(newReg.reg) << "\n");
      return newReg.reg;
    }
    
    if ((newReg.weight + addedWeight) >= regWeight && !ignoreWeights())
      break;
    
    if (!rangeOverlapsWithReg(newReg.reg, range)) {
      int leftRange = range.first;
      for (auto &liveRange : liveRegisters[newReg.reg]) {
        if (liveRange.second <= range.first)
          leftRange = range.first - liveRange.second;
        else
          break;
      }

      int rightRange = liveOutNumber - range.second;
      for (auto &liveRange : liveRegisters[newReg.reg]) {
        if (liveRange.first >= range.second) {
          rightRange = liveRange.first - range.second;
          break;
        }
      }

      ReplacementInfo replacement = { newReg.reg, leftRange, rightRange };
      
      int leftDistance = replacement.leftRange - candidate.leftRange;
      int rightDistance = replacement.rightRange - candidate.rightRange;

      if (leftDistance > 0 && rightDistance > 0)
        candidate = replacement;
      else if (leftDistance == 0 && rightDistance > 0)
        candidate = replacement;
      else if (rightDistance == 0 && leftDistance > 0)
        candidate = replacement;
    }
    else if (ignoreWeights()) {
      // Find the register with the longest availability from the start of the range
      // and update the liveRange to match
      const auto &ranges = liveRegisters.at(newReg.reg);
      for (unsigned int i = 0; i < ranges.size(); ++i) {
        const auto &liveRange = ranges.at(i);
        if (liveRange.first >= range.first) {
          if (i > 0 && ranges.at(i-1).second >= range.first)
            break; // In the middle of the previous live range, can't be used

          unsigned int distance = liveRange.first - range.first;
          if (distance > partialRange) {
            partialReg = newReg.reg;
            partialRange = distance;
          }
          break;
        }
      }
    }
  }

  if (candidate.reg != SHAVE::NoRegister) {
    unsigned int replacementReg = candidate.reg;
    DEBUG(dbgs() << "Found register replacement for sub-graph:\n");
    DEBUG(dbgs() << "  " << SRI->getName(reg) << " -> " << SRI->getName(replacementReg) << "\n");
    return replacementReg;
  }

  if (ignoreWeights() && partialReg != SHAVE::NoRegister) {
    DEBUG(dbgs() << "Found partial register during post-RA:\n");
    DEBUG(dbgs() << SRI->getName(partialReg) << "\n");
    range.second = range.first + partialRange;
    return partialReg;
  }

  return SHAVE::NoRegister;
}

void LiveRangesBase::update(unsigned int rangeStart, unsigned int rangeEnd, unsigned int originalReg, unsigned int newReg) {
  LiveRange range(rangeStart, rangeEnd);
  removeRangeFromReg(range, originalReg);
  updateRangesForRegister(range, newReg);
}

//
// VerboseLiveRanges private functions
//

void VerboseLiveRanges::addLiveIn(unsigned int reg) {
  liveIns.insert(reg);
}

void VerboseLiveRanges::addExtraTrackedRegisters(RegisterMap &registers) {
  // These registers will be live-in to a function without being marked explicitly as such
  registers[SHAVE::I19] = 0;
  registers[SHAVE::I30] = 0;
  registers[SHAVE::I31] = 0;
}

void VerboseLiveRanges::handleUnprocessedDef(unsigned int reg,
                                             unsigned int position,
                                             RegisterMap &definedRegisters) {
  // For InterblockMovement, we need to split ranges on every def, including re-definitions
  addRangeToRegister(reg, LiveRange(definedRegisters[reg], position), true);
  definedRegisters[reg] = position;
}

void VerboseLiveRanges::handleUnprocessedUse(unsigned int reg,
                                             unsigned int position,
                                             RegisterMap &definedRegisters,
                                             bool isAlsoDef) {
  // For InterblockMovement, we need to split ranges on every use, not just defs and kills
  if (definedRegisters.find(reg) != definedRegisters.end() && !isAlsoDef) {
    addRangeToRegister(reg, LiveRange(definedRegisters[reg], position));
    definedRegisters[reg] = position;
  }
  else if (!liveRegisters[reg].empty()) {
    addRangeToRegister(reg, LiveRange(liveRegisters[reg].back().second, position));
  }
  else {
    addRangeToRegister(reg, LiveRange(0, position));
  }
}

void VerboseLiveRanges::addInstruction(const MachineInstr &instruction,
                                       unsigned int position) {
  instructionLookup[&instruction] = position;
}

void VerboseLiveRanges::updateMetadata(unsigned int reg,
                                       const LiveRange &range,
                                       bool isDef) {
  // Track the number of instructions which use/define each register so that we can
  // estimate the potential instruction movement if an instruction is hoisted into
  // this block
  if (range.second == liveOutNumber) // Only interested in instructions, not live-outs
    return;

  if (isDef) {
    // FIXME: C++17 has a try_emplace function which we can use here after the upgrade to LLVM18
    auto it = definers.find(reg);
    if (it == definers.end())
      it = definers.emplace(reg, BitVector(liveOutNumber, false)).first;
    it->second.set(range.second);
  }
  else {
    auto it = users.find(reg);
    if (it == users.end())
      it = users.emplace(reg, BitVector(liveOutNumber, false)).first;
    it->second.set(range.second);
  }
}

//
// VerboseLiveRanges public functions
//

bool VerboseLiveRanges::isLiveIn(Register reg) const {
  const auto registers = SRI->getPhysicalSHAVERegisters(reg);
  for (const auto physReg : registers) {
    if (physReg == SHAVE::NoRegister)
      continue;

    if (liveIns.find(physReg) != liveIns.end())
      return true;
  }
  return false;
}

bool VerboseLiveRanges::isLiveAt(const MachineInstr &instruction,
                                 const MachineInstr &positionInstruction) const {
  // returns true if the instruction defines a register which is live when position executes
  unsigned int position = instructionLookup.at(&positionInstruction);
  LiveRange range = { position, position };

  for (const auto &def : instruction.defs()) {
    auto registers = SRI->getPhysicalSHAVERegisters(def.getReg());

    for (unsigned int reg : registers) {
      if (reg == SHAVE::NoRegister)
        continue;

      if (rangeOverlapsWithReg(reg, range))
        return true;
    }
  }

  return false;
}

bool VerboseLiveRanges::isHoistable(const MachineInstr &instruction) const {
  // returns true if the instruction does not rely on any other instructions before it
  // in the block
  unsigned int position = instructionLookup.at(&instruction);

  for (const auto &def : instruction.defs()) {
    auto registers = SRI->getPhysicalSHAVERegisters(def.getReg());

    for (unsigned int reg : registers) {
      if (reg == SHAVE::NoRegister)
        continue;

      const auto &firstRange = liveRegisters.at(reg).front();

      // This is the first instruction to define this register in the block
      if (firstRange.first == position)
        continue;

      // The register is live-in to the block and this instruction is the first user (with a redefinition)
      if (firstRange.first == 0 && firstRange.second >= position)
        continue;

      return false;
    }
  }

  for (const auto &use : instruction.uses()) {
    if (!use.isReg())
      continue;

    const auto registers = SRI->getPhysicalSHAVERegisters(use.getReg());

    for (unsigned int reg : registers) {
      if (reg == SHAVE::NoRegister)
        continue;

      auto registerRanges = liveRegisters.find(reg);
      // The only requirement for a use is that it is defined before entry to this block
      if (registerRanges != liveRegisters.end() && !registerRanges->second.empty()) {
        const auto &firstRange = registerRanges->second.front();
        if (firstRange.first != 0 || firstRange.second < position)
          return false;
      }
    }
  }

  return true;
}

void VerboseLiveRanges::addInstructionToLiveOuts(const MachineInstr &instruction) {
  for (const auto &def : instruction.defs()) {
    auto registers = SRI->getPhysicalSHAVERegisters(def.getReg());

    for (unsigned int reg : registers) {
      if (reg == SHAVE::NoRegister)
        continue;

      if (liveRegisters[reg].empty() || liveRegisters[reg].back().second < liveOutNumber)
        addRangeToRegister(reg, LiveRange(liveOutNumber, liveOutNumber));
    }
  }

  instructionLookup[&instruction] = liveOutNumber;
}

void VerboseLiveRanges::addInstructionToLiveIns(const MachineInstr &instruction) {
  unsigned int position = instructionLookup.at(&instruction);

  for (const auto &operand : instruction.operands()) {
    if (!operand.isReg())
      continue;

    auto registers = SRI->getPhysicalSHAVERegisters(operand.getReg());

    for (unsigned int reg : registers) {
      if (reg == SHAVE::NoRegister)
        continue;

      // Add the register to the set of live-ins for this block
      liveIns.insert(reg);

      // Then update the first live range for the register (if it exists) to mark it is
      // defined before entry to the block
      auto it = liveRegisters.find(reg);
      if (it == liveRegisters.end())
        continue;

      auto &registerRanges = it->second;
      if (registerRanges.empty())
        continue;

      auto &firstRange = registerRanges.front();
      if (firstRange.first == position) // The hoisted instruction was the first definition of the register in this block
        firstRange.first = 0;
      if (firstRange.second == position) { // The register was already live-in and the hoisted instruction was its first user
        registerRanges.erase(registerRanges.begin());
        if (!registerRanges.empty() && registerRanges.front().first == position)
          registerRanges.front().first = 0; // Update the next user of this register to mark live-in
      }
    }
  }
}

BitVector VerboseLiveRanges::getUsers(const MachineInstr &instruction) const {
  // Given an instruction from another block, get the set of instructions in this block
  // which would be potentially pushed up the schedule if it were to be hoisted here
  BitVector allUses(liveOutNumber, false);
  for (const auto &operand : instruction.operands()) {
    if (!operand.isReg())
      continue;

    const auto registers = SRI->getPhysicalSHAVERegisters(operand.getReg());
    const bool isDef = operand.isDef();

    for (unsigned int reg : registers) {
      if (reg == SHAVE::NoRegister)
        continue;

      auto user = users.find(reg);
      if (isDef && user != users.end())
        allUses |= user->second;

      auto definer = definers.find(reg);
      if (definer != definers.end())
        allUses |= definer->second;
    }
  }
  return allUses;
}

//
// LiveRanges public DEBUG functions
//

#ifndef NDEBUG
void LiveRangesBase::dump(const SHAVEInstrInfo *SII, const SHAVERegisterInfo *SRI) {
  // Each entry is a list of node names which should be placed at the same "rank" in the graph
  // This is what ensures all nodes line up for each instruction
  std::map<unsigned int, std::string> rankedNodes;

  // Boilerplate to setup graph
  DEBUG(dbgs() << "digraph{\n");
  DEBUG(dbgs() << "rankdir=\"LR\";\n");
  DEBUG(dbgs() << "node[shape=\"plaintext\"];\n");

  // Nodes for each instruction, labelled with instruction opcode name
  unsigned int number = 1;
  DEBUG(dbgs() << "N0[style=invis];\n"); // N0 for live-ins
  rankedNodes[0] = "N0";
  for (auto &instruction : block.instrs()) {
    if (!skipInstruction(instruction)) {
      DEBUG(dbgs() << "N" << number << "[label=\"" << SII->getName(instruction.getOpcode()) << "\"];\n");
      rankedNodes[number] = "N" + std::to_string(number);
      ++number;
    }
  }
  DEBUG(dbgs() << "N" << number << "[style=invis];\n"); // Nlast for live-outs
  rankedNodes[number] = "N" + std::to_string(number);
  const unsigned int liveOutNumber = number;

  // Create a line of instruction nodes, left-to-right in the order they appear in the block
  DEBUG(dbgs() << "edge[weight=1000];\n"); // Set edge weight high to force the instructions into a straight line
  for (unsigned int i = 0; i < number; ++i) {
    DEBUG(dbgs() << "N" << i << "->N" << i+1 << "[style=invis];\n");
  }

  DEBUG(dbgs() << "edge[weight=1];\n"); // Back to normal for everything else

  // Emit live-ranges, one at a time, in order
  std::map<unsigned int, std::vector<LiveRange>> orderedRanges(liveRegisters.begin(), liveRegisters.end());
  for (auto &registerRanges : orderedRanges) {
    std::string regName = std::string(SRI->getName(registerRanges.first));
    std::string lastNode = "";
    for (auto &range : registerRanges.second) {
      std::string startNode = regName + "_" + std::to_string(range.first);
      std::string endNode = regName + "_" + std::to_string(range.second);

      if (lastNode != startNode) {
        if (lastNode == "") {
          // Create an invisibile node for each register at position 0, unless the register is live-in
          // in which case, make it visible
          lastNode = regName + "_0";
          DEBUG(dbgs() << lastNode);
          if (lastNode == startNode)
            DEBUG(dbgs() << "[label=\"" << regName << "\"]");
          else
            DEBUG(dbgs() << "[style=invis]");
          DEBUG(dbgs() << ";\n");
          rankedNodes[0] += " " + regName + "_0";
        }
        // Add an invisible line to ensure the same register sticks to the same horizontal line
        if (startNode != lastNode)
          DEBUG(dbgs() << lastNode << "->" << startNode << "[style=invis];\n");
      }

      // Create an invisibible edge from N<val> to Reg<val> so that they can be forced onto
      // the same rank, and therefore the same vertical plane
      DEBUG(dbgs() << startNode << "[label=\"" << regName << "\"];\n");
      DEBUG(dbgs() << endNode << "[label=\"" << regName << "\"];\n");
      // Link start and end with a visible line
      DEBUG(dbgs() << startNode << "->" << endNode << ";\n");

      rankedNodes[range.first] += " " + startNode;
      rankedNodes[range.second] += " " + endNode;

      lastNode = endNode;
    }

    // Insert an invisible final node and invisible edge to keep everything straight
    std::string liveOutNode = regName + "_" + std::to_string(liveOutNumber);
    if (lastNode != liveOutNode) {
      DEBUG(dbgs() << liveOutNode << "[style=invis];\n");
      DEBUG(dbgs() << lastNode << "->" << liveOutNode << "[style=invis];\n");
      rankedNodes[liveOutNumber] += " " + liveOutNode;
    }
  }

  for (auto &ranks : rankedNodes) {
    DEBUG(dbgs() << "{ rank=same; " << ranks.second << "; }\n");
  }

  DEBUG(dbgs() << "}\n\n");

  // Dump assignable registers in order of weight
  DEBUG(dbgs() << "Available registers and weights:\n");
  
  DEBUG(dbgs() << "  IRF32:\n");
  for (auto &weight : registerWeights[SHAVE::IRF32RegClassID])
    DEBUG(dbgs() << "    " << SRI->getName(weight.reg) << " - " << weight.weight << "\n");

  if (SII->hasFeature(SHAVE::HasVRF128_Feature)) {
    DEBUG(dbgs() << "  VRF128:\n");
    for (auto &weight : registerWeights[SHAVE::VRF128RegClassID])
      DEBUG(dbgs() << "    " << SRI->getName(weight.reg) << " - " << weight.weight << "\n");
  }
  else {
    DEBUG(dbgs() << "  WVRF512:\n");
    for (auto &weight : registerWeights[SHAVE::WVRF512RegClassID])
      DEBUG(dbgs() << "    " << SRI->getName(weight.reg) << " - " << weight.weight << "\n");
  }
  DEBUG(dbgs() << "\n");
}
#endif // NDEBUG
