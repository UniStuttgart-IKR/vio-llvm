//===-- IKRRISC2TargetMachine.h - Define TargetMachine for IKRRISC2 ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the IKRRISC2 specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_IKRRISC2_IKRRISC2TARGETMACHINE_H
#define LLVM_LIB_TARGET_IKRRISC2_IKRRISC2TARGETMACHINE_H

#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "IKRRISC2Subtarget.h"
#include "llvm/Target/TargetMachine.h"
#include "IKRRISC2FrameLowering.h"
#include <optional>

namespace llvm {

class IKRRISC2TargetMachine : public CodeGenTargetMachineImpl {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  IKRRISC2Subtarget Subtarget;
public:
  IKRRISC2TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                        StringRef FS, const TargetOptions &Options,
                        std::optional<Reloc::Model> RM,
                        std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                        bool JIT);
  ~IKRRISC2TargetMachine() override;

  const IKRRISC2Subtarget *getSubtargetImpl() const { return &Subtarget; }
  const IKRRISC2Subtarget *getSubtargetImpl(const Function &) const override { return &Subtarget; }

  // Pass Pipeline Configuration
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
  
  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
};

} // end namespace llvm

#endif
