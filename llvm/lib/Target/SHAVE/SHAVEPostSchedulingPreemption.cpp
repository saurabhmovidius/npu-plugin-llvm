//===-- SHAVEPostSchedulingPreemption.cpp - NPU5 Preemption  -*- C++ -*----===//
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
// Inserts the code needed to enable/disable code protection on NPU5+ for
// preemption support
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "shave-post-scheduling-preemption"

#include "SHAVEPostSchedulingPreemption.h"

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Debug.h"

using namespace llvm;
using namespace SHAVEPostSchedulingPreemptionNS;

STATISTIC(NumberOfLoopHeaders, "Number of loop headers seen");
STATISTIC(HeaderInserted, "Number of loop headers with no additional cycles added");
STATISTIC(HeaderExtra1Cycles, "Number of loop headers with 1 additional cycle added");
STATISTIC(HeaderExtra2Cycles, "Number of loop headers with 2 additional cycles added");
STATISTIC(HeaderExtra3Cycles, "Number of loop headers with 3 additional cycles added");
STATISTIC(HeaderExtra4Cycles, "Number of loop headers with 4 additional cycles added");
STATISTIC(AdditionalBlocks, "Number of extra blocks with preemption inserted");

char SHAVEPostSchedulingPreemption::ID = 0;

INITIALIZE_PASS_BEGIN(SHAVEPostSchedulingPreemption, "shavepostschedulingpreemptionpass", "SHAVE Post Scheduling Preemption Pass", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(SHAVEPostSchedulingPreemption, "shavepostschedulingpreemptionpass", "SHAVE Post Scheduling Preemption Pass", false, false)

FunctionPass *llvm::createSHAVEPostSchedulingPreemptionPass() {
  return new SHAVEPostSchedulingPreemption();
}

static bool isDebug(const MachineInstr &instruction) {
  return instruction.isDebugValue() || instruction.isCFIInstruction() || instruction.isPosition() || instruction.isKill();
}

/*
 * RangeCycleData Functions
 */

RangeCycleData::RangeCycleData(const SHAVEInstrInfo * SII, MachineBasicBlock &block, unsigned int start, unsigned int end, unsigned int reg) :
    SII(SII), block(block), functionalUnitUsage(end-start), portUsage(end-start), start(start), end(end), reg(reg) {
  // Inserting into header and there are no cycles free at the beginning
  if (start == end)
    return;

  unsigned int cycle = 0;
  for (const auto &instruction : block.instrs()) {
    if (isDebug(instruction))
      continue;

    if (instruction.isInlineAsm()) {
      cycle += 1;
      continue;
    }

    if (instruction.getOpcode() != SHAVE::NOP) {
      if (cycle >= start) {
        // Set functional unit usage
        TrackedFUs unit = TrackedFUs::NumFunctionalUnits;
        unsigned int opcode = instruction.getOpcode();

        if (SHAVEConflicts::check_usesCMU(opcode)) {
          unit = TrackedFUs::CMU;
        }
        else if (SHAVEConflicts::check_isPredicating(opcode)) {
          if (opcode != SHAVE::PEU_PC1C && opcode != SHAVE::PEU_PCCX) {
            unit = TrackedFUs::PEU;
          }
          else {
            unsigned conditionCode = instruction.getOperand(0).getImm();
            // PEU.PCCX does not support O or UO condition codes
            if (conditionCode == SHAVECC::O || conditionCode == SHAVECC::UO)
              unit = TrackedFUs::PEU;
          }
        }
        else {
          const int functionalUnitOperandIndex = SII->getFUnitOperandIndex(&instruction);

          if (functionalUnitOperandIndex != -1) {
            const unsigned int LSU = instruction.getOperand(functionalUnitOperandIndex).getImm();
            if      (LSU == SHAVE::LSU0) unit = TrackedFUs::LSU0;
            else if (LSU == SHAVE::LSU1) unit = TrackedFUs::LSU1;
          }
        }

        if (unit != TrackedFUs::NumFunctionalUnits)
          functionalUnitUsage.at(cycle-start).set((size_t) unit);
      }

      // Set port usage
      TargetSchedModel::ProcResIter portsBegin, portsEnd;
      SII->GetSchedPortRange(&instruction, portsBegin, portsEnd);

      for (TargetSchedModel::ProcResIter p = portsBegin; p != portsEnd; ++p) {
        const uint64_t port = p->ProcResourceIdx;
        unsigned int minDelay = p->AcquireAtCycle;
        unsigned int maxDelay = p->ReleaseAtCycle - 1;

        for (unsigned int delay = minDelay; delay <= maxDelay; ++delay) {
          // We only care about port usage within the start to end range
          unsigned portCycle = cycle + delay;
          if (portCycle >= start && portCycle < end)
            portUsage.at(portCycle - start).insert(port);
        }
      }
    }

    if (!instruction.isBundledWithSucc())
      cycle += 1;

    if (cycle >= end)
      break;
  }
}

bool RangeCycleData::isUnitAvailable(TrackedFUs unit, unsigned int cycle) const {
  return !functionalUnitUsage.at(cycle - start).test((size_t) unit);
}

bool RangeCycleData::anyPortClashes(const MachineInstr &instruction, unsigned int cycle) {
  // NOTE: All of the instructions we use for preemption have a single cycle latency
  //       so we do not currently need to look beyond this cycle for port usage
  auto ports = portsCache.find(&instruction);
  if (ports == portsCache.end()) {
    TargetSchedModel::ProcResIter portsBegin, portsEnd;
    SII->GetSchedPortRange(&instruction, portsBegin, portsEnd);
    Ports initPorts;

    for (TargetSchedModel::ProcResIter p = portsBegin; p != portsEnd; ++p)
      initPorts.insert(p->ProcResourceIdx);

    auto inserted = portsCache.insert(std::make_pair(&instruction, initPorts));
    ports = inserted.first;
  }

  for (const auto &port : ports->second)
    if (portUsage.at(cycle - start).find(port) != portUsage.at(cycle - start).end())
      return true;

  return false;
}

void RangeCycleData::insert(const MachineInstr &instruction, TrackedFUs unit, unsigned int cycle) {
  // We don't need to track functional unit or port usage for new cycles as the instruction
  // sequence we use will never produce a clash
  if (cycle != 0) {
    functionalUnitUsage.at(cycle - start).set((size_t) unit);
    const auto &ports = portsCache.at(&instruction);
    portUsage.at(cycle - start).insert(ports.begin(), ports.end());
  }
}

void RangeCycleData::remove(const MachineInstr &instruction) {
  auto ports = portsCache.find(&instruction);
  if (ports != portsCache.end())
    portsCache.erase(ports);
}

/*
 * OptimisedPreemption Functions
 */

void OptimisedPreemption::setRegisterUse(Cycles &cycles, unsigned int reg, unsigned int cycle) {
  if (SHAVE::IRF32RegClass.contains(reg))
    cycles[cycle].set(indexIRF + reg - SHAVE::I0);
  else if (SHAVE::WVRF512RegClass.contains(reg))
    cycles[cycle].set(indexVRF + reg - SHAVE::W0);
  else
    cycles[cycle].set(indexTRF);
}

OptimisedPreemption::Cycles OptimisedPreemption::generateRegisters(std::function<bool(const MachineOperand &)> &operandPredicate) {
  Cycles registers(cycles);

  unsigned int cycle = 0; // First cycle in block is 0, incrementing by 1 per instruction bundle
  int callDelay = -1; // Track if we are in the delay slots of a BRU.SWP
  for (const auto &instruction : block.instrs()) {
    if (isDebug(instruction))
      continue;

    if (instruction.isInlineAsm())
      registers[cycle].set(); // Never safe to bundle with inline assembly

    if (callDelay == 0) {
      // Last cycle was the final delay slot of a BRU.SWP, so ensure code protection is
      // turned on by this cycle
      registers[cycle].set();
      callDelay = -1;
    }

    if (instruction.isCall())
      callDelay = 7;

    for (unsigned int i = 0; i < instruction.getNumOperands(); ++i) {
      const auto &operand = instruction.getOperand(i);
      // Only interested in register operands
      if (!operand.isReg() || !operandPredicate(operand))
        continue;

      // Only interested in IRF, WVRF and TRF registers
      const auto aliases = SRI->getPhysicalSHAVERegisters(operand.getReg());
      SHAVEOpSchedInfo info;
      if(!SII->GetSchedOperand(&instruction, i, info))
        continue;

      for (const auto reg : aliases) {
        if (reg == SHAVE::NoRegister)
          continue;

        // Start at +1 for writes as instructions with 0-latency writes cannot clobber a read by another instruction
        for (unsigned int pending = operand.isDef() ? 1 : 0; pending <= info.Latency && (cycle+pending) < cycles; ++pending)
          setRegisterUse(registers, reg, cycle+pending);
      }
    }

    if (!instruction.isBundledWithSucc()) {
      cycle += 1;
      if (callDelay != -1)
        --callDelay;
    }
  }

  return registers;
}

void OptimisedPreemption::generateWriteRegisters() {
  std::function<bool(const MachineOperand &)> predicate = [](const MachineOperand &operand) { return operand.isDef(); };
  writeRegisters = generateRegisters(predicate);
}

void OptimisedPreemption::generateReadRegisters() {
  std::function<bool(const MachineOperand &)> predicate = [](const MachineOperand &operand) { return !operand.isDef() && !operand.isTied(); };
  readRegisters = generateRegisters(predicate);
}

void OptimisedPreemption::generateFreeCycles() {
  assert(writeRegisters.size() == readRegisters.size());
  freeCycles.resize(writeRegisters.size(), true);

  for (unsigned int i = 0; i < writeRegisters.size(); ++i) {
    auto overlap = writeRegisters.at(i) & readRegisters.at(i);
    if (overlap.none())
      continue;

    for (int backwardsCycle = (int) i; backwardsCycle >= 0; --backwardsCycle) {
      if (overlap.none())
        break;
      freeCycles[backwardsCycle] = false;
      overlap &= writeRegisters.at(backwardsCycle); // Discard any registers whose defining instruction has been passed
    }
  }
}

static bool isBranch(const MachineInstr &instruction) {
  return instruction.isBranch(MachineInstr::AnyInBundle) || instruction.isCall(MachineInstr::AnyInBundle);
}

void OptimisedPreemption::countCycles() {
  nonBranchCycles = 0u;
  cycles = 0u;
  for (auto instruction = block.rbegin(); instruction != block.rend(); ++instruction) {
    if (isDebug(*instruction))
      continue;

    ++nonBranchCycles;
    if (isBranch(*instruction))
      nonBranchCycles = 0;

    ++cycles;
  }
}

void OptimisedPreemption::generatePreemptionRanges() {
  unsigned int cycle = 0;
  unsigned int rangeStart = cycle;

  auto addRange = [this](unsigned int rangeStart, unsigned int cycle) {
    unsigned int rangeEnd = cycle - 1;
    if (cycle != 0 && rangeEnd >= (rangeStart + minCycles)) {
      auto liveRange = LiveRange(rangeStart+1, rangeEnd+1);
      unsigned int reg = liveRanges.getRegister(&SHAVE::IRF32RegClass, SHAVE::NoRegister, liveRange); // LiveRanges start counting at 1, not 0

      if (reg == SHAVE::NoRegister)
        return;

      unsigned int distance = liveRange.second - liveRange.first;
      if (distance < minCycles)
        return;

      rangeStart = liveRange.first - 1;
      rangeEnd = liveRange.second - 1;

      bool inserted = false;
      for (auto it = ranges.begin(); it != ranges.end(); ++it) {
        if (distance > (it->end - it->start)) {
          ranges.insert(it, { rangeStart, rangeEnd, reg });
          inserted = true;
          break;
        }
      }
      if (!inserted)
        ranges.push_back({ rangeStart, rangeEnd, reg });
    }
  };

  for (const auto &instruction : block.instrs()) {
    if (isDebug(instruction))
      continue;

    if (isBranch(instruction)) {
      addRange(rangeStart, cycle + SII->getDelaySlots() + 1);
      break;
    }

    if (!freeCycles.at(cycle)) {
      addRange(rangeStart, cycle);
      rangeStart = cycle + 1;
    }

    if (!instruction.isBundledWithSucc())
      cycle += 1;
  }
}

// Returns the index of the instructionData which was inserted into the function on success
// On failure, returns -1
int OptimisedPreemption::insertInstruction(std::vector<InstructionData> &instructionData, unsigned int &cycle, RangeCycleData &rangeCycleData, bool allowNewCycles) {
  bool newCycle = cycle == 0;

  if (newCycle && !allowNewCycles)
    return -1;

  int useInstruction = -1;
  while (!newCycle) {
    if ((cycle > rangeCycleData.end || cycle <= rangeCycleData.start) && !allowNewCycles)
      return -1;

    for (unsigned int i = 0; i < instructionData.size(); ++i) {
      if (rangeCycleData.isUnitAvailable(TrackedFUs::PEU, cycle - 1) &&
          rangeCycleData.isUnitAvailable(instructionData.at(i).functionalUnit, cycle - 1) &&
          !rangeCycleData.anyPortClashes(*(instructionData.at(i).instructionIt), cycle - 1)) {
        useInstruction = i;
        break;
      }
    }

    if (useInstruction != -1)
      break;

    if (rangeCycleData.moveBackwards)
      --cycle;
    else
      ++cycle;

    if (cycle == 0)
      newCycle = true;
  }

  if (newCycle && !allowNewCycles)
    return -1;

  if (useInstruction == -1)
    useInstruction = 0;

  auto functionalUnit = instructionData.at(useInstruction).functionalUnit;
  auto instructionIt = instructionData.at(useInstruction).instructionIt;
  auto &instruction = *instructionIt;

  if (!newCycle) {
    rangeCycleData.insert(instruction, functionalUnit, cycle - 1);
    instructionData.at(useInstruction).position = cycle;
    // Move to the next cycle for the next instruction
    if (rangeCycleData.moveBackwards)
      cycle -= 1;
    else
      cycle += 1;
  }

  DEBUG(dbgs() << "Inserted instruction: ");
  DEBUG(instruction.dump());
  DEBUG(dbgs() << "  At cycle " << ((int) cycle - 1) << "\n");

  // If the instruction is inserted into a new cycle then there is no need to move or bundle it
  // as it is already at the start of the block
  return useInstruction;
}

OptimisedPreemption::InstructionData OptimisedPreemption::flipCodeProtection(unsigned int &cycle, RangeCycleData &rangeCycleData, bool turnOn, bool allowNewCycles) {
  bool newCycle = cycle == 0;

  if (newCycle && !allowNewCycles)
    return InstructionData();

  auto insertPoint = block.begin();
  DebugLoc dbgLoc;
  if (insertPoint != block.end())
    dbgLoc = insertPoint->getDebugLoc();

  std::vector<InstructionData> instructions;

  unsigned int immediate = turnOn ? (1<<16) : 0;
  unsigned int reg = rangeCycleData.reg;
  // We have a choice of LSU0.LDIH, LSU1.LDIH and CMU.INVB here. Unfortunately, to get the itineraries for these instructions
  // we need to create all instructions and then delete the ones we don't use later.
  SII->finaliseMI(BuildMI(block, block.begin(), dbgLoc, SII->get(SHAVE::LSU_LDIH), reg).addReg(reg).addImm(immediate));
  auto lsu0LDIHIt = block.begin();
  instructions.push_back(InstructionData(lsu0LDIHIt, TrackedFUs::LSU0));

  SII->finaliseMI(BuildMI(block, block.begin(), dbgLoc, SII->get(SHAVE::LSU_LDIH), reg).addReg(reg).addImm(immediate));
  auto lsu1LDIHIt = block.begin();
  lsu1LDIHIt->getOperand(lsu1LDIHIt->getNumOperands() - 1).setImm(SHAVE::LSU1);
  instructions.push_back(InstructionData(lsu1LDIHIt, TrackedFUs::LSU1));

  SII->finaliseMI(BuildMI(block, block.begin(), dbgLoc, SII->get(SHAVE::CMU_INVB), reg).addReg(reg).addImm(16));
  auto invbIt = block.begin();
  instructions.push_back(InstructionData(invbIt, TrackedFUs::CMU));

  int inserted = insertInstruction(instructions, cycle, rangeCycleData, allowNewCycles);

  for (int i = 0; i < (int) instructions.size(); ++i) {
    if (i != inserted) {
      rangeCycleData.remove(*(instructions.at(i).instructionIt));
      instructions.at(i).instructionIt->eraseFromParent();
    }
  }

  if (inserted != -1)
    return instructions.at(inserted);

  return InstructionData();
}

OptimisedPreemption::InstructionData OptimisedPreemption::setB_CFG(unsigned int &cycle, RangeCycleData &rangeCycleData, bool allowNewCycles) {
  auto insertPoint = block.begin();
  DebugLoc dbgLoc;
  if (insertPoint != block.end())
    dbgLoc = insertPoint->getDebugLoc();

  // Insert at begin for now, we can move it to its actual cycle later
  SII->finaliseMI(BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::CMU_CPIT_preemption), SHAVE::B_CFG).addReg(rangeCycleData.reg));
  auto instructionIt = block.begin();

  std::vector<InstructionData> instruction;
  instruction.push_back(InstructionData(instructionIt, TrackedFUs::CMU));

  int inserted = insertInstruction(instruction, cycle, rangeCycleData, allowNewCycles);

  if (inserted == -1) {
    rangeCycleData.remove(*instructionIt);
    instructionIt->eraseFromParent();
    return InstructionData();
  }

  return instruction.front();
}

OptimisedPreemption::InstructionData OptimisedPreemption::getB_CFG(unsigned int &cycle, RangeCycleData &rangeCycleData, bool allowNewCycles) {
  bool newCycle = cycle == 0;

  if (newCycle && !allowNewCycles)
    return InstructionData();

  auto insertPoint = block.begin();
  DebugLoc dbgLoc;
  if (insertPoint != block.end())
    dbgLoc = insertPoint->getDebugLoc();

  // Insert at begin for now, we can move it to its actual cycle later
  SII->finaliseMI(BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::CMU_CPTI_preemption), rangeCycleData.reg).addReg(SHAVE::B_CFG));
  auto instructionIt = block.begin();

  std::vector<InstructionData> instruction;
  instruction.push_back(InstructionData(instructionIt, TrackedFUs::CMU));

  int inserted = insertInstruction(instruction, cycle, rangeCycleData, allowNewCycles);

  if (inserted == -1) {
    rangeCycleData.remove(*instructionIt);
    instructionIt->eraseFromParent();
    return InstructionData();
  }

  return instruction.front();
}

unsigned int OptimisedPreemption::moveInstructions(const std::vector<InstructionData> &instructions) {
  unsigned int extraCycles = 0;

  for (unsigned int i = 0; i < instructions.size(); ++i) {
    auto &instruction = instructions.at(i);
    if (instruction.position == 0) { // Instruction inserted in new cycle
      if (i == 1 && instructions.at(2).position == 0) {
        assert(instruction.instructionIt->getOpcode() == SHAVE::LSU_LDIH && instruction.instructionIt->getOperand(2).getImm() == (1<<16));
        assert(instructions.at(2).instructionIt->getOpcode() == SHAVE::CMU_CPIT_preemption);
        instruction.instructionIt->bundleWithPred();
      }
      else {
        ++extraCycles;
      }

      continue;
    }

    // Move to the correct cycle
    auto moveTo = instruction.instructionIt;
    for (unsigned int j = 0; j < instruction.position; ++j) {
      while (isDebug(*moveTo))
        ++moveTo;
      ++moveTo;
    }

    while (isDebug(*moveTo))
      ++moveTo;

    block.splice(moveTo, &block, instruction.instructionIt);

    // Don't bundle with a NOP, delete it instead
    if (moveTo->getOpcode() == SHAVE::NOP)
      moveTo->eraseFromParent();
    else
      instruction.instructionIt->bundleWithSucc();
  }

  return extraCycles;
}

void OptimisedPreemption::fixPEUInstructions(std::vector<InstructionData> &instructions) {
  // If any of the generated instructions have been bundled with a PEU.PC1C or PEU.PCCX then
  // the PEU instruction needs to be updated so that the inserted instructions always execute

  auto getPCCXMask = [](unsigned int mask, TrackedFUs functionalUnit) {
      switch (functionalUnit) {
      case TrackedFUs::LSU0: mask ^= (1 << 4); break;
      case TrackedFUs::LSU1: mask ^= (1 << 5); break;
      case TrackedFUs::CMU:  mask ^= (1 << 2); break;
      default: llvm_unreachable("Unhandled functional unit when calculating PCCX mask");
      }
      return mask;
    };

  for (auto &instructionData : instructions) {
    MachineInstr * instruction = &*instructionData.instructionIt;
    if (!instruction->isBundledWithSucc())
      continue;

    TrackedFUs functionalUnit = instructionData.functionalUnit;
    MachineBasicBlock::instr_iterator bundle = instruction->getIterator();
    auto bundleHead = bundle;
    while (bundleHead->isBundledWithPred()) --bundleHead; // Insert point should be at the top of the bundle so we can bundleWithSucc
    do {
      ++bundle;

      MachineInstr &bundledInstruction = *bundle;
      unsigned int opcode = bundledInstruction.getOpcode();
      if (opcode != SHAVE::PEU_PC1C && opcode != SHAVE::PEU_PCCX)
        continue;

      if (opcode == SHAVE::PEU_PC1C) {
        DebugLoc dbgLoc = bundledInstruction.getDebugLoc();
        auto PCCX = BuildMI(block, bundleHead, dbgLoc, SII->get(SHAVE::PEU_PCCX));
        PCCX.addImm(bundledInstruction.getOperand(0).getImm()); // Condition Code
        PCCX.addImm(getPCCXMask(0x3F, functionalUnit)); // New mask (predicate everything except the inserted instruction)
        PCCX.copyImplicitOps(bundledInstruction);
        PCCX->bundleWithSucc();

        bundledInstruction.eraseFromBundle(); // Remove the original PC1C
        break;
      }
      else if (opcode == SHAVE::PEU_PCCX) {
        auto &mask = bundledInstruction.getOperand(1);
        mask.setImm(getPCCXMask(mask.getImm(), functionalUnit)); // Make sure the inserted instruction always executes
        break;
      }

      llvm_unreachable("PEU.PC1C or PEU.PCCX should have been handled already");
    } while (bundle->isBundledWithSucc());
  }
}

bool OptimisedPreemption::insertIntoRanges() {
  DEBUG(dbgs() << "Attempting insertion into ranges\n");

  for (const auto &range : ranges) {
    const unsigned int start = range.start;
    const unsigned int end = range.end;
    const unsigned int reg = range.reg;

    DEBUG(dbgs() << "Trying cycle range " << start << " to " << end << " with register " << SRI->getName(reg) << "\n");

    RangeCycleData rangeCycleData(SII, block, start, end, reg);

    unsigned int cycle = end;
    std::vector<InstructionData> instructions;

    auto failure = [&instructions]() {
      for (const auto &instruction : instructions)
        instruction.instructionIt->eraseFromParent();
    };

    // Insert the last two instructions in reverse order at the end of the range
    // Set TRF.B_CFG to turn code protection back on
    auto instruction = setB_CFG(cycle, rangeCycleData, false);
    if (instruction.functionalUnit == TrackedFUs::NumFunctionalUnits) {
      failure();
      continue;
    }
    instructions.push_back(instruction);

    // Flip code protection bit in <reg> to turn it back on
    instruction = flipCodeProtection(cycle, rangeCycleData, true, false);
    if (instruction.functionalUnit == TrackedFUs::NumFunctionalUnits) {
      failure();
      continue;
    }
    instructions.push_back(instruction);

    // Turning code protection off and on again inside a branch's delay slots will not allow
    // preemption to trigger
    if (cycle > nonBranchCycles)
      cycle = nonBranchCycles == 0 ? 0 : nonBranchCycles - 1;

    if (cycle <= start) {
      failure();
      continue;
    }

    // Start inserting from the start of the range so that we can disable code protetion for as many
    // cycles as possible. These three instructions are inserted in the correct order
    rangeCycleData.moveBackwards = false;
    rangeCycleData.end = cycle;
    cycle = start + 1;

    if (cycle >= rangeCycleData.end) {
      failure();
      continue;
    }

    // Copy TRF.B_CFG to <reg> to maintain original values in the low half of the register
    instruction = getB_CFG(cycle, rangeCycleData, false);
    if (instruction.functionalUnit == TrackedFUs::NumFunctionalUnits) {
      failure();
      continue;
    }
    instructions.push_back(instruction);

    // Flip code protection bit in <reg> to turn it back off
    instruction = flipCodeProtection(cycle, rangeCycleData, false, false);
    if (instruction.functionalUnit == TrackedFUs::NumFunctionalUnits) {
      failure();
      continue;
    }
    instructions.push_back(instruction);

    // Set TRF.B_CFG to turn code protection off
    instruction = setB_CFG(cycle, rangeCycleData, false);
    if (instruction.functionalUnit == TrackedFUs::NumFunctionalUnits) {
      failure();
      continue;
    }
    instructions.push_back(instruction);

    // Move the instructions to the correct positions and bundles
    moveInstructions(instructions);

    // Fix any PEU.PC1C or PEU.PCCX instructions so that the inserted instructions always execute
    fixPEUInstructions(instructions);

    return true;
  }

  return false;
}

void OptimisedPreemption::insertIntoHeader() {
  DEBUG(dbgs() << "Inserting at beginning of loop header\n");

  const unsigned int start = 0;
  unsigned int end = 0; // Cycle after the final cycle we can insert into

  const unsigned int maximumCycle = std::min(freeCycles.size(), (size_t) nonBranchCycles + 1u + SII->getDelaySlots()); // Only go as far as the first branch delay slots
  for (; end < maximumCycle && freeCycles.at(end); ++end); // Count the number of available cyles from the start of the block

  DEBUG(dbgs() << "Cycle range " << start << " to " << end << "\n");

  auto liveRange = LiveRange(start+1, end+1);
  unsigned int reg = liveRanges.getRegister(&SHAVE::IRF32RegClass, SHAVE::NoRegister, liveRange); // LiveRanges start counting at 1, not 0
  end = liveRange.second - 1; // Adjust the end cycle to the register's availability

  if (reg == SHAVE::NoRegister) {
    reg = SHAVE::I0;
    end = 0;
  }

  RangeCycleData rangeCycleData(SII, block, start, end, reg);

  unsigned int cycle = end;
  std::vector<InstructionData> instructions;

  // Insert instructions in reverse order
  // Set TRF.B_CFG to turn code protection back on
  auto instruction = setB_CFG(cycle, rangeCycleData, true);
  assert(instruction.functionalUnit != TrackedFUs::NumFunctionalUnits);
  instructions.push_back(instruction);

  // Flip code protection bit in <reg> to turn it back on
  instruction = flipCodeProtection(cycle, rangeCycleData, true, true);
  assert(instruction.functionalUnit != TrackedFUs::NumFunctionalUnits);
  instructions.push_back(instruction);

  if (cycle != 0)
    cycle += 1; // Flipping the code protection bit back on in <reg> can be inserted in parallel with the CMU.CPIT turning it off

  // Turning code protection off and on again inside a branch's delay slots will not allow
  // preemption to trigger
  if (cycle > nonBranchCycles)
    cycle = nonBranchCycles == 0 ? 0 : nonBranchCycles - 1;

  // Set TRF.B_CFG to turn code protection off
  instruction = setB_CFG(cycle, rangeCycleData, true);
  assert(instruction.functionalUnit != TrackedFUs::NumFunctionalUnits);
  instructions.push_back(instruction);

  // Flip code protection bit in <reg> to turn it back off
  instruction = flipCodeProtection(cycle, rangeCycleData, false, true);
  assert(instruction.functionalUnit != TrackedFUs::NumFunctionalUnits);
  instructions.push_back(instruction);

  // Copy TRF.B_CFG to <reg> to maintain original values in the low half of the register
  instruction = getB_CFG(cycle, rangeCycleData, true);
  assert(instruction.functionalUnit != TrackedFUs::NumFunctionalUnits);
  instructions.push_back(instruction);

  // Move the instructions to the correct positions and bundles
  unsigned int extraCycles = moveInstructions(instructions);

  // Fix any PEU.PC1C or PEU.PCCX instructions so that the inserted instructions always execute
  fixPEUInstructions(instructions);

  switch (extraCycles) {
  case 0: ++HeaderInserted; break;
  case 1: ++HeaderExtra1Cycles; break;
  case 2: ++HeaderExtra2Cycles; break;
  case 3: ++HeaderExtra3Cycles; break;
  case 4: ++HeaderExtra4Cycles; break;
  default: llvm_unreachable("More than 4 added cycles shouldn't be possible\n");
  }
}

bool OptimisedPreemption::insert() {
  // We need at least 4 cycles to turn code protection off and back on
  // That cost is only acceptable in loops which must be preemptable
  if (!isLoopHeader && nonBranchCycles < 4u)
    return false;

  DEBUG(dbgs() << "\nBeginning optimised insertion for block BB#" << block.getNumber() << " " << block.getName() << "\n");

  // Generate a bitset of pending register writes for each cycle of the block.
  // For each register at any given cycle, a 1 in the bitset indicates that a write
  // to that register is pending at that cycle. This means that an instruction which
  // writes that register is currently "in-flight" at that cycle.
  // This set will include all cycles from the cycle after the instruction to the cycle before
  // the write occurs.
  generateWriteRegisters();

  // Generate a bitset of pending register read for each cycle of the block.
  // Uses the same mechanism as the write registers bitset, except it starts at the cycle of the
  // instruction itself, rather than the one following it.
  generateReadRegisters();

  // Use the write and read register bitsets to find all cycles where it is safe to turn off
  // code protection before needing to turn it back on.
  // At each cycle, if there is an overlap between the pending write set and the read set, then
  // servicing preemption at that point would cause the read value to be clobbered when the handler
  // returns control to the application. For these cycles, the "freeCycles" set will continue a false
  // value, indicating that preemption is unsafe. For all other cycles, the value is set to true.
  generateFreeCycles();

  DEBUG(dumpOFreeCycles());

  // Find contiguous ranges of cycles for which it is safe to turn off code protection (i.e. freeCycles
  // contains a true value). Ranges are produced in order of longest to shortest, and will meet the minimum
  // required cycles for the code protection off/on instructions.
  generatePreemptionRanges();

  DEBUG(dumpRanges());

  // If there are no ranges which meet the minimum requirements, then don't bother with preemption checks for
  // this block. The only exception to this rule is if the block is a loop header which we must make preemptable
  // to meet the requirements of the feature.
  if (ranges.empty() && !isLoopHeader) {
    DEBUG(dbgs() << "No sufficient ranges available for insertion. Exiting, as block is not a loop header\n");
    return false;
  }

  // Try to insert into available ranges, this is not guaranteed to succeed as there may be no available functional
  // units for the code protection off/on instructions.
  bool inserted = insertIntoRanges();
  if (!inserted && !isLoopHeader) {
    DEBUG(dbgs() << "Unable to insert into any of the available cycle ranges. Exiting, as block is not a loop header\n");
    return false;
  }
  if (inserted) {
    if (!isLoopHeader)
      ++AdditionalBlocks;
    else
      ++HeaderInserted;
    return true;
  }

  // If we failed to insert into the ranges, then force the insertion into loop headers only, likely resulting
  // in extra cycles at the top of the block.
  insertIntoHeader();
  return true;
}

#ifndef NDEBUG
void OptimisedPreemption::dumpOFreeCycles() const {
  dbgs() << "\nRegister overlaps for block BB#" << block.getNumber() << " " << block.getName() << "\n";
  unsigned int cycle = 0; // First cycle in block is 0, incrementing by 1 per instruction bundle
  for (auto &instruction : block.instrs()) {
    if (isDebug(instruction))
      continue;

    std::string tag = std::to_string(cycle) + " " + std::to_string(freeCycles.at(cycle));
    if (instruction.isBundledWithPred())
      dbgs() << (std::string(tag.size(), ' ')) << "* ";
    else
      dbgs() << tag << ": ";
    instruction.dump();

    if (!instruction.isBundledWithSucc())
      cycle += 1;
  }
}

void OptimisedPreemption::dumpRanges() const {
  dbgs() << "\nRanges:\n";
  for (const auto &range : ranges) {
    dbgs() << "    " << range.start << " -> " << range.end << " with register " << SRI->getName(range.reg) << "\n";
  }
}
#endif

/*
 * SHAVEPostSchedulingPreemption Functions
 */

void SHAVEPostSchedulingPreemption::insertUnoptimised(MachineBasicBlock &block) {
  auto insertPoint = block.begin();
  DebugLoc dbgLoc;
  if (insertPoint != block.end())
    dbgLoc = insertPoint->getDebugLoc();

  DEBUG(dbgs() << "Inserting preemption into block #" << block.getNumber() << " " << block.getName() << "\n");

  // Get the value of TRF.B_CFG
  SII->finaliseMI(BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::CMU_CPTI), SHAVE::I0).addReg(SHAVE::B_CFG));

  // Set the high bit to 0 (disable code protection)
  SII->finaliseMI(BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::LSU_LDIH), SHAVE::I0).addReg(SHAVE::I0).addImm(0));

  // Set TRF.B_CFG
  SII->finaliseMI(BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::CMU_CPIT), SHAVE::B_CFG).addReg(SHAVE::I0));

  // Set the high bit to 1 (re-enable code protection)
  SII->finaliseMI(BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::LSU_LDIH), SHAVE::I0).addReg(SHAVE::I0).addImm(1<<16));

  // Set TRF.B_CFG
  SII->finaliseMI(BuildMI(block, insertPoint, dbgLoc, SII->get(SHAVE::CMU_CPIT), SHAVE::B_CFG).addReg(SHAVE::I0));
}

bool SHAVEPostSchedulingPreemption::runOnMachineFunction(MachineFunction &MF) {
  SII = MF.getSubtarget<SHAVESubtarget>().getInstrInfo();

  // Nothing to do if this target doesn't have Code Protection support, preemption has already been handled by SHAVEPreemptionHandler
  if (!SII->hasFeature(SHAVE::HasCodeProtection_Feature))
    return false;

  DEBUG(dbgs() << "SHAVEPostSchedulingPreemption: Running SHAVE Post Scheduling Preemption Pass on function " << MF.getName() << "\n");

  machineLoopInfo = &getAnalysis<MachineLoopInfo>();

  // Register set is needed for picking an IRF register to use for B_CFG
  auto calleeSavedInfo = MF.getFrameInfo().getCalleeSavedInfo();
  std::set<unsigned int> availableCalleeSavedRegs;
  for (CalleeSavedInfo &info : calleeSavedInfo)
    availableCalleeSavedRegs.insert(info.getReg());

  const SHAVETargetMachine &TM = static_cast<const SHAVETargetMachine&>(MF.getTarget());

  bool modified = false;

  for (MachineBasicBlock &block : MF) {
    const MachineLoop * loopInfo = machineLoopInfo->getLoopFor(&block);
    bool isLoopHeader = loopInfo && loopInfo->getHeader() == &block;

    if (isLoopHeader)
      ++NumberOfLoopHeaders;

    if (SHAVEOptions::PreemptionDisableOptimisedPostSched || TM.getOptLevel() == CodeGenOptLevel::None) {
      if (isLoopHeader) {
        insertUnoptimised(block);
        modified = true;
      }
    }
    else {
      OptimisedPreemption optimised(SII, block, availableCalleeSavedRegs, isLoopHeader);
      modified |= optimised.insert();
    }
  }

  DEBUG(dbgs() << "SHAVEPostSchedulingPreemption: Completed SHAVE Preemption Handler Pass on function " << MF.getName() << "\n");

  return modified;
}

void SHAVEPostSchedulingPreemption::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<MachineLoopInfo>();
  MachineFunctionPass::getAnalysisUsage(AU);
}
