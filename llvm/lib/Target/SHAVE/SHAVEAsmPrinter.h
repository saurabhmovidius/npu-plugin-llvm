//===-- SHAVEAsmPrinter.h - Assembly Code Emitter ---------------*- C++ -*-===//
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

#ifndef SHAVEASMPRINTER_H
#define SHAVEASMPRINTER_H (1)


#include <list>
#include <set>
#include <string>

#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetMachine.h"

#include "MCTargetDesc/SHAVEMCAsmInfo.h"

#include "SHAVE.h"
#include "SHAVEMCInstLower.h"
#include "SHAVETargetMachine.h"
#include "SHAVETargetObjectFile.h"
#include "MCTargetDesc/SHAVEMCTargetStreamer.h"


namespace llvm {
  class LLVM_LIBRARY_VISIBILITY SHAVEAsmPrinter : public AsmPrinter {
    SHAVETargetMachine &SHAVETM;

  public:
    SHAVEAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer);

  protected:
    bool doInitialization(Module &M) override;
    bool doFinalization(Module &M) override;

    CodeGenOptLevel getOptLevel () const { return TM.getOptLevel(); }
    
    SHAVEMCTargetStreamer * getTargetStreamer() {
      return static_cast<SHAVEMCTargetStreamer *>(OutStreamer->getTargetStreamer());
    }
    

  private:
    StringRef getPassName() const override { return "SHAVE Assembly Printer"; }

    /////////////////////////////////////////////////////////////////////////////////////////
    // Inline assembly support functions
    mutable const MachineInstr *LastMI;
    mutable unsigned LastFn;
    mutable unsigned Counter;
    SHAVEMCInstLower MCInstLowering;

    void printOperand(const MachineInstr *MI, int OpNum, raw_ostream &O);
    bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                         const char *ExtraCode,
                         raw_ostream &O) override;
    bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                               const char *ExtraCode,
                               raw_ostream &O) override;
    void PrintSpecial(const MachineInstr *MI, raw_ostream &OS,
                      StringRef Code) const override;

    void emitInlineAsmStart() const override;
    void emitInlineAsmEnd(const MCSubtargetInfo &StartInfo, const MCSubtargetInfo *EndInfo) const override;

    /////////////////////////////////////////////////////////////////////////////////////////

    const SHAVETargetObjectFile &getObjFileLowering() const {
      return static_cast<const SHAVETargetObjectFile &>(AsmPrinter::getObjFileLowering());
    }

    void emitFunctionHeader() override;
    void emitFunctionBodyEnd() override;
    void emitFunctionEntryLabel() override;
    void emitStartOfAsmFile(Module &) override;

    void addToBundle(const MachineInstr *MI, SmallVectorImpl<MCInst> &Bundle);
    void emitInstruction(const MachineInstr *MI) override;
    void emitBasicBlockStart(const MachineBasicBlock &MBB) override;
    bool isBlockOnlyReachableByFallthrough(const MachineBasicBlock *MBB) const override;

    bool isIntrinsicFunction (const std::string &Name);

    const SHAVETargetObjectFile &PTOF;

    std::set<std::string> exportedNames; // Set of function names for IAT creation

    mutable unsigned int nPendingNOPs;
    void FlushPendingNOPs () const;

    bool functionHasNoInstructions; // See Bugzilla #27955
  };
} // End of namespace 'llvm'


#endif  // SHAVEASMPRINTER_H
