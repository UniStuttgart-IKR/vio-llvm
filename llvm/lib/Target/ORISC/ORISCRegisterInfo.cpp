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
#include <cassert>
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
ORISCRegisterInfo::getPointerRegClass(unsigned Kind) const {
	return &ORISC::PRRegClass;
}

static void replaceFI(MachineFunction &MF, MachineBasicBlock::iterator II,
											MachineInstr &MI, const DebugLoc &dl,
											unsigned FIOperandNum, int Offset, unsigned FramePtr) {
	if (Offset <= 4095) {
		// If the offset is small enough to fit in the immediate field, directly
		// encode it.
		MI.getOperand(FIOperandNum).ChangeToImmediate(Offset);
		return;
	}

  	const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  	MachineRegisterInfo &MRI = MF.getRegInfo();
  	Register FrameIndex = MRI.createVirtualRegister(&ORISC::DRRegClass);
	unsigned HI20 = (Offset >> 12) & 0xFFFFF;
	BuildMI(*MI.getParent(), II, dl, TII.get(ORISC::LUI), FrameIndex)
		.addImm(HI20);
	unsigned LO12 = Offset & 0xFFF;
	BuildMI(*MI.getParent(), II, dl, TII.get(ORISC::ORI), FrameIndex)
		.addReg(FrameIndex)
		.addImm(LO12);
	
	MI.getOperand(FIOperandNum).ChangeToRegister(FramePtr, false);
	MI.getOperand(FIOperandNum + 1).ChangeToRegister(FrameIndex, false);
}

bool
ORISCRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
										int SPAdj, unsigned FIOperandNum,
										RegScavenger *RS) const {
	assert(SPAdj == 0 && "Unexpected");

	MachineInstr &MI = *II;
	DebugLoc dl = MI.getDebugLoc();
	int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
	
	MachineFunction &MF = *MI.getParent()->getParent();
	const ORISCFrameLowering *TFI = getFrameLowering(MF);

	Register FrameReg;
	int Offset = TFI->getFrameIndexReference(MF, FrameIndex, FrameReg).getFixed();
	if (MI.getOpcode() == ORISC::LW_I || MI.getOpcode() == ORISC::LP_I
		|| MI.getOpcode() == ORISC::SW_I || MI.getOpcode() == ORISC::SP_I) {
		assert(Offset % 4 == 0 && "Unalligned Memory Access");
		Offset = Offset/4;
	} else if (MI.getOpcode() == ORISC::LHS_I || MI.getOpcode() == ORISC::LHU_I
		|| MI.getOpcode() == ORISC::SH_I) {
		assert(Offset % 2 == 0 && "Unalligned Memory Access");
		Offset = Offset/2;
	}
	//Offset += MI.getOperand(FIOperandNum + 1).getImm();  

	replaceFI(MF, II, MI, dl, FIOperandNum, Offset, FrameReg);

	// replaceFI never removes II
	return false;
}

Register ORISCRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
	return ORISC::P30;
}