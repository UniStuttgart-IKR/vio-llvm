//===-- ORISCMCAsmBackend.cpp - ORISC assembler backend -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/ORISCFixupKinds.h"
#include "MCTargetDesc/ORISCMCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "asmbackend"

namespace llvm {
class MCObjectTargetWriter;
class ORISCMCAsmBackend : public MCAsmBackend {
    uint8_t OSABI;
    const MCSubtargetInfo &STI;

public:
    ORISCMCAsmBackend(const MCSubtargetInfo &STI, uint8_t OSABI)
        : MCAsmBackend(llvm::endianness::big), OSABI(OSABI),
            STI(STI) {}

    MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override;
    void applyFixup(const MCFragment &Fragment, const MCFixup &Fixup,
                          const MCValue &Target, uint8_t *Data, uint64_t Value,
                          bool IsResolved) override;
    bool mayNeedRelaxation(unsigned Opcode, ArrayRef<MCOperand> Operands,
                                 const MCSubtargetInfo &STI) const override;
    void relaxInstruction(MCInst &Inst,
                            const MCSubtargetInfo &STI) const override;
    bool writeNopData(raw_ostream &OS, uint64_t Count,
                        const MCSubtargetInfo *STI) const override;

    std::unique_ptr<MCObjectTargetWriter> createObjectTargetWriter() const override {
        return createORISCELFObjectWriter(OSABI, false);
    }
};
} // namespace llvm

MCFixupKindInfo 
ORISCMCAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  const static MCFixupKindInfo Infos[ORISC::NumTargetFixupKinds] = {
        {"fixup_orisc_ctxt_idx", 8, 12, 0},
        {"fixup_orisc_branch_12", 13, 12, 0},
        {"fixup_orisc_branch_25", 0, 25, 0},
        {"fixup_orisc_jlib_idx", 0, 20, 0}};

    if (Kind < FirstTargetFixupKind)
        return MCAsmBackend::getFixupKindInfo(Kind);
    assert(unsigned(Kind - FirstTargetFixupKind) < ORISC::NumTargetFixupKinds &&
            "Invalid kind!");
    return Infos[Kind - FirstTargetFixupKind];
}

static uint64_t adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                 MCContext &Ctx) {
    switch (Fixup.getKind()) {
        default:
            LLVM_DEBUG(dbgs() << "Number: " << Fixup.getKind() << "\n");
            llvm_unreachable("Unknown fixup kind!");
            return 0;
        case ORISC::fixup_orisc_branch_25:
            if (!isInt<25>(Value))
                Ctx.reportError(Fixup.getLoc(), "fixup value out of range");
            return Value;
        case ORISC::fixup_orisc_branch_12:
            if (!isInt<12>(Value))
                Ctx.reportError(Fixup.getLoc(), "fixup value out of range");

            unsigned Hi5 = (Value >> 7) & 0x1f;
            unsigned Lo7 = (Value & 0x7f) << 5;
            return (Lo7 | Hi5);
    }
}

void ORISCMCAsmBackend::applyFixup(const MCFragment &Fragment, const MCFixup &Fixup,
                                    const MCValue &Target, uint8_t *Data, uint64_t Value,
                                    bool IsResolved)  {
    MCFixupKind Kind = Fixup.getKind();
    if (Kind >= FirstLiteralRelocationKind)
        return;
    MCContext &Ctx = getContext();
    MCFixupKindInfo Info = getFixupKindInfo(Kind);
    if (!Value)
        return; // Doesn't change encoding.
    // Apply any target-specific value adjustments.
    Value = adjustFixupValue(Fixup, Value, Ctx);

    // Shift the value into position.
    Value <<= Info.TargetOffset;

    unsigned Offset = Fixup.getOffset();
    unsigned NumBytes = alignTo(Info.TargetSize + Info.TargetOffset, 8) / 8;

    //assert(Offset + NumBytes <= Data.size() && "Invalid fixup offset!");

    // For each byte of the fragment that the fixup touches, mask in the
    // bits from the fixup value.
    for (unsigned i = 0; i != NumBytes; ++i) {
        Data[Offset + i] |= uint8_t((Value >> (i * 8)) & 0xff);
    }
}

bool ORISCMCAsmBackend::mayNeedRelaxation(unsigned Opcode, ArrayRef<MCOperand> Operands,
                                 const MCSubtargetInfo &STI) const {
  return false;
}

void ORISCMCAsmBackend::relaxInstruction(MCInst &Inst,
                                          const MCSubtargetInfo &STI) const {}

bool ORISCMCAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                      const MCSubtargetInfo *STI) const {
    for (unsigned I = 0; I < Count; ++I) {
        OS.write("\x00", 1);
    }
    return true;
}

MCAsmBackend *llvm::createORISCMCAsmBackend(const Target &T,
                                             const MCSubtargetInfo &STI,
                                             const MCRegisterInfo &MRI,
                                             const MCTargetOptions &Options) {
    uint8_t OSABI =
        MCELFObjectTargetWriter::getOSABI(STI.getTargetTriple().getOS());
    return new llvm::ORISCMCAsmBackend(STI, OSABI);
}
