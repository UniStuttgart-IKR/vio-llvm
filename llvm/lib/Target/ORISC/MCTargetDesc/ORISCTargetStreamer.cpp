//===-- ORISCTargetStreamer.cpp - ORISC Target Streamer Methods ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides ORISC specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "ORISCTargetStreamer.h"
#include "ORISCInstPrinter.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

ORISCTargetStreamer::ORISCTargetStreamer(MCStreamer &S)
    : MCTargetStreamer(S) {}

ORISCTargetAsmStreamer::ORISCTargetAsmStreamer(MCStreamer &S,
                                                 formatted_raw_ostream &OS)
    : ORISCTargetStreamer(S), OS(OS) {}
    
void ORISCTargetAsmStreamer::changeSection(const MCSection *CurSection, MCSection *Section,
                             uint32_t SubSection, raw_ostream &OS) {
  MCTargetStreamer::changeSection(CurSection, Section, SubSection, OS);
  /*if (Section->getName() == ".text") {
    OS << "@user:" << "\n\tpublic" << "\n\tprivate" << "\n\n";
  } else {
    //MCTargetStreamer::changeSection(CurSection, Section, SubSection, OS);
  } */
}



ORISCTargetELFStreamer::ORISCTargetELFStreamer(MCStreamer &S)
    : ORISCTargetStreamer(S) {}

MCELFStreamer &ORISCTargetELFStreamer::getStreamer() {
  return static_cast<MCELFStreamer &>(Streamer);
}
