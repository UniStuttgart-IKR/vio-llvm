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
#include <cassert>
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
      LegalizeAction Action = OtherVT == MVT::i1 ? Promote : Legal;
      setTruncStoreAction(VT, OtherVT, Action);
      setLoadExtAction(LoadList, VT, OtherVT, Action);
    }
  }
  setOperationAction(ISD::LOAD, MVT::i32, Legal);
  setOperationAction(ISD::LOAD, MVT::iPTR, Legal);
  setOperationAction(ISD::STORE, MVT::i32, Legal);
  setOperationAction(ISD::STORE, MVT::iPTR, Legal);
  setOperationAction(ISD::STORE, MVT::pointer, Legal);

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

  setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::pointer, Custom);
  setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::i32, Custom);
  setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::Other, Custom);

  setOperationAction(ISD::INTRINSIC_W_CHAIN, MVT::pointer, Custom);
  setOperationAction(ISD::INTRINSIC_W_CHAIN, MVT::i32, Custom);
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
    case ORISCISD::RET:
      return "ORISCISD::RET";
    case ORISCISD::BUILD_PTRARG:
      return "ORISCISD::BUILD_PTRARG";
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
        case ISD::TRUNCATE:
            return lowerTruncate(Op, DAG);
        case ISD::INTRINSIC_W_CHAIN:
            return lowerIntrinsicWChain(Op, DAG);
        case ISD::INTRINSIC_WO_CHAIN:
            return lowerIntrinsicWOChain(Op, DAG);

        default: llvm_unreachable("Should not custom lower this!");
  }
}

SDValue ORISCTargetLowering::
  lowerAdd(SDValue Op, SelectionDAG &DAG) const {

    //ADD (GEP B, I), O -> GEP B, (ADD I, O)
    //ADD O, (GEP B, I) -> GEP B, (ADD I, O)
    bool LHSIsGep = Op->getOperand(0)->getOpcode() == ISD::INTRINSIC_WO_CHAIN
                    && Op->getOperand(0).getConstantOperandVal(0) == Intrinsic::orisc_gep;
    bool RHSIsGep = Op->getOperand(1)->getOpcode() == ISD::INTRINSIC_WO_CHAIN
                    && Op->getOperand(1).getConstantOperandVal(0) == Intrinsic::orisc_gep;

    if (LHSIsGep || RHSIsGep) {
      SDLoc DL(Op);
      SDValue OldGep = LHSIsGep ? Op->getOperand(0) : Op->getOperand(1);
      SDValue GepID = DAG.getConstant(Intrinsic::orisc_gep, DL, MVT::i32);
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
    lowerIntrinsicWOChain(SDValue Op,
                            SelectionDAG &DAG) const {
    SDLoc DL(Op);
    uint64_t IntrinsicID = Op.getConstantOperandVal(0);
    switch (IntrinsicID) {
    default:
        return Op;
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
        return lowerBoxIntrinsic(Op->getOperand(0), Op->getOperand(2), Op->getOperand(3), DAG);
    }
}

SDValue ORISCTargetLowering::
    lowerBoxIntrinsic(SDValue Chain, SDValue Base, SDValue Index, SelectionDAG &DAG) const {
        // If we encounter a "llvm.orisc.box" intrinsic, we split it into
        // allocate 1, 4 -> gep with index 0 -> store base-ptr to ptr slot -> store index to data slot
        SDLoc DL(Chain);
        SDValue Const0 = DAG.getConstant(0, DL, MVT::i32);
        SDValue Const1 = DAG.getConstant(1, DL, MVT::i32);
        SDValue Const4 = DAG.getConstant(4, DL, MVT::i32);
        SDValue AlcID = DAG.getConstant(Intrinsic::orisc_allocate, DL, MVT::i32);
        SDValue GepID = DAG.getConstant(Intrinsic::orisc_gep, DL, MVT::i32);
        
        SDValue Alc = DAG.getNode(ISD::INTRINSIC_W_CHAIN, DL, 
                            { MVT::pointer, MVT::Other }, { Chain, AlcID, Const1, Const4 });
        Chain = Alc.getValue(1);
        SDValue Gep = DAG.getNode(ISD::INTRINSIC_WO_CHAIN, DL, 
                            MVT::i32, { GepID, Alc, Const0 });
        Chain = DAG.getStore(Chain, DL, Base, Gep, MachinePointerInfo());
        Chain = DAG.getStore(Chain, DL, Index, Gep, MachinePointerInfo());
        return DAG.getMergeValues(Alc, Chain);
    }


bool ORISCTargetLowering::getTgtMemIntrinsic(IntrinsicInfo &Info,
                                              const CallInst &I,
                                              MachineFunction &MF,
                                              unsigned Intrinsic) const {

  switch(Intrinsic) {
    default:
      return false;
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
    Chain = DAG.getNode(ISD::CALLSEQ_START, DL, {MVT::Other, MVT::Glue}, {Chain, Zero, Zero});
    SDValue CallSeqStart = Chain;

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

    // Check if Chain is from an Outgoing Box or Gep and replace it by parent Chain
    if (Chain->getOpcode() == ISD::INTRINSIC_W_CHAIN && 
        (Chain->getConstantOperandVal(1) == Intrinsic::orisc_box
                    || Chain->getConstantOperandVal(1) == Intrinsic::orisc_gep))
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
            if (Chain->getOpcode() == ISD::INTRINSIC_W_CHAIN && 
                Chain->getConstantOperandVal(1) == Intrinsic::orisc_box) {
                Base = RetValue->getOperand(2);
                Index = RetValue->getOperand(3);
            } else if (Chain->getOpcode() == ISD::INTRINSIC_WO_CHAIN && 
                        Chain->getConstantOperandVal(0) == Intrinsic::orisc_gep) {
                Base = RetValue->getOperand(1);
                Index = RetValue->getOperand(2);
            } else {
                Base = RetValue;
                Index = DAG.getConstant(0, DL, MVT::i32);
            }

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