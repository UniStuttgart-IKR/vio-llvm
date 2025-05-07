//===-- ORISCTargetInfo.cpp - ORISC Target Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ORISCTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

Target &llvm::getTheORISCTarget() {
	static Target TheORISCTarget;
	return TheORISCTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeORISCTargetInfo() {
	RegisterTarget<Triple::orisc, /*HasJIT=*/false> X(getTheORISCTarget(), "orisc", "Objective RISC", "orisc");
}
