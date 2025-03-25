//===-- SHAVEMCInstLower.h - Lower MI to MCInst -----------------*- C++ -*-===//
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

#ifndef SHAVEMCINSTLOWER_H
#define SHAVEMCINSTLOWER_H (1)


#include <string>
#include <map>

#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/Support/Compiler.h"


namespace llvm {
  class BasicBlock;
  class MCContext;
  class MCInst;
  class MCOperand;
  class MachineInstr;
  class MachineFunction;
  class Mangler;
  class AsmPrinter;

  /// \brief This class is used to lower an MachineInstr into an MCInst.
  class LLVM_LIBRARY_VISIBILITY SHAVEMCInstLower {
    typedef MachineOperand::MachineOperandType MachineOperandType;
    MCContext *Ctx = nullptr;
    AsmPrinter &Printer;

  public:
    SHAVEMCInstLower(class AsmPrinter &asmprinter);
    void Initialize(MCContext *C);
    void Lower(const MachineInstr *MI, MCInst &OutMI) const;
    MCOperand LowerOperand(const MachineOperand &MO, unsigned offset = 0) const;

  private:
    MCOperand LowerSymbolOperand(const MachineOperand &MO,
                                 MachineOperandType MOTy, unsigned Offset) const;
    MCOperand LowerFPImmOperand(const MachineOperand &MO) const;
    // FIXME: Movidius - remove this if the LLVM implementation works.
#if 0
    MCSymbol *GetBlockAddressSymbol(const BlockAddress *BA, 
                                    const MachineFunction &MF) const;
#endif
    MCSymbol *GetExternalSymbolSymbol(StringRef Sym) const;
    bool isIntrinsicFunction(llvm::StringRef Name) const;
  };
} // End of namespace 'llvm'


#endif  // SHAVEMCINSTLOWER_H
