//===-- ObjectiveRISCRegisterInfo.td - Objective-RISC Register defs --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Declarations that describe the Objective-RISC register files
//===----------------------------------------------------------------------===//

let Namespace = "ObjectiveRISC" in {

class ObjectiveRISCReg<string N, bits<5> ENC, list<string> alt = []> : Register<N, alt> {
	let HWEncoding{4-0} = ENC;
}

}

def D0:		ObjectiveRISCReg<"d0",	0, ["zero"]>;
def D1:		ObjectiveRISCReg<"d1",	1, ["t5"]>;
def D2:		ObjectiveRISCReg<"d2",	2, ["t6"]>;
def D3:		ObjectiveRISCReg<"d3",	3, ["a3"]>;
def D4:		ObjectiveRISCReg<"d4",	4, ["a4"]>;
def D5:		ObjectiveRISCReg<"d5",	5, ["a5"]>;
def D6:		ObjectiveRISCReg<"d6",	6, ["a6"]>;
def D7:		ObjectiveRISCReg<"d7",	7, ["a7"]>;
def D8:		ObjectiveRISCReg<"d8",	8, ["s0"]>;
def D9:		ObjectiveRISCReg<"d9",	9, ["s1"]>;
def D10:	ObjectiveRISCReg<"d10",	10, ["a0"]>;
def D11:	ObjectiveRISCReg<"d11",	11, ["a1"]>;
def D12:	ObjectiveRISCReg<"d12",	12, ["a2"]>;
def D13:	ObjectiveRISCReg<"d13",	13, ["t0"]>;
def D14:	ObjectiveRISCReg<"d14",	14, ["t1"]>;
def D15:	ObjectiveRISCReg<"d15",	15, ["t2"]>;
def D16:	ObjectiveRISCReg<"d16",	16, ["t3"]>;
def D17:	ObjectiveRISCReg<"d17",	17, ["t4"]>;
def D18:	ObjectiveRISCReg<"d18",	18, ["s2"]>;
def D19:	ObjectiveRISCReg<"d19",	19, ["s3"]>;
def D20:	ObjectiveRISCReg<"d20",	20, ["s4"]>;
def D21:	ObjectiveRISCReg<"d21",	21, ["s5"]>;
def D22:	ObjectiveRISCReg<"d22",	22, ["s6"]>;
def D23:	ObjectiveRISCReg<"d23",	23, ["s7"]>;
def D24:	ObjectiveRISCReg<"d24",	24, ["s8"]>;
def D25:	ObjectiveRISCReg<"d25",	25, ["s9"]>;
def D26:	ObjectiveRISCReg<"d26",	26, ["s10"]>;
def D27:	ObjectiveRISCReg<"d27",	27, ["s11"]>;
def D28:	ObjectiveRISCReg<"d28",	28, ["s12"]>;
def D29:	ObjectiveRISCReg<"d29",	29, ["s13"]>;
def D30:	ObjectiveRISCReg<"d30",	30, ["s14"]>;
def D31:	ObjectiveRISCReg<"d31",	31, ["s15"]>;

def P0:		ObjectiveRISCReg<"p0",	0, ["zero"]>;
def P1:		ObjectiveRISCReg<"p1",	1, ["h5"]>;
def P2:		ObjectiveRISCReg<"p2",	2, ["h6"]>;
def P3:		ObjectiveRISCReg<"p3",	3, ["b3"]>;
def P4:		ObjectiveRISCReg<"p4",	4, ["b4"]>;
def P5:		ObjectiveRISCReg<"p5",	5, ["b5"]>;
def P6:		ObjectiveRISCReg<"p6",	6, ["b6"]>;
def P7:		ObjectiveRISCReg<"p7",	7, ["b7"]>;
def P8:		ObjectiveRISCReg<"p8",	8, ["q0"]>;
def P9:		ObjectiveRISCReg<"p9",	9, ["q1"]>;
def P10:	ObjectiveRISCReg<"p10",	10, ["b0"]>;
def P11:	ObjectiveRISCReg<"p11",	11, ["b1"]>;
def P12:	ObjectiveRISCReg<"p12",	12, ["b2"]>;
def P13:	ObjectiveRISCReg<"p13",	13, ["h0"]>;
def P14:	ObjectiveRISCReg<"p14",	14, ["h1"]>;
def P15:	ObjectiveRISCReg<"p15",	15, ["h2"]>;
def P16:	ObjectiveRISCReg<"p16",	16, ["h3"]>;
def P17:	ObjectiveRISCReg<"p17",	17, ["h4"]>;
def P18:	ObjectiveRISCReg<"p18",	18, ["q2"]>;
def P19:	ObjectiveRISCReg<"p19",	19, ["q3"]>;
def P20:	ObjectiveRISCReg<"p20",	20, ["q4"]>;
def P21:	ObjectiveRISCReg<"p21",	21, ["q5"]>;
def P22:	ObjectiveRISCReg<"p22",	22, ["q6"]>;
def P23:	ObjectiveRISCReg<"p23",	23, ["q7"]>;
def P24:	ObjectiveRISCReg<"p24",	24, ["q8"]>;
def P25:	ObjectiveRISCReg<"p25",	25, ["q9"]>;
def P26:	ObjectiveRISCReg<"p26",	26, ["q10"]>;
def P27:	ObjectiveRISCReg<"p27",	27, ["q11"]>;
def P28:	ObjectiveRISCReg<"p28",	28, ["cnst"]>;
def P29:	ObjectiveRISCReg<"p29",	29, ["ctxt"]>;
def P30:	ObjectiveRISCReg<"p30",	30, ["frame"]>;
def P31:	ObjectiveRISCReg<"p31",	31, ["rpc", "core"]>;

class DataRegClass<dag regList>: RegisterClass<"ObjectiveRISC", [i32], 32, regList>;
class PointerRegClass<dag regList>: RegisterClass<"ObjectiveRISC", [ptr], 32, regList>;

def DR: DataRegClass<(sequence "D%u", 0, 31)>;
def PR: PointerRegClass<(sequence "P%u", 0, 31)>;
