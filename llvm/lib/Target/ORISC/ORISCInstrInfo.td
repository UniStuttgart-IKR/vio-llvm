//===-- ObjectiveRISCInstrInfo.td - Target Description for Objective-RISC --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the Objective-RISC instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

include "ORISCInstrFormats.td"

//===----------------------------------------------------------------------===//
// DAG Nodes
//===----------------------------------------------------------------------===//

//TODO: ADD ORISCISD::LOAD and ORISCISD:STORE
def SDTOPtrLeaf: SDTypeProfile<1, 0, [SDTCisVT<0, iPTR>]>;
def oframeindex : SDNode<"ISD::FrameIndex",           SDTOPtrLeaf, [],
                         "FrameIndexSDNode">;

def orisc_ret  : SDNode<"ORISCISD::RET", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

//===----------------------------------------------------------------------===//
// Instruction Selection Patterns
//===----------------------------------------------------------------------===//

//				------ ARITHMETICS ------
class PatDD<SDPatternOperator OpNode, ORISCInstr Inst>
		: Pat<(i32 (OpNode (i32 DR:$dy), (i32 DR:$dz))), (Inst DR:$dy, DR:$dz)>;
class PatDUI12<SDPatternOperator OpNode, ORISCInstr Inst>
		: Pat<(i32 (OpNode (i32 DR:$dy), uimm12:$ui12)), (Inst DR:$dy, uimm12:$ui12)>;
class PatDSI12<SDPatternOperator OpNode, ORISCInstr Inst>
		: Pat<(i32 (OpNode (i32 DR:$dy), simm12:$si12)), (Inst DR:$dy, simm12:$si12)>;
class PatDUI5<SDPatternOperator OpNode, ORISCInstr Inst>
		: Pat<(i32 (OpNode (i32 DR:$dy), uimm5:$ui5)), (Inst DR:$dy, uimm5:$ui5)>;
class PatD<SDPatternOperator OpNode, ORISCInstr Inst>
		: Pat<(i32 (OpNode (i32 DR:$dy))), (Inst DR:$dy)>;

def : PatDD<add, 	ADD>;
def : PatDD<sub, 	SUB>;
def : PatDD<and, 	AND>;
def : PatDD<or, 	OR>;
def : PatDD<xor, 	XOR>;
def : PatDD<setult, SLTU>;
def : PatDD<setlt, 	SLTS>;
def : PatDD<mul, 	MUL>;
def : PatDD<mulhs, 	MULHS>;
def : PatDD<mulhu, 	MULHU>;

def : PatDUI12<add,    ADDI>;
def : PatDUI12<sub,    SUBI>;
def : PatDUI12<and,    ANDI>;
def : PatDUI12<or, 	   ORI>;
def : PatDUI12<xor,    XORI>;
def : PatDUI12<setult, SLTIU>;
def : PatDSI12<setlt,  SLTIS>;
def : PatDUI12<mul,    MULIU>;
def : PatDSI12<mul,    MULIS>;

def : PatDD<srl, 	SRL>;
def : PatDD<sra, 	SRA>;
def : PatDD<shl, 	SLL>;
def : PatDD<rotl, 	ROL>;
def : PatDUI5<srl, 	SRL>;
def : PatDUI5<sra, 	SRA>;
def : PatDUI5<shl, 	SLL>;
def : PatDUI5<rotl, ROL>;

def : PatD<not, NOT>;
def : Pat<(sext_inreg (i32 DR:$dy), i8),  (EXTBS (i32 DR:$dy))>;
def : Pat<(sext_inreg (i32 DR:$dy), i16), (EXTHS (i32 DR:$dy))>;
def : PatD<ctlz, CLZ>;


def to_tframeindex : SDNodeXForm<frameindex, [{
  return CurDAG->getTargetFrameIndex(N->getIndex(), N->getValueType(0));
}]>;


//				------ BRANCHES ------

//Conditional Branches
class BranchPat<CondCode Cond, ORISCInstr Inst, RegisterClass XR = DR>
		: Pat<(brcc Cond, XR:$dy, XR:$dz, bb:$sd12), (Inst XR:$dy, XR:$dz, brtarget:$sd12)>;

def : BranchPat<SETEQ,  BEQ>;
def : BranchPat<SETNE,  BNE>;
def : BranchPat<SETUGE, BGEU>;
def : BranchPat<SETGE,  BGES>;
def : BranchPat<SETULT, BLTU>;
def : BranchPat<SETLT,  BLTS>;

def : BranchPat<SETEQ, BEQP, PR>;
def : BranchPat<SETNE, BNEP, PR>;

//Inversed Branch Conditions
class InverseBranchPat<CondCode Cond, ORISCInstr Inst>
		: Pat<(brcc Cond, DR:$dy, DR:$dz, bb:$sd12), (Inst DR:$dz, DR:$dy, brtarget:$sd12)>;

def : InverseBranchPat<SETUGT, BLTU>;
def : InverseBranchPat<SETGT,  BLTS>;
def : InverseBranchPat<SETULE, BGEU>;
def : InverseBranchPat<SETLE,  BGES>;

//Unconditional Branch
def : Pat<(br bb:$sd25), (BRA brtarget:$sd25)>;



//				------ STORES ------

//Stores with constant Offset
class StoreIPat<PatFrag StoreOp, ORISCInstr Inst>
		: Pat<(StoreOp DR:$dy, (int_orisc_gep PR:$px, uimm12:$ix12)), (Inst PR:$px, uimm12:$ix12, DR:$dy)>;
//					   value       			  base   	 offset				 base      offset     value

//Stores with variable Index
class StorePat<PatFrag StoreOp, ORISCInstr Inst>
    : Pat<(StoreOp DR:$dy, (int_orisc_gep PR:$px, DR:$dz)), (Inst PR:$px, DR:$dz, 1, 0, DR:$dy)>;
//				   value		 	  	   base	  index   		   base	  index   s  p  value

def : StoreIPat<store,         SW_I>;
def : StoreIPat<truncstorei16, SH_I>;
def : StoreIPat<truncstorei8,  SB_I>;

def : StorePat<store,         SW>;
def : StorePat<truncstorei16, SH>;
def : StorePat<truncstorei8,  SB>;



//				------ LOADS ------

//Loads with constant Offset
class LoadIPat<PatFrag LoadOp, ORISCInstr Inst, ValueType vt = i32>
		: Pat<(vt (LoadOp (int_orisc_gep PR:$px, uimm12:$ix12))), (Inst PR:$px, uimm12:$ix12)>;
//					   					  base       offset   			 base       offset

//Loads with variable Index
class LoadPat<PatFrag LoadOp, ORISCInstr Inst, ValueType vt = i32>
    : Pat<(vt (LoadOp (int_orisc_gep PR:$px, DR:$dz))), (Inst PR:$px, DR:$dz, 1, 0)>;
//					   				  base   index		 	   base	  index   s  p

def : LoadIPat<sextloadi8,  LBS_I>;
def : LoadIPat<zextloadi8,  LBU_I>;
def : LoadIPat<extloadi8,   LBU_I>;
def : LoadIPat<sextloadi16, LHS_I>;
def : LoadIPat<zextloadi16, LHU_I>;
def : LoadIPat<extloadi16,  LHU_I>;
def : LoadIPat<load,        LW_I, i32>;

def : LoadPat<sextloadi8,  LBS>;
def : LoadPat<zextloadi8,  LBU>;
def : LoadPat<extloadi8,   LBU>;
def : LoadPat<sextloadi16, LHS>;
def : LoadPat<zextloadi16, LHU>;
def : LoadPat<extloadi16,  LHU>;
def : LoadPat<load,        LW, i32>;


//				------ ALLOCATES ------
//
def : Pat<(int_orisc_allocate DR:$dy,       DR:$dz),          (ALC    DR:$dy,       DR:$dz)>;
def : Pat<(int_orisc_allocate DR:$dy,       uimm15:$delta15), (ALC_D  DR:$dy,       uimm15:$delta15)>;
def : Pat<(int_orisc_allocate uimm11:$pi11, DR:$dz),          (ALC_P  uimm11:$pi11, DR:$dz)>;
def : Pat<(int_orisc_allocate uimm11:$pi11, uimm15:$delta15), (ALC_PD uimm11:$pi11, uimm15:$delta15)>;



//				------ MISC ------

def : Pat<(orisc_ret), (RET)>;
def : Pat<(uimm12:$ui12), (ORI D0, uimm12:$ui12)>;