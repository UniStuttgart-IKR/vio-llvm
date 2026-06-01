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
def SDTOIntLeaf: SDTypeProfile<1, 0, [SDTCisVT<0, i32>]>;
def oframeindex : SDNode<"ISD::FrameIndex",           SDTOIntLeaf, [],
                         "FrameIndexSDNode">;

def orisc_ret  : SDNode<"ORISCISD::RET", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;


def SDT_ORISCLocalCall     : SDTypeProfile<0, -1, [SDTCisVT<0, i32>]>;
def orisc_local_call  : SDNode<"ORISCISD::LOCAL_CALL", SDT_ORISCLocalCall,
                             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;
def SDT_ORISCLibraryCall     : SDTypeProfile<0, -1, [SDTCisVT<0, i32>, SDTCisVT<1, iPTR>]>;
def orisc_library_call  : SDNode<"ORISCISD::LIBRARY_CALL", SDT_ORISCLibraryCall,
                             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;

def SDT_ORISCLoadConst     : SDTypeProfile<0, -1, []>;
def orisc_load_const  : SDNode<"ORISCISD::LOAD_CONST", SDT_ORISCLoadConst, []>;
def orisc_get_capability  : SDNode<"ORISCISD::GET_CAPABILITY", SDT_ORISCLoadConst, [SDNPHasChain]>;
def orisc_get_context  : SDNode<"ORISCISD::GET_CONTEXT", SDTPtrLeaf, [SDNPHasChain]>;


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


//				------ BRANCHES ------

//Conditional Branches
class BranchDPat<CondCode Cond, ORISCInstr Inst>
		: Pat<(brcc Cond, DR:$dy, DR:$dz, bb:$sd12), (Inst DR:$dy, DR:$dz, brtarget12:$sd12)>;
class BranchPPat<CondCode Cond, ORISCInstr Inst>
		: Pat<(brcc Cond, (pointer PR:$py), (pointer PR:$pz), bb:$sd12), (Inst PR:$py, PR:$pz, brtarget12:$sd12)>;

def : BranchDPat<SETEQ,  BEQ>;
def : BranchDPat<SETNE,  BNE>;
def : BranchDPat<SETUGE, BGEU>;
def : BranchDPat<SETGE,  BGES>;
def : BranchDPat<SETULT, BLTU>;
def : BranchDPat<SETLT,  BLTS>;

def : BranchPPat<SETEQ, BEQP>;
def : BranchPPat<SETNE, BNEP>;

//Inversed Branch Conditions
class InverseBranchPat<CondCode Cond, ORISCInstr Inst>
		: Pat<(brcc Cond, DR:$dy, DR:$dz, bb:$sd12), (Inst DR:$dz, DR:$dy, brtarget12:$sd12)>;

def : InverseBranchPat<SETUGT, BLTU>;
def : InverseBranchPat<SETGT,  BLTS>;
def : InverseBranchPat<SETULE, BGEU>;
def : InverseBranchPat<SETLE,  BGES>;

//Unconditional Branch
//def : Pat<(br bb:$sd25), (BRA brtarget25:$sd25)>;

//				------ CALLS ------
def : Pat<(orisc_local_call tglobaladdr:$symbol), (BSR subroutine_symbol:$symbol)>;
def : Pat<(orisc_local_call globaladdr:$symbol), (BSR subroutine_symbol:$symbol)>;
def : Pat<(orisc_local_call texternalsym:$symbol), (BSR subroutine_symbol:$symbol)>;

def : Pat<(orisc_load_const tglobaladdr:$symbol), (LP_I (pointer P28), external_symbol:$symbol)>;
// def : Pat<(orisc_local_call texternalsym:$symbol), (JLIB (LP_I (pointer P29), call_symbol:$symbol), call_symbol:$symbol)>;
def : Pat<(orisc_library_call texternalsym:$symbol, PR:$px), (JLIB PR:$px, external_symbol:$symbol)>;


//				------ STORES ------

//Stores with constant Offset
multiclass StoreIPat<PatFrag StoreOp, ORISCInstr Inst, RegisterClass XR = DR> {
	def : Pat<(StoreOp XR:$dy, oframeindex:$fi), (Inst (pointer P30), (i32 (to_tframeindex $fi)), XR:$dy)>;
	def : Pat<(StoreOp XR:$dy, (int_orisc_gep (iPTR PR:$px), uimm12:$ix12)), (Inst PR:$px, uimm12:$ix12, XR:$dy)>;
//					   value       			  base   	 offset				 base      offset     value
}

//Stores with variable Index
multiclass StorePat<PatFrag StoreOp, ORISCInstr Inst, RegisterClass XR = DR> {
	def  : Pat<(StoreOp XR:$dy, (int_orisc_gep (iPTR PR:$px), (add DR:$dz, uimm4:$ui4))), (Inst PR:$px, DR:$dz, 1, uimm4:$ui4, XR:$dy)>;
	def  : Pat<(StoreOp XR:$dy, (int_orisc_gep (iPTR PR:$px), DR:$dz)), (Inst PR:$px, DR:$dz, 1, 0, XR:$dy)>;
//					     value		 	  			   base	   index   		    base	index  s  p  value
}

//def : StoreIPat<store,         SP_I, PR>;
defm : StoreIPat<store,         SW_I>;
defm : StoreIPat<truncstorei16, SH_I>;
defm : StoreIPat<truncstorei8,  SB_I>;

//def : StorePat<store,         SP, PR>;
defm : StorePat<store,         SW>;
defm : StorePat<truncstorei16, SH>;
defm : StorePat<truncstorei8,  SB>;

def : Pat<(store (pointer PR:$py), oframeindex:$fi), (SP_I (pointer P30), (i32 (to_tframeindex $fi)), PR:$py)>;
def : Pat<(store (pointer PR:$py), (int_orisc_gep (iPTR PR:$px), uimm12:$ix12)), (SP_I PR:$px, uimm12:$ix12, PR:$py)>;
def : Pat<(store (pointer PR:$py), (int_orisc_gep (iPTR PR:$px), DR:$dz)), (SP PR:$px, DR:$dz, 1, 0, PR:$py)>;
def : Pat<(store (pointer PR:$py), (int_orisc_gep (iPTR PR:$px), (add DR:$dz, uimm4:$ui4))), (SP PR:$px, DR:$dz, 1, uimm4:$ui4, PR:$py)>;


//				------ LOADS ------

def : Pat<(orisc_load_const tglobaladdr:$symbol), (LP_I (pointer P28), external_symbol:$symbol)>;
def : Pat<(orisc_get_capability (iPTR texternalsym:$symbol)), (LP_I (pointer P29), (iPTR external_symbol:$symbol))>;

//Loads with constant Offset
multiclass LoadIPat<PatFrag LoadOp, ORISCInstr Inst, ValueType vt = i32> {
	def : Pat<(vt (LoadOp oframeindex:$fi)), (Inst (pointer P30), (i32 (to_tframeindex $fi)))>;
	def : Pat<(vt (LoadOp (int_orisc_gep PR:$px, uimm12:$ix12))), (Inst PR:$px, uimm12:$ix12)>;
//					   					  base       offset   			 base       offset
}

//Loads with variable Index
multiclass LoadPat<PatFrag LoadOp, ORISCInstr Inst, ValueType vt = i32> {
	def : Pat<(vt (LoadOp (int_orisc_gep PR:$px, (add DR:$dz, uimm4:$ui4)))), (Inst PR:$px, DR:$dz, 1, uimm4:$ui4)>;
	def : Pat<(vt (LoadOp (int_orisc_gep PR:$px, DR:$dz))), (Inst PR:$px, DR:$dz, 1, 0)>;
//					   				  	  base   index		 	   base	  index   s  p
}

defm : LoadIPat<sextloadi8,  LBS_I>;
defm : LoadIPat<zextloadi8,  LBU_I>;
defm : LoadIPat<extloadi8,   LBU_I>;
defm : LoadIPat<sextloadi16, LHS_I>;
defm : LoadIPat<zextloadi16, LHU_I>;
defm : LoadIPat<extloadi16,  LHU_I>;
defm : LoadIPat<load,        LW_I>;
defm : LoadIPat<load,        LP_I, iPTR>;

defm : LoadPat<sextloadi8,  LBS>;
defm : LoadPat<zextloadi8,  LBU>;
defm : LoadPat<extloadi8,   LBU>;
defm : LoadPat<sextloadi16, LHS>;
defm : LoadPat<zextloadi16, LHU>;
defm : LoadPat<extloadi16,  LHU>;
defm : LoadPat<load,        LW>;
defm : LoadPat<load,        LP, iPTR>;


//				------ ALLOCATES ------
//
def : Pat<(int_orisc_allocate DR:$dy,       DR:$dz),          (ALC    DR:$dy,       DR:$dz)>;
def : Pat<(int_orisc_allocate DR:$dy,       uimm15:$delta15), (ALC_D  DR:$dy,       uimm15:$delta15)>;
def : Pat<(int_orisc_allocate uimm11:$pi11, DR:$dz),          (ALC_P  uimm11:$pi11, DR:$dz)>;
def : Pat<(int_orisc_allocate uimm11:$pi11, uimm15:$delta15), (ALC_PD uimm11:$pi11, uimm15:$delta15)>;



//				------ MISC ------

def : Pat<(int_orisc_unbox_base PR:$px), (LP_I PR:$px, 0)>;
def : Pat<(int_orisc_unbox_index PR:$px), (LW_I PR:$px, 0)>;
def : Pat<(orisc_ret), (RET)>;
def : Pat<(i32 0), (i32 D0)>;
def : Pat<(uimm12:$ui12), (ORI D0, uimm12:$ui12)>;
def : Pat<(imm:$imm), (ORI (LUI (Upper20 imm:$imm)), (Lower12 imm:$imm))>;
def : Pat<(orisc_get_context), (GCP)>;