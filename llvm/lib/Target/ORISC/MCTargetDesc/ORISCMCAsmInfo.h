//===-- ORISCMCAsmInfo.h - ORISC Asm Info --------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the ORISCMCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ORISC_MCTARGETDESC_ORISCMCASMINFO_H
#define LLVM_LIB_TARGET_ORISC_MCTARGETDESC_ORISCMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class ORISCMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit ORISCMCAsmInfo(const Triple &TT, const MCTargetOptions &Options);
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_MCTARGETDESC_ORISCMCASMINFO_H
