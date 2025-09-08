#include "Passes/ORISCTransformGEPsPass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"

using namespace llvm;

PreservedAnalyses TransformGEPsPass::run(Module &M, ModuleAnalysisManager &AM){
    LLVMContext &Ctx = M.getContext();
    Type *PtrTy = PointerType::get(Ctx, 0);
    Type *IntTy = Type::getInt32Ty(Ctx);
    Type *VoidTy = Type::getVoidTy(Ctx);
    FunctionType *LoadPrimFnTy = FunctionType::get(IntTy, {PtrTy}, false);
    FunctionType *StorePrimFnTy = FunctionType::get(VoidTy, {PtrTy, IntTy}, false);
    LoadPtrFn = M.getOrInsertFunction("llvm.orisc.loadprim", LoadPrimFnTy);
    StorePtrFn = M.getOrInsertFunction("llvm.orisc.storeprim", StorePrimFnTy);

    bool Changed = false;
    for (Function &F : M)
        for (BasicBlock &BB : F)
            for (Instruction &I : BB)
                if (isa<LoadInst>(I) || isa<StoreInst>(I))
                    Changed |= visitLoadStoreInst(&I);
                
    for (Instruction *I : RemoveFromParentList) {
        I->eraseFromParent();
        Changed = true;
    }
    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

bool TransformGEPsPass::visitLoadStoreInst(Instruction *I){
    bool ValueIsPointerTy;
    Value *PointerOperand;
    if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
        PointerOperand = LI->getPointerOperand();
        ValueIsPointerTy = LI->getType()->isPointerTy();
    } else if (StoreInst *SI = dyn_cast<StoreInst>(I)){
        PointerOperand = SI->getPointerOperand();
        ValueIsPointerTy = SI->getValueOperand()->getType()->isPointerTy();
    } else {
        llvm_unreachable("visitLoadStoreInst expects Load or Store as Parameter");
    }

    if (!ValueIsPointerTy)
        return false;
    
    IRBuilder<> Builder(I);
    Value *LSPtr;
    if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        LSPtr = Builder.CreateCall(StorePtrFn, { SI->getValueOperand(), PointerOperand });
        RemoveFromParentList.push_back(I);
    } else {
        LSPtr = Builder.CreateCall(LoadPtrFn, { PointerOperand });
        I->replaceAllUsesWith(LSPtr);
    }
    return true;
}