#ifndef LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMGEPSPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMGEPSPASS_H

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class TransformGEPsPass : public PassInfoMixin<TransformGEPsPass>  {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    static bool isRequired() { return true; }
    
private:
    FunctionCallee LoadPtrFn;
    FunctionCallee StorePtrFn;

    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();

    bool visitLoadStoreInst(Instruction *I);
    friend PassInfoMixin<TransformGEPsPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMGEPSPASS_H