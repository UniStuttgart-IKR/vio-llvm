//===-- ORISCMCTargetDesc.h - ORISC Target Descriptions ---------*- C++ -*-===//
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

#ifndef LLVM_LIB_TARGET_ORISC_MCTARGETDESC_ORISCMCTARGETDESC_H
#define LLVM_LIB_TARGET_ORISC_MCTARGETDESC_ORISCMCTARGETDESC_H

#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/DataTypes.h"

#include <memory>

namespace llvm {
class MCContext;
class MCInstrInfo;
class MCRegisterInfo;
class MCSubtargetInfo;
class Target;

MCCodeEmitter *createORISCMCCodeEmitter(const MCInstrInfo &MCII,
                                      MCContext &Ctx);

MCAsmBackend *createORISCMCAsmBackend(const Target &T,
                                       const MCSubtargetInfo &STI,
                                       const MCRegisterInfo &MRI,
                                       const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter>
createORISCELFObjectWriter(uint8_t OSABI, bool Is64Bit);

}


// Defines symbolic names for ORISC registers.
#define GET_REGINFO_ENUM
#include "ORISCGenRegisterInfo.inc"

// Defines symbolic names for ORISC instructions.
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "ORISCGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "ORISCGenSubtargetInfo.inc"

#endif
