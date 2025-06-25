#include "Passes/ORISCMoveAllocaOnHeapPass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/Casting.h"

using namespace llvm;

PreservedAnalyses MoveAllocaOnHeapPass::run(Module &M, ModuleAnalysisManager &AM){
    LLVMContext &Ctx = M.getContext();
    Type *PtrTy = PointerType::get(Ctx, 0);
    Type *IntTy = Type::getInt32Ty(Ctx);
    FunctionType *AllocateFnTy = FunctionType::get(PtrTy, {IntTy, IntTy}, false);
    AllocateFn = M.getOrInsertFunction("llvm.orisc.allocate", AllocateFnTy);

    bool Changed = false;
    for (Function &F : M)
        for (BasicBlock &BB : F)
            for (Instruction &I : BB)
                if (auto *CI = dyn_cast<CallInst>(&I))
                    Changed |= visitCallInst(CI);

    for (Instruction *I : RemoveFromParentList) {
        I->eraseFromParent();
        Changed = true;
    }
    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

bool MoveAllocaOnHeapPass::visitCallInst(CallInst *I){
    //TODO: Insert Recursive Logic to find source of all arguments of this call
    //Skip if this is orisc.intrinsic
    //if we find alloca, we make it a allocate
    return false;
}