//===-- SHAVESubtarget.cpp - Handle SHAVE subtargets-------------*- C++ -*-===//
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
// Implementations for the properties for each of the Myriad processors
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "shave-subtarget"

#include <set>

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringSwitch.h"

#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVE.h"
#include "SHAVEInstrInfo.h"
#include "SHAVESubtarget.h"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "SHAVEGenSubtargetInfo.inc"

using namespace llvm;
using namespace SHAVEOptions;


SHAVESubtarget::SHAVESubtarget(const SHAVETargetMachine *STM, const Triple &TT, StringRef CPU, StringRef FS)
    : SHAVEGenSubtargetInfo(TT, CPU, CPU, FS),
      SHAVETM(*STM),
      cpuName(!CPU.empty() ? CPU : defaultSHAVECPUName), // Did the programmer specify which target CPU type?
      InstrInfo(*this),
      FrameInfo(InstrInfo, Align(AlignStack)),
      TLInfo{(*STM)}
{
  // Handle the target specific features
  ParseSubtargetFeatures(cpuName, cpuName, FS);

  // Initialize scheduling itinerary for the specified CPU.
  InstrItins = getInstrItineraryForCPU(cpuName);

  // SAURABH: LLVM7 change
  //  TSModel.init(getSchedModel(), this, &InstrInfo);
  TSModel.init(this);
  convertPRMToItins(&InstrInfo);

  // Initialise lowering actions last as it requires the subtarget features to have been initialised
  TLInfo.initialiseLoweringActions(*STM);

  // Same goes for latency values in InstrInfo
  InstrInfo.initialiseLatencies();

  assert(cmxCutSize != 0        && "cmxCutSize not set for target");
  assert(cmxNumberOfCuts != 0   && "cmxNumberOfCuts not set for target");
  assert(maxBRARange != 0       && "maxBRARange not set for target");
  assert(minLoadLatency != 0    && "minLoadLatency not set for target");
  assert(LDOSTO_OffsetBits != 0 && "LDOSTO_OffsetBits not set for target");
}

bool PortCompare(const SHAVEPortUse &A, const SHAVEPortUse &B) {
  return A.Latency < B.Latency;
}

void SHAVESubtarget::convertPRMToItins(const TargetInstrInfo *TII) {
  const SHAVEInstrInfo *SII = static_cast<const SHAVEInstrInfo *>(TII);

  Stages.clear();
  OperandCycles.clear();
  Forwardings.clear();
  Itineraries.clear();

  // Determine the maximum scheduling class ID and map classes to units.
  std::vector<unsigned> FUnits;
  std::set<unsigned> ProcessedClasses;
  unsigned MaxClassID = 0;

  for (unsigned i = 0, e = TII->getNumOpcodes(); i != e; ++i) {
    const MCInstrDesc &InstDesc = TII->get(i);
    unsigned SchedClass = InstDesc.getSchedClass();

    if (ProcessedClasses.find(SchedClass) != ProcessedClasses.end())
      continue;

    MaxClassID = std::max(MaxClassID, SchedClass);

    if (FUnits.size() <= MaxClassID)
      FUnits.resize(MaxClassID + 1);

    FUnits[SchedClass] = SII->GetFunctionalUnit(i);
    ProcessedClasses.insert(SchedClass);
  }

  // Emit an itinerary for each scheduling class.
  for (unsigned i = 0, e = MaxClassID + 1; i != e; ++i) {
    InstrItinerary Itinerary;
    const MCSchedClassDesc *SCDesc = getSchedModel().getSchedClassDesc(i);

    if (!SCDesc->isValid()) {
      // Emit invalid itinerary.
      Itinerary.NumMicroOps = 0;
      Itinerary.FirstStage = Itinerary.LastStage = 0;
      Itinerary.FirstOperandCycle = Itinerary.LastOperandCycle = 0;
      Itineraries.push_back(Itinerary);
      continue;
    }

    Itinerary.NumMicroOps = 1;
    Itinerary.FirstStage = Stages.size();
    Itinerary.FirstOperandCycle = OperandCycles.size();

    // Populate ports.
    SmallVector<SHAVEPortUse, 4> Ports;
    TargetSchedModel::ProcResIter PI = getWriteProcResBegin(SCDesc);
    TargetSchedModel::ProcResIter PE = getWriteProcResEnd(SCDesc);

    for (; PI != PE; ++PI) {
      SHAVEPortUse PU;
      PU.Port = PI->ProcResourceIdx;

      // PR_DUMMY has zero unit of resources, ignore it.
      const MCProcResourceDesc *ProcDesc = getSchedModel().getProcResource(PU.Port);
      if (!ProcDesc->NumUnits)
        continue;

      for (int latency = PI->AcquireAtCycle; latency < PI->ReleaseAtCycle; ++latency) {
        PU.Latency = latency;
        Ports.push_back(PU);
      }
    }

    std::sort(Ports.begin(), Ports.end(), PortCompare);

    // Populate stages.
    InstrStage Stage;
    unsigned Cycle = 0;
    Stage.Cycles_ = 1;
    Stage.Units_ = (1ULL << FUnits[i]);
    Stage.Kind_ = InstrStage::Required;

    for (unsigned j = 0, e2 = Ports.size(); j != e2; ++j) {
      // Emit previous stage.
      SHAVEPortUse &Port = Ports[j];
      Stage.NextCycles_ = (Port.Latency - Cycle);
      Stages.push_back(Stage);

      // Populate current stage.
      Cycle = Port.Latency;
      Stage.Cycles_ = 1;
      Stage.Units_ = (1ULL << (8 + Port.Port));
      Stage.Kind_ = InstrStage::Required;
    }

    Stage.NextCycles_ = -1;
    Stages.push_back(Stage);

    // Populate operand cycles and forwardings.
    for (unsigned j = 0, e2 = SCDesc->NumWriteLatencyEntries; j != e2; ++j) {
      const MCWriteLatencyEntry *WLEntry = getWriteLatencyEntry(SCDesc, j);
      unsigned Latency = (WLEntry->Cycles < 0) ? (unsigned)SII->getDelaySlots()
                                               : (unsigned)WLEntry->Cycles;

      OperandCycles.push_back(Latency);
      Forwardings.push_back(0);
    }

    // Emit the itinerary for the scheduling class.
    Itinerary.LastStage = Stages.size();
    Itinerary.LastOperandCycle = OperandCycles.size();
    Itineraries.push_back(Itinerary);
  }

  InstrItins.SchedModel = getSchedModel();
  InstrItins.Stages = &Stages[0];
  InstrItins.OperandCycles = &OperandCycles[0];
  InstrItins.Forwardings = &Forwardings[0];
  InstrItins.Itineraries = &Itineraries[0];
}

bool SHAVESubtarget::useLDXSTX() const {
  // FIXME: Movidius - shouldn't we be using the commented out form only?
  return EnableLDXandSTXInstructions;
//  return isMyriad2v3() || (isMyriad2v2() && EnableLDXandSTXInstructions);
}
