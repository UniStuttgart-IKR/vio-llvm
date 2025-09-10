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
#include "llvm-c/Types.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGenTypes/MachineValueType.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IntrinsicsORISC.h"
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
  addRegisterClass(MVT::pointer, &ORISC::PRRegClass);

  // Set up special registers.
  setStackPointerRegisterToSaveRestore(ORISC::P30);

  setSchedulingPreference(Sched::RegPressure);

  setMinFunctionAlignment(Align(4));

  setTargetDAGCombine({ISD::LOAD, ISD::STORE});

  setOperationAction(ISD::Constant, MVT::i32, Legal);
  setOperationAction(ISD::Constant, MVT::i64, Expand);
  
  //setOperationAction(ISD::ADD, {MVT::orisc_pointer, MVT::orisc_fatpointer}, Custom);
  //setOperationAction(ISD::SUB, {MVT::orisc_pointer, MVT::orisc_fatpointer}, Custom);

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
  unsigned LoadList[] = {ISD::NON_EXTLOAD, ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD};
  for (MVT VT : MVT::integer_valuetypes()) {
    for (MVT OtherVT : MVT::integer_valuetypes()) {
      LegalizeAction Action = OtherVT == MVT::i1 ? Promote : Legal;
      setTruncStoreAction(VT, OtherVT, Action);
      setLoadExtAction(LoadList, VT, OtherVT, Action);
    }
  }
  setOperationAction(ISD::LOAD, MVT::i32, Legal);
  setOperationAction(ISD::LOAD, MVT::iPTR, Legal);
  setOperationAction(ISD::STORE, MVT::i32, Legal);
  setOperationAction(ISD::STORE, MVT::iPTR, Legal);

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

  setOperationAction(ISD::INTRINSIC_W_CHAIN, MVT::Other, Custom);
  setOperationAction(ISD::INTRINSIC_W_CHAIN, MVT::i32, Custom);
  setOperationAction(ISD::INTRINSIC_W_CHAIN, MVT::i64, Custom);
  setOperationAction(ISD::INTRINSIC_W_CHAIN, MVT::iPTR, Custom);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());
}

SDValue ORISCTargetLowering::performAddSubCombine(SDNode *N,
                                                  DAGCombinerInfo &DCI) const {
  return SDValue();
}

SDValue ORISCTargetLowering::PerformDAGCombine(SDNode *N,
                                                 DAGCombinerInfo &DCI) const {
  SDLoc DL(N);
  switch (N->getOpcode()) {

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
    case ORISCISD::LOAD_POINTER:
      return "ORISCISD::LOAD_POINTER";
    case ORISCISD::STORE_POINTER:
      return "ORISCISD::STORE_POINTER";
    case ORISCISD::PTR_ADD:
      return "ORISCISD::PTR_ADD";
    case ORISCISD::PTR_SUB:
      return "ORISCISD::PTR_SUB";
    default:
      return "Unnamed Node";
  }
  return nullptr;
}

unsigned ORISCTargetLowering::getNumRegisters(LLVMContext &Context, EVT VT,
                  std::optional<MVT> RegisterVT) const {
  if (VT == MVT::iPTR)
    return 1;
  if (VT == MVT::exnref)
    return 1;
  return TargetLoweringBase::getNumRegisters(Context, VT, RegisterVT);
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
    case ISD::STORE:
      return lowerStore(Op, DAG);
    case ISD::INTRINSIC_W_CHAIN:
      return lowerIntrinsicWChain(Op, DAG);

    default: llvm_unreachable("Should not custom lower this!");
  }
}

//We only support unindexed Loads with a PTR_ADD/SUB as BaserPtr
SDValue ORISCTargetLowering::
lowerStore(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  StoreSDNode *StoreOp = cast<StoreSDNode>(Op.getNode());

  LLVM_DEBUG(StoreOp->dump());
  if (StoreOp->getBasePtr()->getOpcode() == ORISCISD::PTR_ADD)
    return Op;

  SDValue OldIndex;
  if (StoreOp->isIndexed()){
    OldIndex = StoreOp->getOffset();
  } else {
    OldIndex = DAG.getConstant(0, DL, MVT::i32);
  }

  SDValue Chain = StoreOp->getChain();
  SDValue SeparatedPointer = separateBaseAndIndex(StoreOp->getBasePtr(), OldIndex, 
                                                    StoreOp->getMemoryVT(), DAG);

  return DAG.getTruncStore(Chain, DL, StoreOp->getValue(), SeparatedPointer, 
                                      StoreOp->getMemoryVT(), StoreOp->getMemOperand());
}

//We only support unindexed Loads with a PTR_ADD/SUB as BaserPtr
SDValue ORISCTargetLowering::
lowerLoad(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  LoadSDNode *LoadOp = cast<LoadSDNode>(Op.getNode());

  if (LoadOp->getBasePtr()->getOpcode() == ORISCISD::PTR_ADD)
    return Op;

  SDValue OldIndex;
  if (LoadOp->isIndexed()){
    OldIndex = LoadOp->getOffset();
  } else {
    OldIndex = DAG.getConstant(0, DL, MVT::i32);
  }
  SDValue Chain = LoadOp->getChain();
  SDValue SeparatedPointer = separateBaseAndIndex(LoadOp->getBasePtr(), OldIndex,
                                                    LoadOp->getMemoryVT(), DAG);

  ISD::LoadExtType Ext = LoadOp->getExtensionType() == ISD::SEXTLOAD ?
                          ISD::SEXTLOAD : ISD::ZEXTLOAD;
  return DAG.getExtLoad(Ext, DL, LoadOp->getValueType(0), Chain, 
                        SeparatedPointer, LoadOp->getMemoryVT(), LoadOp->getMemOperand());
}

SDValue ORISCTargetLowering::separateBaseAndIndex(SDValue OldBase, SDValue OldIndex, EVT MemVT, SelectionDAG &DAG) const{
  /*SDLoc DL(OldBase);
  EVT VT = OldBase.getValueType();
  SDValue NewBase, NewIndex;
  if (VT == MVT::orisc_pointer || VT == MVT::Other) {
    NewBase = OldBase;
    NewIndex = DAG.getConstant(0, DL, MVT::i32);
  } else if (VT == MVT::orisc_fatpointer){
    LLVM_DEBUG(dbgs() << "\nStarting FatPointer Recursion\n");
    auto FatPtr = recursivelyLowerFatPtrs(OldBase, DAG);
    NewBase = FatPtr.Base;
    NewIndex = FatPtr.Index;
  } else {
    llvm_unreachable("Bases MUST be pointer types!");
  }

  NewIndex = DAG.getNode(ISD::ADD, DL, NewIndex.getValueType(), NewIndex, OldIndex);
  uint64_t Shamt = Log2_64(MemVT.getSizeInBits() / 8);
  if (Shamt > 0) {
    SDValue ShamtNode = DAG.getConstant(Shamt, DL, MVT::i32);
    NewIndex = DAG.getNode(ISD::SRA, DL, NewIndex.getValueType(),
                          NewIndex, ShamtNode);
  }

  return DAG.getNode(ORISCISD::PTR_ADD, DL, MVT::orisc_fatpointer, NewBase, NewIndex);*/
}

//Traverse Tree upwards until we reach a Node where the Pointer
//is clean Base Pointer. Eiter N1 or N2 can be (Fat-)Ptr, but
//never both. 
ORISCTargetLowering::FatPointer ORISCTargetLowering::
recursivelyLowerFatPtrs(SDValue OldOp, SelectionDAG &DAG) const {
  /*SDLoc DL(OldOp);

  if (OldOp->getOpcode() == ISD::CopyFromReg){
    SDValue Index = DAG.getConstant(0, DL, MVT::i32);
    return {OldOp, Index};
  }
  if (OldOp->getOperand(0).getValueType() == MVT::orisc_pointer) {
    //We reached a clean instruction with separate base and index
    LLVM_DEBUG(dbgs() << "FOUND BASE:  "; OldOp->getOperand(0).dump());
    SDValue Index = OldOp->getOperand(1);
    if (ConstantSDNode *CIndex = dyn_cast<ConstantSDNode>(Index))
      Index = DAG.getConstant(*CIndex->getConstantIntValue(), DL, MVT::i32);
    return {OldOp->getOperand(0), Index};
  }

  ORISCTargetLowering::
    FatPointer CurFatPtr = recursivelyLowerFatPtrs(OldOp->getOperand(0), DAG);
  
  LLVM_DEBUG(dbgs() << "Looking at:  "; OldOp->dump());
  SDValue N1 = CurFatPtr.Index;
  SDValue N2 = OldOp->getOperand(0).getValueType() == MVT::orisc_fatpointer ?
                OldOp->getOperand(1) : OldOp->getOperand(0);
  //Sometimes Constants dont get leaglized correctly to Int-Tys (why?)
  //so we ensure it here
  if (ConstantSDNode *CN2 = dyn_cast<ConstantSDNode>(N2))
    N2 = DAG.getConstant(*CN2->getConstantIntValue(), DL, MVT::i32);

  SDValue NewIndex = DAG.getNode(OldOp->getOpcode(), DL, N1.getValueType(),
                                          N1, N2);
                                          
  LLVM_DEBUG(dbgs() << "Transforming to "; NewIndex->dump());
  return {CurFatPtr.Base, NewIndex};*/
}

SDValue ORISCTargetLowering::
  lowerLoadStorePointer(uint64_t Type, SDValue Intrinsic, SelectionDAG &DAG) const {
  SDLoc DL(Intrinsic);
  MemIntrinsicSDNode *MemIntrinsic = cast<MemIntrinsicSDNode>(Intrinsic.getNode());
  LLVM_DEBUG(MemIntrinsic->dump());
  SDValue ZeroIndex = DAG.getConstant(0, DL, MVT::i32);
  SDValue Chain = MemIntrinsic->getChain();
  SDValue Pointer = MemIntrinsic->readMem() ? MemIntrinsic->getOperand(2) : MemIntrinsic->getOperand(3);
  //SDValue SeparatedPointer = separateBaseAndIndex(Pointer, ZeroIndex,
  //                                                  MemIntrinsic->getMemoryVT(), DAG);

  auto *MMO = MemIntrinsic->getMemOperand();
  auto Flags = MMO->getFlags() | MachineMemOperand::MOVolatile;
  MMO->setFlags(Flags);
  
  if (MemIntrinsic->readMem()) {
    //return DAG.getNode(ORISCISD::LOAD_POINTER, DL, MVT::orisc_pointer, Chain, SeparatedPointer);
    return DAG.getLoad(MVT::iPTR, DL, Chain, Pointer, MMO);
  }

  //return DAG.getNode(ORISCISD::STORE_POINTER, DL, MVT::Other, Chain, SeparatedPointer);
  return DAG.getStore(Chain, DL, MemIntrinsic->getOperand(2), Pointer, MMO);
}


SDValue ORISCTargetLowering::
  lowerIntrinsicWChain(SDValue Op,
                        SelectionDAG &DAG) const {
  SDLoc DL(Op);
  uint64_t IntrinsicID = Op.getConstantOperandVal(1);
  switch (IntrinsicID) {
  default:
    return Op;
  case Intrinsic::orisc_storepointer:
  case Intrinsic::orisc_loadpointer:
    return lowerLoadStorePointer(IntrinsicID, Op, DAG);
  //case Intrinsic::orisc_allocate:
  //  return DAG.getNode(ORISCISD::ALLOCATE, DL, MVT::orisc_pointer, Op->getOperand(0), Op->getOperand(2), Op->getOperand(3));

  }
}


bool ORISCTargetLowering::getTgtMemIntrinsic(IntrinsicInfo &Info,
                                              const CallInst &I,
                                              MachineFunction &MF,
                                              unsigned Intrinsic) const {

  switch(Intrinsic) {
    default:
      return false;
    case Intrinsic::orisc_loadpointer:
      Info.opc = ISD::INTRINSIC_W_CHAIN;
      Info.ptrVal = I.getArgOperand(0);
      Info.offset = 0;
      Info.align = Align(1);
      Info.flags |= MachineMemOperand::MOLoad;
      Info.memVT = MVT::iPTR;
      return true;
    case Intrinsic::orisc_storepointer:
      Info.opc = ISD::INTRINSIC_W_CHAIN;
      Info.ptrVal = I.getArgOperand(1);
      Info.offset = 0;
      Info.align = Align(1);
      Info.flags |= MachineMemOperand::MOStore;
      Info.memVT = MVT::iPTR;
      return true;
  }
}

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
      if (RegVT == MVT::iPTR)
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