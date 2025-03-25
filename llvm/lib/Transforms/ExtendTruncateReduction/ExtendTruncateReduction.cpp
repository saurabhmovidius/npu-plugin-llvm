//===-- ExtendTruncateReduction.cpp - Extend Truncate Reduction Pass ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file contains the ExtendTruncateReduction optimisation pass
///
//===----------------------------------------------------------------------===//

#include <map>

#include "llvm/Transforms/ExtendTruncateReduction.h"
#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

/// \brief Generate the list of values that need to be modified in order to
/// remove the trunc instruction which generates startingValue.
///
/// \param startingValue typically the result of a truncate instruction which
/// we are trying to remove.
///
/// \returns a list of instructions that need to be modified in order to delete
/// the truncate instruction \p startingValue.
static std::vector<Value *> generateModList(Value * startingValue){
  // workList used while building modList
  std::vector<Value *> workList;
  workList.push_back(startingValue);
  // modList is the list of instructions that need to be modified
  std::vector<Value *> modList;

  while (!workList.empty()){
    // Pop the top element off the workList
    Value * currentValue = workList.front();
    workList.erase(workList.begin());
    // And add it to the modList
    modList.push_back(currentValue);

    // A constant has no operands so we do not need to process any further
    if(isa<Constant>(currentValue)) continue;

    // Add all operands of the current instruction to the workList,
    // unless this instruction is an extend
    Instruction * currentInstruction = dyn_cast<Instruction>(currentValue);
    if(!currentInstruction) continue;

    unsigned int opcode = currentInstruction->getOpcode();
    if(!opcode) continue;

    // Add all operands of the current instruction to the workList if they
    // have not already been added or processed (ignoring extends)
    if (opcode != Instruction::ZExt && opcode != Instruction::SExt){
      for (auto op : currentInstruction->operand_values()){
        // if this value is not in the workList or in the modList then add 
        // it to workList (if it's not a constant)
        if (std::find(workList.begin(), workList.end(), op) == workList.end()
            && std::find(modList.begin(), modList.end(), op) == modList.end()){
          workList.push_back(op);
        }
      }
    }

    // Add all instructions which use the current value to the workList,
    // unless this instruction is a trunc
    if (opcode != Instruction::Trunc){
      for (User * user : currentValue->users()){
        Value * userValue = cast<Value>(user);
        // if this value is not in the workList or in the modList then add 
        // it to workList
        if (std::find(workList.begin(), workList.end(), userValue) 
              == workList.end()
          && std::find(modList.begin(), modList.end(), userValue) 
               == modList.end()){
          workList.push_back(userValue);
        }
      }
    }
  }

  return modList;
}

/// \brief Check if the list of instructions \p modList can all be transformed
/// into the type \p targetType safely, in order to remove all extend and
/// truncate instructions in the list.
///
/// \param modList is the list of instructions to be checked.
/// \param targetType is the type we want to convert all operations in 
/// /p modList into.
///
/// \returns true if it is safe to perform the transformation, false otherwise.
static bool isTransformationSafe(std::vector<Value *> modList, 
                                 Type * targetType){
  for (auto value : modList){
    Instruction * instruction = dyn_cast<Instruction>(value);
    
    // All values in the list must be either a constant or an instruction
    if(!instruction && !isa<Constant>(value))
      return false;

    if(!instruction) continue;
      
    unsigned int opcode = instruction->getOpcode();

    // If there is an instruction in the list which has no uses then there
    // is an execution path through the list that does not conform to the
    // pattern extend->op->truncate, which we cannot safely optimise
    if (instruction->getNumUses() == 0){
      return false;
    }

    // Instructions must be a truncate, extend or binary operator instruction
    if (!isa<BinaryOperator>(instruction) && opcode != Instruction::Trunc 
        && opcode != Instruction::ZExt && opcode != Instruction::SExt){
      return false;
    }

    // It's unsafe to perform a div/mod operation on a mix of signed/unsigned
    // values (see comment at top of function) or if the div is not the first
    // instruction in the list (after the extend instructions)
    if (opcode == Instruction::SRem || opcode == Instruction::SDiv){
      Instruction * op0 = dyn_cast<Instruction>(instruction->getOperand(0));
      Instruction * op1 = dyn_cast<Instruction>(instruction->getOperand(1));

      if ((op0 && op0->getOpcode() != Instruction::SExt)
           || (op1 && op1->getOpcode() != Instruction::SExt))
        return false;
    }
    if(opcode == Instruction::URem || opcode == Instruction::UDiv){
      Instruction * op0 = dyn_cast<Instruction>(instruction->getOperand(0));
      Instruction * op1 = dyn_cast<Instruction>(instruction->getOperand(1));

      if ((op0 && op0->getOpcode() != Instruction::ZExt)
           || (op1 && op1->getOpcode() != Instruction::ZExt))
        return false;
    }

    // We can only safely transform a shift right operation if it is immediately
    // preceded by a shift left by the exact same (constant) value. This 
    // constant value must be the difference between 32 and the bit size of the
    // type being converted to. In these instances the shifts are being used to 
    // force a value into the range of the target type
    if (opcode == Instruction::AShr || opcode == Instruction::LShr){
      Instruction * shlInstruction = 
        dyn_cast<Instruction>(instruction->getOperand(0));

      if(!shlInstruction)
        return false;
      
      if (shlInstruction->getOpcode() != Instruction::Shl)
        return false;

      ConstantInt * constOperand0 = dyn_cast<ConstantInt>(
                                              shlInstruction->getOperand(1));
      ConstantInt * constOperand1 = dyn_cast<ConstantInt>(
                                              instruction->getOperand(1));

      if (!constOperand0 || !constOperand1)
        return false;

      if (constOperand0->getZExtValue() != constOperand1->getZExtValue())
        return false;

      if (constOperand0->getZExtValue() != 
           (32 - targetType->getIntegerBitWidth()))
        return false;
    }
  }
  return true;
}

/// \brief This function actually performs the extend-truncate-reduction
/// optimisation on the list of instructions \p modList. The function
/// isTransformation MUST be called before this one.
///
/// \param modList is the list of instructions to be transformed.
/// \param targetType is the type we want to convert all operations in 
/// /p modList into.
///
/// \returns true if the transformation was performed, false otherwise.
static bool performTransformation(std::vector<Value *> modList,
                                  Type * targetType){
  // valueMap is used to map the original values from the function to the values
  // that will replace them if the transformation is performed
  std::map<Value *, Value *> valueMap;
  // generatedValues is only used in the event that we need to abort and delete
  // all generated values (this should be rare if isTransformationSafe is called
  // before performTransformation)
  std::vector<Value *> generatedValues;
  // Make a copy of modList since we will be modifying it later
  std::vector<Value *> originalList = modList;

  // Initialise the value map
  for (auto currentValue : modList){
    valueMap[currentValue] = nullptr;
  }

  // Replace each value in modList one at a time. This loop only generates new
  // values and instructions but does not commit them (by deleting the old 
  // values) until later
  bool changeMade = true;
  while (!modList.empty() && changeMade){
    changeMade = false;
    for (auto it = modList.begin(); it != modList.end(); ++it){
      Value * currentValue = *it;
      
      // If currentValue is a constant then simply replace it with the same
      // constant of the target type
      if (ConstantInt * currentConstant = dyn_cast<ConstantInt>(currentValue)){
        int64_t constantValue = currentConstant->getSExtValue();
        Constant * newConstant;
        if (constantValue < 0){
          newConstant = ConstantInt::getSigned(targetType, constantValue);
        }
        else{
          newConstant = ConstantInt::get(targetType, constantValue);
        }
        valueMap[currentValue] = cast<Value>(newConstant);
        generatedValues.insert(generatedValues.begin(), 
                               cast<Value>(newConstant));
        modList.erase(it);
        changeMade = true;
        break;
      }

      Instruction * currentInstruction = cast<Instruction>(currentValue);

      // Handle extend instructions
      if (currentInstruction->getOpcode() == Instruction::SExt 
          || currentInstruction->getOpcode() == Instruction::ZExt){
        // If this chain of operations is being performed on integers of 
        // different sizes, we still need to extend the "smaller" values
        if (currentInstruction->getOperand(0)->getType()->getScalarSizeInBits()
             < targetType->getScalarSizeInBits()){
          CastInst * newExt;
          if (currentInstruction->getOpcode() == Instruction::SExt){
            newExt = CastInst::CreateSExtOrBitCast(
                                            currentInstruction->getOperand(0),
                                            targetType, "", currentInstruction);
          }
          else{
            newExt = CastInst::CreateZExtOrBitCast(
                                            currentInstruction->getOperand(0),
                                            targetType, "", currentInstruction);
          }
          generatedValues.insert(generatedValues.begin(), cast<Value>(newExt));
          valueMap[currentValue] = cast<Value>(newExt);
        }
        else{
          // If the current instruction is an extend then simply replace it with
          // the value it is extending
          valueMap[currentValue] = currentInstruction->getOperand(0);
        }
        modList.erase(it);
        changeMade = true;
        break;
      }

      bool transformPossible = true;
      for (auto op : currentInstruction->operand_values()){
        if (valueMap[op] == nullptr){
          transformPossible = false;
        }
      }

      // Build a new instruction for the currentInstruction only if all of
      // its operands have been transformed already
      if (!transformPossible) continue;

      // If the instruction is a truncate then we need to replace its uses with
      // the value being truncated
      if (currentInstruction->getOpcode() == Instruction::Trunc){
        Type * instructionType = currentInstruction->getType();
        // If this chain of operations is being performed on integers of 
        // different sizes, we may need to truncate "larger" values anyway
        if (instructionType->getScalarSizeInBits() 
                  < targetType->getScalarSizeInBits()){
          CastInst * newTrunc = CastInst::CreateTruncOrBitCast(
                                  valueMap[currentInstruction->getOperand(0)], 
                                  instructionType, "", currentInstruction);
          generatedValues.insert(generatedValues.begin(), 
                                 cast<Value>(newTrunc));
          valueMap[currentValue] = cast<Value>(newTrunc);
        }
        else{
          valueMap[currentValue] = valueMap[currentInstruction->getOperand(0)];
        }
        modList.erase(it);
        changeMade = true;
        break;
      }

      // If we reach here then this instruction must be a binary operator instr
      BinaryOperator * currentBinOp = cast<BinaryOperator>(currentInstruction);
      
      // Ensure that both operands are of the same type, promote either 
      // if necessary
      Value * operand0 = valueMap[currentInstruction->getOperand(0)];
      Value * operand1 = valueMap[currentInstruction->getOperand(1)];

      // sometimes clang will use shl and shr to force a value into the range of
      // an i8 or i16 while keeping the value in an i32. We don't need these 
      // since the operations are actually being performed as i8 or i16.
      if (currentBinOp->getOpcode() == Instruction::Shl){
        if (ConstantInt * constOperand1 = dyn_cast<ConstantInt>(operand1)){
          unsigned int shiftAmount = 32 - targetType->getIntegerBitWidth();
          if (constOperand1->getZExtValue() == shiftAmount 
              && currentBinOp->getNumUses() == 1){
            Value * useValue = *(currentBinOp->user_begin());
            Instruction * useInstruction = cast<Instruction>(useValue);
            if (useInstruction->getOpcode() == Instruction::AShr
                || useInstruction->getOpcode() == Instruction::LShr){
              ConstantInt * useConstOperand1 = 
                           dyn_cast<ConstantInt>(useInstruction->getOperand(1));
              if (useConstOperand1){
                if (useConstOperand1->getZExtValue() == shiftAmount){
                  valueMap[useValue] = operand0;
                  modList.erase(it);
                  modList.erase(std::find(modList.begin(), modList.end(), 
                                          useValue));
                  changeMade = true;
                  break;
                }
              }
            }
          }
        }
      }

      Instruction::BinaryOps opcode;
      // Clang may replace an arithmetic shift right of an i8 or i16 with a SExt
      // to i32 and a logical shift right. We need to undo this here
      if (currentBinOp->getOpcode() == Instruction::LShr){
        Instruction * instructionOperand0 = 
                       dyn_cast<Instruction>(currentInstruction->getOperand(0));
        if (instructionOperand0){
          bool foundExt = false;
          std::vector<Value *> workList;
          workList.push_back(instructionOperand0);
          // Find the extend instruction relevent to this shift
          while (!foundExt && !workList.empty()){
            Value * currentValue = workList.back();
            workList.pop_back();
            Instruction * currentInstruction = dyn_cast<Instruction>(
                                                                  currentValue);
            if(!currentInstruction) continue;
           
            // If SExt then we need to use an arithmetic shift right, else if
            // ZExt then we need to use a logical shift right
            if (currentInstruction->getOpcode() == Instruction::SExt){
              opcode = Instruction::AShr;
              foundExt = true;
            }
            else if (currentInstruction->getOpcode() == Instruction::ZExt){
              opcode = Instruction::LShr;
              foundExt = true;
            }
            else{
              for (auto op : currentInstruction->operand_values()){
                workList.push_back(op);
              }
            }
          }
        }
      }
      else{
        opcode = currentBinOp->getOpcode();
      }
      // Replace the current binary operator instruction with the corresponding
      // instruction of the new type
      BinaryOperator * newBinOp = BinaryOperator::Create(opcode, operand0, 
                                              operand1, Twine(), 
                                              cast<Instruction>(currentValue));
      valueMap[currentValue] = cast<Value>(newBinOp);
      generatedValues.insert(generatedValues.begin(), cast<Value>(newBinOp));
      modList.erase(it);
      changeMade = true;
      break;
    }
  }

  // If all instructions in modList have been successfully transformed
  // then commit these transformations to the Function by deleting the originals
  if (modList.empty()){
    bool stillDeleting = true;
    while (stillDeleting){
      stillDeleting = false;
      for (auto it = originalList.begin(); it != originalList.end(); ++it){
        Value * value = *it;

        // Only attempt to delete instructions (leaving all constants alone)
        Instruction * instruction = dyn_cast<Instruction>(value);
        if(!instruction) continue;

        // Replace all uses of a trunc with the value it was truncating
        if (instruction->getOpcode() == Instruction::Trunc){
          instruction->replaceAllUsesWith(valueMap[value]);
        }

        // Only attempt to delete this instruction if it has no more uses
        if (instruction->getNumUses() == 0){
          instruction->removeFromParent();
          value->deleteValue();
          originalList.erase(it);
          stillDeleting = true;
          break;
        }
        stillDeleting = true;
      }
    }
    return true;
  }
  else{
    // It is not possible to transform all the instructions in modList so we 
    // must delete all generated values
    for (auto value : generatedValues){
      // Only delete generated instructions and not constants
      if (isa<Instruction>(value))
        value->deleteValue();
    }
  }

  return false;
}

/// \brief This function returns the integer type with the largest bit width
/// contained in the list of instructions \p list which is larger than the type
/// \p startingType.
///
/// \param list the list of instructions to be searched.
/// \param startingType the initial integer type to compare all other types in
/// \p list to.
///
/// \returns the largest integer type in \p list that is larger than
/// \p startingType, otherwise it returns \p startingType.
static Type * getWidestTypeFromList(std::vector<Value *> list, 
                                    Type * startingType){
  Type * IType = startingType;
  for (auto value : list){
    Instruction * instruction = dyn_cast<Instruction>(value);
    if(!instruction) continue;

    unsigned int ITypeSize = IType->getScalarSizeInBits();
    unsigned int opcode = instruction->getOpcode();
    if (opcode == Instruction::Trunc 
        && value->getType()->getScalarSizeInBits() > ITypeSize){
      IType = value->getType();
    }
    else if (opcode == Instruction::SExt || opcode == Instruction::ZExt){
      unsigned int opSize = 
                   instruction->getOperand(0)->getType()->getScalarSizeInBits();
      if(opSize > ITypeSize)
        IType = value->getType();
    }
  }
  return IType;
}

PreservedAnalyses ExtendTruncateReduction::run(Function &F,
                                               FunctionAnalysisManager &AM) {
  bool changeMade = true;
  bool returnValue = false;

  while (changeMade) {
    changeMade = false;

    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (I.getOpcode() != Instruction::Trunc)
          continue;

        Type *IType = I.getType(), *OpType = I.getOperand(0)->getType();
        // We only handle extends to i32 from i8 or i16
        if (OpType->isIntegerTy(32) &&
            (IType->isIntegerTy(16) || IType->isIntegerTy(8))) {
          // Generate the list of instructions that need to be modified in
          // order to remove this trunc instruction
          std::vector<Value *> modList = generateModList(cast<Value>(&I));

          // When we have a mixture of i8, i16 and i32 instructions, we need
          // to extend all operands to the largest type
          IType = getWidestTypeFromList(modList, IType);

          bool changeSafe = isTransformationSafe(modList, IType);

          if (changeSafe && (IType->isIntegerTy(8) || IType->isIntegerTy(16)))
            changeMade = performTransformation(modList, IType);

          if (changeMade)
            returnValue = true;
        }
        if (changeMade)
          break;
      }
      if (changeMade)
        break;
    }
  }

  // FIXME: Maybe we can do better here.
  if (returnValue)
    return PreservedAnalyses::none();

  return PreservedAnalyses::all();
}
