//===-- ExtendTruncateReduction.h - Extend Truncate Reduction --*- C++ -*--===//
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
///
/// \file
/// \brief Header file for the ExtendTruncateReduction optimisation pass
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_EXTEND_TRUNCATE_REDUCTION_H
#define LLVM_TRANSFORMS_EXTEND_TRUNCATE_REDUCTION_H

#include "llvm/IR/PassManager.h"

namespace llvm {

struct ExtendTruncateReduction : public PassInfoMixin<ExtendTruncateReduction> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_EXTEND_TRUNCATE_REDUCTION_H
