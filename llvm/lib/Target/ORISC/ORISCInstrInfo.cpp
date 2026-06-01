//=- ORISCInstrInfo.cpp - ORISC Instruction Information -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the ORISC implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "ORISCInstrInfo.h"
#include "ORISCRegisterInfo.h"
#include "ORISCSubtarget.h"
#include "ORISCTargetMachine.h"
#include "MCTargetDesc/ORISCMCTargetDesc.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/Support/Debug.h"

using namespace llvm;
#define DEBUG_TYPE "ORISC-InstInfo"

#define GET_INSTRINFO_CTOR_DTOR
#include "ORISCGenInstrInfo.inc"

ORISCInstrInfo::ORISCInstrInfo(const ORISCSubtarget &STI, const TargetRegisterInfo &TRI)
    : ORISCGenInstrInfo(STI, TRI), STI(STI) {}

MCInst ORISCInstrInfo::getNop() const {
    return MCInstBuilder(ORISC::CLZ)
        .addReg(ORISC::D0)
        .addReg(ORISC::D0);
}

void ORISCInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MBBI,
                                    const DebugLoc &DL, Register DestReg,
                                    Register SrcReg, bool KillSrc,
                                    bool RenamableDest, bool RenamableSrc) const {
    if (ORISC::DRRegClass.contains(DestReg, SrcReg))
        BuildMI(MBB, MBBI, DL, get(ORISC::OR), DestReg)
            .addReg(SrcReg, getKillRegState(KillSrc))
            .addReg(SrcReg, getKillRegState(KillSrc));
    else if (ORISC::PRRegClass.contains(DestReg, SrcReg))
        BuildMI(MBB, MBBI, DL, get(ORISC::CPP), DestReg)
            .addReg(SrcReg, getKillRegState(KillSrc));
    else
        report_fatal_error("Impossible reg-to-reg copy");
}

Register ORISCInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                            int &FrameIndex) const {
    TypeSize DummyBytes = TypeSize::getZero();
    return isLoadFromStackSlot(MI, FrameIndex, DummyBytes);
}

Register ORISCInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                            int &FrameIndex,
                                            TypeSize &MemBytes) const {
    switch (MI.getOpcode()) {
    default:
        return 0;
    case ORISC::LBS:
    case ORISC::LBU:
    case ORISC::LBS_I:
    case ORISC::LBU_I:
        MemBytes = TypeSize::getFixed(1);
        break;
    case ORISC::LHS:
    case ORISC::LHU:
    case ORISC::LHS_I:
    case ORISC::LHU_I:
        MemBytes = TypeSize::getFixed(2);
        break;
    //case ORISC::LWS:
    case ORISC::LW:
    //case ORISC::LWS_I
    case ORISC::LW_I:
    case ORISC::RSTRPC:
        MemBytes = TypeSize::getFixed(4);
        break;
    //case ORISC::LD:
    //    MemBytes = TypeSize::getFixed(8);
    //    break;
    }

    if (MI.getOperand(2).isFI()) {
        FrameIndex = MI.getOperand(2).getIndex();
        return MI.getOperand(0).getReg();
    }

    return 0;
}
void ORISCInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
      int FrameIndex, const TargetRegisterClass *RC, Register VReg,
      unsigned SubReg, MachineInstr::MIFlag Flags) const {
    MachineFunction *MF = MBB.getParent();
    MachineFrameInfo &MFI = MF->getFrameInfo();
    DebugLoc DL =
        Flags & MachineInstr::FrameDestroy ? MBB.findDebugLoc(MI) : DebugLoc();

    MachineMemOperand *MMO = MF->getMachineMemOperand(
        MachinePointerInfo::getFixedStack(*MF, FrameIndex), MachineMemOperand::MOLoad,
        MFI.getObjectSize(FrameIndex), MFI.getObjectAlign(FrameIndex));

    unsigned int Opcode;
    if (DestReg == ORISC::P31)
        Opcode = ORISC::RSTRPC;
    else if (ORISC::PRRegClass.contains(DestReg))
        Opcode = ORISC::LP_I;
    else
        Opcode = ORISC::LW_I;

    BuildMI(MBB, MI, DebugLoc(), get(Opcode))
        .addReg(DestReg)
        .addReg(TRI.getFrameRegister(*MF))
        .addFrameIndex(FrameIndex)
        .addMemOperand(MMO)
        .setMIFlag(Flags);
}

Register ORISCInstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                            int &FrameIndex) const {
    TypeSize DummyBytes = TypeSize::getZero();
    return isStoreToStackSlot(MI, FrameIndex, DummyBytes);
}

Register ORISCInstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                            int &FrameIndex,
                                            TypeSize &MemBytes) const {
    switch (MI.getOpcode()) {
    default:
        return 0;
    case ORISC::SB:
    case ORISC::SB_I:
        MemBytes = TypeSize::getFixed(1);
        break;
    case ORISC::SH:
    case ORISC::SH_I:
        MemBytes = TypeSize::getFixed(2);
        break;
    case ORISC::SW:
    case ORISC::SW_I:
    case ORISC::SVRPC:
        MemBytes = TypeSize::getFixed(4);
        break;
    //case ORISC::SD:
    //    MemBytes = TypeSize::getFixed(8);
    //    break;
    }

    if (MI.getOperand(1).isFI()) {
        FrameIndex = MI.getOperand(1).getIndex();
        return MI.getOperand(2).getReg();
    }

    return 0;
}

void ORISCInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
                                            bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
                                            MachineInstr::MIFlag Flags) const {
    MachineFunction *MF = MBB.getParent();
    MachineFrameInfo &MFI = MF->getFrameInfo();

    MachineMemOperand *MMO = MF->getMachineMemOperand(
        MachinePointerInfo::getFixedStack(*MF, FrameIndex), MachineMemOperand::MOLoad,
        MFI.getObjectSize(FrameIndex), MFI.getObjectAlign(FrameIndex));

    unsigned int Opcode;
    if (SrcReg == ORISC::P31)
        Opcode = ORISC::SVRPC;
    else if (ORISC::PRRegClass.contains(SrcReg))
        Opcode = ORISC::SP_I;
    else
        Opcode = ORISC::SW_I;

    BuildMI(MBB, MI, DebugLoc(), get(Opcode))
        .addReg(TRI.getFrameRegister(*MF), getKillRegState(isKill))
        .addFrameIndex(FrameIndex)
        .addReg(SrcReg, getKillRegState(isKill))
        .addMemOperand(MMO)
        .setMIFlag(Flags);
}



/// Analyze the branching code at the end of MBB, returning
/// true if it cannot be understood (e.g. it's a switch dispatch or isn't
/// implemented for a target).  Upon success, this returns false and returns
/// with the following information in various cases:
///
/// 1. If this block ends with no branches (it just falls through to its succ)
///    just return false, leaving TBB/FBB null.
/// 2. If this block ends with only an unconditional branch, it sets TBB to be
///    the destination block and returns false.
/// 3. If this block ends with a conditional branch and it falls through to a
///    successor block, it sets TBB to be the branch destination block,
///    cond[0] to the bcc opc, cond[1] to the operand-register of the bcc
//     and finally returns false.
/// 4. If this block ends with a conditional branch followed by an
///    unconditional branch ("Two-Way Branch"), it returns the 
///    'true' destination in TBB, the 'false' destination in FBB,
///    cond[0] to the bcc opc, cond[1] to the operand-register of the bcc
//     and finally returns false.
bool ORISCInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                        MachineBasicBlock *&TBB,
                                        MachineBasicBlock *&FBB,
                                        SmallVectorImpl<MachineOperand> &Cond,
                                        bool AllowModify) const {
    TBB = nullptr;
    FBB = nullptr;
    //Set the iterator to the end of the BB and walk from the end to the top
    MachineBasicBlock::instr_iterator I = MBB.instr_end();

    //if this block is empty, we dont need to do anything
    if (I == MBB.instr_begin())
        return false;
    --I;

    // Walk upwards until we encounter a branch
    while (isPredicated(*I) || I->isTerminator() || I->isDebugValue()) {
        // Flag to be raised on unanalyzeable instructions. This is useful in cases
        // where we want to clean up on the end of the basic block before we bail
        // out.
        bool CantAnalyze = false;

        // Skip over DEBUG values, predicated nonterminators and speculation
        // barrier terminators.
        while (I->isDebugInstr() || !I->isTerminator()){
            if (I == MBB.instr_begin())
                return false;
            --I;
        }

        if (isUncondJumpOpcode(I->getOpcode())) {
            // Jumps can't be analyzed, but we still want
            // to clean up any instructions at the tail of the basic block.
            CantAnalyze = true;
        } else if (isUncondBranchOpcode(I->getOpcode())) {
            // STANDARD CASE 2
            TBB = I->getOperand(0).getMBB();
        } else if (isCondBranchOpcode(I->getOpcode())) {
            // Bail if this is part of a cond-branch chain (too complex to analyze)
            if (!Cond.empty())
                return true;

            // STANDARD CASE 4
            assert(!FBB && "FBB should have been null.");
            FBB = TBB;
            TBB = I->getOperand(2).getMBB();
            Cond.push_back(MachineOperand::CreateImm(I->getOpcode()));
            Cond.push_back(I->getOperand(0));
            Cond.push_back(I->getOperand(1));
        } else if (I->isReturn()) {
            // Returns can't be analyzed, but we should run cleanup.
            CantAnalyze = true;
        } else {
            // We encountered other unrecognized terminator. Bail out immediately.
            return true;
        }

        // Cleanup code - to be run for unpredicated unconditional branches,
        //                jumps and returns.
        if (!isPredicated(*I) &&
                (isUncondBranchOpcode(I->getOpcode()) ||
                isUncondJumpOpcode(I->getOpcode()) ||
                I->isReturn())) {
            // Forget any previous condition branch information - it no longer applies.
            Cond.clear();
            FBB = nullptr;

            // If we can modify the function, delete everything below this
            // unconditional branch.
            if (AllowModify) {
                MachineBasicBlock::iterator DI = std::next(I);
                while (DI != MBB.instr_end()) {
                    MachineInstr &InstToDelete = *DI;
                    ++DI;
                    InstToDelete.eraseFromParent();
                }
            }
        }

        if (CantAnalyze) {
            // We may not be able to analyze the block, but we could still have
            // an unconditional branch as the last instruction in the block, which
            // just branches to layout successor. If this is the case, then just
            // remove it if we're allowed to make modifications.
            if (AllowModify && !isPredicated(MBB.back()) &&
                isUncondBranchOpcode(MBB.back().getOpcode()) &&
                TBB && MBB.isLayoutSuccessor(TBB))
                removeBranch(MBB);
            return true;
        }

        if (I == MBB.instr_begin())
            return false;

        --I;
    }

    // STANDARD CASE 3
    // We made it past the terminators without bailing out - we must have
    // analyzed this branch successfully.
    return false;
}

//is called if it turns out the branch (or Two-Way Branch)
//at the end of a MBB is not needed anymore.
//returns number of instructions removed
unsigned ORISCInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                        int *BytesRemoved) const {

    MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
    //if we are past the end of the mbb, dont remove anything
    if (I == MBB.end())
        return 0;

    //if this is not a branch, dont remove anything
    if (!isUncondBranchOpcode(I->getOpcode()) &&
        !isCondBranchOpcode(I->getOpcode()))
        return 0;

    // Remove the branch.
    I->eraseFromParent();

    //if now there is only one instruction left in the block
    //leave it at that
    I = MBB.end();
    if (I == MBB.begin()) return 1;

    //if before the uncond. branch there is no
    //cond. branch, we are done
    --I;
    if (!isCondBranchOpcode(I->getOpcode()))
        return 1;

    // Also remove the uncond. branch.
    I->eraseFromParent();
    return 2;
}

//is called if it turns out if a branch (or Two-Way Branch)
//has to be inserted at the end of MBB.
//returns number of instructions added
unsigned ORISCInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                        MachineBasicBlock *TBB,
                                        MachineBasicBlock *FBB,
                                        ArrayRef<MachineOperand> Cond,
                                        const DebugLoc &DL,
                                        int *BytesAdded) const {

    // Shouldn't be a fall through.
    assert(TBB && "insertBranch must not be told to insert a fallthrough");
    assert((Cond.size() == 3 || Cond.size() == 0) &&
            "ORISC Cond. Branches have exactly 3 elements in Cond");

    int BraOpc =  ORISC::BRA;
    int BccOpc = Cond.size() == 3 ? Cond[0].getImm() : 0;

    // For conditional branches, we use addOperand to preserve CPSR flags.
    if (!FBB) {
        if (Cond.empty()) { // Unconditional branch?
            BuildMI(&MBB, DL, get(BraOpc)).addMBB(TBB);
        } else {
            BuildMI(&MBB, DL, get(BccOpc))
                .add(Cond[1])
                .add(Cond[2])
                .addMBB(TBB);
        }
        return 1;
    }

    // Two-way conditional branch.
    BuildMI(&MBB, DL, get(BccOpc))
        .add(Cond[1])
        .add(Cond[2])
        .addMBB(TBB);
    BuildMI(&MBB, DL, get(BraOpc)).addMBB(FBB);
    return 2;
}

//Cond is Cond[0] Bcc Opc as Imm, Cond[1] is register
//Invert Cond[0] and return false if inversion is possible
//else return true
bool ORISCInstrInfo::
reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const {
    if (Cond.size() == 2 && Cond[0].isImm()) {
        switch(Cond[0].getImm()) {
            case ORISC::BEQ:
                Cond[0].setImm(ORISC::BNE);
                break;
            case ORISC::BNE:
                Cond[0].setImm(ORISC::BEQ);
                break;
            case ORISC::BEQP:
                Cond[0].setImm(ORISC::BNEP);
                break;
            case ORISC::BNEP:
                Cond[0].setImm(ORISC::BEQP);
                break;
            case ORISC::BLTU:
                Cond[0].setImm(ORISC::BGEU);
                break;
            case ORISC::BLTS:
                Cond[0].setImm(ORISC::BGES);
                break;
            case ORISC::BGES:
                Cond[0].setImm(ORISC::BLTS);
                break;
            case ORISC::BGEU:
                Cond[0].setImm(ORISC::BLTU);
                break;
            default:
                return true;
        }
        return false;
    }
    return true;
}