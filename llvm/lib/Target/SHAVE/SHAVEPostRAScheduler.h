//===-- SHAVEPostRAScheduler.h - Post-RA Scheduler Pass ---------*- C++ -*-===//
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

#include "llvm/CodeGen/TargetInstrInfo.h"

#include "SHAVESchedulerBase.h"
#include "SHAVEPostRASchedule.h"

namespace llvm {

class SHAVEPostRAScheduler : public SHAVESchedulerBase {
private:
  //
  // private variables
  //
  enum class SchedulingMethod {
    Legacy = 0,
    List,
    BreadthFirst,
    NumMethods
  };
  MachineFunction *currentFunction = nullptr;

  //
  // private functions
  //
  void scheduleGraphLegacy(SHAVEDependencyNodePtr startingNode, SHAVEPostRASchedule& schedule);
  void scheduleGraphList(SHAVEDependencyNodePtr startingNode, SHAVEPostRASchedule& schedule);
  void scheduleGraphBreadthFirst(SHAVEDependencyNodePtr startingNode, SHAVEPostRASchedule &schedule);
  void setupLSUFunctionalUnits();
  void replaceJMPwithBRA();

#ifndef NDEBUG
  //
  // overridden functions from SHAVESchedulerBase
  //
  void generateAndDumpInstrIDs(raw_ostream& out) override;
#endif // NDEBUG

public:
  SHAVEPostRAScheduler(MachineFunction &MF, AliasAnalysis* AA, const SHAVEInstrInfo* SII)
    : SHAVESchedulerBase(SII, AA), currentFunction(&MF) {}

  bool run();
};

} // namespace llvm
