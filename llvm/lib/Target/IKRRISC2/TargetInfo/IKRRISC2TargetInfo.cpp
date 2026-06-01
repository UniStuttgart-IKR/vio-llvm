//===-- IKRRISC2TargetInfo.cpp - IKRRISC2 Target Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "IKRRISC2TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

Target &llvm::getTheIKRRISC2Target() {
	static Target TheIKRRISC2Target;
	return TheIKRRISC2Target;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeIKRRISC2TargetInfo() {
	RegisterTarget<Triple::ikrrisc2, /*HasJIT=*/false> X(getTheIKRRISC2Target(), "ikrrisc2", "IKR RISC 2", "ikrrisc2");
}
