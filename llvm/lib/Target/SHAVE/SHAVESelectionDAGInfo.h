//===-- SHAVESelectionDAGInfo.h - Memory Functions --------------*- C++ -*-===//
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
// Provides intrinsic implementations for memcpy, memmove, memset
//
//===----------------------------------------------------------------------===//

#ifndef SHAVESELECTIONDAGINFO_H
#define SHAVESELECTIONDAGINFO_H (1)


#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

namespace llvm {
  class SHAVESelectionDAGInfo : public SelectionDAGTargetInfo {
public:
    SDValue EmitTargetCodeForMemcpy(SelectionDAG &DAG, const SDLoc &dbgLoc,
                                    SDValue Chain,
                                    SDValue Dst, SDValue Src,
                                    SDValue Size, Align Alignment,
                                    bool isVolatile, bool AlwaysInline,
                                    MachinePointerInfo DstPtrInfo,
                                    MachinePointerInfo SrcPtrInfo) const override;
  };
} // End of namespace llvm


#endif  // SHAVESELECTIONDAGINFO_H
