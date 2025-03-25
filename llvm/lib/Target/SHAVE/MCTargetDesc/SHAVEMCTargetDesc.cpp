// ***************************************************************************
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
// ---------------------------------------------------------------------------
// File       :  SHAVEMCTargetDesc.cpp
// Description:  Provides SHAVE specific target descriptions.
// ***************************************************************************

#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"

#include "SHAVEInstPrinter.h"
#include "SHAVEMCAsmInfo.h"
#include "SHAVEMCTargetDesc.h"
#include "SHAVEMCTargetStreamer.h"

#define GET_INSTRINFO_MC_DESC
#include "SHAVEGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "SHAVEGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "SHAVEGenSubtargetInfo.inc"

using namespace llvm;


namespace {
  MCInstrInfo *createSHAVEMCInstrInfo() {
    MCInstrInfo *MII = new MCInstrInfo();
    InitSHAVEMCInstrInfo(MII);
    return MII;
  }

  MCRegisterInfo *createSHAVEMCRegisterInfo(const Triple &TT) {
    MCRegisterInfo *MRI = new MCRegisterInfo();
    InitSHAVEMCRegisterInfo(MRI, SHAVE::I31);
    return MRI;
  }

  MCInstPrinter *createSHAVEMCInstPrinter(const Triple &T, unsigned SyntaxVariant,
                                          const MCAsmInfo &MAI, const MCInstrInfo &MII,
                                          const MCRegisterInfo &MRI) {
    return new SHAVEInstPrinter(MAI, MII, MRI);
  }

  MCSubtargetInfo *createSHAVEMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
    return createSHAVEMCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
  }

  MCTargetStreamer *createSHAVEMCTargetStreamer(MCStreamer &S,
                                              formatted_raw_ostream &OS,
                                              MCInstPrinter *InstPrint,
                                              bool IsVerboseAsm) {
    return new SHAVEMCTargetStreamer(S, OS, InstPrint, IsVerboseAsm);
  }

} // End of anonymous namespace

// Force static initialization.
extern "C" void LLVMInitializeSHAVETargetMC() {
  RegisterMCAsmInfo<SHAVEMCAsmInfo> A(TheSHAVETarget);

  TargetRegistry::RegisterMCInstrInfo(TheSHAVETarget, createSHAVEMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(TheSHAVETarget, createSHAVEMCRegisterInfo);
  TargetRegistry::RegisterMCInstPrinter(TheSHAVETarget, createSHAVEMCInstPrinter);
  TargetRegistry::RegisterMCSubtargetInfo(TheSHAVETarget, createSHAVEMCSubtargetInfo);
  TargetRegistry::RegisterAsmTargetStreamer(TheSHAVETarget, createSHAVEMCTargetStreamer);
}
