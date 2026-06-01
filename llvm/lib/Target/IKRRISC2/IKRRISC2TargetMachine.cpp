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

#include "IKRRISC2.h"
#include "IKRRISC2TargetMachine.h"
#include "IKRRISC2Subtarget.h"
#include "TargetInfo/IKRRISC2TargetInfo.h"
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

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeIKRRISC2Target() {
  RegisterTargetMachine<IKRRISC2TargetMachine> X(getTheIKRRISC2Target());
  auto *PR = PassRegistry::getPassRegistry();
  initializeIKRRISC2DAGToDAGISelLegacyPass(*PR);
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
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()) ,
      Subtarget(TT, CPU, FS, *this){
  initAsmInfo();
}

IKRRISC2TargetMachine::~IKRRISC2TargetMachine() = default;


//===----------------------------------------------------------------------===//
// Pass Pipeline Configuration
//===----------------------------------------------------------------------===//

namespace {
class IKRRISC2PassConfig : public TargetPassConfig {
public:
  IKRRISC2PassConfig(IKRRISC2TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  IKRRISC2TargetMachine &getIKRRISC2TargetMachine() const {
    return getTM<IKRRISC2TargetMachine>();
  }

  const IKRRISC2Subtarget &getIKRRISC2Subtarget() const {
    return *getIKRRISC2TargetMachine().getSubtargetImpl();
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

TargetPassConfig *IKRRISC2TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new IKRRISC2PassConfig(*this, PM);
}
/*
void IKRRISC2PassConfig::addIRPasses() {
  //addPass(createAtomicExpandLegacyPass());
  TargetPassConfig::addIRPasses();
}
*/
bool IKRRISC2PassConfig::addInstSelector() {
  // Install an instruction selector.
  addPass(createIKRRISC2ISelDag(getIKRRISC2TargetMachine()));
  //addPass(createIKRRISC2GlobalBaseRegPass());
  return false;
}
/*
bool IKRRISC2PassConfig::addIRTranslator() {
  //addPass(new IRTranslator());
  return false;
}

bool IKRRISC2PassConfig::addLegalizeMachineIR() {
  //addPass(new Legalizer());
  return false;
}

bool IKRRISC2PassConfig::addRegBankSelect() {
  //addPass(new RegBankSelect());
  return false;
}

bool IKRRISC2PassConfig::addGlobalInstructionSelect() {
  //addPass(new InstructionSelect());
  return false;
}

void IKRRISC2PassConfig::addPreSched2() { 
  //addPass(createIKRRISC2ExpandPseudoPass());
}

void IKRRISC2PassConfig::addPreEmitPass() {
  //addPass(createIKRRISC2CollapseMOVEMPass());
}*/
