//===-- IKRRISC2RegisterInfo.td - IKRRISC2 Register defs --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Declarations that describe the IKR-RISC2 register files
//===----------------------------------------------------------------------===//

class IKRRISC2Reg<string N, bits<5> ENC, list<string> alt = []> : Register<N, alt> {
	let HWEncoding{4-0} = ENC;
	let Namespace = "IKRRISC2";
}

def R0:		IKRRISC2Reg<"r0",	0, ["zero"]>;
def R1:		IKRRISC2Reg<"r1",	1>;
def R2:		IKRRISC2Reg<"r2",	2>;
def R3:		IKRRISC2Reg<"r3",	3>;
def R4:		IKRRISC2Reg<"r4",	4>;
def R5:		IKRRISC2Reg<"r5",	5>;
def R6:		IKRRISC2Reg<"r6",	6>;
def R7:		IKRRISC2Reg<"r7",	7>;
def R8:		IKRRISC2Reg<"r8",	8>;
def R9:		IKRRISC2Reg<"r9",	9>;
def R10:	IKRRISC2Reg<"r10",	10>;
def R11:	IKRRISC2Reg<"r11",	11>;
def R12:	IKRRISC2Reg<"r12",	12>;
def R13:	IKRRISC2Reg<"r13",	13>;
def R14:	IKRRISC2Reg<"r14",	14>;
def R15:	IKRRISC2Reg<"r15",	15>;
def R16:	IKRRISC2Reg<"r16",	16>;
def R17:	IKRRISC2Reg<"r17",	17>;
def R18:	IKRRISC2Reg<"r18",	18>;
def R19:	IKRRISC2Reg<"r19",	19>;
def R20:	IKRRISC2Reg<"r20",	20>;
def R21:	IKRRISC2Reg<"r21",	21>;
def R22:	IKRRISC2Reg<"r22",	22>;
def R23:	IKRRISC2Reg<"r23",	23>;
def R24:	IKRRISC2Reg<"r24",	24>;
def R25:	IKRRISC2Reg<"r25",	25>;
def R26:	IKRRISC2Reg<"r26",	26>;
def R27:	IKRRISC2Reg<"r27",	27>;
def R28:	IKRRISC2Reg<"r28",	28>;
def R29:	IKRRISC2Reg<"r29",	29>;
def R30:	IKRRISC2Reg<"r30",	30, ["sp"]>;
def R31:	IKRRISC2Reg<"r31",	31, ["ra", "lr"]>;

def GPR : RegisterClass<"IKRRISC2", [i32], 32, (add (sequence "R%u", 0, 31))>;