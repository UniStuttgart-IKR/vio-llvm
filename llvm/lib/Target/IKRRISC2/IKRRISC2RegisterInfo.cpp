//===-- IKRRISC2RegisterInfo.cpp - Objective-RISC Register Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Objective-RISC implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "IKRRISC2RegisterInfo.h"
#include "IKRRISC2FrameLowering.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "MCTargetDesc/IKRRISC2MCTargetDesc.h"

using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "IKRRISC2GenRegisterInfo.inc"

const MCPhysReg*
IKRRISC2RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
    return CSR_SaveList;
}

const uint32_t *
IKRRISC2RegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                        CallingConv::ID CC) const {
    return CSR_RegMask;
}

BitVector IKRRISC2RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
	BitVector Reserved(getNumRegs());

	Reserved.set(IKRRISC2::R0);	// zero
	Reserved.set(IKRRISC2::R30);	// sp
	Reserved.set(IKRRISC2::R31);	// ra

	return Reserved;
}

bool IKRRISC2RegisterInfo::isReservedReg(const MachineFunction &MF, MCRegister Reg) const {
	return getReservedRegs(MF)[Reg];
}

static void replaceFI(MachineFunction &MF, MachineBasicBlock::iterator II,
											MachineInstr &MI, const DebugLoc &dl,
											unsigned FIOperandNum, int Offset, unsigned FramePtr) {
	// TODO
}

bool
IKRRISC2RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
																			 int SPAdj, unsigned FIOperandNum,
																			 RegScavenger *RS) const {
	// TODO
}

Register IKRRISC2RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
	return IKRRISC2::R30;
}