//=- IKRRISC2InstrInfo.cpp - IKRRISC2 Instruction Information -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the IKRRISC2 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "IKRRISC2InstrInfo.h"
#include "IKRRISC2RegisterInfo.h"
#include "MCTargetDesc/IKRRISC2MCTargetDesc.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/MC/MCInstBuilder.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "IKRRISC2GenInstrInfo.inc"

IKRRISC2InstrInfo::IKRRISC2InstrInfo(IKRRISC2Subtarget &STI)
    : IKRRISC2GenInstrInfo() {}



void IKRRISC2InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MBBI,
                                    const DebugLoc &DL, Register DestReg,
                                    Register SrcReg, bool KillSrc,
                                    bool RenamableDest, bool RenamableSrc) const {
    if (IKRRISC2::GPRRegClass.contains(DestReg, SrcReg))
        BuildMI(MBB, MBBI, DL, get(IKRRISC2::OR), DestReg)
            .addReg(SrcReg, getKillRegState(KillSrc))
            .addReg(SrcReg, getKillRegState(KillSrc));
    else
        report_fatal_error("Impossible reg-to-reg copy");
}
