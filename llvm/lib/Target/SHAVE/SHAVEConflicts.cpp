//===-- SHAVEConflicts.cpp - Post-RA Conflicts and Modifications -*- C++ -*-===//
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
//===-----------------------------------------------------------------------===//

#include "SHAVEPostRASchedule.h"
#include "SHAVEConflicts.h"

#define DEBUG_TYPE "shave-postra-scheduler"

using namespace llvm;

// Generated instruction mutation mapping
#include "SHAVEGenMutations.inc"

//
// Definition of private member functions of SHAVESchedulerConflicts for finding conflicts
//

bool SHAVESchedulerConflicts::hasConflict(const SHAVEConflictPredicate& pred,
                                          const SHAVEConflictSet& set) const {
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

bool SHAVESchedulerConflicts::checkConflict(const SHAVEConflictPredicate& pred1,
                                            const SHAVEConflictPredicate& pred2,
                                            const SHAVEConflictSet& set1,
                                            const SHAVEConflictSet& set2) const {
  return (hasConflict(pred1, set1) && hasConflict(pred2, set2));
}

static bool isLoadHighInstruction(unsigned opcode) {
  if (check_isLoadImmediateHigh(opcode)) return true;
  else if (check_isLD128High(opcode)) return true;
  else if (check_isLD512High(opcode)) return true;
  else return false;
}

bool SHAVESchedulerConflicts::getLoadStoreBaseAndOffset(MachineInstr* instr,
                                                        std::pair<unsigned int, int>& baseOffset) const {
  if (SHAVEConflicts::check_isLDXorSTX(instr->getOpcode()) || SHAVEConflicts::check_isPrefetch(instr->getOpcode()))
    return false;

  if (!instr->mayLoad() && !instr->mayStore())
    return false;

  int offset = 0;
  unsigned int base = 0;
  SII->findLoadStoreOffset(instr, &offset);
  bool hasBase = SII->findLoadStoreBase(instr, &base, isLoadHighInstruction(instr->getOpcode()));

  if (!hasBase)
    return false;

  baseOffset.first = base;
  baseOffset.second = offset;
  return true;
}

bool SHAVESchedulerConflicts::findCrossLSUAliasedStoreLoadPairs(MachineInstr* instr,
                                                                const SHAVESchedulePosition &cycle,
                                                                const SHAVEConflictSet &scheduleFunctionalUnits,
                                                                SHAVEConflictSet &nodeFunctionalUnits,
                                                                bool &useMutation) const {
  MachineInstr* storeOnLSU0 = nullptr;
  MachineInstr* storeOnLSU1 = nullptr;
  bool storeOnLSU0MustUseLSU0 = false;
  bool storeOnLSU1MustUseLSU1 = false;

  const bool scheduleUsesLSU0 = scheduleFunctionalUnits.test(SHAVEConflict::usesLSU0);
  const bool scheduleUsesLSU1 = scheduleFunctionalUnits.test(SHAVEConflict::usesLSU1);
  const bool nodeUsesLSU0 = nodeFunctionalUnits.test(SHAVEConflict::usesLSU0);
  const bool nodeUsesLSU1 = nodeFunctionalUnits.test(SHAVEConflict::usesLSU1);

  SHAVEConflict LSUToModify = SHAVEConflict::usesAllFUs;

  if (nodeUsesLSU0 && !scheduleUsesLSU1 && !nodeUsesLSU1)
    LSUToModify = SHAVEConflict::usesLSU0;
  else if (nodeUsesLSU1 && !scheduleUsesLSU0 && !nodeUsesLSU0)
    LSUToModify = SHAVEConflict::usesLSU1;

  // Examine the schedules for the 'AliasedStoreDistance' number of cycles following this cycle
  SHAVESchedulePosition aliasedLoadCycle = cycle;

  // If loads or stores are set to always use a certain LSU then do not attempt to modify them
  if (instr->mayStore(/*MachineInstr::IgnoreBundle*/)) {
    SHAVE::SHAVEFuncUnit storeMustUseFU = SHAVE::NONE;

    if (instr->memoperands_empty() || !instr->hasOrderedMemoryRef()) {
      if (SHAVEOptions::LSUStorePolicy == SHAVEOptions::AlwaysUseLSU0 || SHAVEOptions::LSUStorePolicy == SHAVEOptions::AlwaysUseLSU1) {
        storeMustUseFU = (SHAVEOptions::LSUStorePolicy == SHAVEOptions::AlwaysUseLSU0) ? SHAVE::LSU0 : SHAVE::LSU1;
      }
    }
    else {
      // Volatile stores should never change LSU
      storeMustUseFU = (SHAVEOptions::LSUVolatileLoadStorePolicy == SHAVEOptions::AlwaysUseLSU0) ? SHAVE::LSU0 : SHAVE::LSU1;
    }

    int storeFUIndex = SII->getFUnitOperandIndex(instr);

    if (storeFUIndex != -1) { // FIXME: Need better handling of the PEU instructions with maystore set
      assert((storeFUIndex != -1) && "Unable to determine the LSU used for the load");

      switch (instr->getOperand(storeFUIndex).getImm()) {
      case SHAVE::LSU0:
        assert(!storeOnLSU0 && "Node appears to be adding more than one store on LSU0");
        storeOnLSU0MustUseLSU0 = (storeMustUseFU == SHAVE::LSU0);
        storeOnLSU0 = instr;
        break;
      case SHAVE::LSU1:
        assert(!storeOnLSU1 && "Node appears to be adding more than one store on LSU1");
        storeOnLSU1MustUseLSU1 = (storeMustUseFU == SHAVE::LSU1);
        storeOnLSU1 = instr;
        break;
      default:
        assert(false && "Unable to determine the LSU used for the store");
      }
    }
  }

  if ((storeOnLSU0 || storeOnLSU1) && !(scheduleUsesLSU0 && scheduleUsesLSU1) && (SHAVEOptions::AliasedStoreDistance > 0 || SHAVEOptions::NPU4LoadStoreDistance > 0)) {
    bool storeOnLSU0_aliases_LoadOnLSU0 = false;
    bool storeOnLSU0_aliases_LoadOnLSU1 = false;
    bool storeOnLSU1_aliases_LoadOnLSU0 = false;
    bool storeOnLSU1_aliases_LoadOnLSU1 = false;
    const unsigned aliasedStoreDistance = std::max(SHAVEOptions::AliasedStoreDistance, SHAVEOptions::NPU4LoadStoreDistance);

    for (unsigned range = 0; range < aliasedStoreDistance; range++) {
      aliasedLoadCycle.cycle += 1;

      if (!schedule.hasCycle(aliasedLoadCycle))
        break;

      // Look for aliased loads in the schedule
      for (MachineInstr* checkInstr : schedule.getCycle(aliasedLoadCycle).instructions) {
        if (checkInstr->mayLoad(/*MachineInstr::IgnoreBundle*/)) {
          int loadFUIndex = SII->getFUnitOperandIndex(checkInstr);

          if (loadFUIndex == -1)
            continue;

          // Compute the Load/Store alias relationships
          switch (checkInstr->getOperand(loadFUIndex).getImm()) {
          case SHAVE::LSU0:
            storeOnLSU0_aliases_LoadOnLSU0 |= (storeOnLSU0 && (schedulerInfo.compareMemoryAccesses(storeOnLSU0, checkInstr) != AliasResult::NoAlias));
            storeOnLSU1_aliases_LoadOnLSU0 |= (storeOnLSU1 && (schedulerInfo.compareMemoryAccesses(storeOnLSU1, checkInstr) != AliasResult::NoAlias));
            break;

          case SHAVE::LSU1:
            storeOnLSU0_aliases_LoadOnLSU1 |= (storeOnLSU0 && (schedulerInfo.compareMemoryAccesses(storeOnLSU0, checkInstr) != AliasResult::NoAlias));
            storeOnLSU1_aliases_LoadOnLSU1 |= (storeOnLSU1 && (schedulerInfo.compareMemoryAccesses(storeOnLSU1, checkInstr) != AliasResult::NoAlias));
            break;

          default:
            assert(false && "Unable to determine the LSU used for the load");
          }
        }
      }
    }

    bool rejectThisCycle = false;

    // No stores within 'aliasedStoreDistance' from return/call branches
    if (cycle.cycle > (SII->getDelaySlots() - (int)aliasedStoreDistance)) {
      nodeFunctionalUnits.set(usesAllFUs);
      return true;
    }

    bool isLastInsnBeforeCallOrRet = false;
    SHAVESchedulePosition branchCycle = cycle;
    branchCycle.cycle -= SII->getDelaySlots();

    for (unsigned int i = 0; i < aliasedStoreDistance; ++i, ++branchCycle.cycle) {
      if (!schedule.hasCycle(branchCycle))
        continue;

      for (ConstNodePtr checkNode : schedule.getCycle(branchCycle).nodes)
        isLastInsnBeforeCallOrRet |= (checkNode->isCall || checkNode->isReturn);
    }

    if (isLastInsnBeforeCallOrRet) {
      // FIXME: Movidius - is this still necessary?
      nodeFunctionalUnits.set(usesAllFUs);
      return true;
    }

    // Are any aliased loads present in the range of conflicting cycles?
    if (storeOnLSU0_aliases_LoadOnLSU0 || storeOnLSU0_aliases_LoadOnLSU1 || storeOnLSU1_aliases_LoadOnLSU0 || storeOnLSU1_aliases_LoadOnLSU1) {
      // Handle non-mutable conflicts between a store on LSU0 and loads on LSU1
      if (storeOnLSU0_aliases_LoadOnLSU1 && storeOnLSU0MustUseLSU0) {
        rejectThisCycle = true;
      }
      // Handle non-mutable conflicts between a store on LSU1 and loads on LSU0
      else if (storeOnLSU1_aliases_LoadOnLSU0 && storeOnLSU1MustUseLSU1)
        rejectThisCycle = true;
      // Handle mutable conflicts between a store on LSU0 and loads on LSU1
      else if (storeOnLSU0_aliases_LoadOnLSU1) {
        // Is mutation possible, and would it be successful at resolving the conflict?
        if ((SHAVEConflict::usesLSU0 == LSUToModify) && !scheduleUsesLSU1 && !nodeUsesLSU1 && !storeOnLSU0_aliases_LoadOnLSU0)
          useMutation = true;
        else
          rejectThisCycle = true;
      }
      // Handle mutable conflicts between a store on LSU1 and loads on LSU0
      else if (storeOnLSU1_aliases_LoadOnLSU0) {
        // Is mutation possible, and would it be successful at resolving the conflict?
        if ((SHAVEConflict::usesLSU1 == LSUToModify) && !scheduleUsesLSU0 && !nodeUsesLSU0 && !storeOnLSU1_aliases_LoadOnLSU1)
          useMutation = true;
        else
          rejectThisCycle = true;
      }

      // Prevent LSU mutation by handleLSUConflict in cases where there is a direct resource conflict and mutation
      // might lead to aliased store-load pairs on opposite lsus
      if ((nodeUsesLSU0 && scheduleUsesLSU0 && storeOnLSU0_aliases_LoadOnLSU0)
            || (nodeUsesLSU1 && scheduleUsesLSU1 && storeOnLSU1_aliases_LoadOnLSU1))
        rejectThisCycle = true;

      if (rejectThisCycle) {
        // Force it to skip this cycle by setting 'usesAllFUs' to prevent LSU mutation
        // FIXME: Movidius - is this still necessary?
        nodeFunctionalUnits.set(usesAllFUs);
        return true;
      }

      // FIXME: How to handle 'useMutation'?
      if (useMutation)
        return true;
    }
  }

  return false;
}

bool SHAVESchedulerConflicts::checkSchedulingConflict(const SHAVESchedulePosition& checkCycle,
                                                      const SHAVEConflictDescription &description,
                                                      const SHAVEConflictSet &conflicts,
                                                      std::vector<SHAVEModification>& modificationActions) const {
  SHAVEInstructionBundle instructions;
  SHAVEInstructionBundle removedInstructions;

  for (SHAVEModification& action : modificationActions) {
    if (action.applyToSchedule) {
      for (MachineInstr* instr : action.removeInstructions)
        removedInstructions.push_back(instr);
      for (MachineInstr* instr : action.insertInstructions)
        instructions.push_back(instr);
    }
  }

  for (MachineInstr* instr : schedule.getCycle(checkCycle).instructions)
    if (std::find(removedInstructions.begin(), removedInstructions.end(), instr) == removedInstructions.end())
      instructions.push_back(instr);

  SHAVEConflictSet checkCycleConflicts;
  for (MachineInstr* instr : instructions) {
    checkCycleConflicts |= getModifiedConflictSet(instr);
    if (instr->isInlineAsm()) {
      checkCycleConflicts.set(SHAVEConflict::usesAllFUs);
    }
  }

  if (checkConflict(description.pred1, description.pred2, checkCycleConflicts, conflicts)
      || checkConflict(description.pred1, description.pred2, conflicts, checkCycleConflicts)) {
    return true;
  }

  return false;
}

bool SHAVESchedulerConflicts::checkPortConflict(const SHAVESchedulePosition& checkCycle,
                                                const SHAVEConflictDescription &description,
                                                const std::vector<SHAVEModification>& modificationActions) const {
  SmallVector<const SHAVEModification *, 2u> inplaceModifications;
  for (auto &modification : modificationActions)
    if (modification.applyToSchedule)
      inplaceModifications.push_back(&modification);

  if (schedule.hasCycle(checkCycle)) {
    for (uint64_t port : schedule.getCycle(checkCycle).ports) {
      bool ignore = false;
      for (auto inplaceModification : inplaceModifications) {
        if (inplaceModification->cycle == checkCycle &&
            inplaceModification->ignorePorts.find(port) != inplaceModification->ignorePorts.end()) {
          ignore = true;
          break;
        }
      }

      if (!ignore && port == description.port)
        return true;

      // Check the additional ports against the scheduled ports
      for (auto inplaceModification : inplaceModifications) {
        if (inplaceModification->cycle == checkCycle) {
          for (uint64_t additionalPort : inplaceModification->additionalPorts)
            if (port == additionalPort)
              return true;
        }
      }
    }
  }

  for (auto inplaceModification : inplaceModifications)
    if (inplaceModification->cycle == checkCycle &&
        inplaceModification->additionalPorts.find(description.port) != inplaceModification->additionalPorts.end())
      return true;

  return false;
}

SHAVEConflictTypeSet SHAVESchedulerConflicts::findPortConflicts(ConstNodePtr node,
                                                                const SHAVESchedulePosition& cycle,
                                                                std::vector<SHAVEModification>& modificationActions) const {
  SHAVEConflictTypeSet result;
  SHAVEInstructionBundle instructions;
  SHAVEInstructionBundle removedInstructions;

  for (SHAVEModification& action : modificationActions) {
    if (!action.applyToSchedule) {
      for (MachineInstr* instr : action.removeInstructions)
        removedInstructions.push_back(instr);
      for (MachineInstr* instr : action.insertInstructions)
        instructions.push_back(instr);
    }
  }

  for (MachineInstr* instr : node->instructions)
    if (std::find(removedInstructions.begin(), removedInstructions.end(), instr) == removedInstructions.end())
      instructions.push_back(instr);

  for (MachineInstr* instr : instructions) {
    if (instr->getOpcode() <= SHAVE::ADJCALLSTACKUP)
      continue;

    TargetSchedModel::ProcResIter begin, end;
    SII->GetSchedPortRange(instr, begin, end);

    for (TargetSchedModel::ProcResIter p = begin; p != end; ++p) {
      uint64_t port = p->ProcResourceIdx;
      unsigned int minDelay = p->AcquireAtCycle;
      unsigned int maxDelay = p->ReleaseAtCycle - 1;

      if (instr->getOpcode() == SHAVE::BRU_SWP)
        minDelay = maxDelay = SII->getDelaySlots();

      SHAVEConflictDescription description;
      description.maxDelay = maxDelay;
      description.minDelay = minDelay;
      description.port = port;
      description.types = (1 << SHAVEConflictType::registerPort);

      result |= findConflict(cycle, description, SHAVEConflictSet(), modificationActions, true);

      if (result.any())
        break;
    }

    if (result.any())
      break;
  }

  return result;
}

SHAVEConflictTypeSet SHAVESchedulerConflicts::findConflict(const SHAVESchedulePosition& cycle,
                                                           const SHAVEConflictDescription &description,
                                                           const SHAVEConflictSet &conflicts,
                                                           std::vector<SHAVEModification>& modificationActions,
                                                           bool checkPorts) const {
  SHAVEConflictTypeSet result;

  SHAVESchedulePosition startCycle = cycle;
  startCycle.cycle += description.minDelay;

  SHAVESchedulePosition checkCycle = startCycle;
  bool conflictFound = false;

  // First check the cycles after the specified cycle for conflicts
  for (unsigned int delay = description.minDelay; delay <= description.maxDelay; ++delay) {
    if (checkPorts) {
      if (!schedule.hasCycle(checkCycle))
        break;

      if (checkPortConflict(checkCycle, description, modificationActions)) {
        result |= description.types;
        conflictFound = true;
        break;
      }
    }
    else {
      if (!schedule.hasCycle(checkCycle))
        break;

      if (checkSchedulingConflict(checkCycle, description, conflicts, modificationActions)) {
        result |= description.types;
        conflictFound = true;
        break;
      }
    }

    checkCycle.cycle += 1;
  }

  if (conflictFound)
    return result;

  // Reset the check cycles and checked blocks
  startCycle = cycle;
  startCycle.cycle -= description.minDelay;

  checkCycle = startCycle;

  // Next check the cycles before the specified cycle for conflicts
  for (int delay = description.maxDelay; delay >= (int)description.minDelay; --delay) {
    if (!schedule.hasCycle(checkCycle))
      break;

    if (!checkPorts) {
      if (checkSchedulingConflict(checkCycle, description, conflicts, modificationActions)) {
        result |= description.types;
        conflictFound = true;
        break;
      }
    }

    checkCycle.cycle -= 1;
  }

  return result;
}

SHAVEConflictSet SHAVESchedulerConflicts::getFunctionalUnits(MachineInstr* instr) const {
  SHAVEConflictSet conflictSet;
  unsigned int opcode = instr->getOpcode();

  conflictSet.set(usesAllFUs, instr->isInlineAsm());
  conflictSet.set(usesBRU, check_usesBRU(opcode));
  conflictSet.set(usesCMU, check_usesCMU(opcode));
  conflictSet.set(usesIAU, check_usesIAU(opcode));
  conflictSet.set(usesPEU, check_usesPEU(opcode));
  conflictSet.set(usesSAU, check_usesSAU(opcode));
  conflictSet.set(usesVAU, check_usesVAU(opcode));

  if (check_usesLSU0(opcode) || check_usesLSU1(opcode)) {
    const int functionalUnitOperandIndex = SII->getFUnitOperandIndex(instr);

    if (functionalUnitOperandIndex < 0)
      llvm_unreachable("LSU instruction is missing functional unit operand");

    unsigned int LSU = instr->getOperand(functionalUnitOperandIndex).getImm();

    conflictSet.set(usesLSU0, (LSU == SHAVE::LSU0));
    conflictSet.set(usesLSU1, (LSU == SHAVE::LSU1));
  }

  return conflictSet;
}

//
// Definition of private member functions of SHAVESchedulerConflicts for mutating instructions to avoid conflicts
//

SHAVEModification SHAVESchedulerConflicts::createInstruction(MachineInstr* instruction,
                                                             MutationInfo& info) {
  // 1:1 replacement of the instructions
  MachineInstrBuilder newInstrBuilder = BuildMI(*(instruction->getParent()),
                                                instruction,
                                                instruction->getDebugLoc(),
                                                SII->get(info.opcode));
  for (MachineOperand& operand : instruction->operands())
    newInstrBuilder = newInstrBuilder.add(operand);
  MachineInstr* newInstruction = newInstrBuilder.getInstr();

  // Update LSU functional unit operand if necessary
  if (info.newFU == SHAVEConflict::usesLSU0 || info.newFU == SHAVEConflict::usesLSU1) {
    unsigned int newFunctionalUnit = (info.newFU == SHAVEConflict::usesLSU0) ? SHAVE::LSU0 : SHAVE::LSU1;
    const int functionalUnitOperandIndex = SII->getFUnitOperandIndex(newInstruction);
    if (functionalUnitOperandIndex < 0)
      llvm_unreachable("LSU instruction is missing functional unit operand");
    newInstruction->getOperand(functionalUnitOperandIndex).setImm(newFunctionalUnit);
  }

  SHAVEModification newAction(false);
  newAction.removeInstructions.push_back(instruction);
  newAction.insertInstructions.push_back(newInstruction);

  return newAction;
}

SHAVEModification SHAVESchedulerConflicts::createOrCopyInstruction(MachineInstr* copyInstr,
                                                                   MutationInfo& info) {
  MachineInstrBuilder newInstr = BuildMI(*copyInstr->getParent(), copyInstr, copyInstr->getDebugLoc(), SII->get(info.opcode));

  newInstr.add(copyInstr->getOperand(0)); // Add the destination register
  newInstr.add(copyInstr->getOperand(1)); // Add the source register
  newInstr.add(copyInstr->getOperand(1));

  newInstr.copyImplicitOps(*copyInstr);

  SHAVEModification newAction(false);
  newAction.removeInstructions.push_back(copyInstr);
  newAction.insertInstructions.push_back(newInstr.getInstr());

  return newAction;
}

SHAVEModification SHAVESchedulerConflicts::createCPZIMutation(MachineInstr* cpzi,
                                                              MutationInfo& info) {
  MachineInstrBuilder newInstr = BuildMI(*cpzi->getParent(), cpzi, cpzi->getDebugLoc(), SII->get(info.opcode));
  MachineOperand& destinationReg = cpzi->getOperand(0);
  newInstr.add(destinationReg); // Add the destination register

  switch (info.opcode) {
  case SHAVE::LSU_LDIL:
    newInstr.addImm(0); // Load immediate 0
    newInstr.add(cpzi->getOperand(1)); // CC
    newInstr.add(cpzi->getOperand(2)); // CC register
    newInstr.addImm((info.newFU == SHAVEConflict::usesLSU0) ? SHAVE::LSU0 : SHAVE::LSU1); // Functional unit
    break;
  case SHAVE::IAU_XOR_32:
  case SHAVE::SAU_XOR_i32:
    newInstr.addReg(destinationReg.getReg()); // XOR the destination register
    newInstr.addReg(destinationReg.getReg()); // with itself
    break;
  default:
    llvm_unreachable("Unsupported mutation for CPZI");
  }

  newInstr.copyImplicitOps(*cpzi);

  SHAVEModification newAction(false);
  newAction.removeInstructions.push_back(cpzi);
  newAction.insertInstructions.push_back(newInstr.getInstr());

  return newAction;
}

bool SHAVESchedulerConflicts::handlePredicationConflict(ConstNodePtr node,
                                                        const SHAVESchedulePosition& cycle,
                                                        std::vector<SHAVEModification>& modificationActions) {
  MachineInstr* nodePredicateInstr = nullptr;
  MachineInstr* schedulePredicateInstr = nullptr;

  bool nodeCanBeModified = true;
  bool scheduleCanBeModified = true;

  SmallVector<MachineInstr*, 2u> nodeInstructions;
  SmallVector<MachineInstr*, 2u> removedInstructions;
  SmallVector<MachineInstr*, 2u> scheduledInstructions;
  SmallVector<MachineInstr *, 2u> removedFromSchedule;

  for (SHAVEModification& action : modificationActions) {
    if (!action.applyToSchedule) {
      for (MachineInstr* instr : action.removeInstructions)
        removedInstructions.push_back(instr);
      for (MachineInstr* instr : action.insertInstructions)
        nodeInstructions.push_back(instr);
    }
    else {
      for (MachineInstr* instr : action.removeInstructions)
        removedFromSchedule.push_back(instr);
      for (MachineInstr* instr : action.insertInstructions)
        scheduledInstructions.push_back(instr);
    }
  }

  for (MachineInstr* instr : node->instructions)
    if (std::find(removedInstructions.begin(), removedInstructions.end(), instr) == removedInstructions.end())
      nodeInstructions.push_back(instr);

  for (MachineInstr * instr : schedule.getCycle(cycle).instructions)
    if (std::find(removedFromSchedule.begin(), removedFromSchedule.end(), instr) == removedFromSchedule.end())
      scheduledInstructions.push_back(instr);

  // Check this node's instructions for predicate instructions
  for (MachineInstr* instr : nodeInstructions) {
    if (check_usesPEU(instr->getOpcode())) {
      nodePredicateInstr = instr;
      nodeCanBeModified = predicateCanBeModified(instr->getOpcode());
    }
  }

  // Check the schedule for predicate instructions
  for (MachineInstr* instr : scheduledInstructions) {
    if (check_usesPEU(instr->getOpcode())) {
      schedulePredicateInstr = instr;
      scheduleCanBeModified = predicateCanBeModified(instr->getOpcode());
    }
  }

  if ((nodePredicateInstr != nullptr && schedulePredicateInstr == nullptr && nodePredicateInstr->getOpcode() == SHAVE::PEU_ANDACC) ||
      (nodePredicateInstr == nullptr && schedulePredicateInstr != nullptr && schedulePredicateInstr->getOpcode() == SHAVE::PEU_ANDACC))
    return true;

  // If either the node or the schedule cannot be modified then exit
  if (!nodeCanBeModified || !scheduleCanBeModified)
    return false;

  if (nodePredicateInstr != nullptr && schedulePredicateInstr != nullptr) {
    // If both nodes have PEU instructions, then they must be the same predicate
    // from the same condition codes to be merged
    unsigned nodeCC = nodePredicateInstr->getOperand(0).getImm();
    unsigned scheduleCC = schedulePredicateInstr->getOperand(0).getImm();

    // If the condition codes are not the same, they can't be merged
    if (nodeCC != scheduleCC)
      return false;

    // If we make it this far, then we can safely remove the PEU instruction on the current node and schedule
    // it at the current cycle
    SHAVEModification newAction(false);
    newAction.removeInstructions.push_back(nodePredicateInstr);

    modificationActions.push_back(newAction);

    return true;
  }
  else if (nodePredicateInstr != nullptr || schedulePredicateInstr != nullptr) {
    unsigned newOpcode = 0;
    unsigned originalOpcode = 0;
    FUnitMask units = 0;
    MachineInstr* predicateInstr = nullptr;

    if (nodePredicateInstr != nullptr)
      predicateInstr = nodePredicateInstr;
    else
      predicateInstr = schedulePredicateInstr;

    originalOpcode = predicateInstr->getOpcode();

    // Find the condition code for the predicate instruction
    unsigned nodeCC = predicateInstr->getOperand(0).getImm();

    switch (originalOpcode) {
    default:
      newOpcode = originalOpcode;
      break;
    case SHAVE::PEU_PC1C:
      newOpcode = SHAVE::PEU_PCCX;
      // PEU.PCCX does not support O or UO condition codes
      if (nodeCC == SHAVECC::O || nodeCC == SHAVECC::UO)
        return false;
      break;
    }

    if (nodePredicateInstr != nullptr) {
      for (MachineInstr* instr : scheduledInstructions) {
        if (check_usesBRU(instr->getOpcode()))
          return false;
        units |= SII->GetFunctionalUnitMask(instr);
      }
    }
    else { // schedulePredicateInstr != nullptr
      for (MachineInstr* instr : nodeInstructions) {
        if (check_usesBRU(instr->getOpcode()))
          return false;
        units |= SII->GetFunctionalUnitMask(instr);
      }
    }

    units = SII->FUnitsToPCXXMask(units);
    units = ~units & 0x0000003f; // Invert the units so that they always execute

    // Update the "always enabled" functional units on an existing PEU.PCCX
    if (newOpcode == originalOpcode)
      units &= predicateInstr->getOperand(1).getImm();

    MachineBasicBlock::instr_iterator insertionPoint = predicateInstr->getIterator();

    while (insertionPoint->isBundledWithPred())
      insertionPoint--;

    MachineInstrBuilder newInstr = BuildMI(*predicateInstr->getParent(), insertionPoint, predicateInstr->getDebugLoc(), SII->get(newOpcode));
    newInstr = newInstr.addImm(nodeCC);
    newInstr = newInstr.addImm(units);
    newInstr = newInstr.copyImplicitOps(*predicateInstr);
    newInstr.getInstr()->bundleWithSucc();

    SHAVEModification newAction(predicateInstr == schedulePredicateInstr);
    newAction.removeInstructions.push_back(predicateInstr);
    newAction.insertInstructions.push_back(newInstr.getInstr());

    modificationActions.push_back(newAction);

    return true;
  }

  return false;
}

void SHAVESchedulerConflicts::handleLSUConflict(ConstNodePtr node,
                                                const SHAVESchedulePosition& cycle,
                                                std::vector<SHAVEModification>& modificationActions,
                                                SHAVEConflictSet& scheduleFunctionalUnits,
                                                SHAVEConflictSet& nodeFunctionalUnits) {
  bool cannotModifyLSU = false;
  MachineInstr* ldilZero = nullptr;

  // If loads or stores are set to always use a certain LSU then do not attempt to modify them
  for (MachineInstr* instr : node->instructions) {
    // Special case - if the instruction is marked 'mayLoad' AND 'mayStore', do not permit mutation
    if (instr->mayLoad(/*MachineInstr::IgnoreBundle*/) && instr->mayStore(/*MachineInstr::IgnoreBundle*/))
      return;

    if (instr->mayStore(/*MachineInstr::IgnoreBundle*/)) {
      if (instr->memoperands_empty() || !instr->hasOrderedMemoryRef()) {
        if (SHAVEOptions::LSUStorePolicy == SHAVEOptions::AlwaysUseLSU0 || SHAVEOptions::LSUStorePolicy == SHAVEOptions::AlwaysUseLSU1)
          cannotModifyLSU = true;
      }
      else {
        // Volatile stores should never change LSU
        cannotModifyLSU = true;
      }
    }
    else if (instr->mayLoad(/*MachineInstr::IgnoreBundle*/)) {
      if (instr->memoperands_empty() || !instr->hasOrderedMemoryRef()) {
        if (SHAVEOptions::LSULoadPolicy == SHAVEOptions::AlwaysUseLSU0 || SHAVEOptions::LSULoadPolicy == SHAVEOptions::AlwaysUseLSU1)
          cannotModifyLSU = true;
      }
      else {
        // Volatile loads should never change LSU
        cannotModifyLSU = true;
      }
    }
    else if (instr->getOpcode() == SHAVE::LSU_LDIL && instr->getOperand(1).getImm() == 0) {
      ldilZero = instr;
    }
  }

  if (cannotModifyLSU)
    return;

  const bool scheduleUsesLSU0 = scheduleFunctionalUnits.test(SHAVEConflict::usesLSU0);
  const bool scheduleUsesLSU1 = scheduleFunctionalUnits.test(SHAVEConflict::usesLSU1);
  const bool nodeUsesLSU0 = nodeFunctionalUnits.test(SHAVEConflict::usesLSU0);
  const bool nodeUsesLSU1 = nodeFunctionalUnits.test(SHAVEConflict::usesLSU1);

  SHAVEConflict LSUToModify = SHAVEConflict::usesAllFUs;

  if (nodeUsesLSU0 && !scheduleUsesLSU1 && !nodeUsesLSU1)
    LSUToModify = SHAVEConflict::usesLSU0;
  else if (nodeUsesLSU1 && !scheduleUsesLSU0 && !nodeUsesLSU0)
    LSUToModify = SHAVEConflict::usesLSU1;

  if (LSUToModify != SHAVEConflict::usesAllFUs) {
    // Two instructions use the same LSU, and no instruction uses the other LSU so mutate the node's instruction to use the other LSU
    MachineInstr* nodeInstr = nullptr;
    for (MachineInstr* instr : node->instructions)
      if (getFunctionalUnits(instr).test(LSUToModify))
        nodeInstr = instr;

    if (nodeInstr == nullptr)
      llvm_unreachable("Could not find node instruction that uses this LSU");

    MachineInstrBuilder newInstr = BuildMI(*nodeInstr->getParent(), nodeInstr, nodeInstr->getDebugLoc(), SII->get(nodeInstr->getOpcode()));

    for (MachineOperand& operand : nodeInstr->operands())
      newInstr = newInstr.add(operand);

    const int functionalUnitOperandIndex = SII->getFUnitOperandIndex(nodeInstr);
    if (functionalUnitOperandIndex < 0)
      llvm_unreachable("LSU instruction is missing functional unit operand");

    if (LSUToModify == SHAVEConflict::usesLSU0)
      newInstr.getInstr()->getOperand(functionalUnitOperandIndex).setImm(SHAVE::LSU1);
    else
      newInstr.getInstr()->getOperand(functionalUnitOperandIndex).setImm(SHAVE::LSU0);

    SHAVEModification newAction(false);
    newAction.removeInstructions.push_back(nodeInstr);
    newAction.insertInstructions.push_back(newInstr.getInstr());

    // Update any PVL instructions to the new LSU as well
    for (MachineInstr* peuInstruction : node->instructions) {
      unsigned int newPEUOpcode = 0;
      if (LSUToModify == SHAVEConflict::usesLSU0) {
        switch (peuInstruction->getOpcode()) {
        case SHAVE::PEU_PVL0_8:  newPEUOpcode = PEU_PVL1_8;  break;
        case SHAVE::PEU_PVL0_16: newPEUOpcode = PEU_PVL1_16; break;
        case SHAVE::PEU_PVL0_32: newPEUOpcode = PEU_PVL1_32; break;
        }
      }
      else {
        switch (peuInstruction->getOpcode()) {
        case SHAVE::PEU_PVL1_8:  newPEUOpcode = PEU_PVL0_8;  break;
        case SHAVE::PEU_PVL1_16: newPEUOpcode = PEU_PVL0_16; break;
        case SHAVE::PEU_PVL1_32: newPEUOpcode = PEU_PVL0_32; break;
        }
      }

      if (newPEUOpcode != 0) {
        MachineInstrBuilder newPVL = BuildMI(*nodeInstr->getParent(), nodeInstr, nodeInstr->getDebugLoc(), SII->get(newPEUOpcode));
        for (MachineOperand& operand : peuInstruction->operands())
          newPVL = newPVL.add(operand);
        newPVL->copyImplicitOps(*function, *peuInstruction);

        newAction.removeInstructions.push_back(peuInstruction);
        newAction.insertInstructions.push_back(newPVL.getInstr());
      }
    }

    modificationActions.push_back(newAction);
    nodeFunctionalUnits.reset(LSUToModify);

    if (LSUToModify == SHAVEConflict::usesLSU0)
      nodeFunctionalUnits.set(SHAVEConflict::usesLSU1);
    else
      nodeFunctionalUnits.set(SHAVEConflict::usesLSU0);
  }
  else if (ldilZero != nullptr && !schedule.isMemLowHighSequence(node)) {
    // Replace standalone "LSU.LDIL $dst 0" with "IAU/SAU.XOR $dst $dst $dst"
    // FIXME: Movidius - we could also try 'CMU.CMX.i32 $dst'?
    unsigned int opcode = 0;
    SHAVEConflict newFU = usesAllFUs;

    if (!scheduleFunctionalUnits.test(SHAVEConflict::usesIAU)) {
      opcode = SHAVE::IAU_XOR_32;
      newFU = SHAVEConflict::usesIAU;
    }
    else if (!scheduleFunctionalUnits.test(SHAVEConflict::usesSAU) && SII->hasSAU()) {
      opcode = SHAVE::SAU_XOR_i32;
      newFU = SHAVEConflict::usesSAU;
    }

    if (opcode != 0 && newFU != usesAllFUs) {
      unsigned int reg = ldilZero->getOperand(0).getReg();
      MachineInstrBuilder newInstr = BuildMI(*ldilZero->getParent(), ldilZero, ldilZero->getDebugLoc(), SII->get(opcode), reg);
      newInstr = newInstr.addReg(reg).addReg(reg);
      newInstr = newInstr.copyImplicitOps(*ldilZero);

      MachineInstr* instr = newInstr.getInstr();

      if (ldilZero->isBundledWithPred() && !instr->isBundledWithPred())
        instr->bundleWithPred();
      if (ldilZero->isBundledWithSucc())
        instr->bundleWithSucc();

      SHAVEModification newAction(false);
      newAction.removeInstructions.push_back(ldilZero);
      newAction.insertInstructions.push_back(instr);

      modificationActions.push_back(newAction);
      nodeFunctionalUnits.reset(LSUToModify);
      nodeFunctionalUnits.set(newFU);
    }
  }
}

/*
 * This function mutates an existing instruction in the provided
 * schedule to use a newly provided Opcode, iff this change does not
 * cause any conflicts.
 */
bool SHAVESchedulerConflicts::mutateInPlace(ConstNodePtr node,
                                            MutationInfo &info,
                                            std::vector<SHAVEModification>& modificationActions,
                                            MachineInstr* instr,
                                            const SHAVESchedulePosition &cycle,
                                            SHAVEConflictSet& scheduleFunctionalUnits) {
  // Erase the original instruction from the schedule
  SHAVEModification modification(true);
  modification.removeInstructions.push_back(instr);

  // Call mutator function to produce replacement instruction
  MachineInstr* newInstr = info.mutator(this, instr, info).insertInstructions[0];

  modification.insertInstructions.push_back(newInstr);

  scheduleFunctionalUnits.reset(SHAVEConflicts::usesCMU);
  scheduleFunctionalUnits.set(info.newFU);

  // Add the ports for the removed instruction to the ignore list for future port clash detection
  TargetSchedModel::ProcResIter begin, end;
  SII->GetSchedPortRange(instr, begin, end);
  for (TargetSchedModel::ProcResIter p = begin; p != end; ++p) {
    assert((p->ReleaseAtCycle - p->AcquireAtCycle) == 1 && "In-place mutations are only supported for single-cycle instructions");
    modification.ignorePorts.insert(p->ProcResourceIdx);
  }

  // Add the ports for the new instruction to the additional list for future port clash detection
  SII->GetSchedPortRange(newInstr, begin, end);
  for (TargetSchedModel::ProcResIter p = begin; p != end; ++p) {
    assert((p->ReleaseAtCycle - p->AcquireAtCycle) == 1 && "In-place mutations are only supported for single-cycle instructions");
    modification.additionalPorts.insert(p->ProcResourceIdx);
  }

  modification.cycle = cycle;

  modificationActions.push_back(modification);

  SHAVEConflictTypeSet inplaceMutationConflicts = findConflicts(node, cycle, modificationActions, true);

  if (inplaceMutationConflicts.none())
    return true;

  modificationActions.pop_back(); // Remove the modification we just added
  newInstr->eraseFromBundle(); // No longer needed

  return false;
}

SHAVEConflict SHAVESchedulerConflicts::getFunctionalUnit(unsigned int opcode) {
  if (check_usesCMU(opcode))
    return SHAVEConflict::usesCMU;
  else if (check_usesLSU0(opcode) || check_usesLSU1(opcode))
    return SHAVEConflict::usesLSU0;
  else if (check_usesIAU(opcode))
    return SHAVEConflict::usesIAU;
  else if (check_usesSAU(opcode))
    return SHAVEConflict::usesSAU;
  else if (check_usesVAU(opcode))
    return SHAVEConflict::usesVAU;
  else
    llvm_unreachable("Invalid mutation target during Post-RA Scheduling");
}

bool SHAVESchedulerConflicts::tryMutateInstruction(MachineInstr* instruction,
                                                   std::vector<SHAVEModification>& modificationActions,
                                                   SHAVEConflictSet& availableFunctionalUnits,
                                                   SHAVEConflictSet& nodeFunctionalUnits) {
  const unsigned int fromOpcode = instruction->getOpcode();
  MutationInfo mutationInfo;
  while (Mutation mutation = getMutation(fromOpcode, mutationInfo)) {
    mutationInfo = mutation.value();
    mutationInfo.newFU = getFunctionalUnit(mutationInfo.opcode);

    // If LSU0 is not available then try LSU1
    if (mutationInfo.newFU == SHAVEConflict::usesLSU0 && !availableFunctionalUnits.test(mutationInfo.newFU))
      mutationInfo.newFU = SHAVEConflict::usesLSU1;

    if (availableFunctionalUnits.test(mutationInfo.newFU)) {
      SHAVEModification newAction = mutationInfo.mutator(this, instruction, mutationInfo);

      modificationActions.push_back(newAction);
      nodeFunctionalUnits.reset(getFunctionalUnit(fromOpcode));
      nodeFunctionalUnits.set(mutationInfo.newFU);

      return true;
    }
  }

  return false;
}

/*
 * This function attempts to mutate the already scheduled CMU instructions in order
 * to free up the slot for a new incoming CMU instruction
 */
void SHAVESchedulerConflicts::mutateScheduledInstruction(ConstNodePtr node,
                                                         const SHAVESchedulePosition& cycle,
                                                         std::vector<SHAVEModification>& modificationActions,
                                                         SHAVEConflictSet& scheduleFunctionalUnits,
                                                         SHAVEConflictSet& availableFunctionalUnits) {
  // LSU.SWZC8 modifies the input to the CMU instruction in the same bundle, which means we can't
  // safely mutate the CMU instruction
  for (MachineInstr* instr : schedule.getCycle(cycle).instructions) {
    if (instr->getOpcode() == SHAVE::LSU_SWZC8 ||
        instr->getOpcode() == SHAVE::PEU_PCCX) {
      return;
    }
  }

  SHAVEConflictSet nodeFunctionalUnits;
  for (MachineInstr* instr : node->instructions)
    nodeFunctionalUnits |= getFunctionalUnits(instr);

  // check if any of the already scheduled instruction may be mutated (and should be mutated)
  for (MachineInstr* instr : schedule.getCycle(cycle).instructions) {
    auto overlap = nodeFunctionalUnits & getFunctionalUnits(instr);
    if (overlap.none())
      continue;

    unsigned int fromOpcode = instr->getOpcode();

    MutationInfo mutationInfo;
    while (Mutation mutation = getMutation(fromOpcode, mutationInfo)) {
      mutationInfo = mutation.value();
      mutationInfo.newFU = getFunctionalUnit(mutationInfo.opcode);

      if (availableFunctionalUnits.test(mutationInfo.newFU) &&
          mutateInPlace(node, mutationInfo, modificationActions, instr, cycle, scheduleFunctionalUnits)) {
        DEBUG(dbgs() << "** In place mutation attempted, CMU -> " << functionalUnit << " at cycle " << cycle.cycle << "\n");
        return;
      }

      // Special-case to try both LSUs
      if (mutationInfo.newFU == SHAVEConflict::usesLSU0 &&
          availableFunctionalUnits.test(SHAVEConflict::usesLSU1)) {
        mutationInfo.newFU = SHAVEConflict::usesLSU1;
        if (mutateInPlace(node, mutationInfo, modificationActions, instr, cycle, scheduleFunctionalUnits)) {
          DEBUG(dbgs() << "** In place mutation attempted, CMU -> " << functionalUnit << " at cycle " << cycle.cycle << "\n");
          return;
        }
      }
    }
  }
}

void SHAVESchedulerConflicts::handleCMUConflict(ConstNodePtr node,
                                                const SHAVESchedulePosition& cycle,
                                                std::vector<SHAVEModification>& modificationActions,
                                                SHAVEConflictSet& scheduleFunctionalUnits,
                                                SHAVEConflictSet& nodeFunctionalUnits) {
  SHAVEConflictSet availableFunctionalUnits = ~(nodeFunctionalUnits | scheduleFunctionalUnits);
  // LSU.SWZC8 modifies the input to the CMU instruction in the same bundle, which means we can't
  // safely mutate the CMU instruction
  for (MachineInstr* instr : node->instructions)
    if (instr->getOpcode() == SHAVE::LSU_SWZC8)
      return;

  for (MachineInstr* instruction : node->instructions) {
    unsigned int fromOpcode = instruction->getOpcode();
    if (!check_usesCMU(fromOpcode))
      continue;

    if (tryMutateInstruction(instruction, modificationActions, availableFunctionalUnits, nodeFunctionalUnits))
      return;
  }

  // If the incoming CMU instruction cannot be mutated, try the one already scheduled at the given cycle
  if (SHAVEOptions::EnableInPlaceMutation)
    mutateScheduledInstruction(node, cycle, modificationActions, scheduleFunctionalUnits, availableFunctionalUnits);
}

void SHAVESchedulerConflicts::handleIAUConflict(ConstNodePtr node,
                                                const SHAVESchedulePosition& cycle,
                                                std::vector<SHAVEModification>& modificationActions,
                                                SHAVEConflictSet& scheduleFunctionalUnits,
                                                SHAVEConflictSet& nodeFunctionalUnits) {
  SHAVEConflictSet availableFunctionalUnits = ~(nodeFunctionalUnits | scheduleFunctionalUnits);

  // First try the mutations specified in SHAVEInstrInfo_Mutating.td
  for (MachineInstr* instruction : node->instructions) {
    unsigned int fromOpcode = instruction->getOpcode();
    if (!check_usesIAU(fromOpcode))
      continue;

    if (tryMutateInstruction(instruction, modificationActions, availableFunctionalUnits, nodeFunctionalUnits))
      return;
  }

  // If the SAU isn't available then we can't mutate this IAU instruction
  if (!availableFunctionalUnits.test(usesSAU) || !SII->hasSAU())
    return;

  MachineInstr* originalInstr = nullptr;

  for (MachineInstr* instr : node->instructions)
    if (check_usesIAU(instr->getOpcode()))
      originalInstr = instr;

  if (originalInstr == nullptr)
    llvm_unreachable("IAU instruction should not be null");

  unsigned int newOpcode = 0;
  bool hasImmediateOperand = false;
  bool hasZeroLatency = false;
  int immediateMin = 0;
  int immediateMax = 0;

  switch (originalInstr->getOpcode()) {
  case SHAVE::IAU_ADD_32:
    newOpcode = SHAVE::SAU_ADD_i32;
    break;
  case SHAVE::IAU_ADD_32_imm:
    newOpcode = SHAVE::SAU_ADD_i32_imm;
    hasImmediateOperand = true;
    immediateMin = 0;
    immediateMax = 31;
    break;
  case SHAVE::IAU_SUB_32:
    newOpcode = SHAVE::SAU_SUB_i32;
    break;
  case SHAVE::IAU_SUB_32_imm:
    newOpcode = SHAVE::SAU_SUB_i32_imm;
    hasImmediateOperand = true;
    immediateMin = 0;
    immediateMax = 31;
    break;
  case SHAVE::IAU_LMULL:
  case SHAVE::IAU_MUL_32:
    newOpcode = SHAVE::SAU_MUL_i32;
    break;
  case SHAVE::IAU_MUL_32_imm:
    newOpcode = SHAVE::SAU_MUL_i32_imm;
    hasImmediateOperand = true;
    immediateMin = -16;
    immediateMax = 15;
    break;
  case SHAVE::IAU_ABS_32:
    newOpcode = SHAVE::SAU_ABS_i32;
    hasZeroLatency = true;
    break;
  case SHAVE::IAU_OR_32:
    newOpcode = SHAVE::SAU_OR_i32;
    hasZeroLatency = true;
    break;
  case SHAVE::IAU_XOR_32:
    newOpcode = SHAVE::SAU_XOR_i32;
    hasZeroLatency = true;
    break;
  case SHAVE::IAU_AND_32:
    newOpcode = SHAVE::SAU_AND_i32;
    hasZeroLatency = true;
    break;
  case SHAVE::IAU_SHL_i32:
    newOpcode = SHAVE::SAU_SHL_x32;
    break;
  case SHAVE::IAU_SHL_32_imm:
    newOpcode = SHAVE::SAU_SHL_x32_imm;
    hasImmediateOperand = true;
    immediateMin = 0;
    immediateMax = 31;
    break;
  case SHAVE::IAU_SHR_i32:
    newOpcode = SHAVE::SAU_SHR_i32;
    break;
  case SHAVE::IAU_SHR_i32_imm:
    newOpcode = SHAVE::SAU_SHR_i32_imm;
    hasImmediateOperand = true;
    immediateMin = 0;
    immediateMax = 31;
    break;
  case SHAVE::IAU_SHR_u32:
    newOpcode = SHAVE::SAU_SHR_u32;
    break;
  case SHAVE::IAU_SHR_u32_imm:
    newOpcode = SHAVE::SAU_SHR_u32_imm;
    hasImmediateOperand = true;
    immediateMin = 0;
    immediateMax = 31;
    break;
  case SHAVE::IAU_ROL:
    newOpcode = SHAVE::SAU_ROL_x32;
    break;
  case SHAVE::IAU_ROL_imm:
    newOpcode = SHAVE::SAU_ROL_x32_imm;
    immediateMin = 0;
    immediateMax = 31;
    break;
  }

  // We can't mutate instructions not in the above list
  if (newOpcode == 0)
    return;

  // The SAU instructions have a smaller immediate range than their IAU counterparts
  if (hasImmediateOperand) {
    int immediate = originalInstr->getOperand(2).getImm();
    if (immediate < immediateMin || immediate > immediateMax)
      return;
  }

  // The latency of some of the SAU instructions we are mutating to is 1 cycle higher than the original instructions.
  // Because of this, we need to ensure the current cycle has moved past the starting cycle by at least one cycle
  if (!hasZeroLatency && cycle == schedule.getStartingCycle(node))
    return;

  // If this instruction has a non-dead def of either I_STATE or CC_IAU0 then we cannot mutate this instruction.
  // This is to account for cases like 64-bit integer addition/subtraction where the next instruction uses the carry-bit
  // set by this instruction
  for (MachineOperand& operand : originalInstr->operands())
    if (operand.isReg() && operand.isDef() && !operand.isDead() && (operand.getReg() == SHAVE::I_STATE || operand.getReg() == SHAVE::CC_IAU0))
      return;

  // Build the new instruction
  MachineInstrBuilder newInstr = BuildMI(*originalInstr->getParent(), originalInstr, originalInstr->getDebugLoc(), SII->get(newOpcode));

  for (MachineOperand& operand : originalInstr->operands()) {
    // Don't add I_STATE or CC_IAU0 to SAU instructions
    if (operand.isReg() && (operand.getReg() == SHAVE::I_STATE || operand.getReg() == SHAVE::CC_IAU0))
      continue;

    newInstr = newInstr.add(operand);
  }

  SHAVEModification newAction(false);
  newAction.removeInstructions.push_back(originalInstr);
  newAction.insertInstructions.push_back(newInstr.getInstr());

  modificationActions.push_back(newAction);
  nodeFunctionalUnits.reset(SHAVEConflict::usesIAU);
  nodeFunctionalUnits.set(SHAVEConflict::usesSAU);
}

bool SHAVESchedulerConflicts::handleFunctionalUnitConflict(ConstNodePtr node,
                                                           const SHAVESchedulePosition& cycle,
                                                           std::vector<SHAVEModification>& modificationActions) {
  SHAVEConflictSet scheduleFunctionalUnits;
  for (MachineInstr* instr : schedule.getCycle(cycle).instructions)
    scheduleFunctionalUnits |= getFunctionalUnits(instr);

  SHAVEConflictSet nodeFunctionalUnits;
  for (MachineInstr* instr : node->instructions)
    nodeFunctionalUnits |= getFunctionalUnits(instr);

  // Set the PEU bits to 0, PEU conflicts are handled separately
  nodeFunctionalUnits.reset(SHAVEConflict::usesPEU);
  scheduleFunctionalUnits.reset(SHAVEConflict::usesPEU);

  // SHAVEConflict::usesAllFUs is set on inline asm blocks which we should never try to mutate
  if (scheduleFunctionalUnits.test(SHAVEConflict::usesAllFUs) || nodeFunctionalUnits.test(SHAVEConflict::usesAllFUs))
    return false;

  //
  // Handle LSU0/1 conflicts
  //
  if (SHAVEOptions::HasLSU1)
    handleLSUConflict(node, cycle, modificationActions, scheduleFunctionalUnits, nodeFunctionalUnits);

  // #ifdef ALIASEDSTOREDISTANCE
  // FIXME saurabh: not needed any more, now that the check has been moved to findConflicts
  // 'SHAVEConflict::usesAllFUs' is also set when an aliased Store on one LSU is followed by a
  // corresponding aliased Load on the other within the range specified by 'AliasedStoreDistance'
  if (nodeFunctionalUnits.test(SHAVEConflict::usesAllFUs)) {
    nodeFunctionalUnits.reset(SHAVEConflict::usesAllFUs);
    return false;
  }
  // #endif // ALIASEDSTOREDISTANCE

  //
  // Handle CMU conflicts
  //
  SHAVEConflictSet conflictingFunctionalUnits = nodeFunctionalUnits & scheduleFunctionalUnits;

  if (conflictingFunctionalUnits.test(usesCMU))
    handleCMUConflict(node, cycle, modificationActions, scheduleFunctionalUnits, nodeFunctionalUnits);

  if (conflictingFunctionalUnits.test(usesIAU))
    handleIAUConflict(node, cycle, modificationActions, scheduleFunctionalUnits, nodeFunctionalUnits);

  return ((nodeFunctionalUnits & scheduleFunctionalUnits) == 0);
}

//
// Definition of public member functions of SHAVESchedulerConflicts
//


SHAVESchedulerConflicts::SHAVESchedulerConflicts(SHAVEPostRASchedule& schedule,
                                                 SHAVESchedulerBase& schedulerInfo,
                                                 const SHAVEInstrInfo* SII,
                                                 MachineFunction* function)
  : schedule(schedule), schedulerInfo(schedulerInfo), SII(SII), function(function) {
  targetUsesConflicts = !SII->hasFeature(SHAVE::HasSharedResources_Feature);

  initConflictTable();
}


SHAVEConflictSet SHAVESchedulerConflicts::getModifiedConflictSet(MachineInstr* instr) const {
  SHAVEConflictSet conflicts = getConflictSet(instr->getOpcode());

  if (conflicts.test(usesLSU0) || conflicts.test(usesLSU1)) {
    const int functionalUnitOperandIndex = SII->getFUnitOperandIndex(instr);

    if (functionalUnitOperandIndex < 0) {
      DEBUG(instr->dump());
      llvm_unreachable("LSU instruction is missing functional unit operand");
    }

    unsigned int LSU = instr->getOperand(functionalUnitOperandIndex).getImm();

    conflicts.set(usesLSU0, (LSU == SHAVE::LSU0));
    conflicts.set(usesLSU1, (LSU == SHAVE::LSU1));
  }

  return conflicts;
}


SHAVEConflictTypeSet SHAVESchedulerConflicts::findConflicts(ConstNodePtr node,
                                                            const SHAVESchedulePosition& cycle,
                                                            std::vector<SHAVEModification>& modificationActions,
                                                            bool ignorePredication) const {
  SHAVEConflictTypeSet result;
  SHAVEConflictSet conflicts;

  // Check for port conflicts first
  result = findPortConflicts(node, cycle, modificationActions);

  SHAVEConflictSet scheduleFunctionalUnits;
  bool scheduleIsLanePredicating = false;
  if (schedule.hasCycle(cycle)) {
    for (MachineInstr* instr : schedule.getCycle(cycle).instructions) {
      scheduleFunctionalUnits |= getFunctionalUnits(instr);
      scheduleIsLanePredicating |= check_isWritebackPredicating(instr->getOpcode());
    }
  }

  SmallVector<MachineInstr*, 4u> instructions;
  SmallVector<MachineInstr*, 4u> removedInstructions;

  for (SHAVEModification& action : modificationActions) {
    if (!action.applyToSchedule) {
      for (MachineInstr* instr : action.removeInstructions)
        removedInstructions.push_back(instr);
      for (MachineInstr* instr : action.insertInstructions)
        instructions.push_back(instr);
    }
    else {
      for (MachineInstr* instr : action.removeInstructions)
        scheduleFunctionalUnits ^= getFunctionalUnits(instr);
      for (MachineInstr* instr : action.insertInstructions) {
        SHAVEConflictSet addedFUs = getFunctionalUnits(instr);
        if ((scheduleFunctionalUnits & addedFUs).any())
          result.set(SHAVEConflictType::functionalUnit);
        scheduleFunctionalUnits |= getFunctionalUnits(instr);
      }
    }
  }

  for (MachineInstr* instr : node->instructions)
    if (std::find(removedInstructions.begin(), removedInstructions.end(), instr) == removedInstructions.end())
      instructions.push_back(instr);

  for (MachineInstr* instr : instructions) {
    SHAVEConflictSet nodeFunctionalUnits = getFunctionalUnits(instr);

    if ((nodeFunctionalUnits & scheduleFunctionalUnits).any())
      result.set(SHAVEConflictType::functionalUnit);

    // It isn't possible for the conflict handler to introduce a predication conflict,
    // it can however remove one. After a predication conflict has been removed, we
    // should ignore such conflicts again (when ignorePredication is set to true)
    bool nodeHasPredicate = nodeFunctionalUnits.test(SHAVEConflict::usesPEU) && !check_isWritebackPredicating(instr->getOpcode());
    bool scheduleHasPredicate = scheduleFunctionalUnits.test(SHAVEConflict::usesPEU) && !scheduleIsLanePredicating;
    if (!ignorePredication)
      if ((nodeHasPredicate && scheduleFunctionalUnits.any()) || (scheduleHasPredicate && nodeFunctionalUnits.any()))
        result.set(SHAVEConflictType::predication);
  }

  if (result.any())
    return result;

  // Get the conflict set for this node. The conflict set for a node is the conflict
  // sets for each individual instruction in the node combined
  for (MachineInstr* instr : instructions) {
    conflicts |= getModifiedConflictSet(instr);
    if (instr->isInlineAsm()) {
      conflicts.set(SHAVEConflict::usesAllFUs);
      // Inline asm block cannot be scheduled in delay slots
      if (cycle.cycle > 0)
        result.set(SHAVEConflictType::invalidCycle);
    }

    SHAVEConflictSet nodeFunctionalUnits = getFunctionalUnits(instr);
    bool useMutation = false;

    if (findCrossLSUAliasedStoreLoadPairs(instr, cycle, scheduleFunctionalUnits, nodeFunctionalUnits, useMutation)) {
      // FIXME: Movidius - if 'useMutation' is true, then we should ensure that the mutation takes place
      if (!useMutation)
        conflicts.set(SHAVEConflict::usesAllFUs); // FIXME: Movidius - Do we need this? I don't think that this is quite right
      result.set(SHAVEConflictType::functionalUnit);
    }
  }

  if (result.any())
    return result;

  // Check for tile clashes between LSU memory accessing instructions
  for (MachineInstr* instr : instructions) {
    std::pair<unsigned int, int> baseOffset;
    bool hasBaseOffset = getLoadStoreBaseAndOffset(instr, baseOffset);

    if (!hasBaseOffset)
      continue;

    std::pair<unsigned int, int> scheduleBaseOffset;
    bool scheduleHasBaseOffset = false;
    if (schedule.hasCycle(cycle)) {
      for (MachineInstr* scheduleInstr : schedule.getCycle(cycle).instructions)
        scheduleHasBaseOffset |= getLoadStoreBaseAndOffset(scheduleInstr, scheduleBaseOffset);
    }

    if (!scheduleHasBaseOffset)
      break;

    // FIXME: Movidius - This assumes that the base pointer is 8-byte aligned
    if (baseOffset.first == scheduleBaseOffset.first) {
      const unsigned int tileSize = SII->getCMXCutSize();
      const unsigned int numberOfTiles = SII->getCMXNumberOfCuts();

      const unsigned int tile = (baseOffset.second / tileSize) % numberOfTiles;
      const unsigned int scheduleTile = (scheduleBaseOffset.second / tileSize) % numberOfTiles;

      if (tile == scheduleTile)
        result.set(SHAVEConflictType::tileClash);
      break;
    }
  }

  if (result.any())
    return result;

  // Only targets pre-NPU4 use the conflict table
  if (targetUsesConflicts) {
    for (unsigned int i = 0; i < SHAVEConflicts::conflictTableSize; ++i) {
      SHAVEConflictDescription description = SHAVEConflicts::conflictTable[i];
      result |= findConflict(cycle, description, conflicts, modificationActions, false);

      if (result.any())
        break;
    }
  }

  return result;
}

bool SHAVESchedulerConflicts::predicateCanBeModified(unsigned int opcode) const {
  switch (opcode) {
  default:
    return false;
  case SHAVE::PEU_PC1C:
  case SHAVE::PEU_PCCX:
    return true;
  }
}

void SHAVESchedulerConflicts::handleConflicts(ConstNodePtr node,
                                              const SHAVESchedulePosition& cycle,
                                              SHAVEConflictTypeSet& conflictsSet,
                                              std::vector<SHAVEModification>& modificationActions) {
  if (conflictsSet.test(SHAVEConflictType::functionalUnit) || conflictsSet.test(SHAVEConflictType::registerPort)) {
    bool conflictRemoved = handleFunctionalUnitConflict(node, cycle, modificationActions);
    if (conflictRemoved)
      conflictsSet.set(SHAVEConflictType::functionalUnit, false);
  }

  if (conflictsSet.test(SHAVEConflictType::predication)) {
    bool conflictRemoved = handlePredicationConflict(node, cycle, modificationActions);
    if (conflictRemoved)
      conflictsSet.set(SHAVEConflictType::predication, false);
  }

  // Modifications made may have removed or added new register port conflicts so we need to recompute the set
  SHAVEConflictTypeSet portConflicts = findPortConflicts(node, cycle, modificationActions);
  if (portConflicts.test(SHAVEConflictType::registerPort))
    conflictsSet.set(SHAVEConflictType::registerPort);
  else
    conflictsSet.reset(SHAVEConflictType::registerPort);
}

void SHAVESchedulerConflicts::clearModificationActions(std::vector<SHAVEModification>& modificationActions) {
  for (SHAVEModification& action : modificationActions)
    for (MachineInstr* instr : action.insertInstructions)
      instr->eraseFromBundle();

  modificationActions.clear();
}
