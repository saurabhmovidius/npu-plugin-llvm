//===-- SHAVEConflicts.h - Post-RA Conflicts and Modifications --*- C++ -*-===//
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

#include "SHAVESchedulerBase.h"
#include "SHAVEInstrInfo.h"

#include <optional>
#include <unordered_map>

using namespace llvm;
using namespace SHAVEConflicts;

namespace llvm {
  class SHAVEPostRASchedule; // Forward declaration, SHAVEPostRASchedule and SHAVESchedulerConflicts reference each other

  class SHAVEModification {
  public:
    bool applyToSchedule = false;
    SHAVEInstructionBundle insertInstructions;
    SHAVEInstructionBundle removeInstructions;

    // Cycle for in-place mutations (applyToSchedule = true) only, so the port clash detection knows which cycle to apply to
    SHAVESchedulePosition cycle;
    std::set<uint64_t> ignorePorts;
    std::set<uint64_t> additionalPorts;

    SHAVEModification(bool applyToSchedule) : applyToSchedule(applyToSchedule) {}
  };

  class SHAVESchedulerConflicts {
  private:
    typedef SHAVEDependencyNode const* ConstNodePtr;
    struct MutationInfo; // forward declaration for Mutator
    typedef std::function<SHAVEModification(SHAVESchedulerConflicts *, MachineInstr*, MutationInfo&)> Mutator;

    struct MutationInfo {
      unsigned int opcode = 0;
      Mutator mutator = {};
      int position = -1;
      SHAVEConflict newFU = SHAVEConflict::usesAllFUs;
      MutationInfo() {}
      MutationInfo(unsigned int opcode, int position)
        : opcode(opcode), position(position) {}
      MutationInfo(unsigned int opcode, Mutator mutator, int position)
        : opcode(opcode), mutator(mutator), position(position) {}
    };
    typedef std::optional<MutationInfo> Mutation;

    SHAVEPostRASchedule &schedule;
    SHAVESchedulerBase& schedulerInfo;
    const SHAVEInstrInfo *SII = nullptr;
    bool targetUsesConflicts = false;
    MachineFunction* function = nullptr;

    // Private functions for finding conflicts
    bool hasConflict(const SHAVEConflictPredicate& pred,
                     const SHAVEConflictSet& set) const;
    bool checkConflict(const SHAVEConflictPredicate& pred1,
                       const SHAVEConflictPredicate& pred2,
                       const SHAVEConflictSet& set1,
                       const SHAVEConflictSet& set2) const;
    bool getLoadStoreBaseAndOffset(MachineInstr* instr,
                                   std::pair<unsigned int, int>& baseOffset) const;
    bool findCrossLSUAliasedStoreLoadPairs(MachineInstr* instr,
                                           const SHAVESchedulePosition& cycle,
                                           const SHAVEConflictSet& scheduleFunctionalUnits,
                                           SHAVEConflictSet& nodeFunctionalUnits,
                                           bool& useMutation) const;
    bool checkSchedulingConflict(const SHAVESchedulePosition& cycle,
                                 const SHAVEConflictDescription &description,
                                 const SHAVEConflictSet &conflicts,
                                 std::vector<SHAVEModification>& modificationActions) const;
    bool checkPortConflict(const SHAVESchedulePosition& cycle,
                           const SHAVEConflictDescription &description,
                           const std::vector<SHAVEModification>& modificationActions) const;
    SHAVEConflictTypeSet findPortConflicts(ConstNodePtr node,
                                           const SHAVESchedulePosition& cycle,
                                           std::vector<SHAVEModification>& modificationActions) const;
    SHAVEConflictTypeSet findConflict(const SHAVESchedulePosition& cycle,
                                      const SHAVEConflictDescription &description,
                                      const SHAVEConflictSet &conflicts,
                                      std::vector<SHAVEModification>& modificationActions,
                                      bool checkPorts) const;
    SHAVEConflictSet getFunctionalUnits(MachineInstr* instr) const;

    // Private static function for extracting the functional unit from a mutation target instruction opcode
    static SHAVEConflict getFunctionalUnit(unsigned int opcode);

    // Private function for matching opcodes to their equivalents on another functional unit
    // The implementation of this function is generated by Tablegen from the contents of SHAVEInstrInfo_Mutating.td
    Mutation getMutation(unsigned int opcode, const MutationInfo &previous) const;

    // Private functions for mutating instructions to avoid conflicts
    SHAVEModification createInstruction(MachineInstr* copyInstr,
                                        MutationInfo& info);
    SHAVEModification createOrCopyInstruction(MachineInstr* copyInstr,
                                              MutationInfo& info);
    SHAVEModification createCPZIMutation(MachineInstr* cpzi,
                                         MutationInfo& info);
    bool handlePredicationConflict(ConstNodePtr node,
                                   const SHAVESchedulePosition& cycle,
                                   std::vector<SHAVEModification>& modificationActions);
    void handleLSUConflict(ConstNodePtr node,
                           const SHAVESchedulePosition& cycle,
                           std::vector<SHAVEModification>& modificationActions,
                           SHAVEConflictSet& scheduleFunctionalUnits,
                           SHAVEConflictSet& nodeFunctionalUnits);
    bool mutateInPlace(ConstNodePtr node,
                       MutationInfo &info,
                       std::vector<SHAVEModification>& modificationActions,
                       MachineInstr* instr,
                       const SHAVESchedulePosition& cycle,
                       SHAVEConflictSet& scheduleFunctionalUnits);
    bool tryMutateInstruction(MachineInstr* instruction,
                              std::vector<SHAVEModification>& modificationActions,
                              SHAVEConflictSet& availableFunctionalUnits,
                              SHAVEConflictSet& nodeFunctionalUnits);
    void mutateScheduledInstruction(ConstNodePtr node,
                                    const SHAVESchedulePosition& cycle,
                                    std::vector<SHAVEModification>& modificationActions,
                                    SHAVEConflictSet& scheduleFunctionalUnits,
                                    SHAVEConflictSet& nodeFunctionalUnits);
    void handleCMUConflict(ConstNodePtr node,
                           const SHAVESchedulePosition& cycle,
                           std::vector<SHAVEModification>& modificationActions,
                           SHAVEConflictSet& scheduleFunctionalUnits,
                           SHAVEConflictSet& nodeFunctionalUnits);
    void handleIAUConflict(ConstNodePtr node,
                           const SHAVESchedulePosition& cycle,
                           std::vector<SHAVEModification>& modificationActions,
                           SHAVEConflictSet& scheduleFunctionalUnits,
                           SHAVEConflictSet& nodeFunctionalUnits);
    bool handleFunctionalUnitConflict(ConstNodePtr node,
                                      const SHAVESchedulePosition& cycle,
                                      std::vector<SHAVEModification>& modificationActions);

  public:
    SHAVESchedulerConflicts(SHAVEPostRASchedule& schedule,
                            SHAVESchedulerBase& schedulerInfo,
                            const SHAVEInstrInfo* SII,
                            MachineFunction* function);

    // Public functions used by SHAVEPostRASchedule for detecting conflicts
    SHAVEConflictSet getModifiedConflictSet(MachineInstr* instr) const;
    SHAVEConflictTypeSet findConflicts(ConstNodePtr node,
                                       const SHAVESchedulePosition& cycle,
                                       std::vector<SHAVEModification>& modificationActions,
                                       bool ignorePredication = false) const;
    bool predicateCanBeModified(unsigned int opcode) const;

    // Public functions used by SHAVEPostRASchedule for modifying detected conflicts
    void handleConflicts(ConstNodePtr node,
                         const SHAVESchedulePosition& cycle,
                         SHAVEConflictTypeSet& conflicts,
                         std::vector<SHAVEModification>& modificationActions);
    void clearModificationActions(std::vector<SHAVEModification>& modificationActions);
  };

} // namespace llvm
