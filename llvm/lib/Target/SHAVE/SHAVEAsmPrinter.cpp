//===-- SHAVEAsmPrinter.cpp - Assembly Code Emitter -------------*- C++ -*-===//
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
// Emits the 'moviAsm' compatible SHAVE assembly code
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE  "shave-asm-printer"

#include <cstdio>
#include <cstring>

#include "llvm/ADT/SmallString.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Timer.h"

#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVEAsmPrinter.h"
#include "SHAVENames.h"
#include "SHAVESection.h"
#include "SHAVESubtarget.h"
#include "SHAVEVersionInfo.h"

using namespace llvm;


SHAVEAsmPrinter::SHAVEAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
    : AsmPrinter(TM, std::move(Streamer)),
      SHAVETM(static_cast<SHAVETargetMachine &>(TM)),
      LastMI(nullptr),
      LastFn(0),
      Counter(0),
      MCInstLowering(*this),
      PTOF(getObjFileLowering()),
      nPendingNOPs(0),
      functionHasNoInstructions(false)
      {}

void SHAVEAsmPrinter::FlushPendingNOPs() const {
  if (nPendingNOPs) {
    std::stringstream multiNOP;

    multiNOP << "NOP";

    if (nPendingNOPs > 1)
      multiNOP << " " << nPendingNOPs;

    multiNOP << "\n";

    OutStreamer->emitRawText(Twine(SHAVE_CODE_INDENT) + multiNOP.str());

    // Reset
    nPendingNOPs = 0;
  }
}

// EmitFunctionBodyEnd - Targets can override this to emit stuff after the last basic block in the function.
void SHAVEAsmPrinter::emitFunctionBodyEnd() {
  const Function &currentFunction = MF->getFunction();

  // Make sure that any remaining NOPs are flushed before starting the next function
  FlushPendingNOPs();

  // Output a message to the assembly file if verbose assembly is selected
  OutStreamer->getCommentOS() << "End of the definition for function '" << currentFunction.getName() << "'\n";
}

void SHAVEAsmPrinter::emitFunctionEntryLabel() {
  SHAVEMCTargetStreamer * ShaveTS = getTargetStreamer();
  ShaveTS->OS << SHAVE_LABEL_INDENT;
  OutStreamer->emitLabel(CurrentFnSym);
}

void SHAVEAsmPrinter::emitInstruction(const MachineInstr *MI) {
  unsigned Opc = MI->getDesc().getOpcode();
  
  if (Opc == SHAVE::NOP) {
    if (!SHAVEOptions::DisableNOPCompression) {
      // Accumulate NOPs so that they can be grouped as 'NOP #' instead of multiple NOPs
      // NOTE: The NOP compression is generally disabled with '-g' because it interfers
      // with the logical sequence of '.loc' directives
      nPendingNOPs++;
      return;
    }
    else {
      OutStreamer->emitRawText(Twine(SHAVE_CODE_INDENT) + "NOP\n");
      return;
    }
  }

  FlushPendingNOPs();

  SmallVector<MCInst, 4> BundledMCInsts;

  // Lower bundled instructions.
  for (MachineBasicBlock::const_instr_iterator MII = MachineBasicBlock::const_instr_iterator(getBundleStart(MI->getIterator())),
        MIE = getBundleEnd(MI->getIterator()); MII != MIE; ++MII)
    addToBundle(&(*MII), BundledMCInsts);

  // Emit the instruction or instruction bundle.
  MCInst TmpInst;

  if (BundledMCInsts.size() > 1) {
    // Sort instructions so that PEU instructions are first.
    const SHAVEInstrInfo *SII = SHAVETM.getSubtargetImpl()->getInstrInfo();

    std::stable_sort(BundledMCInsts.begin(), BundledMCInsts.end(),
                      [&SII](const MCInst &A, const MCInst &B) -> bool { 
                        // FIXME: Movidius - this doesn't handle LSU0/LSU1.
                        unsigned FUnitA = SII->GetFunctionalUnit(A.getOpcode());
                        unsigned FUnitB = SII->GetFunctionalUnit(B.getOpcode());
                        if (FUnitA == SHAVE::PEU)
                          return true;
                        else if (FUnitB == SHAVE::PEU)
                          return false;
                        else
                          return FUnitA > FUnitB;
                      });
    TmpInst.setOpcode(TargetOpcode::BUNDLE);

    for (unsigned i = 0; i < BundledMCInsts.size(); i++)
      TmpInst.addOperand(MCOperand::createInst(&BundledMCInsts[i]));
  } else
    TmpInst = BundledMCInsts[0];

  EmitToStreamer(*OutStreamer, TmpInst);
}

void SHAVEAsmPrinter::addToBundle(const MachineInstr *MI,
                                  SmallVectorImpl<MCInst> &Bundle) {
  MCInst BundledMCInst;

  MCInstLowering.Lower(MI, BundledMCInst);

  // Add the lowered MC instruction to the bundle
  Bundle.push_back(BundledMCInst);
}

// FIXME: Movidius - Calling the generic 'EmitFunctionHeader' is not compatible with 'moviAsm'
// So this mirrors what 'EmitFunctionHeader' does, but with appropriate differences
// FIXME: The original EmitFunctionHeader was forked in LLVM 3.6.0, this code
// is probably dated now.
void SHAVEAsmPrinter::emitFunctionHeader() {
  emitConstantPool();

  // Detect functions with no instructions (see Bugzilla #27955 for rationale)
  int nInstrsInFunction = 0;
  for (auto &MBB : *MF) {
    for (auto &MI : MBB) {
      if (!MI.isPosition() && !MI.isImplicitDef() && !MI.isKill() && !MI.isDebugValue())
        nInstrsInFunction++;
    }
  }
  functionHasNoInstructions = (nInstrsInFunction == 0);

  MCInstLowering.Initialize(&MF->getContext());

  // Space away from the previous entity
  OutStreamer->addBlankLine();
  OutStreamer->addBlankLine();

  const Function &F = MF->getFunction();

  // Output a message to the assembly file if verbose assembly is selected
  OutStreamer->getCommentOS() << "Start of the definition for function '" << F.getName() << "'\n";

  // Check for Link-Once requirements
  GlobalValue::LinkageTypes linkageType = F.getLinkage();
  bool isLinkOnce = GlobalValue::isLinkOnceLinkage(linkageType) || GlobalValue::isWeakODRLinkage(linkageType);
  bool isWeak = GlobalValue::isWeakLinkage(linkageType);

  // Check for dllexport linkage for creating IATs
  if (F.hasDLLExportStorageClass())
    exportedNames.insert(F.getName().str());

  // Now emit the instructions for the function in its code section
  MCSection *fCodeSection = getObjFileLowering().SectionForCode(F, SHAVETM, isLinkOnce);

  // Start the Code Section
  MF->setSection(fCodeSection);
  OutStreamer->switchSection(fCodeSection);

  // If this is a Link-Once function, emit the '.linkonce' directive
  if (isLinkOnce) {
    OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_LINKONCE));
    OutStreamer->addBlankLine();
  }

  // Handle the function alignment information
  unsigned int functionAlignment = F.getAlignment();

  // If no alignment was specified with an '__attribute__' use the global default
  if (0 == functionAlignment)
    functionAlignment = SHAVEOptions::AlignFunctionLabels;

  if (0 == functionAlignment)
    functionAlignment = 16;

  // If the alignment is '1', then there is no need to emit an '.align'.
  if (1 != functionAlignment)
    OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_ALIGN)
                             + Twine(functionAlignment));

  // Emit the function type information if enabled
  if (MAI->hasDotTypeDotSizeDirective()) {
    OutStreamer->emitSymbolAttribute(CurrentFnSym, MCSA_ELF_TypeFunction);

    if (isVerbose()) {
      F.printAsOperand(OutStreamer->getCommentOS(),
        /*PrintType=*/false, F.getParent());
      OutStreamer->getCommentOS() << '\n';
    }
  }

  // If this has weak linkage, emit the '.weak' directive
  if (isWeak)
    OutStreamer->emitSymbolAttribute(CurrentFnSym, MCSA_Weak);

  OutStreamer->addBlankLine();

  // Construct and emit the function name
  emitFunctionEntryLabel();

  // If the function has no instructions, force an immediate abort
  // See Bugzilla #27955 for rationale
  if (functionHasNoInstructions) {
    if (SHAVEOptions::HasLSU1) {
      OutStreamer->emitRawText(Twine(SHAVE_CODE_INDENT) + "BRU.SWIH 1 || LSU0.LDIL i18, 0xFFFF || LSU1.LDIH i18, 0xFFFF  // 'abort'\n");
    }
    else {
      OutStreamer->emitRawText(Twine(SHAVE_CODE_INDENT) + "LSU0.LDIL i18, 0xFFFF\n");
      OutStreamer->emitRawText(Twine(SHAVE_CODE_INDENT) + "BRU.SWIH 1 || LSU0.LDIH i18, 0xFFFF  // 'abort'\n");
    }
    OutStreamer->AddComment(Twine(" The function '") + CurrentFnSym->getName() + "' contains no instructions, so force an");
    OutStreamer->AddComment("immediate 'abort'.  This is most likely because the function definition involved");
    OutStreamer->AddComment("'undefined behaviour', and an immediate 'abort' makes it easier to debug than the");
    OutStreamer->AddComment("default - which is to emit an orphaned label");
    OutStreamer->addBlankLine();
  }

  MCSymbol * functionBegin = getFunctionBegin();
  if (functionBegin) {
    if (MAI->useAssignmentForEHBegin()) {
      MCSymbol *CurPos = OutContext.createTempSymbol();
      OutStreamer->emitLabel(CurPos);
      OutStreamer->emitAssignment(functionBegin,
        MCSymbolRefExpr::create(CurPos, OutContext));
    }
    else {
      OutStreamer->emitLabel(functionBegin);
    }
  }
#if 0
  // FIXME-LLVM11: This is removed now.
  
  // If the function had address-taken blocks that got deleted, then we have
  // references to the dangling symbols.  Emit them at the start of the function
  // so that we don't get references to undefined symbols.
  std::vector<MCSymbol*> DeadBlockSyms;
  MMI->takeDeletedSymbolsForFunction(&F, DeadBlockSyms);
  for (unsigned i = 0, e = DeadBlockSyms.size(); i != e; ++i) {
    OutStreamer->AddComment("Address taken block that was later removed");
    OutStreamer->emitLabel(DeadBlockSyms[i]);
  }
#endif
  // Emit per-function debug and/or EH information.
  for (const HandlerInfo &HI : Handlers) {
    NamedRegionTimer T(HI.TimerName, HI.TimerDescription, HI.TimerGroupName, HI.TimerGroupDescription, TimePassesIsEnabled);
    HI.Handler->beginFunction(MF);
  }
  for (const HandlerInfo &HI : Handlers) {
    NamedRegionTimer T(HI.TimerName, HI.TimerDescription, HI.TimerGroupName,
                       HI.TimerGroupDescription, TimePassesIsEnabled);
    HI.Handler->beginBasicBlockSection(MF->front());
  }

  // Emit the prologue data.
  if (F.hasPrologueData())
    emitGlobalConstant(getDataLayout(), F.getPrologueData());
}

void SHAVEAsmPrinter::emitStartOfAsmFile(Module& module) {
  // Output the version header information
  OutStreamer->emitRawText(StringRef("/* -------------------------------------------------------------------------------"));
  OutStreamer->emitRawText(StringRef(" *"));
  OutStreamer->emitRawText(StringRef(" * Generated by 'moviCompile v") + SHAVEVersionInfo::getLongVersionString() + "'");
  OutStreamer->emitRawText(StringRef(" *"));
  OutStreamer->emitRawText(StringRef(" * ---------------------------------------------------------------------------- */"));
  OutStreamer->addBlankLine();
  OutStreamer->emitRawText(StringRef(SHAVE_DIRECTIVE_NOWARN));
  OutStreamer->addBlankLine();
}

bool SHAVEAsmPrinter::isBlockOnlyReachableByFallthrough(const MachineBasicBlock *MBB) const {
  if (!AsmPrinter::isBlockOnlyReachableByFallthrough(MBB))
    return false;

  // Check the predecessors one more time using AnalyzeBranch.
  const TargetInstrInfo *TII = SHAVETM.getSubtargetImpl()->getInstrInfo();
  MachineBasicBlock *PredBB = *MBB->pred_begin();
  MachineBasicBlock *TBB = nullptr;
  MachineBasicBlock *FBB = nullptr;
  SmallVector<MachineOperand, 2> Cond;

  if (TII->analyzeBranch(*PredBB, TBB, FBB, Cond, false)) {
    // Cannot analyse branch, conservatively return 'no fallthrough'.
    return false;
  }

  // Look for branches in the predecessor block.
  if (!Cond.size())
    return !TBB;  // No branch or unconditional branch.
  else if (TBB && !FBB)
    return (TBB != MBB);  // Conditional branch with no unconditional branch.

  return false;
}

// EmitBasicBlockStart - This method prints the label for the specified
// MachineBasicBlock, an alignment (if present) and a comment describing
// it if appropriate.
void SHAVEAsmPrinter::emitBasicBlockStart(const MachineBasicBlock &MBB) {
  if (!functionHasNoInstructions) {
    // Dump any remaining NOPs that have not yet been emitted from the preceding BB (if any)
    FlushPendingNOPs();

    std::vector<MCSymbol *> labelsToPrint;

    // Get the primary label for the block if it is needed
    if (MBB.hasAddressTaken() || (!MBB.pred_empty() && (!isBlockOnlyReachableByFallthrough(&MBB))))
      labelsToPrint.push_back(MBB.getSymbol());

    // If the block has its address taken, add any labels that were used to reference the block.
    // It is possible that there are more than one labels here, because multiple LLVM BB's may
    // have been RAUW'd to this block after the references were generated
    if (MBB.hasAddressTaken()) {
      const BasicBlock *BB = MBB.getBasicBlock();
      std::vector<MCSymbol *>	labels = getAddrLabelSymbolToEmit(BB);

      // A Basic-Block can have more than one active labels following optimisations, so add them too
      for (MCSymbol * label : labels)
        labelsToPrint.push_back(label);
    }

    // Add a blank line if there are any labels to emit - skip alignment if the basic-block has no target labels
    if (!labelsToPrint.empty()) {
      OutStreamer->addBlankLine();

      // Handle alignment
      int bbAlignment = MBB.getAlignment().value();
      // This is a power of 2, so 0 means unaligned
      // Never expect an alignment that is not on a 16-byte boundary
      if ((bbAlignment != 1) && (bbAlignment != 16)) {
        std::stringstream msg;

        msg << "Not expecting to see the Basic-Block alignment not equal to zero or 2^^4 for SHAVE; got 2^^" << std::dec << bbAlignment << std::endl;
        llvm_unreachable(msg.str().c_str());
      }

      // FIXME: Movidius - TODO: We need to provide a real implementation of '-falign-loops's and '-falign-jumps'

      // WARNING: Label alignment must always use '.lalign' and never '.align'
      // Only emit '.lalign' if all BBs are to be aligned, or if the BB has an alignment of 4
      // and at least one of the alignment options is selected
      if (SHAVEOptions::AlignAllLabels || ((SHAVEOptions::AlignJumpLabels || SHAVEOptions::AlignLoopLabels) && (4 == bbAlignment)))
        OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_LALIGN));
    }

    // Write out each of the labels associated with the block
    for (MCSymbol* label : labelsToPrint)
      OutStreamer->emitLabel(label);
  }
}

/// doInitialization - Perform Module level initializations here.
/// One task that we do here is to sectionize all global variables.
/// The MemSelOptimizer pass depends on the sectionizing.
///
bool SHAVEAsmPrinter::doInitialization(Module &M) {
  bool Result = AsmPrinter::doInitialization(M);

  // Set the section names for all globals.
  for (Module::global_iterator I = M.global_begin(), E = M.global_end(); I != E; ++I) {
    // Record External Var Decls.
    if (I->isDeclaration())
      continue;

    // Sectionify actual data.
    if (!I->hasAvailableExternallyLinkage()) {
      const MCSection *S = getObjFileLowering().SectionForGlobal(&(*I), TM);

      // FIXME: Movidius - what is this actually doing, and is it necessary?
      if (S)
        I->setSection(((const SHAVESection *)S)->getName());
    }
  }

  return Result;
}

bool SHAVEAsmPrinter::doFinalization(Module &M) {
  // Create the IAT tables
  if (!exportedNames.empty()) {
    OutStreamer->addBlankLine();
    OutStreamer->addBlankLine();

    OutStreamer->getCommentOS () << "Indirect access table (IAT) and IAT name lookup table\n";

    // First output the strings for the names for IAT names lookup table
    OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_DATA_SECTION) + SHAVEOptions::SectionPrefix + SHAVE_SECTION_NAME_IAT_STRINGS);
    OutStreamer->addBlankLine();

    int nameIndex = 0;

    for (const std::string &iExport : exportedNames) {
      OutStreamer->emitRawText(Twine(SHAVE_LABEL_INDENT) + SHAVEOptions::SectionPrefix + SHAVE_IAT_NAME_PREFIX + Twine(nameIndex) + ":");
      OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_STRING)
        + "\""
        + iExport
        + "\"");
      ++nameIndex;
    }

    OutStreamer->addBlankLine();
    OutStreamer->addBlankLine();

    // Next create the IAT itself placing each entry in a unique section so that they can be sorted
    for (const std::string &iExport : exportedNames) {
      OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_DATA_SECTION) + SHAVEOptions::SectionPrefix + SHAVE_IAT_ENTRY_PREFIX + iExport);
      OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_ALIGN) + "4");
      OutStreamer->addBlankLine();

      OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_WORD) + iExport);
      OutStreamer->addBlankLine();
    }

    OutStreamer->addBlankLine();

    // Then create the IAT names lookup table placing each entry in a unique section so that they can be sorted
    nameIndex = 0;

    for (const std::string &iExport : exportedNames) {
      OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_DATA_SECTION) + SHAVEOptions::SectionPrefix + SHAVE_SECTION_NAME_IAT_NAMES + iExport);
      OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_ALIGN) + "4");
      OutStreamer->addBlankLine();

      OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_WORD)
                               + SHAVE_IAT_NAME_PREFIX
                               + Twine(nameIndex));
      OutStreamer->addBlankLine();
      ++nameIndex;
    }

    OutStreamer->addBlankLine();

    // Finally, ensure that the IAT names table null terminator exists
    OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_DATA_SECTION)
                             + SHAVE_LINKONCE_PREFIX
                             + SHAVEOptions::SectionPrefix
                             + SHAVE_SECTION_NAME_IAT_NAMESEND);
    OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_LINKONCE));
    OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_ALIGN) + "4");
    OutStreamer->addBlankLine();
    OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_WORD) + "0");

    OutStreamer->addBlankLine();
  }

  OutStreamer->addBlankLine();

  bool ret = AsmPrinter::doFinalization(M);

  OutStreamer->addBlankLine();

  OutStreamer->emitRawText(StringRef(SHAVE_DIRECTIVE_NOWARNEND));
  OutStreamer->addBlankLine();
  OutStreamer->emitRawText(Twine(SHAVE_DIRECTIVE_END));

  exportedNames.clear();

  return ret;
}

extern "C" void LLVMInitializeSHAVEAsmPrinter() {
  RegisterAsmPrinter<SHAVEAsmPrinter> X(TheSHAVETarget);
}
