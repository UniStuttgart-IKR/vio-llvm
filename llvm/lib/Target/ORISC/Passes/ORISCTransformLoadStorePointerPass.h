#ifndef LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMLOADSTOREPOINTERPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMLOADSTOREPOINTERPASS_H

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class TransformLoadStorePointerPass : public PassInfoMixin<TransformLoadStorePointerPass>  {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    static bool isRequired() { return true; }
    
private:
    FunctionCallee LoadPtrFn;
    FunctionCallee StorePtrFn;

    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();

    bool visitLoadStoreInst(Instruction *I);
    friend PassInfoMixin<TransformLoadStorePointerPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMLOADSTOREPOINTERPASS_H