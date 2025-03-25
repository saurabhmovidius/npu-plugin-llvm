//===-- SHAVEWidenVectorTypes.h - Vector Types Widening Pass -*- C++ -*---===//
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

#ifndef SHAVEWIDENVECTORTYPES_H
#define SHAVEWIDENVECTORTYPES_H (1)


#include "llvm/CodeGen/MachineFunctionPass.h"

#include "SHAVE.h"

using namespace llvm;

class SHAVEWidenVectorTypes : public FunctionPass {
private:
  void promoteBoolVectorLoad(Instruction * instruction);
  void promoteBoolVectorStore(Instruction * instruction);
  void widenThreeElementVectorLoad(Instruction * instruction);
  void widenThreeElementVectorStore(Instruction * instruction);

public:
  SHAVEWidenVectorTypes() : FunctionPass(ID) {
    initializeSHAVEWidenVectorTypesPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "SHAVE Vector Type Widening Pass";
  }

  bool runOnFunction(Function &F) override;

  static char ID;
};


#endif // SHAVEWIDENVECTORTYPES_H
