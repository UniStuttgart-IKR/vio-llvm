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

// Immediate Operands
class ObjRISCOp<ValueType vt = i32> : Operand<vt> {
  let OperandNamespace = "ObjectiveRISCOp";
}

class ObjRISCUImmOp<int bitsNum> : ObjRISCOp {
  let MCOperandPredicate = [{
    int64_t Imm;
    if (!MCOp.evaluateAsConstantImm(Imm))
      return false;
    return isUInt<}]# bitsNum #[{>(Imm);
  }];
}

class ObjRISCUImmLeafOp<int bitsNum> :
  ObjRISCUImmOp<bitsNum>, ImmLeaf<i32, "return isUInt<" # bitsNum # ">(Imm);">;

class ObjRISCSImmOp<int bitsNum> : ObjRISCOp {
  let MCOperandPredicate = [{
    int64_t Imm;
    if (!MCOp.evaluateAsConstantImm(Imm))
      return false;
    return isInt<}] # bitsNum # [{>(Imm);
  }];
}

class ObjRISCSImmLeafOp<int bitsNum> :
  ObjRISCSImmOp<bitsNum>, ImmLeaf<i32, "return isInt<" # bitsNum # ">(Imm);">;

class ObjRISCScaleImmOp<int bitsNum> : ObjRISCOp {
  let MCOperandPredicate = [{
    int64_t Imm;
    if (!MCOp.evaluateAsConstantImm(Imm))
      return false;
    return 1 <= Imm && Imm <= (1 <<}] # bitsNum # [{);
  }];
}

class ObjRISCScaleImmLeafOp<int bitsNum> :
  ObjRISCSImmOp<bitsNum>, ImmLeaf<i32, "return 1 <= Imm && Imm <= (1 <<" # bitsNum # ");">;

def uimm4  : ObjRISCUImmLeafOp<4>;
def uimm5  : ObjRISCUImmLeafOp<5>;
def uimm10 : ObjRISCUImmLeafOp<10>;
def uimm11 : ObjRISCUImmLeafOp<11>;
def uimm12 : ObjRISCUImmLeafOp<12>; 
def uimm15 : ObjRISCUImmLeafOp<15>;
def uimm20 : ObjRISCUImmLeafOp<20>;

def simm12 : ObjRISCSImmLeafOp<12>;
def simm20 : ObjRISCSImmLeafOp<20>;
def simm25 : ObjRISCSImmLeafOp<25>;

def scimm3 : ObjRISCScaleImmLeafOp<3>;


// Instruction Formats
//======================================================================================
class InstrFormat;

def FormatReg0 : InstrFormat;
def FormatALU1 : InstrFormat;
def FormatALU2 : InstrFormat;
def FormatALU3 : InstrFormat;
def FormatPGU4 : InstrFormat;
def FormatPGU5 : InstrFormat;
def FormatPGU6 : InstrFormat;
def FormatLSU8 : InstrFormat;
def FormatLSU9 : InstrFormat;
def FormatLSUA : InstrFormat;
def FormatLSUB : InstrFormat;
def FormatBPUC : InstrFormat;
def FormatBPUD : InstrFormat;
def FormatBPUE : InstrFormat;
def FormatBPUF : InstrFormat;

class ObjRISCInstr<string opcodestr, dag outs, dag ins, string argstr, InstrFormat fmt>
		: Instruction {
	let Namespace = "ObjectiveRISC";

	InstrFormat Format = fmt;
	dag OutOperandList = outs;
	dag InOperandList = ins;
	let AsmString = opcodestr # !if(!empty(argstr), "", !if(!lt(!size(opcodestr), 4), "\t", "") # "\t" # argstr);
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {

// ALU Instructions
class ObjRISCInstrRegALU<string opcodestr, bit Commutable = 0>
		: ObjRISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dy, DR:$dz), "$dx = $dy, $dz", FormatReg0> {

	let isCommutable = Commutable;
}

class ObjRISCInstrUImmALU<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dy, uimm12:$ui12), "$dx = $dy, $ui12", FormatALU1> {
}

class ObjRISCInstrSImmALU<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dy, simm12:$si12), "$dx = $dy, $si12", FormatALU2> {
}

class ObjRISCInstrRegShiftImm<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dy, uimm5:$ui5), "$dx = $dy, $ui5", FormatReg0> {
}

class ObjRISCInstrRegUnary<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dz), "$dx = $dz", FormatReg0> {
}

// Attribute Instructions
class ObjRISCInstrRegAttr<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs DR:$dy), (ins PR:$px), "$dy = $px", FormatReg0> {
}

}

// Load/Store Instructions with Immediate Index
let hasSideEffects = 0 in {
class ObjRISCInstrLoadImmD<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs DR:$dy), (ins PR:$px, uimm12:$ix12), "$dy = $px[ix12]", FormatLSU8> {
	let mayLoad = 1;
	let mayStore = 0;
}

class ObjRISCInstrStoreImmD<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs), (ins PR:$px, uimm12:$ix12, DR:$dy), "$px[ix12] = $dy", FormatLSU9> {
	let mayLoad = 0;
	let mayStore = 1;
}

class ObjRISCInstrLoadImmP<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs PR:$py), (ins PR:$px, uimm12:$ix12), "$py = $px[ix12]", FormatLSU9> {
	let mayLoad = 1;
	let mayStore = 0;
}

class ObjRISCInstrStoreImmP<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs), (ins PR:$px, uimm12:$ix12, PR:$py), "$px[ix12] = $py", FormatLSU9> {
	let mayLoad = 0;
	let mayStore = 1;
}

// Load/Store Instructions with Register Index
class ObjRISCInstrLoadRegD<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs DR:$dy), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4), "$dy = $px[$dz*$s3+$ud4]", FormatLSUA> {
	let mayLoad = 1;
	let mayStore = 0;
}

class ObjRISCInstrStoreRegD<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4, DR:$dy), "$px[$dz*$s3+$ud4] = $dy", FormatLSUB> {
	let mayLoad = 0;
	let mayStore = 1;
}

class ObjRISCInstrLoadRegP<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs PR:$py), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4), "$py = $px[$dz*$s3+$ud4]", FormatLSUB> {
	let mayLoad = 1;
	let mayStore = 0;
}

class ObjRISCInstrStoreRegP<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4, PR:$py), "$px[$dz*$s3+$ud4] = $py", FormatLSUB> {
	let mayLoad = 0;
	let mayStore = 1;
}
}

// Branch Instructions
let mayLoad = 0, mayStore = 0 in {
let hasSideEffects = 0 in {
class ObjRISCInstrBranchP<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs), (ins PR:$px, PR:$py, simm12:$sd12), "$px, $py, $sd12", FormatBPUC> {
}

class ObjRISCInstrBranchD<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs), (ins DR:$dx, DR:$dz, simm12:$sd12), "$dx, $dz, $sd12", FormatBPUC> {
}
}

class ObjRISCInstrBranch<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs), (ins simm25:$sd25), "$sd25", FormatBPUD> {
}

// Dynamic Jump Instructions
class ObjRISCInstrRegJ<string opcodestr>
		: ObjRISCInstr<opcodestr, (outs), (ins DR:$dy), "$dy", FormatReg0> {
}
}

// Concrete Instructions
//======================================================================================

// TODO: Eigene Definition für SCP und LCP!? let Constraints = "$py = $py_out"
// TODO: Attribute? isBranch, isCall, isReturn, isTerminator, Defs, Uses, hasSideEffects, mayLoad, mayStore, ...

// ALU Instructions
def ADD:		ObjRISCInstrRegALU<"add", Commutable=1>;
def SUB:		ObjRISCInstrRegALU<"sub", Commutable=0>;
def AND:		ObjRISCInstrRegALU<"and", Commutable=1>;
def ANDN:		ObjRISCInstrRegALU<"andn", Commutable=1>;
def OR:			ObjRISCInstrRegALU<"or", Commutable=1>;
def XOR:		ObjRISCInstrRegALU<"xor", Commutable=1>;
def SLTU:		ObjRISCInstrRegALU<"sltu", Commutable=0>;
def SLTS:		ObjRISCInstrRegALU<"slts", Commutable=0>;
def MUL:		ObjRISCInstrRegALU<"mul", Commutable=1>;
def MULHS:	ObjRISCInstrRegALU<"mulhs", Commutable=1>;
def MULHU:	ObjRISCInstrRegALU<"mulhu", Commutable=1>;
def MULHSU:	ObjRISCInstrRegALU<"mulhsu", Commutable=0>;

def ADDI:		ObjRISCInstrUImmALU<"addi">;
def SUBI:		ObjRISCInstrUImmALU<"subi">;
def ANDI:		ObjRISCInstrUImmALU<"andi">;
def ANDNI:	ObjRISCInstrUImmALU<"andni">;
def ORI:		ObjRISCInstrUImmALU<"ori">;
def XORI:		ObjRISCInstrUImmALU<"xori">;
def SLTIU:	ObjRISCInstrUImmALU<"sltui">;
def SLTIS:	ObjRISCInstrSImmALU<"sltsi">;
def MULIU:	ObjRISCInstrUImmALU<"muliu">;
def MULIS:	ObjRISCInstrSImmALU<"mulis">;

def SRL:		ObjRISCInstrRegALU<"srl", Commutable=0>;
def SRA:		ObjRISCInstrRegALU<"sra", Commutable=0>;
def SLL:		ObjRISCInstrRegALU<"sll", Commutable=0>;
def ROL:		ObjRISCInstrRegALU<"rol", Commutable=0>;
//def ROR		ObjRISCInstrRegALU<"ror", Commutable=0>;
def SRLI:		ObjRISCInstrRegShiftImm<"srli">;
def SRAI:		ObjRISCInstrRegShiftImm<"srai">;
def SLLI:		ObjRISCInstrRegShiftImm<"slli">;
def ROLI:		ObjRISCInstrRegShiftImm<"roli">;

def NOT:		ObjRISCInstrRegUnary<"not">;
def EXTBS:	ObjRISCInstrRegUnary<"extbs">;
def EXTHU:	ObjRISCInstrRegUnary<"exthu">;
def EXTHS:	ObjRISCInstrRegUnary<"exths">;
def CLZ:		ObjRISCInstrRegUnary<"clz">;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def LUI:		ObjRISCInstr<"lui", (outs DR:$dx), (ins uimm20:$ui20), "$dx = $ui20", FormatALU3>;

// Attribute Instructions
def QPI:		ObjRISCInstrRegAttr<"qpi">;
def QDTB:		ObjRISCInstrRegAttr<"qdtb">;
def QDTH:		ObjRISCInstrRegAttr<"qdth">;
def QDTW:		ObjRISCInstrRegAttr<"qdtw">;
def QDTD:		ObjRISCInstrRegAttr<"qdtd">;
let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
def NCHK:		ObjRISCInstr<"nchk", (outs), (ins PR:$px), "$px", FormatReg0>;

// Pointer Instructions
let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
def	CPP:		ObjRISCInstr<"cpp", (outs PR:$py), (ins PR:$px), "$py = $px", FormatReg0>;
def	GCP:		ObjRISCInstr<"gcp", (outs PR:$px), (ins), "$px", FormatReg0>;
def	ALC:		ObjRISCInstr<"alc", (outs PR:$px), (ins DR:$dy, DR:$dz), "$px = $dy, $dz", FormatReg0>;
def	ALC_D:	ObjRISCInstr<"alc", (outs PR:$px), (ins DR:$dy, uimm15:$delta15), "$px = $dy, $delta15", FormatPGU4>;
def	ALC_P:	ObjRISCInstr<"alc", (outs PR:$px), (ins uimm11:$pi11, DR:$dz), "$px = $pi11, $dz", FormatPGU5>;
def	ALC_PD:	ObjRISCInstr<"alc", (outs PR:$px), (ins uimm11:$pi11, uimm15:$delta15), "$px = $pi11, $delta15", FormatPGU6>;
}

// Stack Instructions
let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
let Defs = [P30] in {
def	PUSH:		ObjRISCInstr<"push", (outs), (ins uimm11:$pi11, uimm15:$delta15), "$px = $pi11, $delta15", FormatPGU6>;
def	PUSHT:	ObjRISCInstr<"pusht", (outs), (ins uimm11:$pi11, uimm15:$delta15), "$px = $pi11, $delta15", FormatPGU5>;
}
let Uses = [P30] in
def	POP:		ObjRISCInstr<"pop", (outs), (ins), "", FormatReg0>;
}
let hasSideEffects = 1, mayLoad = 0, mayStore = 1, Uses = [P31] in
def	SVRPC:	ObjRISCInstr<"sv", (outs), (ins), "rpc", FormatLSUB>;
let hasSideEffects = 1, mayLoad = 1, mayStore = 0, Defs = [P31] in
def	RSTRPC:	ObjRISCInstr<"rst", (outs), (ins), "rpc", FormatLSUB>;

// Load/Store Instructions
def LBU_I:	ObjRISCInstrLoadImmD<"lbu">;
def LBS_I:	ObjRISCInstrLoadImmD<"lbs">;
def LHU_I:	ObjRISCInstrLoadImmD<"lhu">;
def LHS_I:	ObjRISCInstrLoadImmD<"lhs">;
def LW_I:		ObjRISCInstrLoadImmD<"lw">;
def LP_I:		ObjRISCInstrLoadImmP<"lp">;

def SB_I:		ObjRISCInstrStoreImmD<"sb">;
def SH_I:		ObjRISCInstrStoreImmD<"sh">;
def SW_I:		ObjRISCInstrStoreImmD<"sw">;
def SP_I:		ObjRISCInstrStoreImmP<"sp">;

def LBU:		ObjRISCInstrLoadRegD<"lbu">;
def LBS:		ObjRISCInstrLoadRegD<"lbs">;
def LHU:		ObjRISCInstrLoadRegD<"lhu">;
def LHS:		ObjRISCInstrLoadRegD<"lhs">;
def LW:			ObjRISCInstrLoadRegD<"lw">;
def LP:			ObjRISCInstrLoadRegP<"lp">;

def SB:			ObjRISCInstrStoreRegD<"sb">;
def SH:			ObjRISCInstrStoreRegD<"sh">;
def SW:			ObjRISCInstrStoreRegD<"sw">;
def SP:			ObjRISCInstrStoreRegP<"sp">;

// Branch and Jump Instructions
let hasSideEffects = 0 in {
def JMP:		ObjRISCInstrRegJ<"jmp">;
def BRA:		ObjRISCInstrBranch<"bra">;
let Defs = [P31] in {
def BSR:		ObjRISCInstrBranch<"bsr">;
def JSR:		ObjRISCInstrRegJ<"jsr">;
let mayLoad = 0, mayStore = 0 in
def JLIB:		ObjRISCInstr<"jlib", (outs), (ins PR:$px, uimm20:$ix20), "$px, $ix20", FormatBPUF>;
}
let isReturn = 1, isTerminator = 1, mayLoad = 0, mayStore = 0, Uses = [P31] in
def	RET:		ObjRISCInstr<"ret", (outs), (ins), "", FormatReg0>;
}

def	BEQP:		ObjRISCInstrBranchP<"beqp">;
def	BNEP:		ObjRISCInstrBranchP<"bnep">;
def	BEQ:		ObjRISCInstrBranchD<"beq">;
def	BNE:		ObjRISCInstrBranchD<"bne">;
def	BGEU:		ObjRISCInstrBranchD<"bgeu">;
def	BGES:		ObjRISCInstrBranchD<"bges">;
def	BLTU:		ObjRISCInstrBranchD<"bltu">;
def	BLTS:		ObjRISCInstrBranchD<"blts">;

// Other Instructions
let hasSideEffects = 1, mayLoad = 1, mayStore = 1 in
def	TRAP:		ObjRISCInstr<"trap", (outs), (ins uimm10:$ui10), "$ui10", FormatReg0>;

// Pseudo Instructions
def NOP: 		InstAlias<"nop",										(ADD D0, D0, D0)>;
def CP: 		InstAlias<"cp\t\t$dx = $dy", 				(ADD DR:$dx, DR:$dy, D0)>;
def LI: 		InstAlias<"li\t\t$dx = $ui12", 			(ADDI DR:$dx, D0, uimm12:$ui12)>;
def LNI: 		InstAlias<"lni\t$dx = $ui12",				(SUB DR:$dx, D0, DR:$dy)>;
def CLR: 		InstAlias<"clr\t$dx",								(ADD DR:$dx, D0, D0)>;
def INC: 		InstAlias<"inc\t$dx",								(ADDI DR:$dx, DR:$dx, 1)>;
def DEC: 		InstAlias<"dec\t$dx",								(SUBI DR:$dx, DR:$dx, 1)>;
def EXTBU:	InstAlias<"extbu\t$dx = $dy",				(ANDI DR:$dx, DR:$dy, 255)>;
def NEG: 		InstAlias<"neg\t$dx = $dy",					(SUB DR:$dx, D0, DR:$dy)>;
def QDT: 		InstAlias<"qdt\t$dy = $px",					(QDTB DR:$dy, PR:$px)>;

def SV_D:		InstAlias<"sv\t\t$dx, $ui12",				(SW_I P30, uimm12:$ui12, DR:$dx)>;
def SV_P:		InstAlias<"sv\t\t$px, $ui12",				(SP_I P30, uimm12:$ui12, PR:$px)>;
def RST_D:	InstAlias<"rst\t$dy, $ui12",				(LW_I DR:$dx, P30, uimm12:$ui12)>;
def RST_P:	InstAlias<"rst\t$py, $ui12",				(LP_I PR:$px, P30, uimm12:$ui12)>;

def MULHUS:	InstAlias<"mulhsu\t$dx = $dy, $dz", (MULHSU DR:$dx, DR:$dz, DR:$dy)>;
def BGTS:		InstAlias<"bgts\t$dy, $dz, $sd12", 	(BLTS DR:$dz, DR:$dy, simm12:$sd12)>;
def BLES:		InstAlias<"bles\t$dy, $dz, $sd12", 	(BGES DR:$dz, DR:$dy, simm12:$sd12)>;
def BGTU:		InstAlias<"bgtu\t$dy, $dz, $sd12", 	(BLTU DR:$dz, DR:$dy, simm12:$sd12)>;
def BLEU:		InstAlias<"bleu\t$dy, $dz, $sd12", 	(BGEU DR:$dz, DR:$dy, simm12:$sd12)>;

def BEQP_1:	InstAlias<"beqp\t$px, $sd12",				(BEQP PR:$px, P0, simm12:$sd12)>;
def BNEP_1:	InstAlias<"bnep\t$px, $sd12",				(BNEP PR:$px, P0, simm12:$sd12)>;
def BEQ_1:	InstAlias<"beq\t$dx, $sd12",				(BEQ  PR:$dx, D0, simm12:$sd12)>;
def BNE_1:	InstAlias<"bne\t$dx, $sd12",				(BNE  PR:$dx, D0, simm12:$sd12)>;
def BLE_1:	InstAlias<"ble\t$dx, $sd12",				(BGES D0, PR:$dx, simm12:$sd12)>;
def BGE_1:	InstAlias<"bge\t$dx, $sd12",				(BGES PR:$dx, D0, simm12:$sd12)>;
def BLT_1:	InstAlias<"blt\t$dx, $sd12",				(BLTS D0, PR:$dx, simm12:$sd12)>;
def BGT_1:	InstAlias<"bgt\t$dx, $sd12",				(BLTS PR:$dx, D0, simm12:$sd12)>;

// TODO: multiclass benutzen?
def	ADDI_1:	InstAlias<"addi\t$dx = $ui12",			(ADDI DR:$dx, DR:$dx, uimm12:$ui12)>;
def	SUBI_1:	InstAlias<"subi\t$dx = $ui12",			(SUBI DR:$dx, DR:$dx, uimm12:$ui12)>;
def	ANDI_1:	InstAlias<"andi\t$dx = $ui12",			(ANDI DR:$dx, DR:$dx, uimm12:$ui12)>;
def	ANDNI_1:InstAlias<"andni\t$dx = $ui12",			(ANDNI DR:$dx, DR:$dx, uimm12:$ui12)>;
def	ORI_1:	InstAlias<"ori\t$dx = $ui12",				(ORI DR:$dx, DR:$dx, uimm12:$ui12)>;
def	XORI_1:	InstAlias<"xori\t$dx = $ui12",			(XORI DR:$dx, DR:$dx, uimm12:$ui12)>;
def	MULIU_1:InstAlias<"muliu\t$dx = $ui12",			(MULIU DR:$dx, DR:$dx, uimm12:$ui12)>;
def	MULIS_1:InstAlias<"mulis\t$dx = $ui12",			(MULIS DR:$dx, DR:$dx, uimm12:$ui12)>;
def	SRLI_1:	InstAlias<"srli\t$dx = $ui5",				(SRLI DR:$dx, DR:$dx, uimm5:$ui5)>;
def	SRAI_1:	InstAlias<"srai\t$dx = $ui5",				(SRAI DR:$dx, DR:$dx, uimm5:$ui5)>;
def	SLLI_1:	InstAlias<"slli\t$dx = $ui5",				(SLLI DR:$dx, DR:$dx, uimm5:$ui5)>;

def	ADD_1:	InstAlias<"addi\t$dx = $dz",				(ADDI DR:$dx, DR:$dx, DR:$dz)>;
def	SUB_1:	InstAlias<"subi\t$dx = $dz",				(SUBI DR:$dx, DR:$dx, DR:$dz)>;
def	AND_1:	InstAlias<"andi\t$dx = $dz",				(ANDI DR:$dx, DR:$dx, DR:$dz)>;
def	ANDN_1:	InstAlias<"andni\t$dx = $dz",				(ANDNI DR:$dx, DR:$dx, DR:$dz)>;
def	OR_1:		InstAlias<"ori\t$dx = $dz",					(ORI DR:$dx, DR:$dx, DR:$dz)>;
def	XOR_1:	InstAlias<"xori\t$dx = $dz",				(XORI DR:$dx, DR:$dx, DR:$dz)>;
def	MUL_1:	InstAlias<"muliu\t$dx = $dz",				(MULIU DR:$dx, DR:$dx, DR:$dz)>;
def	SRL_1:	InstAlias<"srli\t$dx = $dz",				(SRLI DR:$dx, DR:$dx, DR:$dz)>;
def	SRA_1:	InstAlias<"srai\t$dx = $dz",				(SRAI DR:$dx, DR:$dx, DR:$dz)>;
def	SLL_1:	InstAlias<"slli\t$dx = $dz",				(SLLI DR:$dx, DR:$dx, DR:$dz)>;

def SEQ	:		InstAlias<"seq\t$dx = $dy",					(SLTIU DR:$dx, DR:$dy, 1)>;
def SNE	:		InstAlias<"sne\t$dx = $dy",					(SLTU DR:$dx, D0, DR:$dy)>;
def SLT	:		InstAlias<"slt\t$dx = $dy",					(SLTS DR:$dx, DR:$dy, D0)>;
def SGT	:		InstAlias<"sgt\t$dx = $dy",					(SLTS DR:$dx, D0, DR:$dy)>;

// DAG Nodes
//======================================================================================

// TODO

// Instruction Selection Patterns
//======================================================================================

class PatDD<SDPatternOperator OpNode, ObjRISCInstr Inst>
		: Pat<(i32 (OpNode (i32 DR:$dy), (i32 DR:$dz))), (Inst DR:$dy, DR:$dz)>;

def : PatDD<add, ADD>;
def : PatDD<sub, SUB>;
def : PatDD<mul, MUL>;
def : PatDD<sub, AND>;
def : PatDD<sub, OR>;
def : PatDD<sub, XOR>;
def : PatDD<setult, SLTU>;
def : PatDD<setlt, SLTS>;

// TODO: Shifts, Immediate Versions, ANDN, different Multiplications
// TODO: Load, Store, alc
// TODO: Branch, Jump, etc.
