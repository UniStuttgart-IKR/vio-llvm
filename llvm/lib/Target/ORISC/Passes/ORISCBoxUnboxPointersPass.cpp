
#include "Passes/ORISCBoxUnboxPointersPass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicsORISC.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>

using namespace llvm;

#define DEBUG_TYPE "ORISC-Box-Unbox-Pointers"

PreservedAnalyses BoxUnboxPointersPass::run(Module &M, ModuleAnalysisManager &AM){
    LLVMContext &Ctx = M.getContext();

    BaseTy = PointerType::get(Ctx, 1);
    IndexTy = PointerType::get(Ctx, 0);
    Type *FatPtrTy = PointerType::get(Ctx, 0);
    IntTy = Type::getInt32Ty(Ctx);
    FunctionType *BoxFnTy = FunctionType::get(FatPtrTy, {BaseTy, IndexTy}, false);
    FunctionType *UnboxBaseFnTy = FunctionType::get(BaseTy, {FatPtrTy}, false);
    FunctionType *UnboxIndexFnTy = FunctionType::get(IndexTy, {FatPtrTy}, false);
    FunctionType *AllocateFnTy = FunctionType::get(BaseTy, {IntTy, IntTy}, false);
    BoxFn = M.getOrInsertFunction("llvm.orisc.box", BoxFnTy);
    GepFn = M.getOrInsertFunction("llvm.orisc.gep", BoxFnTy);
    UnboxBaseFn = M.getOrInsertFunction("llvm.orisc.unbox.base", UnboxBaseFnTy);
    UnboxIndexFn = M.getOrInsertFunction("llvm.orisc.unbox.index", UnboxIndexFnTy);
    AllocateFn = M.getOrInsertFunction("llvm.orisc.allocate", AllocateFnTy);

    bool Changed = false;

    for (Function &F : M) {
        if (F.isIntrinsic())
            continue;
        for (unsigned i = 0; i < F.arg_size(); ++i) {
            if (F.getArg(i)->getType()->isPointerTy())
                Changed |= visitPointerArgument(F.getArg(i));
        }
        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (isa<AllocaInst>(&I)) {
                    Changed |= visitAlloc(&I);
                } else if (CallInst *CI = dyn_cast<CallInst>(&I)) {
                    if (CI->getIntrinsicID() == Intrinsic::orisc_allocate_placeholder)
                        Changed |= visitAlloc(CI);
                    else if (CI->getIntrinsicID() == Intrinsic::orisc_allocate
                            || CI->getIntrinsicID() == Intrinsic::orisc_box
                            || CI->getIntrinsicID() == Intrinsic::orisc_gep
                            || CI->getIntrinsicID() == Intrinsic::orisc_unbox_base
                            || CI->getIntrinsicID() == Intrinsic::orisc_unbox_index)
                        continue;
                    else if (CI->getFunctionType()->getReturnType()->isPointerTy())
                        visitOther(CI);
                } else if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
                    if (LI->getType()->isPointerTy())
                        visitOther(LI);
                }
            }
        }
    }

    for (unsigned i = 0; i < RemoveFromParentList.size(); ++i) {
        RemoveFromParentList[i]->eraseFromParent();
        Changed = true;
    }
    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

bool BoxUnboxPointersPass::visitPointerArgument(Argument *A) {
    bool Changed = false;
    IRBuilder<> Builder(A->getContext());
    Builder.SetInsertPointPastAllocas(A->getParent());
    Value *Base = Builder.CreateCall(UnboxBaseFn, A);
    Value *Index = Builder.CreateCall(UnboxIndexFn, A);
    SmallVector<User *> Users = SmallVector<User *, 32>(A->users());
    for (User *U : Users) {
        if (CallInst *C = dyn_cast<CallInst>(U)) {
            if (C->getIntrinsicID() == Intrinsic::orisc_unbox_base)
                continue;
            if (C->getIntrinsicID() == Intrinsic::orisc_unbox_index)
                continue;
        }
        Changed |= handleUser(Base, Index, A, U);
    }
    return Changed;
}

bool BoxUnboxPointersPass::visitAlloc(Instruction *I) {
    bool Changed = false;
    Value *Base;
    IRBuilder<> Builder(I->getContext());
    if (isa<AllocaInst>(I)) {
        Builder.SetInsertPointPastAllocas(I->getFunction());
        Base = Builder.CreateAddrSpaceCast(I, BaseTy);
    } else {
        Builder.SetInsertPoint(I->getNextNonDebugInstruction());
        Base = Builder.CreateCall(AllocateFn, { I->getOperand(0), I->getOperand(1) });
        RemoveFromParentList.push_back(I);
    }
    Value *Index = ConstantPointerNull::get(IndexTy);
    SmallVector<User *> Users = SmallVector<User *, 32>(I->users());
    for (User *U : Users) {
        if (U != Base) {
            Changed |= handleUser(Base, Index, I, U);
        }
    }
    return Changed;
}

bool BoxUnboxPointersPass::visitOther(Instruction *I) {
    bool Changed = false;
    IRBuilder<> Builder(I->getNextNonDebugInstruction());
    Value *Base = Builder.CreateCall(UnboxBaseFn, I);
    Value *Index = Builder.CreateCall(UnboxIndexFn, I);
    SmallVector<User *> Users = SmallVector<User *, 32>(I->users());
    for (User *U : Users) {
        if (CallInst *C = dyn_cast<CallInst>(U)) {
            if (C->getIntrinsicID() == Intrinsic::orisc_unbox_base)
                continue;
            if (C->getIntrinsicID() == Intrinsic::orisc_unbox_index)
                continue;
        }
        Changed |= handleUser(Base, Index, I, U);
    }
    return Changed;
}

bool BoxUnboxPointersPass::handleUser(Value *Base, Value *CurrentIndex, Value *Parent, User *U) {
    bool Changed = false;
    IRBuilder<> Builder(U->getContext());
    if (GetElementPtrInst *G = dyn_cast<GetElementPtrInst>(U)) {
        G->setOperand(0, CurrentIndex);
        for (User *GU : G->users())
            Changed |= handleUser(Base, G, G, GU);
    } else if (Instruction *I = dyn_cast<Instruction>(U)) {
        Builder.SetInsertPoint(I);
        Value *N;
        if (isa<StoreInst>(I) && I->getOperand(1) == Parent)
            N = Builder.CreateCall(GepFn, {Base, CurrentIndex});
        else if (isa<LoadInst>(I) && I->getOperand(0) == Parent)
            N = Builder.CreateCall(GepFn, {Base, CurrentIndex});
        else
            N = Builder.CreateCall(BoxFn, {Base, CurrentIndex});
        I->replaceUsesOfWith(Parent, N);
        Changed = true;
    }
    return Changed;
}