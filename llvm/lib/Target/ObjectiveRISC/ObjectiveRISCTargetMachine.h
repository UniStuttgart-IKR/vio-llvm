//===-- ObjectiveRISCTargetMachine.h - Define TargetMachine for Objective-RISC ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the Objective-RISC specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_OBJECTIVERISC_OBJECTIVERISCTARGETMACHINE_H
#define LLVM_LIB_TARGET_OBJECTIVERISC_OBJECTIVERISCTARGETMACHINE_H

#include "ObjectiveRISCInstrInfo.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/Target/TargetMachine.h"
#include <optional>

namespace llvm {

class ObjectiveRISCTargetMachine : public CodeGenTargetMachineImpl {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;

public:
  ObjectiveRISCTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
					 StringRef FS, const TargetOptions &Options,
					 std::optional<Reloc::Model> RM,
					 std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
					 bool JIT, bool is64bit);
  ~ObjectiveRISCTargetMachine() override;

  const ObjectiveRISCSubtarget *getSubtargetImpl(const Function &F) const override;

  // Pass Pipeline Configuration
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
  TargetLoweringObjectFile *getObjFileLowering() const override {
	return TLOF.get();
  }

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
							const TargetSubtargetInfo *STI) const override;
};

}

#endif