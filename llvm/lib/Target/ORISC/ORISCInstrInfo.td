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

//===----------------------------------------------------------------------===//
// Operands
//===----------------------------------------------------------------------===//

class ORISCOp<ValueType vt = i32> : Operand<vt> {
  let OperandNamespace = "ORISCOp";
}

class ORISCUImmOp<int bitsNum> : ORISCOp {
  let MCOperandPredicate = [{
    int64_t Imm;
    if (!MCOp.evaluateAsConstantImm(Imm))
      return false;
    return isUInt<}]# bitsNum #[{>(Imm);
  }];
}

class ORISCUImmLeafOp<int bitsNum> :
  ORISCUImmOp<bitsNum>, ImmLeaf<i32, "return isUInt<" # bitsNum # ">(Imm);">;

class ORISCSImmOp<int bitsNum> : ORISCOp {
  let MCOperandPredicate = [{
    int64_t Imm;
    if (!MCOp.evaluateAsConstantImm(Imm))
      return false;
    return isInt<}] # bitsNum # [{>(Imm);
  }];
}

class ORISCSImmLeafOp<int bitsNum> :
  ORISCSImmOp<bitsNum>, ImmLeaf<i32, "return isInt<" # bitsNum # ">(Imm);">;

class ORISCScaleImmOp<int bitsNum> : ORISCOp {
  let MCOperandPredicate = [{
    int64_t Imm;
    if (!MCOp.evaluateAsConstantImm(Imm))
      return false;
    return 1 <= Imm && Imm <= (1 <<}] # bitsNum # [{);
  }];
}

class ORISCScaleImmLeafOp<int bitsNum> :
  ORISCSImmOp<bitsNum>, ImmLeaf<i32, "return 1 <= Imm && Imm <= (1 <<" # bitsNum # ");">;

def uimm4  : ORISCUImmLeafOp<4>;
def uimm5  : ORISCUImmLeafOp<5>;
def uimm10 : ORISCUImmLeafOp<10>;
def uimm11 : ORISCUImmLeafOp<11>;
def uimm12 : ORISCUImmLeafOp<12>; 
def uimm15 : ORISCUImmLeafOp<15>;
def uimm20 : ORISCUImmLeafOp<20>;

def simm12 : ORISCSImmLeafOp<12>;
def simm20 : ORISCSImmLeafOp<20>;
def simm25 : ORISCSImmLeafOp<25>;

def scimm3 : ORISCScaleImmLeafOp<3>;


def IKRRISC2BRTargetAsmOperand : AsmOperandClass {
  let Name = "BRTarget";
  let ParserMethod = "parseBRTarget";
  let PredicateMethod = "isImm";
  let RenderMethod = "addImmOperands";
}

def brtarget : Operand<OtherVT> {
  let PrintMethod = "printBranchTarget";
  let EncoderMethod = "getBranchTargetEncoding";
  let DecoderMethod = "decodeBranchOperand";
  let ParserMatchClass = IKRRISC2BRTargetAsmOperand;
}

//===----------------------------------------------------------------------===//
// Instruction Formats
//===----------------------------------------------------------------------===//
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

class ORISCInstr<string opcodestr, dag outs, dag ins, string argstr, InstrFormat fmt>
		: Instruction {
	let Namespace = "ORISC";

	InstrFormat Format = fmt;
	dag OutOperandList = outs;
	dag InOperandList = ins;
	let AsmString = opcodestr # !if(!empty(argstr), "", !if(!lt(!size(opcodestr), 4), "\t", "") # "\t" # argstr);
}

// ALU Instructions
class ORISCInstrRegALU<string opcodestr, bit Commutable = 0>
		: ORISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dy, DR:$dz), "$dx = $dy, $dz", FormatReg0> {

	let isCommutable = Commutable;
}

class ORISCInstrUImmALU<string opcodestr>
		: ORISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dy, uimm12:$ui12), "$dx = $dy, $ui12", FormatALU1> {
}

class ORISCInstrSImmALU<string opcodestr>
		: ORISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dy, simm12:$si12), "$dx = $dy, $si12", FormatALU2> {
}

class ORISCInstrRegShiftImm<string opcodestr>
		: ORISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dy, uimm5:$ui5), "$dx = $dy, $ui5", FormatReg0> {
}

class ORISCInstrRegUnary<string opcodestr>
		: ORISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dz), "$dx = $dz", FormatReg0> {
}

// Attribute Instructions
class ORISCInstrRegAttr<string opcodestr>
		: ORISCInstr<opcodestr, (outs DR:$dy), (ins PR:$px), "$dy = $px", FormatReg0> {
}

// Load/Store Instructions with Immediate Index
class ORISCInstrLoadImmD<string opcodestr>
		: ORISCInstr<opcodestr, (outs DR:$dy), (ins PR:$px, uimm12:$ix12), "$dy = $px[ix12]", FormatLSU8> {

}

class ORISCInstrStoreImmD<string opcodestr>
		: ORISCInstr<opcodestr, (outs), (ins PR:$px, uimm12:$ix12, DR:$dy), "$px[ix12] = $dy", FormatLSU9> {

}

class ORISCInstrLoadImmP<string opcodestr>
		: ORISCInstr<opcodestr, (outs PR:$py), (ins PR:$px, uimm12:$ix12), "$py = $px[ix12]", FormatLSU9> {

}

class ORISCInstrStoreImmP<string opcodestr>
		: ORISCInstr<opcodestr, (outs), (ins PR:$px, uimm12:$ix12, PR:$py), "$px[ix12] = $py", FormatLSU9> {

}

// Load/Store Instructions with Register Index
class ORISCInstrLoadRegD<string opcodestr>
		: ORISCInstr<opcodestr, (outs DR:$dy), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4), "$dy = $px[$dz*$s3+$ud4]", FormatLSUA> {

}

class ORISCInstrStoreRegD<string opcodestr>
		: ORISCInstr<opcodestr, (outs), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4, DR:$dy), "$px[$dz*$s3+$ud4] = $dy", FormatLSUB> {

}

class ORISCInstrLoadRegP<string opcodestr>
		: ORISCInstr<opcodestr, (outs PR:$py), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4), "$py = $px[$dz*$s3+$ud4]", FormatLSUB> {

}

class ORISCInstrStoreRegP<string opcodestr>
		: ORISCInstr<opcodestr, (outs), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4, PR:$py), "$px[$dz*$s3+$ud4] = $py", FormatLSUB> {

}

// Branch Instructions
class ORISCInstrBranchP<string opcodestr>
		: ORISCInstr<opcodestr, (outs), (ins PR:$px, PR:$py, brtarget:$sd12), "$px, $py, $sd12", FormatBPUC> {
}

class ORISCInstrBranchD<string opcodestr>
		: ORISCInstr<opcodestr, (outs), (ins DR:$dy, DR:$dz, brtarget:$sd12), "$dy, $dz, $sd12", FormatBPUC> {
}


class ORISCInstrBranch<string opcodestr>
		: ORISCInstr<opcodestr, (outs), (ins brtarget:$sd25), "$sd25", FormatBPUD> {
	let isBarrier = true;
}

// Dynamic Jump Instructions
class ORISCInstrRegJ<string opcodestr>
		: ORISCInstr<opcodestr, (outs), (ins DR:$dy), "$dy", FormatReg0> {
}

//===----------------------------------------------------------------------===//
// Concrete Instructions
//===----------------------------------------------------------------------===//

// TODO: Eigene Definition für SCP und LCP!? let Constraints = "$py = $py_out"
// TODO: Attribute? isBranch, isCall, isReturn, isTerminator, Defs, Uses, hasSideEffects, mayLoad, mayStore, ...

// ALU Instructions
let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
	def ADD:	ORISCInstrRegALU<"add",    1>;
	def SUB:	ORISCInstrRegALU<"sub",    0>;
	def AND:	ORISCInstrRegALU<"and",    1>;
	def ANDN:	ORISCInstrRegALU<"andn",   1>;
	def OR:		ORISCInstrRegALU<"or",     1>;
	def XOR:	ORISCInstrRegALU<"xor",    1>;
	def SLTU:	ORISCInstrRegALU<"sltu",   0>;
	def SLTS:	ORISCInstrRegALU<"slts",   0>;
	def MUL:	ORISCInstrRegALU<"mul",    1>;
	def MULHS:	ORISCInstrRegALU<"mulhs",  1>;
	def MULHU:	ORISCInstrRegALU<"mulhu",  1>;
	def MULHSU:	ORISCInstrRegALU<"mulhsu", 0>;

	def ADDI:	ORISCInstrUImmALU<"addi">;
	def SUBI:	ORISCInstrUImmALU<"subi">;
	def ANDI:	ORISCInstrUImmALU<"andi">;
	def ANDNI:	ORISCInstrUImmALU<"andni">;
	def ORI:	ORISCInstrUImmALU<"ori">;
	def XORI:	ORISCInstrUImmALU<"xori">;
	def SLTIU:	ORISCInstrUImmALU<"sltui">;
	def SLTIS:	ORISCInstrSImmALU<"sltsi">;
	def MULIU:	ORISCInstrUImmALU<"muliu">;
	def MULIS:	ORISCInstrSImmALU<"mulis">;

	def SRL:	ORISCInstrRegALU<"srl", 0>;
	def SRA:	ORISCInstrRegALU<"sra", 0>;
	def SLL:	ORISCInstrRegALU<"sll", 0>;
	def ROL:	ORISCInstrRegALU<"rol", 0>;
	//def ROR	ORISCInstrRegALU<"ror", 0>;
	def SRLI:	ORISCInstrRegShiftImm<"srli">;
	def SRAI:	ORISCInstrRegShiftImm<"srai">;
	def SLLI:	ORISCInstrRegShiftImm<"slli">;
	def ROLI:	ORISCInstrRegShiftImm<"roli">;

	def NOT:	ORISCInstrRegUnary<"not">;
	def EXTBS:	ORISCInstrRegUnary<"extbs">;
	def EXTHU:	ORISCInstrRegUnary<"exthu">;
	def EXTHS:	ORISCInstrRegUnary<"exths">;
	def CLZ:	ORISCInstrRegUnary<"clz">;

	def LUI:	ORISCInstr<"lui", (outs DR:$dx), (ins uimm20:$ui20), "$dx = $ui20", FormatALU3>;
}

// Attribute Instructions
let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
	def QPI:		ORISCInstrRegAttr<"qpi">;
	def QDTB:		ORISCInstrRegAttr<"qdtb">;
	def QDTH:		ORISCInstrRegAttr<"qdth">;
	def QDTW:		ORISCInstrRegAttr<"qdtw">;
	def QDTD:		ORISCInstrRegAttr<"qdtd">;
}
let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
	def NCHK:		ORISCInstr<"nchk", (outs), (ins PR:$px), "$px", FormatReg0>;

// Pointer Instructions
let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
	def	CPP:	ORISCInstr<"cpp", (outs PR:$py), (ins PR:$px), "$py = $px", FormatReg0>;
	def	GCP:	ORISCInstr<"gcp", (outs PR:$px), (ins), "$px", FormatReg0>;
	def	ALC:	ORISCInstr<"alc", (outs PR:$px), (ins DR:$dy, DR:$dz), "$px = $dy, $dz", FormatReg0>;
	def	ALC_D:	ORISCInstr<"alc", (outs PR:$px), (ins DR:$dy, uimm15:$delta15), "$px = $dy, $delta15", FormatPGU4>;
	def	ALC_P:	ORISCInstr<"alc", (outs PR:$px), (ins uimm11:$pi11, DR:$dz), "$px = $pi11, $dz", FormatPGU5>;
	def	ALC_PD:	ORISCInstr<"alc", (outs PR:$px), (ins uimm11:$pi11, uimm15:$delta15), "$px = $pi11, $delta15", FormatPGU6>;
}

// Stack Instructions
let hasSideEffects = 1, mayLoad = 0, mayStore = 0, Defs = [P30] in {
	def	PUSH:	ORISCInstr<"push", (outs PR:$px), (ins uimm11:$pi11, uimm15:$delta15), "$px = $pi11, $delta15", FormatPGU6>;
	def	PUSHT:	ORISCInstr<"pusht", (outs PR:$px), (ins uimm11:$pi11, uimm15:$delta15), "$px = $pi11, $delta15", FormatPGU5>;
}
let hasSideEffects = 1, mayLoad = 0, mayStore = 0, Uses = [P30] in {
	def	POP:		ORISCInstr<"pop", (outs), (ins), "", FormatReg0>;
}

let hasSideEffects = 1, mayLoad = 0, mayStore = 1, Uses = [P31] in
	def	SVRPC:	ORISCInstr<"sv", (outs), (ins), "rpc", FormatLSUB>;
let hasSideEffects = 1, mayLoad = 1, mayStore = 0, Defs = [P31] in
	def	RSTRPC:	ORISCInstr<"rst", (outs), (ins), "rpc", FormatLSUB>;

// Load/Store Instructions
let mayLoad = 1, mayStore = 0, hasSideEffects = 0 in {
	def LBU_I:	ORISCInstrLoadImmD<"lbu">;
	def LBS_I:	ORISCInstrLoadImmD<"lbs">;
	def LHU_I:	ORISCInstrLoadImmD<"lhu">;
	def LHS_I:	ORISCInstrLoadImmD<"lhs">;
	def LW_I:	ORISCInstrLoadImmD<"lw">;
	def LP_I:	ORISCInstrLoadImmP<"lp">;

	def LBU:	ORISCInstrLoadRegD<"lbu">;
	def LBS:	ORISCInstrLoadRegD<"lbs">;
	def LHU:	ORISCInstrLoadRegD<"lhu">;
	def LHS:	ORISCInstrLoadRegD<"lhs">;
	def LW:		ORISCInstrLoadRegD<"lw">;
	def LP:		ORISCInstrLoadRegP<"lp">;
}

let mayLoad = 0, mayStore = 1, hasSideEffects = 0 in {
	def SB_I:	ORISCInstrStoreImmD<"sb">;
	def SH_I:	ORISCInstrStoreImmD<"sh">;
	def SW_I:	ORISCInstrStoreImmD<"sw">;
	def SP_I:	ORISCInstrStoreImmP<"sp">;

	def SB:		ORISCInstrStoreRegD<"sb">;
	def SH:		ORISCInstrStoreRegD<"sh">;
	def SW:		ORISCInstrStoreRegD<"sw">;
	def SP:		ORISCInstrStoreRegP<"sp">;
}

// Branch and Jump Instructions
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
	def JMP:		ORISCInstrRegJ<"jmp">;
	let Defs = [P31] in
	def JSR:		ORISCInstrRegJ<"jsr">;
	let Defs = [P31] in
	def JLIB:		ORISCInstr<"jlib", (outs), (ins PR:$px, uimm20:$ix20), "$px, $ix20", FormatBPUF>;
}

let isReturn = 1, isTerminator = 1, mayLoad = 0, mayStore = 0, hasSideEffects = 0, Uses = [P31] in
	def	RET:		ORISCInstr<"ret", (outs), (ins), "", FormatReg0>;

let mayLoad = 0, mayStore = 0, hasSideEffects = 0, isBranch = 1, isTerminator = 1 in {
	def BRA:		ORISCInstrBranch<"bra">;
	def	BEQP:		ORISCInstrBranchP<"beqp">;
	def	BNEP:		ORISCInstrBranchP<"bnep">;
	def	BEQ:		ORISCInstrBranchD<"beq">;
	def	BNE:		ORISCInstrBranchD<"bne">;
	def	BGEU:		ORISCInstrBranchD<"bgeu">;
	def	BGES:		ORISCInstrBranchD<"bges">;
	def	BLTU:		ORISCInstrBranchD<"bltu">;
	def	BLTS:		ORISCInstrBranchD<"blts">;

	let Defs = [P31] in
	def BSR:		ORISCInstrBranch<"bsr">;
}

// Other Instructions
let hasSideEffects = 1, mayLoad = 1, mayStore = 1 in
	def	TRAP:		ORISCInstr<"trap", (outs), (ins uimm10:$ui10), "$ui10", FormatReg0>;

// Pseudo Instructions
def NOP: 	InstAlias<"nop",					(ADD D0, D0, D0)>;
def CP: 	InstAlias<"cp\t\t$dx = $dy", 		(ADD DR:$dx, DR:$dy, D0)>;
def LI: 	InstAlias<"li\t\t$dx = $ui12", 		(ADDI DR:$dx, D0, uimm12:$ui12)>;
def LNI: 	InstAlias<"lni\t$dx = $ui12",		(SUB DR:$dx, D0, DR:$dy)>;
def CLR: 	InstAlias<"clr\t$dx",				(ADD DR:$dx, D0, D0)>;
def INC: 	InstAlias<"inc\t$dx",				(ADDI DR:$dx, DR:$dx, 1)>;
def DEC: 	InstAlias<"dec\t$dx",				(SUBI DR:$dx, DR:$dx, 1)>;
def EXTBU:	InstAlias<"extbu\t$dx = $dy",		(ANDI DR:$dx, DR:$dy, 255)>;
def NEG: 	InstAlias<"neg\t$dx = $dy",			(SUB DR:$dx, D0, DR:$dy)>;
def QDT: 	InstAlias<"qdt\t$dy = $px",			(QDTB DR:$dy, PR:$px)>;

def SV_D:	InstAlias<"sv\t\t$dx, $ui12",		(SW_I P30, uimm12:$ui12, DR:$dx)>;
def SV_P:	InstAlias<"sv\t\t$px, $ui12",		(SP_I P30, uimm12:$ui12, PR:$px)>;
def RST_D:	InstAlias<"rst\t$dy, $ui12",		(LW_I DR:$dx, P30, uimm12:$ui12)>;
def RST_P:	InstAlias<"rst\t$py, $ui12",		(LP_I PR:$px, P30, uimm12:$ui12)>;

def MULHUS:	InstAlias<"mulhsu\t$dx = $dy, $dz", (MULHSU DR:$dx, DR:$dz, DR:$dy)>;
def BGTS:	InstAlias<"bgts\t$dy, $dz, $sd12", 	(BLTS DR:$dz, DR:$dy, brtarget:$sd12)>;
def BLES:	InstAlias<"bles\t$dy, $dz, $sd12", 	(BGES DR:$dz, DR:$dy, brtarget:$sd12)>;
def BGTU:	InstAlias<"bgtu\t$dy, $dz, $sd12", 	(BLTU DR:$dz, DR:$dy, brtarget:$sd12)>;
def BLEU:	InstAlias<"bleu\t$dy, $dz, $sd12", 	(BGEU DR:$dz, DR:$dy, brtarget:$sd12)>;

def BEQP_1:	InstAlias<"beqp\t$px, $sd12",		(BEQP PR:$px, P0, brtarget:$sd12)>;
def BNEP_1:	InstAlias<"bnep\t$px, $sd12",		(BNEP PR:$px, P0, brtarget:$sd12)>;
def BEQ_1:	InstAlias<"beq\t$dx, $sd12",		(BEQ  DR:$dx, D0, brtarget:$sd12)>;
def BNE_1:	InstAlias<"bne\t$dx, $sd12",		(BNE  DR:$dx, D0, brtarget:$sd12)>;
def BLE_1:	InstAlias<"ble\t$dx, $sd12",		(BGES D0, DR:$dx, brtarget:$sd12)>;
def BGE_1:	InstAlias<"bge\t$dx, $sd12",		(BGES DR:$dx, D0, brtarget:$sd12)>;
def BLT_1:	InstAlias<"blt\t$dx, $sd12",		(BLTS D0, DR:$dx, brtarget:$sd12)>;
def BGT_1:	InstAlias<"bgt\t$dx, $sd12",		(BLTS DR:$dx, D0, brtarget:$sd12)>;

// TODO: multiclass benutzen?
def	ADDI_1:	InstAlias<"addi\t$dx = $ui12",		(ADDI  DR:$dx, DR:$dx, uimm12:$ui12)>;
def	SUBI_1:	InstAlias<"subi\t$dx = $ui12",		(SUBI  DR:$dx, DR:$dx, uimm12:$ui12)>;
def	ANDI_1:	InstAlias<"andi\t$dx = $ui12",		(ANDI  DR:$dx, DR:$dx, uimm12:$ui12)>;
def	ANDNI_1:InstAlias<"andni\t$dx = $ui12",		(ANDNI DR:$dx, DR:$dx, uimm12:$ui12)>;
def	ORI_1:	InstAlias<"ori\t$dx = $ui12",		(ORI   DR:$dx, DR:$dx, uimm12:$ui12)>;
def	XORI_1:	InstAlias<"xori\t$dx = $ui12",		(XORI  DR:$dx, DR:$dx, uimm12:$ui12)>;
def	MULIU_1:InstAlias<"muliu\t$dx = $ui12",		(MULIU DR:$dx, DR:$dx, uimm12:$ui12)>;
def	MULIS_1:InstAlias<"mulis\t$dx = $ui12",		(MULIS DR:$dx, DR:$dx, uimm12:$ui12)>;
def	SRLI_1:	InstAlias<"srli\t$dx = $ui5",		(SRLI  DR:$dx, DR:$dx, uimm5:$ui5)>;
def	SRAI_1:	InstAlias<"srai\t$dx = $ui5",		(SRAI  DR:$dx, DR:$dx, uimm5:$ui5)>;
def	SLLI_1:	InstAlias<"slli\t$dx = $ui5",		(SLLI  DR:$dx, DR:$dx, uimm5:$ui5)>;

def	ADD_1:	InstAlias<"addi\t$dx = $dz",		(ADD  DR:$dx, DR:$dx, DR:$dz)>;
def	SUB_1:	InstAlias<"subi\t$dx = $dz",		(SUB  DR:$dx, DR:$dx, DR:$dz)>;
def	AND_1:	InstAlias<"andi\t$dx = $dz",		(AND  DR:$dx, DR:$dx, DR:$dz)>;
def	ANDN_1:	InstAlias<"andni\t$dx = $dz",		(ANDN DR:$dx, DR:$dx, DR:$dz)>;
def	OR_1:	InstAlias<"ori\t$dx = $dz",			(OR   DR:$dx, DR:$dx, DR:$dz)>;
def	XOR_1:	InstAlias<"xori\t$dx = $dz",		(XOR  DR:$dx, DR:$dx, DR:$dz)>;
def	MUL_1:	InstAlias<"muliu\t$dx = $dz",		(MUL  DR:$dx, DR:$dx, DR:$dz)>;
def	SRL_1:	InstAlias<"srli\t$dx = $dz",		(SRL  DR:$dx, DR:$dx, DR:$dz)>;
def	SRA_1:	InstAlias<"srai\t$dx = $dz",		(SRA  DR:$dx, DR:$dx, DR:$dz)>;
def	SLL_1:	InstAlias<"slli\t$dx = $dz",		(SLL  DR:$dx, DR:$dx, DR:$dz)>;

def SEQ	:	InstAlias<"seq\t$dx = $dy",			(SLTIU DR:$dx, DR:$dy, 1)>;
def SNE	:	InstAlias<"sne\t$dx = $dy",			(SLTU DR:$dx,  D0, DR:$dy)>;
def SLT	:	InstAlias<"slt\t$dx = $dy",			(SLTS DR:$dx,  DR:$dy, D0)>;
def SGT	:	InstAlias<"sgt\t$dx = $dy",			(SLTS DR:$dx,  D0, DR:$dy)>;

//===----------------------------------------------------------------------===//
// DAG Nodes
//===----------------------------------------------------------------------===//

//TODO: ADD ORISCISD::LOAD and ORISCISD:STORE

def SDT_ORISC_IStore : SDTypeProfile<0, 3, [       // indexed store
  SDTCisInt<0>, SDTCisVT<1, orisc_pointer>, SDTCisInt<2>
]>;

def SDT_ORISC_ILoad : SDTypeProfile<1, 2, [       // indexed load
  SDTCisInt<0>, SDTCisVT<1, orisc_pointer>, SDTCisInt<2>
]>;

def orisc_ild  : SDNode<"ISD::LOAD"       , SDT_ORISC_ILoad,
                        [SDNPHasChain, SDNPMayLoad, SDNPMemOperand]>;
def orisc_pre_ild : PatFrag<(ops       node:$base, node:$offset),
                            (orisc_ild node:$base, node:$offset), [{
	ISD::MemIndexedMode AM = cast<LoadSDNode>(N)->getAddressingMode();
	return AM == ISD::PRE_INC || AM == ISD::PRE_DEC;
}]>;
class orisc_sextiload<ValueType MemVT>
		: PatFrag<(ops node:$base, node:$offset), (orisc_pre_ild node:$base, node:$offset)> {
  	let IsLoad = true;
  	let IsSignExtLoad = true;
	let ScalarMemoryVT = MemVT;
}
class orisc_zextiload<ValueType MemVT>
		: PatFrag<(ops node:$base, node:$offset), (orisc_pre_ild node:$base, node:$offset)> {
	let IsLoad = true;
	let IsZeroExtLoad = true;
	let ScalarMemoryVT = MemVT;
}
def orisc_loadi8u  : orisc_zextiload<i8>;
def orisc_loadi16u : orisc_zextiload<i16>;
def orisc_loadi32u : orisc_zextiload<i32>;
def orisc_loadi8s  : orisc_sextiload<i8>;
def orisc_loadi16s : orisc_sextiload<i16>;
def orisc_loadi32s : orisc_sextiload<i32>;

def orisc_ist  : SDNode<"ISD::STORE"      , SDT_ORISC_IStore,
                        [SDNPHasChain, SDNPMayStore, SDNPMemOperand]>;
class orisc_pre_truncstore<ValueType MemVT> : PatFrag<(ops     node:$val, node:$base, node:$offset),
                                  					(orisc_ist node:$val, node:$base, node:$offset), [{
  ISD::MemIndexedMode AM = cast<StoreSDNode>(N)->getAddressingMode();
  return AM == ISD::PRE_INC || AM == ISD::PRE_DEC;
}]> {
	let IsStore = true;
	let IsTruncStore = true;
	let MemoryVT = MemVT;
}
def orisc_storei8  : orisc_pre_truncstore<i8>;
def orisc_storei16 : orisc_pre_truncstore<i16>;
def orisc_storei32 : orisc_pre_truncstore<i32>;

def orisc_ret  : SDNode<"ORISCISD::RET", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

//===----------------------------------------------------------------------===//
// Instruction Selection Patterns
//===----------------------------------------------------------------------===//

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


class PatStoreD<SDPatternOperator OpNode, ORISCInstr Inst>
		: Pat<(OpNode DR:$dy, PR:$px, DR:$dz), (Inst PR:$px, DR:$dz, 1, 0, DR:$dy)>;
class PatStoreI<SDPatternOperator OpNode, ORISCInstr Inst>
		: Pat<(OpNode DR:$dy, PR:$px, uimm12:$ix12), (Inst PR:$px, uimm12:$ix12, DR:$dy)>;
class PatLoadDS<SDPatternOperator OpNode, ORISCInstr Inst>
		: Pat<(OpNode PR:$px, DR:$dz), (Inst PR:$px, DR:$dz, 1, 0)>;
class PatLoadIS<SDPatternOperator OpNode, ORISCInstr Inst>
		: Pat<(OpNode PR:$px, uimm12:$ix12), (Inst PR:$px, uimm12:$ix12)>;
class PatLoadDU<SDPatternOperator OpNode, ORISCInstr Inst>
		: Pat<(OpNode PR:$px, DR:$dz), (Inst PR:$px, DR:$dz, 1, 0)>;
class PatLoadIU<SDPatternOperator OpNode, ORISCInstr Inst>
		: Pat<(OpNode PR:$px, uimm12:$ix12), (Inst PR:$px, uimm12:$ix12)>;

def : PatStoreD<orisc_storei8,  SB>;
def : PatStoreD<orisc_storei16, SH>;
def : PatStoreD<orisc_storei32, SW>;
def : PatStoreI<orisc_storei8,  SB_I>;
def : PatStoreI<orisc_storei16, SH_I>;
def : PatStoreI<orisc_storei32, SW_I>;

def : PatLoadDS<orisc_loadi8s,  LBS>;
def : PatLoadDU<orisc_loadi8u,  LBU>;
def : PatLoadIS<orisc_loadi8s,  LBS_I>;
def : PatLoadIU<orisc_loadi8u,  LBU_I>;
def : PatLoadDS<orisc_loadi16s, LHS>;
def : PatLoadDU<orisc_loadi16u, LHU>;
def : PatLoadIS<orisc_loadi16s, LHS_I>;
def : PatLoadIU<orisc_loadi16u, LHU_I>;
def : PatLoadDS<orisc_loadi32s, LW>;
def : PatLoadIS<orisc_loadi32s, LW_I>;


class PatBranch<CondCode Cond, ORISCInstr Inst>
		: Pat<(brcc Cond, DR:$dy, DR:$dz, bb:$sd12), (Inst DR:$dy, DR:$dz, brtarget:$sd12)>;

def : PatBranch<SETEQ,  BEQ>;
def : PatBranch<SETNE,  BNE>;
def : PatBranch<SETUGE, BGEU>;
def : PatBranch<SETGE,  BGES>;
def : PatBranch<SETULT, BLTU>;
def : PatBranch<SETLT,  BLTS>;

def : Pat<(brcc SETUGT, DR:$px, DR:$py, bb:$sd12), (BLTU DR:$py, DR:$px, brtarget:$sd12)>;
def : Pat<(brcc SETGT,  DR:$px, DR:$py, bb:$sd12), (BLTS DR:$py, DR:$px, brtarget:$sd12)>;
def : Pat<(brcc SETULE, DR:$px, DR:$py, bb:$sd12), (BGEU DR:$py, DR:$px, brtarget:$sd12)>;
def : Pat<(brcc SETLE,  DR:$px, DR:$py, bb:$sd12), (BGES DR:$py, DR:$px, brtarget:$sd12)>;

def : Pat<(brcc SETEQ, PR:$px, PR:$py, bb:$sd12), (BEQP PR:$px, PR:$py, brtarget:$sd12)>;
def : Pat<(brcc SETNE, PR:$px, PR:$py, bb:$sd12), (BNEP PR:$px, PR:$py, brtarget:$sd12)>;

def : Pat<(br bb:$sd25), (BRA brtarget:$sd25)>;


def : Pat<(orisc_ret), (RET)>;
def : Pat<(uimm12:$ui12), (ADDI D0, uimm12:$ui12)>;
// TODO: Shifts, Immediate Versions, ANDN, different Multiplications
// TODO: Load, Store, alc
// TODO: Branch, Jump, etc.
