//===-- ORISCTargetStreamer.h - ORISC Target Streamer --------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ORISC_ORISCTARGETSTREAMER_H
#define LLVM_LIB_TARGET_ORISC_ORISCTARGETSTREAMER_H

#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/SMLoc.h"

namespace llvm {
class formatted_raw_ostream;

class ORISCTargetStreamer : public MCTargetStreamer {
public:
  ORISCTargetStreamer(MCStreamer &S);
};

class ORISCTargetAsmStreamer : public ORISCTargetStreamer {
  formatted_raw_ostream &OS;

public:
  ORISCTargetAsmStreamer(MCStreamer &S, formatted_raw_ostream &OS);
  void changeSection(const MCSection *CurSection, MCSection *Section,
                             uint32_t SubSection, raw_ostream &OS) override;
};

class ORISCTargetELFStreamer : public ORISCTargetStreamer {
public:
  ORISCTargetELFStreamer(MCStreamer &S);
  MCELFStreamer &getStreamer();
};
} // end namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCTARGETSTREAMER_H
