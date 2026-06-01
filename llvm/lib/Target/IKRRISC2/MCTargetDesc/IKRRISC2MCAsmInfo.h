//===-- IKRRISC2MCAsmInfo.h - IKRRISC2 Asm Info --------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the IKRRISC2MCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_IKRRISC2_MCTARGETDESC_IKRRISC2MCASMINFO_H
#define LLVM_LIB_TARGET_IKRRISC2_MCTARGETDESC_IKRRISC2MCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class IKRRISC2MCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit IKRRISC2MCAsmInfo(const Triple &TT, const MCTargetOptions &Options);
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_IKRRISC2_MCTARGETDESC_IKRRISC2MCASMINFO_H
