//===-- SHAVEAntiDependencyBreaker.h - Pre-Scheduling Pass ------*- C++ -*-===//
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

#ifndef SHAVE_ANTI_DEPENDENCY_BREAKER_H
#define SHAVE_ANTI_DEPENDENCY_BREAKER_H (1)

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

#include "SHAVE.h"
#include "SHAVEInstrInfo.h"
#include "SHAVELiveRanges.h"
#include "SHAVERegisterInfo.h"
#include "SHAVETargetMachine.h"

#include <bitset>
#include <initializer_list>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "SHAVEGenConflicts.inc"

using namespace llvm;
using namespace SHAVELiveRanges;

namespace SHAVEAntiDependencyBreaker {
  class DependencyNode; // Forward declaration for use in DependencyEdge
  typedef std::shared_ptr<DependencyNode> DependencyNodePtr;

  class DependencyEdge {
  public:
    template<typename ...Registers>
    DependencyEdge(DependencyNodePtr node, Registers... registers) :
      node(node), registers(std::initializer_list<unsigned int>({registers...})), edgeNotBreakable(false) {}

    void merge(const DependencyEdge &edge);

    // Required for std::find
    bool operator==(const DependencyEdge &edge) const;

    // The Dependency Node that this edge points to
    DependencyNodePtr node;
    // The set of registers which define this edge. For example, if this is a read-after-write edge
    // then this is the set of registers which are read by the node which owns this edge, defined
    // by the node pointed to by this edge.
    // This can be more than one register on SHAVE, e.g. for CMU.ALIGNVEC which defines two registers
    // This set will be empty for memory edges
    std::set<unsigned int> registers;
    // Bool which is set when an edge is not breakable, here to save time rechecking edges again
    bool edgeNotBreakable;
  };

  enum class EdgeType {
    HighestPrecedence = 0,
    ReadAfterWrite = HighestPrecedence,
    Memory,
    WriteAfterWrite,
    WriteAfterRead,
    All
  };

  class DependencyNode {
  private:
    //
    // Private member variables
    //
    typedef std::vector<DependencyEdge> Edges;
    std::array<Edges, (size_t) EdgeType::All> edges;
    std::array<uint64_t, (size_t) EdgeType::All + 1> edgeWeights {0}; // Index EdgeType::All is total weight

    //
    // Private member functions
    //

    bool hasEdgeWithType(DependencyEdge &edge, EdgeType type) const;
    void updateEdge(DependencyEdge &edge, EdgeType type);
    void upgradeEdge(DependencyEdge &edge, EdgeType existingType, EdgeType newType);
    void insertEdge(DependencyEdge &edge, EdgeType type);

  public:
    //
    // Public member variables
    //
    std::vector<MachineInstr *> instructions;
    unsigned int number;
    bool hasLoad, hasStore;
    MachineMemOperand * memoryOperand;
    bool usesPEU;

    //
    // Public member functions
    //
    DependencyNode(std::vector<MachineInstr *> &nodeInstructions, unsigned int number)
        : instructions(nodeInstructions), number(number) {
      hasLoad = false;
      hasStore = false;
      memoryOperand = nullptr;
      usesPEU = false;
      for (auto instruction : nodeInstructions) {
        hasLoad |= instruction->mayLoad();
        hasStore |= instruction->mayStore();
        if (instruction->hasOneMemOperand())
          memoryOperand = *(instruction->memoperands_begin());
        
        usesPEU |= SHAVEConflicts::check_usesPEU(instruction->getOpcode());
      }
    }

    template<typename ...Registers>
    void addSuccessor(DependencyNodePtr node, EdgeType edgeType, Registers... registers);
    void calculateWeights(unsigned int latency);
    uint64_t getWeight(EdgeType edgeType);
    Edges& getEdges(EdgeType edgeType);

    bool mayLoad() { return hasLoad; }
    bool mayStore() { return hasStore; }
    MachineMemOperand * getMemoryOperand() { return memoryOperand; }
    bool mayBePredicated() { return usesPEU; }
  };
  
  class DependencyGraph {
  private:
#ifndef NDEBUG
    std::vector<DependencyNodePtr> allNodes;
#endif // NDEBUG
    std::array<std::vector<DependencyNodePtr>, (size_t) EdgeType::All+1> orderedNodes;

    const SHAVEInstrInfo *SII;
    const SHAVERegisterInfo *SRI;
    AliasAnalysis *AA;

    template<typename ...Registers>
    void addNodeSuccessor(DependencyNodePtr node, DependencyNodePtr successor, EdgeType edgeType, Registers... registers);
    void updateEdges(DependencyNodePtr node, EdgeType edgeType, unsigned int removedReg, std::vector<DependencyNodePtr> &removedNodes, const std::function<bool(DependencyNodePtr)> &check);

  public:
    DependencyGraph(const SHAVEInstrInfo *SII, const SHAVERegisterInfo *SRI, AliasAnalysis *AA) : 
      SII(SII), SRI(SRI), AA(AA) {}

    void generateGraph(MachineBasicBlock &block);
    void update(DependencyNodePtr node, std::vector<DependencyNodePtr> &nodes, unsigned int originalReg, unsigned int newReg);

    typedef iterator_range<std::vector<DependencyNodePtr>::iterator> OrderedIterator;
    OrderedIterator ordered(EdgeType type) { return OrderedIterator(orderedNodes[(size_t) type].begin(), orderedNodes[(size_t) type].end()); }
#ifndef NDEBUG
    void dump();
#endif // NDEBUG
  };

  class AntiDepBreaker {
  private:
    LiveRanges &liveRanges;
    DependencyGraph &dependencyGraph;

    const SHAVERegisterInfo *SRI;

    bool usesIRF64(DependencyNodePtr node, unsigned int followRegister) const;
    bool getChangeSet(DependencyNodePtr node, const unsigned int followRegister, std::vector<DependencyNodePtr> &nodes);
    bool costlyEdge(DependencyNodePtr node1, DependencyNodePtr node2) const;
    bool sameSubgraph(DependencyNodePtr node1, DependencyNodePtr node2) const;
    bool getBreakable(DependencyNodePtr node, std::vector<DependencyNodePtr> &nodes, std::vector<unsigned int> &originalRegister, std::vector<unsigned int> &newRegister);
    bool isLastNode(DependencyNodePtr node, const std::vector<DependencyNodePtr> &nodes) const;
    bool isTiedDef(DependencyNodePtr node, unsigned int reg) const;

  public:
    AntiDepBreaker(LiveRanges &liveRanges, DependencyGraph &dependencyGraph, const SHAVERegisterInfo *SRI)
      : liveRanges(liveRanges), dependencyGraph(dependencyGraph), SRI(SRI) {}

    bool breakAntiDependencies();
  };
} // SHAVEAntiDependencyBreaker

namespace {
  class SHAVEAntiDepBreaker : public MachineFunctionPass {
  public:
    static char ID;

    SHAVEAntiDepBreaker() : MachineFunctionPass(ID) {}

    StringRef getPassName() const override {
      return "SHAVE Anti-Dependency Breaker Pass";
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override;
    bool runOnMachineFunction(MachineFunction &MF) override;
  };
}

#endif // SHAVE_ANTI_DEPENDENCY_BREAKER_H
