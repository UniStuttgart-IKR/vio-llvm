//===-- ORISCMCFixups.h - ORISC-specific fixup entries --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ORISC_MCTARGETDESC_ORISCMCFIXUPS_H
#define LLVM_LIB_TARGET_ORISC_MCTARGETDESC_ORISCMCFIXUPS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace ORISC {
enum FixupKind {
  fixup_orisc_ctxt_idx = FirstTargetFixupKind,
  fixup_orisc_branch_12,
  fixup_orisc_branch_25,
  fixup_orisc_jlib_idx,
  fixup_orisc_invalid,
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};
} // end namespace ORISC
} // end namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_MCTARGETDESC_ORISCMCFIXUPS_H
