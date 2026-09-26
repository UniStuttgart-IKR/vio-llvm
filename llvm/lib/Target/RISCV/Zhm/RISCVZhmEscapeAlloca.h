//===-- RISCVZhmEscapeAlloca.h - Move frame-escaping allocas to alc ---*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVZHMESCAPEALLOCA_H
#define LLVM_LIB_TARGET_RISCV_RISCVZHMESCAPEALLOCA_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class TargetMachine;

// Shared implementation of the new-PM and legacy pass. With a TargetMachine,
// only functions whose subtarget has Zhm are processed.
bool runRISCVZhmEscapeAlloca(Module &M, const TargetMachine *TM);

class RISCVZhmEscapeAlloca : public RequiredPassInfoMixin<RISCVZhmEscapeAlloca> {
  const TargetMachine *TM;

public:
  explicit RISCVZhmEscapeAlloca(const TargetMachine *TM = nullptr) : TM(TM) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

  // Correctness pass: must not be skipped for optnone functions or by
  // -opt-bisect-limit.
  static bool isRequired() { return true; }
};

} // end namespace llvm

#endif