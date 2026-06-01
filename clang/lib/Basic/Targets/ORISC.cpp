//===--- ORISC.cpp - Implement ORISC target feature support -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements ORISC TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "ORISC.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetBuiltins.h"

using namespace clang;
using namespace clang::targets;

void ORISCTargetInfo::getTargetDefines(const LangOptions &Opts,
                                        MacroBuilder &Builder) const {
  Builder.defineMacro("__ORISC__");
  Builder.defineMacro("__ORISC__");
  if (BigEndian)
    Builder.defineMacro("__ORISC_EB__");
  else
    Builder.defineMacro("__ORISC_EL__");
  Builder.defineMacro("__XCHAL_HAVE_BE", BigEndian ? "1" : "0");
  Builder.defineMacro("__XCHAL_HAVE_ABS");  // core arch
  Builder.defineMacro("__XCHAL_HAVE_ADDX"); // core arch
  Builder.defineMacro("__XCHAL_HAVE_L32R"); // core arch
}
