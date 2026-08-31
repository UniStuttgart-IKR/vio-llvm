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
#include "ORISCTargetObjectFile.h"
#include "Passes/ORISCBoxUnboxPointersPass.h"
#include "Passes/ORISCEscapeAllocaPass.h"
#include "Passes/ORISCRejectUnsupportedIRPass.h"
#include "Passes/ORISCEscapeOutgoingLocalPointersPass.h"
#include "Passes/ORISCReplaceDeAllocLibCallsPass.h"
#include "Passes/ORISCReplaceNullPointersPass.h"
#include "Passes/ORISCSplitMixedStructsPass.h"
#include "Passes/ORISCPromoteInnerStructsPass.h"
#include "ORISCSubtarget.h"
#include "TargetInfo/ORISCTargetInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/CodeGen/GlobalISel/IRTranslator.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/GlobalISel/RegBankSelect.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/PassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/ErrorHandling.h"
#include <optional>
using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeORISCTarget() {
  RegisterTargetMachine<ORISCTargetMachine> X(getTheORISCTarget());
  auto *PR = PassRegistry::getPassRegistry();
  //initializeORISCGEPTransformPass(*PR);
  initializeORISCDAGToDAGISelLegacyPass(*PR);
}

static std::string getDataLayout() {
	return "E-p:32:32-i64:64:64-i32:32:32-i16:16:16-i8:8:8-n32";
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
      TLOF(std::make_unique<ORISCTargetObjectFile>()),
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
  void addCodeGenPrepare() override;
  bool addInstSelector() override;
  void addPreLegalizeMachineIR() override;
};
} // namespace

void ORISCTargetMachine::registerPassBuilderCallbacks(PassBuilder &PB) {

  #define GET_PASS_REGISTRY "Passes/ORISCPassRegistry.def"
  #include "llvm/Passes/TargetPassRegistry.inc"

  PB.registerPipelineStartEPCallback(
    [](ModulePassManager &MPM, OptimizationLevel Level){
      MPM.addPass(PromoteInnerStructsPass());
      MPM.addPass(SplitMixedStructsPass());
      MPM.addPass(createModuleToFunctionPassAdaptor(ReplaceDeAllocLibCallsPass()));
      MPM.addPass(EscapeOutgoingLocalPointersPass()); //Has do be done here because opt will crash the program if you return a local pointer
    });
  PB.registerPeepholeEPCallback(
    [](FunctionPassManager &FPM, OptimizationLevel Level){
    });
  PB.registerScalarOptimizerLateEPCallback(
    [](FunctionPassManager &FPM, OptimizationLevel Level){
      //FPM.addPass(DividePointerIndicesPass());
    });
  PB.registerOptimizerLastEPCallback(
    [](ModulePassManager &MPM, OptimizationLevel Level, ThinOrFullLTOPhase T) {
      MPM.addPass(EscapeAllocaPass());
      //if -NoUnnecessaryAllocations remove unneeded Allocates
      MPM.addPass(BoxUnboxPointersPass());
      MPM.addPass(ReplaceNullPointersPass());
    });
}

TargetPassConfig *ORISCTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new ORISCPassConfig(*this, PM);
}

void ORISCPassConfig::addCodeGenPrepare() {
  addPass(createORISCShrinkPointerIndicesPass());
}

bool ORISCPassConfig::addInstSelector() {
  addPass(createORISCISelDag(getORISCTargetMachine()));
  return false;
}

void ORISCPassConfig::addPreLegalizeMachineIR() {
  llvm_unreachable("PreLegalize");
}