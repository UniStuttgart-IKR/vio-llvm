#include "Passes/ORISCMoveAllocaOnHeapPass.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsORISC.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include <cassert>
#include <cstdint>

using namespace llvm;

/*
This first implementation is a bit over conservative:
1) Visit all Calls and Stores
2) Check Arguments for Pointers (for stores, only the ValueOperand)
3) If a Pointer is encountered, check the Parent of the Instruction
4) If it is a Load, we are done with this Argument
6) If it is a Function Argument, we are done with this Argument
5) If it is a Alloca, we replace it by a allocate intrinsic
6) Else, we check the Parent of this Instruction and continue with 4)

*/
PreservedAnalyses MoveAllocaOnHeapPass::run(Module &M, ModuleAnalysisManager &AM){
    LLVMContext &Ctx = M.getContext();

    Type *PtrTy = PointerType::get(Ctx, 0);
    Type *IntTy = Type::getInt32Ty(Ctx);
    FunctionType *AllocateFnTy = FunctionType::get(PtrTy, {IntTy, IntTy}, false);
    AllocateFn = M.getOrInsertFunction("llvm.orisc.allocate", AllocateFnTy);

    Zero = ConstantInt::get(Type::getInt32Ty(Ctx), 0);
    One =  ConstantInt::get(Type::getInt32Ty(Ctx), 1);

    bool Changed = false;
    for (Function &F : M)
        for (BasicBlock &BB : F)
            for (Instruction &I : BB)
                if (auto *CI = dyn_cast<CallInst>(&I))
                    Changed |= visitCallInst(CI);
                else if (auto *SI = dyn_cast<StoreInst>(&I))
                    Changed |= visitStoreInst(SI);
                else if (auto *RI = dyn_cast<ReturnInst>(&I))
                    Changed |= visitReturnInst(RI);

    for (Instruction *I : RemoveFromParentList) {
        I->eraseFromParent();
        Changed = true;
    }
    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

bool MoveAllocaOnHeapPass::visitCallInst(CallInst *I){
    //Intrinsics are skipped (TODO: can we safely do that?)
    if (I->getIntrinsicID() != Intrinsic::not_intrinsic)
        return false;

    bool Changed = false;
    for (Value *Arg : I->operand_values()) 
        Changed |= checkArgument(Arg);
    return Changed;
}

bool MoveAllocaOnHeapPass::visitStoreInst(StoreInst *I){
    return checkArgument(I->getValueOperand());
}

bool MoveAllocaOnHeapPass::visitReturnInst(ReturnInst *I){
    Value *RetVal = I->getReturnValue();
    if (!RetVal)
        return false;
    return checkArgument(RetVal);
}

bool MoveAllocaOnHeapPass::checkArgument(Value *Arg){
    if (auto *GEP = dyn_cast<GetElementPtrInst>(Arg))
        return checkArgument(GEP->getPointerOperand());

    if (isa<LoadInst>(Arg))
        return false;
    if (isa<Argument>(Arg))
        return false;
    if (isa<IntToPtrInst>(Arg))
        return false;

    AllocaInst *AI = dyn_cast<AllocaInst>(Arg);
    assert(AI && "Argument not created by Load, IncomingArg or Alloca?!");

    //Determine Pi and Delta
    ObjectSize OS = ObjectSize(AI);
    addTypeSizeToObjectSize(AI->getAllocatedType(), &OS);
    
    //Get Pi and Delta as "Value"
    Value *Pi = OS.Pi;
    if (!Pi) {
        Pi = ConstantInt::get(
                Type::getInt32Ty(AI->getContext()),
                OS.PiConst);
    }
    Value *Dt = OS.Dt;
    if (!Dt) {
        Dt = ConstantInt::get(
                Type::getInt32Ty(AI->getContext()),
                OS.DtConst);
    }

    //Replace Alloca by Intrinsic Call
    IRBuilder<> Builder(AI);
    CallInst *CI = Builder.CreateCall(AllocateFn, { Pi, Dt });
    AI->replaceAllUsesWith(CI);
    RemoveFromParentList.push_back(AI);

    return true;
}

//FIXME: Paddings inside Structs not considered
void MoveAllocaOnHeapPass::addTypeSizeToObjectSize(Type *AllocatedType, ObjectSize *OS){
    if (AllocatedType->isIntegerTy()){
        OS->DtConst = AllocatedType->getPrimitiveSizeInBits()/8;
        return;
    }
    if (AllocatedType->isPointerTy()) {
        OS->PiConst = 1;
        return;
    }
    if (AllocatedType->isArrayTy()) {
        ObjectSize ElOS(OS->AI);
        addTypeSizeToObjectSize(AllocatedType->getArrayElementType(), &ElOS);
        *OS = ElOS * AllocatedType->getArrayNumElements();
        return;
    }
    assert(AllocatedType->isStructTy() && "Not Array, Not Struct, Not Pointer, Not Integer?!");

    for (unsigned i = 0; i < AllocatedType->getStructNumElements(); ++i) {
        ObjectSize ElOS(OS->AI);
        addTypeSizeToObjectSize(AllocatedType->getStructElementType(i), &ElOS);
        *OS = *OS + ElOS;
    }
}
