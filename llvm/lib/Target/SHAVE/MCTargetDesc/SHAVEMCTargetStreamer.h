//===-- SHAVEMCTargetStreamer.h - Instruction Information -------*- C++ -*-===//
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

#ifndef SHAVEMCTARGETSTREAMER_H
#define SHAVEMCTARGETSTREAMER_H (1)


#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/FormattedStream.h"

namespace llvm {

  class SHAVEMCTargetStreamer : public MCTargetStreamer {
  public:
    formatted_raw_ostream &OS;
    SHAVEMCTargetStreamer(MCStreamer &S,
                          formatted_raw_ostream &OS,
                          MCInstPrinter *InstPrint,
                          bool IsVerboseAsm);
    ~SHAVEMCTargetStreamer() override;

    void prettyPrintAsm(MCInstPrinter &InstPrinter, uint64_t Address,
			const MCInst &Inst, const MCSubtargetInfo &STI,
			raw_ostream &OS) override;
  };

} // namespace llvm

#endif // SHAVEMCTARGETSTREAMER_H
