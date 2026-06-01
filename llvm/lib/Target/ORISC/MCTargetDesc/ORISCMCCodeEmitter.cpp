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
#include "ORISCFixupKinds.h"
#include "ORISCMCTargetDesc.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constant.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
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
    const MCOperand &MO = MI.getOperand(OpNum);
    if (MO.isImm())
        return static_cast<uint32_t>(MO.getImm());
    if (MO.isExpr()) {
        int64_t Res;
        // This checks if the expression can be resolved to a number right now
        if (MO.getExpr()->evaluateAsAbsolute(Res)) {
            return static_cast<uint32_t>(Res);
        }
    }
    
    switch (MI.getOpcode()) {
        case ORISC::BRA:
        case ORISC::BSR:
            Fixups.push_back(MCFixup::create(
                0, MO.getExpr(), MCFixupKind(ORISC::fixup_orisc_branch_25), MI.getLoc()));
            return 0;

        case ORISC::BEQ:
        case ORISC::BNE:
        case ORISC::BGEU:
        case ORISC::BGES:
        case ORISC::BLTU:
        case ORISC::BLTS:
        case ORISC::BEQP:
        case ORISC::BNEP:
            Fixups.push_back(MCFixup::create(
                0, MO.getExpr(), MCFixupKind(ORISC::fixup_orisc_branch_12), MI.getLoc()));
            return 0;
        default:
            LLVM_DEBUG(MI.dump());
            LLVM_DEBUG(MO.dump());
            report_fatal_error("Unhandled Branch Target!");
            return 0;
    }
}

uint32_t ORISCMCCodeEmitter::getExternalSymbolEncoding(
    const MCInst &MI, unsigned int OpNum, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
    const MCOperand &MO = MI.getOperand(OpNum);
    const MCExpr *Expr = MO.getExpr();
    
    switch (MI.getOpcode()) {
    case ORISC::JLIB:
        Fixups.push_back(MCFixup::create(
            0, Expr, MCFixupKind(ORISC::fixup_orisc_jlib_idx), MI.getLoc()));
        return 0;
    case ORISC::LP_I:
    //case ORISC::LD_I:
    //case ORISC::LWS_I:
    case ORISC::LW_I:
    case ORISC::LHS_I:
    case ORISC::LHU_I:
    case ORISC::LBS_I:
    case ORISC::LBU_I:
        Fixups.push_back(MCFixup::create(
            0, Expr, MCFixupKind(ORISC::fixup_orisc_ctxt_idx), MI.getLoc()));
        return 0;
    default:
        LLVM_DEBUG(MI.dump());
        LLVM_DEBUG(MO.dump());
        llvm_unreachable("Don't know how to emit this operand");
        return 0;
    }
    return 0;
}

uint64_t
ORISCMCCodeEmitter::getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                                       SmallVectorImpl<MCFixup> &Fixups,
                                       const MCSubtargetInfo &STI) const {

    if (MO.isExpr() && MO.isBareSymbolRef()) {
        for (unsigned O = 0; O < MI.getNumOperands(); ++O)
            if (&MI.getOperand(O) == &MO)
                return getExternalSymbolEncoding(MI, O, Fixups, STI);
    }

    if (MO.isReg())
        return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());

    if (MO.isImm())
        return MO.getImm();

    LLVM_DEBUG(MO.dump());
    llvm_unreachable("Don't know how to emit this operand");
    return 0;
}

void ORISCMCCodeEmitter::encodeInstruction(const MCInst &MI,
                                            SmallVectorImpl<char> &CB,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
    const MCInstrDesc &Desc = MCII.get(MI.getOpcode());
    // Get byte count of instruction.
    unsigned Size = Desc.getSize();
    switch (Size) {
        default:
            llvm_unreachable("Unhandled encodeInstruction length!");
        case 2: {
            llvm_unreachable("Unhandled encodeInstruction length! (2)");
            //uint16_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
            //support::endian::write<uint16_t>(CB, Bits, llvm::endianness::little);
            break;
        }
        case 4: {
            uint32_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
            support::endian::write(CB, Bits, llvm::endianness::big);
            break;
        }
    }
    ++MCNumEmitted;
}

#include "ORISCGenMCCodeEmitter.inc"
