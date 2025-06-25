#ifndef LLVM_LIB_TARGET_ORISC_ORISCMOVEALLOCAONHEAPPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCMOVEALLOCAONHEAPPASS_H

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class MoveAllocaOnHeapPass : public PassInfoMixin<MoveAllocaOnHeapPass>  {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    static bool isRequired() { return true; }
    
private:
    FunctionCallee AllocateFn;

    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();

    bool visitCallInst(CallInst *I);
    friend PassInfoMixin<MoveAllocaOnHeapPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCMOVEALLOCAONHEAPPASS_H