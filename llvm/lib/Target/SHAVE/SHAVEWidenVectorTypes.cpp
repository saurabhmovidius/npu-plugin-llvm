//===-- SHAVEWidenVectorTypes.cpp - Vector Types Widening Pass -*- C++ -*--===//
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

#define DEBUG_TYPE "shave-widen-vector-types"

#include "llvm/Analysis/TargetFolder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

#include "MCTargetDesc/SHAVEOptions.h"

#include "SHAVEWidenVectorTypes.h"

using namespace llvm;

//
// Initialisation code required by the Pass Manager
//
char SHAVEWidenVectorTypes::ID = 0;

INITIALIZE_PASS(SHAVEWidenVectorTypes, "shavewidenvectortypespass", "SHAVE Vector Type Widening Pass", false, false)

FunctionPass *llvm::createSHAVEWidenVectorTypesPass() {
  return new SHAVEWidenVectorTypes();
}

//
// Workaround for bug #31615
// Transform vNi1 vector loads and stores into equivalent vNi8 loads and stores.
// During legalization, LLVM assumes that all vector element types are multiples of 8-bits in size for loads and stores, so we need to
// promote i1 vectors to equivalent i8 vectors here to prevent invalid code generation.
//
void SHAVEWidenVectorTypes::promoteBoolVectorLoad(Instruction * instruction) {
  IRBuilder<TargetFolder> builder(instruction->getParent()->getContext(), TargetFolder(instruction->getParent()->getModule()->getDataLayout()));
  builder.SetInsertPoint(instruction);

  LoadInst * load = cast<LoadInst>(instruction);
  Type * type = instruction->getType();

  // Cast the original vNi1 pointer to the equivalent vNi8 pointer in the same address space
  Value *op = load->getPointerOperand();
  assert(op != nullptr && "Expected a pointer operand");
  Type * newType = FixedVectorType::get(Type::getInt8Ty(type->getContext()), cast<FixedVectorType>(type)->getNumElements());
  Value * newPointer = builder.CreatePointerCast(op, PointerType::get(newType, load->getPointerAddressSpace()));
  // Generate the new load using the casted pointer
  LoadInst * newLoad = builder.CreateLoad(newType, newPointer);
  // Truncate the loaded value back to vNi1
  Value * newVec = builder.CreateTrunc(newLoad, type);

  DEBUG(dbgs() << "  Replacing "; load->dump());
  DEBUG(dbgs() << "  With "; newPointer->dump());
  DEBUG(dbgs() << "       "; newLoad->dump());
  DEBUG(dbgs() << "       "; newVec->dump());

  // Replace the original value with the new value and erase the original load from the parent basic block
  load->replaceAllUsesWith(newVec);
  load->eraseFromParent();
}

void SHAVEWidenVectorTypes::promoteBoolVectorStore(Instruction * instruction) {
  IRBuilder<TargetFolder> builder(instruction->getParent()->getContext(), TargetFolder(instruction->getParent()->getModule()->getDataLayout()));
  builder.SetInsertPoint(instruction);

  StoreInst * store = cast<StoreInst>(instruction);
  Value *valueOp = store->getValueOperand();
  assert(valueOp != nullptr && "Expected a value operand");
  Type * type = valueOp->getType();

  // Zero-extend the vNi1 vector to the equivalent vNi8 vector type
  Value * newValue = builder.CreateZExt(store->getValueOperand(), FixedVectorType::get(Type::getInt8Ty(type->getContext()), cast<FixedVectorType>(type)->getNumElements()));
  // Cast the original vNi1 pointer to the equivalent vNi8 pointer in the same address space
  Value * newPointer = builder.CreatePointerCast(store->getPointerOperand(), PointerType::get(FixedVectorType::get(Type::getInt8Ty(type->getContext()), cast<FixedVectorType>(type)->getNumElements()), store->getPointerAddressSpace()));
  // Generate the new store using the casted pointer and extended value
  LLVM_ATTRIBUTE_UNUSED StoreInst * newStore = builder.CreateStore(newValue, newPointer, store->isVolatile());

  DEBUG(dbgs() << "  Replacing "; store->dump());
  DEBUG(dbgs() << "  With "; newPointer->dump());
  DEBUG(dbgs() << "       "; newValue->dump());
  DEBUG(dbgs() << "       "; newStore->dump());

  // Finish by erasing the original store from the parent basic block
  store->eraseFromParent();
}

//
// Transform loads and stores of 3 element vectors into equivalent 4 element loads/stores for performance purposes.
// The 4th value in each transformed value is undefined.
// LLVM will take care of transforming all other operations
//
void SHAVEWidenVectorTypes::widenThreeElementVectorLoad(Instruction * instruction) {
  IRBuilder<TargetFolder> builder(instruction->getParent()->getContext(), TargetFolder(instruction->getParent()->getModule()->getDataLayout()));
  builder.SetInsertPoint(instruction);

  LoadInst * load = cast<LoadInst>(instruction);
  Type * type = instruction->getType();

  // Cast the original v3xx pointer to the equivalent v4xx pointer in the same address space
  Value *pointerOp = load->getPointerOperand();
  assert(pointerOp != nullptr && "Expected a pointer operand");
  Type * newType = FixedVectorType::get(cast<VectorType>(type)->getElementType(), 4);
  Value * newPointer = builder.CreatePointerCast(pointerOp, PointerType::get(newType, load->getPointerAddressSpace()));
  // Generate the new load using the casted pointer
  LoadInst * newLoad = builder.CreateLoad(newType, newPointer);
  // Generate a shuffle of the v4xx to produce a v3xx vector which will be used to replace all uses of the original value. Once all other v3xx operations
  // have been transformed, this will effectively become a no-op
  SmallVector<int, 3> mask = {0, 1, 2};
  Value * newVec3 = builder.CreateShuffleVector(newLoad, newLoad, mask);

  DEBUG(dbgs() << "  Replacing "; load->dump());
  DEBUG(dbgs() << "  With "; newPointer->dump());
  DEBUG(dbgs() << "       "; newLoad->dump());
  DEBUG(dbgs() << "       "; newVec3->dump());

  // Replace the original value with the new value and erase the original load from the parent basic block
  load->replaceAllUsesWith(newVec3);
  load->eraseFromParent();
}

void SHAVEWidenVectorTypes::widenThreeElementVectorStore(Instruction * instruction) {
  IRBuilder<TargetFolder> builder(instruction->getParent()->getContext(), TargetFolder(instruction->getParent()->getModule()->getDataLayout()));
  builder.SetInsertPoint(instruction);

  StoreInst * store = cast<StoreInst>(instruction);
  Value *valueOp = store->getValueOperand();
  assert(valueOp != nullptr && "Expected a value operand");
  Type * type = valueOp->getType();

  // Generate a shuffle instruction with the mask { 0, 1, 2, undef } to widen the incoming v3xx value to the equivalent v4xx type
  SmallVector<Constant *, 4> mask = { builder.getInt32(0), builder.getInt32(1), builder.getInt32(2), UndefValue::get(builder.getInt32Ty()) };
  Value * newValue = builder.CreateShuffleVector(store->getValueOperand(), store->getValueOperand(), ConstantVector::get(mask));
  // Cast the original v3xx pointer to the equivalent v4xx pointer in the same address space
  Value *pointerOp = store->getPointerOperand();
  assert(pointerOp != nullptr && "Expected a pointer operand");
  Value * newPointer = builder.CreatePointerCast(pointerOp, PointerType::get(FixedVectorType::get(cast<VectorType>(type)->getElementType(), 4), store->getPointerAddressSpace()));
  // Generate the new store using the casted pointer
  LLVM_ATTRIBUTE_UNUSED StoreInst * newStore = builder.CreateStore(newValue, newPointer, store->isVolatile());

  DEBUG(dbgs() << "  Replacing "; store->dump());
  DEBUG(dbgs() << "  With "; newPointer->dump());
  DEBUG(dbgs() << "       "; newValue->dump());
  DEBUG(dbgs() << "       "; newStore->dump());

  // Finish by erasing the original store from the parent basic block
  store->eraseFromParent();
}

//
// Definition of public member functions of SHAVEWidenVectorTypes
//
bool SHAVEWidenVectorTypes::runOnFunction(Function &F) {
  DEBUG(dbgs() << "SHAVEWidenVectorTypes: Starting vector type widening for machine function " << F.getName() << "\n");
  bool changed = false;
  bool enableLoadWidening = (SHAVEOptions::WidenThreeElementVector == SHAVEOptions::ThreeElementVectorWideningActions::LoadOnly 
                             || SHAVEOptions::WidenThreeElementVector == SHAVEOptions::ThreeElementVectorWideningActions::LoadAndStore);
  bool enableStoreWidening = (SHAVEOptions::WidenThreeElementVector == SHAVEOptions::ThreeElementVectorWideningActions::StoreOnly
                              || SHAVEOptions::WidenThreeElementVector == SHAVEOptions::ThreeElementVectorWideningActions::LoadAndStore);

  for (BasicBlock &block : F) {
    IRBuilder<TargetFolder> builder(block.getContext(), TargetFolder(block.getModule()->getDataLayout()));
    bool blockChanged;

    do {
      blockChanged = false;
      for (Instruction &instr : block) {
        
        if (instr.getOpcode() == Instruction::Load) {
          Type * type = instr.getType();

          // If this load instruction produces a vector value with 3 elements, then we transform it
          // into an equivalent vector load with 4 elements. The 4th vector element will be ignored
          if (type->isVectorTy() && cast<FixedVectorType>(type)->getNumElements() == 3 && enableLoadWidening) {
            widenThreeElementVectorLoad(&instr);
            changed = blockChanged = true;
            break;
          }

          // If this load instruction produces a vNi1 vector value, then promote it to an equivalent vNi8
          // vector load and truncate the result back to vNi1
          if (type->isVectorTy() && type->getScalarSizeInBits() == 1) {
            promoteBoolVectorLoad(&instr);
            changed = blockChanged = true;
            break;
          }
        }
        else if (instr.getOpcode() == Instruction::Store) {
          StoreInst * store = cast<StoreInst>(&instr);
	  Value *valueOp = store->getValueOperand();
	  assert(valueOp != nullptr && "Expected a value operand");
          Type * type = valueOp->getType();

          // If this store instruction uses a vector value with 3 elements, then we transform it
          // into an equivalent vector store with 4 elements. The 4th vector element will be undefined
          if (type->isVectorTy() && cast<FixedVectorType>(type)->getNumElements() == 3 && enableStoreWidening) {
            widenThreeElementVectorStore(&instr);
            changed = blockChanged = true;
            break;
          }

          // If this store instruction uses a vNi1 vector value, then zero-extend the incoming value to
          // vNi8 and store that value to the destination pointer
          if (type->isVectorTy() && type->getScalarSizeInBits() == 1) {
            promoteBoolVectorStore(&instr);
            changed = blockChanged = true;
            break;
          }
        }
      }
    } while (blockChanged);
  }

  DEBUG(dbgs() << "SHAVEWidenVectorTypes: Finished vector type widening for machine function " << F.getName() << "\n");

  return changed;
}
