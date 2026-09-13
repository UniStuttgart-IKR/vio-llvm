#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constant.h"
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
#include "llvm/Support/Debug.h"
#include <cassert>
#include "RISCVZhmEscapeAlloca.h"
#include <cstdint>

using namespace llvm;

#define DEBUG_TYPE "RISCV-Zhm-Escape-Alloca"

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
PreservedAnalyses RISCVZhmEscapeAlloca::run(Module &M, ModuleAnalysisManager &AM){
    
    LLVMContext &Ctx = M.getContext();
    
    Type *PtrTy = PointerType::get(Ctx, 0);
    Type *IntTy = Type::getInt32Ty(Ctx);
    FunctionType *AllocateFnTy = FunctionType::get(PtrTy, {IntTy}, false);
    AllocateFn = M.getOrInsertFunction("llvm.riscv.alci", AllocateFnTy);

    Zero = ConstantInt::get(Type::getInt32Ty(Ctx), 0);
    One =  ConstantInt::get(Type::getInt32Ty(Ctx), 1);

    StructTys = M.getIdentifiedStructTypes();
    DL = &M.getDataLayout();

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

bool RISCVZhmEscapeAlloca::visitCallInst(CallInst *I){
    //Intrinsics are skipped (TODO: can we safely do that?)
    if (I->getIntrinsicID() != Intrinsic::not_intrinsic)
        return false;

    bool Changed = false;
    for (Value *Arg : I->operand_values()) 
        Changed |= checkArgument(Arg);
    return Changed;
}

bool RISCVZhmEscapeAlloca::visitStoreInst(StoreInst *I){
    return checkArgument(I->getValueOperand());
}

bool RISCVZhmEscapeAlloca::visitReturnInst(ReturnInst *I){
    Value *RetVal = I->getReturnValue();
    if (!RetVal)
        return false;
    CallInst *Call = dyn_cast<CallInst>(RetVal);
    if (Call && Call->getIntrinsicID() == Intrinsic::orisc_box){
        RetVal = Call->getOperand(0);
        I->setOperand(0, RetVal);
        RemoveFromParentList.push_back(Call);
    }
    return checkArgument(RetVal);
}

//Follow Argument Parents until we meet a Alloca
bool RISCVZhmEscapeAlloca::checkArgument(Value *Arg){
    //Only Pointer Arguments can lead us to an Alloca
    if (!Arg->getType()->isPointerTy())
        return false;
    if (auto *GEP = dyn_cast<GetElementPtrInst>(Arg))
        return checkArgument(GEP->getPointerOperand());

    if (isa<LoadInst>(Arg))
        return false;
    if (isa<Argument>(Arg))
        return false;
    if (isa<CallInst>(Arg))
        return false;
    if (isa<IntToPtrInst>(Arg))
        return false;
    if (isa<Constant>(Arg))
        return false;

    AllocaInst *AI = dyn_cast<AllocaInst>(Arg);
    assert(AI && "Argument not created by Load, IncomingArg or Alloca?!");

    //Get Size as "Value"
    Value *Size;
    std::optional<TypeSize> ConstSize = AI->getAllocationSize(*DL);
    if (ConstSize.has_value()) {
        Size = ConstantInt::get(
                Type::getInt32Ty(AI->getContext()),
                ConstSize.value());
    } else {
        Size = AI->getArraySize();
    }

    //Replace Alloca by Intrinsic Call
    IRBuilder<> Builder(AI);
    CallInst *CI = Builder.CreateCall(AllocateFn, { Size });
    CI->takeName(AI);
    AI->replaceAllUsesWith(CI);
    RemoveFromParentList.push_back(AI);

    return true;
}
