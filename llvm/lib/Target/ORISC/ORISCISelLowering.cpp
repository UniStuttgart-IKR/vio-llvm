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
#include "ORISCCallingConv.h"
#include "ORISCInstrInfo.h"
#include "ORISCRegisterInfo.h"
#include "ORISC.h"
#include "ORISCSubtarget.h"
#include "ORISCTargetMachine.h"
#include "MCTargetDesc/ORISCMCTargetDesc.h"
#include "llvm-c/Types.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
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
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "Helper/ORISCMangling.h"
#include <cassert>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <utility>

using namespace llvm;

#define DEBUG_TYPE "ORISC-lower"

ORISCTargetLowering::ORISCTargetLowering(const TargetMachine &TM,
                                           const ORISCSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  MVT PtrVT = MVT::i32;
  // Set up the register classes.
  
  addRegisterClass(MVT::i32, &ORISC::DRRegClass);
  addRegisterClass(MVT::pointer, &ORISC::PRRegClass);

  // Set up special registers.
  setStackPointerRegisterToSaveRestore(ORISC::P30);

  setSchedulingPreference(Sched::RegPressure);

  setMinFunctionAlignment(Align(4));

  setTargetDAGCombine({});

  setOperationAction(ISD::Constant, MVT::i32, Legal);
  setOperationAction(ISD::Constant, MVT::i64, Expand);

  setBooleanContents(ZeroOrOneBooleanContent);

  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  setOperationAction(ISD::TRUNCATE, MVT::i1, Legal);
  setOperationAction(ISD::TRUNCATE, MVT::i8, Legal);
  setOperationAction(ISD::TRUNCATE, MVT::i16, Legal);

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
      LegalizeAction Action = OtherVT == MVT::i1 ? Promote : Custom;
      setTruncStoreAction(VT, OtherVT, Action);
      setLoadExtAction(LoadList, VT, OtherVT, Action);
    }
  }
  setOperationAction(ISD::LOAD, MVT::iPTR, Custom);
  setOperationAction(ISD::LOAD, MVT::pointer, Custom);
  setOperationAction(ISD::STORE, MVT::iPTR, Custom);
  setOperationAction(ISD::STORE, MVT::pointer, Custom);

  setOperationAction(ISD::ConstantPool, MVT::pointer, Custom);
  setOperationAction(ISD::GlobalAddress, MVT::pointer, Custom);
  setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);
  setOperationAction(ISD::ExternalSymbol, MVT::pointer, Custom);
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

  setOperationAction(ISD::MUL, MVT::i32, Legal);
  setOperationAction(ISD::MULHU, MVT::i32, Legal);
  setOperationAction(ISD::MULHS, MVT::i32, Legal);
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

  setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::Other, Custom); //for box/unbox handling

  //setOperationAction(ISD::INTRINSIC_W_CHAIN, MVT::pointer, Custom);
  //setOperationAction(ISD::INTRINSIC_W_CHAIN, MVT::i32, Custom);
  setOperationAction(ISD::INTRINSIC_W_CHAIN, MVT::Other, Custom);

  setOperationAction(ISD::ADD, MVT::i32, Custom);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());
}

bool ORISCTargetLowering::isOffsetFoldingLegal(
    const GlobalAddressSDNode *GA) const {
  // The ORISC target isn't yet aware of offsets.
  return false;
}

const char *ORISCTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch ((ORISCISD::NodeType) Opcode) {
    case ORISCISD::BUILD_PTRARG:
      return "ORISCISD::BUILD_PTRARG";
    case ORISCISD::GET_CAPABILITY:
      return "ORISCISD::GET_CAPABILITY";
    case ORISCISD::GET_CONTEXT:
      return "ORISCISD::GET_CONTEXT";
    case ORISCISD::LIBRARY_CALL:
        return "ORISCISD::LIBRARY_CALL";
    case ORISCISD::LOCAL_CALL:
        return "ORISCISD::LOCAL_CALL";
    case ORISCISD::RET:
      return "ORISCISD::RET";
    default:
      return "Unnamed Node";
  }
  return nullptr;
}

unsigned ORISCTargetLowering::getNumRegisters(LLVMContext &Context, EVT VT,
                  std::optional<MVT> RegisterVT) const {
  if (VT == MVT::iPTR)
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

SDValue ORISCTargetLowering::PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const {
  return SDValue();
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
        case ISD::ADD:
            return lowerAdd(Op, DAG);
        case ISD::INTRINSIC_W_CHAIN:
            return lowerIntrinsicWChain(Op, DAG);
        case ISD::INTRINSIC_WO_CHAIN:
            return lowerIntrinsicWOChain(Op, DAG);
        case ISD::LOAD:
            return lowerLoad(Op, DAG);
        case ISD::STORE:
            return lowerStore(Op, DAG);
        case ISD::TRUNCATE:
            return lowerTruncate(Op, DAG);
        case ISD::ConstantPool:
        case ISD::GlobalAddress:
        case ISD::ExternalSymbol:
          return lowerGlobalAddress(Op, DAG);

        default: llvm_unreachable("Should not custom lower this!");
  }
}

SDValue ORISCTargetLowering::
  lowerAdd(SDValue Op, SelectionDAG &DAG) const {

    //ADD (GEP B, I), O -> GEP B, (ADD I, O)
    //ADD O, (GEP B, I) -> GEP B, (ADD I, O)
    bool LHSIsGep = Op->getOperand(0)->getOpcode() == ISD::INTRINSIC_WO_CHAIN
                    && (Op->getOperand(0).getConstantOperandVal(0) == Intrinsic::orisc_gep_p || Op->getOperand(0).getConstantOperandVal(0) == Intrinsic::orisc_gep_i);
    bool RHSIsGep = Op->getOperand(1)->getOpcode() == ISD::INTRINSIC_WO_CHAIN
                    && (Op->getOperand(1).getConstantOperandVal(0) == Intrinsic::orisc_gep_p || Op->getOperand(1).getConstantOperandVal(0) == Intrinsic::orisc_gep_i);

    if (LHSIsGep || RHSIsGep) {
      uint64_t IntrinsicID = Op->getOperand(LHSIsGep ? 0 : 1).getConstantOperandVal(0);
      SDLoc DL(Op);
      SDValue OldGep = LHSIsGep ? Op->getOperand(0) : Op->getOperand(1);
      SDValue GepID = DAG.getConstant(IntrinsicID, DL, MVT::i32);
      SDValue Base = OldGep->getOperand(1);
      SDValue Index = OldGep->getOperand(2);
      SDValue Offset = LHSIsGep ? Op->getOperand(1) : Op->getOperand(0);
      SDValue Add = DAG.getNode(ISD::ADD, DL, Op.getValueType(), Index, Offset);
      SDValue Gep = DAG.getNode(ISD::INTRINSIC_WO_CHAIN, DL,
                            Op.getValueType(), { GepID, Base, Add });
      return Gep;
    }
    return Op;
}

static inline SDValue getShiftedIndexGep(SDValue GEP, EVT MemVT, SelectionDAG &DAG){
    if (MemVT == MVT::i8)
        return GEP;

    uint64_t S;
    if (MemVT == MVT::i16)
        S = 1;
    else if (MemVT == MVT::i64)
        S = 3;
    else
        S = 2;
    
    SDLoc DL(GEP);
    SDValue GepID = DAG.getConstant(Intrinsic::orisc_gep_p, DL, MVT::i32);  // FIXME: Is this enough or do we need to consider orisc_gep_i?
    SDValue Base = GEP->getOperand(1);
    SDValue Index = GEP->getOperand(2);

    //Optimization: if there is a ADDI before ShiftRight
    //Shift Imm on Compile time and put Add after SRA
    //So Add can be combined with Load
    SDValue AddAfterShift;
    if (Index->getOpcode() == ISD::ADD) {
      bool LHSIsConst = isa<ConstantSDNode>(Index->getOperand(0));
      bool RHSIsConst = isa<ConstantSDNode>(Index->getOperand(1));
      if (LHSIsConst || RHSIsConst) {
        SDValue ConstValue = LHSIsConst ? Index->getOperand(0) : Index->getOperand(1);
        ConstantSDNode *Const = cast<ConstantSDNode>(ConstValue);
        int64_t NewImm = Const->getSExtValue() >> S;
        //See if NewImm Fits in Load/Store Displacement and ensure that we don't shift out some bits
        if (NewImm >= 0 && NewImm <= 15 && NewImm << S == Const->getSExtValue()){
          AddAfterShift = DAG.getConstant(Const->getSExtValue() >> S, DL, ConstValue.getValueType());
          Index = LHSIsConst ? Index->getOperand(1) : Index->getOperand(0);
        }
      }
    }

    SDValue Shamt = DAG.getShiftAmountConstant(S, MVT::i32, DL);
    Index = DAG.getNode(ISD::SRA, DL, Index.getValueType(), Index, Shamt);
    if (AddAfterShift)
      Index = DAG.getNode(ISD::ADD, DL, Index.getValueType(), Index, AddAfterShift);
    return DAG.getNode(ISD::INTRINSIC_WO_CHAIN, DL,
                            GEP.getValueType(), { GepID, Base, Index });
}

SDValue ORISCTargetLowering::
    lowerStore(SDValue Op, SelectionDAG &DAG) const {
    // If we are trying to store a BUILD_PTRARG, we are storing the
    // Pointer to the Box in Memory. But because we never really allocated
    // that box, we have to do it here.
    StoreSDNode *OrigStore = cast<StoreSDNode>(Op);
    if (OrigStore->getMemOperand()->getFlags() & MachineMemOperand::MOTargetFlag1)
        return Op;
    MachineMemOperand *MMO = OrigStore->getMemOperand();
    MMO->setFlags(MachineMemOperand::MOTargetFlag1);

    SDLoc DL(Op);
    SDValue GEP = OrigStore->getBasePtr();
    if (GEP->getOpcode() == ISD::ADD)
        GEP = lowerAdd(GEP, DAG);

    assert(GEP->getOpcode() == ISD::INTRINSIC_WO_CHAIN
            && (GEP.getConstantOperandVal(0) == Intrinsic::orisc_gep_p || GEP->getConstantOperandVal(0) == Intrinsic::orisc_gep_i)
                && "Pointer Addresses Have To Be GEPs!");
    
    EVT MemVT = OrigStore->getMemoryVT();
    GEP = getShiftedIndexGep(GEP, MemVT, DAG);

    if (OrigStore->getValue()->getOpcode() == ORISCISD::BUILD_PTRARG) {
        SDValue Base = OrigStore->getValue()->getOperand(0);
        SDValue Index = OrigStore->getValue()->getOperand(1);
        SDValue Chain = OrigStore->getChain();

        SDValue Box = lowerBoxIntrinsic(Chain, Chain, Base, Index, DAG);
        DAG.ReplaceAllUsesOfValueWith(OrigStore->getValue(), Box->getOperand(0));

        return DAG.getStore(Box->getOperand(1), DL, Box->getOperand(0), GEP, MMO);
    }

    if (MemVT == MVT::i32 || MemVT == MVT::pointer || MemVT == MVT::iPTR)
      return DAG.getStore(OrigStore->getChain(), DL, OrigStore->getValue(), GEP, MMO);

    return DAG.getTruncStore(OrigStore->getChain(), DL, OrigStore->getValue(), GEP, 
                            MemVT, MMO);
}

SDValue ORISCTargetLowering::
    lowerLoad(SDValue Op, SelectionDAG &DAG) const {
    LoadSDNode *OrigLoad = cast<LoadSDNode>(Op);
    if (OrigLoad->getMemOperand()->getFlags() & MachineMemOperand::MOTargetFlag1)
        return Op;
    MachineMemOperand *MMO = OrigLoad->getMemOperand();
    MMO->setFlags(MachineMemOperand::MOTargetFlag1);
    
    SDLoc DL(Op);
    SDValue GEP = OrigLoad->getBasePtr();
    if (GEP->getOpcode() == ISD::ADD)
        GEP = lowerAdd(GEP, DAG);

    assert(GEP->getOpcode() == ISD::INTRINSIC_WO_CHAIN
            && (GEP.getConstantOperandVal(0) == Intrinsic::orisc_gep_p || GEP->getConstantOperandVal(0) == Intrinsic::orisc_gep_i)
                && "Pointer Addresses Have To Be GEPs!");
    
    EVT MemVT = OrigLoad->getMemoryVT();
    GEP = getShiftedIndexGep(GEP, MemVT, DAG);

    if (MemVT == MVT::i32 || MemVT == MVT::pointer || MemVT == MVT::iPTR)
      return DAG.getLoad(MemVT, DL, OrigLoad->getChain(), GEP, MMO);

    return DAG.getExtLoad(ISD::LoadExtType::EXTLOAD, DL, MVT::i32, OrigLoad->getChain(), GEP, 
                          MMO->getPointerInfo(), MemVT, 
                          OrigLoad->getAlign(), MMO->getFlags());
}

SDValue ORISCTargetLowering::
  lowerTruncate(SDValue Op, SelectionDAG &DAG) const {
    SDValue N0 = Op->getOperand(0);
    EVT VT = Op->getValueType(0);
    if (ISD::isUNINDEXEDLoad(N0.getNode())) {
      auto *LN0 = cast<LoadSDNode>(N0);
      if (LN0->isSimple() && LN0->getMemoryVT().bitsLE(VT)) {
        SDValue NewLoad = DAG.getExtLoad(
            LN0->getExtensionType(), SDLoc(LN0), VT, LN0->getChain(),
            LN0->getBasePtr(), LN0->getMemoryVT(), LN0->getMemOperand());
        return NewLoad;
      }
    }
    return Op;
  }


SDValue ORISCTargetLowering::
  lowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const {
    SDLoc DL(Op);
    GlobalAddressSDNode *GA = cast<GlobalAddressSDNode>(Op);
    SDValue Addr = DAG.getTargetGlobalAddress(GA->getGlobal(), DL, Op.getValueType());
    return DAG.getNode(ORISCISD::LOAD_CONST, DL, MVT::pointer, Addr);
  }


SDValue ORISCTargetLowering::
    lowerIntrinsicWOChain(SDValue Op,
                            SelectionDAG &DAG) const {
    SDLoc DL(Op);
    uint64_t IntrinsicID = Op.getConstantOperandVal(0);
    switch (IntrinsicID) {
    default:
        return Op;

    case Intrinsic::orisc_gep_i:
    case Intrinsic::orisc_gep_p:
        if (isa<FrameIndexSDNode>(Op->getOperand(1)) 
                && isa<ConstantSDNode>(Op->getOperand(2)) && Op->getConstantOperandVal(2) == 0) {
            uint64_t FI = cast<FrameIndexSDNode>(Op->getOperand(1))->getIndex();
            return DAG.getFrameIndex(FI, MVT::i32);
        } else {
            return Op;
        }
    case Intrinsic::orisc_unbox_base:
        // LowerFormalArguments left a llvm.orisc.unbox.base( BUILD_PTRARG( CopyFromReg, CopyFromReg ) )
        // And we are only interested in the Result of the first CopyFromReg (the Base)
        if (Op->getOperand(1).getNode()->getOpcode() == ORISCISD::BUILD_PTRARG)
            return Op->getOperand(1)->getOperand(0);
        else
            return Op;
    case Intrinsic::orisc_unbox_index:
        // LowerFormalArguments left a llvm.orisc.unbox.index( BUILD_PTRARG( CopyFromReg, CopyFromReg ) )
        // And we are only interested in the Result of the second CopyFromReg (the Index)
        if (Op->getOperand(1).getNode()->getOpcode() == ORISCISD::BUILD_PTRARG)
            return Op->getOperand(1)->getOperand(1);
        else
            return Op;
    case Intrinsic::orisc_null:
      return DAG.getRegister(ORISC::P0, MVT::pointer);
    }
}

SDValue ORISCTargetLowering::
    lowerIntrinsicWChain(SDValue Op,
                            SelectionDAG &DAG) const {
    SDLoc DL(Op);
    uint64_t IntrinsicID = Op.getConstantOperandVal(1);
    switch (IntrinsicID) {
    default:
        return Op;
    case Intrinsic::orisc_box:
        return lowerBoxIntrinsic(Op->getOperand(0), SDValue(Op.getNode(), 1), Op->getOperand(2), Op->getOperand(3), DAG);
    }
}

SDValue ORISCTargetLowering::
    lowerBoxIntrinsic(SDValue ChainIn, SDValue ChainOut, SDValue Base, SDValue Index, SelectionDAG &DAG) const {
        SDLoc DL(ChainIn);
        //SDValue Dummy = DAG.getConstant(0, DL, MVT::i32);
        //return DAG.getNode(ORISCISD::BOX, DL, MVT::pointer, ChainIn, Base, Index);

        // If we encounter a "llvm.orisc.box" intrinsic, we split it into
        // allocate 1, 4 -> gep with index 0 -> store base-ptr to ptr slot -> store index to data slot
        SDValue DummyChain = DAG.getUNDEF(MVT::Other);
        SDValue Const0 = DAG.getConstant(0, DL, MVT::i32);
        SDValue Const1 = DAG.getConstant(1, DL, MVT::i32);
        SDValue Const4 = DAG.getConstant(4, DL, MVT::i32);
        SDValue AlcID = DAG.getConstant(Intrinsic::orisc_allocate, DL, MVT::i32);
        SDValue GepID = DAG.getConstant(Intrinsic::orisc_gep_i, DL, MVT::i32);
        
        SDValue AlcOps[] = { DummyChain, AlcID, Const1, Const4 };
        SDValue Alc = DAG.getNode(ISD::INTRINSIC_W_CHAIN, DL, 
                            { MVT::pointer, MVT::Other }, AlcOps);
        SDValue Chain = Alc.getValue(1);
        SDValue Gep = DAG.getNode(ISD::INTRINSIC_WO_CHAIN, DL, 
                            MVT::i32, { GepID, Alc, Const0 });
        Chain = DAG.getStore(Chain, DL, Base, Gep, MachinePointerInfo());
        if (isa<ConstantSDNode>(Index) && cast<ConstantSDNode>(Index)->getSExtValue() != 0)
            Chain = DAG.getStore(Chain, DL, Index, Gep, MachinePointerInfo());

        if (ChainIn != ChainOut)
            DAG.ReplaceAllUsesOfValueWith(ChainOut, Chain);
        AlcOps[0] = ChainIn;
        DAG.UpdateNodeOperands(Alc.getNode(), AlcOps);
        SDValue Res = DAG.getMergeValues({Alc.getValue(0), Chain}, DL);
        return Res;
    }


void ORISCTargetLowering::getTgtMemIntrinsic(SmallVectorImpl<IntrinsicInfo> &Infos,
                                  const CallBase &I, MachineFunction &MF,
                                  unsigned Intrinsic) const {

  switch(Intrinsic) {
    default:
      return;
  }
}

static inline bool includesEqualitySetCC(ISD::CondCode Code) {
  return Code == ISD::SETGE || Code == ISD::SETUGE || Code == ISD::SETLE || Code == ISD::SETULE;
}

//===----------------------------------------------------------------------===//
// Calling conventions
//===----------------------------------------------------------------------===//

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

    for (unsigned I = 0, ArgIdx = 0; I != ArgLocs.size(); ++I, ++ArgIdx) {
        CCValAssign &VA = ArgLocs[I];

        if (VA.isRegLoc()) {
            EVT RegVT = VA.getLocVT();
            SDValue ArgValue;
            if (RegVT == MVT::pointer) {
                Register Reg = MF.addLiveIn(VA.getLocReg(), &ORISC::PRRegClass);
                SDValue Base = DAG.getCopyFromReg(Chain, DL, Reg, RegVT);
                VA = ArgLocs[++I];
                Reg = MF.addLiveIn(VA.getLocReg(), &ORISC::DRRegClass);
                SDValue Index = DAG.getCopyFromReg(Chain, DL, Reg, VA.getLocVT());
                ArgValue = DAG.getNode(ORISCISD::BUILD_PTRARG, DL, MVT::pointer, Base, Index);
            } else {
                Register Reg = MF.addLiveIn(VA.getLocReg(), &ORISC::DRRegClass);
                ArgValue = DAG.getCopyFromReg(Chain, DL, Reg, RegVT);
            }
            InVals.push_back(ArgValue);
        }
    }

    return Chain;
}

static inline void getBaseIndex(SDValue Value, SDValue &Base, SDValue &Index, SelectionDAG &DAG) {
    if (Value->getOpcode() == ISD::INTRINSIC_W_CHAIN && 
        Value->getConstantOperandVal(1) == Intrinsic::orisc_box) {
        Base = Value->getOperand(2);
        Index = Value->getOperand(3);
    } else if (Value->getOpcode() == ISD::INTRINSIC_WO_CHAIN && 
                (Value->getConstantOperandVal(0) == Intrinsic::orisc_gep_p
                || Value->getConstantOperandVal(0) == Intrinsic::orisc_gep_i)) {
        Base = Value->getOperand(1);
        Index = Value->getOperand(2);
    } else if (Value->getOpcode() == ORISCISD::BUILD_PTRARG) {
        Base = Value->getOperand(0);
        Index = Value->getOperand(1);
    } else {
        Base = Value;
        Index = DAG.getConstant(0, SDLoc(Value), MVT::i32);
    }
}

SDValue
ORISCTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                               SmallVectorImpl<SDValue> &InVals) const {            
    SelectionDAG &DAG                     = CLI.DAG;
    SDLoc &DL                             = CLI.DL;
    SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
    SmallVectorImpl<SDValue> &OutVals     = CLI.OutVals;
    SmallVectorImpl<ISD::InputArg> &Ins   = CLI.Ins;
    SDValue Chain                         = CLI.Chain;
    SDValue Callee                        = CLI.Callee;
    bool &IsTailCall                      = CLI.IsTailCall;
    CallingConv::ID CallConv              = CLI.CallConv;
    bool IsVarArg                         = CLI.IsVarArg;
    bool IsPatchPoint                     = CLI.IsPatchPoint;
    const CallBase *CB                    = CLI.CB;

    MachineFunction &MF = DAG.getMachineFunction();

    if (IsTailCall) {
    }

    SmallVector<CCValAssign, 16> ArgLocs;
    CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                    *DAG.getContext());

    CCInfo.AnalyzeCallOperands(Outs, CC_ORISC);
    
    SDValue Zero = DAG.getConstant(0, DL, MVT::i32);
    //Chain = DAG.getNode(ISD::CALLSEQ_START, DL, {MVT::Other, MVT::Glue}, {Chain, Zero, Zero});

    SmallVector<std::pair<unsigned, SDValue>, 8> RegsToPass;
    SmallVector<SDValue, 8> MemOpChains;

    // Walk the register/memloc assignments, inserting copies/loads.
    // I - Tracks the index into the list of registers allocated for the call
    // ArgIdx - Tracks the index into the list of actual function arguments
    // J - Tracks the index into the list of byval arguments
    for (unsigned I = 0, ArgIdx = 0, J = 0, E = ArgLocs.size(); I != E; ++I, ++ArgIdx) {
        CCValAssign &VA = ArgLocs[I];
        SDValue Arg = OutVals[ArgIdx];
        //ISD::ArgFlagsTy Flags = Outs[ArgIdx].Flags;

        if (VA.isRegLoc()) {
            // Put argument in a physical register.
            if (Arg.getValueType() == MVT::pointer) {
                SDValue Base, Index;
                getBaseIndex(Arg, Base, Index, DAG);
                RegsToPass.push_back(std::make_pair(VA.getLocReg(), Base));
                RegsToPass.push_back(std::make_pair(ArgLocs[++I].getLocReg(),
                                        Index));
            } else {
                RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
            }
        } else {
            // Mem Loc
        }
    }

    if (!MemOpChains.empty())
        Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

    // Build a sequence of copy-to-reg nodes chained together with token chain
    // and flag operands which copy the outgoing args into the appropriate regs.
    SDValue InGlue;
    for (unsigned I = 0, E = RegsToPass.size(); I != E; ++I) {
        Chain = DAG.getCopyToReg(Chain, DL, RegsToPass[I].first,
                                    RegsToPass[I].second, InGlue);
        InGlue = Chain.getValue(1);
    }

    SDValue GetCap, GetCtxt;
    if (GlobalAddressSDNode *S = dyn_cast<GlobalAddressSDNode>(Callee)) {
        const GlobalValue *GV = S->getGlobal();
        if (GV->isDeclaration()) {
          Callee = DAG.getTargetExternalSymbol(GV->getName().begin(), MVT::i32, 0);
          SmallVector<StringRef, 4> Names;
          if(parseNestedNames(GV->getName().begin(), Names)){
            Names.pop_back();
            if (!Names.empty()) {
              std::string *ClassName = new std::string();
              ClassName->append("_ZN");
              for (StringRef N : Names) {
                ClassName->append(std::to_string(N.size()));
                ClassName->append(N.str());
              }
              ClassName->append("E");
              if (!GetCtxt) {
                SDVTList VTs = DAG.getVTList(MVT::pointer, MVT::Other);
                GetCtxt = DAG.getNode(ORISCISD::GET_CONTEXT, DL, VTs, DAG.getEntryNode());
              }
              GetCap = DAG.getTargetExternalSymbol(ClassName->c_str(), MVT::i32, 0);
              GetCap = DAG.getNode(ORISCISD::GET_CAPABILITY, DL, MVT::pointer, GetCtxt.getValue(1), GetCap);
            }
          }
        } else {
          Callee = DAG.getTargetGlobalAddress(GV, DL, MVT::i32, 0, 0);
        }
    } else if (ExternalSymbolSDNode *S = dyn_cast<ExternalSymbolSDNode>(Callee)) {
        Callee = DAG.getTargetExternalSymbol(S->getSymbol(), MVT::pointer, 0);
    }
    
    // The first call operand is the chain and the second is the target address.
    SmallVector<SDValue, 8> Ops;
    Ops.push_back(Chain);
    Ops.push_back(Callee);
    if (GetCap.getNode())
      Ops.push_back(GetCap);

    // Add argument registers to the end of the list so that they are
    // known live into the call.
    for (auto &Reg : RegsToPass)
        Ops.push_back(DAG.getRegister(Reg.first, Reg.second.getValueType()));

    // Add a register mask operand representing the call-preserved registers.
    const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
    const uint32_t *Mask = TRI->getCallPreservedMask(MF, CallConv);
    assert(Mask && "Missing call preserved mask for calling convention");
    Ops.push_back(DAG.getRegisterMask(Mask));

    // Glue the call to the argument copies, if any.
    if (InGlue.getNode())
        Ops.push_back(InGlue);

    SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
    if (IsTailCall) {
        MF.getFrameInfo().setHasTailCall();
        SDValue Ret = DAG.getNode(ORISCISD::TAIL_CALL, DL, NodeTys, Ops);
        if (CLI.CFIType)
            Ret.getNode()->setCFIType(CLI.CFIType->getZExtValue());
        DAG.addNoMergeSiteInfo(Ret.getNode(), CLI.NoMerge);
        return Ret;
    }
    
    if (GetCap.getNode())
      Chain = DAG.getNode(ORISCISD::LIBRARY_CALL, DL, NodeTys, Ops);
    else
      Chain = DAG.getNode(ORISCISD::LOCAL_CALL, DL, NodeTys, Ops);
    if (CLI.CFIType)
        Chain.getNode()->setCFIType(CLI.CFIType->getZExtValue());
    DAG.addNoMergeSiteInfo(Chain.getNode(), CLI.NoMerge);
    InGlue = Chain.getValue(1);

    // Mark the end of the call, which is glued to the call itself.
    //Chain = DAG.getCALLSEQ_END(Chain, Zero, Zero, InGlue, DL);
    SDValue OutGlue = Chain.getValue(1);
    SmallVector<CCValAssign, 16> RetValLocs;
    CCState CCRetInfo(CallConv, IsVarArg, MF, RetValLocs, *DAG.getContext());
    CCRetInfo.AnalyzeCallResult(Ins, RetCC_ORISC);

    for (unsigned I = 0, E = RetValLocs.size(); I != E; ++I) {
        auto &VA = RetValLocs[I];
        // Copy the value out
        SDValue RetValue =
            DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getLocVT(), OutGlue);
        // Glue the RetValue to the end of the call sequence
        Chain = RetValue.getValue(1);
        OutGlue = RetValue.getValue(2);

        if (VA.getValVT() == MVT::pointer) {
            assert(VA.needsCustom());
            VA = RetValLocs[++I];
            SDValue RetValue2 = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(),
                                                    VA.getLocVT(), OutGlue);
            Chain = RetValue2.getValue(1);
            OutGlue = RetValue2.getValue(2);
            RetValue = DAG.getNode(ORISCISD::BUILD_PTRARG, DL, MVT::pointer, 
                                    RetValue,RetValue2);
        }
        InVals.push_back(RetValue);
    }

    return Chain;
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

    // Check if Chain is from an Outgoing Box or Gep and replace it by parent Chain
    if (Chain->getOpcode() == ISD::INTRINSIC_W_CHAIN && 
        (Chain->getConstantOperandVal(1) == Intrinsic::orisc_box
                    || Chain->getConstantOperandVal(1) == Intrinsic::orisc_gep_p
                    || Chain->getConstantOperandVal(1) == Intrinsic::orisc_gep_i))
        Chain = Chain->getOperand(0);

    // Assign locations to each returned value.
    SmallVector<CCValAssign, 16> RetLocs;
    CCState RetCCInfo(CallConv, IsVarArg, MF, RetLocs, *DAG.getContext());
    RetCCInfo.AnalyzeReturn(Outs, RetCC_ORISC);

    SDValue Glue;
    // Copy the result values into the output registers.
    SmallVector<SDValue, 4> RetOps;
    RetOps.push_back(Chain);
    for (unsigned I = 0, ArgIdx = 0; I != RetLocs.size(); ++I, ++ArgIdx) {
        CCValAssign &VA = RetLocs[I];
        SDValue RetValue = OutVals[ArgIdx];

        // Make the return register live on exit.
        assert(VA.isRegLoc() && "Can only return in registers!");

        Register Reg = VA.getLocReg();
        if (VA.getLocVT() == MVT::pointer) {
            // Extract Base and Index from RetValue
            SDValue Base, Index;
            getBaseIndex(RetValue, Base, Index, DAG);

            // Push Base into Pointer Register
            Chain = DAG.getCopyToReg(Chain, DL, Reg, Base, Glue);
            Glue = Chain.getValue(1);
            RetOps.push_back(DAG.getRegister(Reg, VA.getLocVT()));
            // Push Index into Data Register
            VA = RetLocs[++I];
            Reg = VA.getLocReg();
            Chain = DAG.getCopyToReg(Chain, DL, Reg, Index, Glue);
            Glue = Chain.getValue(1);
            RetOps.push_back(DAG.getRegister(Reg, VA.getLocVT()));
        } else {
            // Chain and glue the copies together.
            Chain = DAG.getCopyToReg(Chain, DL, Reg, RetValue, Glue);
            Glue = Chain.getValue(1);
            RetOps.push_back(DAG.getRegister(Reg, VA.getLocVT()));
        }
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