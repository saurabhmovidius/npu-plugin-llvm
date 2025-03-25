//===-- SHAVEPostRASchedule.h - Post-RA Schedule ----------------*- C++ -*-===//
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

#include "SHAVEConflicts.h"
#include "SHAVESchedulerBase.h"
#include "SHAVEInstrInfo.h"

#include <set>

namespace llvm {
  class SHAVEPostRACycle {
  public:
    std::vector<SHAVEDependencyNode const*> nodes;
    std::vector<uint64_t> ports;
    SHAVEConflictSet conflicts;
    SHAVEInstructionBundle instructions; // Modified list of instructions from nodes

    SHAVEPostRACycle(SHAVEDependencyNode const* node) : nodes(1, node), instructions(node->instructions) {}
    SHAVEPostRACycle() {}
  };

  class SHAVEPostRASchedule {
  private:
    enum LoadType {
      noLoad = 0,
      loadImmediate,
      LD128,
      LD512
    };

    typedef std::map<SHAVECycle, SHAVEPostRACycle> Schedule;
    typedef SHAVEDependencyNode const * ConstNodePtr;

    // The block that is currently being scheduled
    MachineBasicBlock *block = nullptr;
    MachineFunction *function = nullptr;
    // Schedules, one per branch position
    std::pair<Schedule, Schedule> schedules;
    // schedule of ports used at each cycle
    std::map<SHAVECycle, std::vector<uint64_t> > portSchedule;
    bool blockUsesI0Jump = false;
    // Bitset of nodes which have been scheduled already, indexed using node numbers
    std::vector<bool> completedNodes;
    // Starting cycle for incomplete nodes
    std::unordered_map<unsigned int, SHAVESchedulePosition> startingCycles;
    // Lookup for the cycle each node has been scheduled at
    std::unordered_map<unsigned int, SHAVESchedulePosition> cycles;

    const SHAVEInstrInfo* SII = nullptr;
    const SHAVERegisterInfo* SRI = nullptr;
    SHAVESchedulerBase& schedulerInfo;
    SHAVESchedulerConflicts conflicts;

    // Private functions for finding load low and load high instruction pairs
    LoadType isLoadLowInstruction(unsigned opcode) const;
    LoadType isLoadHighInstruction(unsigned opcode) const;
    bool isStoreLowInstruction(unsigned opcode) const;
    bool isStoreHighInstruction(unsigned opcode) const;
    int getLSUInstructionIndex(ConstNodePtr node) const;
    bool isLowHighLoadSequence(ConstNodePtr node,
                               MachineInstr* nodeInstruction,
                               ConstNodePtr& foundPredecessor) const;
    bool isLoadVectorElementPair(ConstNodePtr node,
                                 MachineInstr* nodeInstruction,
                                 ConstNodePtr& foundPredecessor) const;
    bool isIRF64LoadPair(ConstNodePtr node,
                         MachineInstr* nodeInstruction,
                         ConstNodePtr& foundPredecessor) const;
    bool isElementCopyPair(ConstNodePtr node,
                           ConstNodePtr& foundPredecessor) const;
    bool isMemLowHighSequence(ConstNodePtr node,
                              ConstNodePtr& foundPredecessor) const;

    // Private functions for finding the start/end of the schedule
    SHAVESchedulePosition getStartOfBlock();
    SHAVESchedulePosition getEndOfBlock();

    // Private functions for scheduling a node at a given cycle
    void markAsComplete(const ConstNodePtr& node);
    void addPorts(const std::set<uint64_t>& ports,
                  const SHAVESchedulePosition& cycle);
    void removePorts(const std::set<uint64_t>& ports,
                     const SHAVESchedulePosition& cycle);
    void scheduleNodeAtCycle(ConstNodePtr node,
                             const SHAVESchedulePosition& cycle,
                             const std::vector<SHAVEModification>& modificationActions);
    void setPortUsage(const SHAVEInstructionBundle& instructions,
                      const SHAVESchedulePosition& cycle);

    // Private functions for working with node starting cycle metadata
    SHAVESchedulePosition calculateStartingCycle(ConstNodePtr node);
    void updateSuccessorStartingCycles(ConstNodePtr node,
                                       const SHAVESchedulePosition& cycle,
                                       unsigned int latency);

    // Private functions for calculating node cycles
    unsigned int getNodeLatency(ConstNodePtr node);
    SHAVESchedulePosition getOffsetCycleForNode(ConstNodePtr node,
                                                ConstNodePtr predecessor,
                                                const SHAVESchedulePosition& cycle,
                                                unsigned int latency);
    SHAVESchedulePosition getNextCycleForNode(ConstNodePtr currentNode,
                                              const SHAVESchedulePosition& cycle,
                                              unsigned int latency);

#ifndef NDEBUG
    void dumpSchedule(raw_ostream& out);
#endif // NDEBUG

  public:
    SHAVEPostRASchedule(SHAVESchedulerBase &schedulerInfo,
                        const SHAVEInstrInfo* SII,
                        MachineBasicBlock* block);

    // Public functions used by SHAVEPostRAScheduler
    void initialiseSchedule();
    void scheduleNode(ConstNodePtr node);
    void emitSchedule();
    size_t getNumCycles() const { return schedules.first.size() + schedules.second.size(); }

    // Public functions used by SHAVEConflicts
    bool isComplete(const ConstNodePtr& node) const { return completedNodes.at(node->number); }
    bool hasCycle(const SHAVESchedulePosition& position) const;
    SHAVEPostRACycle& getCycle(const SHAVESchedulePosition& position);
    SHAVESchedulePosition getStartingCycle(const ConstNodePtr& node) const;
    bool isMemLowHighSequence(ConstNodePtr node) const;
  };

} // namespace llvm
