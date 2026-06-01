//===-- ORISCMCObjectWriter.cpp - ORISC ELF writer ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/ORISCMCTargetDesc.h"
#include "ORISCFixupKinds.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"
#include <cassert>
#include <cstdint>

using namespace llvm;

namespace {
class ORISCObjectWriter : public MCELFObjectTargetWriter {
public:
  ORISCObjectWriter(uint8_t OSABI);

  virtual ~ORISCObjectWriter();

protected:
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                                bool IsPCRel) const override;
  bool needsRelocateWithSymbol(const MCValue &, unsigned Type) const override;
};
} // namespace

ORISCObjectWriter::ORISCObjectWriter(uint8_t OSABI)
    : MCELFObjectTargetWriter(false, OSABI, ELF::EM_ORISC,
                              /*HasRelocationAddend=*/false) {}

ORISCObjectWriter::~ORISCObjectWriter() {}

unsigned ORISCObjectWriter::getRelocType(const MCFixup &Fixup, const MCValue &Target,
                                          bool IsPCRel) const {
  auto Kind = Fixup.getKind();
  auto Spec = Target.getSpecifier();
  switch (Spec) {
    default:
      return ELF::R_ORISC_NONE;
    case ORISC::fixup_orisc_ctxt_idx:
      return ELF::R_ORISC_CTXT_IDX;
    case ORISC::fixup_orisc_jlib_idx:
      return ELF::R_ORISC_JLIB_IDX;
    case ORISC::fixup_orisc_branch_25:
      return ELF::R_ORISC_BRANCH_25;
    case ORISC::fixup_orisc_branch_12:
      return ELF::R_ORISC_BRANCH_12;
  }
  return ELF::EM_ORISC;
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createORISCELFObjectWriter(uint8_t OSABI, bool Is64Bit) {
  return std::make_unique<ORISCObjectWriter>(OSABI);
}

bool ORISCObjectWriter::needsRelocateWithSymbol(const MCValue &, unsigned Type) const {
  return false;
}
