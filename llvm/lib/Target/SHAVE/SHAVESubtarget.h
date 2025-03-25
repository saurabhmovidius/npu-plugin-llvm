//===-- SHAVESubtarget.h - Handle SHAVE subtargets---------------*- C++ -*-===//
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

#ifndef LLVM_TARGET_SHAVE_SUBTARGET_H
#define LLVM_TARGET_SHAVE_SUBTARGET_H (1)

#include <vector>

#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/TargetSchedule.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Target/TargetMachine.h"

#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVEFrameLowering.h"
#include "SHAVELowering.h"
#include "SHAVESelectionDAGInfo.h"

#define GET_SUBTARGETINFO_HEADER
#include "SHAVEGenSubtargetInfo.inc"

namespace llvm {
  class TargetInstrInfo;
  class SHAVETargetMachine;

  class SHAVESubtarget : public SHAVEGenSubtargetInfo {
  public:
    SHAVESubtarget(const SHAVETargetMachine *STM, const Triple &TT, StringRef CPU, const StringRef FS);

    FeatureBitset subtargetFeatures;

    #define defaultSHAVECPUName ("3720xx")

    // Features of the CPU
    bool          hasFeature(unsigned int feature) const { return subtargetFeatures.test(feature); }
    bool          useLDXSTX() const;
    bool          hasFPU() const { return SHAVEOptions::HasFPU; }
    bool          hasLSU1() const { return SHAVEOptions::HasLSU1; }
    bool          hasSAU() const { return SHAVEOptions::HasSAU; }
    bool          hasVAU() const { return (hasFeature(SHAVE::HasVRF128_Feature) || hasFeature(SHAVE::HasVRF512_Feature)) && SHAVEOptions::HasVAU; }
    
    unsigned int  getMinimumLoadLatency() const { return minLoadLatency; }
    unsigned int  getCMXCutSize() const { return cmxCutSize; }
    unsigned int  getCMXNumberOfCuts() const { return cmxNumberOfCuts; }
    unsigned int  getBRARange() const { return maxBRARange; }
    unsigned int  getLDOSTO_OffsetBits() const { return LDOSTO_OffsetBits; }
    unsigned int  getNumShavesPerTile() const { return numShavesPerTile; }

    void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

    const SHAVESelectionDAGInfo *getSelectionDAGInfo() const override { return &DAGInfo; }
    const SHAVEInstrInfo *getInstrInfo() const override { return &InstrInfo; }
    const SHAVEFrameLowering *getFrameLowering() const override { return &FrameInfo; }
    const SHAVELowering *getTargetLowering() const override { return &TLInfo; }
    const TargetRegisterInfo *getRegisterInfo() const override { return InstrInfo.getSHAVERegisterInfo(); }
    const InstrItineraryData *getInstrItineraryData() const override { return &InstrItins; }
    const TargetSchedModel *getTargetSchedModel() const { return &TSModel; }

    // convertPRMToItins - convert the per-operand scheduling information
    // to instruction itineraries.
    void convertPRMToItins(const TargetInstrInfo *TII);

  private:
    const SHAVETargetMachine &SHAVETM;

    InstrItineraryData InstrItins;
    std::vector<InstrStage> Stages;
    std::vector<unsigned> OperandCycles;
    std::vector<unsigned> Forwardings;
    std::vector<InstrItinerary> Itineraries;

    StringRef cpuName;
    unsigned int cmxCutSize = 0;
    unsigned int cmxNumberOfCuts = 0;
    unsigned int maxBRARange = 0;
    unsigned int minLoadLatency = 0;
    unsigned int LDOSTO_OffsetBits = 0;
    unsigned int numShavesPerTile = 0;

    SHAVESelectionDAGInfo DAGInfo;
    SHAVEInstrInfo InstrInfo;
    SHAVEFrameLowering FrameInfo;
    SHAVELowering TLInfo;
    TargetSchedModel TSModel;
  };
} // End of namespace 'llvm'


#endif  // LLVM_TARGET_SHAVE_SUBTARGET_H
