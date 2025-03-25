//===-- SHAVEv3Lowering.cpp - Instruction Selection for SHAVEv3 ---------------*- C++ -*-===//
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
// SHAVE Target Instruction lowering implementation
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "shave-lowering"
// SHAVEv3 FIXME: we possibly don't need all these includes
#include "llvm/ADT/ArrayRef.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MathExtras.h"

#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVELowering.h"
#include "SHAVEMachineFunctionInfo.h"
#include "SHAVERegisterInfo.h"
#include "SHAVESubtarget.h"
#include "SHAVETargetMachine.h"
#include "SHAVETargetObjectFile.h"

using namespace llvm;
#include "SHAVEGenCallingConv.inc"

void SHAVELowering::SHAVEv3Lowering(void)
{
  // 512-bit Vector register classes
  for (auto type : { MVT::v64i8, MVT::v32i16, MVT::v16i32, MVT::v32f16, MVT::v16f32 })
    addRegisterClass(type, &SHAVE::WVRF512RegClass);

  // 256-bit Vector register classes
  for (auto type : { MVT::v32i8, MVT::v16i16, MVT::v8i32, MVT::v16f16, MVT::v8f32 })
    addRegisterClass(type, &SHAVE::WVRF256_0RegClass);

  // 128-bit Vector register classes
  for (auto type : { MVT::v16i8, MVT::v8i16, MVT::v4i32, MVT::v8f16, MVT::v4f32 })
    addRegisterClass(type, &SHAVE::WVRF128_0RegClass);

  // 64-bit Vector register classes
  for (auto type : { MVT::v8i8, MVT::v4i16, MVT::v2i32, MVT::v4f16, MVT::v2f32 })
    addRegisterClass(type, &SHAVE::WVRF64_0RegClass);
}

void SHAVELowering::InitializeSHAVEv3LoweringActions() {
  auto customVectorTypes = { MVT::v64i8, MVT::v32i16, MVT::v16i32, MVT::v16f32, MVT::v32f16,
                             MVT::v32i8, MVT::v16i16, MVT::v8i32,  MVT::v16f16, MVT::v8f32, 
                             MVT::v16i8, MVT::v8i16,  MVT::v4i32,  MVT::v8f16,  MVT::v4f32, 
                             MVT::v8i8,  MVT::v4i16,  MVT::v2i32,  MVT::v4f16,  MVT::v2f32 };

  auto expandVectorTypes = { MVT::v64i8, MVT::v32i16, MVT::v16i32, MVT::v16f32, MVT::v32f16,
                             MVT::v32i8, MVT::v16i16, MVT::v8i32,  MVT::v16f16, MVT::v8f32, 
                             MVT::v16i8, MVT::v8i16,  MVT::v4i32,  MVT::v8f16,  MVT::v4f32, 
                             MVT::v8i8,  MVT::v4i16,  MVT::v2i32,  MVT::v4f16,  MVT::v2f32 };

  // Only use these types for lowering unique to Myriad4.0. Most of the functionality
  // for small vectors is defined in Myriad2.3 lowering and need not be overridden
  // in most cases
  auto smallVectorTypes = { MVT::v2i16, MVT::v2f16, MVT::v4i8, MVT::v2i8 };

  for (auto type : customVectorTypes) {
    setOperationAction(ISD::BUILD_VECTOR, type, Custom);
    setOperationAction(ISD::VECTOR_SHUFFLE, type, Custom);
    setOperationAction(ISD::SCALAR_TO_VECTOR, type, Custom);
  }

  for (auto type : customVectorTypes) {
    setOperationAction(ISD::SELECT, type, Custom);
    setOperationAction(ISD::VSELECT, type, Custom);
    setOperationAction(ISD::SELECT_CC, type, Expand);
    setOperationAction(ISD::SETCC, type, Custom);
  }

  for (auto type : expandVectorTypes)
    setOperationAction(ISD::SIGN_EXTEND_INREG, type, Expand);

  for (auto type : expandVectorTypes) {
    setOperationAction(ISD::INSERT_SUBVECTOR, type, Expand);
    setOperationAction(ISD::CONCAT_VECTORS, type, Custom);
    
    setOperationAction(ISD::BSWAP, type, Expand);
  }
  for (auto type : smallVectorTypes) {
    setOperationAction(ISD::INSERT_SUBVECTOR, type, Expand);
  }

  unsigned expandIntegerOperations[] = {
    ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM, ISD::SDIVREM, ISD::UDIVREM, ISD::ROTR };

  for (auto type : expandVectorTypes) {
    for (auto opcode : expandIntegerOperations) {
      setOperationAction(opcode, type, Expand);
    }
  }

  /* ToDo: implement lowering for ISD::FEXP and other instructions supported by NPU4+ */
  unsigned expandFloatingPointOperations[] = {
    ISD::FNEG, ISD::FREM, ISD::FSINCOS, ISD::FFLOOR, ISD::FDIV,
    ISD::FCEIL, ISD::FTRUNC, ISD::FMA, ISD::FLOG, ISD::FSQRT,
    ISD::FPOW, ISD::FLOG10, ISD::FEXP, ISD::FEXP2, ISD::FNEARBYINT,
    ISD::FSIN, ISD::FCOS};

  for (auto type : expandVectorTypes) {
    for (auto opcode : expandFloatingPointOperations) {
      setOperationAction(opcode, type, Expand);
    }
  }

  auto fpToIntTypes = { MVT::v64i8, MVT::v32i16, MVT::v16i32,
                        MVT::v32i8, MVT::v16i16, MVT::v8i32,
                        MVT::v16i8, MVT::v8i16,  MVT::v4i32,
                        MVT::v8i8,  MVT::v4i16,  MVT::v2i32,
                        MVT::v4i8,  MVT::v2i16 };

  for (auto type : fpToIntTypes) {
    setOperationAction(ISD::FP_TO_SINT, type, Legal);
    setOperationAction(ISD::FP_TO_UINT, type, Legal);
    setOperationAction(ISD::SINT_TO_FP, type, Legal);
    setOperationAction(ISD::UINT_TO_FP, type, Legal);
  }

  auto mstoreLegalTypes = {MVT::v8i32, MVT::v8f32,  MVT::v16i16, MVT::v16f16,
                           MVT::v32i8, MVT::v16i32, MVT::v32i16, MVT::v16f32,
                           MVT::v32f16, MVT::v64i8};

  for (auto type : expandVectorTypes)
    setOperationAction(ISD::MSTORE, type, Expand);

  for (auto type : mstoreLegalTypes)
    setOperationAction(ISD::MSTORE, type, Custom);

  setOperationAction(ISD::VECREDUCE_ADD, MVT::v32i16, Legal);
  setOperationAction(ISD::VECREDUCE_ADD, MVT::v64i8, Legal);

  for (auto type : {MVT::v64i8, MVT::v32i16, MVT::v16i32}) {
    setOperationAction(ISD::VECREDUCE_AND, type, Legal);
    setOperationAction(ISD::VECREDUCE_OR, type, Legal);
    setOperationAction(ISD::VECREDUCE_XOR, type, Legal);
  }

  for (auto type : { MVT::v16f32, MVT::v32f16,
                     MVT::v8f32,  MVT::v16f16,
                     MVT::v4f32,  MVT::v8f16 }) {
    setOperationAction(ISD::VECREDUCE_FADD, type, Legal);
  }

  // Set all trunc store types to Expand first, then set the ones we support to Legal
  // There are a lot of technically invalid type pairs here but the table doesn't care
  for (auto valueType : expandVectorTypes)
    for (auto pointerType : expandVectorTypes)
      setTruncStoreAction(valueType, pointerType, Expand);

  // There are no 32-bit or 16-bit truncating stores supported on Myriad4.0,
  // with the exception of v2i16->v2i8, signed and unsigned
  for (auto valueType : expandVectorTypes)
    for (auto pointerType : smallVectorTypes)
      setTruncStoreAction(valueType, pointerType, Expand);

  // FIXME: v*i64 types?
  setTruncStoreAction(MVT::v8i32, MVT::v8i1, Expand);

  for (auto valueType : {MVT::v16i16, MVT::v16i32})
    setTruncStoreAction(valueType, MVT::v16i1, Expand);

  for (auto valueType : {MVT::v32i8, MVT::v32i16})
    setTruncStoreAction(valueType, MVT::v32i1, Expand);

  setTruncStoreAction(MVT::v64i8, MVT::v64i1, Expand);

  setTruncStoreAction(MVT::v16f32, MVT::v16f16, Legal);
  setTruncStoreAction(MVT::v16i32, MVT::v16i16, Legal);
  setTruncStoreAction(MVT::v16i32, MVT::v16i8, Legal);
  setTruncStoreAction(MVT::v32i16, MVT::v32i8, Legal);

  // And do the same for extending loads
  for (auto valueType : expandVectorTypes) {
    for (auto pointerType : expandVectorTypes) {
      setLoadExtAction(ISD::EXTLOAD, valueType, pointerType, Expand);
      setLoadExtAction(ISD::SEXTLOAD, valueType, pointerType, Expand);
      setLoadExtAction(ISD::ZEXTLOAD, valueType, pointerType, Expand);
    }
  }

  for (auto valueType : expandVectorTypes) {
    for (auto pointerType : smallVectorTypes) {
      setLoadExtAction(ISD::EXTLOAD, valueType, pointerType, Expand);
      setLoadExtAction(ISD::SEXTLOAD, valueType, pointerType, Expand);
      setLoadExtAction(ISD::ZEXTLOAD, valueType, pointerType, Expand);
    }
  }

  // 512-bit extending loads are supported
  setLoadExtAction(ISD::EXTLOAD, MVT::v16f32, MVT::v16f16, Legal);
  for (auto opcode : { ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD }) {
    setLoadExtAction(opcode, MVT::v16i32, MVT::v16i16, Legal);
    setLoadExtAction(opcode, MVT::v16i32, MVT::v16i8, Legal);
    setLoadExtAction(opcode, MVT::v32i16, MVT::v32i8, Legal);
  }

  setOperationAction(ISD::LOAD, MVT::i64, Custom);
  setOperationAction(ISD::STORE, MVT::i64, Custom);

  // Subvector Extract
  const auto subvectorExtractTypes = { MVT::v32i8, MVT::v16i16, MVT::v8i32,  MVT::v16f16, MVT::v8f32,
                                       MVT::v16i8, MVT::v8i16,  MVT::v4i32,  MVT::v8f16,  MVT::v4f32,
                                       MVT::v8i8,  MVT::v4i16,  MVT::v2i32,  MVT::v4f16,  MVT::v2f32,
                                       MVT::v4i8,  MVT::v2i16,               MVT::v2f16,
                                       MVT::v2i8
                                     };
  for (const auto &type : subvectorExtractTypes)
    setOperationAction(ISD::EXTRACT_SUBVECTOR, type, Custom);

  // Subvector Insert
  const auto subvectorInsertTypes = { MVT::v64i8, MVT::v32i16, MVT::v16i32, MVT::v32f16, MVT::v16f32,
                                      MVT::v32i8, MVT::v16i16, MVT::v8i32,  MVT::v16f16, MVT::v8f32,
                                      MVT::v16i8, MVT::v8i16,  MVT::v4i32,  MVT::v8f16,  MVT::v4f32,
                                      MVT::v8i8,  MVT::v4i16,  MVT::v2i32,  MVT::v4f16,  MVT::v2f32,
                                      MVT::v4i8
                                    };
  for (const auto &type : subvectorInsertTypes)
    setOperationAction(ISD::INSERT_SUBVECTOR, type, Legal);

  // MIN/MAX convert to equivalent SHAVEISD opcodes
  const auto minMaxType = { MVT::v64i8, MVT::v32i16, MVT::v16i32,
                            MVT::v32i8, MVT::v16i16, MVT::v8i32,
                            MVT::v16i8, MVT::v8i16,  MVT::v4i32,
                            MVT::v8i8,  MVT::v4i16,  MVT::v2i32,
                            MVT::v4i8,  MVT::v2i16,
                            MVT::v2i8
                          };
  for (const auto& type : minMaxType) {
    setOperationAction(ISD::SMIN, type, Custom);
    setOperationAction(ISD::UMIN, type, Custom);
    setOperationAction(ISD::SMAX, type, Custom);
    setOperationAction(ISD::UMAX, type, Custom);
  }

  auto fcopysignTypes = { MVT::v16f32, MVT::v32f16,
                          MVT::v8f32,  MVT::v16f16,
                          MVT::v4f32,  MVT::v8f16,
                          MVT::v2f32,  MVT::v4f16,
                                       MVT::v2f16
                        };
  for (const auto& type : fcopysignTypes)
    setOperationAction(ISD::FCOPYSIGN, type, Custom);

  auto floats64bitTo512bit = { MVT::v16f32, MVT::v32f16,
                               MVT::v8f32,  MVT::v16f16,
                               MVT::v4f32,  MVT::v8f16,
                               MVT::v2f32,  MVT::v4f16 };
  for (const auto& type : floats64bitTo512bit) {
    setOperationAction(ISD::FABS, type, Legal);
    setOperationAction(ISD::FMINNUM, type, Custom);
    setOperationAction(ISD::FMAXNUM, type, Custom);
    // Other variants of min/max with requirements for +/-0 that do not match CMU.MIN/MAX
    setOperationAction(ISD::FMINIMUM, type, Custom);
    setOperationAction(ISD::FMAXIMUM, type, Custom);
  }

  for (const auto& type : { MVT::v32f16, MVT::v16f16, MVT::v8f16, MVT::v4f16, MVT::v2f16 }) {
    // ULP 0 instructions
    setOperationAction(ISD::FSQRT, type, Custom);
    // ULP 1 instructions
    setOperationAction(ISD::FLOG, type, Custom);
    setOperationAction(ISD::FEXP, type, Custom);
  }
}

//
// Lowering support for vector shuffle operations
//

enum ShufflePatternV3 {
  Splat,
  Unknown
};

//
// Pattern matching functions
//

void SHAVEv3ShuffleLowering::checkIdentity(std::vector<bool> &match, ShuffleInfo &info, SourceVector source) {
  int base = source == Left ? 0 : inputASize;
  int limit = source == Left ? inputASize : inputASize + inputBSize;
  for (int i = 0; i < (int) mask.size(); ++i)
    match[i] = (mask[i] == (base + i) && mask[i] < limit);
}

void SHAVEv3ShuffleLowering::checkSplat(std::vector<bool> &match, ShuffleInfo &info, SourceVector source) {
  std::vector<unsigned int> counters(match.size(), 0);
  int base = source == Left ? 0 : inputASize;
  int upperBound = source == Left ? inputASize : inputASize + inputBSize;

  for (unsigned int i = 0; i < mask.size(); ++i)
    if (mask[i] > base && mask[i] < upperBound)
      counters[mask[i] - base]++;

  int index = base;
  unsigned int max = counters[0];
  for (unsigned int i = 1; i < counters.size(); ++i) {
    if (counters[i] > max) {
      max = counters[i];
      index = base + i;
    }
  }

  for (unsigned int i = 0; i < match.size(); ++i)
    if (mask[i] == index)
      match[i] = true;

  info.splatIndex = index;
}

void SHAVEv3ShuffleLowering::checkRotate(std::vector<bool> &match, ShuffleInfo &info, SourceVector source) {
  std::vector<unsigned int> offsetCounters(match.size(), 0);
  int base = source == Left ? 0 : inputASize;
  int size = source == Left ? inputASize : inputBSize;
  int upperBound = source == Left ? inputASize : inputASize + inputBSize;

  // Find which offset is the most used for a rotate
  for (unsigned int i = 0; i < mask.size(); ++i)
    if (mask[i] >= base && mask[i] < upperBound)
      offsetCounters[(i - (mask[i] - base) + size) & (size - 1)]++;

  unsigned int offset = 0, max = offsetCounters[0];
  for (unsigned int i = 1; i < offsetCounters.size(); ++i) {
    if (offsetCounters[i] > max) {
      max = offsetCounters[i];
      offset = i;
    }
  }

  for (unsigned int i = 0; i < match.size(); ++i)
    if (mask[i] >= base && mask[i] < upperBound)
      if (((i - (mask[i] - base) + size) & (size - 1)) == offset)
        match[i] = true;

  info.offset = offset;
}

void SHAVEv3ShuffleLowering::checkInterleave(std::vector<bool> &match, ShuffleInfo &info, SourceVector source) {
  int otherOffset = inputASize;
  if (source == Left && getOtherSource(source).isUndef()) {
    info.interleaveBothLeft = true;
    otherOffset = 0;
  }

  std::vector<bool> output0(mask.size(), 0);
  for (int i = 0; i < (int) mask.size(); ++i) {
    if (i % 2 == 0) {
      if (mask[i] == i / 2)
        output0[i] = true;
    }
    else {
      if (mask[i] == (otherOffset + i / 2))
        output0[i] = true;
    }
  }

  // Second output is only valid if inputs have the full vector width
  // as it uses the high 256-bits of the registers
  std::vector<bool> output1(mask.size(), 0);
  if ((sourceA.getScalarValueSizeInBits() * inputASize) == 512u && inputASize == inputBSize) {
    for (int i = 0; i < (int) mask.size(); ++i) {
      if (i % 2 == 0) {
        if (mask[i] == (otherOffset / 2) + (i / 2))
          output1[i] = true;
      }
      else {
        if (mask[i] == (otherOffset / 2) + (otherOffset + i / 2))
          output1[i] = true;
      }
    }
  }

  if (std::count(output1.begin(), output1.end(), true) > std::count(output0.begin(), output0.end(), true)) {
    match = output1;
    info.interleaveOutput = 1;
  }
  else {
    match = output0;
    info.interleaveOutput = 0;
  }
}

void SHAVEv3ShuffleLowering::checkDeInterleave(std::vector<bool> &match, ShuffleInfo &info, SourceVector source) {
  int upperLimit = inputASize + inputBSize;
  if (source == Left && getOtherSource(source).isUndef()) {
    info.interleaveBothLeft = true;
    upperLimit = inputASize;
  }

  std::vector<bool> output0(mask.size(), 0);
  for (int i = 0; i < (int) mask.size(); ++i)
    if (mask[i] == ((i * 2) + 1) % upperLimit)
      output0[i] = true;

  std::vector<bool> output1(mask.size(), 0);
  for (int i = 0; i < (int) mask.size(); ++i)
    if (mask[i] == (i * 2) % upperLimit)
      output1[i] = true;

  if (std::count(output1.begin(), output1.end(), true) > std::count(output0.begin(), output0.end(), true)) {
    match = output1;
    info.interleaveOutput = 1;
  }
  else {
    match = output0;
    info.interleaveOutput = 0;
  }
}

void SHAVEv3ShuffleLowering::checkAlignvec(std::vector<bool> &match, ShuffleInfo &info, SourceVector source) {
  std::vector<unsigned int> offsetCounters(match.size(), 0);
  int base0 = source == Left ? 0 : inputASize;
  int upperBound0 = source == Left ? inputASize : inputASize * 2;

  // Count offsets used for the first input vector
  for (unsigned int i = 0; i < mask.size(); ++i)
    if (mask[i] >= base0 && mask[i] < upperBound0 && (mask[i] - base0) > (int) i)
      offsetCounters[(mask[i] - base0) - i]++;

  int base1 = source == Right ? 0 : inputASize;
  int upperBound1 = source == Right ? inputASize : inputASize * 2;

  // Count offsets used for the second input vector
  for (int i = 0; i < (int) mask.size(); ++i)
    if (mask[i] >= base1 && mask[i] < upperBound1 && (mask[i] - base1) < (int) i)
      offsetCounters[inputASize - (i - (mask[i] - base1))]++;

  // Find the most used offset
  unsigned int offset = 0, max = offsetCounters[0];
  for (unsigned int i = 1; i < offsetCounters.size(); ++i) {
    if (offsetCounters[i] > max) {
      max = offsetCounters[i];
      offset = i;
    }
  }

  for (unsigned int i = 0; i < match.size(); ++i)
    if (mask[i] >= base0 && mask[i] < upperBound0 && ((mask[i] - base0) - i) == offset)
      match[i] = true;
    else if (mask[i] >= base1 && mask[i] < upperBound1 && (inputASize - (i - (mask[i] - base1))) == offset)
      match[i] = true;

  info.offset = offset;
}

void SHAVEv3ShuffleLowering::checkCombine(std::vector<bool> &match, ShuffleInfo &info, SourceVector source) {
  // Start by finding the identity elements in the "destination" vector
  std::vector<bool> identityElements(mask.size(), 0);
  int base = source == Left ? 0 : inputASize;
  for (int i = 0; i < (int) mask.size(); ++i)
    identityElements[i] = (mask[i] == (base + i));

  // Now find the elements in the "source" vector that map 1:1 with the "destination"
  std::vector<bool> mappedElements(mask.size(), 0);
  base = source == Right ? 0 : inputASize;
  for (int i = 0; i < (int) mask.size(); ++i)
    mappedElements[i] = (mask[i] == (base + i));

  // Now look for a CMB compatible pattern in the mapped elements
  std::vector<bool> mappedMask;
  int mappedMaskElements = 0;
  unsigned int combineSize = 0, elementsPerGroup = 0;

  unsigned int elementBitsize = sourceA.getValueType().getScalarSizeInBits();
  unsigned int vectorBitsize = outputType.getSizeInBits();

  for (unsigned int bitsize : { 512, 256, 128, 64, 32 }) {
    unsigned int numGroups = vectorBitsize / bitsize;
    unsigned int currentElementsPerGroup = bitsize / elementBitsize;

    for (unsigned int i = 0; i < numGroups; ++i) {
      // Generate mask for this group
      std::vector<bool> groupIdentityElements(identityElements.begin() + i*currentElementsPerGroup, identityElements.begin() + (i*currentElementsPerGroup + currentElementsPerGroup));
      std::vector<bool> groupElements(mappedElements.begin() + i*currentElementsPerGroup, mappedElements.begin() + (i*currentElementsPerGroup + currentElementsPerGroup));
      std::vector<bool> currentMask;
      int matchingElements = 0;

      for (unsigned int j = 0; j < 4; ++j) {
        int enableElements = 0;
        for (unsigned int k = 0; k < currentElementsPerGroup /4; ++k) {
          if (groupElements[j*(currentElementsPerGroup / 4) + k])
            enableElements++;
          if (groupIdentityElements[j*(currentElementsPerGroup / 4) + k])
            enableElements--;
        }

        if (enableElements > 0) {
          currentMask.push_back(true);
          matchingElements += enableElements;
        }
        else {
          currentMask.push_back(false);
        }
      }

      // All elements of the mask are set to false so this is a no-op
      if (matchingElements == 0)
        continue;

      // Check how well this pattern fits the rest of the groups
      for (unsigned int group = 0; group < numGroups; ++group) {
        if (group == i)
          continue;

        std::vector<bool> groupIdentityElements(identityElements.begin() + group*currentElementsPerGroup, identityElements.begin() + (group*currentElementsPerGroup + currentElementsPerGroup));
        std::vector<bool> groupElements(mappedElements.begin() + group*currentElementsPerGroup, mappedElements.begin() + (group*currentElementsPerGroup + currentElementsPerGroup));

        for (unsigned int j = 0; j < 4; ++j) {
          if (currentMask[j]) {
            for (unsigned int k = 0; k < currentElementsPerGroup / 4; ++k) {
              if (groupElements[j*(currentElementsPerGroup / 4) + k])
                matchingElements++;
              if (groupIdentityElements[j*(currentElementsPerGroup / 4) + k])
                matchingElements--;
            }
          }
        }
      }

      if (matchingElements > mappedMaskElements) {
        mappedMaskElements = matchingElements;
        mappedMask = currentMask;
        combineSize = bitsize;
        elementsPerGroup = currentElementsPerGroup;
      }
    }
  }

  if (mappedMaskElements > 0) {
    info.combineMask = mappedMask;
    info.combineSize = combineSize;

    std::vector<bool> groupMask;
    unsigned int elementsPerMaskElement = (combineSize / 4) / elementBitsize;
    for (bool maskElement : mappedMask)
      for (unsigned int i = 0; i < elementsPerMaskElement; ++i)
        groupMask.push_back(maskElement);

    std::vector<bool> generatedMask;
    unsigned int numGroups = (vectorBitsize / combineSize);
    for (unsigned int group = 0; group < numGroups; ++group) {
      for (unsigned int maskElement = 0; maskElement < groupMask.size(); ++maskElement) {
        if (groupMask[maskElement]) {
          if (mappedElements[(group*elementsPerGroup) + maskElement])
            generatedMask.push_back(true);
          else
            generatedMask.push_back(false);
        }
        else {
          if (identityElements[(group*elementsPerGroup) + maskElement])
            generatedMask.push_back(true);
          else
            generatedMask.push_back(false);
        }
      }
    }

    assert(generatedMask.size() == mask.size());

    match = generatedMask;
  }
}

void SHAVEv3ShuffleLowering::checkCompress(std::vector<bool> &match, ShuffleInfo &info, SourceVector source) {
  int base = source == Left ? 0 : inputASize;
  int size = source == Left ? inputASize : inputBSize;

  struct CompressPoint {
    int element;
    unsigned int index, depth;
    std::shared_ptr<CompressPoint> prev;

    CompressPoint(int element, unsigned int index, unsigned int depth, std::shared_ptr<CompressPoint> prev = nullptr)
      : element(element), index(index), depth(depth), prev(prev) {}
  };
  std::vector<std::shared_ptr<CompressPoint>> allPoints; // Sorted by depth, lowest to highest

  // Get all possible ranges of elements which can be lowered to a compress
  for (unsigned int i = 0; i < mask.size(); ++i) {
    int element = mask[i] - base;
    if (element < 0 || element >= size)
      continue;

    bool inserted = false;
    for (auto it = allPoints.rbegin(); it != allPoints.rend(); ++it) {
      auto &point = *it;

      // The shuffle element is after the last element
      // And there are enough elements between the current masked element and the last used
      // by COMPRESS to fill in the gap between lastIndex and i
      if (element > point->element && ((int) (i - point->index)) <= (element - point->element)) {
        auto newPoint = std::make_shared<CompressPoint>(element, i, point->depth + 1, point);

        for (auto insertPoint = allPoints.rbegin(); insertPoint != allPoints.rend(); ++insertPoint) {
          if (newPoint->depth > (*insertPoint)->depth) {
            allPoints.insert(insertPoint.base(), newPoint);
            break;
          }
        }

        inserted = true;
        break;
      }
    }

    // Only add the point if this is the first point in the mask, or if there are enough
    // elements before "element" in the source to fill in any gaps from index zero to here
    if (!inserted && element >= (int) i)
      allPoints.insert(allPoints.begin(), std::make_shared<CompressPoint>(element, i, 1));
  }

  if (allPoints.empty())
    return;

  auto point = allPoints.back();
  if (point->depth > 1u) {
    info.compressMask = 0;
    int last = -1;
    unsigned int lastIndex = 0;

    // Build the compress mask in reverse order, starting with the very last element in the compression
    do {
      match[point->index] = true;
      info.compressMask |= 1ull << point->element; // Set the bit for this element

      // Set the bits for elements inbetween this point and the last to fill in any gaps in the output vector
      int gap = lastIndex == 0 ? 0 : ((int) (lastIndex - point->index)) - 1; // No gap after the final element (i.e. the first processed here)
      for (int index = point->element + 1; index <= last && gap > 0; ++index, --gap)
        info.compressMask |= 1ull << index;

      last = point->element;
      lastIndex = point->index;

      point = point->prev;
    } while (point != nullptr);
  }
}

void SHAVEv3ShuffleLowering::checkPermuteBlend(std::vector<bool>& match, ShuffleInfo& info, SourceVector source) {
  // First check the blend part, this is just an identity of the other vector source
  checkIdentity(match, info, source == Left ? Right : Left);

  // The rest is a general permute
  checkPermute(match, info, source);
}

void SHAVEv3ShuffleLowering::checkPermute(std::vector<bool>& match, ShuffleInfo& info, SourceVector source) {
  int base = source == Left ? 0 : inputASize;
  int limit = source == Left ? inputASize : inputASize + inputBSize;

  // This is a general permute, so mark all elements from the source as matched
  for (int i = 0; i < (int)mask.size(); ++i)
    if(mask[i] >= base && mask[i] < limit)
      match[i] = true;
}

//
// Lowering helper functions
//

SDValue SHAVEv3ShuffleLowering::getInterleaveOtherSource(SourceVector source) const {
  SDValue otherSource = getOtherSource(source);
  if (usedInfo.interleaveBothLeft) // Right is undef, so the check function used the left as both inputs
    return getSource(source);
  return otherSource;
}

//
// Lowering functions
//

SDValue SHAVEv3ShuffleLowering::lowerSplat(const std::vector<bool> &match, SourceVector source) {
  DEBUG(dbgs() << "SHAVEv3ShuffleLowering: Lowering splat with element matches ");
  DEBUG(for (bool value : match) dbgs() << value);
  DEBUG(dbgs() << "\n");

  unsigned int splatIndex = usedInfo.splatIndex;

  // Adjust the index if we're splatting an element in the right-hand operand
  if (source == Right)
    splatIndex -= inputASize;

  return DAG.getNode(SHAVEISD::LANESPLAT, dbgLoc, outputType, getSource(source), DAG.getConstant(splatIndex, dbgLoc, MVT::i32));
}

SDValue SHAVEv3ShuffleLowering::lowerRotate(const std::vector<bool> &match, SourceVector source) {
  DEBUG(dbgs() << "SHAVEv3ShuffleLowering: Lowering rotate with element matches ");
  DEBUG(for (bool value : match) dbgs() << value);
  DEBUG(dbgs() << "\n");

  // Reverse the offset value for the rotate. The function checkRotate will detect rotate right offsets in
  // LLVM reckoning (i.e. elements arragned left-to-right) but in hardware, it is backwards (elements arranged
  // right-to-left). Reverse the offset to reverse the rotate to the correct orientation in hardware.
  unsigned int rotate = (outputType.getVectorNumElements() - usedInfo.offset) * (outputType.getScalarSizeInBits() / 8u);
  return DAG.getNode(SHAVEISD::ROTATE_VECTOR, dbgLoc, outputType, getSource(source), DAG.getConstant(rotate, dbgLoc, MVT::i8));
}

SDValue SHAVEv3ShuffleLowering::lowerInterleave(const std::vector<bool> &match, SourceVector source) {
  DEBUG(dbgs() << "SHAVEv3ShuffleLowering: Lowering interleave with element matches ");
  DEBUG(for (bool value : match) dbgs() << value);
  DEBUG(dbgs() << "\n");

  SDValue interleave;
  if (inputASize < (int) outputType.getVectorNumElements())
    interleave = DAG.getNode(SHAVEISD::INTERLEAVE_VECTORS_COMBINE, dbgLoc, DAG.getVTList(outputType, outputType), getSource(source), getInterleaveOtherSource(source));
  else
    interleave = DAG.getNode(SHAVEISD::INTERLEAVE_VECTORS, dbgLoc, DAG.getVTList(outputType, outputType), getSource(source), getInterleaveOtherSource(source));

  return SDValue(interleave.getNode(), usedInfo.interleaveOutput);
}

SDValue SHAVEv3ShuffleLowering::lowerDeInterleave(const std::vector<bool> &match, SourceVector source) {
  DEBUG(dbgs() << "SHAVEv3ShuffleLowering: Lowering de-interleave with element matches ");
  DEBUG(for (bool value : match) dbgs() << value);
  DEBUG(dbgs() << "\n");

  SDValue interleave;
  interleave = DAG.getNode(SHAVEISD::DEINTERLEAVE_VECTORS, dbgLoc, DAG.getVTList(outputType, outputType), getSource(source), getInterleaveOtherSource(source));

  return SDValue(interleave.getNode(), usedInfo.interleaveOutput);
}

SDValue SHAVEv3ShuffleLowering::lowerAlignvec(const std::vector<bool> &match, SourceVector source) {
  DEBUG(dbgs() << "SHAVEv3ShuffleLowering: Lowering alignvec with element matches ");
  DEBUG(for (bool value : match) dbgs() << value);
  DEBUG(dbgs() << "\n");

  unsigned int offset = usedInfo.offset * (outputType.getScalarSizeInBits() / 8);
  return DAG.getNode(SHAVEISD::ALIGNVEC, dbgLoc, outputType, getSource(source), getOtherSource(source), DAG.getConstant(offset, dbgLoc, MVT::i8));
}

SDValue SHAVEv3ShuffleLowering::lowerCombine(const std::vector<bool> &match, SourceVector source) {
  DEBUG(dbgs() << "SHAVEv3ShuffleLowering: Lowering combine with element matches ");
  DEBUG(for (bool value : match) dbgs() << value);
  DEBUG(dbgs() << "\n");

  SmallVector<SDValue, 6> operands = {
    getSource(source),
    getOtherSource(source),
    DAG.getConstant(usedInfo.combineSize, dbgLoc, MVT::i32),
    DAG.getConstant(usedInfo.combineMask[0], dbgLoc, MVT::i8),
    DAG.getConstant(usedInfo.combineMask[1], dbgLoc, MVT::i8),
    DAG.getConstant(usedInfo.combineMask[2], dbgLoc, MVT::i8),
    DAG.getConstant(usedInfo.combineMask[3], dbgLoc, MVT::i8)
  };

  return DAG.getNode(SHAVEISD::COMBINE_VECTORS, dbgLoc, outputType, operands);
}

SDValue SHAVEv3ShuffleLowering::lowerCompress(const std::vector<bool> &match, SourceVector source) {
  DEBUG(dbgs() << "SHAVEv3ShuffleLowering: Lowering compress with element matches ");
  DEBUG(for (bool value : match) dbgs() << value);
  DEBUG(dbgs() << " and mask " << usedInfo.compressMask << "\n");

  SmallVector<SDValue, 2> operands = {
    getSource(source),
    DAG.getConstant(usedInfo.compressMask, dbgLoc, outputType.getScalarSizeInBits() == 8u ? MVT::i64 : MVT::i32)
  };

  return DAG.getNode(SHAVEISD::COMPRESS_VECTOR, dbgLoc, outputType, operands);
}

SDValue SHAVEv3ShuffleLowering::lowerPermute(const std::vector<bool>& match, SourceVector source, bool blend) {
  DEBUG(dbgs() << "SHAVEv3ShuffleLowering: Lowering permute with element matches ");
  DEBUG(for (bool value : match) dbgs() << value);
  DEBUG(dbgs() << (blend ? "with blend" : "") << "\n");

  MVT maskType = MVT::INVALID_SIMPLE_VALUE_TYPE;
  unsigned int enableBit = 0u;
  switch (outputType.getVectorElementType().getSizeInBits()) {
  case 32: maskType = MVT::i32; enableBit = 16; break;
  case 16: maskType = MVT::i16; enableBit = 32; break;
  case 8:  maskType = MVT::i8;  enableBit = 64; break;
  default: llvm_unreachable("Unsupported vector element size in permute lowering");
  }

  int maskAdjust = source == Left ? 0 : inputASize;
  int maskLimit = source == Left ? inputASize : inputASize + inputBSize;
  SmallVector<SDValue> permuteMaskOperands;
  permuteMaskOperands.reserve(mask.size());
  for (unsigned int i = 0; i < mask.size(); ++i) {
    unsigned int index = 0;

    if (match[i]) {
      if (blend) {
        if (mask[i] >= maskAdjust && mask[i] < maskLimit)
          index = (mask[i] - maskAdjust) | enableBit;
      }
      else {
        index = mask[i] - maskAdjust;
      }
    }

    permuteMaskOperands.push_back(DAG.getConstant(index, dbgLoc, maskType));
  }

  SDValue permuteMask = DAG.getBuildVector(MVT::getVectorVT(maskType, mask.size()), dbgLoc, permuteMaskOperands);

  SmallVector<SDValue, 2> operands = {
    getSource(source),
    permuteMask
  };

  unsigned int opcode = blend ? SHAVEISD::PERMUTE_BLEND_VECTORS : SHAVEISD::PERMUTE_VECTOR;

  return DAG.getNode(opcode, dbgLoc, outputType, operands);
}

namespace {
    void updateCopySizesForIndex(std::vector<std::pair<unsigned int, unsigned int>>& copyList, unsigned int idx, unsigned int maxLength) {
      unsigned int gs = 2;
      while (gs <= maxLength && idx >= gs && (copyList[idx - gs].first == (gs / 2))) {
        unsigned int sourceIndex = copyList[idx - gs].second;
        // Check if both subvectors will be aligned if we double the curent copy size
        // And the copy size doesn't exceed the maximum length of the consecutive source element sequence
        if ((sourceIndex % gs) == 0 && (idx % gs) == 0) {
          copyList[idx - gs].first = gs;
        } else {
          return;
        }
        gs *= 2;
      }
    }

    void regroupCopies(std::vector<std::pair<unsigned int, unsigned int>> &copyList, unsigned int eltSize) {
        // copyList is a vector of pairs for each shuffle index where the first is the maximum amount of elements
        // we can safely copy starting from the current shuffle index
        // The second in pair is the index in source vector the element is copied from
        // Initially the amount of elements to copy is 1 if the element needed to be copied after shuffle, and 0 otherwise

        unsigned int curLength = 0;
        for (unsigned int i = 0; i < copyList.size(); ++i) {
            if (i % 2 == 0) {
              // When the currLength builds up to a minimal group size, we can check if
              // the current group can be copied as a subvector, and then if it can be
              // combined into bigger groups
              updateCopySizesForIndex(copyList, i, curLength);
            }

            // Update max length of a sequence of consecutive elements from the source
            if (i >= 1 && (copyList[i].first > 0) &&
                (copyList[i].second == copyList[i - 1].second + 1)) {
              curLength += 1;
            } else {
              curLength = copyList[i].first;
            }
        }
        // Process the tail
        updateCopySizesForIndex(copyList, copyList.size(), curLength);
    }

    struct ShuffleCopy {
      SDValue copySource;
      unsigned int destIndex;
      unsigned int sourceIndex;
      unsigned int copySize;
      ShuffleCopy(SDValue src, unsigned int dstIdx, unsigned int srcIdx,
                  unsigned int sz)
          : copySource(src), destIndex(dstIdx), sourceIndex(srcIdx),
            copySize(sz) {}
    };

    void getShuffleCopies(std::vector<ShuffleCopy> &result,
        const std::vector<std::pair<unsigned int, unsigned int>>& copyList,
        SDValue source) {
      unsigned int i = 0;
      while (i < copyList.size()) {
        unsigned int copySize = copyList[i].first;
        if (copySize > 0) {
          result.push_back(
              ShuffleCopy(source, i, copyList[i].second, copySize));
          i += copySize;
        } else {
          i += 1;
        }
      }
    }
 }

SDValue SHAVEv3ShuffleLowering::insertCopies(SDValue destVector, std::vector<bool> &match) {
  unsigned int destSize = match.size();
  std::vector<std::pair<unsigned int, unsigned int>> copiesFromA(destSize);
  std::vector<std::pair<unsigned int, unsigned int>> copiesFromB(destSize);

  for (unsigned int element = 0; element < match.size(); ++element) {
    if (!match[element]) {
      if (mask[element] < inputASize) {
        copiesFromA[element] = std::make_pair(1, mask[element]);
      } else {
        copiesFromB[element] = std::make_pair(1, mask[element] - inputASize);
      }
    }
  }

  unsigned int eltSize =
      destVector.getValueType().getVectorElementType().getSizeInBits();
  regroupCopies(copiesFromA, eltSize);
  regroupCopies(copiesFromB, eltSize);

  std::vector<ShuffleCopy> shuffleCopies;
  getShuffleCopies(shuffleCopies, copiesFromA, sourceA);
  getShuffleCopies(shuffleCopies, copiesFromB, sourceB);

  for (unsigned int i = 0; i < shuffleCopies.size(); ++i) {
    unsigned int copySize = shuffleCopies[i].copySize;
    if (copySize == 1){
      SDValue extract = DAG.getNode(ISD::EXTRACT_VECTOR_ELT,
          dbgLoc, outputType.getScalarType(),
          shuffleCopies[i].copySource,
          DAG.getConstant(shuffleCopies[i].sourceIndex, dbgLoc, MVT::i32));

      destVector = DAG.getNode(ISD::INSERT_VECTOR_ELT, dbgLoc, outputType, destVector, extract,
          DAG.getConstant(shuffleCopies[i].destIndex, dbgLoc, MVT::i32));
    } else {
      EVT subvectorVT = EVT::getVectorVT(
          *DAG.getContext(), outputType.getVectorElementType(), copySize, false);
      SDValue extract = DAG.getNode(
          ISD::EXTRACT_SUBVECTOR, dbgLoc, subvectorVT, shuffleCopies[i].copySource,
                      DAG.getConstant(shuffleCopies[i].sourceIndex, dbgLoc, MVT::i32));
      destVector =
          DAG.getNode(ISD::INSERT_SUBVECTOR, dbgLoc, outputType, destVector,
                      extract, DAG.getConstant(shuffleCopies[i].destIndex, dbgLoc, MVT::i32));
    }
  }
  return destVector;
}

SDValue SHAVEv3ShuffleLowering::lower() {
  SourceVector source = Left;
  ShufflePattern pattern = Identity;
  std::vector<bool> patternMatch(mask.size(), 0);
  unsigned int matching = 0;
  ShuffleInfo info;
  const bool hasPermute = SHAVEST.hasFeature(SHAVE::HasPERM_Feature);

  DEBUG(dbgs() << "SHAVEv3ShuffleLowering: Lowering shuffle mask <");
  DEBUG(for (const auto idx : mask) dbgs() << idx << ", ");
  DEBUG(dbgs() << ">\n");
  DEBUG(dbgs() << "InputA: "; sourceA.dump());
  DEBUG(dbgs() << "InputB: "; sourceB.dump());

  for (unsigned int currentPattern = Identity; currentPattern < NumPatterns; ++currentPattern) {
    for (unsigned int vector = Left; vector < NumSources; ++vector) {
      DEBUG(dbgs() << "  Vector Source " << (vector == Left ? "Left" : "Right") << "\n");
      std::vector<bool> currentMatch(mask.size(), 0);
      ShuffleInfo currentInfo;

      DEBUG(dbgs() << "    ShufflePattern ");
      switch ((ShufflePattern) currentPattern) {
      case Identity:
        DEBUG(dbgs() << "Identity");
        checkIdentity(currentMatch, currentInfo, (SourceVector) vector);
        break;
      case Splat:
        DEBUG(dbgs() << "Splat");
        checkSplat(currentMatch, currentInfo, (SourceVector) vector);
        break;
      case Rotate:
        DEBUG(dbgs() << "Rotate");
        // Rotate is only possible if we are shuffling two 512-bit vectors into a 512-bit vector result
        if (outputType.getSizeInBits() == 512u && (int) outputType.getVectorNumElements() == (source == Left ? inputASize : inputBSize))
          checkRotate(currentMatch, currentInfo, (SourceVector) vector);
        break;
      case Interleave:
        DEBUG(dbgs() << "Interleave");
        if ((int) outputType.getVectorNumElements() <= inputASize * 2 && inputASize == inputBSize)
          checkInterleave(currentMatch, currentInfo, (SourceVector) vector);
        break;
      case DeInterleave:
        DEBUG(dbgs() << "De-Interleave");
        // De-Interleave is only possible when both inputs and the output have the same size, and all are 512-bits
        if (outputType.getSizeInBits() == 512u && (int) outputType.getVectorNumElements() == inputASize && inputASize == inputBSize)
          checkDeInterleave(currentMatch, currentInfo, (SourceVector) vector);
        break;
      case Alignvec:
        DEBUG(dbgs() << "Alignvec");
        // Alignvec is only possible if we are shuffling two 512-bit vectors into a 512-bit vector result
        if (outputType.getSizeInBits() == 512u && (int) outputType.getVectorNumElements() == (source == Left ? inputASize : inputBSize))
          checkAlignvec(currentMatch, currentInfo, (SourceVector)vector);
        break;
      case Combine:
        DEBUG(dbgs() << "Combine");
        // Combine is only possible when both inputs and the output have the same size
        if ((int) outputType.getVectorNumElements() == inputASize && inputASize == inputBSize)
          checkCombine(currentMatch, currentInfo, (SourceVector)vector);
        break;
      case Compress:
        DEBUG(dbgs() << "Compress");
        if (SHAVEOptions::UseCompress)
          checkCompress(currentMatch, currentInfo, (SourceVector)vector);
        break;
      case Permute:
        DEBUG(dbgs() << "Permute");
        if (hasPermute && outputType.getSizeInBits() == 512u)
          checkPermute(currentMatch, currentInfo, (SourceVector)vector);
        break;
      case PermuteBlend:
        DEBUG(dbgs() << "Permute with Blend");
        if (hasPermute && outputType.getSizeInBits() == 512u)
          checkPermuteBlend(currentMatch, currentInfo, (SourceVector)vector);
        break;

      default:
        // Unsupported patterns
        break;
      }

      unsigned currentMatching = std::count(currentMatch.begin(), currentMatch.end(), true);

      DEBUG(dbgs() << " matched " << currentMatching << " elements: ");
      DEBUG(for (const auto &bit : currentMatch) dbgs() << (bit ? "1" : "0"));
      DEBUG(dbgs() << "\n");

      if (currentMatching > matching) {
        source = (SourceVector) vector;
        pattern = (ShufflePattern) currentPattern;
        matching = currentMatching;
        patternMatch = currentMatch;
        info = currentInfo;
      }

      if (matching == mask.size())
        break;
    }

    if (matching == mask.size())
      break;
  }

  // While optimising BUILD_VECTOR, we only want to use shuffle lowering if enough elements can
  // be produced by a shuffle instruction
  if ((int) matching < threshold)
    return SDValue();

  // Identity shuffles with BUILD_VECTOR is typically caused by an EXTRACT_SUBVECTOR that is expanded
  // to a BUILD_VECTOR. If we allow identity shuffles here, then we create a legalisation loop
  if (threshold != -1 && pattern == Identity)
    return SDValue();

  SDValue shuffledVector, copySource;

  usedPattern = pattern;
  usedInfo = info;

  switch (pattern) {
  case Identity:
    shuffledVector = getSource(source);
    break;
  case Splat:
    shuffledVector = lowerSplat(patternMatch, source);
    break;
  case Rotate:
    shuffledVector = lowerRotate(patternMatch, source);
    break;
  case Interleave:
    shuffledVector = lowerInterleave(patternMatch, source);
    break;
  case DeInterleave:
    shuffledVector = lowerDeInterleave(patternMatch, source);
    break;
  case Alignvec:
    shuffledVector = lowerAlignvec(patternMatch, source);
    break;
  case Combine:
    shuffledVector = lowerCombine(patternMatch, source);
    break;
  case Compress:
    shuffledVector = lowerCompress(patternMatch, source);
    break;
  case Permute:
    shuffledVector = lowerPermute(patternMatch, source);
    break;
  case PermuteBlend:
    shuffledVector = lowerPermute(patternMatch, source, true);
    break;
  default:
    llvm_unreachable("Unsupported shuffle pattern found during lowering");
  }

  shuffledVector = insertCopies(shuffledVector, patternMatch);

  return shuffledVector;
}

SDValue SHAVELowering::SHAVEv3LowerVECTOR_SHUFFLE(SDValue op, SelectionDAG &DAG) const {
  SHAVEv3ShuffleLowering shuffle(SHAVEST, cast<ShuffleVectorSDNode>(op.getNode()), DAG);
  return shuffle.lower();
}

SDValue SHAVELowering::SHAVEv3OptimizeBUILD_VECTOR(SDValue op, SelectionDAG &DAG) const {
  unsigned numberOfElements = op.getNumOperands();
  SDLoc dbgLoc(op);
  
  // Detect shuffles that got split into EXTRACT_VECTOR_ELTs -> BUILD_VECTOR
  // ISD::VECTOR_SHUFFLE requires that the source vectors have the same type
  // as the shuffle itself. Any shuffles that don't meet this requirement are
  // split in this way
  std::map<unsigned int, unsigned int> sourceCounts;
  std::map<unsigned int, SDNode *> sourceNodes;
  for (unsigned i = 0; i < numberOfElements; ++i) {
    const SDValue &operand = op.getOperand(i);
    if (operand.getOpcode() == ISD::EXTRACT_VECTOR_ELT && isa<ConstantSDNode>(operand.getOperand(1).getNode())) {
      SDNode * source = operand.getOperand(0).getNode();
      sourceCounts[source->getNodeId()]++;
      sourceNodes[source->getNodeId()] = source;
    }
  }

  if (sourceCounts.empty())
    return op;

  unsigned int sourceACount = sourceCounts.begin()->second;
  SDNode * sourceA = sourceNodes[sourceCounts.begin()->first];
  unsigned int sourceBCount = 0;
  SDNode * sourceB = sourceA;

  for (auto it = std::next(sourceCounts.begin()); it != sourceCounts.end(); ++it) {
    if (it->second > sourceBCount) {
      sourceB = sourceNodes[it->first];
      sourceBCount = it->second;
    }
    else if (it->second > sourceACount) {
      sourceA = sourceNodes[it->first];
      sourceACount = it->second;
    }
  }

  EVT outputType = op.getValueType();
  EVT shuffleType = outputType;
  if (sourceA->getValueSizeInBits(0) > shuffleType.getSizeInBits())
    shuffleType = sourceA->getValueType(0);
  if (sourceB->getValueSizeInBits(0) > shuffleType.getSizeInBits())
    shuffleType = sourceB->getValueType(0);

  const unsigned int maskSize = shuffleType.getVectorNumElements();
  int * mask = new int[maskSize];
  memset(mask, 0xff, maskSize*sizeof(int)); // initialise to -1 to disable all elements by default

  for (unsigned i = 0; i < numberOfElements; ++i) {
    const SDValue &operand = op.getOperand(i);
    if (operand.getOpcode() == ISD::EXTRACT_VECTOR_ELT && isa<ConstantSDNode>(operand.getOperand(1).getNode())) {
      unsigned int adjustment = 0;
      if (operand.getOperand(0).getNode() == sourceA)
        adjustment = 0;
      else if (operand.getOperand(0).getNode() == sourceB)
        adjustment = sourceA->getValueType(0).getVectorNumElements();
      else
        continue; // Skip any extracts not from A or B

      mask[i] = cast<ConstantSDNode>(operand.getOperand(1).getNode())->getZExtValue() + adjustment;
    }
  }

  SDValue newVector;

  int first = mask[0];
  bool isSubvectorExtract = shuffleType.getSizeInBits() > outputType.getSizeInBits();
  for (int i = 0; i < (int) numberOfElements; ++i)
    isSubvectorExtract &= mask[i] == (first + i);

  if (isSubvectorExtract) {
    int firstElements = (int) sourceA->getValueType(0).getVectorNumElements();
    SDNode * source = first < firstElements ? sourceA : sourceB;
    int index = source == sourceA ? first : first - firstElements;

    newVector = DAG.getNode(SHAVEISD::EXTRACT_SUBVECTOR, dbgLoc, outputType, SDValue(source, 0), DAG.getConstant(index, dbgLoc, MVT::i32));
  }
  else {
    // Only use shuffle lowering if 2 or more elements in the new vector can be generated using a shuffle
    const int shuffleThreshold = 2;

    SHAVEv3ShuffleLowering shuffle(SHAVEST, SDValue(sourceA, 0), SDValue(sourceB, 0), ArrayRef<int>(mask, maskSize), shuffleType, dbgLoc, DAG, shuffleThreshold);
    newVector = shuffle.lower();

    // Shuffle lowering didn't produce anything
    if (newVector.getNode() == nullptr) {
      delete[] mask;
      return op;
    }

    if (shuffleType != outputType)
      newVector = DAG.getNode(ISD::EXTRACT_SUBVECTOR, dbgLoc, outputType, newVector, DAG.getConstant(0, dbgLoc, MVT::i32));

    // For CMU.COMPRESS, this is the element index after which all elements will be implicitly zeroed by the instruction
    unsigned int lastMatch = 0;
    if (shuffle.isCompress())
      lastMatch = outputType.getVectorNumElements() - llvm::popcount(shuffle.getCompressMask());

    // Insert any elements not captured by the shuffle
    for (unsigned int i = 0; i < numberOfElements; ++i) {
      if (mask[i] == -1) {
        // No need to insert zero value into element that is implicitly set to zero by CMU.COMPRESS
        if (shuffle.isCompress() && i > lastMatch &&
            op.getOperand(i).getOpcode() == ISD::Constant &&
            cast<ConstantSDNode>(op.getOperand(i))->getSExtValue() == 0)
          continue;

        newVector = DAG.getNode(ISD::INSERT_VECTOR_ELT, dbgLoc, outputType, newVector, op.getOperand(i), DAG.getConstant(i, dbgLoc, MVT::i32));
      }
    }
  }

  delete[] mask;
  return newVector;
}

SDValue SHAVELowering::SHAVEv3LowerBUILD_VECTOR(SDValue op, SelectionDAG &DAG) const {
  // Try to optimize the operation before emitting a constant pool load.
  SDValue OptVector = OptimizeBUILD_VECTOR(op.getNode(), DAG, false);

  if (OptVector.getNode())
    return OptVector;

  // Try to use constant pool
  SDValue constantPool = TryConstantPool(op, DAG);
  if (constantPool.getNode())
    return constantPool;
  
  // Test for vector with all the same elements and use SPLAT if possible
  SDLoc dbgLoc(op);
  BuildVectorSDNode *buildVectorNode = cast<BuildVectorSDNode>(op.getNode());
  unsigned numberOfElements = buildVectorNode->getNumOperands();
  bool isSplat = true;
  SDValue firstOperand = buildVectorNode->getOperand(0);

  for (unsigned i = 1; i < numberOfElements; ++i) {
    if (firstOperand != buildVectorNode->getOperand(i)) {
      isSplat = false;
      break;
    }
  }

  if (isSplat) {
    if (firstOperand->getOpcode() == ISD::EXTRACT_VECTOR_ELT)
      return DAG.getNode(SHAVEISD::LANESPLAT, dbgLoc, buildVectorNode->getValueType(0), firstOperand.getOperand(0), firstOperand.getOperand(1));
    else
      return DAG.getNode(SHAVEISD::SPLAT, dbgLoc, buildVectorNode->getValueType(0), firstOperand);
  }

  SDValue shuffledVector = SHAVEv3OptimizeBUILD_VECTOR(op, DAG);
  if (shuffledVector.getNode())
    return shuffledVector;

  return op;
}

SDValue SHAVELowering::SHAVEv3LowerSimpleCONCAT_VECTORS(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  EVT type = op.getValueType();
  EVT subtype = op.getOperand(0).getValueType();
  auto result = DAG.getUNDEF(type);

  for (unsigned int i = 0; i < op.getNumOperands(); ++i) {
    result = DAG.getNode(ISD::INSERT_SUBVECTOR, dbgLoc, type, result, op.getOperand(i), DAG.getConstant(i*subtype.getVectorNumElements(), dbgLoc, MVT::i32));
  }

  return result;
}

SDValue SHAVELowering::SHAVEv3LowerCONCAT_VECTORS(SDValue op, SelectionDAG &DAG) const {
  SmallVector<ShuffleVectorSDNode *, 4> shuffles;
  for (unsigned int i = 0; i < op.getNumOperands(); ++i)
    if (isa<ShuffleVectorSDNode>(op.getOperand(i).getNode()))
      shuffles.push_back(cast<ShuffleVectorSDNode>(op.getOperand(i).getNode()));

  // All operands must be produced by shuffle operations for this optimisation
  if (shuffles.size() != op.getNumOperands())
    return SHAVEv3LowerSimpleCONCAT_VECTORS(op, DAG);

  SDValue sourceA = shuffles.front()->getOperand(0);
  SDValue sourceB = shuffles.front()->getOperand(1);
  bool matchingAB = true;
  // All shuffles must have the same pair of input vectors or the second input on each must be undef
  for (const auto &shuffle : shuffles)
    if (shuffle->getOperand(0) != sourceA || shuffle->getOperand(1) != sourceB)
      matchingAB = false;

  if (!matchingAB) {
    // We can optimise the concat of two shuffles if both shuffles have a single vector input (i.e. second input is undef)
    if (shuffles.size() != 2u)
      return SDValue();

    if (!shuffles.back()->getOperand(1).isUndef())
      return SDValue();

    if (!sourceB.isUndef()) {
      // We can allow a case like this:
      //   t3: vector_shuffle<...> t1, t2
      //   t4: vector_shuffle<...> t2, undef
      // t5 : concat_vectors t3, t4
      if (shuffles.back()->getOperand(0) != sourceB)
        return SDValue();
    }
    else {
      sourceB = shuffles.back()->getOperand(0);
    }
  }

  const EVT outputType = op.getValueType();
  const unsigned int maskSize = outputType.getVectorNumElements();
  int * mask = new int[maskSize];
  const unsigned int aLimit = outputType.getVectorNumElements() / 2u; // limit for A source when A/B don't match

  // Mask for lowering is the concatenation of each individual mask
  unsigned int idx = 0;
  for (const auto &shuffle : shuffles) {
    for (const auto &element : shuffle->getMask()) {
      if (matchingAB) {
        mask[idx++] = element;
      }
      else {
        auto maskValue = element + (idx < aLimit ? 0 : aLimit);
        mask[idx++] = maskValue;
      }
    }
  }

  SDLoc dbgLoc(op);
  // Only use shuffle lowering if 2 or more elements in the new vector can be generated using a shuffle
  const int shuffleThreshold = 2;

  SHAVEv3ShuffleLowering shuffle(SHAVEST, sourceA, sourceB, ArrayRef<int>(mask, maskSize), outputType, dbgLoc, DAG, shuffleThreshold);
  SDValue newVector = shuffle.lower();

  // Shuffle lowering didn't produce anything
  if (newVector.getNode() == nullptr)
    return SDValue();

  return newVector;
}

namespace {
  SDValue createLoweredMaskedStore(MaskedStoreSDNode *N, SDValue mask,
                                  unsigned int ccRegister,
                                  SHAVEISD::NodeType nodeType,
                                  SelectionDAG &DAG) {
    SDLoc dbgLoc(N);
    SDValue compare = DAG.getNode(SHAVEISD::CMU_CM, dbgLoc, MVT::Glue, mask,
                                  DAG.getConstant(0, dbgLoc, mask.getValueType()),
                                  DAG.getCondCode(ISD::SETNE));

    SmallVector<SDValue, 6> operands{
      N->getChain(),
      N->getValue(),
      N->getBasePtr(),
      DAG.getTargetConstant(SHAVECC::NEQ, dbgLoc, MVT::i8),
      DAG.getRegister(ccRegister, MVT::i8),
      compare};

    return DAG.getNode(nodeType, dbgLoc, N->getValueType(0),
                       operands);
  }
}

SDValue SHAVELowering::SHAVEv3LowerMSTORE(SDValue op, SelectionDAG &DAG) const {
  MaskedStoreSDNode *N = cast<MaskedStoreSDNode>(op.getNode());
  SDLoc dbgLoc(op);
  SDValue mask = N->getMask();       // op.getOperand(3);
  SDValue value = N->getValue();     // op.getOperand(1);

  unsigned int ccRegister = SHAVE::NoRegister;
  switch (value.getValueType().getScalarSizeInBits()) {
  case 32u:
    ccRegister = SHAVE::C_CMU_0_3;
    break;
  case 16u:
    ccRegister = SHAVE::C_CMU0;
    break;
  case 8u:
    ccRegister = SHAVE::C_CMU_0_15;
    break;
  default:
    llvm_unreachable("Unsupported masked store vector element type");
  }

  unsigned valueSizeinBits = value.getValueType().getSizeInBits();
  assert((valueSizeinBits == 512 || valueSizeinBits == 256 ||
         (valueSizeinBits == 32 && value.getValueType().getScalarSizeInBits() == 8)) &&
         "Unsupported masked store vector size");

  SHAVEISD::NodeType storeType = (valueSizeinBits == 32)
                                     ? SHAVEISD::MASKED_STORE
                                     : SHAVEISD::MASKED_STORE_L;

  SDValue maskedStore = createLoweredMaskedStore(N, mask, ccRegister,
                                                 storeType, DAG);

  if (valueSizeinBits == 512) {
    // need to split the store into high and low and adjust the mask accordingly
    SDValue mask_h =
        DAG.getNode(SHAVEISD::ROTATE_VECTOR, dbgLoc, mask.getValueType(), mask,
                    DAG.getConstant(32, dbgLoc, MVT::i8));
    SDValue outChains[2] = {maskedStore,
                            createLoweredMaskedStore(N, mask_h, ccRegister,
                                                     SHAVEISD::MASKED_STORE_H, DAG)};
    return DAG.getNode(ISD::TokenFactor, dbgLoc, MVT::Other, outChains);
  }

  return maskedStore;
}

SDValue SHAVELowering::SHAVEv3LowerEXTRACT_SUBVECTOR(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  const SDValue &source = op.getOperand(0);
  const SDValue &index = op.getOperand(1);

  if (isa<ConstantSDNode>(index.getNode()))
    return DAG.getNode(SHAVEISD::EXTRACT_SUBVECTOR, dbgLoc, op.getValueType(), source, index);
  else
    return SDValue();
}

SDValue SHAVELowering::SHAVEv3LowerFCOPYSIGN(SDValue op, SelectionDAG &DAG) const {
  SDLoc dbgLoc(op);
  SDValue magnitude = op.getOperand(0);
  SDValue sign = op.getOperand(1);

  const auto magnitudeType = magnitude.getValueType();
  const auto signType = sign.getValueType();

  // Don't bother with mismatched f32/f16 for vectors. The DAG combiner won't produce these like it does for scalars
  // without using the option "-combiner-vector-fcopysign-extend-round". fp_trunc for SHAVE is 0-cycles so there is
  // no additional overhead in taking this approach
  if (magnitudeType != signType)
    return SDValue();

  // f32 and f16 only
  const auto scalarSize = magnitudeType.getScalarSizeInBits();
  if (scalarSize != 32u && scalarSize != 16u)
    return SDValue();

  const auto maskType = MVT::getVectorVT(MVT::getIntegerVT(scalarSize), magnitudeType.getVectorNumElements());
  const uint64_t signMask = 1ull << (scalarSize - 1ull);
  const uint64_t magnitudeMask = signMask - 1u;

  // Zero the sign-bit on the magnitude operand
  SDValue extractMagnitude = DAG.getNode(ISD::AND, dbgLoc, maskType,
                                         DAG.getBitcast(maskType, magnitude),
                                         DAG.getConstant(magnitudeMask, dbgLoc, maskType));

  // Isolate the sign-bit on the sign operand
  SDValue extractSign = DAG.getNode(ISD::AND, dbgLoc, maskType,
                                    DAG.getBitcast(maskType, sign),
                                    DAG.getConstant(signMask, dbgLoc, maskType));

  // Combine the two
  SDValue result = DAG.getNode(ISD::OR, dbgLoc, maskType, extractMagnitude, extractSign);

  return DAG.getBitcast(magnitudeType, result);
}

SDValue SHAVELowering::SHAVEv3LowerMathOperation(SDValue op, SelectionDAG &DAG) const {
  // If denormals are to be preserved, then we cannot use this lowering as:
  //  1. When lowering straight to instructions, all input and output denormals are flushed to zero
  //  2. When lowering to vector math functions, there is a conversion from FP32 to FP16, which on SHAVE will flush denormal FP16 values to zero
  if (!DAG.getDenormalMode(MVT::f16).outputsAreZero())
    return SDValue();

  // The SHAVE instructions for these opcodes have a ULP of 1 so we can only select them if
  // the "afn" flag has been set on the operation
  if (op->getFlags().hasApproximateFuncs())
    return op;

  MVT outputType = op.getValueType().getSimpleVT();

  // This lowering is for vector types only
  if (!outputType.isVector())
    return SDValue();

  // Element type must be 16-bit floating-point, and the vector at most 512-bits in size
  if (outputType.getScalarType() != MVT::f16 || outputType.getSizeInBits() > 512u)
    return SDValue();

  // Otherwise we promote to a call of the equivalent 32-bit floating-point vector library function
  SDLoc dbgLoc(op);

  const char *symbolName = nullptr;
  switch (op.getOpcode()) {
  case ISD::FLOG: symbolName = "vlogf_v16f32"; break;
  case ISD::FEXP: symbolName = "vexpf_v16f32"; break;
  default:
    llvm_unreachable("Unsupported opcode in SHAVE512 math operation lowering");
  }

  SDValue callee = DAG.getExternalSymbol(symbolName, getPointerTy(DAG.getDataLayout()));
  SDValue chain = DAG.getEntryNode();
  Type *type = FixedVectorType::get(Type::getFloatTy(*DAG.getContext()), 16u); // Equivalent of MVT::v16f32
  SDValue input = op.getOperand(0);

  auto getArgs = [&](SDValue operand) {
    ArgListTy arguments;
    ArgListEntry arg;
    arg.Node = operand;
    arg.Ty = type;
    arguments.push_back(arg);
    return arguments;
  };

  auto lowerCall = [&](SDValue input) {
    // Convert from MVT::v16f16 to MVT::v16f32
    SDValue inputAsFP32 = DAG.getFPExtendOrRound(input, dbgLoc, MVT::v16f32);

    TargetLowering::CallLoweringInfo CLI(DAG);
    CLI.setDebugLoc(dbgLoc)
       .setChain(chain)
       .setCallee(CallingConv::C, type, callee, std::move(getArgs(inputAsFP32)));

    // Call the library function
    std::pair<SDValue, SDValue> callInfo = LowerCallTo(CLI);

    // Convert back from MVT::v16f32 to MVT::v16f16
    return DAG.getFPExtendOrRound(callInfo.first, dbgLoc, MVT::v16f16);;
  };

  if (outputType == MVT::v32f16) {
    // Split into two calls and concat the results
    // Extract the low half of the input and call the vector math library function
    SDValue lowOperand = DAG.getNode(ISD::EXTRACT_SUBVECTOR, dbgLoc, MVT::v16f16,
                                     input,
                                     DAG.getConstant(0, dbgLoc, MVT::i32));

    SDValue lowResult = lowerCall(lowOperand);

    // Extract the high half of the input and call the vector math library function
    SDValue highOperand = DAG.getNode(ISD::EXTRACT_SUBVECTOR, dbgLoc, MVT::v16f16,
                                      input,
                                      DAG.getConstant(16, dbgLoc, MVT::i32));

    SDValue highResult = lowerCall(highOperand);

    return DAG.getNode(ISD::CONCAT_VECTORS, dbgLoc, MVT::v32f16, lowResult, highResult);
  }

  if (outputType != MVT::v16f16) {
    // Input is smaller than 256-bits so extend to 256-bits
    input = DAG.getNode(ISD::INSERT_SUBVECTOR, dbgLoc, MVT::v16f16,
                        DAG.getUNDEF(MVT::v16f16),
                        input,
                        DAG.getConstant(0, dbgLoc, MVT::i32));
  }

  SDValue result = lowerCall(input);

  if (outputType != MVT::v16f16) {
    // Input was smaller than 256-bits so truncate back to the expected size
    result = DAG.getNode(ISD::EXTRACT_SUBVECTOR, dbgLoc, outputType,
                         result,
                         DAG.getConstant(0, dbgLoc, MVT::i32));
  }

  return result;
}
