//===- IKRRISC2ISelLowering.cpp - IKRRISC2 DAG Lowering Implementation --------===//
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

#include "IKRRISC2ISelLowering.h"
#include "IKRRISC2InstrInfo.h"
#include "IKRRISC2RegisterInfo.h"
#include "IKRRISC2.h"
#include "IKRRISC2Subtarget.h"
#include "IKRRISC2TargetMachine.h"
#include "MCTargetDesc/IKRRISC2MCTargetDesc.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
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

#define DEBUG_TYPE "IKRRISC2-lower"

IKRRISC2TargetLowering::IKRRISC2TargetLowering(const TargetMachine &TM,
                                           const IKRRISC2Subtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  MVT PtrVT = MVT::i32;
  // Set up the register classes.
  addRegisterClass(MVT::i32, &IKRRISC2::GPRRegClass);

  // Set up special registers.
  setStackPointerRegisterToSaveRestore(IKRRISC2::R30);

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
  setOperationAction(ISD::SETCC, MVT::i32, Custom);

  setOperationAction(ISD::SCMP, MVT::i32, Legal);
  setOperationAction(ISD::UCMP, MVT::i32, Legal);

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
  setOperationAction(ISD::SHL, MVT::i32, Custom);
  setOperationAction(ISD::SRL, MVT::i32, Custom);
  setOperationAction(ISD::SRA, MVT::i32, Custom);
  setOperationAction(ISD::ROTL, MVT::i32, Custom);
  setOperationAction(ISD::ROTR, MVT::i32, Custom);

  setOperationAction(ISD::BSWAP, MVT::i32, Expand);
  setOperationAction(ISD::CTPOP, MVT::i32, Expand);
  setOperationAction(ISD::CTTZ, MVT::i32, Expand);
  setOperationAction(ISD::CTLZ, MVT::i32, Expand);

  // Implement custom stack allocations
  setOperationAction(ISD::DYNAMIC_STACKALLOC, PtrVT, Expand);
  // Implement custom stack save and restore
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);

  // VASTART, VAARG and VACOPY need to deal with the IKRRISC2-specific varargs
  // structure, but VAEND is a no-op.
  setOperationAction(ISD::VASTART, MVT::Other, Expand);
  setOperationAction(ISD::VAARG, MVT::Other, Expand);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());
}

SDValue IKRRISC2TargetLowering::performShiftLikeCombine(SDNode *N,
                                                 DAGCombinerInfo &DCI) const {

  return SDValue(N, 0); //is already hanled
  return SDValue();                 //needs standard handling
  //return Dag.getNode ...          //custom handling
}

SDValue IKRRISC2TargetLowering::PerformDAGCombine(SDNode *N,
                                                 DAGCombinerInfo &DCI) const {
                                                  return SDValue(N, 0);
  SDLoc DL(N);
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

bool IKRRISC2TargetLowering::isOffsetFoldingLegal(
    const GlobalAddressSDNode *GA) const {
  // The IKRRISC2 target isn't yet aware of offsets.
  return false;
}

const char *IKRRISC2TargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch ((IKRRISC2ISD::NodeType) Opcode) {
    case IKRRISC2ISD::RET:
      return "IKRRISC2ISD::RET";
    case IKRRISC2ISD::SWAPH:
      return "IKRRISC2ISD::SWAPH";
    case IKRRISC2ISD::SWAPB:
      return "IKRRISC2ISD::SWAPB";
    case IKRRISC2ISD::AND1I:
      return "IKRRISC2ISD::AND1I";
    case IKRRISC2ISD::NOT:
      return "IKRRISC2ISD::NOT";
    case IKRRISC2ISD::NEG:
      return "IKRRISC2ISD::NEG";
    case IKRRISC2ISD::SELECT:
      return "IKRRISC2ISD::SELECT";
    case IKRRISC2ISD::SHIFT_REG:
      return "IKRRISC2ISD::SHIFT_REG";
    default:
      return "Unnamed Node";
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Inline asm support
//===----------------------------------------------------------------------===//
TargetLowering::ConstraintType
IKRRISC2TargetLowering::getConstraintType(StringRef Constraint) const {
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
IKRRISC2TargetLowering::getSingleConstraintMatchWeight(
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
IKRRISC2TargetLowering::getRegForInlineAsmConstraint(
    const TargetRegisterInfo *TRI, StringRef Constraint, MVT VT) const {
  if (Constraint.size() == 1) {
    // GCC Constraint Letters
    switch (Constraint[0]) {
    default:
      break;
    case 'r': // General-purpose register
      return std::make_pair(0U, &IKRRISC2::GPRRegClass);
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

void IKRRISC2TargetLowering::LowerAsmOperandForConstraint(
    SDValue Op, StringRef Constraint, std::vector<SDValue> &Ops,
    SelectionDAG &DAG) const {
  SDLoc DL(Op);

  // Only support length 1 constraints for now.
  if (Constraint.size() > 1)
    return;

  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

SDValue IKRRISC2TargetLowering::
LowerOperation(SDValue Op, SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
    case ISD::SHL:
    case ISD::SRL:
    case ISD::SRA:
    case ISD::ROTL:
    case ISD::ROTR:
      return lowerShiftLikes(Op, DAG);

    case ISD::SELECT:
      return lowerSelect(Op, DAG);
    case ISD::SETCC:
      return lowerSetCC(Op, DAG);

    default: llvm_unreachable("Should not custom lower this!");
  }
}

SDValue IKRRISC2TargetLowering::
lowerShiftLikes(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);

  SDValue Value = Op.getOperand(0);
  SDValue Shamt = Op.getOperand(1);

  //Expand constant shifts to multiple single shifts
  ConstantSDNode *ConstShamt = dyn_cast<ConstantSDNode>(Shamt);
  if (ConstShamt) {
    //if shamt is already const 1, shift is already legal
    uint64_t shamt = ConstShamt->getZExtValue();
    if (ConstShamt->getZExtValue() == 1){
      return Op;
    }
    if (shamt >= 24 && Op->getOpcode() != ISD::ROTL && Op->getOpcode() != ISD::ROTR){
      SDValue Trunc = DAG.getNode(IKRRISC2ISD::AND1I, DL, Op.getValueType(), Op,
                                      DAG.getConstant(0, DL, MVT::i32));
      SDValue Swap = DAG.getNode(IKRRISC2ISD::SWAPH, DL, Op.getValueType(), Trunc);
      Swap = DAG.getNode(IKRRISC2ISD::SWAPB, DL, Op.getValueType(), Swap);
      SDNodeFlags Flags;
      if (Op->getOpcode() == ISD::SRA)
        Swap = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, Op.getValueType(), Swap,
                                DAG.getValueType(MVT::i8));
      return DAG.getNode(Op->getOpcode(), DL, Op.getValueType(), Swap,
                              DAG.getConstant(shamt - 24, DL, MVT::i32));
    }

    if (shamt >= 16){
      SDValue Swap = DAG.getNode(IKRRISC2ISD::SWAPH, DL, Op.getValueType(), Value);

      if (Op->getOpcode() == ISD::SHL)
        Swap = DAG.getNode(IKRRISC2ISD::AND1I, DL, Op.getValueType(), Swap,
                                DAG.getConstant(0xFF00, DL, MVT::i32));
      else if (Op->getOpcode() == ISD::SRL)
        Swap = DAG.getNode(ISD::AND, DL, Op.getValueType(), Swap,
                                DAG.getConstant((1 << 16)-1, DL, MVT::i32));
      else if (Op->getOpcode() == ISD::SRA)
        Swap = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, Op.getValueType(), Swap,
                                DAG.getValueType(MVT::i16));

      SDValue Result = DAG.getNode(Op->getOpcode(), DL, Op.getValueType(), Swap,
                                    DAG.getConstant(shamt - 16, DL, MVT::i32));
      return Result;
    }

  }
  SDValue ShiftKind = DAG.getConstant(Op->getOpcode(), DL, Op.getValueType());
  return DAG.getNode(IKRRISC2ISD::SHIFT_REG, DL, Op.getValueType(), Value, Shamt, ShiftKind);
}

SDValue IKRRISC2TargetLowering::
lowerSelect(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT Ty = Op.getOperand(0).getValueType();
  SDValue COND = Op.getOperand(0);
  SDValue TrueValue = Op.getOperand(1);
  SDValue FalseValue = Op.getOperand(2);

  SDValue Bitmap = DAG.getNode(IKRRISC2ISD::NEG, DL, Ty, COND);
  TrueValue = DAG.getNode(ISD::AND, DL, Ty, TrueValue, Bitmap);
  Bitmap = DAG.getNode(IKRRISC2ISD::NOT, DL, Ty, Bitmap);
  FalseValue = DAG.getNode(ISD::AND, DL, Ty, FalseValue, Bitmap);
  return DAG.getNode(ISD::OR, DL, Ty, TrueValue,FalseValue);
}

static inline bool includesEqualitySetCC(ISD::CondCode Code) {
  return Code == ISD::SETGE || Code == ISD::SETUGE || Code == ISD::SETLE || Code == ISD::SETULE;
}

SDValue IKRRISC2TargetLowering::
lowerSetCC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT Ty = Op.getOperand(0).getValueType();

  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op->getOperand(2))->get();
  SDValue Const1 = DAG.getConstant(1, DL, Ty);
  SDValue Res;

  if ((!isa<ConstantSDNode>(LHS) && !isa<ConstantSDNode>(RHS) && includesEqualitySetCC(CC))
    ||  isa<ConstantSDNode>(LHS)){
    CC = ISD::getSetCCInverse(CC, Ty);
    LHS = Op.getOperand(1);
    RHS = Op.getOperand(0);
  }

  switch(CC){
    //ANDI (CMPS L,R), 1
    case ISD::SETNE:
      Res =  DAG.getNode(ISD::UCMP, DL, Ty, LHS, RHS);
      return DAG.getNode(ISD::AND,  DL, Ty, Res, Const1);
    //ANDI (NOT (CMPS L,R)), 1
    case ISD::SETEQ:
      Res =  DAG.getNode(ISD::UCMP, DL, Ty, LHS, RHS);
      Res =  DAG.getNode(IKRRISC2ISD::NOT, DL, Ty, Res);
      return DAG.getNode(ISD::AND, DL, Ty, Res, Const1);
    //SRL (ADDI (CMPS L,R), 1), 1
    case ISD::SETGT:
      Res =  DAG.getNode(ISD::SCMP, DL, Ty, LHS, RHS);
      Res =  DAG.getNode(ISD::ADD,  DL, Ty, Res, Const1);
      return DAG.getNode(ISD::SRL,  DL, Ty, Res, Const1);
    //SRL (ADDI (CMPU L,R), 1), 1
    case ISD::SETUGT:
      Res =  DAG.getNode(ISD::UCMP, DL, Ty, LHS, RHS);
      Res =  DAG.getNode(ISD::ADD,  DL, Ty, Res, Const1);
      return DAG.getNode(ISD::SRL,  DL, Ty, Res, Const1);
    //AND (ROL (NOT (CMPS L,R))), 1
    case ISD::SETGE:
      Res =  DAG.getNode(ISD::SCMP, DL, Ty, LHS, RHS);
      Res =  DAG.getNode(IKRRISC2ISD::NOT, DL, Ty, Res);
      Res =  DAG.getNode(ISD::ROTL, DL, Ty, LHS, Const1);
      return DAG.getNode(ISD::AND,  DL, Ty, Res, Const1);
    //AND (ROL (NOT (CMPU L,R))), 1
    case ISD::SETUGE:
      Res =  DAG.getNode(ISD::SCMP, DL, Ty, LHS, RHS);
      Res =  DAG.getNode(IKRRISC2ISD::NOT, DL, Ty, Res);
      Res =  DAG.getNode(ISD::ROTL, DL, Ty, LHS, Const1);
      return DAG.getNode(ISD::AND,  DL, Ty, Res, Const1);
    //AND (ROL (CMPS L,R)), 1
    case ISD::SETLT:
      Res =  DAG.getNode(ISD::SCMP, DL, Ty, RHS, LHS);
      Res =  DAG.getNode(ISD::ROTL, DL, Ty, LHS, Const1);
      return DAG.getNode(ISD::AND,  DL, Ty, Res, Const1);
    //AND (ROL (CMPU L,R)), 1
    case ISD::SETULT:
      Res =  DAG.getNode(ISD::UCMP, DL, Ty, RHS, LHS);
      Res =  DAG.getNode(ISD::ROTL, DL, Ty, LHS, Const1);
      return DAG.getNode(ISD::AND,  DL, Ty, Res, Const1);
    //AND (SRL (NOT (ADD (CMPS L,R), 1))), 1
    case ISD::SETLE:
      Res =  DAG.getNode(ISD::SCMP, DL, Ty, RHS, LHS);
      Res =  DAG.getNode(ISD::ADD,  DL, Ty, Res, Const1);
      Res =  DAG.getNode(IKRRISC2ISD::NOT, DL, Ty, Res);
      Res =  DAG.getNode(ISD::SRL,  DL, Ty, Res, Const1);
      return DAG.getNode(ISD::AND,  DL, Ty, Res, Const1);
      //AND (SRL (NOT (ADD (CMPU L,R), 1))), 1
    case ISD::SETULE:
      Res =  DAG.getNode(ISD::UCMP, DL, Ty, RHS, LHS);
      Res =  DAG.getNode(ISD::ADD,  DL, Ty, Res, Const1);
      Res =  DAG.getNode(IKRRISC2ISD::NOT, DL, Ty, Res);
      Res =  DAG.getNode(ISD::SRL,  DL, Ty, Res, Const1);
      return DAG.getNode(ISD::AND,  DL, Ty, Res, Const1);
    default:
      return Op;
  }
  return Op;
}

//===----------------------------------------------------------------------===//
// Calling conventions
//===----------------------------------------------------------------------===//

#include "IKRRISC2GenCallingConv.inc"

SDValue IKRRISC2TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());

  CCInfo.AnalyzeFormalArguments(Ins, CC_IKRRISC2);

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];

    if (VA.isRegLoc()) {
      EVT RegVT = VA.getLocVT();
      Register Reg = MF.addLiveIn(VA.getLocReg(), &IKRRISC2::GPRRegClass);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, Reg, RegVT);
      InVals.push_back(ArgValue);
    }
  }

  return Chain;
}


SDValue
IKRRISC2TargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                               SmallVectorImpl<SDValue> &InVals) const {
  return CLI.Chain;
}

bool
IKRRISC2TargetLowering::CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                                        bool isVarArg,
                                        const SmallVectorImpl<ISD::OutputArg> &Outs,
                                        LLVMContext &Context, const Type *RetTy) const {
  return true;
}

SDValue
IKRRISC2TargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                                    const SmallVectorImpl<ISD::OutputArg> &Outs,
                                    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                                    SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();

  // Assign locations to each returned value.
  SmallVector<CCValAssign, 16> RetLocs;
  CCState RetCCInfo(CallConv, IsVarArg, MF, RetLocs, *DAG.getContext());
  RetCCInfo.AnalyzeReturn(Outs, RetCC_IKRRISC2);

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
  return DAG.getNode(IKRRISC2ISD::RET, DL, MVT::Other, RetOps);
}

bool
IKRRISC2TargetLowering::decomposeMulByConstant(LLVMContext &Context, EVT VT,
                                              SDValue C) const {
  return false;
}

//===----------------------------------------------------------------------===//
// Custom insertion
//===----------------------------------------------------------------------===//


//Because IKR RISC2 only has single bit shift/rotate instructions,
//we need to emit a loop to shift by a variable length
MachineBasicBlock *
IKRRISC2TargetLowering::emitShiftLikeLoop(MachineInstr &MI,
                                   MachineBasicBlock *MBB) const {
  const TargetInstrInfo &TII = *Subtarget.getInstrInfo();
  MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
  DebugLoc DL = MI.getDebugLoc();

  Register Result = MI.getOperand(0).getReg();
  Register Shamt = MRI.createVirtualRegister(&IKRRISC2::GPRRegClass);

  unsigned ShiftKind = 0;
  switch (MI.getOperand(3).getImm()) {
    case ISD::SHL:
      ShiftKind = IKRRISC2::LSL;
      break;
    case ISD::SRA:
      ShiftKind = IKRRISC2::ASR;
      break;
    case ISD::SRL:
      ShiftKind = IKRRISC2::LSR;
      break;
    case ISD::ROTL:
      ShiftKind = IKRRISC2::ROL;
      break;
    case ISD::ROTR:
      ShiftKind = IKRRISC2::ROR;
      break;
  }

  // The current Basic Block "MBB" is split in two parts where the
  // "ShiftReg"-Pseudo-Instruction was placed, everything bevor this
  // Instruction stays MBB, everything after becomes "SinkMBB".
  // The "ShiftMBB" is the loop-body, where the register is
  // successively shifted and the Shamt is decremented by 1 with
  // each iteration. "BranchMBB" then checks if Shamt reached zero or not.
  //
  //   MBB
  //   |
  //   |  ShiftMBB
  //   |    /  \
  //   BranchMBB
  //   |
  //   SinkMBB
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineFunction::iterator It = ++MBB->getIterator();

  MachineFunction *F = MBB->getParent();
  MachineBasicBlock *ShiftMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *BranchMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *SinkMBB = F->CreateMachineBasicBlock(LLVM_BB);

  F->insert(It, ShiftMBB);
  F->insert(It, BranchMBB);
  F->insert(It, SinkMBB);

  // Transfer the remainder of MBB and its successor edges to SinkMBB.
  SinkMBB->splice(SinkMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());
  SinkMBB->transferSuccessorsAndUpdatePHIs(MBB);

  MBB->addSuccessor(BranchMBB);
  BranchMBB->addSuccessor(SinkMBB);
  BranchMBB->addSuccessor(ShiftMBB);
  ShiftMBB->addSuccessor(BranchMBB);

  BuildMI(MBB, DL, TII.get(IKRRISC2::BRA))
      .addMBB(BranchMBB);


  Register ShiftResult = MRI.createVirtualRegister(&IKRRISC2::GPRRegClass);
  BuildMI(ShiftMBB, DL, TII.get(ShiftKind), ShiftResult)
      .addReg(Result);

  Register SubResult = MRI.createVirtualRegister(&IKRRISC2::GPRRegClass);
  BuildMI(ShiftMBB, DL, TII.get(IKRRISC2::ADDI), SubResult)
      .addReg(Shamt)
      .addImm(-1);


  //if we come from MBB:      we select the original Value to be shifted
  //if we come from ShiftMBB: we select the shifted Value
  BuildMI(*BranchMBB, BranchMBB->begin(), DL, TII.get(IKRRISC2::PHI),
          Result)
      .addReg(MI.getOperand(1).getReg())
      .addMBB(MBB)
      .addReg(ShiftResult)
      .addMBB(ShiftMBB);

  //if we come from MBB:      we select the original Shamt
  //if we come from ShiftMBB: we select the decremented Shamt
  BuildMI(BranchMBB, DL, TII.get(IKRRISC2::PHI),
          Shamt)
      .addReg(MI.getOperand(2).getReg())
      .addMBB(MBB)
      .addReg(SubResult)
      .addMBB(ShiftMBB);

  BuildMI(BranchMBB, DL, TII.get(IKRRISC2::BNE))
      .addReg(Shamt)
      .addMBB(ShiftMBB);

  //finally, erase the pseudo instruction
  MI.eraseFromParent();
  return SinkMBB;
}

MachineBasicBlock *
IKRRISC2TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                 MachineBasicBlock *BB) const {
  switch (MI.getOpcode()) {
    case IKRRISC2::SHIFT_REG:
      return emitShiftLikeLoop(MI, BB);
    default:
      LLVM_DEBUG(dbgs() << "\nOpcode " << MI.getOpcode());
      llvm_unreachable(" was flagged as custom insert, but not handeled in ISelLowering :(");
  }
}