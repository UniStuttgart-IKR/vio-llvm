#include "Passes/ORISCEscapeOutgoingLocalPointersPass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <cassert>

using namespace llvm;

#define DEBUG_TYPE "ORISC-Outgoing-Local-Pointers-Escape"

PreservedAnalyses EscapeOutgoingLocalPointersPass::run(Module &M, ModuleAnalysisManager &AM){
    LLVMContext &Ctx = M.getContext();

    Type *PtrTy = PointerType::get(Ctx, 0);
    IndexTy = PointerType::get(Ctx, 1);
    FunctionType *BoxFnTy = FunctionType::get(PtrTy, {PtrTy, IndexTy}, false);
    BoxFn = M.getOrInsertFunction("llvm.orisc.box", BoxFnTy);

    bool Changed = false;
    for (Function &F : M)
        for (BasicBlock &BB : F)
            for (Instruction &I : BB)
                if (auto *RI = dyn_cast<ReturnInst>(&I))
                    Changed |= visitReturnInst(RI);

    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

bool EscapeOutgoingLocalPointersPass::makeAllStoresVolatile(Value *V) {
    bool Changed = false;
    SmallVector<Value *> Users(V->users());
    for (Value *U : Users) {
        if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
            SI->setVolatile(true);
            Changed = true;
        } else if (auto *GEP = dyn_cast<GetElementPtrInst>(U)) {
            Changed = makeAllStoresVolatile(GEP);
            Changed = makeAllStoresVolatile(GEP);
        }
    }
    return Changed;
}

bool EscapeOutgoingLocalPointersPass::visitReturnInst(ReturnInst *RI){
    Value *RetVal = RI->getReturnValue();
    if (!RetVal)
        return false;

    //If we return a to a local object (created with alloca)
    //we have to ensure opt does not see this function as unnecessary
    //and/or crashes the program
    if (RetVal->getType()->isPointerTy()) {
        Value *Creator = RI->getReturnValue();
        while (auto *Gep = dyn_cast<GetElementPtrInst>(Creator))
            Creator = Gep->getPointerOperand();
        makeAllStoresVolatile(Creator);
        IRBuilder<> Builder(RI);
        Value *Null = ConstantPointerNull::get(IndexTy);
        CallInst *CI = Builder.CreateCall(BoxFn, { RI->getOperand(0), Null });
        RI->setOperand(0, CI);
        return true;
    }
    return false;
}
