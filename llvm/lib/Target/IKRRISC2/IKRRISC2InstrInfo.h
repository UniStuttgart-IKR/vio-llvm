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
    explicit IKRRISC2InstrInfo(IKRRISC2Subtarget &STI);

    void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                    const DebugLoc &DL, Register DestReg, Register SrcReg,
                    bool KillSrc, bool RenamableDest = false,
                    bool RenamableSrc = false) const override;

};
}
#endif