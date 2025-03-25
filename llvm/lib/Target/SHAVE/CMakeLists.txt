# ***************************************************************************
# INTEL CONFIDENTIAL
#
# Copyright 2025 Intel Corporation.
#
# This software and the related documents are Intel copyrighted materials, and
# your use of them is governed by the express license under which they were
# provided to you ("License"). Unless the License provides otherwise, you may
# not use, modify, copy, publish, distribute, disclose or transmit this software
# or the related documents without Intel's prior written permission.
#
# This software and the related documents are provided as is, with no express or
# implied warranties, other than those that are expressly stated in the License.
# ---------------------------------------------------------------------------
# File       :  CMakeLists.txt
# Description:  CMake file for SHAVE target
# ---------------------------------------------------------------------------

add_llvm_component_group(SHAVE)
set(LLVM_TARGET_DEFINITIONS SHAVE.td)


tablegen(LLVM SHAVEGenRegisterInfo.inc  -gen-register-info)
tablegen(LLVM SHAVEGenInstrInfo.inc     -gen-instr-info)
tablegen(LLVM SHAVEGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM SHAVEGenAsmWriter.inc     -gen-asm-writer)
tablegen(LLVM SHAVEGenDAGISel.inc       -gen-dag-isel)
tablegen(LLVM SHAVEGenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM SHAVEGenCallingConv.inc   -gen-callingconv)
tablegen(LLVM SHAVEGenConflicts.inc     -gen-shave-conflict-descriptions)
tablegen(LLVM SHAVEGenMutations.inc     -gen-shave-mutation-mappings)


add_public_tablegen_target(SHAVECommonTableGen)

add_llvm_target(SHAVECodeGen
  SHAVEAddressSimplifier.cpp
  SHAVEAntiDependencyBreaker.cpp
  SHAVEAsmPrinter.cpp
  SHAVEAsmPrinterInlineAsm.cpp
  SHAVEConflicts.cpp
  SHAVEFixLUTOperands.cpp
  SHAVEFrameLowering.cpp
  SHAVEInstrInfo.cpp
  SHAVEInterblockMovement.cpp
  SHAVEISelDAGtoDAG.cpp
  SHAVELiveRanges.cpp
  SHAVELowering.cpp
  SHAVEv3/SHAVEv3Lowering.cpp
  SHAVEMachineFunctionInfo.cpp
  SHAVEMCInstLower.cpp
  SHAVEPostRASchedule.cpp
  SHAVEPostRAScheduler.cpp
  SHAVEPostSchedulingPreemption.cpp
  SHAVEPredicator.cpp
  SHAVEPreemptionHandler.cpp
  SHAVEPreRASchedPeephole.cpp
  SHAVEPreRAScheduler.cpp
  SHAVEPreSched2Peephole.cpp
  SHAVERegisterInfo.cpp
  SHAVESchedulerBase.cpp
  SHAVESection.cpp
  SHAVESelectionDAGInfo.cpp
  SHAVESubtarget.cpp
  SHAVETargetMachine.cpp
  SHAVETargetObjectFile.cpp
  SHAVETargetTransformInfo.cpp
  SHAVEWidenVectorTypes.cpp
  SHAVEUtilities.cpp

  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  Core
  MC
  SelectionDAG
  Support
  Target
  SHAVEDesc
  SHAVEInfo
  
  ADD_TO_COMPONENT
  SHAVE
)

add_dependencies(LLVMSHAVECodeGen SHAVECommonTableGen)

add_subdirectory(TargetInfo)
#add_subdirectory(InstPrinter)
add_subdirectory(MCTargetDesc)
add_subdirectory(SHAVEv3)
