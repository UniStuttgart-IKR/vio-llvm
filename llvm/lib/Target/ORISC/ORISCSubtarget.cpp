//===-- ORISCSubtarget.cpp - ORISC Subtarget Information ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements the ORISC specific subclass of TargetSubtargetInfo.
///
//===----------------------------------------------------------------------===//

#include "ORISCSubtarget.h"

#include "ORISCRegisterInfo.h"
#include "ORISCTargetMachine.h"

#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "ORISC-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "ORISCGenSubtargetInfo.inc"

void ORISCSubtarget::anchor() {}

ORISCSubtarget::ORISCSubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                             const ORISCTargetMachine &TM)
    : ORISCGenSubtargetInfo(TT, CPU, /*TuneCPU*/ CPU, FS),
      FrameLowering(*this), RegisterInfo(), InstrInfo(*this),
      TLInfo(TM, *this), TSInfo() {
        
}