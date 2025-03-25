//===-- SHAVEMCTargetStreamer.cpp - Instruction Information -----*- C++ -*-===//
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

#include "llvm/MC/MCInstPrinter.h"

#include "SHAVEMCTargetStreamer.h"
#include "SHAVESubtarget.h"

using namespace llvm;

SHAVEMCTargetStreamer::SHAVEMCTargetStreamer(MCStreamer &S,
                                             formatted_raw_ostream &OS,
                                             MCInstPrinter *InstPrint,
                                             bool IsVerboseAsm)
: MCTargetStreamer(S), OS(OS) {}

SHAVEMCTargetStreamer::~SHAVEMCTargetStreamer() {}

void SHAVEMCTargetStreamer::prettyPrintAsm(MCInstPrinter &InstPrinter, uint64_t Address,
                                           const MCInst &Inst, const MCSubtargetInfo &STI,
                                           raw_ostream &OS) {
  // Capture the assembly code to a string
  std::string tmp;
  raw_string_ostream asString(tmp);

  InstPrinter.printInst(&Inst, 0, "", STI, asString);
  OS << asString.str();
}
