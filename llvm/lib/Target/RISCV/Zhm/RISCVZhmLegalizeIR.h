//===-- RISCVZhmLegalizeIR.h ---*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVZHMLEGALIZEIR_H
#define LLVM_LIB_TARGET_RISCV_RISCVZHMLEGALIZEIR_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class TargetMachine;

// Shared implementation of the new-PM and legacy pass. With a TargetMachine,
// only functions whose subtarget has Zhm are processed.
bool runRISCVZhmLegalizeIR(Module &M, const TargetMachine *TM);

class RISCVZhmLegalizeIR : public RequiredPassInfoMixin<RISCVZhmLegalizeIR> {
  const TargetMachine *TM;

public:
  explicit RISCVZhmLegalizeIR(const TargetMachine *TM = nullptr) : TM(TM) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

  static bool isRequired() { return true; }
};

} // end namespace llvm

#endif