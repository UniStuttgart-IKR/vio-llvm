//===-- ORISCRegisterInfo.td - Objective-RISC Register defs --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Declarations that describe the Objective-RISC register files
//===----------------------------------------------------------------------===//

class ORISCReg<string N, bits<5> ENC, list<string> alt = []> : Register<N, alt> {
	let HWEncoding{4-0} = ENC;
	let Namespace = "ORISC";
  	let AltNames = alt;
}

let FallbackRegAltNameIndex = NoRegAltName in
def ABIRegAltName : RegAltNameIndex;

let RegAltNameIndices = [ABIRegAltName] in {
	let isConstant = true in
	def D0:		ORISCReg<"d0",	0, ["zero"]>;

	def D1:		ORISCReg<"d1",	1, ["t5"]>;
	def D2:		ORISCReg<"d2",	2, ["t6"]>;
	def D3:		ORISCReg<"d3",	3, ["a3"]>;
	def D4:		ORISCReg<"d4",	4, ["a4"]>;
	def D5:		ORISCReg<"d5",	5, ["a5"]>;
	def D6:		ORISCReg<"d6",	6, ["a6"]>;
	def D7:		ORISCReg<"d7",	7, ["a7"]>;
	def D8:		ORISCReg<"d8",	8, ["s0"]>;
	def D9:		ORISCReg<"d9",	9, ["s1"]>;
	def D10:	ORISCReg<"d10",	10, ["a0"]>;
	def D11:	ORISCReg<"d11",	11, ["a1"]>;
	def D12:	ORISCReg<"d12",	12, ["a2"]>;
	def D13:	ORISCReg<"d13",	13, ["t0"]>;
	def D14:	ORISCReg<"d14",	14, ["t1"]>;
	def D15:	ORISCReg<"d15",	15, ["t2"]>;
	def D16:	ORISCReg<"d16",	16, ["t3"]>;
	def D17:	ORISCReg<"d17",	17, ["t4"]>;
	def D18:	ORISCReg<"d18",	18, ["s2"]>;
	def D19:	ORISCReg<"d19",	19, ["s3"]>;
	def D20:	ORISCReg<"d20",	20, ["s4"]>;
	def D21:	ORISCReg<"d21",	21, ["s5"]>;
	def D22:	ORISCReg<"d22",	22, ["s6"]>;
	def D23:	ORISCReg<"d23",	23, ["s7"]>;
	def D24:	ORISCReg<"d24",	24, ["s8"]>;
	def D25:	ORISCReg<"d25",	25, ["s9"]>;
	def D26:	ORISCReg<"d26",	26, ["s10"]>;
	def D27:	ORISCReg<"d27",	27, ["s11"]>;
	def D28:	ORISCReg<"d28",	28, ["s12"]>;
	def D29:	ORISCReg<"d29",	29, ["s13"]>;
	def D30:	ORISCReg<"d30",	30, ["s14"]>;
	def D31:	ORISCReg<"d31",	31, ["s15"]>;


	let isConstant = true in
	def P0:		ORISCReg<"p0",	0, ["null"]>;

	def P1:		ORISCReg<"p1",	1, ["h5"]>;
	def P2:		ORISCReg<"p2",	2, ["h6"]>;
	def P3:		ORISCReg<"p3",	3, ["b3"]>;
	def P4:		ORISCReg<"p4",	4, ["b4"]>;
	def P5:		ORISCReg<"p5",	5, ["b5"]>;
	def P6:		ORISCReg<"p6",	6, ["b6"]>;
	def P7:		ORISCReg<"p7",	7, ["b7"]>;
	def P8:		ORISCReg<"p8",	8, ["q0"]>;
	def P9:		ORISCReg<"p9",	9, ["q1"]>;
	def P10:	ORISCReg<"p10",	10, ["b0"]>;
	def P11:	ORISCReg<"p11",	11, ["b1"]>;
	def P12:	ORISCReg<"p12",	12, ["b2"]>;
	def P13:	ORISCReg<"p13",	13, ["h0"]>;
	def P14:	ORISCReg<"p14",	14, ["h1"]>;
	def P15:	ORISCReg<"p15",	15, ["h2"]>;
	def P16:	ORISCReg<"p16",	16, ["h3"]>;
	def P17:	ORISCReg<"p17",	17, ["h4"]>;
	def P18:	ORISCReg<"p18",	18, ["q2"]>;
	def P19:	ORISCReg<"p19",	19, ["q3"]>;
	def P20:	ORISCReg<"p20",	20, ["q4"]>;
	def P21:	ORISCReg<"p21",	21, ["q5"]>;
	def P22:	ORISCReg<"p22",	22, ["q6"]>;
	def P23:	ORISCReg<"p23",	23, ["q7"]>;
	def P24:	ORISCReg<"p24",	24, ["q8"]>;
	def P25:	ORISCReg<"p25",	25, ["q9"]>;
	def P26:	ORISCReg<"p26",	26, ["q10"]>;
	def P27:	ORISCReg<"p27",	27, ["q11"]>;
	def P28:	ORISCReg<"p28",	28, ["cnst"]>;
	def P29:	ORISCReg<"p29",	29, ["ctxt"]>;
	def P30:	ORISCReg<"p30",	30, ["frame"]>;
	def P31:	ORISCReg<"p31",	31, ["rpc", "core"]>;
}

def DR : RegisterClass<"ORISC", [i32], 32, (add (sequence "D%u", 0, 31))>;

let Size = 32 in
def PR : RegisterClass<"ORISC", [iPTR, pointer], 32, (add (sequence "P%u", 0, 31))>;
def PRF : RegisterClass<"ORISC", [iPTR, pointer], 32, (add (sequence "P%u", 0, 29), P31)>;
def PRFN : RegisterClass<"ORISC", [iPTR, pointer], 32, (add (sequence "P%u", 1, 29), P31)>;
def F : RegisterClass<"ORISC", [iPTR, pointer], 32, (add P30)>;
