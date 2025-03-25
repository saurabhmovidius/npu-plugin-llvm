//===-- SHAVETargetObjectFile.cpp - Object File Descriptor ------*- C++ -*-===//
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

#define DEBUG_TYPE "shave-target-object-file"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/SectionKind.h"
#include "llvm/Support/FormattedStream.h"

#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVE.h"
#include "SHAVENames.h"
#include "SHAVESection.h"
#include "SHAVETargetMachine.h"
#include "SHAVETargetObjectFile.h"

using namespace llvm;
using namespace SHAVEOptions;


SHAVETargetObjectFile::SHAVETargetObjectFile(TargetMachine *tm)
  : TM(tm) {
  ForceSingleDATASection = !TM->getDataSections();
  ForceSingleCODESection = false;
}


// Find a section. If not found, create one and keep
// track of it by adding it to appropriate section list.
SHAVESection *SHAVETargetObjectFile::getSHAVESection(const std::string &Name, SHAVESectionType Ty) const {
  SectionsByName::iterator iter;
  SHAVESection *Entry = nullptr;

  // Return if we have an already existing one.
  if (Ty == DATA) {
    iter = DATASectionsByName.find(Name);
    if (iter != DATASectionsByName.end())
      return iter->second;
  } else if (Ty == CODE) {
    iter = CODESectionsByName.find(Name);
    if (iter != CODESectionsByName.end())
      return iter->second;
  } else if (Ty == BSS) {
    iter = BSSSectionsByName.find(Name);
    if (iter != BSSSectionsByName.end())
      return iter->second;
  } else
    llvm_unreachable("unknown standard section type.");

  // Else create a new one and add it to appropriate section list.
  Entry = SHAVESection::Create(Name, Ty, getContext());

  if (Ty == DATA) {
    DATASectionsByName.insert(SectionsByNamePair(Name, Entry));
    DATASections_.push_back(Entry);
  } else if (Ty == CODE) {
    CODESectionsByName.insert(SectionsByNamePair(Name, Entry));
    CODESections_.push_back(Entry);
  } else if (Ty == BSS) {
    BSSSectionsByName.insert(SectionsByNamePair(Name, Entry));
    BSSSections_.push_back(Entry);
  } else
    llvm_unreachable("unknown standard section type.");

  return Entry;
}

// Do some standard initialization.
void SHAVETargetObjectFile::Initialize(MCContext &Ctx, const TargetMachine &tm) {
  // Modified by Alberto Taiuti, Codeplay Software Ltd
  // 08/05/2018
  CODESectionsByName.clear();
  DATASectionsByName.clear();
  BSSSectionsByName.clear();
  /////////////////////////////////

  TargetLoweringObjectFile::Initialize(Ctx, tm);
  TM = &tm;

  // Define the Dwarf meta-data section named
  DwarfAbbrevSection      = SHAVESection::Create(".debug_abbrev",      DEBUG, getContext());
  DwarfInfoSection        = SHAVESection::Create(".debug_info",        DEBUG, getContext());
  DwarfLineSection        = SHAVESection::Create(".debug_line",        DEBUG, getContext());
  DwarfFrameSection       = SHAVESection::Create(".debug_frame",       DEBUG, getContext());
  DwarfPubTypesSection    = SHAVESection::Create(".debug_pubtypes",    DEBUG, getContext());
  DwarfDebugInlineSection = SHAVESection::Create(".debug_debuginline", DEBUG, getContext());
  DwarfStrSection         = SHAVESection::Create(".debug_str",         DEBUG, getContext());
  DwarfLocSection         = SHAVESection::Create(".debug_loc",         DEBUG, getContext());
  DwarfARangesSection     = SHAVESection::Create(".debug_aranges",     DEBUG, getContext());
  DwarfRangesSection      = SHAVESection::Create(".debug_ranges",      DEBUG, getContext());
  DwarfPubNamesSection    = SHAVESection::Create(".debug_pubnames",    DEBUG, getContext());
}

// Override default implementation to put the true globals into multiple data sections if required.
MCSection *SHAVETargetObjectFile::SelectSectionForGlobal(const GlobalObject *GO,
                                                         SectionKind Kind,
                                                         const TargetMachine &TM) const {
  SHAVESection *SC = nullptr;
  SHAVESectionType sectionType = DATA;
  std::string sectionName;

  // Determine whether this needs to be in a Link-Once section
  GlobalValue::LinkageTypes linkageType = GO->getLinkage();
  bool isLinkOnce = GlobalValue::isLinkOnceLinkage(linkageType) || GlobalValue::isWeakODRLinkage(linkageType);

  // We select the section based on the initializer here, so it really has to be a GlobalVariable
  if (const GlobalVariable *GV = dyn_cast<GlobalVariable>(GO)) {
    const StringRef gvName = GV->getName();
    bool isLLVMSpecial = StringSwitch<bool>(gvName)
                          .Case("llvm.used", true)
                          .Case("llvm.metadata", true)
                          .Case("llvm.global_ctors", true)
                          .Case("llvm.global_dtors", true)
                          .Default(false);

    // FIXME: Movidius - Handle BSS sections
    // FIXME: Movidius - Shouldn't this be looking for a provided section name?
    if (isLLVMSpecial) {
      if (gvName == "llvm.global_ctors")
        return getStaticCtorSection();
      else if (gvName == "llvm.global_dtors")
        return getStaticDtorSection();
      else
        llvm_unreachable("Unhandled LLVM special name");
    } else {
      if (isLinkOnce)
        sectionName = SHAVE_LINKONCE_PREFIX;

      // Add the canonical prefix for standard sections
      sectionName += SHAVEOptions::SectionPrefix;

      // Choose the data section name based on the alignment
      switch (GO->getAlignment()) {
      case 1:
        sectionName += Kind.isReadOnly() ? SHAVE_SECTION_NAME_RODATA1 : SHAVE_SECTION_NAME_DATA1;
        break;
      case 2:
        sectionName += Kind.isReadOnly() ? SHAVE_SECTION_NAME_RODATA2 : SHAVE_SECTION_NAME_DATA2;
        break;
      case 4:
        sectionName += Kind.isReadOnly() ? SHAVE_SECTION_NAME_RODATA4 : SHAVE_SECTION_NAME_DATA4;
        break;
      case 8:
        sectionName += Kind.isReadOnly() ? SHAVE_SECTION_NAME_RODATA8 : SHAVE_SECTION_NAME_DATA8;
        break;
      case 16:
        sectionName += Kind.isReadOnly() ? SHAVE_SECTION_NAME_RODATA16 : SHAVE_SECTION_NAME_DATA16;
        break;
      default:
        sectionName += Kind.isReadOnly() ? SHAVE_SECTION_NAME_RODATA : SHAVE_SECTION_NAME_DATA;
        break;
      }

      // Use the variable's name to construct the unique section name if data
      // objects need to be emitted into their own section (-fdata-sections).
      StringRef varName = GO->getName();
      if (TM.getDataSections() && varName.size()) {
        // Trim SOH character from variable name.
        if (varName[0] == '\1')
          varName = varName.substr(1);

        sectionName += '.' + varName.str();
      }
    }

    // Create the section
    if (Kind.isText())
      sectionType = CODE;
    else if (Kind.isMetadata())
      sectionType = DEBUG;
#if 0
    else if (Kind.isBSS())
      sectionType = BSS;
#endif

    SC = getSHAVESection(sectionName, sectionType);

    return SC;
  }
  else {
    // The "jump-tables" should reside in the same section as the function, but 'moviAsm' does not
    // support using data directives such as '.word' in a code section.  So the jump-table has to
    // be placed in a data section - in this case '.rodata' is the most suitable.
    //
    // However, there is a problem when interacting with 'linkonce' because the jump-table has an
    // internally linked name, and will vary from TU (translation-unit) to TU.  To prevent link
    // errors resulting from this when coupled with '--gc-sections', the solution is to keep the
    // jump-tables in non-discardable '.rodata' sections.
    //
    // The only "data" I have observed at this stage that has the "isText" attribute are the jump-
    // tables, so do not perform the '.gnu.linkonce' prefixing in this case so that the jump-table
    // is placed in normal '.rodata'.
    //
    // FIXME: Movidius -
    // LLVM/CLang might have a way of configuring the jump-table implementation so that they can
    // be separated into a data section normally, and be safe for ODR and linkonce, but I have not
    // yet found out how to do this.
    if (isLinkOnce && !Kind.isText())
      sectionName = SHAVE_LINKONCE_PREFIX;

    // Add the canonical prefix for standard sections
    sectionName += SHAVEOptions::SectionPrefix;

    if (Kind.isText()) {
      sectionType = CODE;
      sectionName += SHAVE_SECTION_NAME_CODE;
#if 0
    } else if (Kind.isBSS()) {
      // FIXME: Movidius - handle BSS
      sectionType = BSS;
      sectionName += SHAVE_SECTION_NAME_BSS;
#endif
    } else if (Kind.isReadOnly())
      // FIXME: Movidius - make this alignment aware
      sectionName += SHAVE_SECTION_NAME_RODATA;
    else
      // FIXME: Movidius - make this alignment aware
      sectionName += SHAVE_SECTION_NAME_DATA;
  }

  SC = getSHAVESection(sectionName, sectionType);

  return SC;
}

MCSection *SHAVETargetObjectFile::getStaticCtorSection(unsigned Priority,
                                                       const MCSymbol * /* KeySym */) const {
  // Add the canonical prefix for standard sections
  std::string ctorsSectionName(SHAVEOptions::SectionPrefix);

  if (Priority == 65535u)
    ctorsSectionName += ".ctors";
  else
    ctorsSectionName += std::string(".ctors.") + utostr(65535u - Priority);

  return getSHAVESection(ctorsSectionName, DATA);
}

MCSection *SHAVETargetObjectFile::getStaticDtorSection(unsigned Priority,
                                                       const MCSymbol * /* KeySym */) const {
  // Add the canonical prefix for standard sections
  std::string dtorsSectionName(SHAVEOptions::SectionPrefix);

  if (Priority == 65535u)
    dtorsSectionName += ".dtors";
  else
    dtorsSectionName += std::string(".dtors.") + utostr(65535u - Priority);

  return getSHAVESection(dtorsSectionName, DATA);
}

MCSection *SHAVETargetObjectFile::getSectionForConstant(const DataLayout &DL, SectionKind Kind,
                                                        const Constant *C, Align &alignment) const {
  if (Kind.isReadOnly())
    // FIXME: Movidius - TODO: Figure out how to get the alignment for these
    return getSHAVESection(SHAVEOptions::SectionPrefix + SHAVE_SECTION_NAME_RODATA, DATA);
  else
    return TargetLoweringObjectFileELF::getSectionForConstant(DL, Kind, C, alignment);
}

// Allow the target to completely override section assignment of a global.
MCSection *SHAVETargetObjectFile::getExplicitSectionGlobal(const GlobalObject *GO,
                                                           SectionKind Kind,
                                                           const TargetMachine &TM ) const {
  assert(GO->hasSection());

  // Select the section based on the initializer here, so it really has to be a GlobalVariable
  const GlobalVariable *GV = dyn_cast<GlobalVariable>(GO);

  if (!GV)
    return SelectSectionForGlobal(GO, Kind, TM);

  // Get the section name
  std::string sectionName = GV->getSection().str();

  // Default the section name if none specified
  // FIXME: Movidius - I don't think that this should ever be empty
  DEBUG(assert(!sectionName.empty()));

  // FIXME: Movidius - if 'empty' is valid, then this should also handle '-fdata-sections', linkonce linkage and alignment greater than 16
  if (sectionName.empty()) {
    if (Kind.isBSS())
      sectionName = SHAVEOptions::SectionPrefix + SHAVE_SECTION_NAME_BSS;
    else
      sectionName = SHAVEOptions::SectionPrefix + SHAVE_SECTION_NAME_DATA;
  }

  SHAVESection *SC = getSHAVESection(sectionName, Kind.isBSS() ? BSS : DATA);

  return SC;
}

// Interface used by AsmPrinter to get a code section for a function
SHAVESection *SHAVETargetObjectFile::SectionForCode(const Function &F,
                                                    const TargetMachine &STM,
                                                    bool isLinkOnce) const {
  std::string sectionName;
  bool functionSections = STM.getFunctionSections();

  if (F.hasSection()) {
    // If a section name was provided using '__attribute__((section...))' then use it
    sectionName = F.getSection().str();
  } else {
    // Need to prefix the section with ".gnu.linkonce" if the section has "linkonce" set
    if (isLinkOnce)
      sectionName = SHAVE_LINKONCE_PREFIX;

    // Add the canonical prefix for standard sections
    sectionName += SHAVEOptions::SectionPrefix;

    // All code sections start the same way
    sectionName += SHAVE_SECTION_NAME_CODE;

    // If function sections have been selected, then append the function name
    if ((functionSections && F.getName().size()) || isLinkOnce) {
      assert(!isLinkOnce || F.getName().size());
      // All unique code sections use the function-name to form a suffix for the section name
      sectionName += ".";
      sectionName += F.getName().str();
    }
  }

  return getSHAVESection(sectionName, CODE);
}

// Jump table section handling
MCSection *SHAVETargetObjectFile::getSectionForJumpTable(const Function &F,
                                                         const TargetMachine &TM) const {
  std::string sectionName;
  bool functionSections = TM.getFunctionSections();

  // FIXME: Movidius - this does not (cannot) deal with linkonce functions so long as a code
  //        sections cannot contain data.  This is a 'moviAsm' limitation
  sectionName = SHAVEOptions::SectionPrefix + SHAVE_SECTION_NAME_RODATA4;
  sectionName += ".__jt__";

  // If a section name was provided using '__attribute__((section...))' then append it
  if (F.hasSection()) {
    sectionName += ".";
    sectionName += F.getSection();
  } else if (functionSections) {
    sectionName += ".";
    sectionName += F.getName().str();
  }

  return getSHAVESection(sectionName, DATA);
}

bool SHAVETargetObjectFile::shouldPutJumpTableInFunctionSection(bool UsesLabelDifference,
                                                                const Function &F) const {
  return false;
}
