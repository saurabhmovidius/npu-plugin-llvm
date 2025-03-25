//===-- SHAVESection.cpp - Section Handling ---------------------*- C++ -*-===//
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
// SHAVE sections need to be handled differently to normal ELF sections
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCContext.h"

#include "MCTargetDesc/SHAVEOptions.h"

#include "SHAVE.h"
#include "SHAVENames.h"
#include "SHAVESection.h"

using namespace llvm;


// Sections created here do not need to be explicitly deleted as they are managed by auto_ptrs.
SHAVESection* SHAVESection::Create(StringRef SectionName, SHAVESectionType Ty, MCContext & Ctx) {
  /// Determine the internal SectionKind info.
  /// Users of SHAVESection class should not need to know the internal
  /// SectionKind. They should work only with SHAVESectionType.
  SectionKind K;

  switch (Ty) {
    case DATA:
      K = SectionKind::getReadOnly();
      break;

    case CODE:
      K = SectionKind::getText();
      break;

    case BSS:
      K = SectionKind::getBSS();
      break;

   case DEBUG:
      K = SectionKind::getMetadata();
      break;

#ifndef NDEBUG
    default: llvm_unreachable ("Cannot create unknown section type");
#endif // NDEBUG
  }

  // Copy strings into context allocated memory so they get free'd when the
  // context is destroyed.
  char *NameCopy = static_cast<char*>(Ctx.allocate(SectionName.size()+1, 1));
  if (NameCopy == nullptr || SectionName.data() == nullptr)
	  llvm_unreachable("SectionName is null or not a string");
  memcpy(NameCopy, SectionName.data(), SectionName.size()+1);

  // Create the Section.
  SHAVESection *S = new (Ctx)SHAVESection(StringRef(NameCopy, SectionName.size()), K, Ty, Ctx.createTempSymbol(NameCopy, false));

  return S;
}

// A generic way to print all types of sections
void SHAVESection::printSwitchToSection(const MCAsmInfo &MAI, const Triple &,
                                        raw_ostream &OS,
                                        const MCExpr * /* Subsection */) const {
  // Determine the section type.
  switch (getType()) {
    case DATA:
      OS << SHAVE_DIRECTIVE_DATA_SECTION;
      break;
    case CODE:
      OS << SHAVE_DIRECTIVE_CODE_SECTION;
      break;
    case BSS:
#if 0
      // FIXME: Movidius - why is this not?
      OS << SHAVE_DIRECTIVE_BSS_SECTION;
#else
      OS << "// Really BSS\n" << SHAVE_DIRECTIVE_DATA_SECTION;
#endif
      break;
    case DEBUG:
      OS << SHAVE_DIRECTIVE_DEBUG_SECTION;
      break;
#ifndef NDEBUG
    default:
      llvm_unreachable("Unknown SHAVE section type");
#endif // NDEBUG
  }

  // If the name for the section is not available, default the code and data sections to '.text' and '.data'
  if (Name.empty()) {
    switch (getType()) {
    case DATA:  OS << SHAVEOptions::SectionPrefix << SHAVE_SECTION_NAME_DATA; break;
    case CODE:  OS << SHAVEOptions::SectionPrefix << SHAVE_SECTION_NAME_CODE; break;
    default:  break;
    }
  } else
    OS << Name;

  OS << "\n";

#if 0
  // FIXME: Movidius - I don't understand why we would emit '.data name' followed by '.bss'.  BSS handling is badly broken. Disable this for now
  if(getType() == BSS)
    OS << SHAVE_DIRECTIVE_BSS_SECTION << "\n";
#endif  // 0
}
