//===-- SHAVESection.h - Section Handling -----------------------*- C++ -*-===//
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

#ifndef LLVM_SHAVESECTION_H
#define LLVM_SHAVESECTION_H (1)


#include <vector>

#include "llvm/IR/GlobalVariable.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/Support/raw_ostream.h"


namespace llvm {
  // FIXME: Movidius - we should really derive SHAVESection off MCSectionELF and not MCSection
  class SHAVESection : public MCSection {
    SHAVESectionType T;

    // Name of the section to uniquely identify it.
    StringRef Name;

    SHAVESection(StringRef name, SectionKind K, SHAVESectionType T, MCSymbol *Begin = nullptr)
      : MCSection(SV_SHAVE, name, K, Begin), T(T), Name(name)
    {}

  public:
    // Return the name of the section
    StringRef getName() const { return Name; }

    /// Check section type.
    bool isDATA_Type() const { return T == DATA; }
    bool isCODE_Type() const { return T == CODE; }
    bool isBSS_Type() const { return T == BSS; }
    bool isDEBUG_Type() const { return T == DEBUG; }

    SHAVESectionType getType() const { return T; }

    // This would be the only way to create a section.
    static SHAVESection *Create(StringRef Name, SHAVESectionType Ty,
                                MCContext &Ctx);

    // Override this as SHAVE has its own way of printing when switching sections.
    void printSwitchToSection(const MCAsmInfo &MAI, const Triple &T, raw_ostream &OS, const MCExpr *Subsection) const override;

    bool useCodeAlign() const override { return false; }
    bool isVirtualSection() const override { return false; }
  };
} // End namespace llvm


#endif  // LLVM_SHAVESECTION_H
