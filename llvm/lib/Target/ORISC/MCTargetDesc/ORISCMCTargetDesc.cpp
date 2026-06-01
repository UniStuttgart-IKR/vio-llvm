//===-- ORISCMCTargetDesc.cpp - ORISC Target Descriptions -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides ORISC specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "ORISCMCTargetDesc.h"
#include "ORISCELFStreamer.h"
#include "ORISCInstPrinter.h"
#include "ORISCMCAsmInfo.h"
#include "ORISCMCCodeEmitter.h"
#include "ORISCTargetStreamer.h"
#include "TargetInfo/ORISCTargetInfo.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

namespace ORISCOp {
    const static unsigned OPERAND_UNKNOWN = llvm::MCOI::OPERAND_UNKNOWN;
}

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "ORISCGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "ORISCGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "ORISCGenSubtargetInfo.inc"

using namespace llvm;

static MCRegisterInfo *createORISCMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitORISCMCRegisterInfo(X, ORISC::P31);
  return X;
}

static MCInstrInfo *createORISCMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitORISCMCInstrInfo(X);
  return X;
}

static MCInstPrinter *createORISCMCInstPrinter(const Triple &T,
                                                   unsigned SyntaxVariant,
                                                   const MCAsmInfo &MAI,
                                                   const MCInstrInfo &MII,
                                                   const MCRegisterInfo &MRI) {
  return new ORISCInstPrinter(MAI, MII, MRI);
}

static MCAsmInfo *createORISCMCAsmInfo(const MCRegisterInfo &MRI,
                                           const Triple &TT,
                                           const MCTargetOptions &Options) {
  MCAsmInfo *MAI = new ORISCMCAsmInfo(TT);

  return MAI;
}

static MCSubtargetInfo *
createORISCMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
    return createORISCMCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

static MCTargetStreamer *
createORISCAsmTargetStreamer(MCStreamer &S, formatted_raw_ostream &OS,
                              MCInstPrinter *InstPrint) {
  return new ORISCTargetAsmStreamer(S, OS);
}

static MCTargetStreamer *
createORISCObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
  return new ORISCTargetELFStreamer(S);
}


extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeORISCTargetMC() {
    for (Target *T : {&getTheORISCTarget()}) {
        TargetRegistry::RegisterMCRegInfo(*T, createORISCMCRegisterInfo);
        TargetRegistry::RegisterMCInstrInfo(*T, createORISCMCInstrInfo);
        TargetRegistry::RegisterMCSubtargetInfo(*T, createORISCMCSubtargetInfo);
        TargetRegistry::RegisterMCAsmInfo(*T, createORISCMCAsmInfo);
        TargetRegistry::RegisterMCCodeEmitter(*T, createORISCMCCodeEmitter);
        TargetRegistry::RegisterMCAsmBackend(*T, createORISCMCAsmBackend);
        TargetRegistry::RegisterMCInstPrinter(*T, createORISCMCInstPrinter);
        //TargetRegistry::RegisterMCInstrAnalysis(*T, createLoongArchInstrAnalysis);
        TargetRegistry::RegisterELFStreamer(*T, createORISCELFStreamer);
        TargetRegistry::RegisterObjectTargetStreamer( *T, createORISCObjectTargetStreamer);
        TargetRegistry::RegisterAsmTargetStreamer(*T, createORISCAsmTargetStreamer);
    }
}
