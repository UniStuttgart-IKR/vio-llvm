//===-- ORISCInstrInfo.h - RISC-V Instruction Information -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the RISC-V implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ORISC_ORISCINSTRINFO_H
#define LLVM_LIB_TARGET_ORISC_ORISCINSTRINFO_H

#include "ORISCRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DiagnosticInfo.h"

#define GET_INSTRINFO_HEADER
#define GET_INSTRINFO_OPERAND_ENUM
#include "ORISCGenInstrInfo.inc"
#include "ORISCGenRegisterInfo.inc"

namespace llvm {

class ORISCSubtarget;

class ORISCInstrInfo : public ORISCGenInstrInfo {
public:
    explicit ORISCInstrInfo(ORISCSubtarget &STI);

    MCInst getNop() const override;

    void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                    const DebugLoc &DL, Register DestReg, Register SrcReg,
                    bool KillSrc, bool RenamableDest = false,
                    bool RenamableSrc = false) const override;

    Register isLoadFromStackSlot(const MachineInstr &MI,
                                int &FrameIndex) const override;

    Register isLoadFromStackSlot(const MachineInstr &MI,
                                int &FrameIndex,
                                TypeSize &MemBytes) const override;

    Register isStoreToStackSlot(const MachineInstr &MI,
                                int &FrameIndex) const override;

    Register isStoreToStackSlot(const MachineInstr &MI,
                                int &FrameIndex, TypeSize &MemBytes) const override;

    void storeRegToStackSlot(
        MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register SrcReg,
        bool isKill, int FrameIndex, const TargetRegisterClass *RC,
        const TargetRegisterInfo *TRI, Register VReg,
        MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

    void loadRegFromStackSlot(
        MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
        Register DestReg, int FrameIdx, const TargetRegisterClass *RC,
        const TargetRegisterInfo *TRI, Register VReg,
        MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

    // Branch analysis.
    bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                        MachineBasicBlock *&FBB,
                        SmallVectorImpl<MachineOperand> &Cond,
                        bool AllowModify = false) const override;
    unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;
    unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                            MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                            const DebugLoc &DL,
                            int *BytesAdded = nullptr) const override;
    bool
    reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const override;

private:

    static inline
    bool isCondBranchOpcode(int Opc) {
        return Opc == ORISC::BEQP || Opc == ORISC::BNEP || Opc == ORISC::BEQ  || Opc == ORISC::BNE 
            || Opc == ORISC::BGEU || Opc == ORISC::BGES || Opc == ORISC::BLTU || Opc == ORISC::BLTS;
    }

    static inline
    bool isUncondBranchOpcode(int Opc) {
        return Opc == ORISC::BRA || Opc == ORISC::BSR;
    }

    static inline
    bool isUncondJumpOpcode(int Opc) {
        return Opc == ORISC::JMP || Opc == ORISC::JSR || Opc == ORISC::JLIB;
    }

protected:
  const ORISCSubtarget &STI;

};
}
#endif