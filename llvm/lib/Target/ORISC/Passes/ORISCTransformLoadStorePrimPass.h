#ifndef LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMLOADSTOREPRIMPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMLOADSTOREPRIMPASS_H

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class TransformLoadStorePrimPass : public PassInfoMixin<TransformLoadStorePrimPass>  {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    static bool isRequired() { return true; }
    
private:
    FunctionCallee LoadPrmFn;
    FunctionCallee StorePrmFn;

    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();

    bool visitLoadStoreInst(Instruction *I);
    friend PassInfoMixin<TransformLoadStorePrimPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMLOADSTOREPRIMPASS_H