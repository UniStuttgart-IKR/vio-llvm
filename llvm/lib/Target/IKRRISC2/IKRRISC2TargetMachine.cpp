//===-- IKRRISC2TargetMachine.cpp - Define TargetMachine for IKRRISC2 -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "IKRRISC2TargetMachine.h"
#include "IKRRISC2Subtarget.h"
#include "TargetInfo/IKRRISC2TargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include <optional>
using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeIKRRISC2Target() {
  RegisterTargetMachine<IKRRISC2TargetMachine> X(getTheIKRRISC2Target());
}

static std::string getDataLayout() {
	return "E-p:32:32-i32:32:32-n32";
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

/// Create an ILP32 architecture model
IKRRISC2TargetMachine::IKRRISC2TargetMachine(const Target &T, const Triple &TT,
                                       StringRef CPU, StringRef FS,
                                       const TargetOptions &Options,
                                       std::optional<Reloc::Model> RM,
                                       std::optional<CodeModel::Model> CM,
                                       CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, getDataLayout(), TT, CPU, FS, Options, getEffectiveRelocModel(RM), getEffectiveCodeModel(CM, CodeModel::Small), OL),
      Subtarget(TT, CPU, FS, *this) {
  initAsmInfo();
}

IKRRISC2TargetMachine::~IKRRISC2TargetMachine() = default;