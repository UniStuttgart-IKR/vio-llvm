//===-- ObjectiveRISCTargetMachine.cpp - Define TargetMachine for ObjectiveRISC --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the ObjectiveRISC specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeObjectiveRISCTarget() {
  // Register the target.
  RegisterTargetMachine<ObjectiveRISCTargetMachine> X(getTheObjectiveRISCTarget());

//	PassRegistry &PR = *PassRegistry::getPassRegistry();
//	initializeObjectiveRISCDAGToDAGISelLegacyPass(PR);
}

namespace llvm {
class ObjectiveRISCTargetMachine : public CodeGenTargetMachineImpl {
	std::unique_ptr<TargetLoweringObjectFile> TLOF;

static std::string getDataLayout() {
	return "E-p:32:32-i32:32:32-n32";
}

ObjectiveRISCTargetMachine::ObjectiveRISCTargetMachine(const Target &T, const Triple &TT,
									   StringRef CPU, StringRef FS,
									   const TargetOptions &Options,
									   std::optional<Reloc::Model> RM,
									   std::optional<CodeModel::Model> CM,
									   CodeGenOptLevel OL)
	: CodeGenTargetMachineImpl(T, getDataLayout(), TT, CPU, FS, Options, RM, CM, OL),
	TLOF(std::make_unique<ObjectiveRISCTargetObjectFile>()), is64Bit(is64bit) {
  initAsmInfo();
}

ObjectiveRISCTargetMachine::~ObjectiveRISCTargetMachine() = default;

}
}
