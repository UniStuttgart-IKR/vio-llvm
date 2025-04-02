//===-- ObjectiveRISCRegisterInfo.cpp - Objective-RISC Register Information ----------------===//
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

#include "ObjectiveRISCRegisterInfo.h"
#include "ObjectiveRISC.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "ObjectiveRISCGenRegisterInfo.inc"

const MCPhysReg*
ObjectiveRISCRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_SaveList;
}

const uint32_t *
ObjectiveRISCRegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                        CallingConv::ID CC) const {
  return CSR_RegMask;
}

BitVector ObjectiveRISCRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
	BitVector Reserved(32 * 2);

	Reserved.set(ObjectiveRISC::P28);	// cnst
	Reserved.set(ObjectiveRISC::P29);	// ctxt
	Reserved.set(ObjectiveRISC::P30);	// frame
	Reserved.set(ObjectiveRISC::P31);	// rpc/core

	return Reserved;
}

bool ObjectiveRISCRegisterInfo::isReservedReg(const MachineFunction &MF,
																			MCRegister Reg) const {
	return getReservedRegs(MF)[Reg];
}

const TargetRegisterClass*
ObjectiveRISCRegisterInfo::getPointerRegClass(const MachineFunction &MF,
																			unsigned Kind) const {
	return &ObjectiveRISC::PR;
}

static void replaceFI(MachineFunction &MF, MachineBasicBlock::iterator II,
											MachineInstr &MI, const DebugLoc &dl,
											unsigned FIOperandNum, int Offset, unsigned FramePtr) {
	// TODO
}

bool
ObjectiveRISCRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
																			 int SPAdj, unsigned FIOperandNum,
																			 RegScavenger *RS) const {
	// TODO
}

Register ObjectiveRISCRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
	return ObjectiveRISC::P30;
}