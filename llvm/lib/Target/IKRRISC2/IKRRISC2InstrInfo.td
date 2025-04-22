//===-- IKRRISC2InstrInfo.td - Target Description for Objective-RISC --*- tablegen -*-===//
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
// Immediate Operands
//===----------------------------------------------------------------------===//
class IKRRISC2Op<ValueType vt = i32> : Operand<vt> {
    let OperandNamespace = "IKRRISC2Op";
}

def bratarget : IKRRISC2Op<OtherVT> {
  let EncoderMethod = "getBranchTargetOpValue";
}

class IKRRISC2UImmOp<int bitsNum> : IKRRISC2Op {
    let MCOperandPredicate = [{
        int64_t Imm;
        if (!MCOp.evaluateAsConstantImm(Imm))
            return false;
        return isUInt<}]# bitsNum #[{>(Imm);
    }];
}

class IKRRISC2UImmLeafOp<int bitsNum> :
    IKRRISC2UImmOp<bitsNum>, ImmLeaf<i32, "return isUInt<" # bitsNum # ">(Imm);">;

class IKRRISC2SImmOp<int bitsNum> : IKRRISC2Op {
    let MCOperandPredicate = [{
        int64_t Imm;
        if (!MCOp.evaluateAsConstantImm(Imm))
            return false;
        return isInt<}] # bitsNum # [{>(Imm);
    }];
}

class IKRRISC2SImmLeafOp<int bitsNum> :
    IKRRISC2SImmOp<bitsNum>, ImmLeaf<i32, "return isInt<" # bitsNum # ">(Imm);">;


def uimm16 : IKRRISC2UImmLeafOp<16>;

def simm16 : IKRRISC2SImmLeafOp<16>;
def simm18 : IKRRISC2SImmLeafOp<18>;
def simm26 : IKRRISC2SImmLeafOp<26>;


//===----------------------------------------------------------------------===//
// Instruction Formats & Instruction Classes
//===----------------------------------------------------------------------===//
class IKRRISC2InstrFormat;

def IKRRISC2FormatI       : IKRRISC2InstrFormat;
def IKRRISC2FormatR1      : IKRRISC2InstrFormat;
def IKRRISC2FormatR2      : IKRRISC2InstrFormat;
def IKRRISC2FormatB       : IKRRISC2InstrFormat;
def IKRRISC2FormatPseudo  : IKRRISC2InstrFormat;


class IKRRISC2Instr<dag outs, dag ins, IKRRISC2InstrFormat fmt>
		: Instruction {
	let Namespace = "IKRRISC2";

    field bits<32> Inst;
	IKRRISC2InstrFormat Format = fmt;
	dag OutOperandList = outs;
	dag InOperandList = ins;
}

class IKRRISC2InstrReg<dag outs, dag ins, IKRRISC2InstrFormat fmt>
		: IKRRISC2Instr<outs, ins, fmt> {
    bits<5> rc;
    let Inst{25-21} = rc;
}

class IKRRISC2InstrRegReg<dag outs, dag ins, IKRRISC2InstrFormat fmt>
		: IKRRISC2Instr<outs, ins, fmt> {
    bits<5> rb;
    let Inst{20-16} = rb;
}

class IKRRISC2InstrIUImm<string opcstr, bits<6> opcval, list<dag> pattern>
		: IKRRISC2InstrRegReg<(outs GPR:$rc), (ins GPR:$rb, uimm16:$ui16), IKRRISC2FormatI> {
    bits<16> ui16;

	let AsmString = opcstr # !if(!lt(!size(opcstr), 4), "\t", "") # "\t" # "$rc := $rb,#$ui16";
    let Pattern = pattern;

    let Inst{31-26} = opcval;
    let Inst{15-0}  = ui16;
}

class IKRRISC2InstrISImm<string opcstr, bits<6> opcval, list<dag> pattern>
		: IKRRISC2InstrRegReg<(outs GPR:$rc), (ins GPR:$rb, simm16:$si16), IKRRISC2FormatI> {
    bits<16> si16;

	let AsmString = opcstr # !if(!lt(!size(opcstr), 4), "\t", "") # "\t" # "$rc := $rb,#$si16";
    let Pattern = pattern;

    let Inst{31-26} = opcval;
    let Inst{15-0}  = si16;
}

class IKRRISC2InstrR2<string opcstr, bits<6> opcval, list<dag> pattern>
		: IKRRISC2InstrRegReg<(outs GPR:$rc), (ins GPR:$rb, GPR:$ra), IKRRISC2FormatR2> {
    bits<5> ra;

	let AsmString = opcstr # !if(!lt(!size(opcstr), 4), "\t", "") # "\t" # "$rc := $rb,$ra";
    let Pattern = pattern;

    let Inst{31-26} = 63;
    let Inst{15-10} = opcval;
    let Inst{9-5}   = 0;
    let Inst{4-0}   = ra;
}

class IKRRISC2InstrR1<string opcstr, bits<6> opcval, bits<5> ra, list<dag> pattern>
		: IKRRISC2InstrRegReg<(outs GPR:$rc), (ins GPR:$rb), IKRRISC2FormatR1> {
	let AsmString = opcstr # !if(!lt(!size(opcstr), 4), "\t", "") # "\t" # "$rc := $rb";
    let Pattern = pattern;

    let Inst{31-26} = 63;
    let Inst{15-10} = opcval;
    let Inst{9-5}   = 0;
    let Inst{4-0}   = ra;
}

class IKRRISC2InstrR0<string opcstr, bits<6> opcval, list<dag> pattern>
		: IKRRISC2InstrRegReg<(outs), (ins GPR:$rb), IKRRISC2FormatR1> {
	let AsmString = opcstr # !if(!lt(!size(opcstr), 4), "\t", "") # "\t" # "$rb";
    let Pattern = pattern;

    bits<5> rc = 0;
    let Inst{31-26} = 63;
    let Inst{15-10} = opcval;
    let Inst{9-5}   = 0;
    let Inst{4-0}   = 0;
}

class IKRRISC2InstrB0<string opcstr, bits<6> opcval, list<dag> pattern>
		: IKRRISC2Instr<(outs), (ins bratarget:$si26), IKRRISC2FormatB> {
    bits<26> si26;

	let AsmString = opcstr # !if(!lt(!size(opcstr), 4), "\t", "") # "\t" # "#$si26";
    let Pattern = pattern;

    let Inst{31-26} = opcval;
    let Inst{25-0}  = si26;
}

class IKRRISC2InstrB1<string opcstr, bits<3> opcval, list<dag> pattern>
		: IKRRISC2InstrReg<(outs), (ins GPR:$rc, bratarget:$si18), IKRRISC2FormatB> {
    bits<18> si18;

	let AsmString = opcstr # !if(!lt(!size(opcstr), 4), "\t", "") # "\t" # "$rc := #$si18";
    let Pattern = pattern;

    let Inst{31-26} = 62;
    let Inst{20-18} = opcval;
    let Inst{17-0}  = si18;
}

// Pseudo instructions
class Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
    : IKRRISC2Instr<outs, ins, IKRRISC2FormatPseudo> {
  let AsmString = asmstr;
  let Pattern = pattern;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let Size = 4;
}

//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
    def ADDI  : IKRRISC2InstrISImm<"addi",  0x00,  [(set GPR:$rc, (add GPR:$rb, simm16:$si16))]>;
    def ADDLI : IKRRISC2InstrIUImm<"addli", 0x01,  [(set GPR:$rc, (add GPR:$rb, uimm16:$ui16))]>;
    def ADDHI : IKRRISC2InstrIUImm<"addhi", 0x02,  []>;
    def AND0I : IKRRISC2InstrIUImm<"and0i", 0x04,  [(set GPR:$rc, (and GPR:$rb, uimm16:$ui16))]>;
    def AND1I : IKRRISC2InstrIUImm<"and1i", 0x05,  []>;
    def ORI   : IKRRISC2InstrIUImm<"ori",   0x06,  [(set GPR:$rc, (or  GPR:$rb, uimm16:$ui16))]>;
    def XORI  : IKRRISC2InstrIUImm<"xori",  0x07,  [(set GPR:$rc, (xor GPR:$rb, uimm16:$ui16))]>;

    def ADD   : IKRRISC2InstrR2<"add",   0x00,     [(set GPR:$rc, (add GPR:$rb, GPR:$ra))]>;
    def ADDX  : IKRRISC2InstrR2<"addx",  0x20,     []>;
    def SUB   : IKRRISC2InstrR2<"sub",   0x02,     [(set GPR:$rc, (sub GPR:$rb, GPR:$ra))]>;
    def SUBX  : IKRRISC2InstrR2<"subx",  0x22,     []>;
    def AND   : IKRRISC2InstrR2<"and",   0x04,     [(set GPR:$rc, (and GPR:$rb, GPR:$ra))]>;
    def OR    : IKRRISC2InstrR2<"or",    0x06,     [(set GPR:$rc, (or  GPR:$rb, GPR:$ra))]>;
    def XOR   : IKRRISC2InstrR2<"xor",   0x07,     [(set GPR:$rc, (xor GPR:$rb, GPR:$ra))]>;

    def LSL   : IKRRISC2InstrR1<"lsl",   0x28, 1,  [(set GPR:$rc, (shl GPR:$rb, 1))]>;
    def LSR   : IKRRISC2InstrR1<"lsr",   0x29, 1,  [(set GPR:$rc, (srl GPR:$rb, 1))]>;
    def ASL   : IKRRISC2InstrR1<"asl",   0x2A, 1,  []>;
    def ASR   : IKRRISC2InstrR1<"asr",   0x2B, 1,  [(set GPR:$rc, (sra GPR:$rb, 1))]>;
    def ROL   : IKRRISC2InstrR1<"rol",   0x2C, 1,  [(set GPR:$rc, (rotl GPR:$rb, 1))]>;
    def ROR   : IKRRISC2InstrR1<"ror",   0x2D, 1,  [(set GPR:$rc, (rotr GPR:$rb, 1))]>;
    def EXTB  : IKRRISC2InstrR1<"extb",  0x30, 0,  [(set GPR:$rc, (sext_inreg GPR:$rb, i8))]>;
    def EXTH  : IKRRISC2InstrR1<"exth",  0x31, 0,  [(set GPR:$rc, (sext_inreg GPR:$rb, i16))]>;
    def SWAPB : IKRRISC2InstrR1<"swapb", 0x32, 0,  []>;
    def SWAPH : IKRRISC2InstrR1<"swaph", 0x2C, 16, []>;
    def NOT   : IKRRISC2InstrR1<"not",   0x32, 0,  []>;
    def JMP   : IKRRISC2InstrR0<"jmp",   0x3C,     []>;
    def JSR   : IKRRISC2InstrR0<"jsr",   0x3D,     []>;

    def CMPUI : IKRRISC2InstrIUImm<"cmpui", 0x08,  [(set GPR:$rc, (ucmp GPR:$rb, uimm16:$ui16))]>;
    def CMPU  : IKRRISC2InstrR2<"cmpu",  0x08,     [(set GPR:$rc, (ucmp GPR:$rb, GPR:$ra))]>;
    def CMPSI : IKRRISC2InstrISImm<"cmpsi", 0x09,  [(set GPR:$rc, (scmp GPR:$rb, simm16:$si16))]>;
    def CMPS  : IKRRISC2InstrR2<"cmps",  0x09,     [(set GPR:$rc, (scmp GPR:$rb, GPR:$ra))]>;
}

let mayLoad = 0, mayStore = 0, hasSideEffects = 0, isBranch = 1, isTerminator = 1 in {
    def BRA   : IKRRISC2InstrB0<"bra",   0x3C,     [(br bb:$si26)]>;
    def BSR   : IKRRISC2InstrB0<"bsr",  0x3D,      []>;

    def BEQ   : IKRRISC2InstrB1<"beq",   0,        [(brcc SETEQ, GPR:$rc, 0, bb:$si18)]>;
    def BNE   : IKRRISC2InstrB1<"bne",   1,        [(brcc SETNE, GPR:$rc, 0, bb:$si18)]>;
    def BLT   : IKRRISC2InstrB1<"blt",   1,        [(brcc SETLT, GPR:$rc, 0, bb:$si18)]>;
    def BGT   : IKRRISC2InstrB1<"bgt",   1,        [(brcc SETGT, GPR:$rc, 0, bb:$si18)]>;
    def BGE   : IKRRISC2InstrB1<"bge",   1,        [(brcc SETGE, GPR:$rc, 0, bb:$si18)]>;
    def BLE   : IKRRISC2InstrB1<"ble",   1,        [(brcc SETLE, GPR:$rc, 0, bb:$si18)]>;
}

let mayLoad = 1, mayStore = 0, hasSideEffects = 0 in {
    def LDD   : IKRRISC2InstrISImm<"ldd", 0x10,    [(set GPR:$rc, (load (add GPR:$rb, simm16:$si16)))]>;
    def LDR   : IKRRISC2InstrR2<"ldr",    0x10,    [(set GPR:$rc, (load (add GPR:$rb, GPR:$ra)))]>;
}

let mayLoad = 0, mayStore = 1, hasSideEffects = 0 in {
    def SDD   : IKRRISC2InstrISImm<"sdd", 0x14,    [(store GPR:$rc, (add GPR:$rb, simm16:$si16))]>;
    def SDR   : IKRRISC2InstrR2<"sdr",    0x14,    [(store GPR:$rc, (add GPR:$rb, GPR:$ra))]>;
}


//===----------------------------------------------------------------------===//
// Custom ISDs
//===----------------------------------------------------------------------===//

def ikrrisc2_ret  : SDNode<"IKRRISC2ISD::RET", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

let isBarrier = 1, isReturn = 1, isTerminator = 1 in
def PseudoRET : Pseudo<(outs), (ins), "jmp\t\tra", [(ikrrisc2_ret)]>,
                PseudoInstExpansion<(JMP R31)>;