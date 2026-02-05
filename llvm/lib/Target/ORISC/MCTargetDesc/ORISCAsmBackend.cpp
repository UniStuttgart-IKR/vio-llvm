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
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace llvm {
class MCObjectTargetWriter;
class ORISCMCAsmBackend : public MCAsmBackend {
  uint8_t OSABI;
  const MCSubtargetInfo &STI;

public:
  ORISCMCAsmBackend(const MCSubtargetInfo &STI, uint8_t OSABI)
      : MCAsmBackend(llvm::endianness::little), OSABI(OSABI),
        STI(STI) {}

  const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const override;
  void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                  const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsResolved,
                  const MCSubtargetInfo *STI) const override;
  bool mayNeedRelaxation(const MCInst &Inst,
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

const MCFixupKindInfo &
ORISCMCAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  const static MCFixupKindInfo Infos[ORISC::NumTargetFixupKinds] = {
      // name                     offset bits  flags
      {"fixup_orisc_ctxt_idx", 0, 32, MCFixupKindInfo::FKF_IsTarget},
      {"fixup_orisc_jlib_idx", 0, 32, MCFixupKindInfo::FKF_IsTarget},
      {"fixup_ORISC_jump_18", 6, 18, MCFixupKindInfo::FKF_IsPCRel}};

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);
  assert(unsigned(Kind - FirstTargetFixupKind) < ORISC::NumTargetFixupKinds &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

static uint64_t adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                 MCContext &Ctx) {
  unsigned Kind = Fixup.getKind();
  return 0;
}

void ORISCMCAsmBackend::applyFixup(const MCAssembler &Asm,
                                    const MCFixup &Fixup, const MCValue &Target,
                                    MutableArrayRef<char> Data, uint64_t Value,
                                    bool IsResolved,
                                    const MCSubtargetInfo *STI) const {
  report_fatal_error("Not Implemented Yet");
}

bool ORISCMCAsmBackend::mayNeedRelaxation(const MCInst &Inst,
                                           const MCSubtargetInfo &STI) const {
  return false;
}

void ORISCMCAsmBackend::relaxInstruction(MCInst &Inst,
                                          const MCSubtargetInfo &STI) const {}

bool ORISCMCAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                      const MCSubtargetInfo *STI) const {
  OS.write("\x00", 1);
  OS.write("\x00", 1);
  OS.write("\x00", 1);
  OS.write("\x00", 1);
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
