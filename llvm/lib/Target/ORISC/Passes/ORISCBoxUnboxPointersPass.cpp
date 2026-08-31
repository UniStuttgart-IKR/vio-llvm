
#include "Passes/ORISCBoxUnboxPointersPass.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsORISC.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <cassert>
#include <cstdint>

using namespace llvm;

#define DEBUG_TYPE "ORISC-Box-Unbox-Pointers"

PreservedAnalyses BoxUnboxPointersPass::run(Module &M, ModuleAnalysisManager &AM){
    LLVMContext &Ctx = M.getContext();

    BaseTy = PointerType::get(Ctx, 0);
    IndexTy = PointerType::get(Ctx, 1);
    Type *FatPtrTy = PointerType::get(Ctx, 0);
    IntTy = Type::getInt32Ty(Ctx);
    FunctionType *BoxFnTy = FunctionType::get(FatPtrTy, {BaseTy, IndexTy}, false);
    FunctionType *GepPFnTy = FunctionType::get(IndexTy, {BaseTy, IndexTy}, false);
    FunctionType *GepIFnTy = FunctionType::get(IndexTy, {BaseTy, IntTy}, false);
    FunctionType *UnboxBaseFnTy = FunctionType::get(BaseTy, {FatPtrTy}, false);
    FunctionType *UnboxIndexFnTy = FunctionType::get(IndexTy, {FatPtrTy}, false);
    BoxFn = M.getOrInsertFunction("llvm.orisc.box", BoxFnTy);
    GepPFn = M.getOrInsertFunction("llvm.orisc.gep.p", GepPFnTy);
    GepIFn = M.getOrInsertFunction("llvm.orisc.gep.i", GepIFnTy);
    UnboxBaseFn = M.getOrInsertFunction("llvm.orisc.unbox.base", UnboxBaseFnTy);
    UnboxIndexFn = M.getOrInsertFunction("llvm.orisc.unbox.index", UnboxIndexFnTy);

    bool Changed = false;

    SmallVector<std::tuple<GlobalVariable *, Instruction *>, 32> GlobalVarUsers = SmallVector<std::tuple<GlobalVariable *, Instruction *>, 32>();
    SmallVector<std::tuple<GlobalVariable *, ConstantExpr *, Instruction *>, 32> GlobalVarGEPs = SmallVector<std::tuple<GlobalVariable *, ConstantExpr *, Instruction *>, 32>();

    // find all users of global variables, including GEP constant expressions
    for (auto G = M.global_begin(); G != M.global_end(); ++G) {
        for (auto U = G->user_begin(); U != G->user_end(); ++U) {
            if (ConstantExpr *C = dyn_cast<ConstantExpr>(*U)) {
                dbgs() << "Constant Expression: "; C->dump();
                if (C->getOpcode() == Instruction::GetElementPtr) {
                    for (auto CU = C->user_begin(); CU != C->user_end(); ++CU) {
                        if (Instruction *I = dyn_cast<Instruction>(*CU)) {
                            GlobalVarGEPs.push_back(std::tuple<GlobalVariable *, ConstantExpr *, Instruction *>(&*G, C, I));
                        } else {
                            dbgs() << "Sus: "; CU->dump();
                            assert(false && "User of Constant Expression is not an Instruction");
                        }
                    }
                } else if (U->getType()->isPointerTy()) {
                    dbgs() << "Sus: "; U->dump();
                    assert(false && "User is not a GEP Constant Expression");
                }
            } else if (Instruction *I = dyn_cast<Instruction>(*U)) {
                GlobalVarUsers.push_back(std::tuple<GlobalVariable *, Instruction *>(&*G, I));
            } else {
                dbgs() << "Sus: "; U->dump();
                assert(false && "User is not an Instruction and not a Constant Expression");
            }
        }
    }

    // handle instructions using global variables
    for (auto *T = GlobalVarUsers.begin(); T != GlobalVarUsers.end(); ++T) {
        GlobalVariable *G = std::get<0>(*T);
        Instruction *I = std::get<1>(*T);

        Value *Index = ConstantPointerNull::get(IndexTy);
        handleUser(&*G, Index, &*G, I);
    }

    // handle GEPs using global variables
    for (auto *T = GlobalVarGEPs.begin(); T != GlobalVarGEPs.end(); ++T) {
        GlobalVariable *G = std::get<0>(*T);
        ConstantExpr *C = std::get<1>(*T);
        Instruction *I = std::get<2>(*T);

        auto *GEP = cast<GetElementPtrInst>(C->getAsInstruction());
        IRBuilder<> Builder(I);
        Value *GEP2 = Builder.CreateConstGEP1_32(G->getType(), &*G, 0);
        int64_t O = GEP->getPointerOffsetFrom(GEP2, M.getDataLayout()).value();
        GEP->dropAllReferences();
        dbgs() << "\tOffset: " << O << "\n";

        Constant *Offset = Constant::getIntegerValue(IntTy, APInt(32, O));
        //Value *Index = Builder.CreateConstGEP1_32(C->getType(), ConstantPointerNull::get(IndexTy), O);
        //G->replaceUsesOfWith(U, Index);
        handleUser(&*G, Offset, C, I);
    }

    for (Function &F : M) {
        if (F.isIntrinsic() || F.isDeclaration())
            continue;
        for (unsigned i = 0; i < F.arg_size(); ++i) {
            if (F.getArg(i)->getType()->isPointerTy())
                Changed |= visitPointerArgument(F.getArg(i), &F);
        }
        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (isa<AllocaInst>(&I)) {
                    Changed |= visitAlloc(&I);
                } else if (CallInst *CI = dyn_cast<CallInst>(&I)) {
                    if (CI->getIntrinsicID() == Intrinsic::orisc_allocate)
                        Changed |= visitAlloc(CI);
                    else if (CI->getIntrinsicID() == Intrinsic::orisc_box
                            || CI->getIntrinsicID() == Intrinsic::orisc_gep_p
                            || CI->getIntrinsicID() == Intrinsic::orisc_gep_i
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

    dbgs() << "FUN\n";
    for (auto G = M.global_begin(); G != M.global_end(); ++G) {
        for (auto U = G->user_begin(); U != G->user_end(); ++U) {
            if (Instruction *I = dyn_cast<Instruction>(*U)) {
                if (!I->getParent() || !I->getParent()->getParent()) {
                    //U->dropAllReferences();
                    I->dump();
                    if (U->isDroppable())
                        dbgs() << " is droppable";
                    else
                        dbgs() << "is NOT droppable";
                }
            }   
        }
    }


    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

bool BoxUnboxPointersPass::visitPointerArgument(Value *A, Function *Parent) {
    bool Changed = false;
    IRBuilder<> Builder(A->getContext());
    Builder.SetInsertPointPastAllocas(Parent);
    Value *Base = Builder.CreateCall(UnboxBaseFn, A, A->getName() + ".base");
    Value *Index = Builder.CreateCall(UnboxIndexFn, A, A->getName() + ".index");
    SmallVector<User *> Users = SmallVector<User *, 32>(A->users());
    for (User *U : Users) {
        U->dump();
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
    I->dump();
    bool Changed = false;
    SmallVector<User *> Users = SmallVector<User *, 32>(I->users());
    if (Users.empty())
        return false;
    IRBuilder<> Builder(I->getNextNode());
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

inline Value *BoxUnboxPointersPass::createGep(IRBuilder<> *Builder, Value *Base, Value * CurrentIndex) {
    Value *N;
    bool IsIndexPtrTy = CurrentIndex->getType()->isPointerTy();
    StringRef Name = Base->getName();
    if (Name.ends_with(".base"))
        Name = Name.substr(0, Name.size()-5);
    if (!Name.empty())
        N = Builder->CreateCall(IsIndexPtrTy ? GepPFn : GepIFn, {Base, CurrentIndex}, Name + ".gep");
    else
        N = Builder->CreateCall(IsIndexPtrTy ? GepPFn : GepIFn, {Base, CurrentIndex});

    return N;
} 

bool BoxUnboxPointersPass::handleUser(Value *Base, Value *CurrentIndex, Value *Parent, User *U) {
    bool Changed = false;
    IRBuilder<> Builder(U->getContext());
    if (GetElementPtrInst *G = dyn_cast<GetElementPtrInst>(U)) {
        if (G->getParent())
            RemoveFromParentList.push_back(G);
        SmallVector<Value *, 8> Indices(G->indices());
        Builder.SetInsertPoint(G->getNextNode());
        StringRef Name = G->getName();
        G->setName(Name + ".old");
        Value *NewG = Builder.CreateGEP(G->getSourceElementType(), CurrentIndex, Indices, Name, G->getNoWrapFlags());
        SmallVector<User *> Users = SmallVector<User *, 32>(G->users());
        for (User *GU : Users)
            Changed |= handleUser(Base, NewG, G, GU);
    } else if (Instruction *I = dyn_cast<Instruction>(U)) {
        Builder.SetInsertPoint(I);
        Value *N;
        if (isa<StoreInst>(I) && I->getOperand(1) == Parent) {
            N = createGep(&Builder, Base, CurrentIndex);
        } else if (isa<LoadInst>(I) && I->getOperand(0) == Parent) {
            N = createGep(&Builder, Base, CurrentIndex);
        } else if (isa<CallInst>(Base) && cast<CallInst>(Base)->getIntrinsicID() == Intrinsic::orisc_unbox_base
            && isa<CallInst>(CurrentIndex) && cast<CallInst>(CurrentIndex)->getIntrinsicID() == Intrinsic::orisc_unbox_index) {
            //If we are boxing something that just got unboxed, then just use the parent box
            N = cast<CallInst>(Base)->getOperand(0);
            addToRemoveIfNoUses(&RemoveFromParentList, cast<Instruction>(Base));
            addToRemoveIfNoUses(&RemoveFromParentList, cast<Instruction>(CurrentIndex));
        } else {
            if (!Base->getName().empty())
                N = Builder.CreateCall(BoxFn, {Base, CurrentIndex}, Base->getName() + ".box");
            else 
                N = Builder.CreateCall(BoxFn, {Base, CurrentIndex});
        }
        Changed = I->replaceUsesOfWith(Parent, N);
    }
    return Changed;
}