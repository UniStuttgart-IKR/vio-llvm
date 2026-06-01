//===- IKRRISC2ISelLowering.h - IKRRISC2 DAG Lowering Interface -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that IKRRISC2 uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_IKRRISC2_IKRRISC2ISELLOWERING_H
#define LLVM_LIB_TARGET_IKRRISC2_IKRRISC2ISELLOWERING_H

#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

namespace IKRRISC2ISD {
enum NodeType : unsigned  {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  AND1I,
  NEG,
  NOT,
  RET,
  SELECT,
  SHIFT_REG,
  SWAPB,
  SWAPH,
};
}

class IKRRISC2Subtarget;

class IKRRISC2TargetLowering : public TargetLowering {
public:
  explicit IKRRISC2TargetLowering(const TargetMachine &TM,
                                const IKRRISC2Subtarget &STI);

  MVT getScalarShiftAmountTy(const DataLayout &, EVT LHSTy) const override {
    return MVT::i1;
  }

  EVT getSetCCResultType(const DataLayout &, LLVMContext &,
                         EVT VT) const override {
    if (!VT.isVector())
      return MVT::i32;
    return VT.changeVectorElementTypeToInteger();
  }

  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  bool isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const override;

  

  const char *getTargetNodeName(unsigned Opcode) const override;

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

  const IKRRISC2Subtarget &getSubtarget() const { return Subtarget; }

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *BB) const override;

private:
  const IKRRISC2Subtarget &Subtarget;

  SDValue performShiftLikeCombine(SDNode *N, DAGCombinerInfo &DCI) const;
  SDValue lowerShiftLikes(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSelect(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSetCC(SDValue Op, SelectionDAG &DAG) const;

  MachineBasicBlock *emitShiftLikeLoop(MachineInstr &MI, MachineBasicBlock *MBB) const;
};

} // end namespace llvm

#endif /* LLVM_LIB_TARGET_IKRRISC2_IKRRISC2ISELLOWERING_H */
