//===-- SHAVETargetObjectFile.h - Object File Descriptor --------*- C++ -*-===//
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
// Decribe the SHAVE object file information
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SHAVETARGETOBJECTFILE_H
#define LLVM_SHAVETARGETOBJECTFILE_H (1)


#include <map>
#include <string>
#include <utility>
#include <vector>

#include "llvm/ADT/StringMap.h"
#include "llvm/IR/Function.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

#include "SHAVE.h"


namespace llvm {
  class GlobalVariable;
  class Module;
  class SHAVESection;
  class SHAVETargetMachine;

  enum { DataBankSize = 80 };

  class SHAVETargetObjectFile : public TargetLoweringObjectFileELF {
    typedef std::map<const std::string, SHAVESection*> SectionsByName;
    typedef std::pair<const std::string, SHAVESection*> SectionsByNamePair;

    // Bindings of names to allocated sections.
    mutable SectionsByName CODESectionsByName;
    mutable SectionsByName DATASectionsByName;
    mutable SectionsByName BSSSectionsByName;

    // Lists of sections.
    mutable std::vector<SHAVESection *> DATASections_;
    mutable std::vector<SHAVESection *> CODESections_;
    mutable std::vector<SHAVESection *> BSSSections_;

    const TargetMachine *TM;

  public:
    SHAVETargetObjectFile(TargetMachine *TM);

    // Find or Create a SHAVE Section
    SHAVESection *getSHAVESection(const std::string &Name, SHAVESectionType Ty) const;

    void Initialize(MCContext &Ctx, const TargetMachine &TM) override;

    // Override section allocations for user specified sections.
    MCSection *getExplicitSectionGlobal(const GlobalObject *GO, SectionKind Kind,
                                        const TargetMachine &TM) const override;

    // Select sections for Data and Auto variables(globals).
    MCSection *SelectSectionForGlobal(const GlobalObject *GO, SectionKind Kind,
                                      const TargetMachine&) const override;
    MCSection *getStaticCtorSection(unsigned Priority = 65535u, const MCSymbol *KeySym = nullptr) const override;
    MCSection *getStaticDtorSection(unsigned Priority = 65535u, const MCSymbol *KeySym = nullptr) const override;
    MCSection *getSectionForConstant(const DataLayout &DL, SectionKind Kind, const Constant *C, Align &alignment) const override;

    // Return a code section for a function
    SHAVESection *SectionForCode(const Function &F, const TargetMachine& STM, bool isLinkOnce = false) const;

    // Jump table section handling
    MCSection *getSectionForJumpTable(const Function &F,
                                      const TargetMachine &TM) const override;
    bool shouldPutJumpTableInFunctionSection(bool UsesLabelDifference,
                                      const Function &F) const override;

    // Accessors for various section lists.
    const std::vector<SHAVESection *> &DATASections() const { return DATASections_; }
    const std::vector<SHAVESection *> &CODESections() const { return CODESections_; }
    const std::vector<SHAVESection *> &BSSSections() const { return BSSSections_; }

    bool ForceSingleDATASection;
    bool ForceSingleCODESection;

    const llvm::MCSection* getEHFrameSection() const {
      llvm_unreachable("Cannot reach this.");
      return nullptr;
    }
  };
} // end namespace llvm


#endif  // LLVM_SHAVETARGETOBJECTFILE_H
