//===-- ORISCMCCodeEmitter.cpp - Convert ORISC code to machine code -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the ORISCMCCodeEmitter class.
//
//===----------------------------------------------------------------------===//

#include "ORISCMCCodeEmitter.h"
#include "ORISCMCTargetDesc.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constant.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/TargetParser/Triple.h"
#include <cassert>
#include <cstdint>

using namespace llvm;

#define DEBUG_TYPE "mccodeemitter"

STATISTIC(MCNumEmitted, "Number of MC instructions emitted");

MCCodeEmitter *llvm::createORISCMCCodeEmitter(const MCInstrInfo &MCII,
                                            MCContext &Ctx) {
  return new ORISCMCCodeEmitter(MCII, Ctx);
}

uint32_t ORISCMCCodeEmitter::getBranchTargetEncoding(
    const MCInst &MI, unsigned int OpNum, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  report_fatal_error("Unhandled Branch Target!");
  return 0;
}

uint64_t
ORISCMCCodeEmitter::getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                                       SmallVectorImpl<MCFixup> &Fixups,
                                       const MCSubtargetInfo &STI) const {
  report_fatal_error("Unhandled Machine Op!");
  return 0;
}

void ORISCMCCodeEmitter::encodeInstruction(const MCInst &MI,
                                            SmallVectorImpl<char> &CB,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
  report_fatal_error("Instruction Ecoding Not Implemented!");
}

#include "ORISCGenMCCodeEmitter.inc"
