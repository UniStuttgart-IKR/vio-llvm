//===-- ObjectiveRISCTargetInfo.cpp - ObjectiveRISC Target Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ObjectiveRISCTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

Target &llvm::getTheObjectiveRISCTarget() {
	static Target TheObjectiveRISCTarget;
	return TheObjectiveRISCTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeObjectiveRiscTargetInfo() {
	RegisterTarget<Triple::objrisc, /*HasJIT=*/false> X(getTheObjectiveRISCTarget(), "objectiverisc", "Objective-RISC", "ObjectiveRISC");
}
