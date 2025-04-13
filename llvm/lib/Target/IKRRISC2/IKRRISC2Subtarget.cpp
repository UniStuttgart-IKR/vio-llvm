//===-- IKRRISC2Subtarget.cpp - IKRRISC2 Subtarget Information ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements the IKRRISC2 specific subclass of TargetSubtargetInfo.
///
//===----------------------------------------------------------------------===//

#include "IKRRISC2Subtarget.h"

#include "IKRRISC2RegisterInfo.h"
#include "IKRRISC2TargetMachine.h"

#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "IKRRISC2-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "IKRRISC2GenSubtargetInfo.inc"

void IKRRISC2Subtarget::anchor() {}

IKRRISC2Subtarget::IKRRISC2Subtarget(const Triple &TT, StringRef CPU, StringRef FS,
                             const IKRRISC2TargetMachine &TM)
    : IKRRISC2GenSubtargetInfo(TT, CPU, /*TuneCPU*/ CPU, FS),
      FrameLowering(), RegisterInfo(), InstrInfo(*this),
      TLInfo(TM, *this), TSInfo() {
        
}