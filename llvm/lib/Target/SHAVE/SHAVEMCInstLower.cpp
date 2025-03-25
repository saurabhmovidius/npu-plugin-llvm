//===-- SHAVEMCInstLower.cpp - Lower MI to MCInst ---------------*- C++ -*-===//
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
// Lowers a Machine Instruction to an MCInst
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"

#include "SHAVE.h"
#include "SHAVEMCInstLower.h"
#include "SHAVENames.h"
#include "SHAVESubtarget.h"
#include "SHAVETargetMachine.h"

using namespace llvm;


SHAVEMCInstLower::SHAVEMCInstLower(class AsmPrinter &asmprinter)
    : Printer(asmprinter) {}

void SHAVEMCInstLower::Initialize(MCContext *C) {
  Ctx = C;
}

MCOperand SHAVEMCInstLower::LowerSymbolOperand(const MachineOperand &MO,
                                               MachineOperandType MOTy,
                                               unsigned Offset) const {
  const MCSymbol *Symbol = nullptr;

  switch (MOTy) {
  case MachineOperand::MO_MachineBasicBlock:
    Symbol = MO.getMBB()->getSymbol();
    break;
  case MachineOperand::MO_GlobalAddress:
    Symbol = Printer.getSymbol(MO.getGlobal());
    Offset += MO.getOffset();
    break;
  case MachineOperand::MO_BlockAddress:
  // FIXME: Movidius - remove this if the LLVM implementation works.
#if 0
    Symbol = GetBlockAddressSymbol(MO.getBlockAddress(),
                                   *MO.getParent()->getParent()->getParent());
#endif
    Symbol = Printer.GetBlockAddressSymbol(MO.getBlockAddress());
    Offset += MO.getOffset();
    break;
  case MachineOperand::MO_ExternalSymbol:
    Symbol = GetExternalSymbolSymbol(MO.getSymbolName());
    Offset += MO.getOffset();
    break;
  case MachineOperand::MO_JumpTableIndex:
    Symbol = Printer.GetJTISymbol(MO.getIndex());
    break;
  case MachineOperand::MO_ConstantPoolIndex:
    Symbol = Printer.GetCPISymbol(MO.getIndex());
    Offset += MO.getOffset();
    break;
  default:
    llvm_unreachable("<unknown operand type>");
  }

  MCSymbolRefExpr::VariantKind Kind = MCSymbolRefExpr::VK_None;
  const MCSymbolRefExpr *MCSym = MCSymbolRefExpr::create(Symbol, Kind, *Ctx);

  if (!Offset)
    return MCOperand::createExpr(MCSym);

  // Assume offset is never negative.
  assert(Offset > 0);

  const MCConstantExpr *OffsetExpr = MCConstantExpr::create(Offset, *Ctx);
  const MCBinaryExpr *Add = MCBinaryExpr::createAdd(MCSym, OffsetExpr, *Ctx);

  return MCOperand::createExpr(Add);
}

MCOperand SHAVEMCInstLower::LowerFPImmOperand(const MachineOperand &MO) const {
  const ConstantFP *Val = MO.getFPImm();
  Type *Ty = Val->getType();
  int ConstantID = 0;

  if (Ty->isFloatTy())
    ConstantID = SHAVECC::ConstantFPToConstFloat(MO.getFPImm(), 32);
  else if (Ty->isHalfTy())
    ConstantID = SHAVECC::ConstantFPToConstFloat(MO.getFPImm(), 16);
  else
    llvm_unreachable("Unsupported type for FP immediate");

  return MCOperand::createImm(ConstantID);
}

MCOperand SHAVEMCInstLower::LowerOperand(const MachineOperand &MO,
                                         unsigned offset) const {
  MachineOperandType MOTy = MO.getType();

  switch (MOTy) {
  default:
    llvm_unreachable("unknown operand type");
  case MachineOperand::MO_Register:
    // Ignore all implicit register operands.
    if (MO.isImplicit())
      break;
    return MCOperand::createReg(MO.getReg());
  case MachineOperand::MO_Immediate:
    return MCOperand::createImm(MO.getImm() + offset);
  case MachineOperand::MO_FPImmediate:
    return LowerFPImmOperand(MO);
  case MachineOperand::MO_MachineBasicBlock:
  case MachineOperand::MO_GlobalAddress:
  case MachineOperand::MO_ExternalSymbol:
  case MachineOperand::MO_JumpTableIndex:
  case MachineOperand::MO_ConstantPoolIndex:
  case MachineOperand::MO_BlockAddress:
    return LowerSymbolOperand(MO, MOTy, offset);
  case MachineOperand::MO_RegisterMask:
    break;
  case MachineOperand::MO_Metadata:
  case MachineOperand::MO_CFIIndex: // FIXME: Movidius - is ignoring these the right thing to do?
    break;
  }

  return MCOperand();
}

MCSymbol *SHAVEMCInstLower::GetExternalSymbolSymbol(StringRef Sym) const {
  std::string ActualName = Sym.str();

  if (isIntrinsicFunction(Sym))
    ActualName.insert(0, SHAVE_GLOBAL_PREFIX);

  return Printer.GetExternalSymbolSymbol(ActualName);
}

bool SHAVEMCInstLower::isIntrinsicFunction(StringRef Name) const {
  const TargetLowering *TL = static_cast<const SHAVETargetMachine&>(Printer.TM).getSubtargetImpl()->getTargetLowering();

  for (unsigned i = 0; i < RTLIB::UNKNOWN_LIBCALL; i++) {
    // If Libcall is available
    if (TL->getLibcallName((RTLIB::Libcall) i))
      if(!Name.compare(TL->getLibcallName((RTLIB::Libcall) i)))
        return true;
  }

  // Special cases...
  if (!Name.compare("abort"))
    return true;

  return false;
}

void SHAVEMCInstLower::Lower(const MachineInstr *MI, MCInst &OutMI) const {
  OutMI.setOpcode(MI->getOpcode());

  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
    const MachineOperand &MO = MI->getOperand(i);
    MCOperand MCOp = LowerOperand(MO);

    if (MCOp.isValid())
      OutMI.addOperand(MCOp);
  }
}
