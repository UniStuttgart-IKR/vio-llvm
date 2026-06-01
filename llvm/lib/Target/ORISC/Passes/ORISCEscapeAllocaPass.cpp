#include "Passes/ORISCEscapeAllocaPass.h"
#include "Passes/ORISCStructLayoutHelper.h"
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
#include <cstdint>

using namespace llvm;

#define DEBUG_TYPE "ORISC-Alloca-Escape"

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
PreservedAnalyses EscapeAllocaPass::run(Module &M, ModuleAnalysisManager &AM){
    LLVMContext &Ctx = M.getContext();

    Type *PtrTy = PointerType::get(Ctx, 0);
    Type *IntTy = Type::getInt32Ty(Ctx);
    FunctionType *AllocateFnTy = FunctionType::get(PtrTy, {IntTy, IntTy}, false);
    AllocateFn = M.getOrInsertFunction("llvm.orisc.allocate", AllocateFnTy);

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

bool EscapeAllocaPass::visitCallInst(CallInst *I){
    //Intrinsics are skipped (TODO: can we safely do that?)
    if (I->getIntrinsicID() != Intrinsic::not_intrinsic)
        return false;

    bool Changed = false;
    for (Value *Arg : I->operand_values()) 
        Changed |= checkArgument(Arg);
    return Changed;
}

bool EscapeAllocaPass::visitStoreInst(StoreInst *I){
    return checkArgument(I->getValueOperand());
}

bool EscapeAllocaPass::visitReturnInst(ReturnInst *I){
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
bool EscapeAllocaPass::checkArgument(Value *Arg){
    //Only Pointer Arguments can lead us to a Alloca
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

    //Determine Pi and Delta
    ObjectSize OS = ObjectSize(AI);
    addTypeSizeToObjectSize(AI->getAllocatedType(), &OS);
    if (AI->isArrayAllocation())
        OS = OS * AI->getArraySize();
    
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
    CI->takeName(AI);
    AI->replaceAllUsesWith(CI);
    RemoveFromParentList.push_back(AI);

    return true;
}

//FIXME: Paddings inside Structs not considered
void EscapeAllocaPass::addTypeSizeToObjectSize(Type *AllocatedType, ObjectSize *OS){
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
    //find ptr and prm structs
    unsigned Pi = 0;
    unsigned Dt = 0;
    std::string PtrName = (cast<StructType>(AllocatedType)->getName() + ".ptr").str();
    std::string PrmName = (cast<StructType>(AllocatedType)->getName() + ".prm").str();
    for (StructType *STy : StructTys){
        if (PtrName == STy->getName().str()) {
            Pi = DL->getTypeAllocSize(STy) / DL->getPointerSize();
        } else if (PrmName == STy->getName().str()) {
            Dt = DL->getTypeAllocSize(STy);
        }
    }
    if (Pi > 0 || Dt > 0) {
        *OS = *OS + ObjectSize(OS->AI, nullptr, Pi, nullptr, Dt);
        return;
    }

    //Fallback if StructTypes where destroyed: recalculate them!
    if (!FilledReplaceBuffer) {
        splitStructs(*OS->AI->getModule(), StructTys, &ReplaceBuffer);
        FilledReplaceBuffer = true;
    }
    if (ReplaceBuffer.contains(cast<StructType>(AllocatedType))) {
        auto STy = ReplaceBuffer[cast<StructType>(AllocatedType)];
        Pi = DL->getTypeAllocSize(STy.first) / DL->getPointerSize();
        Dt = DL->getTypeAllocSize(STy.second);
        if (Pi > 0 || Dt > 0) {
            *OS = *OS + ObjectSize(OS->AI, nullptr, Pi, nullptr, Dt);
            return;
        }
    }

    // Fallback Fallback if we couldnt find any split structs
    for (unsigned i = 0; i < AllocatedType->getStructNumElements(); ++i) {
        ObjectSize ElOS(OS->AI);
        addTypeSizeToObjectSize(AllocatedType->getStructElementType(i), &ElOS);
        *OS = *OS + ElOS;
    }
}
