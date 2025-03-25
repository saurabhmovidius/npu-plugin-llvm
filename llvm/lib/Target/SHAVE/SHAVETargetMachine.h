//===-- SHAVETargetMachine.cpp - About the SHAVE target ---------*- C++ -*-===//
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
// Implements SHAVE target machine interface
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGET_SHAVETARGETMACHINE_H
#define LLVM_TARGET_SHAVETARGETMACHINE_H (1)

#include "SHAVESubtarget.h"

#include "llvm/CodeGen/TargetPassConfig.h"

#include <optional>

namespace llvm {
  FunctionPass *createSHAVEFixLUTOperands();

  class SHAVETargetMachine : public LLVMTargetMachine {
  public:
    SHAVETargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                       StringRef FS, const TargetOptions &Options,
                       std::optional<Reloc::Model> RM,
                       std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                       bool JIT);
    ~SHAVETargetMachine() override;

    // The required interface
    const SHAVESubtarget *getSubtargetImpl() const { return &Subtarget; }
    const SHAVESubtarget *getSubtargetImpl(const Function& /* F */) const override { return getSubtargetImpl(); }
    TargetLoweringObjectFile *getObjFileLowering() const override { return TLOF.get(); }

    MachineFunctionInfo *
    createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                              const TargetSubtargetInfo *STI) const override;
    TargetTransformInfo getTargetTransformInfo(const Function &F) const override;
    TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

    bool isMachineVerifierClean() const override {
      return false;
    }

    bool isNoopAddrSpaceCast(unsigned SrcAS, unsigned DestAS) const override;

  private:
    SHAVESubtarget Subtarget;
    std::unique_ptr<TargetLoweringObjectFile> TLOF;
  };

  class SHAVEPassConfig : public TargetPassConfig {
  public:
    SHAVEPassConfig(SHAVETargetMachine *TM, PassManagerBase &PBM);

    SHAVETargetMachine &getSHAVETargetMachine() const { return getTM<SHAVETargetMachine>();}
    const SHAVESubtarget &getSHAVESubtarget() const { return *getSHAVETargetMachine().getSubtargetImpl(); }

    bool addPreISel() override;
    bool addInstSelector() override;
    void addPreRegAlloc() override;
    void addPostRegAlloc() override;
    void addPreSched2() override;
    void addPreEmitPass() override;
    void addIRPasses() override;
  };
} // namespace llvm


#endif // LLVM_TARGET_SHAVETARGETMACHINE_H
