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
// File       :  SHAVEMachineFunctionInfo.h
// Description:  Header file
// ---------------------------------------------------------------------------

#ifndef SHAVEMCASMINFO_H_
#define SHAVEMCASMINFO_H_ (1)


#include "llvm/MC/MCAsmInfo.h"


namespace llvm {
  class Triple;

  class SHAVEMCAsmInfo : public MCAsmInfo {
  public:
    explicit SHAVEMCAsmInfo(const Triple &TT, const MCTargetOptions &Options);
  };
} // end namespace llvm


#endif // SHAVEMCASMINFO_H_
