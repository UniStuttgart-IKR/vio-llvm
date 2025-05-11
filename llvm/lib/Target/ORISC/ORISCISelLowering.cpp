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
#include "llvm/CodeGenTypes/MachineValueType.h"
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

  //setTargetDAGCombine({ISD::ADD, ISD::SUB});

  setOperationAction(ISD::Constant, MVT::i32, Legal);
  setOperationAction(ISD::Constant, MVT::i64, Expand);
  AddPromotedToType(ISD::Constant, MVT::orisc_pointer, MVT::i32);
  setOperationAction(ISD::Constant, MVT::orisc_pointer, Promote);
  AddPromotedToType(ISD::Constant, MVT::orisc_fatpointer, MVT::i32);
  setOperationAction(ISD::Constant, MVT::orisc_fatpointer, Promote);

  setOperationAction(ISD::ADD, {MVT::orisc_pointer, MVT::orisc_fatpointer}, Custom);
  setOperationAction(ISD::SUB, {MVT::orisc_pointer, MVT::orisc_fatpointer}, Custom);

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
  }
  setOperationAction(ISD::LOAD, MVT::i32, Custom);
  setOperationAction(ISD::LOAD, MVT::orisc_pointer, Custom);
  setOperationAction(ISD::LOAD, MVT::orisc_fatpointer, Custom);

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

SDValue ORISCTargetLowering::performAddSubCombine(SDNode *N,
                                                  DAGCombinerInfo &DCI) const {
  SDLoc DL(N);
  if (N->getValueType(0) == MVT::orisc_fatpointer) {
    if (N->getOpcode() == ISD::ADD)
      return DCI.DAG.getNode(ORISCISD::PTR_ADD, DL, MVT::orisc_fatpointer, N->getOperand(0), N->getOperand(1));
    
    return DCI.DAG.getNode(ORISCISD::PTR_SUB, DL, MVT::orisc_fatpointer, N->getOperand(0), N->getOperand(1));
  }
  return SDValue();
}

SDValue ORISCTargetLowering::PerformDAGCombine(SDNode *N,
                                                 DAGCombinerInfo &DCI) const {
                                                  return SDValue(N, 0);
  SDLoc DL(N);
  switch (N->getOpcode()) {
    case ISD::ADD:
    case ISD::SUB:
      return performAddSubCombine(N, DCI);

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
    case ORISCISD::INDEXED_LOAD:
      return "ORISCISD::INDEXED_LOAD";
    case ORISCISD::INDEXED_STORE:
      return "ORISCISD::INDEXED_STORE";
    case ORISCISD::PTR_ADD:
      return "ORISCISD::PTR_ADD";
    case ORISCISD::PTR_SUB:
      return "ORISCISD::PTR_SUB";
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
    case ISD::LOAD:
      return lowerLoad(Op, DAG);
    case ISD::ADD:
      return lowerAddSub(Op, DAG, true);
    case ISD::SUB:
      return lowerAddSub(Op, DAG, false);

    default: llvm_unreachable("Should not custom lower this!");
  }
}

//We only support unindexed Loads with a PTR_ADD/SUB as BaserPtr
SDValue ORISCTargetLowering::
lowerLoad(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  LoadSDNode *LoadOp = cast<LoadSDNode>(Op.getNode());
  
  //Load randomly happened to be unindexed with PTR_ADD/SUB as BasePtr
  if (LoadOp->isUnindexed() && LoadOp->getBasePtr()->getOpcode() == ORISCISD::PTR_ADD 
      && LoadOp->getBasePtr()->getValueType(0) == MVT::orisc_fatpointer)
    return Op;

  SDValue Chain = LoadOp->getChain();
  SDValue AddedPointer;
  SDValue Index;
  if (LoadOp->isUnindexed() && 
      (LoadOp->getBasePtr()->getOpcode() == ISD::ADD
        || LoadOp->getBasePtr()->getOpcode() == ISD::SUB)) {
    AddedPointer = LoadOp->getBasePtr();
  } else {
    if (LoadOp->isIndexed()) {
      Index = LoadOp->getOffset();
    } else {
      Index = DAG.getConstant(0, DL, MVT::i32);
    }
    AddedPointer = DAG.getNode(ORISCISD::PTR_ADD, DL, MVT::orisc_fatpointer, LoadOp->getBasePtr(), Index);
  }

  ISD::LoadExtType Ext = LoadOp->getExtensionType() == ISD::SEXTLOAD ?
                          ISD::SEXTLOAD : ISD::ZEXTLOAD;
  return DAG.getExtLoad(Ext, DL, LoadOp->getValueType(0), Chain, 
                        AddedPointer, LoadOp->getMemoryVT(), LoadOp->getMemOperand());
}

SDValue ORISCTargetLowering::
lowerAddSub(SDValue Op, SelectionDAG &DAG, bool IsAdd) const {
  SDLoc DL(Op);
  EVT N1Ty = Op.getOperand(0).getValueType();
  EVT N2Ty = Op.getOperand(1).getValueType();
  EVT VT =   Op->getValueType(0);
  //Conventional Integer Adds/Subs are legal!
  if (!(N1Ty == MVT::orisc_pointer || N1Ty == MVT::orisc_fatpointer
        || N2Ty == MVT::orisc_pointer || N2Ty == MVT::orisc_fatpointer
        || VT == MVT::orisc_pointer || VT == MVT::orisc_fatpointer))
    return Op;

  SDValue N1 = Op.getOperand(0);
  SDValue N2 = Op.getOperand(1);
  //Sometimes Constants dont get leaglized correctly to Int-Tys (why)
  //so we ensure it here
  if (ConstantSDNode *CN2 = dyn_cast<ConstantSDNode>(N2)) {
    N2 = DAG.getConstant(*CN2->getConstantIntValue(), DL, MVT::i32);
    N2Ty = MVT::i32;
  }

  assert((N1->getValueType(0) == MVT::orisc_pointer 
        || N1->getValueType(0) == MVT::orisc_fatpointer)
          && "Operand 0 of PTR_ADD/SUB must be Baser-Pointer");
  assert(N2->getValueType(0).isInteger()
          && "Operand 1 of PTR_ADD/SUB must be Integer-Typed");

  //Transform Chained Fat-Pointer Adds/Subs to
  //single Pointer Add/Sub and chained Int-Add/Subs
  unsigned IndxOpc;
  SDValue N3;
  while (N1->getValueType(0) == MVT::orisc_fatpointer) {
    IndxOpc = N1.getNode()->getOpcode() == ORISCISD::PTR_SUB ? 
              ISD::SUB : ISD::ADD;
    N3 = N1.getNode()->getOperand(1);
    if (ConstantSDNode *CN3 = dyn_cast<ConstantSDNode>(N3))
      N3 = DAG.getConstant(*CN3->getConstantIntValue(), DL, N2Ty);
    N2 = DAG.getNode(IndxOpc, DL, N2Ty, N2, N3);
    N1 = N1.getNode()->getOperand(0);
  }

  unsigned Opc = IsAdd ? ORISCISD::PTR_ADD : ORISCISD::PTR_SUB;
  return DAG.getNode(Opc, DL, MVT::orisc_fatpointer, N1, N2);
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