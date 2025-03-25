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
// ***************************************************************************

#ifndef LLVM_SHAVEOPTIONS_H
#define LLVM_SHAVEOPTIONS_H (1)

#include "llvm/Support/CommandLine.h"


namespace SHAVEOptions
{
    enum LSUPolicies {
      PreferLSU0,
      PreferLSU1,
      AlwaysUseLSU0,
      AlwaysUseLSU1,
      FollowNormalLSUPolicy
    };

    enum TraceGenerationDirections {
      ForwardsOnly,
      BackwardsOnly,
      BothDirections
    };

    enum ThreeElementVectorWideningActions {
      Disable,
      LoadOnly,
      StoreOnly,
      LoadAndStore
    };

    enum Preemption {
      Off,
      NoRestore,
      Restore
    };

    enum Scheduling {
      New,
      LegacyPreRA,
      LegacyPostRA,
      LegacyBoth
    };

    extern llvm::cl::opt<bool>        EnableFPAccMac;
    extern llvm::cl::opt<bool>        EnableINTAccMac;

    extern llvm::cl::opt<bool>        HasFPU;
    extern llvm::cl::opt<bool>        HasLSU1;
    extern llvm::cl::opt<bool>        HasSAU;
    extern llvm::cl::opt<bool>        HasVAU;

    extern llvm::cl::opt<double>      AverageInstructionSize;

    extern llvm::cl::opt<unsigned>    AliasedStoreDistance;
    extern llvm::cl::opt<int>         BranchDelays;
    extern llvm::cl::opt<unsigned>    LoadLatency;
    extern llvm::cl::opt<LSUPolicies> LSULoadPolicy;
    extern llvm::cl::opt<LSUPolicies> LSUStorePolicy;
    extern llvm::cl::opt<LSUPolicies> LSUVolatileLoadStorePolicy;
    extern llvm::cl::opt<bool>        LSUFlushOnEnter;

    extern llvm::cl::opt<bool>        DisableJMPtoBRAPeephole;
    extern llvm::cl::opt<bool>        DisablePeepholeOptimiser;
    extern llvm::cl::opt<bool>        DisableSlotOptimiser;

    extern llvm::cl::opt<bool>        DisableNOPCompression;

    extern llvm::cl::opt<bool>        DisableNaNChecking;

    extern llvm::cl::opt<bool>        EnableLDXandSTXInstructions;

    extern llvm::cl::opt<std::string> SectionPrefix;

    extern llvm::cl::opt<unsigned>    AlignStack;

    // Options for managing the alignment of functions and labels
    extern llvm::cl::opt<unsigned>    AlignFunctionLabels;
    extern llvm::cl::opt<bool>        AlignAllLabels;
    extern llvm::cl::opt<bool>        AlignJumpLabels;
    extern llvm::cl::opt<bool>        AlignLoopLabels;

    // Inline assembly options
    extern llvm::cl::opt<unsigned>    MaxLinesInlineAssembly;
    extern llvm::cl::opt<unsigned>    InlineAssemblyWarnThreshold;
    extern llvm::cl::opt<bool>        UseNOPSync;

    // Other optimisations
    extern llvm::cl::opt<bool>        EnableStashRetrieve;
    extern llvm::cl::opt<bool>        PreRASchedPeepholes;
    extern llvm::cl::opt<bool>        EnablePreSched2Peepholes;
    extern llvm::cl::opt<bool>        UseLDI;
    extern llvm::cl::opt<bool>        UseVILV;
    extern llvm::cl::opt<bool>        UseCompress;
    extern llvm::cl::opt<bool>        EnableAddressSimplifier;
    extern llvm::cl::opt<bool>        EnableAntiDependencyBreaker;
    extern llvm::cl::opt<Preemption>  EnablePreemption;
    extern llvm::cl::opt<int>         PreemptionMaxLoopDepth;
    extern llvm::cl::opt<bool>        EnableLowImpactPreemption;
    extern llvm::cl::opt<bool>        PreemptionDisableFallthroughChecks;
    extern llvm::cl::opt<bool>        PreemptionDisableOptimisedPostSched;
    extern llvm::cl::opt<bool>        EnableInterblockMovement;

    // Scheduler options
    extern llvm::cl::opt<bool>        EnablePreRAScheduler;
    extern llvm::cl::opt<bool>        EnablePostRAOptimisingScheduler;

    extern llvm::cl::opt<unsigned>    MaximumTraceBlocks;
    extern llvm::cl::opt<unsigned>    MaximumTrackedMemoryOperations;
    extern llvm::cl::opt<double>      InnerLoopBlockWeightModifier;
    extern llvm::cl::opt<double>      OuterLoopBlockWeightModifier;
    extern llvm::cl::opt<TraceGenerationDirections>
                                      TraceGenerationDirection;
    extern llvm::cl::opt<std::string> MutationOrder;
    extern llvm::cl::opt<bool>        EnableInPlaceMutation;

    extern llvm::cl::opt<bool>        EnableInterruptFriendlyScheduling;

    extern llvm::cl::opt<bool>        EnableLoopLoadHoisting;

    extern llvm::cl::opt<unsigned>    NPU4LoadStoreDistance;

    extern llvm::cl::opt<Scheduling>  EnableSchedulingMethod;

    // Stack instrumentation options
    extern llvm::cl::opt<bool>        StackOverflowChecking;
    extern llvm::cl::opt<bool>        TrackStackUsage;

    // Option for controlling 3-element vector promotion to 4-element vectors
    extern llvm::cl::opt<ThreeElementVectorWideningActions>
                                      WidenThreeElementVector;
    // Option to prevent emitting contant pools during BUILD_VECTOR lowering
    extern llvm::cl::opt<bool>        DisableConstantPools;

    // Option to enable Machine Late Instructions Cleanup pass for SHAVE
    // (disabled by default due to interference with the scratch register usage)
    extern llvm::cl::opt<bool>        EnableMachineLateInstrsCleanup;
} // End of namespace SHAVEOptions


#endif // LLVM_SHAVEOPTIONS_H
