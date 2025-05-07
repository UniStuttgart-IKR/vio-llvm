//===-- ORISCTargetMachine.cpp - Define TargetMachine for ORISC -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "ORISC.h"
#include "ORISCTargetMachine.h"
#include "ORISCSubtarget.h"
#include "TargetInfo/ORISCTargetInfo.h"
#include "llvm/CodeGen/GlobalISel/IRTranslator.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelect.h"
#include "llvm/CodeGen/GlobalISel/Legalizer.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/GlobalISel/RegBankSelect.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include <optional>
using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeORISCTarget() {
  RegisterTargetMachine<ORISCTargetMachine> X(getTheORISCTarget());
  auto *PR = PassRegistry::getPassRegistry();
  initializeORISCDAGToDAGISelLegacyPass(*PR);
}

static std::string getDataLayout() {
	return "E-p:32:32-i32:32:32-n32";
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

/// Create an ILP32 architecture model
ORISCTargetMachine::ORISCTargetMachine(const Target &T, const Triple &TT,
                                       StringRef CPU, StringRef FS,
                                       const TargetOptions &Options,
                                       std::optional<Reloc::Model> RM,
                                       std::optional<CodeModel::Model> CM,
                                       CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, getDataLayout(), TT, CPU, FS, Options, getEffectiveRelocModel(RM), getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()) ,
      Subtarget(TT, CPU, FS, *this){
  initAsmInfo();
}

ORISCTargetMachine::~ORISCTargetMachine() = default;


//===----------------------------------------------------------------------===//
// Pass Pipeline Configuration
//===----------------------------------------------------------------------===//

namespace {
class ORISCPassConfig : public TargetPassConfig {
public:
  ORISCPassConfig(ORISCTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  ORISCTargetMachine &getORISCTargetMachine() const {
    return getTM<ORISCTargetMachine>();
  }

  const ORISCSubtarget &getORISCSubtarget() const {
    return *getORISCTargetMachine().getSubtargetImpl();
  }
  bool addInstSelector() override;
  /*
  void addIRPasses() override;
  bool addIRTranslator() override;
  bool addLegalizeMachineIR() override;
  bool addRegBankSelect() override;
  bool addGlobalInstructionSelect() override;
  void addPreSched2() override;
  void addPreEmitPass() override;*/
};
} // namespace

TargetPassConfig *ORISCTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new ORISCPassConfig(*this, PM);
}
/*
void ORISCPassConfig::addIRPasses() {
  //addPass(createAtomicExpandLegacyPass());
  TargetPassConfig::addIRPasses();
}
*/
bool ORISCPassConfig::addInstSelector() {
  // Install an instruction selector.
  addPass(createORISCISelDag(getORISCTargetMachine()));
  //addPass(createORISCGlobalBaseRegPass());
  return false;
}
/*
bool ORISCPassConfig::addIRTranslator() {
  //addPass(new IRTranslator());
  return false;
}

bool ORISCPassConfig::addLegalizeMachineIR() {
  //addPass(new Legalizer());
  return false;
}

bool ORISCPassConfig::addRegBankSelect() {
  //addPass(new RegBankSelect());
  return false;
}

bool ORISCPassConfig::addGlobalInstructionSelect() {
  //addPass(new InstructionSelect());
  return false;
}

void ORISCPassConfig::addPreSched2() { 
  //addPass(createORISCExpandPseudoPass());
}

void ORISCPassConfig::addPreEmitPass() {
  //addPass(createORISCCollapseMOVEMPass());
}*/
