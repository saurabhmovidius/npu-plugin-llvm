//===-- SHAVESchedulerBase.h - Scheduler Pass Base --------------*- C++ -*-===//
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

#ifndef SHAVE_SCHEDULER_BASE_H
#define SHAVE_SCHEDULER_BASE_H (1)

#include <unordered_set>
#include <unordered_map>

#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include "MCTargetDesc/SHAVEMCTargetDesc.h"
#include "SHAVE.h"
#include "SHAVEInstrInfo.h"
#include "SHAVERegisterInfo.h"
#include "SHAVETargetMachine.h"

using namespace llvm;
using namespace SHAVE;
using namespace SHAVEConflicts;

namespace llvm {

class SHAVEDependencyNode;
typedef SHAVEDependencyNode * SHAVEDependencyNodePtr;

typedef SmallVector<MachineInstr *, 4u> SHAVEInstructionBundle;
typedef int SHAVECycle;

class SHAVESchedulePosition {
private:
  enum { uninitialised = -1 };

public:
  int branchPosition;
  SHAVECycle cycle;

  SHAVESchedulePosition(int branchPosition = uninitialised, SHAVECycle cycle = 0) : branchPosition(branchPosition), cycle(cycle) {}
  bool isInitialised() { return branchPosition != uninitialised; }
  bool operator==(const SHAVESchedulePosition &other) const { return branchPosition == other.branchPosition && cycle == other.cycle; }
  bool operator!=(const SHAVESchedulePosition &other) const { return !(*this == other); }
};

/// A single node in a dependency graph
///
/// This class represents a single dependency node in a dependency graph. It stores
/// all information associated with a node before, during and after scheduling has run.
class SHAVEDependencyNode {
public:
  SHAVEInstructionBundle instructions;
  SHAVEInstructionBundle debugInstructions;
  unsigned int weight = 0;
  unsigned int latency = 0;
  unsigned int number = 0; // Identifier used for quicker look-up

  std::vector<SHAVEDependencyNodePtr> predecessors;
  std::vector<SHAVEDependencyNodePtr> successors;
  std::vector<SHAVEDependencyNodePtr> outputSuccessors; // subset of successors

  bool isBundled = false;
  bool hasDownstreamI0 = false;
  bool definesI0 = false;
  bool isCall = false;
  bool isReturn = false;
  bool isPrefetch = false;

#ifndef NDEBUG
  void dump();
#endif // NDEBUG

  SHAVEDependencyNode(unsigned int number) : number(number) {}
};

class SHAVESchedulerBase {
private:
  // Private member types
  typedef std::vector<SHAVEDependencyNodePtr> Graphs;

  struct NodeRegisterData {
    std::set<unsigned int> defs;
    std::set<unsigned int> uses;
    std::unordered_set<unsigned int> kills;

    // Implicit uses/defs on branch nodes should not be marked as "output successors" so that
    // the node's defining these registers can be scheduled in the branch's delay slots
    std::unordered_set<unsigned int> explicits;
    std::unordered_set<unsigned int> deads;

    void clear() {
      defs.clear();
      uses.clear();
      kills.clear();
      explicits.clear();
      deads.clear();
    }
  };

  struct GraphBuilderData {
    // Ephermal data used only during graph generation
    std::map<unsigned int, SHAVEDependencyNodePtr> defNodes;
    std::map<unsigned int, std::vector<SHAVEDependencyNodePtr> > useNodes;
    std::map<unsigned int, std::vector<SHAVEDependencyNodePtr> > deadDefNodes;
    std::map<SHAVEDependencyNodePtr, SHAVEDependencyNodePtr> latestOutputDependencies;
    std::vector<SHAVEDependencyNodePtr> memoryOperations;
    SHAVEDependencyNodePtr lastBarrier = nullptr;
    SHAVEDependencyNodePtr lastBumpedMemOp = nullptr;

    std::set<unsigned int> ignoreRegisters; // Post-RA only
    NodeRegisterData latestRegData; // Register data for the latest generated node
  };

  // Private member variables
  std::unordered_map<MachineInstr *, bool> readOnlyCache; // Cache for results of isReadOnly

  // Private member functions
  void assignNodeWeights();
  void getNodeRegisterData(SHAVEDependencyNodePtr node,
                           GraphBuilderData& data);
  void generateIgnoreRegisters(MachineBasicBlock &block,
                               GraphBuilderData& data);
  void addUseRegDependencies(SHAVEDependencyNodePtr node,
                             GraphBuilderData &data);
  void addDefRegDependencies(SHAVEDependencyNodePtr node,
                             GraphBuilderData& data);
  void addMemoryDependencies(SHAVEDependencyNodePtr node,
                             GraphBuilderData& data);
  void addBranchDependencies(SHAVEDependencyNodePtr node,
                             GraphBuilderData& data);
  void addBarrierDependencies(SHAVEDependencyNodePtr node,
                              GraphBuilderData& data);

protected:
  //
  // protected variables
  //
  // Dependency graphs
  std::vector<SHAVEDependencyNodePtr> dependencyGraphs;
  // All generated nodes for the current graphs
  std::vector<SHAVEDependencyNodePtr> allGeneratedNodes;
  // Map containing a vector of debug instructions that should be placed at the start of a basic block
  SHAVEInstructionBundle entryDebugInstructions;
  // Vector of all branch nodes in the current function
  std::vector<SHAVEDependencyNodePtr> branchNodes;
#ifndef NDEBUG
  std::map<MachineInstr *, unsigned int> instrIDs;
#endif // NDEBUG
  bool isPreRA = false;

  const SHAVEInstrInfo *SII = nullptr;
  const SHAVERegisterInfo *SRI = nullptr;
  AliasAnalysis *AA = nullptr;

  //
  // protected functions
  //
  SHAVESchedulerBase() {}
  SHAVESchedulerBase(const SHAVEInstrInfo* SII, AliasAnalysis* AA) : SII(SII), SRI(SII->getSHAVERegisterInfo()), AA(AA) {}

  SmallVector<unsigned int> getRegisterAliases(unsigned int reg);
  void addSuccessorToNode(SHAVEDependencyNodePtr node,
                          SHAVEDependencyNodePtr newSuccessor);
  void addOutputSuccessorToNode(SHAVEDependencyNodePtr node,
                                SHAVEDependencyNodePtr newSuccessor);
  void generateDependencyGraphs(MachineBasicBlock &block);
  bool doMemoryRangesOverlap(int offset1,
                             int offset2,
                             int size1,
                             int size2) const;
  bool memoryOperandUsesStack(MachineInstr *instr,
                              unsigned int ptrOpIdx,
                              unsigned int &stackOffset) const;
  bool isReadOnlyAccess(MachineInstr *instr);
  MachineMemOperand *extractMemoryOperand(MachineInstr *instr,
                                          bool &usesStack,
                                          unsigned int &stackOffset) const;
  MachineBasicBlock *getBranchTarget(MachineInstr *branchInstr);
  void cleanup();

#ifndef NDEBUG
  void dumpDependencyGraphs(raw_ostream &out, bool dumpIDs);
  virtual void generateAndDumpInstrIDs(raw_ostream &out) = 0;
#endif // NDEBUG

public:
  //
  // public functions
  //
  AliasResult compareMemoryAccesses(MachineInstr* instr1, MachineInstr* instr2); // Used by SHAVESchedulerConflicts

  friend class SHAVEPostRASchedule; // avoid unnecessary data duplication
};
}

#endif // SHAVE_SCHEDULER_BASE_H
