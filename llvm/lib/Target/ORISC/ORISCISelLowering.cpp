//===- ORISCISelLowering.cpp - ORISC DAG Lowering Implementation --------===//
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

#include "ORISCISelLowering.h"
#include "ORISCInstrInfo.h"
#include "ORISCRegisterInfo.h"
#include "ORISC.h"
#include "ORISCSubtarget.h"
#include "ORISCTargetMachine.h"
#include "MCTargetDesc/ORISCMCTargetDesc.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <deque>
#include <map>
#include <utility>

using namespace llvm;

#define DEBUG_TYPE "ORISC-lower"

ORISCTargetLowering::ORISCTargetLowering(const TargetMachine &TM,
                                           const ORISCSubtarget &STI)
    : TargetLowering(TM), Subtarget(STI) {
  MVT PtrVT = MVT::i32;
  // Set up the register classes.
  
  addRegisterClass(MVT::i32, &ORISC::DRRegClass);
  addRegisterClass(MVT::orisc_pointer, &ORISC::PRRegClass);

  // Set up special registers.
  setStackPointerRegisterToSaveRestore(ORISC::P30);

  setSchedulingPreference(Sched::RegPressure);

  setMinFunctionAlignment(Align(4));

  setOperationAction(ISD::Constant, MVT::i32, Legal);
  setOperationAction(ISD::Constant, MVT::i64, Expand);

  setBooleanContents(ZeroOrOneBooleanContent);

  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  //setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand); legal
  //setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand); legal

  setOperationAction(ISD::BITCAST, MVT::i32, Expand);
  setOperationAction(ISD::BITCAST, MVT::f32, Expand);
  setOperationAction(ISD::UINT_TO_FP, MVT::i32, Expand);
  setOperationAction(ISD::SINT_TO_FP, MVT::i32, Expand);
  setOperationAction(ISD::FP_TO_UINT, MVT::i32, Expand);
  setOperationAction(ISD::FP_TO_SINT, MVT::i32, Expand);

  // No sign extend instructions for i1 and sign extend load i8
  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::EXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i8, Expand);

    setIndexedLoadAction({ISD::UNINDEXED, ISD::POST_INC, ISD::POST_DEC}, VT, Expand);
    setIndexedMaskedLoadAction(ISD::UNINDEXED, VT, Expand);
    setIndexedMaskedLoadAction(ISD::POST_INC, VT, Expand);
    setIndexedMaskedLoadAction(ISD::POST_DEC, VT, Expand);
    setIndexedLoadAction({ISD::PRE_INC, ISD::PRE_DEC}, VT, Legal);
    setIndexedMaskedLoadAction(ISD::PRE_INC, VT, Legal);
    setIndexedMaskedLoadAction(ISD::PRE_DEC, VT, Legal);

    setIndexedStoreAction({ISD::UNINDEXED, ISD::POST_INC, ISD::POST_DEC}, VT, Expand);
    setIndexedMaskedStoreAction(ISD::UNINDEXED, VT, Expand);
    setIndexedMaskedStoreAction(ISD::POST_INC, VT, Expand);
    setIndexedMaskedStoreAction(ISD::POST_DEC, VT, Expand);
    setIndexedStoreAction({ISD::PRE_INC, ISD::PRE_DEC}, VT, Legal);
    setIndexedMaskedStoreAction(ISD::PRE_INC, VT, Legal);
    setIndexedMaskedStoreAction(ISD::PRE_DEC, VT, Legal);
  }

  setOperationAction(ISD::ConstantPool, PtrVT, Expand);
  setOperationAction(ISD::GlobalAddress, PtrVT, Expand);
  setOperationAction(ISD::BlockAddress, PtrVT, Expand);
  setOperationAction(ISD::JumpTable, PtrVT, Expand);

  // Expand jump table branches as address arithmetic followed by an
  // indirect jump.
  setOperationAction(ISD::BR_JT, MVT::Other, Expand);

  setOperationAction(ISD::BR_CC, MVT::i32, Legal);
  setOperationAction(ISD::BR_CC, MVT::i64, Expand);

  setOperationAction(ISD::SELECT, MVT::i32, Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i32, Expand);
  setOperationAction(ISD::SETCC, MVT::i32, Legal);

  setOperationAction(ISD::MUL, MVT::i32, Expand);
  setOperationAction(ISD::MULHU, MVT::i32, Expand);
  setOperationAction(ISD::MULHS, MVT::i32, Expand);
  setOperationAction(ISD::SMUL_LOHI, MVT::i32, Expand);
  setOperationAction(ISD::UMUL_LOHI, MVT::i32, Expand);

  setOperationAction(ISD::SDIV, MVT::i32, Expand);
  setOperationAction(ISD::UDIV, MVT::i32, Expand);
  setOperationAction(ISD::SREM, MVT::i32, Expand);
  setOperationAction(ISD::UREM, MVT::i32, Expand);
  setOperationAction(ISD::SDIVREM, MVT::i32, Expand);
  setOperationAction(ISD::UDIVREM, MVT::i32, Expand);

  //setTargetDAGCombine({ISD::SHL, ISD::SRL, ISD::SRA, ISD::ROTL, ISD::ROTR});
  setOperationAction(ISD::SHL_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::SRL_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::ROTR, MVT::i32, Expand);

  setOperationAction(ISD::BSWAP, MVT::i32, Expand);
  setOperationAction(ISD::CTPOP, MVT::i32, Expand);
  setOperationAction(ISD::CTTZ, MVT::i32, Expand);
  setOperationAction(ISD::CTLZ, MVT::i32, Legal);
  setOperationAction(ISD::CTTZ_ZERO_UNDEF, MVT::i32, Expand);
  setOperationAction(ISD::CTLZ_ZERO_UNDEF, MVT::i32, Expand);

  // Implement custom stack allocations
  setOperationAction(ISD::DYNAMIC_STACKALLOC, PtrVT, Expand);
  // Implement custom stack save and restore
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);

  // VASTART, VAARG and VACOPY need to deal with the ORISC-specific varargs
  // structure, but VAEND is a no-op.
  setOperationAction(ISD::VASTART, MVT::Other, Expand);
  setOperationAction(ISD::VAARG, MVT::Other, Expand);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());
}

SDValue ORISCTargetLowering::performShiftLikeCombine(SDNode *N,
                                                 DAGCombinerInfo &DCI) const {

  return SDValue(N, 0); //is already hanled
  return SDValue();                 //needs standard handling
  //return Dag.getNode ...          //custom handling
}

SDValue ORISCTargetLowering::PerformDAGCombine(SDNode *N,
                                                 DAGCombinerInfo &DCI) const {
                                                  return SDValue(N, 0);
  SDLoc DL(N);
  cast<LoadSDNode>(N);
  switch (N->getOpcode()) {
    case ISD::SHL:
    case ISD::SRL:
    case ISD::SRA:
    case ISD::ROTL:
    case ISD::ROTR:
      return performShiftLikeCombine(N, DCI);

    default:
      return SDValue();
  }
}

bool ORISCTargetLowering::isOffsetFoldingLegal(
    const GlobalAddressSDNode *GA) const {
  // The ORISC target isn't yet aware of offsets.
  return false;
}

const char *ORISCTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch ((ORISCISD::NodeType) Opcode) {
    case ORISCISD::RET:
      return "ORISCISD::RET";
    case ORISCISD::SWAPH:
      return "ORISCISD::SWAPH";
    case ORISCISD::SWAPB:
      return "ORISCISD::SWAPB";
    case ORISCISD::AND1I:
      return "ORISCISD::AND1I";
    case ORISCISD::NOT:
      return "ORISCISD::NOT";
    case ORISCISD::NEG:
      return "ORISCISD::NEG";
    case ORISCISD::SELECT:
      return "ORISCISD::SELECT";
    case ORISCISD::SHIFT_REG:
      return "ORISCISD::SHIFT_REG";
    default:
      return "Unnamed Node";
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Inline asm support
//===----------------------------------------------------------------------===//
TargetLowering::ConstraintType
ORISCTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      return C_RegisterClass;
    default:
      break;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

TargetLowering::ConstraintWeight
ORISCTargetLowering::getSingleConstraintMatchWeight(
    AsmOperandInfo &Info, const char *Constraint) const {
  ConstraintWeight Weight = CW_Invalid;
  Value *CallOperandVal = Info.CallOperandVal;
  // If we don't have a value, we can't do a match,
  // but allow it at the lowest weight.
  if (!CallOperandVal)
    return CW_Default;

  Type *Ty = CallOperandVal->getType();

  // Look at the constraint type.
  switch (*Constraint) {
  default:
    Weight = TargetLowering::getSingleConstraintMatchWeight(Info, Constraint);
    break;
  case 'r':
    if (Ty->isIntegerTy())
      Weight = CW_Register;
    break;
  }
  return Weight;
}

std::pair<unsigned, const TargetRegisterClass *>
ORISCTargetLowering::getRegForInlineAsmConstraint(
    const TargetRegisterInfo *TRI, StringRef Constraint, MVT VT) const {
  if (Constraint.size() == 1) {
    // GCC Constraint Letters
    switch (Constraint[0]) {
    default:
      break;
    case 'r': // General-purpose register
      return std::make_pair(0U, &ORISC::DRRegClass);
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

void ORISCTargetLowering::LowerAsmOperandForConstraint(
    SDValue Op, StringRef Constraint, std::vector<SDValue> &Ops,
    SelectionDAG &DAG) const {
  SDLoc DL(Op);

  // Only support length 1 constraints for now.
  if (Constraint.size() > 1)
    return;

  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

SDValue ORISCTargetLowering::
LowerOperation(SDValue Op, SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
    //case ISD::SELECT:
    //  return lowerSelect(Op, DAG);

    default: llvm_unreachable("Should not custom lower this!");
  }
}

/*
SDValue ORISCTargetLowering::
lowerSelect(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT Ty = Op.getOperand(0).getValueType();
  SDValue COND = Op.getOperand(0);
  SDValue TrueValue = Op.getOperand(1);
  SDValue FalseValue = Op.getOperand(2);

  SDValue Bitmap = DAG.getNode(ORISCISD::NEG, DL, Ty, COND);
  TrueValue = DAG.getNode(ISD::AND, DL, Ty, TrueValue, Bitmap);
  Bitmap = DAG.getNode(ORISCISD::NOT, DL, Ty, Bitmap);
  FalseValue = DAG.getNode(ISD::AND, DL, Ty, FalseValue, Bitmap);
  return DAG.getNode(ISD::OR, DL, Ty, TrueValue,FalseValue);
}*/

static inline bool includesEqualitySetCC(ISD::CondCode Code) {
  return Code == ISD::SETGE || Code == ISD::SETUGE || Code == ISD::SETLE || Code == ISD::SETULE;
}

//===----------------------------------------------------------------------===//
// Calling conventions
//===----------------------------------------------------------------------===//

#include "ORISCGenCallingConv.inc"

SDValue ORISCTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());

  CCInfo.AnalyzeFormalArguments(Ins, CC_ORISC);

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];

    if (VA.isRegLoc()) {
      EVT RegVT = VA.getLocVT();
      Register Reg;
      if (RegVT == MVT::orisc_pointer)
        Reg = MF.addLiveIn(VA.getLocReg(), &ORISC::PRRegClass);
      else
        Reg = MF.addLiveIn(VA.getLocReg(), &ORISC::DRRegClass);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, Reg, RegVT);
      InVals.push_back(ArgValue);
    }
  }

  return Chain;
}


SDValue
ORISCTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                               SmallVectorImpl<SDValue> &InVals) const {
  return CLI.Chain;
}

bool
ORISCTargetLowering::CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                                        bool isVarArg,
                                        const SmallVectorImpl<ISD::OutputArg> &Outs,
                                        LLVMContext &Context, const Type *RetTy) const {
  return true;
}

SDValue
ORISCTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                                    const SmallVectorImpl<ISD::OutputArg> &Outs,
                                    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                                    SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();

  // Assign locations to each returned value.
  SmallVector<CCValAssign, 16> RetLocs;
  CCState RetCCInfo(CallConv, IsVarArg, MF, RetLocs, *DAG.getContext());
  RetCCInfo.AnalyzeReturn(Outs, RetCC_ORISC);

  SDValue Glue;
  // Copy the result values into the output registers.
  SmallVector<SDValue, 4> RetOps;
  RetOps.push_back(Chain);
  for (unsigned I = 0, E = RetLocs.size(); I != E; ++I) {
    CCValAssign &VA = RetLocs[I];
    SDValue RetValue = OutVals[I];

    // Make the return register live on exit.
    assert(VA.isRegLoc() && "Can only return in registers!");

    // Chain and glue the copies together.
    Register Reg = VA.getLocReg();
    Chain = DAG.getCopyToReg(Chain, DL, Reg, RetValue, Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(Reg, VA.getLocVT()));
  }

  // Update chain and glue.
  RetOps[0] = Chain;
  if (Glue.getNode())
    RetOps.push_back(Glue);

  // Quick exit for void returns
  return DAG.getNode(ORISCISD::RET, DL, MVT::Other, RetOps);
}

bool
ORISCTargetLowering::decomposeMulByConstant(LLVMContext &Context, EVT VT,
                                              SDValue C) const {
  return false;
}

//===----------------------------------------------------------------------===//
// Custom insertion
//===----------------------------------------------------------------------===//


MachineBasicBlock *
ORISCTargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                 MachineBasicBlock *BB) const {
  switch (MI.getOpcode()) {
    default:
      LLVM_DEBUG(dbgs() << "\nOpcode " << MI.getOpcode());
      llvm_unreachable(" was flagged as custom insert, but not handeled in ISelLowering :(");
  }
}