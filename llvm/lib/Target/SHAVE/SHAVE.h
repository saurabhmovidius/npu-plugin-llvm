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
// File       :  SHAVE.h
// Description:  SHAVE header file
// ---------------------------------------------------------------------------


#ifndef LLVM_TARGET_SHAVE_SHAVE_H
#define LLVM_TARGET_SHAVE_SHAVE_H (1)

#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"

#include <optional>

#define DEBUG LLVM_DEBUG

namespace llvm {
  extern Target TheSHAVETarget;

  class ModulePass;
  class FunctionPass;
  class MachineFunctionPass;
  class SHAVETargetMachine;
  class PassRegistry;

  enum SHAVESectionType {
    DATA,
    CODE,
    BSS,
    DEBUG
  };

  enum SHAVEVSZM {
    VSZM_CLEAR = 0,
    VSZM_DISABLE = 1,
    VSZM_LANE0 = 4
  };

  enum SHAVESWZM8 {
    SWZM8_U,
    SWZM8_V,
    SWZM8_Z,
    SWZM8_1
  };

  extern char &SHAVEExperimentalPreRAScheduleID;

  FunctionPass *createSHAVEISelDAGtoDAG(SHAVETargetMachine &TM);
  FunctionPass *createSHAVEPreemptionHandlerPass();
  FunctionPass *createSHAVEPostSchedulingPreemptionPass();
  FunctionPass *createSHAVEPredicator();
  FunctionPass *createSHAVELoopLoadHoister();
  FunctionPass *createSHAVECopyAddCombining();
  FunctionPass *createSHAVEWidenVectorTypesPass();

  MachineFunctionPass *createSHAVEAddressSimplifierPass();
  MachineFunctionPass *createSHAVEPreRASchedPeepholePass();
  MachineFunctionPass *createSHAVEPreRASchedulerPass();
  MachineFunctionPass *createSHAVEPreSched2PeepholePass();
  MachineFunctionPass *createSHAVEAntiDepBreakerPass();
  MachineFunctionPass *createSHAVEInterblockMovementPass();
  MachineFunctionPass *createSHAVEPostRASchedPass();

  void initializeSHAVEAddressSimplifierPass(PassRegistry&);
  void initializeSHAVEWidenVectorTypesPass(PassRegistry&);
  void initializeSHAVEPreRASchedPeepholePass(PassRegistry&);
  void initializeSHAVEPreRASchedulerPass(PassRegistry&);
  void initializeSHAVEPreSched2PeepholePass(PassRegistry&);
  void initializeSHAVEAntiDepBreakerPass(PassRegistry&);
  void initializeSHAVEInterblockMovementPass(PassRegistry&);
  void initializeSHAVEPostRASchedPass(PassRegistry&);

  namespace SHAVECC {
    enum CondCode {
      NONE,
      AL, // always execute
      EQ,
      GT,
      GTE,
      LT,
      LTE,
      NEQ,
      // FP Ordered variants (everything except ONE, intentionally left out)
      O,
      OEQ,
      OGT,
      OGTE,
      OLT,
      OLTE,
      // FP Unordered variants
      UO,
      UONEQ
    };

    namespace {  // Does this really need an anonymous namespace (or static?)
      inline bool isSignedComparison(ISD::CondCode CC) {
        return ISD::isSignedIntSetCC(CC) || ISD::SETEQ == CC || ISD::SETNE == CC;
      }

      inline bool isUnsignedComparison(ISD::CondCode CC) {
        return ISD::isUnsignedIntSetCC(CC);// ||  ISD::SETEQ == CC || ISD::SETNE == CC;
      }

      inline CondCode getSHAVECondCode(ISD::CondCode CC, bool isFloatingPoint) {
        switch (CC) {
        case ISD::SETOEQ:
          return SHAVECC::OEQ;
        case ISD::SETUEQ:
          if (isFloatingPoint)
            llvm_unreachable("Unordered floating-point comparison ISD::SETUEQ not supported on SHAVE");
          LLVM_FALLTHROUGH;
        case ISD::SETEQ:
          return SHAVECC::EQ;

        case ISD::SETOGT:
          return SHAVECC::OGT;
        case ISD::SETUGT:
          if (isFloatingPoint)
            llvm_unreachable("Unordered floating-point comparison ISD::SETUGT not supported on SHAVE");
          LLVM_FALLTHROUGH;
        case ISD::SETGT:
          return SHAVECC::GT;

        case ISD::SETOGE:
          return SHAVECC::OGTE;
        case ISD::SETUGE:
          if (isFloatingPoint)
            llvm_unreachable("Unordered floating-point comparison ISD::SETUGE not supported on SHAVE");
          LLVM_FALLTHROUGH;
        case ISD::SETGE:
          return SHAVECC::GTE;

        case ISD::SETOLT:
          return SHAVECC::OLT;
        case ISD::SETULT:
          if (isFloatingPoint)
            llvm_unreachable("Unordered floating-point comparison ISD::SETULT not supported on SHAVE");
          LLVM_FALLTHROUGH;
        case ISD::SETLT:
          return SHAVECC::LT;

        case ISD::SETOLE:
          return SHAVECC::OLTE;
        case ISD::SETULE:
          if (isFloatingPoint)
            llvm_unreachable("Unordered floating-point comparison ISD::SETULE not supported on SHAVE");
          LLVM_FALLTHROUGH;
        case ISD::SETLE:
          return SHAVECC::LTE;

        case ISD::SETUNE:
          if (isFloatingPoint)
            return SHAVECC::UONEQ; // Only supported unordered floating-point comparison
          LLVM_FALLTHROUGH;
        case ISD::SETNE:
          return SHAVECC::NEQ;

        case ISD::SETO:
          return SHAVECC::O;

        case ISD::SETUO:
          return SHAVECC::UO;

        default:
          return SHAVECC::NONE;
        }
      }

      inline CondCode getOppositeCondition(CondCode CC) {
        switch (CC) {
        case AL :
            return SHAVECC::NONE;
        case NONE :
          return SHAVECC::AL;
        case EQ :
          return SHAVECC::NEQ;
        case GT :
          return SHAVECC::LTE;
        case GTE :
          return SHAVECC::LT;
        case LT :
          return SHAVECC::GTE;
        case LTE :
          return SHAVECC::GT;
        case NEQ :
          return SHAVECC::EQ;
        case O:
          return SHAVECC::UO;
        case UO:
          return SHAVECC::O;
        case OEQ:
        case OGT:
        case OGTE:
        case OLT:
        case OLTE:
        case UONEQ:
          llvm_unreachable("Cannot safely use opposite of ordered condition codes");
        }
        llvm_unreachable("Unknown condition code");
      }

      inline bool canReverse(CondCode CC) {
        switch(CC) {
        case OEQ:
        case OGT:
        case OGTE:
        case OLT:
        case OLTE:
        case UONEQ:
          // The result of a comparison with NAN will always be false, so we cannot safely
          // reverse these condition codes
          return false;
        default:
          return true;
        }
      }

      struct SHAVEReplacementCodes {
        ISD::CondCode A = ISD::CondCode::SETFALSE;
        ISD::CondCode B = ISD::CondCode::SETFALSE;
        unsigned opcode = ISD::DELETED_NODE; // ISD::OR or ISD::AND
        SHAVEReplacementCodes(ISD::CondCode A, ISD::CondCode B, unsigned opcode)
          : A(A), B(B), opcode(opcode) {
          assert(opcode == ISD::OR || opcode == ISD::AND);
        }
      };
      inline std::optional<SHAVEReplacementCodes> getReplacementCodes(ISD::CondCode CC, bool isFloatingPoint) {
        if (!isFloatingPoint) // Replacements are only valid for FP types
          return {};
        switch (CC) {
        default: return {}; // Operation supported
        case ISD::SETONE: return SHAVEReplacementCodes(ISD::SETO, ISD::SETUNE, ISD::AND); // ONE -> O && UNE
        case ISD::SETUEQ: return SHAVEReplacementCodes(ISD::SETUO, ISD::SETOEQ, ISD::OR); // UEQ -> UO || OEQ
        case ISD::SETUGT: return SHAVEReplacementCodes(ISD::SETUO, ISD::SETOGT, ISD::OR); // UGT -> UO || OGT
        case ISD::SETUGE: return SHAVEReplacementCodes(ISD::SETUO, ISD::SETOGE, ISD::OR); // UGE -> UO || OGE
        case ISD::SETULT: return SHAVEReplacementCodes(ISD::SETUO, ISD::SETOLT, ISD::OR); // ULT -> UO || OLT
        case ISD::SETULE: return SHAVEReplacementCodes(ISD::SETUO, ISD::SETOLE, ISD::OR); // ULE -> UO || OLE
        }
      }
    }   // End of anonymous namespace

    inline const char *SHAVECondCodeToString(CondCode CC, bool isPCCX = false) {
      switch (CC) {
      case OEQ :
      case EQ :
        return "EQ";
      case OGT :
      case GT :
        return "GT";
      case OGTE :
      case GTE :
        return "GTE";
      case OLT :
      case LT :
        return "LT";
      case OLTE :
      case LTE :
        return "LTE";
      case UONEQ :
      case NEQ :
        return "NEQ";
      case O :
        if (isPCCX)
          llvm_unreachable("Cannot produce PEU.PCCX with O mask");
        return "O";
      case UO :
        if (isPCCX)
          llvm_unreachable("Cannot produce PEU.PCCX with UO mask");
        return "UO";
      case NONE :
        return "NONE";
      default:
        llvm_unreachable("Unknown condition code");
      }
    }

    enum FloatConst {
      Nought_Point_Five,
      One,
      Two,
      Three,
      Root_Two,
      One_Over_Root_Two,
      Pi,
      E,
      Ten,
      One_Two_Eight,
      Two_Five_Five,

      Neg_Nought_Point_Five,
      Neg_One,
      Neg_Two,
      Neg_Three,
      Neg_Root_Two,
      Neg_One_Over_Root_Two,
      Neg_Pi,
      Neg_E,
      Neg_Ten,
      Neg_One_Two_Eight,
      Neg_Two_Five_Five,

      CONST_FLOAT_NUM
    };

    inline static uint32_t constFloatToInt(FloatConst float_const) {
      switch (float_const) {
      case Nought_Point_Five:     return 0x3F000000;
      case One:                   return 0x3F800000;
      case Two:                   return 0x40000000;
      case Three:                 return 0x40400000;
      case Root_Two:              return 0x3FB504F3;
      case One_Over_Root_Two:     return 0x3F3504F3;
      case Pi:                    return 0x40490FDB;
      case E:                     return 0x402DF854;
      case Ten:                   return 0x41200000;
      case One_Two_Eight:         return 0x43000000;
      case Two_Five_Five:         return 0x437F0000;
      case Neg_Nought_Point_Five: return 0xBF000000;
      case Neg_One:               return 0xBF800000;
      case Neg_Two:               return 0xC0000000;
      case Neg_Three:             return 0xC0400000;
      case Neg_Root_Two:          return 0xBFB504F3;
      case Neg_One_Over_Root_Two: return 0xBF3504F3;
      case Neg_Pi:                return 0xC0490FDB;
      case Neg_E:                 return 0xC02DF854;
      case Neg_Ten:               return 0xC1200000;
      case Neg_One_Two_Eight:     return 0xC3000000;
      case Neg_Two_Five_Five:     return 0xC37F0000;
      default: llvm_unreachable("Unknown floating point constant");
      }
    }

    inline static uint32_t constFloatToShort(FloatConst float_const) {
      switch (float_const) {
      case Nought_Point_Five:     return 0x3800;
      case One:                   return 0x3C00;
      case Two:                   return 0x4000;
      case Three:                 return 0x4200;
      case Root_Two:              return 0x3DA8;
      case One_Over_Root_Two:     return 0x39A8;
      case Pi:                    return 0x4248;
      case E:                     return 0x4170;
      case Ten:                   return 0x4900;
      case One_Two_Eight:         return 0x5800;
      case Two_Five_Five:         return 0x5BF8;
      case Neg_Nought_Point_Five: return 0xB800;
      case Neg_One:               return 0xBC00;
      case Neg_Two:               return 0xC000;
      case Neg_Three:             return 0xC200;
      case Neg_Root_Two:          return 0xBDA8;
      case Neg_One_Over_Root_Two: return 0xB9A8;
      case Neg_Pi:                return 0xC248;
      case Neg_E:                 return 0xC170;
      case Neg_Ten:               return 0xC900;
      case Neg_One_Two_Eight:     return 0xD800;
      case Neg_Two_Five_Five:     return 0xDBF8;
      default: llvm_unreachable("Unknown floating point constant");
      }
    }

    inline static FloatConst ConstantFPToConstFloat(const ConstantFP *f, unsigned int bits) {
      // FIXME: Movidius - This doesn't handle 64-bit Floating-Point
      if (bits == 32) {
        for (uint32_t i = 0; i < CONST_FLOAT_NUM; ++i) {
          FloatConst const_f(static_cast<FloatConst>(i));
          APInt asInt(32, constFloatToInt(const_f));

          if (f->isExactlyValue(APFloat(APFloat::IEEEsingle(), asInt )))
            return const_f;
        }
      } else {
        if (bits != 16)
          llvm_unreachable("Unknown floating point constant size");

        for (uint32_t i = 0; i < CONST_FLOAT_NUM; ++i) {
          FloatConst const_f(static_cast<FloatConst>(i));
          APInt asInt(16, constFloatToShort(const_f));

          if (f->isExactlyValue(APFloat(APFloat::IEEEhalf(), asInt)))
            return const_f;
        }
      }

      return CONST_FLOAT_NUM;
    }

    inline const char *SHAVEConstFloatToString(FloatConst CC) {
      switch (CC) {
      case Nought_Point_Five:     return "0.5";
      case One:                   return "1.0";
      case Two:                   return "2.0";
      case Three:                 return "3.0";
      case Root_Two:              return "SQT2";
      case One_Over_Root_Two:     return "RQT2";
      case Pi:                    return "PI";
      case E:                     return "E";
      case Ten:                   return "10.0";
      case One_Two_Eight:         return "128.0";
      case Two_Five_Five:         return "255.0";
      case Neg_Nought_Point_Five: return "-0.5";
      case Neg_One:               return "-1.0";
      case Neg_Two:               return "-2.0";
      case Neg_Three:             return "-3.0";
      case Neg_Root_Two:          return "-SQT2";
      case Neg_One_Over_Root_Two: return "-RQT2";
      case Neg_Pi:                return "-PI";
      case Neg_E:                 return "-E";
      case Neg_Ten:               return "-10.0";
      case Neg_One_Two_Eight:     return "-128.0";
      case Neg_Two_Five_Five:     return "-255.0";
      default:
          llvm_unreachable("Unknown floating point constant");
      }
    }
  } // end namespace SHAVECC
} // end namespace llvm


#endif // LLVM_TARGET_SHAVE_SHAVE_H
