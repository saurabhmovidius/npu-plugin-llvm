//===-- SHAVEPostRAScheduler.cpp - Post-RA Scheduler Pass -------*- C++ -*-===//
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

#define DEBUG_TYPE "shave-postra-scheduler"

#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineBranchProbabilityInfo.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetSchedule.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/InitializePasses.h"

#include "SHAVE.h"
#include "SHAVEInstrInfo.h"
#include "SHAVEPostRAScheduler.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <vector>

//
// Statistics for scheduling methods
//

STATISTIC(LegacyCount, "SHAVEPostRAScheduler - Number of blocks scheduling using the legacy method");
STATISTIC(ListCount, "SHAVEPostRAScheduler - Number of blocks scheduling using the list method");
STATISTIC(BreadthFirstCount, "SHAVEPostRAScheduler - Number of blocks scheduling using the breadth-first method");

//
// Initialisation code required by the Pass Manager
//

namespace {
class SHAVEPostRASched : public MachineFunctionPass {
public:
  static char ID;

  SHAVEPostRASched() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "SHAVE Post-RA Scheduler Pass";
  }

  void getAnalysisUsage(AnalysisUsage& AU) const override {
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<MachineBranchProbabilityInfo>();
    AU.addRequired<MachineLoopInfo>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  
  bool runOnMachineFunction(MachineFunction& MF) override {
    const SHAVETargetMachine& TM = static_cast<const SHAVETargetMachine&>(MF.getTarget());
    AliasAnalysis *AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();
    const SHAVEInstrInfo * SII = TM.getSubtargetImpl()->getInstrInfo();

    llvm::SHAVEPostRAScheduler scheduler(MF, AA, SII);
    return scheduler.run();
  }
};

}

char SHAVEPostRASched::ID = 0;

INITIALIZE_PASS_BEGIN(SHAVEPostRASched, "shavepostraschedulerpass", "SHAVE Post-RA Scheduler Pass", false, false)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(MachineBranchProbabilityInfo)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(SHAVEPostRASched, "shavepostraschedulerpass", "SHAVE Post-RA Scheduler Pass", false, false)

MachineFunctionPass* llvm::createSHAVEPostRASchedPass() {
  return new SHAVEPostRASched();
}

//
// Definition of private member functions of SHAVEPostRAScheduler
//

void SHAVEPostRAScheduler::scheduleGraphLegacy(SHAVEDependencyNodePtr startingNode, SHAVEPostRASchedule& schedule) {
  std::vector<SHAVEDependencyNodePtr> workList(1, startingNode);
  while (!workList.empty()) {
    SHAVEDependencyNodePtr currentNode = workList.back();

    // Sort the successor nodes of this node by weight (lowest to highest)
    auto compareNodes = [](SHAVEDependencyNodePtr node1, SHAVEDependencyNodePtr node2) {
      return (node1->weight < node2->weight);
    };
    std::stable_sort(currentNode->successors.begin(), currentNode->successors.end(), compareNodes);
    std::stable_sort(currentNode->outputSuccessors.begin(), currentNode->outputSuccessors.end(), compareNodes);

    // Add non-output successor nodes first
    for (SHAVEDependencyNodePtr successor : currentNode->successors) {
      if (schedule.isComplete(successor) && std::find(branchNodes.begin(), branchNodes.end(), successor) == branchNodes.end())
        continue;

      bool incompletePredecessor = false;
      for (SHAVEDependencyNodePtr predecessor : successor->predecessors)
        if (predecessor != currentNode && !schedule.isComplete(predecessor))
          incompletePredecessor = true;

      if (!incompletePredecessor)
        if (std::find(workList.begin(), workList.end(), successor) == workList.end() &&
            std::find(currentNode->outputSuccessors.begin(), currentNode->outputSuccessors.end(), successor) == currentNode->outputSuccessors.end())
          workList.push_back(successor);
    }

    // Then add the rest
    for (SHAVEDependencyNodePtr successor : currentNode->outputSuccessors) {
      if (schedule.isComplete(successor) && std::find(branchNodes.begin(), branchNodes.end(), successor) == branchNodes.end())
        continue;

      bool incompletePredecessor = false;
      for (SHAVEDependencyNodePtr predecessor : successor->predecessors)
        if (predecessor != currentNode && !schedule.isComplete(predecessor))
          incompletePredecessor = true;

      if (!incompletePredecessor)
        if (std::find(workList.begin(), workList.end(), successor) == workList.end())
          workList.push_back(successor);
    }

    if (schedule.isComplete(currentNode)) {
      workList.erase(std::find(workList.begin(), workList.end(), currentNode));
      continue;
    }

    bool incompletePredecessor = false;
    for (SHAVEDependencyNodePtr predecessor : currentNode->predecessors) {
      if (!schedule.isComplete(predecessor)) {
        workList.erase(std::find(workList.begin(), workList.end(), currentNode));
        incompletePredecessor = true;
        break;
      }
    }

    if (incompletePredecessor)
      continue;

    // Remove currentNode from the workList
    workList.erase(std::find(workList.begin(), workList.end(), currentNode));

    DEBUG(dbgs() << "SHAVEPostRAScheduler: Legacy: Scheduling node with instructions:\n");
    DEBUG(for (MachineInstr * instr : currentNode->instructions) {
      dbgs() << "  " << instrIDs[instr] << ":";
      instr->dump();
    });

    schedule.scheduleNode(currentNode);
  }
}

void SHAVEPostRAScheduler::scheduleGraphList(SHAVEDependencyNodePtr startingNode, SHAVEPostRASchedule& schedule) {
  for (auto it = allGeneratedNodes.rbegin(); it != allGeneratedNodes.rend(); ++it) {
    SHAVEDependencyNodePtr node = *it;

    DEBUG(dbgs() << "SHAVEPostRAScheduler: List: Scheduling node with instructions:\n");
    DEBUG(for (MachineInstr* instr : node->instructions) {
      dbgs() << "  " << instrIDs[instr] << ":";
      instr->dump();
    });

    if (!schedule.isComplete(node))
      schedule.scheduleNode(node);
  }
}

void SHAVEPostRAScheduler::scheduleGraphBreadthFirst(SHAVEDependencyNodePtr startingNode, SHAVEPostRASchedule& schedule) {
  std::vector<SHAVEDependencyNodePtr> depth;
  depth.push_back(startingNode);

  auto incompletePredecessor = [&schedule](SHAVEDependencyNodePtr node) {
    for (SHAVEDependencyNodePtr predecessor : node->predecessors)
      if (!schedule.isComplete(predecessor))
        return true;
    return false;
  };

  while (!depth.empty()) {
    // First schedule all nodes at this depth

    DEBUG(dbgs() << "SHAVEPostRAScheduler: Breadth-first: Scheduling next depth:\n");

    // Sort the nodes by latency, or weight if latencies are equal (highest to lowest)
    auto compareNodes = [](SHAVEDependencyNodePtr node1, SHAVEDependencyNodePtr node2) {
      if (node1->latency > node2->latency)
        return true;
      else if (node1->latency == node2->latency && node1->weight > node2->weight)
        return true;
      return false;
    };
    // Maintain original order for sets of equal latency/weight nodes. This means that
    // the first node schedule at the previous depth will have all of its successors
    // scheduled first at this depth
    std::stable_sort(depth.begin(), depth.end(), compareNodes);

    for (SHAVEDependencyNodePtr node : depth) {
      DEBUG(dbgs() << "SHAVEPostRAScheduler: Breadth-first: Scheduling node with instructions:\n");
      DEBUG(for (MachineInstr* instr : node->instructions) {
        dbgs() << "  " << instrIDs[instr] << ":";
        instr->dump();
      });

      if (!schedule.isComplete(node))
        schedule.scheduleNode(node);
    }

    // Then setup all of available successors at the next depth

    std::vector<SHAVEDependencyNodePtr> nextDepth;
    for (SHAVEDependencyNodePtr node : depth)
      for (SHAVEDependencyNodePtr successor : node->successors)
        if (!incompletePredecessor(successor) && std::find(nextDepth.begin(), nextDepth.end(), successor) == nextDepth.end())
          nextDepth.push_back(successor);

    depth = nextDepth;
  }
}

void SHAVEPostRAScheduler::setupLSUFunctionalUnits() {
  bool balanceLSUUsage = false;
  unsigned int loadLSU = SHAVE::LSU0;
  unsigned int storeLSU = SHAVE::LSU1;
  unsigned int volatileLoadLSU = SHAVE::LSU1;
  unsigned int volatileStoreLSU = SHAVE::LSU1;

  if (SHAVEOptions::LSULoadPolicy == SHAVEOptions::PreferLSU1 || SHAVEOptions::LSULoadPolicy == SHAVEOptions::AlwaysUseLSU1)
    loadLSU = SHAVE::LSU1;

  if (SHAVEOptions::LSUStorePolicy == SHAVEOptions::PreferLSU0 || SHAVEOptions::LSUStorePolicy == SHAVEOptions::AlwaysUseLSU0)
    storeLSU = SHAVE::LSU0;

  if (SHAVEOptions::LSUVolatileLoadStorePolicy == SHAVEOptions::AlwaysUseLSU0) {
    volatileLoadLSU = SHAVE::LSU0;
    volatileStoreLSU = SHAVE::LSU0;
  }

  // The In-CMX-Tile ITI arbitration causes stalls if the LSU stores are not
  // balanced across LSU0/1. Though this applies only to inter-tile traffic,
  // we need to defensively do this for all stores
  if (SII->hasFeature(SHAVE::HasITIArbitrationInCMXTile_Feature) &&
      SHAVEOptions::LSUStorePolicy == SHAVEOptions::FollowNormalLSUPolicy)
    balanceLSUUsage = true;

  for (MachineBasicBlock &block : *currentFunction) {
    for (MachineInstr &instr : block.instrs()) {
      unsigned int opcode = instr.getOpcode();

      if (check_usesLSU0(opcode) || check_usesLSU1(opcode)) {
        // Special case - do not alter the selected LSU if the instruction has both 'mayLoad' AND 'mayStore'
        if (!(instr.mayLoad(MachineInstr::IgnoreBundle) && instr.mayStore(MachineInstr::IgnoreBundle))) {
          const int functionalUnitOperandIndex = SII->getFUnitOperandIndex(&instr);

          if (functionalUnitOperandIndex < 0)
            llvm_unreachable("LSU instruction is missing functional unit operand");

          if (!SHAVEOptions::HasLSU1) {
            instr.getOperand(functionalUnitOperandIndex).setImm(SHAVE::LSU0);
          }
          else if (instr.memoperands_empty() || !instr.hasOrderedMemoryRef()) {
            if (instr.mayLoad(MachineInstr::IgnoreBundle))
              instr.getOperand(functionalUnitOperandIndex).setImm(loadLSU);
            else if (instr.mayStore(MachineInstr::IgnoreBundle)) {
              instr.getOperand(functionalUnitOperandIndex).setImm(storeLSU);

              // Use the LSUs alternately for balanced usage
              if (balanceLSUUsage)
                storeLSU =
                    (storeLSU == SHAVE::LSU1) ? SHAVE::LSU0 : SHAVE::LSU1;
            }
          }
          else {
            if (instr.mayLoad(MachineInstr::IgnoreBundle))
              instr.getOperand(functionalUnitOperandIndex).setImm(volatileLoadLSU);
            else if (instr.mayStore(MachineInstr::IgnoreBundle))
              instr.getOperand(functionalUnitOperandIndex).setImm(volatileStoreLSU);
          }
        }
      }
    }
  }

  bool changeMade;

  do {
    changeMade = false;

    for (MachineBasicBlock &block : *currentFunction) {
      for (MachineInstr &instr : block.instrs()) {
        unsigned int opcode = instr.getOpcode();
        if (opcode == SHAVE::PEU_PVL0_32 || opcode == SHAVE::PEU_PVL0_16 || opcode == SHAVE::PEU_PVL0_8) {
          MachineInstr * store = nullptr;
          if (instr.isBundledWithSucc())
            store = &*std::next(instr.getIterator());
          else
            store = &*std::prev(instr.getIterator());

          unsigned int opcodeLSU1 = 0;
          switch (opcode) {
          case SHAVE::PEU_PVL0_32: opcodeLSU1 = SHAVE::PEU_PVL1_32; break;
          case SHAVE::PEU_PVL0_16: opcodeLSU1 = SHAVE::PEU_PVL1_16; break;
          case SHAVE::PEU_PVL0_8:  opcodeLSU1 = SHAVE::PEU_PVL1_8; break;
          }

          if (store->memoperands_empty() || !store->hasOrderedMemoryRef()) {
            if (storeLSU == SHAVE::LSU1) {
              MachineInstrBuilder newPredicate = BuildMI(block, &instr, instr.getDebugLoc(), SII->get(opcodeLSU1)).add(instr.getOperand(0));
              instr.eraseFromBundle();
              newPredicate->bundleWithSucc();
              changeMade = true;
              break;
            }
          }
          else {
            if (volatileStoreLSU == SHAVE::LSU1) {
              MachineInstrBuilder newPredicate = BuildMI(block, &instr, instr.getDebugLoc(), SII->get(opcodeLSU1)).add(instr.getOperand(0));
              instr.eraseFromBundle();
              newPredicate->bundleWithSucc();
              changeMade = true;
              break;
            }
          }
        }
      }

      if (changeMade)
        break;
    }
  } while (changeMade);
}

void SHAVEPostRAScheduler::replaceJMPwithBRA() {
  const double averageInstructionSize = SHAVEOptions::AverageInstructionSize;
  // FIXME: Movidius - should we also make this constant tunable with an option?
  const double maxBasicBlockGap = 7.0;

  std::map<int, int> relativeBBIndexMap;
  int relativeBBIndex = 0;

  // Create a map from the BB number to its relative order in the function.  See
  // Bugzilla #28861 for rationale.
  for (MachineBasicBlock &block : *currentFunction)
    relativeBBIndexMap[block.getNumber()] = relativeBBIndex++;

  for (MachineBasicBlock &block : *currentFunction) {
    bool changeMade = true;

    while (changeMade) {
      changeMade = false;
      MachineInstr *ldih = nullptr;
      MachineInstr *ldil = nullptr;

      for (MachineInstr &instr : block.instrs()) {
        if (instr.getOpcode() == SHAVE::LSU_LDIL_Label) {
          ldil = &instr;
        }
        else if (instr.getOpcode() == SHAVE::LSU_LDIH_Label) {
          ldih = &instr;
        }
        else if (instr.getOpcode() == SHAVE::BRU_JMP || instr.getOpcode() == SHAVE::BRU_JMPcc) {
          if (ldil != nullptr && ldih != nullptr) {
            MachineFunction::iterator begin, end;
            MachineBasicBlock *targetBlock = ldih->getOperand(2).getMBB();
            int blockNumber = relativeBBIndexMap[block.getNumber()];
            int targetBlockNumber = relativeBBIndexMap[targetBlock->getNumber()];

            if (targetBlockNumber < blockNumber) {
              // Counting between the start of targetBlock and the end of this block
              begin = targetBlock->getIterator();
              end = block.getIterator();
            }
            else if (targetBlockNumber == blockNumber) {
              // Counting just this block
              begin = block.getIterator();
              end = std::next(block.getIterator());
            }
            else {
              // Counting between the start of the next block and the start of targetBlock
              begin = std::next(block.getIterator());
              end = targetBlock->getIterator();
            }

            // Count the number of instructions between this branch instruction and its targetBlock basic block
            unsigned int numberOfInstructions = 0;
            unsigned int numberOfBlocks = 0;

            for (MachineFunction::iterator blockIt = begin; blockIt != end; ++blockIt) {
              if (blockIt->size()) {
                numberOfBlocks++;

                for (MachineInstr &instrIt : (*blockIt).instrs())
                  if (!instrIt.isDebugValue() && !instrIt.isCFIInstruction() && !instrIt.isPosition())
                    numberOfInstructions++;
              }
            }

            // If the number of instructions * the average size of those instructions is less than
            // the maximum number of bytes allowed by BRU.BRA, then replace this BRU.JMP with a BRU.BRA
            double maxBRARange = SII->getBRARange();

            if (((numberOfInstructions * averageInstructionSize) + (numberOfBlocks * maxBasicBlockGap)) < maxBRARange) {
              MachineInstrBuilder newBranch = BuildMI(block, std::next(instr.getIterator()), instr.getDebugLoc(), SII->get(SHAVE::BRU_BRA))
                                                 .addMBB(targetBlock).add(instr.getOperand(1)).add(instr.getOperand(2));

              if (instr.isBundled())
                newBranch->bundleWithPred();

              ldil->eraseFromBundle();
              ldih->eraseFromBundle();
              instr.eraseFromBundle();

              changeMade = true;
              break;
            }
          }

          ldil = nullptr;
          ldih = nullptr;
        }
      }
    }
  }
}

#ifndef NDEBUG
//
// Definition of overridden protected member functions from SHAVESchedulerBase
//

void SHAVEPostRAScheduler::generateAndDumpInstrIDs(raw_ostream &out) {
  unsigned int id = 0;
  for (MachineBasicBlock &MBB : *currentFunction) {
    out << "\nBB#" << MBB.getNumber() << ":\n";
    for (MachineInstr &MI : MBB.instrs()) {
      out << id << ": "; MI.print(out);
      instrIDs[&MI] = id++;
    }
  }
}
#endif // NDEBUG

//
// Definition of public member functions of SHAVEPostRAScheduler
//

bool SHAVEPostRAScheduler::run() {
  DEBUG(dbgs() << "SHAVEPostRAScheduler: Starting scheduling for machine function " << currentFunction->getName() << "\n");

  const SHAVETargetMachine &TM = static_cast<const SHAVETargetMachine&>(currentFunction->getTarget());

  // Rewrite the default LSU used for load and store instructions
  setupLSUFunctionalUnits();

  if (!SHAVEOptions::EnablePostRAOptimisingScheduler ||
      (TM.getOptLevel() == CodeGenOptLevel::None)) {
    //
    // Run the non-optimising scheduler for optimisation level "-O0"
    //
    unsigned int minLSUStoreLatency = std::max(SHAVEOptions::AliasedStoreDistance, SHAVEOptions::NPU4LoadStoreDistance);
    unsigned int minLSULoadLatency = std::max(minLSUStoreLatency, SII->getLoadLatency());

    for (MachineBasicBlock &block : *currentFunction) {
      std::vector<MachineInstr *> instructions;

      // Get a list of all non-debug instructions in this basic block
      for (MachineInstr &instr : block.instrs())
        if (!instr.isDebugValue() && !instr.isCFIInstruction() && !instr.isPosition())
          instructions.push_back(&instr);

      unsigned int latency = 0;

      // Insert the appropriate number of NOPs after each instruction
      for (MachineInstr * instr : instructions) {
        latency = std::max(SII->GetSchedMaxLatency(instr), latency);

        if (instr->mayStore())
          latency = std::max(latency, minLSUStoreLatency);
        if (instr->mayLoad())
          latency = std::max(latency, minLSULoadLatency);

        if (!instr->isBundledWithSucc()) {
          while (latency-- != 0)
            BuildMI(block, std::next(instr->getIterator()), instr->getDebugLoc(), SII->get(SHAVE::NOP));
          latency = 0;
        }
      }
    }
  } else {
    //
    // Run the optimising scheduler for all optimisation levels that are not "-O0"
    //

    // Try and replace BRU.JMP instructions with equivalent BRU.BRA instructions when appropriate
    if (!SHAVEOptions::DisableJMPtoBRAPeephole && !SHAVEOptions::DisablePeepholeOptimiser)
      replaceJMPwithBRA();

    DEBUG(dbgs() << "SHAVEPostRAScheduler: Starting instruction scheduling\n");
    for (MachineBasicBlock& block : *currentFunction) {
      // Generate the dependencies graphs for this block
      DEBUG(dbgs() << "SHAVEPostRAScheduler: Generating dependency graph for block BB#" << block.getNumber() << "\n");
      generateDependencyGraphs(block);
      DEBUG(dumpDependencyGraphs(dbgs(), block.getNumber() == 0));

      // Setup the initial schedules for this function. The schedules will be empty except for branch instructions
      DEBUG(dbgs() << "SHAVEPostRAScheduler: Initializing schedules for block\n");
      SmallVector<SHAVEPostRASchedule, (size_t)SchedulingMethod::NumMethods> schedules;
      for (int method = (int)SchedulingMethod::Legacy; method < (int)SchedulingMethod::NumMethods; ++method) {
        schedules.emplace_back(*this, SII, &block);
        schedules.back().initialiseSchedule();
      }

      bool first = true;
      while(!dependencyGraphs.empty()) {
        SHAVEDependencyNodePtr graph = dependencyGraphs.front();
        for (int method = (int) SchedulingMethod::Legacy; method < (int) SchedulingMethod::NumMethods; ++method) {
          switch ((SchedulingMethod) method) {
          case SchedulingMethod::Legacy:
            scheduleGraphLegacy(graph, schedules[method]);
            break;
          case SchedulingMethod::List:
            if (first) // List doesn't use the graph(s)
              scheduleGraphList(graph, schedules[method]);
            break;
          case SchedulingMethod::BreadthFirst:
            scheduleGraphBreadthFirst(graph, schedules[method]);
            break;
          default:
            llvm_unreachable("Unrecognised scheduling method");
          }

          static_assert((int) SchedulingMethod::Legacy == 0, "Legacy needs to be the first method used for this option to work");
          if (SHAVEOptions::EnableSchedulingMethod == SHAVEOptions::Scheduling::LegacyPostRA ||
              SHAVEOptions::EnableSchedulingMethod == SHAVEOptions::Scheduling::LegacyBoth)
            break;
        }

        first = false;
        dependencyGraphs.erase(dependencyGraphs.begin());
      }

      SHAVEPostRASchedule *bestSchedule = &schedules.front();
      SchedulingMethod bestMethod = SchedulingMethod::Legacy;
      DEBUG(dbgs() << "Cycles per method:\n");
      for (int method = (int)SchedulingMethod::Legacy; method < (int)SchedulingMethod::NumMethods; ++method) {
        DEBUG(
          std::string methodStr;
          switch((SchedulingMethod) method) {
          case SchedulingMethod::Legacy:       methodStr = "Legacy";        break;
          case SchedulingMethod::List:         methodStr = "List";          break;
          case SchedulingMethod::BreadthFirst: methodStr = "Breadth-First"; break;
          default: llvm_unreachable("Unknown scheduling method in debug output");
          }
          dbgs() << "  " << methodStr << ": " << schedules[method].getNumCycles() << "\n";
        );
        if (schedules[method].getNumCycles() < bestSchedule->getNumCycles()) {
          bestSchedule = &schedules[method];
          bestMethod = (SchedulingMethod) method;
        }
      }

      switch (bestMethod) {
      case SchedulingMethod::Legacy:
        ++LegacyCount;
        DEBUG(dbgs() << "Used Legacy Scheduling ");
        break;
      case SchedulingMethod::List:
        ++ListCount;
        DEBUG(dbgs() << "Used List Scheduling ");
        break;
      case SchedulingMethod::BreadthFirst:
        ++BreadthFirstCount;
        DEBUG(dbgs() << "Used Breadth-First Scheduling ");
        break;
      default:
        llvm_unreachable("Unrecognised scheduling method");
      }

      DEBUG(dbgs() << "with " << bestSchedule->getNumCycles() << " cycles\n");

      bestSchedule->emitSchedule();

      cleanup();
    }

    DEBUG(dbgs() << "SHAVEPostRAScheduler: Finished scheduling for machine function " << currentFunction->getName() << "\n");
  }

  return true;
}
