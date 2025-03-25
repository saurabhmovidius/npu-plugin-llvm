//===-- SHAVEAsmPrinterInlineAsm.cpp - Inline Assembly Printer --*- C++ -*-===//
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
// Description:  Emit inline assembly code
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Target/TargetMachine.h"

#include "MCTargetDesc/SHAVEInstPrinter.h"
#include "MCTargetDesc/SHAVEOptions.h"
#include "SHAVEAsmPrinter.h"
#include "SHAVENames.h"

using namespace llvm;


// SHAVEPrintSpecial - Print information related to the specified machine instr
// that is independent of the operand, and may be independent of the instr
// itself.  This can be useful for portably encoding the comment character
// or other bits of target-specific knowledge into the asmstrings.  The
// syntax used is ${:comment}.
void SHAVEAsmPrinter::PrintSpecial(const MachineInstr *MI, raw_ostream &OS, StringRef Code) const {
  if (Code.equals("private")) {
    OS << MAI->getPrivateGlobalPrefix();
  } else if (Code.equals("comment")) {
    OS << MAI->getCommentString();
  } else if (Code.equals("uid")) {
    // Comparing the address of MI isn't sufficient, because machineinstrs may
    // be allocated to the same address across functions.

    // If this is a new LastFn instruction, bump the counter.
    if (LastMI != MI || LastFn != getFunctionNumber()) {
      ++Counter;
      LastMI = MI;
      LastFn = getFunctionNumber();
    }
    OS << Counter;
  } else {
    std::string msg;
    raw_string_ostream Msg(msg);
    Msg << "Unknown special formatter '" << Code
        << "' for machine instr: " << *MI;
    report_fatal_error(Twine(Msg.str()));
  }
}

void SHAVEAsmPrinter::printOperand(const MachineInstr *MI, int OpNum,
                                   raw_ostream &O) {
  const MachineOperand &MO = MI->getOperand(OpNum);

  switch (MO.getType()) {
  default: llvm_unreachable("<unknown operand type>");
  case MachineOperand::MO_Register: {
    unsigned Reg = MO.getReg();
    assert(Register::isPhysicalRegister(Reg));
    assert(!MO.getSubReg() && "Subregs should be eliminated!");
    O << SHAVEInstPrinter::getRegisterName(Reg);
    break;
  }
  case MachineOperand::MO_Immediate: {
    O << MO.getImm();
    break;
  }
  case MachineOperand::MO_MachineBasicBlock:
    O << *MO.getMBB()->getSymbol();
    return;
  case MachineOperand::MO_GlobalAddress: {
    const GlobalValue *GV = MO.getGlobal();
    O << *getSymbol(GV);
    printOffset(MO.getOffset(), O);
    break;
  }
  case MachineOperand::MO_ConstantPoolIndex:
    O << *GetCPISymbol(MO.getIndex());
    break;
  }
}

// Print out an operand for an inline asm expression.
bool SHAVEAsmPrinter::PrintAsmOperand(const MachineInstr *MI,
                                      unsigned OpNo,
                                      const char *ExtraCode, raw_ostream &O) {
  // Does this asm operand have a single letter operand modifier?
  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1] != 0) return true; // Unknown modifier.

    // See if this is a generic print operand
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, O);
  }

  printOperand(MI, OpNo, O);
  return false;
}

bool SHAVEAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                            unsigned OpNo,
                                            const char *ExtraCode,
                                            raw_ostream &O) {
  // Does this asm operand have a single letter operand modifier?
  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1] != 0) return true; // Unknown modifier.

    switch (ExtraCode[0]) {
      default: return true;  // Unknown modifier.
      case 'm': // The base register of a memory operand.
        if (!MI->getOperand(OpNo).isReg())
          return true;
        break;
    }
  }

  const MachineOperand &MO = MI->getOperand(OpNo);
  assert(MO.isReg() && "unexpected inline asm memory operand");
  O << SHAVEInstPrinter::getRegisterName(MO.getReg());
  return false;
}

void SHAVEAsmPrinter::emitInlineAsmStart() const {
  FlushPendingNOPs();

  if (MF == nullptr)
    OutStreamer->emitRawText(Twine(SHAVE_ASM_INDENT) + "//" + SHAVE_INLINE_ASM_START);
  OutStreamer->emitRawText(Twine(SHAVE_INLINE_ASM_NOWARNEND));
}

void SHAVEAsmPrinter::emitInlineAsmEnd(const MCSubtargetInfo &StartInfo, const MCSubtargetInfo *EndInfo) const {
  if (MF == nullptr) {
    OutStreamer->emitRawText(Twine(SHAVE_INLINE_ASM_NOWARN));
    OutStreamer->emitRawText(Twine(SHAVE_ASM_INDENT) + "//" + SHAVE_INLINE_ASM_END);
  } else {
    OutStreamer->emitRawText(Twine("\n") + SHAVE_INLINE_ASM_NOWARN);
    if (SHAVEOptions::UseNOPSync)
      OutStreamer->emitRawText(Twine(SHAVE_NOP_SYNC));
  }
}
