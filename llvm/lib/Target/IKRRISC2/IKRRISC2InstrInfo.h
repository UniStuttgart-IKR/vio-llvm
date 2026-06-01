//===-- IKRRISC2InstrInfo.h - RISC-V Instruction Information -------*- C++ -*-===//
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

#ifndef LLVM_LIB_TARGET_IKRRISC2_IKRRISC2INSTRINFO_H
#define LLVM_LIB_TARGET_IKRRISC2_IKRRISC2INSTRINFO_H

#include "IKRRISC2RegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DiagnosticInfo.h"

#define GET_INSTRINFO_HEADER
#define GET_INSTRINFO_OPERAND_ENUM
#include "IKRRISC2GenInstrInfo.inc"
#include "IKRRISC2GenRegisterInfo.inc"

namespace llvm {

class IKRRISC2Subtarget;

class IKRRISC2InstrInfo : public IKRRISC2GenInstrInfo {
public:
    explicit IKRRISC2InstrInfo(const TargetSubtargetInfo &STI, const TargetRegisterInfo &TRI);

    void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                    const DebugLoc &DL, Register DestReg, Register SrcReg,
                    bool KillSrc, bool RenamableDest = false,
                    bool RenamableSrc = false) const override;

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
        return Opc == IKRRISC2::BEQ || Opc == IKRRISC2::BNE || Opc == IKRRISC2::BLT 
            || Opc == IKRRISC2::BGT || Opc == IKRRISC2::BLE || Opc == IKRRISC2::BGE;
    }

    static inline
    bool isUncondBranchOpcode(int Opc) {
        return Opc == IKRRISC2::BRA || Opc == IKRRISC2::BSR;
    }

    static inline
    bool isUncondJumpOpcode(int Opc) {
        return Opc == IKRRISC2::JMP || Opc == IKRRISC2::JSR;
    }

};
}
#endif