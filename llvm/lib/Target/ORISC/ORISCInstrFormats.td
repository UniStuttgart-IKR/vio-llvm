//===-- ObjectiveRISCInstrFormats.td - Target Description for Objective-RISC --*- tablegen -*-===//
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
def uimm9  : ORISCUImmLeafOp<9>;
def uimm10 : ORISCUImmLeafOp<10>;
def uimm11 : ORISCUImmLeafOp<11>;
def uimm12 : ORISCUImmLeafOp<12>; 
def uimm13 : ORISCUImmLeafOp<13>; 
def uimm14 : ORISCUImmLeafOp<14>; 
def uimm15 : ORISCUImmLeafOp<15>;
def uimm20 : ORISCUImmLeafOp<20>;

def simm12 : ORISCSImmLeafOp<12>;
def simm20 : ORISCSImmLeafOp<20>;
def simm25 : ORISCSImmLeafOp<25>;

def scimm3 : ORISCScaleImmLeafOp<3>;

def ORISCBRTargetAsmOperand : AsmOperandClass {
  let Name = "BRTarget";
  let ParserMethod = "parseBRTarget";
  let PredicateMethod = "isImm";
  let RenderMethod = "addImmOperands";
}

def brtarget12 : Operand<OtherVT> {
  let PrintMethod = "printSymbol";
  let EncoderMethod = "getBranchTargetEncoding";
  let DecoderMethod = "decodeBranch12Operand";
}

def brtarget25 : Operand<OtherVT> {
  let PrintMethod = "printSymbol";
  let EncoderMethod = "getBranchTargetEncoding";
  let DecoderMethod = "decodeBranch25Operand";
}

def ExternalSymbol : AsmOperandClass {
  let Name = "ExternalSymbol";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidCallSymbol";
  let DiagnosticString = "operand must be a bare symbol name";
}

// A bare symbol used in call/tail only.
def call_symbol : Operand<iPTR> {
  let ParserMatchClass = ExternalSymbol;
}

def  subroutine_symbol : Operand<iPTR> {
  let PrintMethod = "printSymbol";
  let EncoderMethod = "getBranchTargetEncoding";
  let DecoderMethod = "decodeBranch25Operand";
  let ParserMatchClass = ExternalSymbol;
}

def  external_symbol : Operand<iPTR> {
  let PrintMethod = "printSymbol";
  let EncoderMethod = "getExternalSymbolEncoding";
  let DecoderMethod = "decodeJlibIndexOperand";
  let ParserMatchClass = ExternalSymbol;
}

def to_tframeindex : SDNodeXForm<frameindex, [{
  return CurDAG->getTargetFrameIndex(N->getIndex(), N->getValueType(0));
}]>;


def GetWordsIndex : SDNodeXForm<imm, [{
	return CurDAG->getTargetConstant((N->getZExtValue() / 4), SDLoc(N),
                                    N->getValueType(0));
}]>;
def GetHalfsIndex : SDNodeXForm<imm, [{
	return CurDAG->getTargetConstant((N->getZExtValue() / 2), SDLoc(N),
                                    N->getValueType(0));
}]>;

def Upper20 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(((N->getZExtValue() >> 12) & 0xFFFFF), SDLoc(N),
                                     N->getValueType(0));
}]>;

def Lower12 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant((N->getZExtValue() & 0xFFF), SDLoc(N),
                                    N->getValueType(0));
}]>;

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

def FormatPseudo : InstrFormat;

class ORISCInstr<string opcodestr, dag outs, dag ins, string argstr, InstrFormat fmt>
		: Instruction {
	let Namespace = "ORISC";
	
  	field bits<32> Inst;
	let Inst{31-29} = 0;
  	let Size = 4;

	InstrFormat Format = fmt;
	dag OutOperandList = outs;
	dag InOperandList = ins;
	let AsmString = opcodestr # !if(!empty(argstr), "", !if(!lt(!size(opcodestr), 4), "\t", "") # "\t" # argstr);
}


// REG0 Instructions
class ORISCInstrReg0<string opcodestr, dag outs, dag ins, bits<4> h4, bits<3> g3, string argstr>
		: ORISCInstr<opcodestr, outs, ins, argstr, FormatReg0> {
	bits<5> x;
	bits<5> y;
	bits<5> z;

	let Inst{28-25} = 0;
	let Inst{24-20} = x;
	let Inst{19-16} = h4;
	let Inst{15-13} = 0;
	let Inst{12-8} = z;
	let Inst{7-3} = y;
	let Inst{2-0} = g3;
}

class ORISCInstrReg0AluX<string opcodestr, bits<4> h4, bits<3> g3, bit commutable>
		: ORISCInstrReg0<opcodestr, (outs DR:$dx), (ins DR:$dy, DR:$dz), h4, g3, "$dx = $dy, $dz"> {
	bits<5> dx;
	bits<5> dy;
	bits<5> dz;
	let x = dx;
	let y = dy;
	let z = dz;
	let isCommutable = commutable;
}

class ORISCInstrReg0Alu0<string opcodestr, bits<3> g3, bit commutable = 0>
		: ORISCInstrReg0AluX<opcodestr, 0, g3, commutable>;

class ORISCInstrReg0Alu1<string opcodestr, bits<3> g3, bit commutable = 0>
		: ORISCInstrReg0AluX<opcodestr, 1, g3, commutable>;

class ORISCInstrReg0Alu2<string opcodestr, bits<3> g3, bit commutable = 0>
		: ORISCInstrReg0AluX<opcodestr, 2, g3, commutable>;

class ORISCInstrReg0Alu4<string opcodestr, bits<3> g3, bit commutable = 0>
		: ORISCInstrReg0AluX<opcodestr, 4, g3, commutable>;

class ORISCInstrReg0Alu5<string opcodestr, bits<3> g3>
		: ORISCInstrReg0<opcodestr, (outs DR:$dx), (ins DR:$dy, uimm5:$ui5), 5, g3, "$dx = $dy, $ui5"> {
	bits<5> dx;
	bits<5> dy;
	bits<5> ui5;
	let x = dx;
	let y = dy;
	let z = ui5;
}

class ORISCInstrReg0Alu7<string opcodestr, bits<3> g3>
		: ORISCInstrReg0<opcodestr, (outs DR:$dx), (ins DR:$dz), 7, g3, "$dx = $dz"> {
	bits<5> dx;
	bits<5> dz;
	let x = dx;
	let y = 0;
	let z = dz;
}

class ORISCInstrReg0Alu72<string opcodestr, bits<3> g3>
		: ORISCInstrReg0<opcodestr, (outs), (ins uimm10:$ui10), 7, g3, "$ui10"> {
	bits<10> ui10;
	let x = 0;
	let y = ui10{4-0};
	let z = ui10{9-5};
}

class ORISCInstrReg0AtuB<string opcodestr, bits<3> g3>
		: ORISCInstrReg0<opcodestr, (outs DR:$dy), (ins PR:$px), 11, g3, "$dy = $px"> {
	bits<5> px;
	bits<5> dy;
	let x = px;
	let y = dy;
	let z = 0;
}

class ORISCInstrReg0AtuC<string opcodestr, bits<3> g3, string argstr = "$dz = $px, $py">
		: ORISCInstrReg0<opcodestr, (outs DR:$dz), (ins PR:$px, PR:$py), 12, g3, argstr> {
	bits<5> px;
	bits<5> py;
	bits<5> dz;
	let x = px;
	let y = py;
	let z = dz;
}

class ORISCInstrReg0PguD<string opcodestr, bits<3> g3, string argstr = "$py = $px">
		: ORISCInstrReg0<opcodestr, (outs PR:$py), (ins PR:$px), 13, g3, argstr> {
	bits<5> px;
	bits<5> py;
	let x = px;
	let y = py;
	let z = 0;
}

class ORISCInstrReg0PguE<string opcodestr, bits<3> g3, string argstr = "$px = $dy,$dz">
		: ORISCInstrReg0<opcodestr, (outs PR:$px), (ins DR:$dy, DR:$dz), 14, g3, argstr> {
	bits<5> px;
	bits<5> dy;
	bits<5> dz;
	let x = px;
	let y = dy;
	let z = dz;
}

class ORISCInstrGCP<string opcodestr>
		: ORISCInstrReg0<opcodestr, (outs PR:$py), (ins), 14, 4, "ctxt"> {
	let x = 29;
	let y = 0;
	let z = 0;
}

class ORISCInstrReg0PguE3<string opcodestr>
		: ORISCInstrReg0<opcodestr, (outs), (ins), 14, 5, "frame"> {
	let x = 30;
	let y = 0;
	let z = 0;
}

class ORISCInstrReg0BpuF<string opcodestr, bits<3> g3, string argstr = "$dy">
		: ORISCInstrReg0<opcodestr, (outs), (ins DR:$dy), 15, g3, argstr> {
	bits<5> dy;
	let x = 0;
	let y = dy;
	let z = 0;
}

class ORISCInstrReg0BpuF2<string opcodestr>
		: ORISCInstrReg0<opcodestr, (outs), (ins), 15, 3, ""> {
	let x = 0;
	let y = 31;
	let z = 0;
}


//ALUX Instructions
class ORISCInstrAlu12U<string opcodestr, bits<4> f4, bits<3> g3>
		: ORISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dy, uimm12:$ui12), "$dx = $dy,$ui12", FormatALU1> {
	bits<5> dx;
	bits<5> dy;
	bits<12> ui12;

	let Inst{28-25} = f4;
	let Inst{24-20} = dx;
	let Inst{19-8} = ui12;
	let Inst{7-3} = dy;
	let Inst{2-0} = g3;
}

class ORISCInstrAlu12S<string opcodestr, bits<4> f4, bits<3> g3>
		: ORISCInstr<opcodestr, (outs DR:$dx), (ins DR:$dy, simm12:$si12), "$dx = $dy,$si12", FormatALU1> {
	bits<5> dx;
	bits<5> dy;
	bits<12> si12;

	let Inst{28-25} = f4;
	let Inst{24-20} = dx;
	let Inst{19-8} = si12;
	let Inst{7-3} = dy;
	let Inst{2-0} = g3;
}

class ORISCInstrAlu3<string opcodestr>
		: ORISCInstr<opcodestr, (outs DR:$dx), (ins uimm20:$ui20), "$dx = $ui20", FormatALU3> {
	bits<5> dx;
	bits<20> ui20;

	let Inst{28-25} = 3;
	let Inst{24-20} = dx;
	let Inst{19-0} = ui20;
}


//PGUX Instructions

class ORISCInstrPgu4<string opcodestr>
		: ORISCInstr<opcodestr, (outs PR:$px), (ins DR:$dy, uimm15:$delta15), "$px = $dy, $delta15", FormatPGU4> {
	bits<5> px;
	bits<5> dy;
	bits<15> delta15;

	let Inst{28-25} = 4;
	let Inst{24-20} = px;
	let Inst{19-8} = delta15{14-3};
	let Inst{7-3} = dy;
	let Inst{2-0} = delta15{2-0};
}

class ORISCInstrPgu51<string opcodestr>
		: ORISCInstr<opcodestr, (outs PRF:$px), (ins uimm15:$pi15, DR:$dz), "$px = $pi15, $dz", FormatPGU5> {
	bits<5> px;
	bits<5> dz;
	bits<15> pi15;

	let Inst{28-25} = 5;
	let Inst{24-20} = px;
	let Inst{19-13} = pi15{14-8};
	let Inst{12-8} = dz;
	let Inst{7-0} = pi15{7-0};
}

class ORISCInstrPgu52<string opcodestr>
		: ORISCInstr<opcodestr, (outs F:$px), (ins uimm9:$pi9, uimm11:$delta11), "$px = $pi9, $delta11", FormatPGU5> {
	bits<9> pi9;
	bits<11> delta11;
	let DecoderMethod = "DecodePush";

	let Inst{28-25} = 5;
	let Inst{24-20} = 30;
	let Inst{19-11} = pi9;
	let Inst{10-0} = delta11;
}

class ORISCInstrPgu61<string opcodestr>
		: ORISCInstr<opcodestr, (outs PRF:$px), (ins uimm9:$pi9, uimm11:$delta11), "$px = $pi9, $delta11", FormatPGU5> {
	bits<5> px;
	bits<9> pi9;
	bits<11> delta11;

	let Inst{28-25} = 6;
	let Inst{24-20} = px;
	let Inst{19-11} = pi9;
	let Inst{10-0} = delta11;
}

class ORISCInstrPgu62<string opcodestr>
		: ORISCInstr<opcodestr, (outs F:$px), (ins uimm9:$pi9, uimm11:$delta11), "$px = $pi9, $delta11", FormatPGU5> {
	bits<9> pi9;
	bits<11> delta11;
	let DecoderMethod = "DecodePush";

	let Inst{28-25} = 6;
	let Inst{24-20} = 30;
	let Inst{19-11} = pi9;
	let Inst{10-0} = delta11;
}


//LSUX Instructions

class ORISCInstrLsu8<string opcodestr, bits<3> g3>
		: ORISCInstr<opcodestr, (outs DR:$dy), (ins PR:$px, uimm12:$ix12), "$dy = $px[$ix12]", FormatLSU8> {
	bits<5> px;
	bits<5> dy;
	bits<12> ix12;
	
	let Inst{28-25} = 8;
	let Inst{24-20} = px;
	let Inst{19-8} = ix12;
	let Inst{7-3} = dy;
	let Inst{2-0} = g3;
}

class ORISCInstrLsu9<string opcodestr, bits<3> g3, dag outs, dag ins, string argstr>
		: ORISCInstr<opcodestr, outs, ins, argstr, FormatLSU9> {
	let Inst{28-25} = 9;
	let Inst{2-0} = g3;
}

class ORISCInstrLsu91<string opcodestr, bits<3> g3>
		: ORISCInstrLsu9<opcodestr, g3, (outs), (ins PR:$px, uimm12:$ix12, DR:$dy), "$px[$ix12] = $dy"> {
	bits<5> px;
	bits<5> dy;
	bits<12> ix12;
	let Inst{24-20} = px;
	let Inst{19-8} = ix12;
	let Inst{7-3} = dy;
}

class ORISCInstrLsu92<string opcodestr, bits<3> g3>
		: ORISCInstrLsu9<opcodestr, g3, (outs PR:$py), (ins PR:$px, uimm12:$ix12), "$py = $px[$ix12]"> {
	bits<5> px;
	bits<5> py;
	bits<12> ix12;
	let Inst{24-20} = px;
	let Inst{19-8} = ix12;
	let Inst{7-3} = py;
}

class ORISCInstrLsu93<string opcodestr, bits<3> g3>
		: ORISCInstrLsu9<opcodestr, g3, (outs), (ins PR:$px, uimm12:$ix12, PR:$py), "$px[$ix12] = $py"> {
	bits<5> px;
	bits<5> py;
	bits<12> ix12;
	let Inst{24-20} = px;
	let Inst{19-8} = ix12;
	let Inst{7-3} = py;
}

class ORISCInstrLsuA<string opcodestr, bits<3> g3>
		: ORISCInstr<opcodestr, (outs DR:$dy), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4), "$dy = $px[$dz*$s3+$ud4]", FormatLSUA> {
	bits<5> px;
	bits<5> dy;
	bits<5> dz;
	bits<3> s3;
	bits<4> ud4;
	let Inst{28-25} = 10;
	let Inst{24-20} = px;
	let Inst{19-16} = ud4;
	let Inst{15-13} = s3;
	let Inst{12-8} = dz;
	let Inst{7-3} = dy;
	let Inst{2-0} = g3;
}

class ORISCInstrLsuB1<string opcodestr, bits<3> g3>
		: ORISCInstr<opcodestr, (outs), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4, DR:$dy), "$px[$dz*$s3+$ud4] = $dy", FormatLSUB> {
	bits<5> px;
	bits<5> dy;
	bits<5> dz;
	bits<3> s3;
	bits<4> ud4;
	let Inst{28-25} = 11;
	let Inst{24-20} = px;
	let Inst{19-16} = ud4;
	let Inst{15-13} = s3;
	let Inst{12-8} = dz;
	let Inst{7-3} = dy;
	let Inst{2-0} = g3;
}

class ORISCInstrLsuB2<string opcodestr, bits<3> g3>
		: ORISCInstr<opcodestr, (outs PR:$py), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4), "$py = $px[$dz*$s3+$ud4]", FormatLSUB> {
	bits<5> px;
	bits<5> py;
	bits<5> dz;
	bits<3> s3;
	bits<4> ud4;
	let Inst{28-25} = 11;
	let Inst{24-20} = px;
	let Inst{19-16} = ud4;
	let Inst{15-13} = s3;
	let Inst{12-8} = dz;
	let Inst{7-3} = py;
	let Inst{2-0} = g3;
}

class ORISCInstrLsuB3<string opcodestr, bits<3> g3>
		: ORISCInstr<opcodestr, (outs), (ins PR:$px, DR:$dz, scimm3:$s3, uimm4:$ud4, PR:$py), "$px[$dz*$s3+$ud4] = $py", FormatLSUB> {
	bits<5> px;
	bits<5> py;
	bits<5> dz;
	bits<3> s3;
	bits<4> ud4;
	let Inst{28-25} = 11;
	let Inst{24-20} = px;
	let Inst{19-16} = ud4;
	let Inst{15-13} = s3;
	let Inst{12-8} = dz;
	let Inst{7-3} = py;
	let Inst{2-0} = g3;
}

class ORISCInstrSVRSTRPC<string opcodestr, bits<3> g3>
		: ORISCInstr<opcodestr, (outs), (ins), "rpc", FormatLSUB> {
	let Inst{28-25} = 11;
	let Inst{24-20} = 30;
	let Inst{19-8} = 0;
	let Inst{7-3} = 31;
	let Inst{2-0} = g3;
}


// Branch Instructions

class ORISCInstrBpuC1<string opcodestr, bits<3> g3>
		: ORISCInstr<opcodestr, (outs), (ins PR:$px, PR:$py, brtarget12:$sd12), "$px, $py, $sd12", FormatBPUC> {
	bits<5> px;
	bits<5> py;
	bits<12> sd12;
	
	let Inst{28-25} = 12;
	let Inst{24-20} = px;
	let Inst{19-8} = sd12;
	let Inst{7-3} = py;
	let Inst{2-0} = g3;
}

class ORISCInstrBpuC2<string opcodestr, bits<3> g3>
		: ORISCInstr<opcodestr, (outs), (ins DR:$dy, DR:$dz, brtarget12:$sd12), "$dy, $dz, $sd12", FormatBPUC> {
	bits<5> dy;
	bits<5> dz;
	bits<12> sd12;

	let Inst{28-25} = 12;
	let Inst{24-20} = sd12{11-7};
	let Inst{19-13} = sd12{6-0};
	let Inst{12-8} = dz;
	let Inst{7-3} = dy;
	let Inst{2-0} = g3;
}


class ORISCInstrBpuD<string opcodestr>
		: ORISCInstr<opcodestr, (outs), (ins brtarget25:$sd25), "$sd25", FormatBPUD> {
	bits<25> sd25;

	let Pattern = [(br bb:$sd25)];
	let isBarrier = true;
	let Inst{28-25} = 13;
	let Inst{24-0} = sd25;
}

class ORISCInstrBpuE<string opcodestr>
		: ORISCInstr<opcodestr, (outs), (ins subroutine_symbol:$sd25), "$sd25", FormatBPUD> {
	bits<25> sd25;

	let isBarrier = true;
	let isCall = true;
	let Inst{28-25} = 14;
	let Inst{24-0} = sd25;
}

class ORISCInstrBpuF<string opcodestr>
		: ORISCInstr<opcodestr, (outs), (ins PRFN:$px, external_symbol:$symbol), "$px, $symbol", FormatBPUF> {
	bits<5> px;
	bits<20> symbol;

	let isBarrier = true;
	let isCall = true;
	let Inst{28-25} = 15;
	let Inst{24-20} = px;
	let Inst{19-0} = symbol;
}


//===----------------------------------------------------------------------===//
// Concrete Instructions
//===----------------------------------------------------------------------===//

// TODO: Eigene Definition für SCP und LCP!? let Constraints = "$py = $py_out"
// TODO: Attribute? isBranch, isCall, isReturn, isTerminator, Defs, Uses, hasSideEffects, mayLoad, mayStore, ...

// ALU Instructions
let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
	def ADD:	ORISCInstrReg0Alu0<"add",    0, 1>;
	def SUB:	ORISCInstrReg0Alu0<"sub",    1, 0>;
	def AND:	ORISCInstrReg0Alu0<"and",    4, 1>;
	def ANDN:	ORISCInstrReg0Alu0<"andn",   5, 1>;
	def OR:		ORISCInstrReg0Alu0<"or",     6, 1>;
	def XOR:	ORISCInstrReg0Alu0<"xor",    7, 1>;
	def SLTU:	ORISCInstrReg0Alu1<"sltu",   0, 0>;
	def SLTS:	ORISCInstrReg0Alu1<"slts",   1, 0>;
	def MUL:	ORISCInstrReg0Alu2<"mul",    0, 1>;
	def MULHS:	ORISCInstrReg0Alu2<"mulhs",  1, 1>;
	def MULHU:	ORISCInstrReg0Alu2<"mulhu",  2, 1>;
	def MULHSU:	ORISCInstrReg0Alu2<"mulhsu", 3, 0>;
	def SRL:	ORISCInstrReg0Alu4<"srl", 	 0, 0>;
	def SRA:	ORISCInstrReg0Alu4<"sra", 	 1, 0>;
	def SLL:	ORISCInstrReg0Alu4<"sll", 	 2, 0>;
	def ROL:	ORISCInstrReg0Alu4<"rol", 	 3, 0>;
	//def ROR	ORISCInstrReg0Alu4<"ror", 	 4, 0>;

	def SRLI:	ORISCInstrReg0Alu5<"srli",  0>;
	def SRAI:	ORISCInstrReg0Alu5<"srai",  1>;
	def SLLI:	ORISCInstrReg0Alu5<"slli",  2>;
	def ROLI:	ORISCInstrReg0Alu5<"roli",  3>;
	def NOT:	ORISCInstrReg0Alu7<"not",   0>;
	def EXTBS:	ORISCInstrReg0Alu7<"extbs", 1>;
	def EXTHU:	ORISCInstrReg0Alu7<"exthu", 2>;
	def EXTHS:	ORISCInstrReg0Alu7<"exths", 3>;
	def CLZ:	ORISCInstrReg0Alu7<"clz",   4>;
	
	def QPI:	ORISCInstrReg0AtuB<"qpi",  0>;
	def QDTB:	ORISCInstrReg0AtuB<"qdtb", 4>;
	def QDTH:	ORISCInstrReg0AtuB<"qdth", 5>;
	def QDTW:	ORISCInstrReg0AtuB<"qdtw", 6>;
	def QDTD:	ORISCInstrReg0AtuB<"qdtd", 7>;

	def	CPP:	ORISCInstrReg0PguD<"cpp", 0>;

	def ADDI:	ORISCInstrAlu12U<"addi",  1, 0>;
	def SUBI:	ORISCInstrAlu12U<"subi",  1, 1>;
	def ANDI:	ORISCInstrAlu12U<"andi",  1, 4>;
	def ANDNI:	ORISCInstrAlu12U<"andni", 1, 5>;
	def ORI:	ORISCInstrAlu12U<"ori",   1, 6>;
	def XORI:	ORISCInstrAlu12U<"xori",  1, 7>;
	def SLTIU:	ORISCInstrAlu12U<"sltui", 2, 0>;
	def SLTIS:	ORISCInstrAlu12S<"sltsi", 2, 1>;
	def MULIU:	ORISCInstrAlu12U<"muliu", 2, 4>;
	def MULIS:	ORISCInstrAlu12S<"mulis", 2, 5>;

	def LUI:	ORISCInstrAlu3<"lui">;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0, Defs = [P29] in {
	def	GCP:	ORISCInstrGCP<"gcp">;
}

let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
	def NCHK:		ORISCInstrReg0AtuC<"nchk", 0>;

// Stack Instructions
let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
	def	PUSH:	ORISCInstrPgu62<"push">;
	def	PUSHT:	ORISCInstrPgu52<"pusht">;
}
let hasSideEffects = 1, mayLoad = 0, mayStore = 0, Uses = [P30] in {
	def	POP:		ORISCInstrReg0PguE3<"pop">;
}

// Alc Instructions
let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
	def	ALC:	ORISCInstrReg0PguE<"alc", 0>;
	def	ALC_D:	ORISCInstrPgu4<"alc">;
	def	ALC_P:	ORISCInstrPgu51<"alc">;
	def	ALC_PD:	ORISCInstrPgu61<"alc">;
}

let hasSideEffects = 1, mayLoad = 0, mayStore = 1, Uses = [P30, P31] in
	def	SVRPC:	ORISCInstrSVRSTRPC<"sv", 4>;
let hasSideEffects = 1, mayLoad = 1, mayStore = 0, Uses = [P30], Defs = [P31] in
	def	RSTRPC:	ORISCInstrSVRSTRPC<"rst", 6>;

// Load/Store Instructions
let mayLoad = 1, mayStore = 0, hasSideEffects = 0 in {
	def LBU_I:	ORISCInstrLsu8<"lbu", 0>;
	def LBS_I:	ORISCInstrLsu8<"lbs", 1>;
	def LHU_I:	ORISCInstrLsu8<"lhu", 2>;
	def LHS_I:	ORISCInstrLsu8<"lhs", 3>;
	def LW_I:	ORISCInstrLsu8<"lw",  5>;
	def LP_I:	ORISCInstrLsu92<"lp", 4>;
	def LCP_I:	ORISCInstrLsu92<"lcp", 5>;

	def LBU:	ORISCInstrLsuA<"lbu", 0>;
	def LBS:	ORISCInstrLsuA<"lbs", 1>;
	def LHU:	ORISCInstrLsuA<"lhu", 2>;
	def LHS:	ORISCInstrLsuA<"lhs", 3>;
	def LW:		ORISCInstrLsuA<"lw", 4>;
	def LP:		ORISCInstrLsuB2<"lp", 4>;
	def LCP:	ORISCInstrLsuB2<"lcp", 5>;
}

let mayLoad = 0, mayStore = 1, hasSideEffects = 0 in {
	def SB_I:	ORISCInstrLsu91<"sb", 0>;
	def SH_I:	ORISCInstrLsu91<"sh", 1>;
	def SW_I:	ORISCInstrLsu91<"sw", 2>;
	def SP_I:	ORISCInstrLsu93<"sp", 6>;
	def SCP_I:	ORISCInstrLsu93<"scp", 7>;

	def SB:		ORISCInstrLsuB1<"sb", 0>;
	def SH:		ORISCInstrLsuB1<"sh", 1>;
	def SW:		ORISCInstrLsuB1<"sw", 2>;
	def SP:		ORISCInstrLsuB3<"sp", 6>;
	def SCP:	ORISCInstrLsuB3<"scp", 7>;
}

// Branch and Jump Instructions
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
	def JMP:		ORISCInstrReg0BpuF<"jmp", 0>;
	let Defs = [P31] in
	def JSR:		ORISCInstrReg0BpuF<"jsr", 2>;
	let Defs = [P31] in
	def JLIB:		ORISCInstrBpuF<"jlib">;
}

let isReturn = 1, isTerminator = 1, mayLoad = 0, mayStore = 0, hasSideEffects = 0, Uses = [P31] in
	def	RET:		ORISCInstrReg0BpuF2<"ret">;

let mayLoad = 0, mayStore = 0, hasSideEffects = 0, isBranch = 1, isTerminator = 1 in {
	def	BEQP:		ORISCInstrBpuC1<"beqp", 0>;
	def	BNEP:		ORISCInstrBpuC1<"bnep", 1>;
	def	BEQ:		ORISCInstrBpuC2<"beq", 2>;
	def	BNE:		ORISCInstrBpuC2<"bne", 3>;
	def	BGEU:		ORISCInstrBpuC2<"bgeu", 4>;
	def	BGES:		ORISCInstrBpuC2<"bges", 5>;
	def	BLTU:		ORISCInstrBpuC2<"bltu", 6>;
	def	BLTS:		ORISCInstrBpuC2<"blts", 7>;

	let isBarrier = 1 in
	def BRA:		ORISCInstrBpuD<"bra">;

	let isBarrier = 1, Defs = [P31] in
	def BSR:		ORISCInstrBpuE<"bsr">;
}

// Other Instructions
let hasSideEffects = 1, mayLoad = 1, mayStore = 1 in
	def	TRAP:		ORISCInstrReg0Alu72<"trap", 7>;


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
def BGTS:	InstAlias<"bgts\t$dy, $dz, $sd12", 	(BLTS DR:$dz, DR:$dy, brtarget12:$sd12)>;
def BLES:	InstAlias<"bles\t$dy, $dz, $sd12", 	(BGES DR:$dz, DR:$dy, brtarget12:$sd12)>;
def BGTU:	InstAlias<"bgtu\t$dy, $dz, $sd12", 	(BLTU DR:$dz, DR:$dy, brtarget12:$sd12)>;
def BLEU:	InstAlias<"bleu\t$dy, $dz, $sd12", 	(BGEU DR:$dz, DR:$dy, brtarget12:$sd12)>;

def BEQP_1:	InstAlias<"beqp\t$px, $sd12",		(BEQP PR:$px, P0, brtarget12:$sd12)>;
def BNEP_1:	InstAlias<"bnep\t$px, $sd12",		(BNEP PR:$px, P0, brtarget12:$sd12)>;
def BEQ_1:	InstAlias<"beq\t$dx, $sd12",		(BEQ  DR:$dx, D0, brtarget12:$sd12)>;
def BNE_1:	InstAlias<"bne\t$dx, $sd12",		(BNE  DR:$dx, D0, brtarget12:$sd12)>;
def BLE_1:	InstAlias<"ble\t$dx, $sd12",		(BGES D0, DR:$dx, brtarget12:$sd12)>;
def BGE_1:	InstAlias<"bge\t$dx, $sd12",		(BGES DR:$dx, D0, brtarget12:$sd12)>;
def BLT_1:	InstAlias<"blt\t$dx, $sd12",		(BLTS D0, DR:$dx, brtarget12:$sd12)>;
def BGT_1:	InstAlias<"bgt\t$dx, $sd12",		(BLTS DR:$dx, D0, brtarget12:$sd12)>;

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
