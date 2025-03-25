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
// File       : SHAVEOperands.td
// Description: Definitions of operand types and patterns for instruction
//				selection
// ---------------------------------------------------------------------------


// Immediate predicates:
// generic signed immediate pattern
class simm<int bitSize> : PatLeaf<(imm),
[{
	int sextVal = int(N->getSExtValue());
	int maxVal = (~(unsigned(0)))>>(8 * sizeof(int) - Bits + 1);
	int minVal = - maxVal - 1;

	return (minVal <= sextVal && sextVal <= maxVal);
}]>
{
	int Bits = bitSize;
}

// generic unsigned immediate pattern
class uimm<int bitSize> : PatLeaf<(imm),
[{
	int zextVal = int(N->getZExtValue());
	int maxVal = (~(unsigned(0)))>>(8 * sizeof(int) - Bits);

	return (0 <= zextVal && zextVal <= maxVal);
}]>
{
	int Bits = bitSize;
}

def immHi : PatLeaf<(imm),
[{
	return false;//(zextVal > 0xFFFF && 0 == (zextVal & 0xFFFF));
}]>;

def immLo : PatLeaf<(imm),
[{
	return true; //(zextVal <= 0xFFFF || 0 != (zextVal & 0xFFFF));
}]>;

// concrete immediate patterns
def uimm32 : PatLeaf<(imm),
		[{
			int zextVal = int(N->getZExtValue());
			int maxVal = (~(unsigned(0)))>>(8 * sizeof(int) - 32);

			return (0 <= zextVal && zextVal <= maxVal);
		}]>;
def simm32 : PatLeaf<(imm),
		[{
			int sextVal = int(N->getSExtValue());
			int maxVal = (~(unsigned(0)))>>(8 * sizeof(int) - 32 + 1);
			int minVal = - maxVal - 1;

			return (minVal <= sextVal && sextVal <= maxVal);
		}]>;
def uimm8 : PatLeaf<(imm),
		[{
			uint64_t zextVal = N->getSExtValue();
			return zextVal < 0x100;
		}]>;
def simm8 : PatLeaf<(imm),
		[{
			int sextVal = int(N->getSExtValue());
			int maxVal = (~(unsigned(0)))>>(8 * sizeof(int) - 8 + 1);
			int minVal = - maxVal - 1;

			return (minVal <= sextVal && sextVal <= maxVal);
		}]>;
def uimm16 : PatLeaf<(imm),
		[{
			int zextVal = int(N->getZExtValue());
			int maxVal = (~(unsigned(0)))>>(8 * sizeof(int) - 16);

			return (0 <= zextVal && zextVal <= maxVal);
		}]>;
def simm16 : PatLeaf<(imm),
		[{
			int sextVal = int(N->getSExtValue());
			int maxVal = (~(unsigned(0)))>>(8 * sizeof(int) - 16 + 1);
			int minVal = - maxVal - 1;

			return (minVal <= sextVal && sextVal <= maxVal);
		}]>;
def uimm5 : PatLeaf<(imm),
		[{
			int sextVal = int(N->getZExtValue());
			int maxVal = (~(unsigned(0)))>>(8 * sizeof(int) - 5);

			return (0 <= sextVal && sextVal <= maxVal);
		}]>;
def simm5 : PatLeaf<(imm),
		[{
			int zextVal = int(N->getZExtValue());

		    EVT VT = N->getValueType(0);
			if (VT == MVT::i8)
			{
				zextVal = (char)zextVal;
			}
			else if (VT == MVT::i16)
			{
				zextVal = (short)zextVal;
			}

			int maxVal = (~(unsigned(0)))>>(8 * sizeof(int) - 5 + 1);
			int minVal = - maxVal - 1;

			return (minVal <= zextVal && zextVal <= maxVal);
		}]>;
def uimm4 : PatLeaf<(imm),
    [{
      uint64_t zextVal = N->getZExtValue();
      const uint64_t maxVal = 0x0F;

      return (zextVal <= maxVal);
    }]>;
def uimm3 : PatLeaf<(imm),
    [{
      uint64_t zextVal = N->getZExtValue();
      const uint64_t maxVal = 0x08;

      return (zextVal <= maxVal);
    }]>;
def imm8 : PatLeaf<(imm),
    [{
      return (N->getValueType(0) == MVT::i8);
    }]>;

def imm16 : PatLeaf<(imm),
    [{
      return (N->getValueType(0) == MVT::i16);
    }]>;

def simm14 : PatLeaf<(imm),
    [{
        int64_t sextVal = N->getSExtValue();
        return (/*-2^8*/-8192 <= sextVal && sextVal <= 8191 /*2^8-1*/);

    }]>;

// Float constants

def FloatConst : PatLeaf<(fpimm),
[{
    return (N &&
        (N->getValueType(0) == MVT::f32) &&
        (SHAVECC::ConstantFPToConstFloat(N->getConstantFPValue(), 32) != SHAVECC::CONST_FLOAT_NUM));
}]>;

def HalfFloatConst : PatLeaf<(fpimm),
[{
    return (N &&
        (N->getValueType(0) == MVT::f16) &&
        (SHAVECC::ConstantFPToConstFloat(N->getConstantFPValue(), 16) != SHAVECC::CONST_FLOAT_NUM));
}]>;

def One_half : PatLeaf<(fpimm),
[{
    ConstantFPSDNode *fpNode = dyn_cast<ConstantFPSDNode>(N);
    return fpNode && fpNode->isExactlyValue(1.0) && (fpNode->getValueType(0) == MVT::f16);
}]>;

def u8fConst : PatLeaf<(fpimm),
[{
    ConstantFPSDNode *fpNode = dyn_cast<ConstantFPSDNode>(N);
    return fpNode && fpNode->isExactlyValue(255.0) && (fpNode->getValueType(0) == MVT::f16);
}]>;

// FL_IMM immediate values
let PrintMethod = "printConstFloatOperand" in {
  def constf32 : Operand<f32>;
  def constf16 : Operand<f16>;
}

// Operands:

// memory address operand
def ADDRr : ComplexPattern<iPTR, 1, "SelectADDRr">;
def MEMr : Operand<iPTR>
{
	let MIOperandInfo = (ops IRF32);
}

// memory addressing base + immediate offset
def ADDRri  : ComplexPattern<iPTR, 2, "SelectADDRri", [frameindex]>;
def ADDRri5 : ComplexPattern<iPTR, 2, "SelectADDRri5", [frameindex]>;
def ADDRri4 : ComplexPattern<iPTR, 2, "SelectADDRri4", [frameindex]>;
def ADDRri3 : ComplexPattern<iPTR, 2, "SelectADDRri3", [frameindex]>;
def ADDRri2 : ComplexPattern<iPTR, 2, "SelectADDRri2", [frameindex]>;
def MEMri : Operand<iPTR>
{
  let PrintMethod = "printMemOffsetOperand";
  let MIOperandInfo = (ops IRF32, i16imm);
}

// memory addressing base + register offset
def ADDRrr : ComplexPattern<iPTR, 2, "SelectADDRrr", [add]>;
def MEMrr : Operand<iPTR>
{
  let PrintMethod = "printMemOffsetOperand";
  let MIOperandInfo = (ops IRF32, IRF16_l);
}

def ADDRr32r32 : ComplexPattern<iPTR, 2, "SelectADDRrr", [add]>;
def MEMr32r32 : Operand<iPTR>
{
  let PrintMethod = "printMemOffsetOperand";
  let MIOperandInfo = (ops IRF32, IRF32);
}

def ADDRbb : PatLeaf<(tblockaddress), [{return true;}]>;
def SymBB : Operand<i32>;

def Label : Operand<OtherVT>;

class FUnitOperand<int num> : OperandWithDefaultOps<i8, (ops (i8 num))>
{
  let PrintMethod = "printFUnitOperand";
}

// This must be kept in sync with SHAVEFuncUnit from SHAVEInstrInfo.h
def iau_id : FUnitOperand<1>;
def sau_id : FUnitOperand<2>;
def vau_id : FUnitOperand<3>;
def peu_id : FUnitOperand<4>;
def cmu_id : FUnitOperand<5>;
def bru_id : FUnitOperand<6>;
def lsu0_id : FUnitOperand<7>;
def lsu1_id : FUnitOperand<8>;
def lsu_id : FUnitOperand<7>;

def LO16 : SDNodeXForm<imm,
[{
	unsigned val = N->getZExtValue();
	// Transformation function: get the low 16 bits.
	return getI32Imm(val & 0xffff);
}]>;

// Transform an immediate, returning the high 16 bits shifted down:
def HI16 : SDNodeXForm<imm,
[{
	return getI32Imm((unsigned)N->getZExtValue() >> 16);
}]>;

// Predicate operands: will be transformed in a PEU instruction
def pred : PredicateOperand<OtherVT, (ops i32imm, PEU_TRF), (ops (i32 1), (i32 zero_reg))>;


