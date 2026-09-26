//===-- RISCVZhmVerifyIR.h ---*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVZHMVERIFYIR_H
#define LLVM_LIB_TARGET_RISCV_RISCVZHMVERIFYIR_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class TargetMachine;

// Shared implementation of the new-PM and legacy pass. With a TargetMachine,
// only functions whose subtarget has Zhm are processed.
bool runRISCVZhmVerifyIR(Module &M, const TargetMachine *TM);

class RISCVZhmVerifyIR : public RequiredPassInfoMixin<RISCVZhmVerifyIR> {
  const TargetMachine *TM;

public:
  explicit RISCVZhmVerifyIR(const TargetMachine *TM = nullptr) : TM(TM) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

  static bool isRequired() { return true; }
};

} // end namespace llvm

#endif