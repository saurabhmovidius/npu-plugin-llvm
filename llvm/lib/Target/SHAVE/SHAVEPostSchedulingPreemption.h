//===-- SHAVEPostSchedulingPreemption.h - NPU5 Preemption  -*- C++ -*------===//
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
//
// Inserts the code needed to enable/disable code protection on NPU5+ for
// preemption support
//
//===----------------------------------------------------------------------===//

#ifndef SHAVEPostSchedulingPreemption_H
#define SHAVEPostSchedulingPreemption_H (1)

#include "llvm/Pass.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

#include "SHAVETargetMachine.h"
#include "SHAVELiveRanges.h"

#include <unordered_map>

using namespace SHAVELiveRanges;

namespace llvm {
  namespace SHAVEPostSchedulingPreemptionNS {

    enum class TrackedFUs {
      CMU = 0,
      LSU0,
      LSU1,
      PEU, // Only predicating instructions that cannot be modified
      NumFunctionalUnits
    };

    class RangeCycleData {
    private:
      typedef std::set<uint64_t> Ports;

      const SHAVEInstrInfo * SII;

      MachineBasicBlock &block;
      std::vector<std::bitset<(size_t) TrackedFUs::NumFunctionalUnits>> functionalUnitUsage;
      std::vector<Ports> portUsage;
      std::unordered_map<const MachineInstr *, Ports> portsCache;

    public:
      unsigned int start, end;
      unsigned int reg;
      bool moveBackwards = true;

      RangeCycleData(const SHAVEInstrInfo * SII, MachineBasicBlock &block, unsigned int start, unsigned int end, unsigned int reg);

      bool isUnitAvailable(TrackedFUs unit, unsigned int cycle) const;
      bool anyPortClashes(const MachineInstr &instruction, unsigned int cycle);
      void insert(const MachineInstr &instruction, TrackedFUs unit, unsigned int cycle);
      void remove(const MachineInstr &instruction);
    };

    class OptimisedPreemption {
    private:
      struct InstructionData {
        MachineBasicBlock::iterator instructionIt;
        unsigned int position;
        TrackedFUs functionalUnit;

        InstructionData() : instructionIt(nullptr), position(0), functionalUnit(TrackedFUs::NumFunctionalUnits) {}
        InstructionData(MachineBasicBlock::iterator instructionIt, TrackedFUs functionalUnit) :
         instructionIt(instructionIt), position(0), functionalUnit(functionalUnit) {}
      };

      struct Range {
        unsigned int start, end;
        unsigned int reg;
      };

      // 32 IRF, 32x2 VRF (one bit for high half, one bit for low half), 1 for all TRF registers
      static constexpr unsigned int indexIRF = 0u;
      static constexpr unsigned int indexVRF = indexIRF + 32u;
      static constexpr unsigned int indexTRF = indexVRF + 64u;
      static constexpr unsigned int numRegisters = indexTRF + 1u;

      // Minimum number of cycles needed to insert code protection disable and re-enable
      static const unsigned int minCycles = 4u;

      typedef std::bitset<numRegisters> CycleRegisters;
      typedef std::vector<CycleRegisters> Cycles;
      typedef std::vector<Range> Ranges;
      typedef std::vector<bool> FreeCycles;

      const SHAVEInstrInfo * SII = nullptr;
      const SHAVERegisterInfo *SRI = nullptr;

      MachineBasicBlock &block;
      bool isLoopHeader = false;
      PostSchedLiveRanges liveRanges;

      Cycles writeRegisters, readRegisters;
      Ranges ranges;
      FreeCycles freeCycles;
      unsigned int nonBranchCycles = 0u;
      unsigned int cycles = 0u;

      void setRegisterUse(Cycles &cycles, unsigned int reg, unsigned int cycle);
      Cycles generateRegisters(std::function<bool(const MachineOperand &)> &operandPredicate);
      void generateWriteRegisters();
      void generateReadRegisters();
      void generateFreeCycles();
      void countCycles();
      void generatePreemptionRanges();
      int insertInstruction(std::vector<InstructionData> &instructionData, unsigned int &cycle, RangeCycleData &rangeCycleData, bool allowNewCycles);
      InstructionData flipCodeProtection(unsigned int &cycle, RangeCycleData &rangeCycleData, bool turnOn, bool allowNewCycles);
      InstructionData setB_CFG(unsigned int &cycle, RangeCycleData &rangeCycleData, bool allowNewCycles);
      InstructionData getB_CFG(unsigned int &cycle, RangeCycleData &rangeCycleData, bool allowNewCycles);
      unsigned int moveInstructions(const std::vector<InstructionData> &instructions);
      void fixPEUInstructions(std::vector<InstructionData> &instructions);
      bool insertIntoRanges();
      void insertIntoHeader();

#ifndef NDEBUG
      void dumpOFreeCycles() const;
      void dumpRanges() const;
#endif

    public:
      OptimisedPreemption(const SHAVEInstrInfo * SII, MachineBasicBlock &block, const std::set<unsigned int> availableCalleeSaved, bool isLoopHeader) :
          SII(SII), SRI(SII->getSHAVERegisterInfo()), block(block), isLoopHeader(isLoopHeader), liveRanges(block, SII, availableCalleeSaved) {
        countCycles();
      }
  
      bool insert();
    };

  } // End namespace SHAVEPostSchedulingPreemptionNS

  void initializeSHAVEPostSchedulingPreemptionPass(PassRegistry&);

  class SHAVEPostSchedulingPreemption : public MachineFunctionPass {
  private:
    const SHAVEInstrInfo * SII = nullptr;
    MachineLoopInfo *machineLoopInfo = nullptr;

    void insertUnoptimised(MachineBasicBlock &block);

  public:
    SHAVEPostSchedulingPreemption() : MachineFunctionPass(ID) {
      initializeSHAVEPostSchedulingPreemptionPass(*llvm::PassRegistry::getPassRegistry());
    }

    StringRef getPassName() const override { return "SHAVE Post-Scheduling Preemption Pass"; }
    bool runOnMachineFunction(MachineFunction &MF) override;
    void getAnalysisUsage(AnalysisUsage &AU) const override;

    static char ID;
  };

} // End namespace llvm

#endif // SHAVEPostSchedulingPreemption_H
