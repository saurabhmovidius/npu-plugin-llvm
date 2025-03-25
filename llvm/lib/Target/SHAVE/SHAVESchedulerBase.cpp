//===-- SHAVESchedulerBase.cpp - Scheduler Pass Base ------------*- C++ -*-===//
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

#define DEBUG_TYPE "shave-scheduler-base"

#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineBranchProbabilityInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"

#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVE.h"
#include "SHAVESchedulerBase.h"

using namespace llvm;

//
// Definition of private member functions of SHAVESchedulerBase
//

#ifndef NDEBUG
void SHAVEDependencyNode::dump() {
  for (MachineInstr *instr : instructions)
    instr->dump();
}
#endif // NDEBUG

static bool isBarrier(MachineInstr* instr) {
  // We must treat function calls as memory barriers, so all loads and stores are dependent on nearby calls
  if (instr->isCall(MachineInstr::IgnoreBundle))
    return true;

  // Don't consider branches to be barriers so we can build anti-dependent edges between them and other
  // instructions. This is what enables branch delay slot filling in the post-RA scheduler
  if (instr->isBarrier(MachineInstr::IgnoreBundle) && !instr->isBranch(MachineInstr::IgnoreBundle))
    return true;

  // Ensure all instructions have completed execution before entering an inline asm block
  if (instr->isInlineAsm())
    return true;

  if (instr->getOpcode() == SHAVE::ADJCALLSTACKDOWN || instr->getOpcode() == SHAVE::ADJCALLSTACKUP)
    return true;

  if (instr->getOpcode() == SHAVE::CMU_CPTI || instr->getOpcode() == SHAVE::CMU_CPTI)
    return true;

  return false;
}

void SHAVESchedulerBase::assignNodeWeights() {
  std::unordered_map<SHAVEDependencyNodePtr, std::vector<uint64_t> > weightNodes;
  std::unordered_map<SHAVEDependencyNodePtr, std::vector<uint64_t> > antiDepWeightNodes;
  std::unordered_map<SHAVEDependencyNodePtr, unsigned int> predecessorsComplete;

  auto getNodeSet = [&](SHAVEDependencyNodePtr node, std::unordered_map<SHAVEDependencyNodePtr, std::vector<uint64_t> >& nodes) {
    auto it = nodes.find(node);
    if (it != nodes.end())
      return it;
    auto inserted = nodes.insert(std::make_pair(node, std::vector<uint64_t>(allGeneratedNodes.size() / 64 + 1, 0ull)));
    return inserted.first; // inserted.iterator->value
    };

  auto setBit = [](std::vector<uint64_t>& nodes, unsigned int number) {
    nodes[number / 64] |= 1ull << (number % 64ull);
    };

  auto getBit = [](std::vector<uint64_t>& nodes, unsigned int number) {
    return nodes[number / 64] & (1ull << (number % 64ull));
    };

  for (SHAVEDependencyNodePtr node : allGeneratedNodes) {
    auto& weightNodeSet = getNodeSet(node, weightNodes)->second;
    auto& antiDepNodeWeights = getNodeSet(node, antiDepWeightNodes)->second;

    for (SHAVEDependencyNodePtr successor : node->outputSuccessors) {
      auto& antiDepSuccessorWeightNodeSet = getNodeSet(successor, antiDepWeightNodes)->second;

      setBit(weightNodeSet, successor->number);

      for (unsigned int i = 0; i < weightNodeSet.size(); ++i)
        weightNodeSet[i] = weightNodeSet[i] | antiDepSuccessorWeightNodeSet[i];
    }

    for (SHAVEDependencyNodePtr successor : node->successors) {
      auto& successorWeightNodeSet = getNodeSet(successor, weightNodes)->second;
      auto& antiDepSuccessorWeightNodeSet = getNodeSet(successor, antiDepWeightNodes)->second;

      for (unsigned int i = 0; i < weightNodeSet.size(); ++i)
        weightNodeSet[i] = weightNodeSet[i] | successorWeightNodeSet[i];

      setBit(antiDepNodeWeights, successor->number);

      for (unsigned int i = 0; i < weightNodeSet.size(); ++i)
        antiDepNodeWeights[i] = antiDepNodeWeights[i] | antiDepSuccessorWeightNodeSet[i];


      if (predecessorsComplete.find(successor) == predecessorsComplete.end())
        predecessorsComplete[successor] = 0;

      predecessorsComplete[successor]++;
      if (predecessorsComplete[successor] == successor->predecessors.size()) {
        weightNodes.erase(successor);
        antiDepWeightNodes.erase(successor);
        predecessorsComplete.erase(successor);
      }
    }

    // Remove the output dependencies from the anti-dependency set
    for (unsigned int i = 0; i < weightNodeSet.size(); ++i)
      antiDepNodeWeights[i] = antiDepNodeWeights[i] & ~weightNodeSet[i];

    unsigned int latency = 0;

    for (MachineInstr* MI : node->instructions)
      latency = std::max(latency, SII->GetSchedMaxLatency(MI));

    node->latency = latency;
    node->weight = latency + 1;

    for (unsigned int i = 0; i < node->number; ++i)
      if (getBit(weightNodeSet, i))
        node->weight += allGeneratedNodes.at(i)->latency + 1;
  }
}

void SHAVESchedulerBase::getNodeRegisterData(SHAVEDependencyNodePtr node,
                                             GraphBuilderData &data) {
  data.latestRegData.clear();

  for (MachineInstr* instruction : node->instructions) {
    for (MachineOperand& operand : instruction->operands()) {
      if (!operand.isReg())
        continue;

      Register reg = operand.getReg();

      if (reg == SHAVE::NoRegister)
        continue;

      if (data.ignoreRegisters.find(reg) != data.ignoreRegisters.end())
        continue;

      SmallVector<unsigned int> aliasRegs;

      if (Register::isPhysicalRegister(reg))
        aliasRegs = getRegisterAliases(reg);
      else
        aliasRegs.push_back(reg);

      for (unsigned int aliasReg : aliasRegs) {
#ifndef NDEBUG
        // This would be an assert if not for the call to dumpReg
        if (aliasReg == 0) {
          errs() << "SHAVESchedulerBase: Register "; SRI->dumpReg(reg);
          llvm_unreachable("                    got invalid sub-register 0\n");
        }
#endif // NDEBUG

        if (operand.isDef())
          data.latestRegData.defs.insert(aliasReg);
        else
          data.latestRegData.uses.insert(aliasReg);

        // Add only the whole VRF/IRF kills in the killed registers list
        // Else e.g. a kill of v10_q1 ends up deleting the defNode for v10_q0 (aliasReg), thereby
        // leaving future uses of other v10_qN registers (like v10_q0, v10_q2 etc.) without a defNode and breaking
        // the dependency graph
        if (operand.isKill() &&
            (SHAVE::VRF128RegClass.contains(reg) || SHAVE::IRF32RegClass.contains(reg)
              || SHAVE::TRFRegClass.contains(reg) || SHAVE::Combined_TRFRegClass.contains(reg)))
          data.latestRegData.kills.insert(aliasReg);

        if (operand.isDead() && operand.isImplicit())
          data.latestRegData.deads.insert(aliasReg);

        if (instruction->isCall(MachineInstr::IgnoreBundle) || instruction->isBranch(MachineInstr::IgnoreBundle) ||
            instruction->isIndirectBranch(MachineInstr::IgnoreBundle))
          if (!operand.isImplicit())
            data.latestRegData.explicits.insert(aliasReg);

        if (aliasReg == SHAVE::I0_q0) {
          node->hasDownstreamI0 = true;
          if (operand.isDef())
            node->definesI0 = true;
        }
      }
    }
  }
}

void SHAVESchedulerBase::generateIgnoreRegisters(MachineBasicBlock &block,
                                                 GraphBuilderData &data) {
  // This function has been implemented specifically to workaround an issue with significant performance
  // loss caused by the preemption support introduced in EISW-72384. It may be further extended in future
  // to cover all registers, with great care.
  std::set<unsigned int> checkRegisters = { SHAVE::I_STATE, SHAVE::CC_IAU0 };
  std::set<unsigned int> usedRegisters;

  for (MachineBasicBlock::instr_iterator instr = block.instr_begin(); instr != block.instr_end(); instr++) {
    for (MachineOperand& operand : instr->operands()) {
      if (operand.isReg()) {
        unsigned int reg = operand.getReg();
        if (checkRegisters.find(reg) != checkRegisters.end()) {
          if (operand.isUse() && !operand.isUndef())
            usedRegisters.insert(reg);
        }

        if (usedRegisters.size() == checkRegisters.size())
          return;
      }
    }
  }

  for (unsigned int reg : checkRegisters)
    if (usedRegisters.find(reg) == usedRegisters.end())
      data.ignoreRegisters.insert(reg);
}

void SHAVESchedulerBase::addUseRegDependencies(SHAVEDependencyNodePtr node,
                                               GraphBuilderData& data) {
  const auto &regData = data.latestRegData;

  for (unsigned int reg : regData.uses) {
    // Check for flow dependencies
    if (data.defNodes.find(reg) != data.defNodes.end()) {
      SHAVEDependencyNodePtr defNode = data.defNodes[reg];
      if (defNode != node) {
        addSuccessorToNode(node, defNode);

        if (std::find(dependencyGraphs.begin(), dependencyGraphs.end(), defNode) != dependencyGraphs.end())
          dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), defNode));
      }

      // Add the defNode as an outputSuccessor of the new node unless the new node is a call or return node and the defNode is an implicit
      // use, and not an explicit use. This is done to allow instructions before a call or return to fill in the branch delay slots of
      // that call or return
      if (((!node->isCall && !node->isReturn) ||
        regData.explicits.find(reg) != regData.explicits.end())) {
        addOutputSuccessorToNode(node, defNode);

        if (std::find(dependencyGraphs.begin(), dependencyGraphs.end(), defNode) != dependencyGraphs.end())
          dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), defNode));
      }
    }

    if (regData.kills.find(reg) != regData.kills.end())
      data.defNodes.erase(reg);

    if (data.useNodes.find(reg) != data.useNodes.end())
      data.useNodes[reg].push_back(node);
    else
      data.useNodes[reg] = std::vector<SHAVEDependencyNodePtr>(1, node);
  }
}

void SHAVESchedulerBase::addDefRegDependencies(SHAVEDependencyNodePtr node,
                                               GraphBuilderData& data) {
  const auto& regData = data.latestRegData;

  for (unsigned int reg : regData.defs) {
    // Check for anti-dependencies
    if (data.useNodes.find(reg) != data.useNodes.end()) {
      std::vector<SHAVEDependencyNodePtr>& uses = data.useNodes[reg];

      for (SHAVEDependencyNodePtr useNode : uses) {
        if (useNode != node) {
          addSuccessorToNode(node, useNode);

          if (std::find(dependencyGraphs.begin(), dependencyGraphs.end(), useNode) != dependencyGraphs.end())
            dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), useNode));
        }
      }
    }

    bool alreadyAdded = false;
    // Check for output dependencies with previous dead definitions
    if (data.deadDefNodes.find(reg) != data.deadDefNodes.end() && regData.deads.find(reg) == regData.deads.end()) {
      std::vector<SHAVEDependencyNodePtr>& defs = data.deadDefNodes[reg];
      for (SHAVEDependencyNodePtr defNode : defs) {
        if (defNode != node) {
          addSuccessorToNode(node, defNode);

          alreadyAdded = true;

          if (std::find(dependencyGraphs.begin(), dependencyGraphs.end(), defNode) != dependencyGraphs.end())
            dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), defNode));
        }
      }
    }

    // Check for output dependencies
    if (data.defNodes.find(reg) != data.defNodes.end() && !alreadyAdded) {
      SHAVEDependencyNodePtr defNode = data.defNodes[reg];
      if (defNode != node) {
        addSuccessorToNode(node, defNode);

        if (std::find(dependencyGraphs.begin(), dependencyGraphs.end(), defNode) != dependencyGraphs.end())
          dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), defNode));

        if ((!node->isCall && !node->isReturn) ||
            regData.explicits.find(reg) != regData.explicits.end())
          addOutputSuccessorToNode(node, defNode);
      }
    }

    if (regData.deads.find(reg) != regData.deads.end()) {
      // Add this node to the list of dead definitions for the register
      if (data.deadDefNodes.find(reg) == data.deadDefNodes.end())
        data.deadDefNodes[reg] = std::vector<SHAVEDependencyNodePtr>(1, node);
      else
        data.deadDefNodes[reg].push_back(node);

      for (SHAVEDependencyNodePtr previousUseNode : data.useNodes[reg]) {
        if (node != previousUseNode) {
          addSuccessorToNode(node, previousUseNode);

          if (std::find(dependencyGraphs.begin(), dependencyGraphs.end(), previousUseNode) != dependencyGraphs.end())
            dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), previousUseNode));
        }
      }
    }
    else {
      // If this is not a dead def then clear the previous dead def list
      if (data.deadDefNodes.find(reg) != data.deadDefNodes.end())
        data.deadDefNodes.erase(reg);

      // And set the def node for the register
      data.defNodes[reg] = node;

      // And finally clear all previous uses of this register
      if (data.useNodes.find(reg) != data.useNodes.end())
        data.useNodes.erase(reg);
    }
  }
}

void SHAVESchedulerBase::addMemoryDependencies(SHAVEDependencyNodePtr node,
                                               GraphBuilderData& data) {
  bool hasMemBarrier = false;

  for (MachineInstr* newNodeMI : node->instructions) {
    if ((newNodeMI->mayLoad(MachineInstr::IgnoreBundle) || newNodeMI->mayStore(MachineInstr::IgnoreBundle))) {
      hasMemBarrier = true;
      for (SHAVEDependencyNodePtr memOp : data.memoryOperations) {
        for (MachineInstr* currentNodeMI : memOp->instructions) {
          if ((!newNodeMI->memoperands_empty() && newNodeMI->hasOrderedMemoryRef()) ||         // if the new node is volatile
              (!currentNodeMI->memoperands_empty() && currentNodeMI->hasOrderedMemoryRef())) { // or the current node is volatile
            addSuccessorToNode(node, memOp);

            if (std::find(dependencyGraphs.begin(), dependencyGraphs.end(), memOp) != dependencyGraphs.end())
              dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), memOp));
          }

          if ((currentNodeMI->mayLoad(MachineInstr::IgnoreBundle) && newNodeMI->mayStore(MachineInstr::IgnoreBundle)) ||  // load -> store
              (currentNodeMI->mayStore(MachineInstr::IgnoreBundle) && newNodeMI->mayLoad(MachineInstr::IgnoreBundle)) ||  // store -> load
              (currentNodeMI->mayStore(MachineInstr::IgnoreBundle) && newNodeMI->mayStore(MachineInstr::IgnoreBundle))) { // store -> store
            if (compareMemoryAccesses(newNodeMI, currentNodeMI) != AliasResult::NoAlias) {
              if (data.latestOutputDependencies[memOp] != node) {
                addOutputSuccessorToNode(node, data.latestOutputDependencies[memOp]);

                if (std::find(dependencyGraphs.begin(), dependencyGraphs.end(), data.latestOutputDependencies[memOp]) != dependencyGraphs.end())
                  dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), data.latestOutputDependencies[memOp]));

                if (!newNodeMI->mayLoad()) {
                  for (std::pair<const SHAVEDependencyNodePtr, SHAVEDependencyNodePtr>& it : data.latestOutputDependencies)
                    if (it.second == memOp)
                      it.second = node;

                  data.latestOutputDependencies[memOp] = node;
                }

                break;
              }
            }
          }
        }
      }
    }
    else if (isBarrier(newNodeMI)) {
      for (SHAVEDependencyNodePtr memOp : data.memoryOperations) {
        addSuccessorToNode(node, memOp);

        if (std::find(dependencyGraphs.begin(), dependencyGraphs.end(), memOp) != dependencyGraphs.end())
          dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), memOp));
      }

      data.memoryOperations.clear();
      data.lastBumpedMemOp = nullptr;
    }
  }

  if (hasMemBarrier) {
    data.memoryOperations.push_back(node);

    if (data.lastBumpedMemOp != nullptr) {
      if (!data.lastBumpedMemOp->instructions.front()->mayStore() && !node->instructions.front()->mayStore())
        addSuccessorToNode(node, data.lastBumpedMemOp);
      else
        addOutputSuccessorToNode(node, data.lastBumpedMemOp);

      if (std::find(dependencyGraphs.begin(), dependencyGraphs.end(), data.lastBumpedMemOp) != dependencyGraphs.end())
        dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), data.lastBumpedMemOp));
    }

    if (data.memoryOperations.size() > SHAVEOptions::MaximumTrackedMemoryOperations) {
      SHAVEDependencyNodePtr bumpNode = data.memoryOperations.front();

      if (data.lastBumpedMemOp != nullptr) {
        if (!data.lastBumpedMemOp->instructions.front()->mayStore() && !bumpNode->instructions.front()->mayStore())
          addSuccessorToNode(bumpNode, data.lastBumpedMemOp);
        else
          addOutputSuccessorToNode(bumpNode, data.lastBumpedMemOp);
      }
      data.lastBumpedMemOp = bumpNode;
      data.memoryOperations.erase(data.memoryOperations.begin());
    }
  }
}

void SHAVESchedulerBase::addBranchDependencies(SHAVEDependencyNodePtr node,
                                               GraphBuilderData& data) {
  bool isBranch = false;
  for (MachineInstr* instr : node->instructions)
    if (instr->isBranch(MachineInstr::IgnoreBundle) || instr->isIndirectBranch(MachineInstr::IgnoreBundle))
      isBranch = true;

  if (isBranch) {
    branchNodes.push_back(node);

    while (!dependencyGraphs.empty()) {
      SHAVEDependencyNodePtr graph = *dependencyGraphs.begin();
      dependencyGraphs.erase(dependencyGraphs.begin());
      addSuccessorToNode(node, graph);
    }
  }
  else {
    // Check all of the dependency graph heads for branch instructions
    bool changeMade = true;
    while (changeMade) {
      changeMade = false;
      for (SHAVEDependencyNodePtr currentNode : dependencyGraphs) {
        bool isBranch = false;
        for (MachineInstr* currentNodeMI : currentNode->instructions)
          isBranch |= currentNodeMI->isBranch(MachineInstr::IgnoreBundle) | currentNodeMI->isCall(MachineInstr::IgnoreBundle) |
          currentNodeMI->isIndirectBranch(MachineInstr::IgnoreBundle) | currentNodeMI->isReturn(MachineInstr::IgnoreBundle);

        if (isBranch) {
          addSuccessorToNode(node, currentNode);

          dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), currentNode));

          changeMade = true;
          break;
        }
      }
    }
  }
}

void SHAVESchedulerBase::addBarrierDependencies(SHAVEDependencyNodePtr node,
                                                GraphBuilderData& data) {
  if (data.lastBarrier != nullptr) {
    if (std::find(node->successors.begin(), node->successors.end(), data.lastBarrier) == node->successors.end())
      node->successors.push_back(data.lastBarrier);

    if (std::find(node->outputSuccessors.begin(), node->outputSuccessors.end(), data.lastBarrier) == node->outputSuccessors.end())
      node->outputSuccessors.push_back(data.lastBarrier);

    if (std::find(dependencyGraphs.begin(), dependencyGraphs.end(), data.lastBarrier) != dependencyGraphs.end())
      dependencyGraphs.erase(std::find(dependencyGraphs.begin(), dependencyGraphs.end(), data.lastBarrier));
  }

  for (MachineInstr* instr : node->instructions) {
    if (isBarrier(instr)) {
      data.lastBarrier = node;

      for (SHAVEDependencyNodePtr graphHead : dependencyGraphs)
        addSuccessorToNode(node, graphHead);

      // If there are any anti-dependent chains of successors with a higher combined latency than this node's latency,
      // such chains may outpace this instruction so that they complete their execution after the barrier.
      // To avoid this, we add all nodes in anti-dependent chains as successors of this node
      std::vector<SHAVEDependencyNodePtr> worklist;
      for (auto& successor : node->successors)
        if (std::find(node->outputSuccessors.begin(), node->outputSuccessors.end(), successor) == node->outputSuccessors.end())
          worklist.push_back(successor);
      std::set<SHAVEDependencyNodePtr> donelist(worklist.begin(), worklist.end());

      while (!worklist.empty()) {
        SHAVEDependencyNodePtr successorNode = worklist.front();
        worklist.erase(worklist.begin());

        addSuccessorToNode(node, successorNode);

        for (auto& successor : successorNode->successors) {
          if (donelist.find(successor) == donelist.end() &&
              std::find(successorNode->outputSuccessors.begin(), successorNode->outputSuccessors.end(), successor) == successorNode->outputSuccessors.end()) {
            worklist.push_back(successor);
            donelist.insert(successor);
          }
        }
      }

      data.defNodes.clear();
      data.useNodes.clear();
      data.deadDefNodes.clear();

      dependencyGraphs.clear();
      break;
    }
  }
}

//
// Definition of protected member functions of SHAVESchedulerBase
//

SmallVector<unsigned int> SHAVESchedulerBase::getRegisterAliases(unsigned int reg) {
  SmallVector<unsigned int> result;

  if (SHAVE::IRF64RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getSubReg(reg, SHAVE::vsub32_0), SHAVE::qsub_0));
    result.push_back(SRI->getSubReg(SRI->getSubReg(reg, SHAVE::vsub32_1), SHAVE::qsub_0));
  }
  else if (SHAVE::IRF32RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(reg, SHAVE::qsub_0));
  }
  else if (SHAVE::IRF16_lRegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, hsub_0, &SHAVE::IRF32RegClass), SHAVE::qsub_0));
  }
  else if (SHAVE::IRF16_hRegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, hsub_1, &SHAVE::IRF32RegClass), SHAVE::qsub_0));
  }
  else if (SHAVE::IRF8_q1RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, qsub_1, &SHAVE::IRF32RegClass), SHAVE::qsub_0));
  }
  else if (SHAVE::IRF8_q2RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, qsub_2, &SHAVE::IRF32RegClass), SHAVE::qsub_0));
  }
  else if (SHAVE::IRF8_q3RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, qsub_3, &SHAVE::IRF32RegClass), SHAVE::qsub_0));
  }
  else if (SHAVE::WVRF512RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(reg, SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF256_0RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(reg, SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF256_1RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_256_1, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF128_0RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(reg, SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF128_1RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_128_1, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF128_2RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_128_2, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF128_3RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_128_3, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF64_0RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(reg, SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF64_1RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_64_1, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF64_2RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_64_2, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF64_3RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_64_3, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF64_4RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_64_4, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF64_5RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_64_5, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF64_6RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_64_6, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF64_7RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_64_7, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_0RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(reg, SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_1RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_1, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_2RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_2, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_3RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_3, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_4RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_4, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_5RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_5, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_6RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_6, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_7RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_7, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_8RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_8, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_9RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_9, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_10RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_10, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_11RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_11, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_12RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_12, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_13RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_13, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_14RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_14, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::WVRF32_15RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, wsub_32_15, &SHAVE::WVRF512RegClass), SHAVE::wsub_16_0));
  }
  else if (SHAVE::VRF128RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(reg, SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF64_lRegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, hsub_0, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF64_hRegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, hsub_1, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF32_q0RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, qsub_0, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF32_q1RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, qsub_1, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF32_q2RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, qsub_2, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF32_q3RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, qsub_3, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF16_e1RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, vsub16_1, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF16_e2RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, vsub16_2, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF16_e3RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, vsub16_3, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF16_e4RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, vsub16_4, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF16_e5RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, vsub16_5, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF16_e6RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, vsub16_6, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::VRF16_e7RegClass.contains(reg)) {
    result.push_back(SRI->getSubReg(SRI->getMatchingSuperReg(reg, vsub16_7, &SHAVE::VRF128RegClass), SHAVE::vsub16_0));
  }
  else if (SHAVE::TRFRegClass.contains(reg)) {
    if (reg == SHAVE::C_CMU0) {
      for (int alias = SHAVE::CC_CMU0; alias <= SHAVE::CC_CMU7; alias++)
        result.push_back(alias);
    }
    else if (reg == SHAVE::C_CMU1){
      for (int alias = SHAVE::CC_CMU8; alias <= SHAVE::CC_CMU15; alias++)
        result.push_back(alias);
    }
    else {
      result.push_back(reg);
    }
  }
  else if (SHAVE::Combined_TRFRegClass.contains(reg)) {
    if (reg == SHAVE::C_CMU_0_3) {
      for (int alias = SHAVE::CC_CMU0; alias <= SHAVE::CC_CMU3; alias++)
        result.push_back(alias);
    }
    else if (reg == SHAVE::C_CMU_0_7) {
      for (int alias = SHAVE::CC_CMU0; alias <= SHAVE::CC_CMU7; alias++)
        result.push_back(alias);
    }
    else if (reg == SHAVE::C_CMU_0_15) {
      for (int alias = SHAVE::CC_CMU0; alias <= SHAVE::CC_CMU15; alias++)
        result.push_back(alias);
    }
    else {
      result.push_back(reg);
    }
  }
  else {
    result.push_back(reg);
  }

  return result;
}

void SHAVESchedulerBase::addSuccessorToNode(SHAVEDependencyNodePtr node,
                                            SHAVEDependencyNodePtr newSuccessor) {
  if (std::find(node->successors.begin(), node->successors.end(), newSuccessor) == node->successors.end())
    node->successors.push_back(newSuccessor);

  node->hasDownstreamI0 |= newSuccessor->hasDownstreamI0;
}

void SHAVESchedulerBase::addOutputSuccessorToNode(SHAVEDependencyNodePtr node,
                                                  SHAVEDependencyNodePtr newSuccessor) {
  if (std::find(node->outputSuccessors.begin(), node->outputSuccessors.end(), newSuccessor) == node->outputSuccessors.end()) {
    if (std::find(node->successors.begin(), node->successors.end(), newSuccessor) == node->successors.end())
      node->successors.push_back(newSuccessor);

    node->outputSuccessors.push_back(newSuccessor);
  }

  node->hasDownstreamI0 |= newSuccessor->hasDownstreamI0;
}

void SHAVESchedulerBase::generateDependencyGraphs(MachineBasicBlock &block) {
  branchNodes.clear();
  entryDebugInstructions.clear();

  GraphBuilderData data;
  generateIgnoreRegisters(block, data);

  unsigned int nodeNumber = 0;

  MachineBasicBlock::instr_iterator iter = block.instr_begin();
  const MachineBasicBlock::instr_iterator end = block.instr_end();

  while (iter != end) {
    MachineInstr *MI = &*iter;

    // Append debug instructions onto the last generated node
    if (MI->isDebugValue() || MI->isCFIInstruction() || MI->isPosition()) {
      assert(!MI->isBundled() && "Debug instructions should never be bundled");

      if (allGeneratedNodes.empty()) {
        entryDebugInstructions.push_back(MI);
      }
      else {
        SHAVEDependencyNodePtr node = allGeneratedNodes.back();
        node->debugInstructions.push_back(MI);
      }

      iter++;
      continue;
    }

    SHAVEDependencyNodePtr newNode = new SHAVEDependencyNode(nodeNumber++);

    auto addInstruction = [&newNode](MachineInstr *instruction) {
      newNode->isCall |= instruction->isCall();
      newNode->isReturn |= instruction->isReturn();
      newNode->isPrefetch |= SHAVEConflicts::check_isPrefetch(instruction->getOpcode());
      newNode->instructions.push_back(instruction);
    };

    // Add all instructions in a bundle to the same node
    while (MI->isBundledWithSucc()) {
      addInstruction(MI);
      iter++;
      MI = &*iter;
    }
    addInstruction(MI);
    iter++;

    if (newNode->instructions.size() > 1)
      newNode->isBundled = true;

    data.latestOutputDependencies[newNode] = newNode;

    // Get the set of "alias" register data for the instructions in this node
    getNodeRegisterData(newNode, data);

    //
    // Check for register dependencies
    //
    addUseRegDependencies(newNode, data);
    addDefRegDependencies(newNode, data);

    //
    // Check for memory dependencies
    //
    addMemoryDependencies(newNode, data);

    // If the current bundle contains a branch instructions then all instructions
    // that come before the branch must be completed before the branch is taken
    addBranchDependencies(newNode, data);

    // Barriers are a focal point for all nodes
    addBarrierDependencies(newNode, data);

    dependencyGraphs.push_back(newNode);
    allGeneratedNodes.push_back(newNode);
  }

  // Initialise the predecessor nodes vector for each generated node
  for (SHAVEDependencyNodePtr node : allGeneratedNodes)
    for (SHAVEDependencyNodePtr successor : node->successors)
      successor->predecessors.push_back(node);

  // Finally assign a weight to each node
  assignNodeWeights();
}

bool SHAVESchedulerBase::doMemoryRangesOverlap(int offset1,
                                               int offset2,
                                               int size1,
                                               int size2) const {
  int end1 = offset1 + size1;
  int end2 = offset2 + size2;
  return ((offset2 >= offset1) && (offset2 < end1)) || ((offset1 >= offset2) && (offset1 < end2));
}

bool SHAVESchedulerBase::memoryOperandUsesStack(MachineInstr *instr,
                                                unsigned int pointerOperandIndex,
                                                unsigned int &stackOffset) const {
  MachineOperand &pointerOperand = instr->getOperand(pointerOperandIndex);
  bool found = false;
  stackOffset = 0;

  if (check_isLDXVorSTXV(instr->getOpcode()) || check_isLDXorSTX(instr->getOpcode())) {
    // FIXME: Movidius - It should be possible to work backwards through the block to find an LDIL
    //                   which loads the offset for and LDX/STX accessing the stack. This can happen
    //                   if the offset is outside the range allowed for the equivalent LDO/STO
    return false;
  }

  if (pointerOperand.isReg() && (pointerOperand.getReg() == SHAVERegisterInfo::getSPReg())) {
    found = true;
  } else if (pointerOperand.isFI()) {
    const MachineFrameInfo &MFI = instr->getParent()->getParent()->getFrameInfo();
    stackOffset = MFI.getObjectOffset(pointerOperand.getIndex());
    found = true;
  }

  int immOffset = -1;

  if (found) {
    bool foundOffset = SII->findLoadStoreOffset(instr, &immOffset);
    if (foundOffset && immOffset >= 0)
      stackOffset += (unsigned int) immOffset;
  }

  return found;
}

static unsigned int getPointerOperandIndex(MachineInstr *instr) {
  switch (instr->getOpcode()) {
  default:
    return 1;
  case SHAVE::LSU_LDOV128_h:
    return 2;
  }
}

bool SHAVESchedulerBase::isReadOnlyAccess(MachineInstr *instr) {
  auto cached = readOnlyCache.find(instr);
  if (cached != readOnlyCache.end())
    return cached->second;

  auto result = [this, &instr](bool value) {
    readOnlyCache[instr] = value;
    return value;
  };

  if (instr->mayStore())
    return result(false);

  // Check for constant-pools, which are always read-only on SHAVE
  if (instr->hasOneMemOperand()) {
    const auto * memOperand = *instr->memoperands_begin();
    const auto * pseudoValue = memOperand->getPseudoValue();
    if (pseudoValue && pseudoValue->kind() == PseudoSourceValue::ConstantPool)
      return result(true);
  }

  unsigned int pointerOperandIndex = getPointerOperandIndex(instr);
  MachineOperand &pointer = instr->getOperand(pointerOperandIndex);

  if (!pointer.isReg())
    return result(false);

  MachineBasicBlock::instr_iterator it = instr->getIterator();

  // Find the first instruction which defines the pointer register
  while (it != instr->getParent()->instr_begin()) {
    --it;

    if (it->definesRegister(pointer.getReg())) {
      if (it->getOpcode() == SHAVE::LSU_LDIHSym) {
        // If the LDIHSym is predicated then it might not execute before this load
        // so we must assume it won't and its value is not trustworthy
        if (SII->isPredicated(*it))
          return false;

        MachineOperand &location = it->getOperand(2);

        if (location.isCPI()) {
          return result(true);
        }
        else if (location.isGlobal()) {
          const GlobalVariable *global = dyn_cast<const GlobalVariable>(location.getGlobal());

          if (global != nullptr && global->isConstant())
            return result(true);
        }
      }

      break;
    }
  }

  return result(false);
}

MachineMemOperand *SHAVESchedulerBase::extractMemoryOperand(MachineInstr *instr,
                                                            bool &usesStack,
                                                            unsigned int &stackOffset) const {
  unsigned int pointerOperandIndex = getPointerOperandIndex(instr);

  MachineMemOperand *memOperand = nullptr;
  if (instr->hasOneMemOperand())
    memOperand = *instr->memoperands_begin();

  usesStack = memoryOperandUsesStack(instr, pointerOperandIndex, stackOffset);

  if (!usesStack && memOperand && memOperand->getValue()) {
    // Second chance to detect stack operations.
    if (const PseudoSourceValue * PSV = (*instr->memoperands_begin())->getPseudoValue()) {
      if (isa<FixedStackPseudoSourceValue>(PSV))
        usesStack = true;
      else
        usesStack = PSV->isConstant(nullptr) || PSV->isStack();
    }
  }

  return memOperand;
}

MachineBasicBlock *SHAVESchedulerBase::getBranchTarget(MachineInstr *branchInstr) {
  switch (branchInstr->getOpcode()) {
  default:
    return nullptr;
  case SHAVE::BRU_BRA:
    return branchInstr->getOperand(0).getMBB();
  case SHAVE::BRU_JMP:
  case SHAVE::BRU_JMPcc:
    MachineBasicBlock::reverse_instr_iterator it = branchInstr->getParent()->instr_rbegin();
    MachineBasicBlock::reverse_instr_iterator end = branchInstr->getParent()->instr_rend();

    // Find the branchInstr
    while (it != end && &*it != branchInstr)
      it++;

    while (it != end) {
      if (it->getOpcode() == SHAVE::LSU_LDIH_Label)
        return it->getOperand(2).getMBB();
      ++it;
    }

    return nullptr;
  }
}

void SHAVESchedulerBase::cleanup() {
  dependencyGraphs.clear();
  branchNodes.clear();
  entryDebugInstructions.clear();
  readOnlyCache.clear();

  for (SHAVEDependencyNodePtr node : allGeneratedNodes)
    delete node;
  allGeneratedNodes.clear();
}

#ifndef NDEBUG
void SHAVESchedulerBase::dumpDependencyGraphs(raw_ostream &out, bool dumpIDs) {
  std::vector<SHAVEDependencyNodePtr> workList;
  std::vector<SHAVEDependencyNodePtr> doneList;

  // Assign each machine instruction a unique ID. This ID is used as the name
  // of the instructions node in the generated digraph. We do this so that each
  // instruction is guaranteed to have a unique node identifier and to reduce
  // the complexity of the generated graph (since the node IDs will be short
  // and simple)
  if (dumpIDs) {
    instrIDs.clear();
    out << "SHAVESchedulerBase: Machine Instruction IDs:\n";
    generateAndDumpInstrIDs(out);
  }

  out << "SHAVESchedulerBase: start of the .dot digraphs for the dependency graphs\n";

  for (SHAVEDependencyNodePtr graph : dependencyGraphs) {
    out << "\ndigraph {\n";

    doneList.clear();
    workList.push_back(graph);
    while (!workList.empty()) {
      SHAVEDependencyNodePtr currentNode = workList.back();
      workList.pop_back();

      // If this node has not been printed yet, print out the node ID followed by all
      // edges coming from this node to its successors
      if (std::find(doneList.begin(), doneList.end(), currentNode) == doneList.end()) {
        out << "MI" << instrIDs[currentNode->instructions[0]] << "[label=\"MI" << instrIDs[currentNode->instructions[0]];

        for (unsigned int i = 1; i < currentNode->instructions.size(); ++i) {
          out << "-" << instrIDs[currentNode->instructions[i]];
        }
        out << "\\n";
        for (auto instruction : currentNode->instructions) {
          out << SII->getName(instruction->getOpcode());
          out << "\\n";
        }
        out << currentNode->weight << "\"";
        if (std::find(branchNodes.begin(), branchNodes.end(), currentNode) != branchNodes.end())
          out << ",style=filled,color=yellow";
        out << "];\n";

        for (SHAVEDependencyNodePtr successor : currentNode->successors) {
          out << "MI" << instrIDs[currentNode->instructions[0]] << " -> MI" << instrIDs[successor->instructions[0]];
          if (std::find(currentNode->outputSuccessors.begin(), currentNode->outputSuccessors.end(), successor) != currentNode->outputSuccessors.end())
            out << "[color=\"red\"]";
          out << ";\n";
          workList.push_back(successor);
        }
        doneList.push_back(currentNode);
      }
    }

    out << "}\n";
  }

  out << "\nSHAVESchedulerBase: end of the .dot digraph for the dependency graphs\n";
}
#endif // NDEBUG

//
// Definition of public member functions of SHAVESchedulerBase
//

AliasResult SHAVESchedulerBase::compareMemoryAccesses(MachineInstr* instr1, MachineInstr* instr2) {
  bool usesStack1 = false;
  bool usesStack2 = false;
  unsigned int offset1 = 0;
  unsigned int offset2 = 0;
  MachineMemOperand* memoryOperand1 = extractMemoryOperand(instr1, usesStack1, offset1);
  MachineMemOperand* memoryOperand2 = extractMemoryOperand(instr2, usesStack2, offset2);

  // If one or both of these instruction are loads from a read-only region of memory
  // then there cannot be any memory dependencies with store instructions
  if (isReadOnlyAccess(instr1) || isReadOnlyAccess(instr2))
    return AliasResult::NoAlias;

  // Instructions without memory operand may alias
  if (memoryOperand1 == nullptr || memoryOperand2 == nullptr)
    return AliasResult::MayAlias;

  // Check memory operations that use the stack
  if (usesStack1 && usesStack2) {
    if (offset1 == offset2)
      return AliasResult::MustAlias;
    else if (doMemoryRangesOverlap(offset1, offset2, memoryOperand1->getSize(), memoryOperand2->getSize()))
      return AliasResult::PartialAlias;
    else
      return AliasResult::NoAlias;
  }

  // If either of the memory references are empty, it doesn't matter what the
  // pointer values are
  if (memoryOperand1->getSize() == 0 || memoryOperand2->getSize() == 0)
    return AliasResult::NoAlias;

  // If either of the memory pointers are unknown, they may alias
  const Value* pointer1 = memoryOperand1->getValue();
  const Value* pointer2 = memoryOperand2->getValue();
  if (pointer1 == nullptr || pointer2 == nullptr)
    return AliasResult::MayAlias;

  // Compare memory pointer values
  pointer1 = pointer1->stripPointerCasts();
  pointer2 = pointer2->stripPointerCasts();

  if (pointer1 == pointer2) {
    // Look for intersections in the accessed memory range.
    if (memoryOperand1->getOffset() == memoryOperand2->getOffset())
      return AliasResult::MustAlias;
    else if (doMemoryRangesOverlap(memoryOperand1->getOffset(), memoryOperand2->getOffset(), memoryOperand1->getSize(), memoryOperand2->getSize()))
      return AliasResult::PartialAlias;
    else
      return AliasResult::NoAlias;
  }
  else if (AA) {
    int64_t MinOffset = std::min(memoryOperand1->getOffset(), memoryOperand2->getOffset());
    int64_t Overlap1 = memoryOperand1->getSize() + memoryOperand1->getOffset() - MinOffset;
    int64_t Overlap2 = memoryOperand2->getSize() + memoryOperand2->getOffset() - MinOffset;

    return AA->alias(
      MemoryLocation(memoryOperand1->getValue(), Overlap1, memoryOperand1->getAAInfo()),
      MemoryLocation(memoryOperand2->getValue(), Overlap2, memoryOperand2->getAAInfo()));
  }

  return AliasResult::MayAlias;
}
