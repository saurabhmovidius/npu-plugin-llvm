//===-- SHAVEAntiDependencyBreaker.cpp - Pre-Scheduling Pass ----*- C++ -*-===//
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

#define DEBUG_TYPE "shave-anti-dep-breaker"

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"

#include "SHAVE.h"
#include "SHAVEAntiDependencyBreaker.h"

#include <queue>
#include <unordered_map>

using namespace llvm;
using namespace SHAVEAntiDependencyBreaker;

char SHAVEAntiDepBreaker::ID = 0;

INITIALIZE_PASS_BEGIN(SHAVEAntiDepBreaker, "shaveantidepbreakerpass", "SHAVE Anti-Dependency Breaker Pass", false, false)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_END(SHAVEAntiDepBreaker, "shaveantidepbreakerpass", "SHAVE Anti-Dependency Breaker Pass", false, false)

MachineFunctionPass *llvm::createSHAVEAntiDepBreakerPass() {
  return new SHAVEAntiDepBreaker();
}

// Instructions which should be skipped for node numbering
static bool skipInstruction(const MachineInstr &instruction) {
  return instruction.isDebugValue() || instruction.isCFIInstruction() || instruction.isPosition();
}

/******************************************************************************
 *
 * Dependency Graph generator and helper classes/functions
 * 
 ******************************************************************************/

//
// DependencyEdge public functions
//

void DependencyEdge::merge(const DependencyEdge &edge) {
  registers.insert(edge.registers.begin(), edge.registers.end());
}

bool DependencyEdge::operator==(const DependencyEdge &edge) const {
  // The registers associated with the edges do not matter, we only care
  // if they point to the same node.
  return node == edge.node;
}

//
// DependencyNode private functions
//

bool DependencyNode::hasEdgeWithType(DependencyEdge &edge, EdgeType type) const {
  // Return true is this node has the specified edge with the given precendence type, false otherwise
  const auto &typeEdges = edges[(size_t) type];
  return std::find(typeEdges.begin(), typeEdges.end(), edge) != typeEdges.end();
}

void DependencyNode::updateEdge(DependencyEdge &edge, EdgeType type) {
  // Update an existing edge with a new set of registers
  auto &typeEdges = edges[(size_t) type];
  auto existingEdge = std::find(typeEdges.begin(), typeEdges.end(), edge);
  existingEdge->merge(edge);
}

void DependencyNode::upgradeEdge(DependencyEdge &edge, EdgeType existingType, EdgeType newType) {
  // Upgrade an existing edge to a higher precendence and update the register set
  auto &typeEdges = edges[(size_t) existingType];
  auto existingEdge = std::find(typeEdges.begin(), typeEdges.end(), edge);
  edge.merge(*existingEdge);
  insertEdge(edge, newType);
  typeEdges.erase(existingEdge);
}

void DependencyNode::insertEdge(DependencyEdge &edge, EdgeType type) {
  // Insert a new edge, sorted by weight, highest first
  auto &typeEdges = edges[(size_t) type];
  auto insertionPoint = typeEdges.begin();
  while (insertionPoint != typeEdges.end()) {
    if (insertionPoint->node->getWeight(EdgeType::All) < edge.node->getWeight(EdgeType::All))
      break;
    ++insertionPoint;
  }
  typeEdges.insert(insertionPoint, edge);
}

//
// DependencyNode public functions
//

template<typename ...Registers>
void DependencyNode::addSuccessor(DependencyNodePtr node, EdgeType edgeType, Registers... registers) {
  DependencyEdge edge(node, std::forward<Registers>(registers)...);

  // Edge already exists with same precedence
  if (hasEdgeWithType(edge, edgeType)) {
    updateEdge(edge, edgeType);
    return;
  }

  // Edge already exists with a higher precedence
  for (int checkType = ((int) edgeType) - 1; checkType >= (int) EdgeType::HighestPrecedence; --checkType) {
    if (hasEdgeWithType(edge, (EdgeType) checkType)) {
      updateEdge(edge, (EdgeType) checkType);
      return;
    }
  }

  // Edge already exists with a lower precendence
  for (int checkType = ((int) edgeType) + 1; checkType < (int) EdgeType::All; ++checkType) {
    if (hasEdgeWithType(edge, (EdgeType) checkType)) {
      upgradeEdge(edge, (EdgeType) checkType, edgeType);
      return;
    }
  }

  insertEdge(edge, edgeType);
}

void DependencyNode::calculateWeights(unsigned int latency) {
  // Should only be called once, after all edges have been added to the node
  uint64_t totalWeight = latency;
  for (size_t edgeType = (size_t) EdgeType::HighestPrecedence; edgeType < (size_t) EdgeType::All; ++edgeType) {
    // Weight from Memory dependencies skew the values we actually care about for
    // anti-dependency breaking (i.e. register dependencies). This is especially the
    // case when we don't have proper alias analysis in the pass
    if (edgeType == (size_t) EdgeType::Memory)
      continue;

    uint64_t weight = 0;
    for (auto &edge : edges[edgeType]) {
      weight += edge.node->getWeight(EdgeType::All);
    }
    edgeWeights[edgeType] = weight;
    totalWeight += weight;
  }
  edgeWeights[(size_t) EdgeType::All] = totalWeight;
}

uint64_t DependencyNode::getWeight(EdgeType edgeType) {
  return edgeWeights[(size_t) edgeType];
}

DependencyNode::Edges& DependencyNode::getEdges(EdgeType edgeType) {
  return edges[(size_t) edgeType];
}

//
// DependencyGraph private functions
// 

template<typename ...Registers>
void DependencyGraph::addNodeSuccessor(DependencyNodePtr node, DependencyNodePtr successor, EdgeType edgeType, Registers... registers) {
  if (node == successor)
    return;

  node->addSuccessor(successor, edgeType, std::forward<Registers>(registers)...);
}


void DependencyGraph::updateEdges(DependencyNodePtr node, EdgeType edgeType, unsigned int removedReg,
                                  std::vector<DependencyNodePtr> &removedNodes,
                                  const std::function<bool(DependencyNodePtr)> &check) {
  auto &edges = node->getEdges(edgeType);
  bool modified;
  do {
    modified = false;
    for (auto it = edges.begin(); it != edges.end(); ++it) {
      auto &edge = *it;
      if (check(edge.node)) {
        if (edge.registers.find(removedReg) != edge.registers.end()) {
          DEBUG(dbgs() << "Removing register " << SRI->getName(removedReg) << " from edge N" << node->number << "->N" << edge.node->number << "\n");
          edge.registers.erase(removedReg);
          removedNodes.push_back(edge.node);
          if (edge.registers.empty()) {
            DEBUG(dbgs() << "  Edge register set is now empty, deleting edge\n");
            edges.erase(it);
            modified = true;
            break;
          }
        }
      }
    }
  } while (modified);
}

//
// DependencyGraph public functions
//

void DependencyGraph::generateGraph(MachineBasicBlock &block) {
  std::unordered_map<unsigned int, DependencyNodePtr> writeNodes;
  std::unordered_map<unsigned int, std::vector<DependencyNodePtr>> readNodes;
  std::vector<DependencyNodePtr> memoryNodes;
  DependencyNodePtr bumpedMemoryNode = nullptr;
  std::vector<MachineInstr *> nodeInstructions;

  unsigned int nodeNumber = 0;
  for (auto &instruction : block.instrs())
    if (!skipInstruction(instruction) && !instruction.isBundledWithSucc())
      ++nodeNumber;

  // Bottom-up traversal of the block
  iterator_range<MachineBasicBlock::reverse_instr_iterator> instructions(block.instr_rbegin(), block.instr_rend());
  for (auto &instruction : instructions) {
    if (skipInstruction(instruction))
      continue;

    if (instruction.isBundledWithPred()) {
      nodeInstructions.push_back(&instruction);
      continue;
    }

    nodeInstructions.push_back(&instruction);

    DependencyNodePtr node = std::make_shared<DependencyNode>(nodeInstructions, nodeNumber--);

    bool isMemoryNode = false;

    // Handle register dependencies
    for (auto nodeInstruction : nodeInstructions) {
      for (const auto &operand : nodeInstruction->operands()) {
        if (!operand.isReg())
          continue;

        auto registers = SRI->getPhysicalSHAVERegisters(operand.getReg());

        for (unsigned int reg : registers) {        
          if (reg == SHAVE::NoRegister)
            continue;

          if (operand.isDef()) {
            // Write reg
            if (writeNodes.find(reg) != writeNodes.end()) {
              // If this is a predicated writeback then there is an implicit read of the previous def
              // So this is a read-after-write dependency
              if (writeNodes[reg]->mayBePredicated())
                addNodeSuccessor(node, writeNodes[reg], EdgeType::ReadAfterWrite, reg);
              // Otherwise, Write-after-write dependency
              else
                addNodeSuccessor(node, writeNodes[reg], EdgeType::WriteAfterWrite, reg);
            }

            if (readNodes.find(reg) != readNodes.end()) {
              // Read-after-write dependency
              for (auto successor : readNodes[reg])
                addNodeSuccessor(node, successor, EdgeType::ReadAfterWrite, reg);
            }

            // Do not clobber the write node for an implicit def of a super-register
            // IRF32RegClass added to the condition for the cases like the following
            // (instructions with implicit super-reg operands):
            // $i7_q0 = IAU_INCS_i8 $i7_q0(tied-def 0), -97, 1, $noreg, implicit killed $i7, implicit-def $i7
            if (!((SHAVE::IRF64RegClass.contains(operand.getReg()) ||
                   SHAVE::IRF32RegClass.contains(operand.getReg())) &&
                  operand.isImplicit())) {
              writeNodes[reg] = node;
              readNodes.erase(reg);
            }
          }
          else {
            // Read reg
            if (!(operand.isImplicit() && operand.isUndef())) { // Can safely ignore implicit undefs
              if (writeNodes.find(reg) != writeNodes.end() && writeNodes[reg] != node) {
                // Write-after-read dependency
                addNodeSuccessor(node, writeNodes[reg], EdgeType::WriteAfterRead, reg);
              }

              if (operand.isKill() && writeNodes[reg] != node)
                writeNodes.erase(reg);
              readNodes[reg].push_back(node);
            }
          }
        }
      }

      // Handle memory dependencies
      if (nodeInstruction->mayLoad() || nodeInstruction->mayStore()) {
        isMemoryNode = true;

        if (!nodeInstruction->memoperands_empty() && nodeInstruction->hasOrderedMemoryRef()) { // Instruction is volatile
          for (auto memoryNode : memoryNodes) {
            addNodeSuccessor(node, memoryNode, EdgeType::Memory);
          }
        }
      
        for (auto memoryNode : memoryNodes) {
          if ((memoryNode->mayStore() && (nodeInstruction->mayStore() || nodeInstruction->mayLoad())) ||
              (memoryNode->mayLoad() && nodeInstruction->mayStore())) {
            // FIXME: Movidius - The SHAVESchedulerBase does a more complex analysis of the memory than this
            //                   so we may be missing some opportunities. We should pull that implementation
            //                   out of the scheduler into some utility header for use here.
            if (AA == nullptr) {
              addNodeSuccessor(node, memoryNode, EdgeType::Memory);
              continue;
            }

            MachineMemOperand *memoryOperand0 = memoryNode->getMemoryOperand();

            MachineMemOperand *memoryOperand1 = nullptr;
            if (nodeInstruction->hasOneMemOperand())
              memoryOperand1 = *(nodeInstruction->memoperands_begin());

            if (memoryOperand0 == nullptr || memoryOperand1 == nullptr ||
                memoryOperand0->getValue() == nullptr || memoryOperand1->getValue() == nullptr) {
              addNodeSuccessor(node, memoryNode, EdgeType::Memory);
              continue;
            }

            int64_t minOffset = std::min(memoryOperand0->getOffset(), memoryOperand1->getOffset());
            int64_t overlap0 = memoryOperand0->getSize() + memoryOperand0->getOffset() - minOffset;
            int64_t overlap1 = memoryOperand1->getSize() + memoryOperand1->getOffset() - minOffset;

            auto aliasResult = AA->alias(MemoryLocation(memoryOperand0->getValue(), overlap0, memoryOperand0->getAAInfo()),
                                         MemoryLocation(memoryOperand1->getValue(), overlap1, memoryOperand1->getAAInfo()));

            if (aliasResult != AliasResult::NoAlias)
              addNodeSuccessor(node, memoryNode, EdgeType::Memory);
          }
        }
      }
    }

    if (isMemoryNode) {
      memoryNodes.push_back(node);
      if (memoryNodes.size() > SHAVEOptions::MaximumTrackedMemoryOperations) {
        DependencyNodePtr nextBumped = memoryNodes.front();
        if (bumpedMemoryNode != nullptr)
          addNodeSuccessor(nextBumped, bumpedMemoryNode, EdgeType::Memory);
        bumpedMemoryNode = nextBumped;
        memoryNodes.erase(memoryNodes.begin());
      }
    }

    unsigned int latency = 0;
    for (auto nodeInstruction : nodeInstructions)
      latency = std::max(latency, SII->GetSchedMaxLatency(nodeInstruction));

    node->calculateWeights(latency + 1);
#ifndef NDEBUG
    allNodes.push_back(node);
#endif // NDEBUG
    
    for (unsigned int type = 0; type < ((unsigned int) EdgeType::All) + 1; ++type) {
      for (std::vector<DependencyNodePtr>::iterator it = orderedNodes[type].begin(); it != orderedNodes[type].end(); ++it) {
        if (node->getWeight(EdgeType::All) > (*it)->getWeight(EdgeType::All)) {
          orderedNodes[type].insert(it, node);
          break;
        }
      }
      if (orderedNodes[type].empty())
        orderedNodes[type].push_back(node);
    }

    nodeInstructions.clear();
  }
}

void DependencyGraph::update(DependencyNodePtr node, std::vector<DependencyNodePtr> &nodes, unsigned int originalReg, unsigned int newReg) {
  // Update the register sets for all modified nodes
  for (DependencyNodePtr updateNode : nodes) {
    for (size_t type = (size_t) EdgeType::HighestPrecedence; type < ((size_t) EdgeType::All); ++type) {
      for (DependencyEdge &edge : updateNode->getEdges((EdgeType) type)) {
        if (std::find(nodes.begin(), nodes.end(), edge.node) != nodes.end() && edge.registers.find(originalReg) != edge.registers.end()) {
          DEBUG(dbgs() << "Replacing edge register " << SRI->getName(originalReg) << " with " << (SRI->getName(newReg)));
          DEBUG(dbgs() << " for edge N" << updateNode->number << "->N" << edge.node->number << "\n");
          edge.registers.erase(originalReg);
          edge.registers.insert(newReg);
        }
      }
    }
  }

  const auto inNodes = [&nodes](DependencyNodePtr node) { return std::find(nodes.begin(), nodes.end(), node) != nodes.end(); };
  const auto notInNodes = [&nodes](DependencyNodePtr node) { return std::find(nodes.begin(), nodes.end(), node) == nodes.end(); };
  std::vector<std::pair<EdgeType, DependencyNodePtr>> removedDependencies;

  // Update the register sets for edges from any nodes to the modified nodes. If the 
  // register set becomes nil then we can delete the edge
  for (auto edgeType : {EdgeType::WriteAfterRead, EdgeType::WriteAfterWrite}) {
    for (DependencyNodePtr checkNode : ordered(EdgeType::All)) {
      std::vector<DependencyNodePtr> removedNodes;
      updateEdges(checkNode, edgeType, originalReg, removedNodes, inNodes);
      if (!removedNodes.empty())
        removedDependencies.push_back({edgeType, checkNode});
    }
  }

  // Update the register sets for edges from any of the modified nodes, in case
  // we broke more than one anti-dependency
  for (auto edgeType : {EdgeType::WriteAfterRead, EdgeType::WriteAfterWrite}) {
    for (DependencyNodePtr checkNode : nodes) {
      std::vector<DependencyNodePtr> removedNodes;
      updateEdges(checkNode, edgeType, originalReg, removedNodes, notInNodes);
      // If we have removed an anti-dependent edge leaving the modified sub-graph then there
      // is now an anti-dependency between the original node we targeted and the node which has
      // just been removed here. The new anti-dependency effectively bypasses the subgraph we have
      // modified. We are adding this new edge in hope that we can break this dependency as well
      if (!removedNodes.empty()) {
        for (auto &edge : removedDependencies) {
          for (auto targetNode : removedNodes) {
            addNodeSuccessor(edge.second, targetNode, edge.first, originalReg);
            DEBUG(dbgs() << "Adding edge with register " << SRI->getName(originalReg) << " for N" << edge.second->number << "->N" << targetNode->number << "\n");
          }
        }
      }
    }
  }
}

#ifndef NDEBUG
void DependencyGraph::dump() {
  std::set<DependencyNodePtr> donelist;

  DEBUG(dbgs() << "Dependency graph:\n");
  DEBUG(dbgs() << "digraph {\n");

  for (DependencyNodePtr node : allNodes) {
    DEBUG(dbgs() << "N" << node->number << "[label=\"N" << node->number << "\\n");
    DEBUG(for (auto &instruction : node->instructions) { dbgs() << SII->getName(instruction->getOpcode()) << "\\n"; });
    DEBUG(dbgs() << node->getWeight(EdgeType::All) << "\"];\n");

    for (unsigned int type = (unsigned int) EdgeType::HighestPrecedence; type < (unsigned int) EdgeType::All; ++type) {
      EdgeType edgeType = (EdgeType) type;

      auto edges = node->getEdges(edgeType);

      for (auto edge : edges) {
        DEBUG(dbgs() << "N" << node->number << " -> N" << edge.node->number << "[color=\"");
        switch(edgeType) {
        case EdgeType::Memory:          DEBUG(dbgs() << "black");     break;
        case EdgeType::ReadAfterWrite:  DEBUG(dbgs() << "red");       break;
        case EdgeType::WriteAfterWrite: DEBUG(dbgs() << "webgreen");  break;
        case EdgeType::WriteAfterRead:  DEBUG(dbgs() << "royalblue"); break;
        default:                        DEBUG(dbgs() << "grey");      break;
        }
        
        std::string label;
        for (auto reg : edge.registers) {
          label += std::string(SRI->getName(reg)) + ",";
        }
        if (!label.empty())
          label.resize(label.size() - 1); // Remove the trailing ','

        DEBUG(dbgs() << "\",label=\"" << label << "\"];\n");
      }
    }
  }

  DEBUG(dbgs() << "}\n");
}
#endif // NDEBUG

/******************************************************************************
 *
 * Anti-dependency breaker and helper classes/functions
 * 
 ******************************************************************************/

//
// AntiDepBreaker private functions
//

bool AntiDepBreaker::usesIRF64(DependencyNodePtr node, unsigned int followRegister) const {
  for (auto &instruction : node->instructions) {
    for (auto &operand : instruction->operands()) {
      if (operand.isReg()) {
        auto registers = SRI->getPhysicalSHAVERegisters(operand.getReg());

        if ((registers[0] == followRegister && registers[1] != SHAVE::NoRegister) ||
            (registers[1] == followRegister && registers[0] != SHAVE::NoRegister))
          return true;
      }
    }
  }

  return false;
}

bool AntiDepBreaker::getChangeSet(DependencyNodePtr node, const unsigned int followRegister, std::vector<DependencyNodePtr> &nodes) {
  std::set<unsigned int> nodeNumbers;
  nodes.push_back(node);
  nodeNumbers.insert(node->number);
  std::queue<DependencyNodePtr> worklist;
  worklist.push(node);

  while (!worklist.empty()) {
    DependencyNodePtr currentNode = worklist.front();
    worklist.pop();

    if (usesIRF64(currentNode, followRegister))
      return false;

    for (MachineInstr * instruction : currentNode->instructions) {
      if (instruction->isCall() || instruction->isReturn()) // Don't break the calling convention
        return false;
      else if (SHAVEConflicts::check_isCMU_LUT(instruction->getOpcode())) // Don't break LUTs
        return false;
    }

    for (auto &edge : currentNode->getEdges(EdgeType::ReadAfterWrite)) {
      if (edge.registers.find(followRegister) != edge.registers.end()) {
        worklist.push(edge.node);

        if (edge.node->number > nodes.back()->number) {
          nodes.push_back(edge.node);
          nodeNumbers.insert(edge.node->number);
        }
        else {
          for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            if ((*it)->number > edge.node->number) {
              nodes.insert(it, edge.node);
              nodeNumbers.insert(edge.node->number);
              break;
            }
          }
        }
      }
    }
  }


  /* If there's a write-after-read edge directed outside of the candidate subgraph,
  and the register is being written by a predicated instruction, we conservatively
  ban this candidate. This is to prevent changing the register which might be used
  further down the graph and not necessarily overwritten by the predicated instruction */
  for (auto node : nodes) {
    for (auto &edge : node->getEdges(EdgeType::WriteAfterRead)) {
      if (edge.registers.find(followRegister) != edge.registers.end()) {
        if (edge.node->mayBePredicated() && nodeNumbers.find(edge.node->number) == nodeNumbers.end())
          return false;
      }
    }
  }

  DEBUG(dbgs() << "Found sub-graph for breaking:\n");
  DEBUG(for (auto node : nodes) dbgs() << "  N" << node->number << "\n");

  return true;
}

bool AntiDepBreaker::costlyEdge(DependencyNodePtr node1, DependencyNodePtr node2) const {
  auto weight = node2->getWeight(EdgeType::All);
  for (unsigned int type = (unsigned int) EdgeType::HighestPrecedence; type < (unsigned int) EdgeType::All; ++type)
    for (DependencyEdge &edge : node1->getEdges((EdgeType) type))
      if (edge.node != node2 && edge.node->getWeight(EdgeType::All) >= weight)
        return false;
  return true;
}

bool AntiDepBreaker::sameSubgraph(DependencyNodePtr node1, DependencyNodePtr node2) const {
  std::set<DependencyNodePtr> checkedNodes;
  std::set<DependencyNodePtr> reachableNodes;
  std::queue<DependencyNodePtr> worklist;
  worklist.push(node1);

  // Get the set of nodes which are reachable through node1
  while (!worklist.empty()) {
    DependencyNodePtr node = worklist.front();
    worklist.pop();
    reachableNodes.insert(node);

    for (DependencyEdge &successor : node->getEdges(EdgeType::ReadAfterWrite)) {
      if (checkedNodes.find(successor.node) == checkedNodes.end()) {
        checkedNodes.insert(successor.node);
        worklist.push(successor.node);
      }
    }

    for (DependencyEdge &successor : node->getEdges(EdgeType::Memory)) {
      if (checkedNodes.find(successor.node) == checkedNodes.end()) {
        checkedNodes.insert(successor.node);
        worklist.push(successor.node);
      }
    }
  }

  // Compare with the set of nodes reachable through node2
  worklist.push(node2);
  checkedNodes.clear();
  while (!worklist.empty()) {
    DependencyNodePtr node = worklist.front();
    worklist.pop();

    if (reachableNodes.find(node) != reachableNodes.end())
      return true;

    for (DependencyEdge &successor : node->getEdges(EdgeType::ReadAfterWrite)) {
      if (checkedNodes.find(successor.node) == checkedNodes.end()) {
        checkedNodes.insert(successor.node);
        worklist.push(successor.node);
      }
    }

    for (DependencyEdge &successor : node->getEdges(EdgeType::Memory)) {
      if (checkedNodes.find(successor.node) == checkedNodes.end()) {
        checkedNodes.insert(successor.node);
        worklist.push(successor.node);
      }
    }
  }

  return false;
}

bool AntiDepBreaker::getBreakable(DependencyNodePtr breakingNode,
                                  std::vector<DependencyNodePtr> &nodes,
                                  std::vector<unsigned int> &originalRegisters,
                                  std::vector<unsigned int> &newRegisters) {
  nodes.clear();
  originalRegisters.clear();
  newRegisters.clear();

  for (auto edgeType : {EdgeType::WriteAfterRead, EdgeType::WriteAfterWrite}) {
    for (auto node : dependencyGraph.ordered(edgeType)) {
      auto &edges = node->getEdges(edgeType);

      for (auto &edge : edges) {
        if (edge.edgeNotBreakable)
          continue;

        nodes.clear();
        auto &edgeRegisters = edge.registers;
        bool candidate = true;

        for (unsigned int edgeReg : edgeRegisters)
          if (liveRanges.isLiveOut(edgeReg))
            candidate = false;

        if (!candidate) {
          edge.edgeNotBreakable = true;
          continue;
        }

        if (!costlyEdge(node, edge.node) && sameSubgraph(node, edge.node)) {
          edge.edgeNotBreakable = true;
          continue;
        }

        // A predicated node will not include the initial def of the target register
        // This means if we attempt to break the anti-depenendency at this point, we
        // will also break the output dependency between the predicated write and its
        // predecessor write
        if (edge.node->mayBePredicated()) {
          edge.edgeNotBreakable = true;
          continue;
        }

        // If the first node is a tied-def on the register we want to break then we cannot
        // continue safely. We would need to break the initial def that this node is tied to
        unsigned int followRegister = *(edgeRegisters.begin());
        if (isTiedDef(edge.node, followRegister)) {
          if (edge.registers.size() == 1)
            edge.edgeNotBreakable = true;
          continue;
        }

        bool breakable = getChangeSet(edge.node, followRegister, nodes);

        if (!breakable) {
          if (edge.registers.size() == 1)
            edge.edgeNotBreakable = true;
          continue;
        }

        bool straddlesCall = false;
        for (MachineBasicBlock::instr_iterator it = nodes.front()->instructions.front()->getIterator(),
                                               end = nodes.back()->instructions.front()->getIterator();
             it != end && !straddlesCall; ++it) {
          if (it->isCall())
            straddlesCall = true;
        }

        // FIXME: Movidius - Once we have support for using callee-saved registers, straddlesCall should be 
        //                   passed into getReplacementRegisters so it knows to pick one of those registers.
        //                   Right now we simply cannot reassign the register safely
        if (straddlesCall) {
          edge.edgeNotBreakable = true;
          continue;
        }

        auto liveRange = LiveRange(nodes.front()->number, nodes.back()->number);
        unsigned int replacementRegister = liveRanges.getReplacementRegister(*(edgeRegisters.begin()), liveRange);
        if (replacementRegister == SHAVE::NoRegister) {
          edge.edgeNotBreakable = true;
          continue;
        }

        originalRegisters.insert(originalRegisters.begin(), edgeRegisters.begin(), edgeRegisters.end());
        newRegisters = originalRegisters;
        *(newRegisters.begin()) = replacementRegister;

        breakingNode = node; 

        return true;
      }
    }
  }

  return false;
}

bool AntiDepBreaker::isLastNode(DependencyNodePtr node, const std::vector<DependencyNodePtr> &nodes) const {
  for (DependencyEdge edge : node->getEdges(EdgeType::ReadAfterWrite))
    if (std::find(nodes.begin(), nodes.end(), edge.node) != nodes.end())
      return false;
  return true;
}

bool AntiDepBreaker::isTiedDef(DependencyNodePtr node, unsigned int reg) const {
  for (MachineInstr * instruction : node->instructions) {
    for (auto &operand : instruction->operands()) {
      if (!operand.isReg() || !operand.isDef())
        continue;

      auto regs = SRI->getPhysicalSHAVERegisters(operand.getReg());
      // TargetOpcode::KILL instructions are getting inserted (incorrectly) around SHAVE::INPUT_SWIZZLE*
      // for some reason, and the operands are not tied on these instructions. We need to handle this
      // specifically here so that we don't accidentally break real dependencies
      if (std::find(regs.begin(), regs.end(), reg) != regs.end())
        return operand.isTied() || instruction->isKill();
    }
  }

  return false;
}

//
// AntiDepBreaker public functions
//

bool AntiDepBreaker::breakAntiDependencies() {
  DependencyNodePtr breakingNode;
  std::vector<DependencyNodePtr> nodes;
  std::vector<unsigned int> originalRegisters;
  std::vector<unsigned int> newRegisters;
  bool modified = false;

  while (getBreakable(breakingNode, nodes, originalRegisters, newRegisters)) {
    DEBUG(dbgs() << "\nBreaking dependency\n");
    DEBUG(dbgs() << "Instructions before:\n");
    DEBUG(for (auto node : nodes) { for (auto &instruction : node->instructions) { dbgs() << "  "; instruction->dump(); } });

    for (unsigned int i = 0; i < originalRegisters.size(); ++i) {
      const unsigned int originalReg = originalRegisters[i];
      const unsigned int newReg = newRegisters[i];

      if (originalReg == newReg)
        continue;

      bool firstNode = true;
      for (DependencyNodePtr node : nodes) {
        for (auto &instruction : node->instructions) {
          for (MachineOperand &operand : instruction->operands()) {
            if (!operand.isReg())
              continue;

            unsigned int reg = operand.getReg();
            if (!SRI->regsOverlap(reg, originalReg))
              continue;

            // Only replace defs on the first node in the sub-graph. We are breaking
            // the dependency from this point onwards, so incoming registers must not
            // be modified
            if (firstNode && !(operand.isDef() || operand.isUndef()))
              continue;

            // Don't replace defs on the last node in the sub-graph, unless it is a
            // dead-def
            if (operand.isDef() && !operand.isDead() && isLastNode(node, nodes))
              continue;

            operand.setReg(SRI->getSubSHAVERegister(newReg, reg));
          }
        }

        firstNode = false;
      }

      liveRanges.update(nodes.front()->number, nodes.back()->number, originalReg, newReg);
      dependencyGraph.update(breakingNode, nodes, originalReg, newReg);

      modified = true;
    }

    DEBUG(dbgs() << "Instructions after:\n");
    DEBUG(for (auto node : nodes) { for (auto &instruction : node->instructions) { dbgs() << "  "; instruction->dump(); } });
  }

  return modified;
}

/******************************************************************************
 *
 * Anti-dependency breaker pass entry-point and helper classes/functions
 * 
 ******************************************************************************/

//
// Definition of public member functions of SHAVEAntiDepBreaker
//

void SHAVEAntiDepBreaker::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AAResultsWrapperPass>();
  MachineFunctionPass::getAnalysisUsage(AU);
}

bool SHAVEAntiDepBreaker::runOnMachineFunction(MachineFunction &MF) {
  DEBUG(dbgs() << "SHAVEAntiDepBreaker: Starting anti-dependence breaking for machine function " << MF.getName() << "\n");
  DEBUG(MF.dump());
  auto AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();
  
  const SHAVETargetMachine &TM = static_cast<const SHAVETargetMachine&>(MF.getTarget());
  auto SII = TM.getSubtargetImpl()->getInstrInfo();
  auto SRI = SII->getSHAVERegisterInfo();

  auto calleeSavedInfo = MF.getFrameInfo().getCalleeSavedInfo();
  std::set<unsigned int> availableCalleeSavedRegs;
  for (CalleeSavedInfo &info : calleeSavedInfo)
    availableCalleeSavedRegs.insert(info.getReg());

  bool modified = false;

  for (MachineBasicBlock &block : MF) {
    DEBUG(dbgs() << "\nStarting anti-dependency breaking for Basic Block # " << block.getNumber() << " \"" << block.getName() << "\"\n");
    DEBUG(block.dump());

    DEBUG(dbgs() << "\nBuilding dependency graph for Basic Block #" << block.getNumber() << " \"" << block.getName() << "\"\n");
    DependencyGraph graph(SII, SRI, AA);
    graph.generateGraph(block);
    DEBUG(graph.dump());

    DEBUG(dbgs() << "\nGenerating live-ranges for Basic Block #" << block.getNumber() << " \"" << block.getName() << "\"\n");
    LiveRanges liveRanges(block, SII, availableCalleeSavedRegs);
    DEBUG(liveRanges.dump(SII, SRI));

    DEBUG(dbgs() << "\nBreaking anti-dependencies\n");
    AntiDepBreaker breaker(liveRanges, graph, SRI);
    bool broken = breaker.breakAntiDependencies();
    modified |= broken;

    DEBUG(dbgs() << "\nAnti-dependency breaking complete\n");
#ifndef NDEBUG
    if (broken) {
      DEBUG(dbgs() << "Some dependencies broken, new graphs and liveRanges:\n");
      DEBUG(graph.dump());
      DEBUG(liveRanges.dump(SII, SRI));
      DEBUG(dbgs() << "\nBasic block after breaking:\n");
      DEBUG(block.dump());
    }
    else {
      DEBUG(dbgs() << "No dependencies broken\n");
    }
#endif // NDEBUG
  }

  DEBUG(dbgs() << "SHAVEAntiDepBreaker: Finished anti-dependence breaking for machine function " << MF.getName() << "\n");

  return modified; 
}
