#ifndef LLVM_LIB_TARGET_ORISC_ORISCREPLACEREPLACENULLPOINTERSPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCREPLACEREPLACENULLPOINTERSPASS_H

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class ReplaceNullPointersPass : public PassInfoMixin<ReplaceNullPointersPass>  {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    static bool isRequired() { return true; }
    
private:

    friend PassInfoMixin<ReplaceNullPointersPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCREPLACEREPLACENULLPOINTERSPASS_H