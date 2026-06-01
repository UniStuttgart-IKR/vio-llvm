//===-- ORISCTargetMachine.h - Define TargetMachine for ORISC ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the ORISC specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ORISC_ORISCTARGETMACHINE_H
#define LLVM_LIB_TARGET_ORISC_ORISCTARGETMACHINE_H

#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "ORISCSubtarget.h"
#include "llvm/Target/TargetMachine.h"
#include "ORISCFrameLowering.h"
#include <optional>

namespace llvm {

class ORISCTargetMachine : public CodeGenTargetMachineImpl {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  ORISCSubtarget Subtarget;
public:
  ORISCTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                        StringRef FS, const TargetOptions &Options,
                        std::optional<Reloc::Model> RM,
                        std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                        bool JIT);
  ~ORISCTargetMachine() override;

  const ORISCSubtarget *getSubtargetImpl() const { return &Subtarget; }
  const ORISCSubtarget *getSubtargetImpl(const Function &) const override { return &Subtarget; }

  // Pass Pipeline Configuration
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
  void registerPassBuilderCallbacks(PassBuilder &PB) override;
  
  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
};

} // end namespace llvm

#endif
