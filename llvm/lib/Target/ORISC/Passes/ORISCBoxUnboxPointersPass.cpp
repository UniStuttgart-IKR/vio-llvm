
#include "Passes/ORISCBoxUnboxPointersPass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsORISC.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <cassert>

using namespace llvm;

#define DEBUG_TYPE "ORISC-Box-Unbox-Pointers"

PreservedAnalyses BoxUnboxPointersPass::run(Module &M, ModuleAnalysisManager &AM){
    LLVMContext &Ctx = M.getContext();

    BaseTy = PointerType::get(Ctx, 0);
    IndexTy = PointerType::get(Ctx, 1);
    Type *FatPtrTy = PointerType::get(Ctx, 0);
    IntTy = Type::getInt32Ty(Ctx);
    FunctionType *BoxFnTy = FunctionType::get(FatPtrTy, {BaseTy, IndexTy}, false);
    FunctionType *GepFnTy = FunctionType::get(IndexTy, {BaseTy, IndexTy}, false);
    FunctionType *UnboxBaseFnTy = FunctionType::get(BaseTy, {FatPtrTy}, false);
    FunctionType *UnboxIndexFnTy = FunctionType::get(IndexTy, {FatPtrTy}, false);
    BoxFn = M.getOrInsertFunction("llvm.orisc.box", BoxFnTy);
    GepFn = M.getOrInsertFunction("llvm.orisc.gep", GepFnTy);
    UnboxBaseFn = M.getOrInsertFunction("llvm.orisc.unbox.base", UnboxBaseFnTy);
    UnboxIndexFn = M.getOrInsertFunction("llvm.orisc.unbox.index", UnboxIndexFnTy);

    bool Changed = false;

    for (Function &F : M) {
        if (F.isIntrinsic() || F.isDeclaration())
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
                    if (CI->getIntrinsicID() == Intrinsic::orisc_allocate)
                        Changed |= visitAlloc(CI);
                    else if (CI->getIntrinsicID() == Intrinsic::orisc_box
                            || CI->getIntrinsicID() == Intrinsic::orisc_gep
                            || CI->getIntrinsicID() == Intrinsic::orisc_unbox_base
                            || CI->getIntrinsicID() == Intrinsic::orisc_unbox_index
                            || CI->getIntrinsicID() == Intrinsic::lifetime_start
                            || CI->getIntrinsicID() == Intrinsic::lifetime_end)
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

    for (int i = RemoveFromParentList.size()-1; i >= 0; --i) {
        if (RemoveFromParentList[i]->use_empty())
            RemoveFromParentList[i]->eraseFromParent();
        else {
            dbgs() << "\nBoxUnboxPass\n";
            dbgs() << "\nInstruction marked for removal but still in use!\n";
            RemoveFromParentList[i]->dump();
        }
        Changed = true;
    }
    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

bool BoxUnboxPointersPass::visitPointerArgument(Argument *A) {
    bool Changed = false;
    IRBuilder<> Builder(A->getContext());
    Builder.SetInsertPointPastAllocas(A->getParent());
    Value *Base = Builder.CreateCall(UnboxBaseFn, A, A->getName() + ".base");
    Value *Index = Builder.CreateCall(UnboxIndexFn, A, A->getName() + ".index");
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
    Value *Base = I;
    Value *Index = ConstantPointerNull::get(IndexTy);
    SmallVector<User *> Users = SmallVector<User *, 32>(I->users());
    for (User *U : Users) {
        if (CallInst *C = dyn_cast<CallInst>(U)) {
            if (C->getIntrinsicID() == Intrinsic::lifetime_start)
                continue;
            if (C->getIntrinsicID() == Intrinsic::lifetime_end)
                continue;
        }
        if (U != Base) {
            Changed |= handleUser(Base, Index, I, U);
        }
    }
    return Changed;
}

bool BoxUnboxPointersPass::visitOther(Instruction *I) {
    bool Changed = false;
    SmallVector<User *> Users = SmallVector<User *, 32>(I->users());
    if (Users.empty())
        return false;
    IRBuilder<> Builder(I->getNextNonDebugInstruction());
    Value *Base = Builder.CreateCall(UnboxBaseFn, I, I->getName() + ".base");
    Value *Index = Builder.CreateCall(UnboxIndexFn, I, I->getName() + ".index");
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

static inline void addToRemoveIfNoUses(SmallVector<Instruction *> *RL, Instruction *I){
    unsigned i = 0;
    for (User *_ : I->users()) {
        if (i != 0) {
            RL->push_back(cast<Instruction>(I));
            break;
        }
        i++;
    }
}

bool BoxUnboxPointersPass::handleUser(Value *Base, Value *CurrentIndex, Value *Parent, User *U) {
    bool Changed = false;
    IRBuilder<> Builder(U->getContext());
    if (GetElementPtrInst *G = dyn_cast<GetElementPtrInst>(U)) {
        RemoveFromParentList.push_back(G);
        SmallVector<Value *, 8> Indices(G->indices());
        Builder.SetInsertPoint(G->getNextNonDebugInstruction());
        StringRef Name = G->getName();
        G->setName(Name + ".old");
        Value *NewG = Builder.CreateGEP(G->getSourceElementType(), CurrentIndex, Indices, Name, G->getNoWrapFlags());
        SmallVector<User *> Users = SmallVector<User *, 32>(G->users());
        for (User *GU : Users)
            Changed |= handleUser(Base, NewG, G, GU);
    } else if (Instruction *I = dyn_cast<Instruction>(U)) {
        Builder.SetInsertPoint(I);
        Value *N;
        StringRef Name = Base->getName();
        if (Name.ends_with(".base"))
            Name = Name.substr(0, Name.size()-5);
        if (isa<StoreInst>(I) && I->getOperand(1) == Parent) {
            if (!Name.empty())
                N = Builder.CreateCall(GepFn, {Base, CurrentIndex}, Name + ".gep");
            else
                N = Builder.CreateCall(GepFn, {Base, CurrentIndex});
        } else if (isa<LoadInst>(I) && I->getOperand(0) == Parent) {
            if (!Name.empty())
                N = Builder.CreateCall(GepFn, {Base, CurrentIndex}, Name + ".gep");
            else
                N = Builder.CreateCall(GepFn, {Base, CurrentIndex});
        } else if (isa<CallInst>(Base) && cast<CallInst>(Base)->getIntrinsicID() == Intrinsic::orisc_unbox_base
            && isa<CallInst>(CurrentIndex) && cast<CallInst>(CurrentIndex)->getIntrinsicID() == Intrinsic::orisc_unbox_index) {
            //If we are boxing something that just got unboxed, then just use the parent box
            N = cast<CallInst>(Base)->getOperand(0);
            addToRemoveIfNoUses(&RemoveFromParentList, cast<Instruction>(Base));
            addToRemoveIfNoUses(&RemoveFromParentList, cast<Instruction>(CurrentIndex));
        } else {
            if (!Name.empty())
                N = Builder.CreateCall(BoxFn, {Base, CurrentIndex}, Name + ".box");
            else 
                N = Builder.CreateCall(BoxFn, {Base, CurrentIndex});
        }
        Changed = I->replaceUsesOfWith(Parent, N);
    }
    return Changed;
}