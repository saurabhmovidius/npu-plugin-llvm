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
// Description:  Initialize some printing directives
// ---------------------------------------------------------------------------

#include "SHAVEMCAsmInfo.h"
#include "SHAVENames.h"
#include "SHAVEOptions.h"

using namespace llvm;


SHAVEMCAsmInfo::SHAVEMCAsmInfo(const Triple &TT, const MCTargetOptions &Options) {
  CommentString = SHAVE_COMMENT;
//  GlobalPrefix = SHAVE_GLOBAL_PREFIX;
  PrivateGlobalPrefix = SHAVE_PRIVATE_PREFIX;
  PrivateLabelPrefix = SHAVE_PRIVATE_PREFIX;

  GlobalDirective = SHAVE_DIRECTIVE_GLOBAL;

  WeakDirective = SHAVE_DIRECTIVE_WEAK;
  WeakRefDirective = SHAVE_DIRECTIVE_WEAK_REFERENCE;

  AvoidWeakIfComdat = true;
  HasDotTypeDotSizeDirective = true;
  NeedsLocalForSize = false;
  HasSingleParameterDotFile = true;
  HasAggressiveSymbolFolding = true;
  HasMachoZeroFillDirective = true;
  UsesELFSectionDirectiveForBSS = true;

  Data8bitsDirective = SHAVE_DIRECTIVE_BYTE;
  Data16bitsDirective = SHAVE_DIRECTIVE_SHORT;
  Data32bitsDirective = SHAVE_DIRECTIVE_WORD;
  Data64bitsDirective = SHAVE_DIRECTIVE_LONGLONG;

  ZeroDirective = SHAVE_DIRECTIVE_FILL;
  AsciiDirective = SHAVE_DIRECTIVE_ASCII;
  AscizDirective = SHAVE_DIRECTIVE_ASCIIZ;

  InlineAsmStart = SHAVE_INLINE_ASM_START;
  if (SHAVEOptions::UseNOPSync)
    InlineAsmEnd = SHAVE_INLINE_ASM_END;
  else
    InlineAsmEnd = SHAVE_INLINE_ASM_END_NO_NOPSYNC;

  // Dwarf debug support
  SupportsDebugInformation = true;
  DwarfUsesRelocationsAcrossSections = true;
//  DwarfRegNumForCFI = true;
  ExceptionsType = ExceptionHandling::DwarfCFI;

  // 'moviAsm' does not support the '.hidden' or '.protected' directives
  HiddenDeclarationVisibilityAttr = MCSA_Invalid;
  HiddenVisibilityAttr = MCSA_Invalid;
  ProtectedVisibilityAttr = MCSA_Invalid;

  UseIntegratedAssembler = false;
}
