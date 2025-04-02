//===-- ObjectiveRISCInstrInfo.cpp - Objective-RISC Instruction Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Objective-RISC implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "ObjectiveRISCInstrInfo.h"
#include "ObjectiveRISCMachineFunctionInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "ObjectiveRISCGenInstrInfo.inc"

// Pin the vtable to this file.
void ObjectiveRISCInstrInfo::anchor() {}

ObjectiveRISCInstrInfo::ObjectiveRISCInstrInfo()
		: ObjectiveRISCGenInstrInfo(SP::ADJCALLSTACKDOWN, SP::ADJCALLSTACKUP), RI(),
			Subtarget(ST) {}

/// isLoadFromStackSlot - If the specified machine instruction is a direct
/// load from a stack slot, return the virtual or physical register number of
/// the destination along with the FrameIndex of the loaded stack slot.  If
/// not, return 0.  This predicate must return 0 if the instruction has
/// any side effects other than loading from the stack slot.
Register ObjectiveRISCInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
																						 int &FrameIndex) const {
	// TODO
}

/// isStoreToStackSlot - If the specified machine instruction is a direct
/// store to a stack slot, return the virtual or physical register number of
/// the source reg along with the FrameIndex of the loaded stack slot.  If
/// not, return 0.  This predicate must return 0 if the instruction has
/// any side effects other than storing to the stack slot.
Register ObjectiveRISCInstrInfo::isStoreToStackSlot(const MachineInstr &MI,
																						int &FrameIndex) const {
	// TODO
}

MachineBasicBlock *
ObjectiveRISCInstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
	// TODO
}

bool ObjectiveRISCInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
																	 MachineBasicBlock *&TBB,
																	 MachineBasicBlock *&FBB,
																	 SmallVectorImpl<MachineOperand> &Cond,
																	 bool AllowModify) const {
	// TODO
}

unsigned ObjectiveRISCInstrInfo::insertBranch(MachineBasicBlock &MBB,
																			MachineBasicBlock *TBB,
																			MachineBasicBlock *FBB,
																			ArrayRef<MachineOperand> Cond,
																			const DebugLoc &DL,
																			int *BytesAdded) const {
	// TODO
}

unsigned ObjectiveRISCInstrInfo::removeBranch(MachineBasicBlock &MBB,
																			int *BytesRemoved) const {
	// TODO
}

bool ObjectiveRISCInstrInfo::reverseBranchCondition(
		SmallVectorImpl<MachineOperand> &Cond) const {
	// TODO
}

bool ObjectiveRISCInstrInfo::isBranchOffsetInRange(unsigned BranchOpc,
																					 int64_t Offset) const {
	// TODO
}

void ObjectiveRISCInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
																 MachineBasicBlock::iterator I,
																 const DebugLoc &DL, MCRegister DestReg,
																 MCRegister SrcReg, bool KillSrc,
																 bool RenamableDest, bool RenamableSrc) const {
	// TODO
}

void ObjectiveRISCInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
																				 MachineBasicBlock::iterator I,
																				 Register SrcReg, bool isKill, int FI,
																				 const TargetRegisterClass *RC,
																				 const TargetRegisterInfo *TRI,
																				 Register VReg) const {
	// TODO
}

void ObjectiveRISCInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
																					MachineBasicBlock::iterator I,
																					Register DestReg, int FI,
																					const TargetRegisterClass *RC,
																					const TargetRegisterInfo *TRI,
																					Register VReg) const {
	// TODO
}

Register ObjectiveRISCInstrInfo::getGlobalBaseReg(MachineFunction *MF) const {
	// TODO
}
