//===-- IKRRISC2MCTargetDesc.h - IKRRISC2 Target Descriptions ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides IKRRISC2 specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_IKRRISC2_MCTARGETDESC_IKRRISC2MCTARGETDESC_H
#define LLVM_LIB_TARGET_IKRRISC2_MCTARGETDESC_IKRRISC2MCTARGETDESC_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"

#include <memory>

namespace llvm {
class MCContext;
class MCInstrInfo;
class MCRegisterInfo;
class MCSubtargetInfo;
class Target;
}

// Defines symbolic names for IKRRISC2 registers.
#define GET_REGINFO_ENUM
#include "IKRRISC2GenRegisterInfo.inc"

// Defines symbolic names for IKRRISC2 instructions.
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "IKRRISC2GenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "IKRRISC2GenSubtargetInfo.inc"

#endif
