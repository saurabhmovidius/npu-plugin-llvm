//===-- SHAVEPreRAScheduler.h - Pre-RA Scheduler Pass -----------*- C++ -*-===//
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

#ifndef SHAVEPRERASCHEDULER_H
#define SHAVEPRERASCHEDULER_H (1)

#include "SHAVESchedulerBase.h"

enum class SHAVESpillStatus {
  NoSpill = 0,
  IRF,
  VRF,
  Both
};

struct SHAVERegisterCosts {
  int IRFcost = 0, VRFcost = 0;

  SHAVERegisterCosts operator+(const SHAVERegisterCosts& other) const {
    SHAVERegisterCosts result;
    result.IRFcost = IRFcost + other.IRFcost;
    result.VRFcost = VRFcost + other.VRFcost;
    return result;
  }
  SHAVERegisterCosts& operator+=(const SHAVERegisterCosts& other) {
    IRFcost += other.IRFcost;
    VRFcost += other.VRFcost;
    return *this;
  }
  SHAVERegisterCosts& operator-=(const SHAVERegisterCosts& other) {
    IRFcost -= other.IRFcost;
    VRFcost -= other.VRFcost;
    return *this;
  }
  // WARNING: Comparison operators are provided only for checking against upper limits
  // (i.e. either cost is >= or >) and are not suitable for any kind of sorting
  bool operator>=(const SHAVERegisterCosts& other) const {
    return IRFcost >= other.IRFcost || VRFcost >= other.VRFcost;
  }
  bool operator>(const SHAVERegisterCosts& other) const {
    return IRFcost > other.IRFcost || VRFcost > other.VRFcost;
  }
  int sum() const {
    return IRFcost + VRFcost;
  }

#ifndef NDEBUG
  std::string to_string() {
    return "IRF: " + std::to_string(IRFcost) + " VRF: " + std::to_string(VRFcost);
  }
#endif
};

struct SHAVEPreRARegister {
  Register reg = 0;
  int cost = 0;
  SHAVEPreRARegister(Register reg, int cost)
    : reg(reg), cost(cost) {}
};

struct SHAVERegisterKill : public SHAVEPreRARegister {
  SHAVERegisterKill(Register reg, int cost)
    : SHAVEPreRARegister(reg, cost) {}
};

struct SHAVERegisterDef : public SHAVEPreRARegister {
  bool isEarlyClobber = false;
  SHAVERegisterDef(Register reg, int cost, bool isEarlyClobber)
    : SHAVEPreRARegister(reg, cost), isEarlyClobber(isEarlyClobber) {}
};

struct SHAVERegisterCounter {
  std::vector<SHAVERegisterDef> defs;
  std::vector<SHAVERegisterKill> kills;
};
typedef std::map<unsigned int, SHAVERegisterCounter> SHAVEInstructionRegisterCounter;

class SHAVEPreRALiveRegisterSets {
private:
  typedef std::map<unsigned int, int> SHAVELiveRegisterCounter;
  typedef std::pair<int, unsigned int> SHAVERegisterCost;

  static const unsigned int numAvailableRegisterClasses = 3u;
  static const TargetRegisterClass * availableRegisterClasses[numAvailableRegisterClasses];

  std::vector<SHAVELiveRegisterCounter> liveRegisterSets;
  SHAVELiveRegisterCounter numberOfAvailableRegisters;
  SHAVELiveRegisterCounter liveIns;
  const SHAVEInstrInfo * SII;
  const SHAVERegisterInfo * SRI;
  LiveIntervals * LIS;
  MachineRegisterInfo * MRI;
  std::unordered_map<unsigned int, std::set<MachineInstr *>> killedRegisterUsers;
  std::unordered_map<unsigned int, SHAVECycle> lastUser;
  SHAVELiveRegisterCounter newCycleAdjustment;

  SHAVERegisterCost getRegisterCost(unsigned int reg) const;

public:
  SHAVEPreRALiveRegisterSets(MachineBasicBlock * MBB, const SHAVEInstrInfo * SII, const SHAVERegisterInfo* SRI, LiveIntervals * LIS);

  void insertNode(SHAVEDependencyNodePtr node, SHAVECycle cycle);
  void insertCycle(SHAVECycle cycle);
  SHAVEInstructionRegisterCounter getNodeCounters(SHAVEDependencyNodePtr node) const;
  SHAVESpillStatus willSpill(SHAVEDependencyNodePtr node, SHAVECycle cycle);
  SHAVERegisterCosts getNodeCost(SHAVEDependencyNodePtr node) const;
  SHAVERegisterCosts getCycle(SHAVECycle cycle) const;
  SHAVERegisterCosts getLimits() const;
  bool atOrBeyondLimits(SHAVECycle cycle) const;
  SHAVERegisterCosts getLiveIns() const;
#ifndef NDEBUG
  void dump(raw_ostream &out, SHAVECycle cycle);
#endif // NDEBUG
};

class SHAVEPreRACycle {
public:
  std::vector<SHAVEDependencyNodePtr> nodes;

  SHAVEPreRACycle(SHAVEDependencyNodePtr node) : nodes(1, node) {}
  SHAVEPreRACycle() {}
};

class SHAVEPreRASchedule {
private:
  //
  // private variables
  //
  std::map<SHAVECycle, SHAVEPreRACycle> schedule;

  bool targetUsesConflicts = false;
  MachineBasicBlock* currentBasicBlock = nullptr;
  const SHAVEInstrInfo* SII = nullptr;
  LiveIntervals* LIS = nullptr;
  SHAVEPreRALiveRegisterSets liveRegisterSets;

  // Lookup for the cycle each node has been scheduled at
  std::unordered_map<unsigned int, SHAVESchedulePosition> cycles;
  // Bitset of nodes which have been scheduled already, indexed using node numbers
  std::vector<bool> completedNodes;
  bool isList = false;

  //
  // private functions
  //
  SHAVEConflictSet getModifiedConflictSet(unsigned int opcode, SHAVEConflictSet conflicts1, SHAVEConflictSet conflicts2);
  SHAVEConflictSet getFunctionalUnits(MachineInstr* instr);
  bool isBundled(SHAVECycle cycle);
  void reorderSchedule();
  SHAVEConflictTypeSet findConflicts(SHAVEDependencyNodePtr node, SHAVECycle cycle);
  void scheduleNode(SHAVEDependencyNodePtr node, SHAVECycle cycle);

public:
  //
  // public functions
  //
  SHAVEPreRASchedule(bool targetUsesConflicts,
                     MachineBasicBlock* currentBasicBlock,
                     const SHAVEInstrInfo* SII,
                     LiveIntervals* LIS,
                     const SHAVEPreRALiveRegisterSets &liveRegisterSets,
                     size_t numNodes)
    : targetUsesConflicts(targetUsesConflicts),
      currentBasicBlock(currentBasicBlock),
      SII(SII),
      LIS(LIS),
      liveRegisterSets(liveRegisterSets),
      completedNodes(numNodes, false)
      {}

  void scheduleList(const std::vector<SHAVEDependencyNodePtr> &allNodes);
  SHAVESpillStatus scheduleNode(SHAVEDependencyNodePtr node, SHAVESpillStatus allowSpills = SHAVESpillStatus::Both);
  SHAVERegisterCosts getMaxCost();

  void emitSchedule();

  bool isComplete(SHAVEDependencyNodePtr node) const { return completedNodes.at(node->number); }
  size_t numCycles() const { return schedule.size(); }

#ifndef NDEBUG
  void dumpSchedule(raw_ostream& out);
#endif // NDEBUG
};

class SHAVEPreRAScheduler : public MachineFunctionPass, SHAVESchedulerBase {
private:
  //
  // private variables
  //
  enum class SchedulingMethod {
    First = 0,
    List = First,
    DepthFirst,
    BreadthFirst,
    NumMethods
  };

  MachineBasicBlock* currentBasicBlock = nullptr;
  LiveIntervals* LIS = nullptr;
  std::unique_ptr<SHAVEPreRALiveRegisterSets> liveRegisterSets = nullptr;
  typedef std::vector<SHAVERegisterCosts> NodeCosts;
  NodeCosts nodeCost;
  NodeCosts maxSubgraphCost;

  //
  // private functions
  //
  void generateNodeCosts();
  std::vector<SHAVEDependencyNodePtr> generateScheduleOrder();
  SHAVEDependencyNodePtr getSpillReliefNode(const SHAVEPreRASchedule& schedule,
                                            SHAVEDependencyNodePtr node,
                                            const std::vector<SHAVEDependencyNodePtr> &toBeScheduled,
                                            SHAVESpillStatus spillStatus) const;
  void scheduleGraphs(SHAVEPreRASchedule& schedule);
  void scheduleGraphDepthFirst(SHAVEPreRASchedule& schedule, SHAVEDependencyNodePtr node);
  bool isBetterSchedule(const std::unique_ptr<SHAVEPreRASchedule> &schedule1,
                        const std::unique_ptr<SHAVEPreRASchedule> &schedule2) const;

#ifndef NDEBUG
  //
  // overridden functions from SHAVESchedulerBase
  //
  void generateAndDumpInstrIDs(raw_ostream &out) override;
#endif // NDEBUG

public:
  //
  // public variables
  //
  static char ID;

  //
  // public functions
  //
  SHAVEPreRAScheduler() : MachineFunctionPass(ID) { isPreRA = true; }

  StringRef getPassName() const override {
    return "SHAVE Pre-RA Scheduler Pass";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool runOnMachineFunction(MachineFunction &MF) override;
};


#endif // SHAVEPRERASCHEDULER_H
