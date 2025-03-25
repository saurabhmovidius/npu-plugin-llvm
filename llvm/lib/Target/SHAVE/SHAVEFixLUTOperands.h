// ***************************************************************************
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
// ---------------------------------------------------------------------------

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/Function.h"

using namespace llvm;

class SHAVEFixLUTOperands : public MachineFunctionPass 
{
public:
    static char ID;

    SHAVEFixLUTOperands();
    virtual bool runOnMachineFunction(MachineFunction &MF) override;

private:
    StringRef getPassName() const override { return "SHAVE Fix LUT Operands"; }

    static bool isLUTPseudoInstruction(unsigned opcode, const MachineFunction &MF);
    static bool getLUTInfo(unsigned opcode, unsigned &laneWidth, bool &isWrite);
    static bool getVRFIndex(unsigned Reg, int &VRFIdx);
    static unsigned getLUTOpcodeFromPseudo(unsigned pseudoOpcode);
    void fixLUTOperands(MachineBasicBlock::iterator I);
};
