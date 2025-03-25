//===-- SHAVESelectionDAGInfo.cpp - Memory Functions ------------*- C++ -*-===//
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
// Provides intrinsic implementations for memcpy, memmove, memset
//
//===----------------------------------------------------------------------===//

#include "SHAVESelectionDAGInfo.h"
#include "SHAVESubtarget.h"
#include "SHAVETargetMachine.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;


SDValue SHAVESelectionDAGInfo::EmitTargetCodeForMemcpy(SelectionDAG &DAG, const SDLoc &dbgLoc,
                                                       SDValue Chain,
                                                       SDValue Dst, SDValue Src,
                                                       SDValue Size, Align Alignment, bool isVolatile,
                                                       bool AlwaysInline,
                                                       MachinePointerInfo DstPtrInfo,
                                                       MachinePointerInfo SrcPtrInfo) const {
  MachineMemOperand::Flags memLoadOpFlags = static_cast<MachineMemOperand::Flags>(isVolatile ? (MachineMemOperand::MOVolatile | MachineMemOperand::MOLoad) : MachineMemOperand::MOLoad);
  MachineMemOperand::Flags memStoreOpFlags = static_cast<MachineMemOperand::Flags>(isVolatile ? (MachineMemOperand::MOVolatile | MachineMemOperand::MOStore) : MachineMemOperand::MOStore);

  // Expand only if the size is a constant
  if (ConstantSDNode *sizeNode = dyn_cast<ConstantSDNode>(Size)) {
    uint64_t sizeVal = sizeNode->getZExtValue();

    const unsigned maxBytesToExpandCopy = 192u; // FIXME: Movidius - decide a right value for max size for inline memcpy
    const unsigned maxNumOfLoadStores = ((maxBytesToExpandCopy - 1u) / 8u) + 3u; // e.g. 192 is 23 x 8-byte L/S, plus 1 x 4-byte L/S, plus 1 x 2-byte L/S, plus 1 x 1-byte L/S; thus 26 L/S maximum

    // Expand only if the number of bytes to copy is suitable
    if (sizeVal <= maxBytesToExpandCopy) {
      // First copy using a VRF as the intermediate
      MVT tmpVT = MVT::v4i32;
      unsigned tmpVTSize = tmpVT.getSizeInBits() >> 3; // Get byte size of tmp storage
      unsigned numOfLoadStores = sizeVal / tmpVTSize;
      unsigned numOfResidualBytes = sizeVal % tmpVTSize;
      unsigned offset = 0;
      SDValue storeOps[maxNumOfLoadStores];
      SDValue tfNode1;

      if (numOfLoadStores) {
#ifndef NDEBUG
        const unsigned maxNumOf8ByteLoadStores = maxBytesToExpandCopy / 8u;
        assert(numOfLoadStores <= maxNumOf8ByteLoadStores);
#endif // NDEBUG

        for (unsigned i = 0; i < numOfLoadStores; ++i) {
          SDValue loadOp = DAG.getLoad(tmpVT, dbgLoc, Chain,
                                       DAG.getNode(ISD::ADD, dbgLoc, MVT::i32, Src,
                                                   DAG.getConstant(offset, dbgLoc, MVT::i32, false)),
                                       SrcPtrInfo.getWithOffset(offset), MaybeAlign(), memLoadOpFlags);

          storeOps[i] = DAG.getStore(Chain, dbgLoc, loadOp,
                                     DAG.getNode(ISD::ADD, dbgLoc, MVT::i32, Dst,
                                                 DAG.getConstant(offset, dbgLoc, MVT::i32, false)),
                                     DstPtrInfo.getWithOffset(offset), MaybeAlign(), memStoreOpFlags);
          offset += tmpVTSize;
        }
      }

      // Any remaining bytes are copied with 4, 2 and 1-byte L/S instructions via an IRF
      //   4-byte residual copy
      while (numOfResidualBytes >= 4u) {
        SDValue loadOp = DAG.getLoad(MVT::i32, dbgLoc, Chain,
                                     DAG.getNode(ISD::ADD, dbgLoc, MVT::i32, Src,
                                                 DAG.getConstant(offset, dbgLoc, MVT::i32, false)),
                                     SrcPtrInfo.getWithOffset(offset), MaybeAlign(), memLoadOpFlags);


        LoadSDNode *LD = cast<LoadSDNode>(loadOp);
        storeOps[numOfLoadStores++] = DAG.getStore(SDValue(LD, 1), dbgLoc, loadOp,
                                                   DAG.getNode(ISD::ADD, dbgLoc, MVT::i32, Dst,
                                                               DAG.getConstant(offset, dbgLoc, MVT::i32, false)),
                                                   DstPtrInfo.getWithOffset(offset), MaybeAlign(), memStoreOpFlags);
        offset += 4u;
        numOfResidualBytes -= 4u;
      }

      //   2-byte residual copy
      if ((numOfResidualBytes & 2u) == 2u) {
        SDValue loadOp = DAG.getLoad(MVT::i16, dbgLoc, Chain,
                                     DAG.getNode(ISD::ADD, dbgLoc, MVT::i32, Src,
                                                 DAG.getConstant(offset, dbgLoc, MVT::i32, false)),
                                     SrcPtrInfo.getWithOffset(offset), MaybeAlign(), memLoadOpFlags);

        LoadSDNode *LD = cast<LoadSDNode>(loadOp);
        storeOps[numOfLoadStores++] = DAG.getStore(SDValue(LD, 1), dbgLoc, loadOp,
                                                   DAG.getNode(ISD::ADD, dbgLoc, MVT::i32, Dst,
                                                               DAG.getConstant(offset, dbgLoc, MVT::i32, false)),
                                                   DstPtrInfo.getWithOffset(offset), MaybeAlign(), memStoreOpFlags);
        offset += 2u;
      }

      //   1-byte residual copy
      if ((numOfResidualBytes & 1u) == 1u) {
        SDValue loadOp = DAG.getLoad(MVT::i8, dbgLoc, Chain,
                                     DAG.getNode(ISD::ADD, dbgLoc, MVT::i32, Src,
                                                 DAG.getConstant(offset, dbgLoc, MVT::i32, false)),
                                     SrcPtrInfo.getWithOffset(offset), MaybeAlign(), memLoadOpFlags);
        LoadSDNode *LD = cast<LoadSDNode>(loadOp);
        storeOps[numOfLoadStores++] = DAG.getStore(SDValue(LD, 1), dbgLoc, loadOp,
                                                   DAG.getNode(ISD::ADD, dbgLoc, MVT::i32, Dst,
                                                               DAG.getConstant(offset, dbgLoc, MVT::i32, false)),
                                                   DstPtrInfo.getWithOffset(offset), MaybeAlign(), memStoreOpFlags);
      }

      // Create and return the copy chain
      return DAG.getNode(ISD::TokenFactor, dbgLoc, MVT::Other, ArrayRef(storeOps, numOfLoadStores));
    }
  }

  // Default to libcall
  return SDValue();
}

#if 0
// FIXME: Movidius - Incomplete implementation of Memset.
// This implementation is currently blocked by the SHAVE Asm Scheduler. 
// The pseudo instruction SHAVEISD::MEMSET_BLOCK expands to the instructions
//     BRU.RPI count
//         || LSU.STI32 value destination inc
// in SHAVEInstrInfo.cpp. These instructions are included in the instruction stream as
//     BRU.RPI
//     * LSU.STI32
// where instructions prefixed by '*' execute currently with the first instruction.
// This meaning of MI bundles is only correct at a late stage in the scheduler and
// not when first entering it. At this early stage these pseudo instructions mark
// the beginning and end of predicated instructions and as a result the scheduler
// expects the pipe to contain a PEU instruction. This will need to be fixed before
// memset can be fully implemented in its current form.
//
// There is another issue associated with these pipes in that they do not glue
// instructions together. In fact, the scheduler will discard the PIPE pseudo 
// instructions once it has dealt with all predicated instructions. Some other 
// way of gluing these instructions together will be necessary as at the moment 
// the scheduler will split up the BRU.RPI and LSU.STI32 instructions while 
// performing optimizations, essentially breaking memset.
//
// Because of these issues, I haven't been able to test the code below fully so there
// may be errors in the DAG being constructed.
//
// The following is the list of files that contain code relevent to this implementation
// of memset. You will need to remove the #if 0 #endif blocks from each of these files 
// to work on it.
//     SHAVEInstrInfo.h
//     SHAVELowering.h
//     SHAVESelectionDAGInfo.h
//     SHAVEInstrInfo.cpp
//     SHAVEISelDAGtoDAG.cpp
//     SHAVELowering.cpp
//     SHAVESelectionDAGInfo.cpp
//     SHAVEInstrInfo.td
//     
// Using LSU.STI32 isn't the most efficient way of handling the main body of memset.
// The best instruction I have found for the job is the 64 bit varient of STI, LSU.STI64.
// There currently isn't a description of this instruction in TableGen. It should be noted
// that LSU.STI64 reads from two consecutive registers so you will need to take this into
// account if carrying on with this implementation.
// 
//
SDValue SHAVESelectionDAGInfo::EmitTargetCodeForMemset(SelectionDAG &DAG, SDLoc dl,
                                                       SDValue Chain,
                                                       SDValue Dst, SDValue Src,
                                                       SDValue Size, unsigned Align,
                                                       bool isVolatile,
                                                       MachinePointerInfo DstPtrInfo) const 
{
  if(optLevel == CodeGenOptLevel::None)
  { // Optimization level is set to O0, just use the libcall
    return SDValue();
  }

  ConstantSDNode *ValC = dyn_cast<ConstantSDNode>(Src);
  SDValue source, destination, count, bytesLeft, alignment, condition, ifCondTrue, selByte;
  unsigned countAlignment, alignmentAlignment, destinationAlignment, bytesLeftAlignment;
  uint64_t Val = 0;

  source = Src;
  destination = Dst;
  count = Size;
  alignment = DAG.getConstant(Align, MVT::i32);

  // Get the alignment of each pointer for use by store instructions
  countAlignment = DAG.InferPtrAlignment(count);
  alignmentAlignment = DAG.InferPtrAlignment(alignment);
  destinationAlignment = DAG.InferPtrAlignment(destination);

  if(ValC)
  { // Value is a constant
    Val = ValC->getZExtValue() & 255;
    source = DAG.getConstant(Val, MVT::i8);
  }
  else
  { // Value is a variable, use SWZBYTE to copy value across
    selByte = DAG.getConstant(3, MVT::i32);
    source = DAG.getNode(SHAVEISD::SWIZZLE_BYTE, dl, MVT::i8, source, selByte, selByte, selByte, selByte);
  }

  // If destination alignment is odd
  //   then Store 1 byte of Val to destination
  //        Decrement count by 1
  //        Decrement alignment by 1
  //        Increment destination by 1
  ifCondTrue = DAG.getStore(Chain, dl, source, destination, MachinePointerInfo(), isVolatile, false, 1);
  ifCondTrue = DAG.getStore(ifCondTrue, dl, DAG.getNode(ISD::SUB, dl, MVT::i32, count, DAG.getConstant(1, MVT::i32)), 
                                                                count, MachinePointerInfo(), isVolatile, false, countAlignment);
  ifCondTrue = DAG.getStore(ifCondTrue, dl, DAG.getNode(ISD::SUB, dl, MVT::i32, alignment, DAG.getConstant(1, MVT::i32)),
                                                      alignment, MachinePointerInfo(), isVolatile, false, alignmentAlignment);
  ifCondTrue = DAG.getStore(ifCondTrue, dl, DAG.getNode(ISD::ADD, dl, MVT::i32, destination, DAG.getConstant(1, MVT::i32)),
                                                  destination, MachinePointerInfo(), isVolatile, false, destinationAlignment);

  condition = DAG.getNode(ISD::AND, dl, MVT::i32, alignment, DAG.getConstant(1, MVT::i32));
  ifCondTrue = DAG.getSelectCC(dl, condition, DAG.getConstant(1, MVT::i32), ifCondTrue, Chain, ISD::SETEQ);

  Chain = DAG.getSelectCC(dl, count, DAG.getConstant(1, MVT::i32), ifCondTrue, Chain, ISD::SETGE);

  // Set source to use 2 bytes of value
  if(ValC)
  {
    Val = (Val << 8) | Val;
    source = DAG.getConstant(Val, MVT::i16);
  }
  else
  {
    source = DAG.getNode(SHAVEISD::SWIZZLE_BYTE, dl, MVT::i16, source, selByte, selByte, selByte, selByte);
  }

  // If destination alignment is 2
  //   then Store 2 bytes of Val to destination
  //        Decrement count by 2
  //        Increment destination by 2
  ifCondTrue = DAG.getStore(Chain, dl, source, destination, MachinePointerInfo(), isVolatile, false, 2);
  ifCondTrue = DAG.getStore(ifCondTrue, dl, DAG.getNode(ISD::SUB, dl, MVT::i32, count, DAG.getConstant(2, MVT::i32)), 
                                                                count, MachinePointerInfo(), isVolatile, false, countAlignment);
  ifCondTrue = DAG.getStore(ifCondTrue, dl, DAG.getNode(ISD::ADD, dl, MVT::i32, destination, DAG.getConstant(2, MVT::i32)),
                                                  destination, MachinePointerInfo(), isVolatile, false, destinationAlignment);

  ifCondTrue = DAG.getSelectCC(dl, alignment, DAG.getConstant(2, MVT::i32), ifCondTrue, Chain, ISD::SETEQ);

  Chain = DAG.getSelectCC(dl, count, DAG.getConstant(2, MVT::i32), ifCondTrue, Chain, ISD::SETGE);

  // Set source to use all 4 bytes of value
  if(ValC)
  {
    Val = (Val << 16) | Val;
    source = DAG.getConstant(Val, MVT::i32);
  }
  else
  {
    source = DAG.getNode(SHAVEISD::SWIZZLE_BYTE, dl, MVT::i32, source, selByte, selByte, selByte, selByte);
  }

  // Emit the following instructions to handle the main write block
  //     BRU.RPI count
  //       || LSU0.STI32 source destination 4
  // SDValue destination2 = DAG.getNode(ISD::ADD, dl, MVT::i32, destination, DAG.getConstant(4, MVT::i32)); // destination2 = destination + 4
  bytesLeft = DAG.getNode(ISD::AND, dl, MVT::i32, count, DAG.getConstant(3, MVT::i32)); // bytesLeft = count % 4
  count = DAG.getNode(ISD::SRL, dl, MVT::i32, count, DAG.getConstant(2, MVT::i32)); // count = count / 4

  bytesLeftAlignment = DAG.InferPtrAlignment(bytesLeft);
  SDValue incAmount = DAG.getConstant(4, MVT::i32);

  SDValue mainBlock = DAG.getNode(SHAVEISD::MEMSET_BLOCK, dl, MVT::Other, destination, count, source, incAmount);
  Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other, mainBlock, Chain);

  destination = DAG.getNode(ISD::ADD, dl, MVT::i32, Dst, Size);
  destination = DAG.getNode(ISD::SUB, dl, MVT::i32, destination, bytesLeft);

  // If bytesLeft >= 4
  //   then Store 4 bytes of Val to destination
  //        Decrement bytesLeft by 4
  //        Increment destination by 4
  ifCondTrue = DAG.getStore(Chain, dl, source, destination, MachinePointerInfo(), isVolatile, false, 4);
  ifCondTrue = DAG.getStore(ifCondTrue, dl, DAG.getNode(ISD::SUB, dl, MVT::i32, bytesLeft, DAG.getConstant(4, MVT::i32)), 
                                                                bytesLeft, MachinePointerInfo(), isVolatile, false, bytesLeftAlignment);
  ifCondTrue = DAG.getStore(ifCondTrue, dl, DAG.getNode(ISD::ADD, dl, MVT::i32, destination, DAG.getConstant(4, MVT::i32)),
                                                  destination, MachinePointerInfo(), isVolatile, false, destinationAlignment);

  Chain = DAG.getSelectCC(dl, bytesLeft, DAG.getConstant(4, MVT::i32), ifCondTrue, Chain, ISD::SETGE);

  // Set source to use 2 bytes of value
  if(ValC)
  {
    Val = Val & 65535;
    source = DAG.getConstant(Val, MVT::i16);
  }
  else
  {
    source = DAG.getNode(SHAVEISD::SWIZZLE_BYTE, dl, MVT::i16, source, selByte, selByte, selByte, selByte);
  }

  // If bytesLeft >= 2
  //   then Store 2 bytes of Val to destination
  //        Increment destination by 2
  ifCondTrue = DAG.getStore(Chain, dl, source, destination, MachinePointerInfo(), isVolatile, false, 2);
  ifCondTrue = DAG.getStore(ifCondTrue, dl, DAG.getNode(ISD::ADD, dl, MVT::i32, destination, DAG.getConstant(2, MVT::i32)),
                                                  destination, MachinePointerInfo(), isVolatile, false, destinationAlignment);

  Chain = DAG.getSelectCC(dl, bytesLeft, DAG.getConstant(2, MVT::i32), ifCondTrue, Chain, ISD::SETGE);

  // Set source to use 1 byte of value
  if(ValC)
  {
    Val = Val & 255;
    source = DAG.getConstant(Val, MVT::i8);
  }
  else
  {
    source = DAG.getNode(SHAVEISD::SWIZZLE_BYTE, dl, MVT::i8, source, selByte, selByte, selByte, selByte);
  }

  // If bytesLeft == 1
  //   then Store 1 byte of Val to destination
  ifCondTrue = DAG.getStore(Chain, dl, source, destination, MachinePointerInfo(), isVolatile, false, 1);

  condition = DAG.getNode(ISD::AND, dl, MVT::i32, bytesLeft, DAG.getConstant(1, MVT::i32));
  Chain = DAG.getSelectCC(dl, condition, DAG.getConstant(1, MVT::i32), ifCondTrue, Chain, ISD::SETEQ);

  return Chain;
}
#endif
