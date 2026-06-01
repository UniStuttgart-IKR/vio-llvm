//===-- IKRRISC2MCTargetDesc.cpp - IKRRISC2 Target Descriptions -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides IKRRISC2 specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "IKRRISC2MCTargetDesc.h"
#include "IKRRISC2InstPrinter.h"
#include "IKRRISC2MCAsmInfo.h"
#include "TargetInfo/IKRRISC2TargetInfo.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

namespace IKRRISC2Op {
    const static unsigned OPERAND_UNKNOWN = llvm::MCOI::OPERAND_UNKNOWN;
}

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "IKRRISC2GenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "IKRRISC2GenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "IKRRISC2GenSubtargetInfo.inc"

using namespace llvm;

static MCRegisterInfo *createIKRRISC2MCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitIKRRISC2MCRegisterInfo(X, IKRRISC2::R31);
  return X;
}

static MCInstrInfo *createIKRRISC2MCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitIKRRISC2MCInstrInfo(X);
  return X;
}

static MCInstPrinter *createIKRRISC2MCInstPrinter(const Triple &T,
                                                   unsigned SyntaxVariant,
                                                   const MCAsmInfo &MAI,
                                                   const MCInstrInfo &MII,
                                                   const MCRegisterInfo &MRI) {
  return new IKRRISC2InstPrinter(MAI, MII, MRI);
}

static MCAsmInfo *createIKRRISC2MCAsmInfo(const MCRegisterInfo &MRI,
                                           const Triple &TT,
                                           const MCTargetOptions &Options) {
  MCAsmInfo *MAI = new IKRRISC2MCAsmInfo(TT, Options);

  return MAI;
}

static MCSubtargetInfo *
createIKRRISC2MCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
    return createIKRRISC2MCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeIKRRISC2TargetMC() {
    for (Target *T : {&getTheIKRRISC2Target()}) {
        TargetRegistry::RegisterMCRegInfo(*T, createIKRRISC2MCRegisterInfo);
        TargetRegistry::RegisterMCInstrInfo(*T, createIKRRISC2MCInstrInfo);
        TargetRegistry::RegisterMCSubtargetInfo(*T, createIKRRISC2MCSubtargetInfo);
        TargetRegistry::RegisterMCAsmInfo(*T, createIKRRISC2MCAsmInfo);
        //TargetRegistry::RegisterMCCodeEmitter(*T, createLoongArchMCCodeEmitter);
        //TargetRegistry::RegisterMCAsmBackend(*T, createLoongArchAsmBackend);
        TargetRegistry::RegisterMCInstPrinter(*T, createIKRRISC2MCInstPrinter);
        //TargetRegistry::RegisterMCInstrAnalysis(*T, createLoongArchInstrAnalysis);
        //TargetRegistry::RegisterELFStreamer(*T, createLoongArchELFStreamer);
        //TargetRegistry::RegisterObjectTargetStreamer(
        //    *T, createLoongArchObjectTargetStreamer);
        //TargetRegistry::RegisterAsmTargetStreamer(*T,
        //                                          createLoongArchAsmTargetStreamer);
    }
}
