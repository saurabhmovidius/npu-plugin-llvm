//===-- SHAVELiveRanges.h - Live Ranges Helper ------------------*- C++ -*-===//
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

#include "SHAVE.h"
#include "SHAVEInstrInfo.h"
#include "SHAVERegisterInfo.h"

#include <llvm/ADT/BitVector.h>

#include <unordered_map>
#include <set>
#include <utility>
#include <vector>

using namespace llvm;

namespace SHAVELiveRanges {

class RegisterWeight {
// This class enables ordering of the available registers by their assigned weights
// The weight is effectively the number of instructions the register is live across.
// This means a register which is live across the entire block will have a weight of
// (a little more than) block.size(), and a register which is not used will have a
// weight of zero.
public:
  unsigned int reg;
  uint64_t weight;

  RegisterWeight(unsigned int reg, uint64_t weight) : reg(reg), weight(weight) {}

  bool operator<(const RegisterWeight &other) const {
    if (weight < other.weight)
      return true;

    if (weight == other.weight)
      return reg < other.reg;

    return false;
  }
};

typedef std::pair<unsigned int, unsigned int> LiveRange;
typedef std::unordered_map<unsigned int, unsigned int> RegisterMap;

class LiveRangesBase {
protected:
  std::unordered_map<unsigned int, std::vector<LiveRange>> liveRegisters; // Map registers to live ranges
  std::unordered_map<unsigned int, std::set<unsigned int>> allRegisters; // Set of all registers per reg-class
  std::unordered_map<unsigned int, std::set<unsigned int>> availableRegisters; // Set of available registers per reg-class
  std::unordered_map<unsigned int, std::set<RegisterWeight>> registerWeights; // Weights for registers, organised by reg-class
  std::unordered_map<unsigned int, uint64_t> registerWeightLookup; // Lookup for register weights

  const SHAVEInstrInfo * SII;
  const SHAVERegisterInfo *SRI;
  const MachineBasicBlock &block;

  unsigned int liveOutNumber;

  virtual void addLiveIn(unsigned int reg) {}
  virtual void addExtraTrackedRegisters(RegisterMap &registers) {}
  virtual void handleUnprocessedDef(unsigned int reg,
                                    unsigned int position,
                                    RegisterMap &definedRegisters) {}
  virtual void handleUnprocessedUse(unsigned int reg,
                                    unsigned int position,
                                    RegisterMap &definedRegisters,
                                    bool isAlsoDef) {}
  virtual void addInstruction(const MachineInstr &instruction,
                              unsigned int position) {}
  virtual unsigned int getInstructionLatency(const MachineInstr &instruction) { return 0; }
  virtual bool ignoreKills() { return false; }
  virtual bool ignoreWeights() { return false; }
  virtual void updateMetadata(unsigned int reg,
                              const LiveRange &range,
                              bool isDef) {}

  void addRangeToRegister(unsigned int reg,
                          const LiveRange &range,
                          bool isDef = false);
  void calculateWeights();
  bool rangesOverlap(const LiveRange &liveRange0, const LiveRange &liveRange1) const;
  bool rangeOverlapsWithReg(const unsigned int reg, const LiveRange &liveRange) const;
  void removeRangeFromReg(const LiveRange &range, unsigned int reg);
  void updateRangesForRegister(const LiveRange &range, unsigned int reg);
  void generateLiveRegisters();

public:
  LiveRangesBase(const MachineBasicBlock &block,
                 const SHAVEInstrInfo * SII,
                 const std::set<unsigned int> availableCalleeSaved,
                 bool trackAllRegisters = false,
                 bool useI0 = false);

  bool isLiveOut(unsigned int reg) const;
  unsigned int getReplacementRegister(unsigned int reg, LiveRange &range);
  unsigned int getRegister(const TargetRegisterClass * regClass, unsigned int reg, LiveRange &range);
  void update(unsigned int rangeStart, unsigned int rangeEnd, unsigned int originalReg, unsigned int newReg);
#ifndef NDEBUG
  void dump(const SHAVEInstrInfo *SII, const SHAVERegisterInfo *SRI);
#endif // NDEBUG
};

//
// LiveRanges - Implementation of LiveRangesBase used by SHAVEAntiDependencyBreaker with no override
//
class LiveRanges : public LiveRangesBase {
public:
  LiveRanges(const MachineBasicBlock &block,
             const SHAVEInstrInfo * SII,
             const std::set<unsigned int> availableCalleeSaved)
    : LiveRangesBase(block, SII, availableCalleeSaved) {
    generateLiveRegisters();
  }
};

//
// PostSchedLiveRanges - A specialised implementation of LiveRanges for SHAVEPostSchedulingPreemption which
//                       ignores register weights and makes I0 available for use
//

class PostSchedLiveRanges : public LiveRangesBase {
private:
  unsigned int getInstructionLatency(const MachineInstr &instruction) override { return SII->GetSchedMaxLatency(&instruction); }
  // Treat a definition/use chain in a delay slot as the definition for the original instruction, extending its live range
  // This code will execute when a register is "redefined" (and used) in the delay slot of another instruction which defines that register
  void handleUnprocessedDef(unsigned int reg,
                            unsigned int position,
                            RegisterMap &definedRegisters) override { definedRegisters[reg] = std::min(definedRegisters[reg], position); }
  // Final use may have been moved upwards by scheduler, so isKill is no longer valid
  bool ignoreKills() override { return true; }
  bool ignoreWeights() override { return true; }

public:
  PostSchedLiveRanges(const MachineBasicBlock &block,
                      const SHAVEInstrInfo * SII,
                      const std::set<unsigned int> availableCalleeSaved)
    : LiveRangesBase(block, SII, availableCalleeSaved, false, true) {
    generateLiveRegisters();
  }
};

//
// VerboseLiveRanges - A specialised implementation of LiveRanges for SHAVEInterblockMovement
//                     This version tracks all registers with a higher granularity than the typical LiveRanges
//                     by splitting the ranges on every single register def and use
//

class VerboseLiveRanges : public LiveRangesBase {
private:
  std::unordered_map<const MachineInstr *, unsigned int> instructionLookup; // Lookup for the position of instructions in the live ranges
  std::set<unsigned int> liveIns; // Set of live-in physical registers for the block
  std::unordered_map<unsigned int, BitVector> users;
  std::unordered_map<unsigned int, BitVector> definers;

  void addLiveIn(unsigned int reg) override;
  void addExtraTrackedRegisters(RegisterMap &registers) override;
  void handleUnprocessedDef(unsigned int reg,
                            unsigned int position,
                            RegisterMap &definedRegisters) override;
  void handleUnprocessedUse(unsigned int reg,
                            unsigned int position,
                            RegisterMap &definedRegisters,
                            bool isAlsoDef) override;
  void addInstruction(const MachineInstr &instruction,
                      unsigned int position) override;
  void updateMetadata(unsigned int reg,
                      const LiveRange &range,
                      bool isDef) override;

public:
  VerboseLiveRanges(const MachineBasicBlock &block,
                    const SHAVEInstrInfo * SII)
    : LiveRangesBase(block, SII, std::set<unsigned int>(), true) {
    generateLiveRegisters();
  }

  bool isLiveIn(Register reg) const;
  bool isLiveAt(const MachineInstr &instruction,
                const MachineInstr &positionInstruction) const;
  bool isHoistable(const MachineInstr &instruction) const;
  void addInstructionToLiveOuts(const MachineInstr &instruction);
  void addInstructionToLiveIns(const MachineInstr &instruction);
  BitVector getUsers(const MachineInstr &instruction) const;
};

} // namespace SHAVELiveRanges
