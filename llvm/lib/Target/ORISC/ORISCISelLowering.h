//===- ORISCISelLowering.h - ORISC DAG Lowering Interface -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that ORISC uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ORISC_ORISCISELLOWERING_H
#define LLVM_LIB_TARGET_ORISC_ORISCISELLOWERING_H

#include "MCTargetDesc/ORISCMCTargetDesc.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGenTypes/MachineValueType.h"

namespace llvm {

namespace ORISCISD {
enum NodeType : unsigned  {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  ALLOCATE,
  LOAD_POINTER,
  STORE_POINTER,
  PTR_ADD,
  PTR_SUB,
  RET,
};
}

class ORISCSubtarget;

class ORISCTargetLowering : public TargetLowering {
public:
  explicit ORISCTargetLowering(const TargetMachine &TM,
                                const ORISCSubtarget &STI);

  EVT getSetCCResultType(const DataLayout &, LLVMContext &,
                         EVT VT) const override {
    if (!VT.isVector())
      return MVT::i32;
    return VT.changeVectorElementTypeToInteger();
  }
  
  MVT getPointerTy(const DataLayout &DL, uint32_t AS = 0) const override {
    return MVT::pointer;
  }

  MVT getPointerMemTy(const DataLayout &DL, uint32_t AS = 0) const override {
    return MVT::pointer;
  }

  bool isCtlzFast() const override {
    return true;
  }

  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  void ReplaceNodeResults(SDNode *, SmallVectorImpl<SDValue> &, SelectionDAG &) const override;

  bool isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const override;

  const char *getTargetNodeName(unsigned Opcode) const override;

  unsigned getNumRegisters(LLVMContext &Context, EVT VT,
                  std::optional<MVT> RegisterVT = std::nullopt) const override;

  MVT getRegisterTypeForCallingConv(LLVMContext &Context,
                                            CallingConv::ID CC, EVT VT) const override {
    if (VT == MVT::iPTR)
      return MVT::iPTR;
    if (VT == MVT::exnref)
      return MVT::exnref;
    return TargetLowering::getRegisterTypeForCallingConv(Context, CC, VT);
  }

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  TargetLowering::ConstraintType
  getConstraintType(StringRef Constraint) const override;

  TargetLowering::ConstraintWeight
  getSingleConstraintMatchWeight(AsmOperandInfo &Info,
                                 const char *Constraint) const override;

  void LowerAsmOperandForConstraint(SDValue Op, StringRef Constraint,
                                    std::vector<SDValue> &Ops,
                                    SelectionDAG &DAG) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool isVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerCall(CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool isVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context, const Type *RetTy) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;

  bool decomposeMulByConstant(LLVMContext &Context, EVT VT,
                              SDValue C) const override;

  const ORISCSubtarget &getSubtarget() const { return Subtarget; }

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *BB) const override;

  bool getTgtMemIntrinsic(IntrinsicInfo &Info, const CallInst &I,
                          MachineFunction &MF, unsigned Intrinsic) const override;

private:
  const ORISCSubtarget &Subtarget;

  struct FatPointer {
    SDValue Base;
    SDValue Index;
  };

  SDValue performAddSubCombine(SDNode *N, DAGCombinerInfo &DCI) const;
  SDValue lowerLoad(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerStore(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerAddSub(SDValue Op, SelectionDAG &DAG, bool IsAdd) const;
  SDValue lowerShiftLikes(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSelect(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSetCC(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerIntrinsicWChain(SDValue Op, SelectionDAG &DAG) const;
  SDValue separateBaseAndIndex(SDValue OldBase, SDValue OldIndex, EVT MemVT, SelectionDAG &DAG) const;
  SDValue lowerLoadStorePointer(uint64_t Type, SDValue Intrinsic, SelectionDAG &DAG) const;
  
  SDValue lowerFatPtrs(SDValue Op, SelectionDAG &DAG) const;
  FatPointer recursivelyLowerFatPtrs(SDValue Op, SelectionDAG &DAG) const;

  MachineBasicBlock *emitShiftLikeLoop(MachineInstr &MI, MachineBasicBlock *MBB) const;
};

} // end namespace llvm

#endif /* LLVM_LIB_TARGET_ORISC_ORISCISELLOWERING_H */
