//===-- ORISCRegisterInfo.cpp - Objective-RISC Register Information ----------------===//
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

#include "ORISCRegisterInfo.h"
#include "ORISCFrameLowering.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "MCTargetDesc/ORISCMCTargetDesc.h"

using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "ORISCGenRegisterInfo.inc"

const MCPhysReg*
ORISCRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_SaveList;
}

const uint32_t *
ORISCRegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                        CallingConv::ID CC) const {
  return CSR_RegMask;
}

BitVector ORISCRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
	BitVector Reserved(getNumRegs());

	Reserved.set(ORISC::D0);	// zero
	Reserved.set(ORISC::P0);	// null
	Reserved.set(ORISC::P28);	// cnst
	Reserved.set(ORISC::P29);	// ctxt
	Reserved.set(ORISC::P30);	// frame
	Reserved.set(ORISC::P31);	// rpc/core

	return Reserved;
}

bool ORISCRegisterInfo::isReservedReg(const MachineFunction &MF,
																			MCRegister Reg) const {
	return getReservedRegs(MF)[Reg];
}

const TargetRegisterClass*
ORISCRegisterInfo::getPointerRegClass(const MachineFunction &MF, unsigned Kind) const {
	return &ORISC::PRRegClass;
}

static void replaceFI(MachineFunction &MF, MachineBasicBlock::iterator II,
											MachineInstr &MI, const DebugLoc &dl,
											unsigned FIOperandNum, int Offset, unsigned FramePtr) {
	// TODO
}

bool
ORISCRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
																			 int SPAdj, unsigned FIOperandNum,
																			 RegScavenger *RS) const {
	// TODO
}

Register ORISCRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
	return ORISC::P30;
}