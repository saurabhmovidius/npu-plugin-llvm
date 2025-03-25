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

#include "SHAVEOptions.h"

using namespace llvm;


namespace SHAVEOptions
{
    // Options for controlling MAC and ACC behaviour
    cl::opt<bool>         EnableFPAccMac(
                            "shave-generate-fp-acc-mac",
                            cl::init(false),
                            cl::desc("Generate accumulator patterns for fp"),
                            cl::Hidden);

    cl::opt<bool>         EnableINTAccMac(
                            "shave-generate-int-acc-mac",
                            cl::init(false),
                            cl::desc("Generate accumulator patterns for int"),
                            cl::Hidden);

    // Options for configuring constrained variants of SHAVE
    cl::opt<bool>         HasFPU(
                            "shave-has-floating-point-unit",
                            cl::init(true),
                            cl::desc("Enable support for floating-point operations"),
                            cl::Hidden);

    cl::opt<bool>         HasLSU1(
                            "shave-has-LSU1-fu",
                            cl::init(true),
                            cl::desc("Enable the second Load-Store functional-unit LSU1"),
                            cl::Hidden);

    cl::opt<bool>         HasSAU(
                            "shave-has-SAU-fu",
                            cl::init(true),
                            cl::desc("Enable the SAU functional-unit"),
                            cl::Hidden);

    cl::opt<bool>         HasVAU(
                            "shave-has-VAU-fu",
                            cl::init(true),
                            cl::desc("Enable the VAU functional-unit"),
                            cl::Hidden);

    // Tunable properties
    cl::opt<double>       AverageInstructionSize(
                            "shave-average-instruction-size",
                            cl::init(5.0),
                            cl::desc("Estimated average instruction size at '-O3' in Bytes"),
                            cl::value_desc("A value around 5.0 is generally good; default is 5.0"),
                            cl::Hidden);

    cl::opt<unsigned>     AliasedStoreDistance(
                            "shave-aliased-store-distance",
                            cl::init(1),
                            cl::desc("Minimum number of additional cycles between a store on one LSU followed by an aliased load on the other LSU"),
                            cl::value_desc("A value between '0' and '6'; default is '1'"),
                            cl::Hidden);

    cl::opt<int>          BranchDelays(
                            "shave-branch-delays",
                            cl::init(6),
                            cl::desc("Number of delay slots following a branch"),
                            cl::value_desc("A value between '0' and 'INT_MAX'; default is '6'"),
                            cl::Hidden);

    cl::opt<unsigned>     LoadLatency(
                            "shave-load-latency",
                            cl::init(6),
                            cl::desc("Number of cycles after issuing a memory load before the register is updated"),
                            cl::value_desc("A value between '6' and 'UINT_MAX'; default is '6'"),
                            cl::Hidden);

    cl::opt<LSUPolicies>  LSULoadPolicy(
                            "shave-lsu-load-policy",
                            cl::init(PreferLSU0),
                            cl::desc("Configure the LSU load policy"),
                            cl::values(
                               clEnumValN(PreferLSU0, "prefer-lsu0", "Memory loads should use LSU0 when possible {default}"),
                               clEnumValN(PreferLSU1, "prefer-lsu1", "Memory loads should use LSU1 when possible"),
                               clEnumValN(AlwaysUseLSU0, "use-only-lsu0", "Memory loads are constrained to use LSU0"),
                               clEnumValN(AlwaysUseLSU1, "use-only-lsu1", "Memory loads are constrained to use LSU1")),
                            cl::Hidden);

    cl::opt<LSUPolicies>  LSUStorePolicy(
                            "shave-lsu-store-policy",
                            cl::init(FollowNormalLSUPolicy),
                            cl::desc("Configure the LSU store policy"),
                            cl::values(
                               clEnumValN(PreferLSU0, "prefer-lsu0", "Memory stores should use LSU0 when possible"),
                               clEnumValN(PreferLSU1, "prefer-lsu1", "Memory stores should use LSU1 when possible {default}"),
                               clEnumValN(AlwaysUseLSU0, "use-only-lsu0", "Memory stores are constrained to use LSU0"),
                               clEnumValN(AlwaysUseLSU1, "use-only-lsu1", "Memory stores are constrained to use LSU1")),
                            cl::Hidden);

    cl::opt<LSUPolicies>  LSUVolatileLoadStorePolicy(
                            "shave-lsu-volatile-load-store-policy",
                            cl::init(AlwaysUseLSU1),
                            cl::desc("Configure the LSU policy for volatile loads and stores"),
                            cl::values(
                               clEnumValN(AlwaysUseLSU0, "use-only-lsu0", "Volatile memory loads and stores are constrained to use LSU0"),
                               clEnumValN(AlwaysUseLSU1, "use-only-lsu1", "Volatile memory loads and stores are constrained to use LSU1 {default}")),
                            cl::Hidden);

    cl::opt<bool>         LSUFlushOnEnter(
                            "shave-lsu-flush-on-enter",
                            cl::init(false),
                            cl::desc("Insert code to flush both LSUs on entry to a function"),
                            cl::Hidden);

    cl::opt<bool>         DisableJMPtoBRAPeephole(
                            "shave-no-replace-jmp-with-bra",
                            cl::init(false),
                            cl::desc("Disable the replacement of BRU.JMP with JMP.BRA"),
                            cl::Hidden);

    cl::opt<bool>         DisablePeepholeOptimiser(
                            "shave-no-peephole",
                            cl::init(false),
                            cl::desc("Disable all peephole optimisations"),
                            cl::Hidden);

    cl::opt<bool>         DisableSlotOptimiser(
                            "shave-disable-slot-optimiser",
                            cl::init(false),
                            cl::desc("Disable the VLIW slot optimiser"),
                            cl::Hidden);

    cl::opt<bool>         DisableNOPCompression(
                            "shave-disable-nop-compression",
                            cl::init(false),
                            cl::desc("Disable the use of NOP compression"),
                            cl::Hidden);

    cl::opt<bool>         DisableNaNChecking(
                            "shave-disable-NaN-checking",
                            cl::init(false),
                            cl::desc("Disable the generation of NaN checking code"),
                            cl::Hidden);

    cl::opt<bool>         EnableLDXandSTXInstructions(
                            "shave-enable-ldx-and-stx-instructions",
                            cl::init(false),
                            cl::desc("Enable the LSU.LDX and LSU.STX instructions"),
                            cl::Hidden);

    cl::opt<std::string>  SectionPrefix("shave-standard-section-prefix",
                            cl::init(""),
                            cl::desc("Specify a canonical prefix for standard section names {default: is empty}"),
                            cl::Hidden);

    cl::opt<unsigned>     AlignStack(
                            "shave-align-stack",
                            cl::init(8u),
                            cl::desc("Align the stack to the specified boundary {default is 8-bytes}"),
                            cl::Hidden);

    // Options for managing the alignment of functions and labels
    cl::opt<unsigned>     AlignFunctionLabels(
                            "shave-align-function-targets",
                            cl::init(16u),
                            cl::desc("Align function labels to the specified boundary {defaults to 16-bytes}"),
                            cl::Hidden);

    cl::opt<bool>         AlignAllLabels(
                            "shave-align-all-branch-targets",
                            cl::init(false),
                            cl::desc("Align all target labels to a 16-byte boundary"),
                            cl::Hidden);

    cl::opt<bool>         AlignJumpLabels(
                            "shave-align-jump-targets",
                            cl::init(false),
                            cl::desc("Align labels for targets which are only reachable by a jump to a 16-byte boundary when possible"),
                            cl::Hidden);

    cl::opt<bool>         AlignLoopLabels(
                            "shave-align-loop-targets",
                            cl::init(false),
                            cl::desc("Align labels which are targets for a loop to a 16-byte boundary when possible"),
                            cl::Hidden);

    // Inline assembly options
    cl::opt<unsigned>     MaxLinesInlineAssembly(
                            "shave-max-inline-assembly-lines",
                            cl::init(64),
                            cl::desc("The maximum number of lines of inline assembly code permitted in a single statement"),
                            cl::Hidden);

    cl::opt<unsigned>     InlineAssemblyWarnThreshold(
                            "shave-inline-assembly-warning-threshold",
                            cl::init(32),
                            cl::desc("The number of lines of inline assembly code considered too complex"),
                            cl::Hidden);

    cl::opt<bool>         UseNOPSync(
                            "shave-nop-sync-after-inline-asm",
                            cl::init(true),
                            cl::desc("Emit a 'NOP SYNC' instruction after inline-assembly"),
                            cl::Hidden);

    // Other optimisations
    cl::opt<bool>         EnableStashRetrieve(
                            "shave-stash-retrieve",
                            cl::init(false),
                            cl::desc("Enable the new SHAVE VRF stash-retrieve logic to avoid spilling IRF registers to stack"),
                            cl::Hidden);

    cl::opt<bool>         PreRASchedPeepholes(
                            "shave-prera-scheduler-peepholes",
                            cl::init(true),
                            cl::desc("Enable the pre-RA Scheduler peephole optimisations"),
                            cl::Hidden);

    cl::opt<bool>         EnablePreSched2Peepholes(
                            "shave-pre-sched2-peepholes",
                            cl::init(true),
                            cl::desc("Enable the peephole optimisations before final (Post-RA) scheduling"),
                            cl::Hidden);

    cl::opt<bool>         UseLDI(
                            "shave-enable-ldi",
                            cl::init(false),
                            cl::desc("Enable generation of LSU.LDI instructions"),
                            cl::Hidden);

    cl::opt<bool>         UseVILV(
                            "shave-enable-vilv",
                            cl::init(true),
                            cl::desc("Enable generation of CMU.VILV instructions"),
                            cl::Hidden);

    cl::opt<bool>         UseCompress(
                            "shave-enable-compress",
                            cl::init(true),
                            cl::desc("Enable generation of CMU.COMPRESS instructions"),
                            cl::Hidden);

    cl::opt<bool>         EnableAddressSimplifier(
                            "shave-enable-address-simplifier",
                            cl::init(false),
                            cl::desc("Enable loop address simplification pass"),
                            cl::Hidden);

    cl::opt<bool>         EnableAntiDependencyBreaker(
                            "shave-enable-anti-dependency-breaker",
                            cl::init(true),
                            cl::desc("Enable Post-RA anti-dependency breaking"),
                            cl::Hidden);

    cl::opt<Preemption>   EnablePreemption(
                            "shave-enable-preemption-checks",
                            cl::init(Preemption::Off),
                            cl::desc("Enable automatically generated preemption cehcks"),
                            cl::values(
                               clEnumValN(Preemption::Off, "off", "Turn off all moviCompile-generated preemption checks {default}"),
                               clEnumValN(Preemption::NoRestore, "norestore", "Generate preemption checks with no save/restore capability"),
                               clEnumValN(Preemption::Restore, "restore", "Generate preemption checks with save/restore capability")),
                            cl::Hidden);

    cl::opt<bool>         EnableLowImpactPreemption(
                            "shave-enable-low-impact-preemption",
                            cl::init(false),
                            cl::desc("Generate preemption checks with lower impact on loop performance"),
                            cl::Hidden);

    cl::opt<int>          PreemptionMaxLoopDepth(
                            "shave-preemption-max-loop-depth",
                            cl::init(-1),
                            cl::desc("Set the maximum loop depth that preemption will be added to, a value of 1 indicates inner-most loops only"),
                            cl::Hidden);

    cl::opt<bool>          PreemptionDisableFallthroughChecks(
                            "shave-preemption-disable-fallthrough",
                            cl::init(false),
                            cl::desc("Disable insertion of preemption checks for loops where the latch falls through to the header"),
                            cl::Hidden);

    cl::opt<bool>          PreemptionDisableOptimisedPostSched(
                            "shave-preemption-disable-optimised-post-sched",
                            cl::init(false),
                            cl::desc("Disable the insertion of the optimised version of post-scheduling preemption in favour of the original loop implementation"),
                            cl::Hidden);

    cl::opt<bool>          EnableInterblockMovement(
                            "shave-enable-interblock-movement",
                            cl::init(true),
                            cl::desc("Enable the SHAVE Interblock Movement Pass"),
                            cl::Hidden);

    // Scheduler options
    cl::opt<bool>         EnablePreRAScheduler(
                            "shave-prera-scheduler",
                            cl::init(true),
                            cl::desc("Use the SHAVE scheduler pre-register-allocation"),
                            cl::Hidden);

    cl::opt<bool>         EnablePostRAOptimisingScheduler(
                            "shave-enable-postra-optimising-scheduler",
                            cl::init(true),
                            cl::desc("Use the optimising SHAVE post-register-allocation scheduler"),
                            cl::Hidden);

    cl::opt<unsigned>     MaximumTraceBlocks(
                            "shave-postra-scheduler-max-trace-blocks",
                            cl::init(5),
                            cl::desc("Set the maximum number of basic blocks that can be in a trace"),
                            cl::Hidden);

    cl::opt<unsigned>     MaximumTrackedMemoryOperations(
                            "shave-scheduler-max-tracked-mem-ops",
                            cl::init(64),
                            cl::desc("Set the maximum number of memory operations which should be tracked during dependency graph generation"),
                            cl::Hidden);

    cl::opt<double>       InnerLoopBlockWeightModifier(
                            "shave-postra-scheduler-inner-loop-block-weight-modifier",
                            cl::init(2.0f),
                            cl::desc("Modify the weight assigned to blocks apart of inner most loops"),
                            cl::Hidden);

    cl::opt<double>       OuterLoopBlockWeightModifier(
                            "shave-postra-scheduler-outer-loop-block-weight-modifier",
                            cl::init(1.0f),
                            cl::desc("Modify the weight assigned to blocks apart of outer loops"),
                            cl::Hidden);

    cl::opt<TraceGenerationDirections> 
                          TraceGenerationDirection(
                            "shave-postra-scheduler-trace-generation-direction",
                            cl::init(BothDirections),
                            cl::desc("Set the direction in which in the scheduler builds traces, starting with critical basic blocks"),
                            cl::values(
                              clEnumValN(ForwardsOnly, "forwards", "Generate traces by moving through each block's successors only"),
                              clEnumValN(BackwardsOnly, "backwards", "Generate traces by moving through each block's predecessors only"),
                              clEnumValN(BothDirections, "both", "Generate traces in both directions {default}")),
                            cl::Hidden);

    cl::opt<std::string>  MutationOrder("shave-postra-scheduler-functional-unit-mutation-order",
                            cl::init("LSU0,LSU1,IAU,SAU,VAU,CMU"),
                            cl::desc("Set the priority ordering for functional units when mutating instructions {default:LSU0,LSU1,IAU,SAU,VAU,CMU}"),
                            cl::Hidden);

    cl::opt<bool>         EnableInPlaceMutation("shave-inplace-mutation",
                            cl::init(true),
                            cl::desc("Enable the in-place mutation of already scheduled instructions"),
                            cl::Hidden);

    cl::opt<bool>         EnableInterruptFriendlyScheduling("shave-enable-interrupt-friendly-scheduling",
                            cl::init(false),
                            cl::desc("Prevent the scheduler from overlapping register lifetimes for anti-dependent instructions"),
                            cl::Hidden);

    cl::opt<bool>         EnableLoopLoadHoisting("shave-enable-loop-load-hoisting",
                            cl::init(false),
                            cl::desc("Allow the Post-RA Instruction Scheduler to hoist load instructions across the back-edge of a loop"),
                            cl::Hidden);

    cl::opt<unsigned>     NPU4LoadStoreDistance("shave-npu4-load-store-distance",
                            cl::init(0),
                            cl::desc("Set the minimum number of cycles between any load and store instructions on the same LSU when targetting NPU4"),
                            cl::Hidden);

    cl::alias            VPU4LoadStoreDistance("shave-vpu4-load-store-distance",
                            cl::desc("Alias for shave-npu4-load-store-distance"),
                            cl::aliasopt(NPU4LoadStoreDistance));

    cl::opt<Scheduling>  EnableSchedulingMethod("shave-scheduling-method",
                           cl::init(New),
                           cl::desc("Set the scheduling method for Pre-RA and Post-RA scheduling (specifically for re-enabling legacy methods)"),
                           cl::values(
                             clEnumValN(New, "default", "Use the new methods for scheduling in Pre-RA and Post-RA schedulers"),
                             clEnumValN(LegacyPreRA, "legacy-pre-ra", "Use the legacy method for Pre-RA scheduling"),
                             clEnumValN(LegacyPostRA, "legacy-post-ra", "Use the legacy method for Post-RA scheduling"),
                             clEnumValN(LegacyBoth, "legacy-both", "Use the legacy methods for scheduling in Pre-RA and Post-RA schedulers")),
                           cl::Hidden);

    // Stack instrumentation options
    cl::opt<bool>         StackOverflowChecking("shave-stack-overflow-checking",
                            cl::init(false),
                            cl::desc("Enable the stack overflow instrumentation"),
                            cl::Hidden);

    cl::opt<bool>         TrackStackUsage("shave-track-stack-usage",
                            cl::init(false),
                            cl::desc("Enable the stack usage instrumentation"),
                            cl::Hidden);

    // Option for controlling 3-element vector promotion to 4-element vectors
    cl::opt<ThreeElementVectorWideningActions>
                          WidenThreeElementVector(
                            "shave-widen-three-element-vectors",
                            cl::init(LoadOnly),
                            cl::desc("Selectively enable widening of three element vectors to equivalent four element vectors"),
                            cl::values(
                              clEnumValN(Disable, "disable", "Disable three element vector widening"),
                              clEnumValN(LoadOnly, "load-only", "Enable widening for three element vector load instructions only {default}"),
                              clEnumValN(StoreOnly, "store-only", "Enable widening for three element vector store instructions only"),
                              clEnumValN(LoadAndStore, "load-and-store", "Enable widening for both loads and stores")),
                            cl::Hidden);
    // Option to prevent emitting contant pools during BUILD_VECTOR lowering
    cl::opt<bool>         DisableConstantPools(
                            "shave-disable-constant-pools",
                            cl::init(false),
                            cl::desc("Do not emit constant pools during BUILD_VECTOR lowering"),
                            cl::Hidden);

    // Option to enable Machine Late Instructions Cleanup pass for SHAVE
    // (disabled by default due to interference with the scratch register usage)
    cl::opt<bool> EnableMachineLateInstrsCleanup(
        "shave-enable-machine-late-instrs-cleanup", cl::init(false),
        cl::desc("Enable Machine Late Instructions Cleanup Pass for SHAVE"),
        cl::Hidden);
}
