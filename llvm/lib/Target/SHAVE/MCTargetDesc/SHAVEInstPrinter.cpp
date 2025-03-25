//===-- SHAVEInstPrinter.cpp - Instruction Printer --------------*- C++ -*-===//
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

#define DEBUG_TYPE "SHAVE-inst-printer"

#include "llvm/ADT/StringExtras.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/raw_ostream.h"

#include "MCTargetDesc/SHAVEMCTargetDesc.h"
#include "SHAVE.h"
#include "SHAVEAsmPrinter.h"
#include "SHAVEInstPrinter.h"
#include "SHAVENames.h"

using namespace llvm;

#include "SHAVEGenAsmWriter.inc"


namespace {
  bool isLDILOp(unsigned Opc) {
    switch (Opc) {
    case SHAVE::LSU_LDIL:
      return true;
    default:
      return false;
    }
  }

  bool isLDIHOp(unsigned Opc) {
    switch (Opc) {
    case SHAVE::LSU_LDIH:
      return true;
    default:
      return false;
    }
  }

  void printExpr(const MCExpr *Expr, raw_ostream &OS) {
    const MCSymbolRefExpr *SRE = cast<MCSymbolRefExpr>(Expr);
    OS << SRE->getSymbol();
  }
} // End of anonymous namespace


void SHAVEInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) const {
  OS << StringRef(getRegisterName(Reg));
}

void SHAVEInstPrinter::printInst(const MCInst *MI, uint64_t Address, StringRef Annot, const MCSubtargetInfo &STI, raw_ostream &O) {
  unsigned Opc = MI->getOpcode();

  if (Opc == TargetOpcode::BUNDLE) {
    bool isFirstInstructionInBundle = true;

    for (unsigned i = 0; i < MI->getNumOperands(); i++) {
      const MCInst *BundledMI = MI->getOperand(i).getInst();
      unsigned opCode = BundledMI->getOpcode();

      // FIXME: Movidius - the test for PHI is needed as a consequence of finding and removing debug meta-instructions
      //        from bundles - but they shouldn't be in bundled in the first place!  See also the FIXME note in:
      //            SHAVEAsmScheduler::removeDebugInfo()
      if ((opCode != SHAVE::CFI_INSTRUCTION) && (opCode != SHAVE::DBG_VALUE) && (opCode != SHAVE::PHI)) {
        if (isFirstInstructionInBundle) {
          O << SHAVE_CODE_INDENT;
          isFirstInstructionInBundle = false;
        } else
          O << SHAVE_PARALLEL_INDENT "|| ";

        printInstruction(BundledMI, 0, O);
      } else {
        // FIXME: Movidius - we need to handle CFI_INSTRUCTION in bundles, perhaps by extracting them and pre-printing them
        O << " // Unable to print debug meta-instructions for opcode '" << opCode << "' inside a bundle\n";
        continue;
//        assert(0 && "Unable to print debug meta-instructions inside a bundle");
      }

      if ((i + 1) < MI->getNumOperands())
        O << "\n";
    }
  }
  else if ((Opc == SHAVE::CFI_INSTRUCTION) || (Opc == SHAVE::DBG_VALUE)) {
    // FIXME: Movidius - we need to handle CFI_INSTRUCTION
    // emitPrologLabel(*MI);
    O << " // Unable to print debug meta-instructions";
    //    assert(0 && "Unable to print debug meta-instructions");
  } else {
    O << SHAVE_CODE_INDENT;
    printInstruction(MI, 0, O);
  }
  printAnnotation(O, Annot);
}

void SHAVEInstPrinter::printOperand(const MCInst *MI, int opNum, raw_ostream &O) const {
  unsigned Opc = MI->getOpcode();
  const MCOperand &MO = MI->getOperand(opNum);
  if (MO.isReg()) {
    printRegName(O, MO.getReg());
    return;
  }

  if (MO.isImm()) {
    const size_t buffSize = 32;
    if (isLDILOp(Opc) || (Opc == SHAVE::BRU_SWIH_imm)) {
      char buff[buffSize];

      uint16_t immval = (((uint32_t)MO.getImm()) & 0x0000FFFF);
      if (immval)
        snprintf(buff, buffSize, "0x%.4x", immval);
      else
        snprintf(buff, buffSize, "0");

      O << buff;
    } else if (isLDIHOp(Opc)) {
      char buff[buffSize];

      uint16_t immval = (((uint32_t)MO.getImm()) & 0xFFFF0000) >> 16;
      if (immval)
        snprintf(buff, buffSize, "0x%.4x", immval);
      else
        snprintf(buff, buffSize, "0");

      O << buff;
    } else if ((Opc == SHAVE::PEU_PC1C)    || (Opc == SHAVE::PEU_PVV_32) ||
               (Opc == SHAVE::PEU_PVV_16)  || (Opc == SHAVE::PEU_PVV_8) ||
               (Opc == SHAVE::PEU_PVS_32)  || (Opc == SHAVE::PEU_PVS_16) ||
               (Opc == SHAVE::PEU_PVS_8)   || (Opc == SHAVE::PEU_PVL0_32) || 
               (Opc == SHAVE::PEU_PVL1_32) || (Opc == SHAVE::PEU_PVL0_16) ||
               (Opc == SHAVE::PEU_PVL1_16) || (Opc == SHAVE::PEU_PVL0_8) ||
               (Opc == SHAVE::PEU_PVL1_8))
      O << SHAVECC::SHAVECondCodeToString((SHAVECC::CondCode)MO.getImm());
    else if ((Opc == SHAVE::PEU_PCCX || Opc == SHAVE::PEU_PCXC) && opNum == 0)
      O << SHAVECC::SHAVECondCodeToString((SHAVECC::CondCode)MO.getImm(), Opc == SHAVE::PEU_PCCX);
    else
      O << MO.getImm();

    return;
  }

  assert(MO.isExpr() && "unknown operand kind in printOperand");
  printExpr(MO.getExpr(), O);
}

void SHAVEInstPrinter::PrintSpecial(const MCInst *MI, raw_ostream &OS,
                                    const char *Code) const {
  if (!strcmp(Code, "private"))
    OS << MAI.getPrivateGlobalPrefix();
  else if (!strcmp(Code, "comment"))
    OS << MAI.getCommentString();
  else {
    std::string msg;
    raw_string_ostream Msg(msg);
    Msg << "Unknown special formatter '" << Code
        << "' for machine instr: " << *MI;
    report_fatal_error(Twine(Msg.str()));
  }
}

void SHAVEInstPrinter::printByteEnableOperand(const MCInst *MI, int opNum,
                                              raw_ostream &O) const {
  const MCOperand &Op = MI->getOperand(opNum);
  O << (Op.getImm() ? "E" : "D");
}

void SHAVEInstPrinter::printCMBSize(const MCInst *MI, int opNum,
  raw_ostream &O) const {
  const MCOperand &Op = MI->getOperand(opNum);
  switch (Op.getImm()) {
  case 32: O << "32.8"; break;
  case 64: O << "64.16"; break;
  case 128: O << "128.32"; break;
  case 256: O << "256.64"; break;
  case 512: O << "512.128"; break;
  default:
    std::string msg;
    raw_string_ostream Msg(msg);
    Msg << "Unknown size specifier '" << Op.getImm()
      << "' for machine instr: " << *MI;
    report_fatal_error(Twine(Msg.str()));
  }
}

void SHAVEInstPrinter::printFUnitOperand(const MCInst *MI, int opNum,
                                         raw_ostream &O) const {
  const MCOperand &Op = MI->getOperand(opNum);

  switch (Op.getImm()) {
  default:
    break;
  case SHAVE::IAU:
    O << "IAU";
    break;
  case SHAVE::SAU:
    O << "SAU";
    break;
  case SHAVE::VAU:
    O << "VAU";
    break;
  case SHAVE::PEU:
    O << "PEU";
    break;
  case SHAVE::CMU:
    O << "CMU";
    break;
  case SHAVE::BRU:
    O << "BRU";
    break;
  case SHAVE::LSU0:
    O << "LSU0";
    break;
  case SHAVE::LSU1:
    O << "LSU1";
    break;
  }
}

void SHAVEInstPrinter::printConstFloatOperand(const MCInst *MI, int opNum,
                                               raw_ostream &O) const {
  const MCOperand &Op = MI->getOperand(opNum);
  SHAVECC::FloatConst Imm = (SHAVECC::FloatConst)Op.getImm();

  if (Imm >= SHAVECC::CONST_FLOAT_NUM) {
    llvm_unreachable("Invalid floating-point constant");
    return;
  }
  O << SHAVECC::SHAVEConstFloatToString(Imm);
}

void SHAVEInstPrinter::printMemOffsetOperand(const MCInst *MI, int opNum,
                                             raw_ostream &O) const {
  const MCOperand &Op1 = MI->getOperand(opNum);

  if (Op1.isReg() && (unsigned (opNum + 1) < MI->getNumOperands())) {
    assert(Register::isPhysicalRegister(Op1.getReg()) && "Not physreg??");
    O << getRegisterName(Op1.getReg()) << " ";

    const MCOperand &Op2 = MI->getOperand(opNum + 1);

    if (Op2.isReg()) {
      assert(Register::isPhysicalRegister(Op2.getReg()) && "Not physreg??");
      O << getRegisterName(Op2.getReg());
      return;
    } else if (Op2.isImm()) {
      O << Op2.getImm();
      return;
    }
  }

  report_fatal_error("printMemOffsetOperand");
}

void SHAVEInstPrinter::printVSZMOperand(const MCInst *MI, int opNum,
                                        raw_ostream &O) const {
  const MCOperand &Op = MI->getOperand(opNum);
  switch(Op.getImm())
  {
    case VSZM_CLEAR: O << "Z"; break;
    case VSZM_DISABLE: O << "D"; break;
  default:
    if (Op.getImm() >= VSZM_LANE0)
      O << (Op.getImm() - VSZM_LANE0);
    break;
  }
}

void SHAVEInstPrinter::printSWZM8Operand(const MCInst *MI, int opNum,
                                         raw_ostream &O) const {
  const MCOperand &Op = MI->getOperand(opNum);
  switch (Op.getImm())
  {
  case SWZM8_U: O << "U"; break;
  case SWZM8_V: O << "V"; break;
  case SWZM8_Z: O << "Z"; break;
  case SWZM8_1: O << "1"; break;
  }
}

// Print select bits for extract from v4i8
void SHAVEInstPrinter::printVSZMSelectBitsExtract8(const MCInst *MI, int opNum,
                                                   raw_ostream &O) const {
  const MCOperand &Op = MI->getOperand(opNum);
  O << "ZZZ" << Op.getImm();
}

// Print select bits for extract from v2i16, v2f16
void SHAVEInstPrinter::printVSZMSelectBitsExtract16(const MCInst *MI, int opNum,
                                                    raw_ostream &O) const {
  const MCOperand &Op = MI->getOperand(opNum);

  switch (Op.getImm()) {
    case 0:
      O << "ZZ10";
      break;
    case 1:
      O << "ZZ32";
      break;
    default:
      llvm_unreachable("could not match the immediate value");
      break;
  }
}

// Print select bits for inserting into v4i8
void SHAVEInstPrinter::printVSZMSelectBitsInsert8(const MCInst *MI, int opNum,
                                                  raw_ostream &O) const {
  const MCOperand &Op = MI->getOperand(opNum);

  switch (Op.getImm()) {
    case 0:
      O << "DDD0";
      break;
    case 1:
      O << "DD0D";
      break;
    case 2:
      O << "D0DD";
      break;
    case 3:
      O << "0DDD";
      break;
    default:
      llvm_unreachable("could not match the immediate value");
      break;
  }
}

// Print select bits for inserting into v2f16,v2i16
void SHAVEInstPrinter::printVSZMSelectBitsInsert16(const MCInst *MI, int opNum,
                                                   raw_ostream &O) const {
  const MCOperand &Op = MI->getOperand(opNum);

  switch (Op.getImm()) {
    case 0:
      O << "DD10";
      break;
    case 1:
      O << "10DD";
      break;
    default:
      llvm_unreachable("could not match the immediate value");
      break;
  }
}
