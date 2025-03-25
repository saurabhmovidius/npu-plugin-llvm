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

#include <ctime>
#include <sstream>

#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"

#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVE.h"
#include "SHAVEMachineFunctionInfo.h"
#include "SHAVEPredicator.h"
#include "SHAVETargetMachine.h"
#include "SHAVETargetObjectFile.h"
#include "SHAVETargetTransformInfo.h"

using namespace llvm;
using namespace SHAVEOptions;


extern "C" void LLVMInitializeSHAVETarget() {
  // Register the target.
  RegisterTargetMachine<SHAVETargetMachine> X(TheSHAVETarget);
  PassRegistry &Registry = *PassRegistry::getPassRegistry();

  initializeSHAVEWidenVectorTypesPass(Registry);
  initializeSHAVELoopLoadHoisterPass(Registry);
  initializeSHAVEPreRASchedPeepholePass(Registry);
  initializeSHAVEPreRASchedulerPass(Registry);
  initializeSHAVEAntiDepBreakerPass(Registry);
  initializeSHAVEInterblockMovementPass(Registry);
  initializeSHAVEPostRASchedPass(Registry);
  initializeSHAVEPredicatorPass(Registry);
}


namespace SHAVESupport {
  // This helper function is used here for LLVM, and also in SHAVETargetInfo for CLang
  // in 'llvm/tools/clang/lib/Basic/Targets.cpp'
  const char* getDataLayoutString(const TargetOptions & /* Options */) {
    // For the most part the defaults are what we want, but override some of them
    return "e"      // Little-Endian (default is Big-Endian)
      "-m:e"        // Use ELF mangling - private symbols are '.L' prefixed (unspecified default)
      "-p:32:32"    // Pointers are 32-bit and 32-bit aligned (default is 64:64)
      "-f64:64"     // 64-bit floating-point are also 64-bit aligned (default is 64:32:64)
      "-i1:8:8"     // 1-bit integers vector-elements are extended to i8
      "-i64:64"     // 64-bit integers are also 64-bit aligned (default is 64:32:64)
      "-v128:64"    // Maximum alignment is 64-bits (default is 128:128)
      "-v64:64"     // 64-bit vectors are 64-bit aligned (unspecified default)
      "-v32:32"     // We also have 32-bit vectors which have no default
      //"-v16:16"     // Optimise memory usage for '[su]char2' - Disabled for Codeplay issue #58
      "-n8:16:32"   // Natural optimal integer size
      "-S64";       // Stack is always 64-bit aligned
  }
}


namespace {
  Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
    if (!RM.has_value())
      return Reloc::Static;
    return *RM;
  }
} // End of anonymous namespace

SHAVETargetMachine::SHAVETargetMachine(const Target &T, const Triple &TargetTriple,
                                       StringRef CPU, StringRef FS, const TargetOptions &Options,
                                       std::optional<Reloc::Model> RM, std::optional<CodeModel::Model> CM,
                                       CodeGenOptLevel OL, bool JIT)
    : LLVMTargetMachine(T, StringRef(SHAVESupport::getDataLayoutString(Options)), TargetTriple,
                        CPU, FS, Options,
                        getEffectiveRelocModel(RM), CM ? *CM : CodeModel::Small, OL),
    Subtarget(this, TargetTriple, CPU, FS),
      TLOF(std::make_unique<SHAVETargetObjectFile>(this)) {
  initAsmInfo();
}

SHAVETargetMachine::~SHAVETargetMachine() {
}

MachineFunctionInfo *SHAVETargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return SHAVEMachineFunctionInfo::create<SHAVEMachineFunctionInfo>(Allocator,
                                                                    F, STI);
}

TargetTransformInfo
SHAVETargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(SHAVETargetTransformInfo(this, F));
}

TargetPassConfig * SHAVETargetMachine::createPassConfig(PassManagerBase &PM) {
  return new SHAVEPassConfig(this, PM);
}

bool SHAVETargetMachine::isNoopAddrSpaceCast(unsigned SrcAS, unsigned DestAS) const {
  // SHAVE has a flat address space so all AddrSpaceCasts are no-ops
  return true;
}

//===----------------------------------------------------------------------===//

SHAVEPassConfig::SHAVEPassConfig(SHAVETargetMachine *TM, PassManagerBase &PBM)
    : TargetPassConfig(*TM, PBM) {}

bool SHAVEPassConfig::addInstSelector() {
  addPass(createSHAVEISelDAGtoDAG(getSHAVETargetMachine()));
  return false;
}

bool SHAVEPassConfig::addPreISel() {
  addPass(createSHAVEWidenVectorTypesPass());
  return false;
}

void SHAVEPassConfig::addPreRegAlloc() {
  if (EnableAddressSimplifier)
    addPass(createSHAVEAddressSimplifierPass());
  if (TM->getOptLevel() != CodeGenOptLevel::None) {
    if (!DisablePeepholeOptimiser && PreRASchedPeepholes)
      addPass(createSHAVEPreRASchedPeepholePass());
    if (EnablePreRAScheduler)
      substitutePass(&MachineSchedulerID, &SHAVEExperimentalPreRAScheduleID);
  }

  insertPass(&MachineSchedulerID, &SHAVELoopLoadHoister::ID);
}

void SHAVEPassConfig::addPostRegAlloc() {
  if (TM->getOptLevel() != CodeGenOptLevel::None)
    addPass(createSHAVECopyAddCombining());
  addPass(createSHAVEFixLUTOperands());
}

void SHAVEPassConfig::addPreSched2() {
  addPass(&ExpandPostRAPseudosID);
  if (EnablePreSched2Peepholes)
    addPass(createSHAVEPreSched2PeepholePass());
  if (EnableInterblockMovement && TM->getOptLevel() >= CodeGenOptLevel::Default) // Only at -O2, -Os and -O3
    addPass(createSHAVEInterblockMovementPass());
  addPass(createSHAVEPredicator());
  if (EnablePreemption != Preemption::Off)
    addPass(createSHAVEPreemptionHandlerPass());
}

void SHAVEPassConfig::addPreEmitPass() {
  if (EnableAntiDependencyBreaker &&
      TM->getOptLevel() >= CodeGenOptLevel::Default) // Only at -O2, -Os and -O3
    addPass(createSHAVEAntiDepBreakerPass());
  addPass(createSHAVEPostRASchedPass());
  if (EnablePreemption != Preemption::Off)
    addPass(createSHAVEPostSchedulingPreemptionPass());
}

void SHAVEPassConfig::addIRPasses() {
    TargetPassConfig::addIRPasses();
    // The pass is disabled for SHAVE by default
    if (!EnableMachineLateInstrsCleanup)
        disablePass(&MachineLateInstrsCleanupID);
}
