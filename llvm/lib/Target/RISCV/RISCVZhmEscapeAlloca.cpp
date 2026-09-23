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
#include <utility>

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
    
    Type *IntTy;
    if (M.getTargetTriple().isArch64Bit()) {
        IntTy = Type::getInt64Ty(Ctx);
    } else if (M.getTargetTriple().isArch32Bit()) {
        IntTy = Type::getInt32Ty(Ctx);
    } else {
        IntTy = Type::getInt128Ty(Ctx);
    }
    FunctionType *AllocateFnTy = FunctionType::get(PtrTy, {IntTy}, false);
    AlciFn = M.getOrInsertFunction("llvm.riscv.alci", AllocateFnTy);
    AlcidFn = M.getOrInsertFunction("llvm.riscv.alci.d", AllocateFnTy);
    StringRef AlcrName = M.getTargetTriple().isArch32Bit() ? "llvm.riscv.alc.32" : "llvm.riscv.alc.64";
    AlcrFn = M.getOrInsertFunction(AlcrName, AllocateFnTy);
    StringRef AlcrdName = M.getTargetTriple().isArch32Bit() ? "llvm.riscv.alc.32" : "llvm.riscv.alc.64";
    AlcrdFn = M.getOrInsertFunction(AlcrdName, AllocateFnTy);

    Zero = ConstantInt::get(Type::getInt32Ty(Ctx), 0);
    One =  ConstantInt::get(Type::getInt32Ty(Ctx), 1);

    StructTys = M.getIdentifiedStructTypes();
    DL = &M.getDataLayout();

    bool Changed = false;
    for (Function &F : M)
        for (BasicBlock &BB : F)
            for (Instruction &I : BB)
                if (AllocaInst *AI = dyn_cast<AllocaInst>(&I))
                    Changed |= visitAllocaInst(AI);
                else if (CallInst *CI = dyn_cast<CallInst>(&I))
                    Changed |= visitCallInst(CI);
                else if (StoreInst *SI = dyn_cast<StoreInst>(&I))
                    Changed |= visitStoreInst(SI);
                else if (ReturnInst *RI = dyn_cast<ReturnInst>(&I))
                    Changed |= visitReturnInst(RI);

    for (Instruction *I : RemoveFromParentList) {
        I->eraseFromParent();
        Changed = true;
    }
    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

static bool isDataOnly(Type *Ty) {
    if (Ty->isStructTy()) {
        StructType *STy = cast<StructType>(Ty);
        for (const auto *ElTy = STy->element_begin(); ElTy != STy->element_end(); ++ElTy) {
            if (!isDataOnly(*ElTy))
                return false;
        }
    } else if (Ty->isArrayTy()) {
        ArrayType *ATy = cast<ArrayType>(Ty);
        return isDataOnly(ATy->getArrayElementType());
    } else {
        return !Ty->isPointerTy();
    }
    return true; //unreachable?
}

static inline std::pair<SmallVector<Value *>, SmallVector<Type *>> extractVarArgs(CallInst *CI, unsigned Size) {
    SmallVector<Value *> VarArgs = SmallVector<Value *>(Size);
    SmallVector<Type *> VarArgTys = SmallVector<Type *>(Size);
    for (unsigned i = CI->getFunctionType()->getNumParams(); i < CI->arg_size(); ++i){
        VarArgs.push_back(CI->getArgOperand(i));
        VarArgTys.push_back(CI->getArgOperand(i)->getType());
    }
    return {VarArgs, VarArgTys};
}

static inline std::pair<SmallVector<Value *>, SmallVector<Type *>> vectorizeNewArguments(CallInst *CI, unsigned NumParams, Value *NewVarArgs) {
    SmallVector<Value *> NewArgs = SmallVector<Value *>(NumParams + 1);
    SmallVector<Type *> NewArgTys = SmallVector<Type *>(NumParams + 1);
    for (unsigned i = 0; i < NumParams; ++i){
        NewArgs.push_back(CI->getArgOperand(i));
        NewArgTys.push_back(CI->getArgOperand(i)->getType());
    }
    NewArgs.push_back(NewVarArgs);
    NewArgTys.push_back(NewVarArgs->getType());
    return {NewArgs, NewArgTys};
}

//Check if Alloca allocates a VLA
bool RISCVZhmEscapeAlloca::visitAllocaInst(AllocaInst *AI){
    std::optional<TypeSize> Size = AI->getAllocationSize(*DL);
    if (Size.has_value())
        return false;
    
    FunctionCallee Callee = isDataOnly(AI->getAllocatedType()) ? AlciFn : AlcrFn;
    replaceAlloca(Callee, AI->getArraySize(), AI);
    return true;
}

bool RISCVZhmEscapeAlloca::visitCallInst(CallInst *I){
    //Intrinsics are skipped (TODO: can we safely do that?)
    if (I->getIntrinsicID() != Intrinsic::not_intrinsic)
        if (I->getIntrinsicID() != Intrinsic::memset 
            && I->getIntrinsicID() != Intrinsic::memcpy 
            && I->getIntrinsicID() != Intrinsic::memmove)
            return false;

    bool Changed = false;
    //Handle VarArg Calls such that an Object is allocated for the Variable Arguments
    if (I->getFunctionType()->isVarArg()) {
        for (unsigned i = 0; i < I->getFunctionType()->getNumParams(); ++i){
            Changed |= checkArgument(I->getArgOperand(i));
        }
        unsigned Size = I->arg_size()-I->getFunctionType()->getNumParams();
        if (Size) {
            auto VarArgs = extractVarArgs(I, Size);
            Value *NewVarArgs = escapeVarArgs(I, &VarArgs.first, &VarArgs.second);
            auto NewArgs = vectorizeNewArguments(I, I->getFunctionType()->getNumParams(), NewVarArgs);
            replaceCall(I, &NewArgs.first, &NewArgs.second);
            Changed = true;
        }
        return Changed;
    }

    //Keep old behaviour for our sanity's sake
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

    //Get Size as Value and determine Callee
    Value *Size;
    FunctionCallee Callee;
    std::optional<TypeSize> ConstSize = AI->getAllocationSize(*DL);
    if (ConstSize.has_value()) {
        Size = ConstantInt::get(
                Type::getInt32Ty(AI->getContext()),
                ConstSize.value());
        Callee = isDataOnly(AI->getAllocatedType()) ? AlcidFn : AlciFn;
    } else {
        Size = AI->getArraySize();
        Callee = isDataOnly(AI->getAllocatedType()) ? AlcrdFn : AlcrFn;
    }

    //Replace Alloca by Intrinsic Call
    replaceAlloca(Callee, Size, AI);
    return true;
}

void RISCVZhmEscapeAlloca::replaceAlloca(FunctionCallee Callee, Value *Size, AllocaInst *AI){
    // Remove Lifetime Intrinsics on this Alloca, because Verifier will throw an error otherwise
    for (User *U : AI->users()) {
        if (CallInst *UCI = dyn_cast<CallInst>(U)) {
            if (UCI->getIntrinsicID() == Intrinsic::lifetime_start || UCI->getIntrinsicID() == Intrinsic::lifetime_end) {
                RemoveFromParentList.push_back(UCI);
            }
        }
    }
    IRBuilder<> Builder(AI);
    CallInst *CI = Builder.CreateCall(Callee, { Size });
    CI->takeName(AI);
    AI->replaceAllUsesWith(CI);
    RemoveFromParentList.push_back(AI);
}

void RISCVZhmEscapeAlloca::replaceCall(CallInst *CI, SmallVector<Value *> *NewArgs, SmallVector<Type *> *NewArgTys){
    IRBuilder<> Builder(CI);

    Type *RetType = CI->getFunctionType()->getReturnType();
    FunctionType *NewFuncType = FunctionType::get(RetType, *NewArgTys, /*isVarArg=*/false);
    Value *Callee = CI->getCalledOperand();
    CallInst *NewCall = Builder.CreateCall(NewFuncType, Callee, *NewArgs);

    NewCall->setCallingConv(CI->getCallingConv());
    NewCall->setTailCallKind(CI->getTailCallKind());
    
    if (!CI->getType()->isVoidTy()) {
        CI->replaceAllUsesWith(NewCall);
    }
    RemoveFromParentList.push_back(CI);
}

Value * RISCVZhmEscapeAlloca::escapeVarArgs(CallInst *I, SmallVector<Value *> *VarArgs, SmallVector<Type *> *VarArgTys){
    FunctionCallee Callee = AlcidFn;
    for (Type *Ty : *VarArgTys) {
        if (!isDataOnly(Ty)) {
            Callee = AlciFn;
            break;
        }
    }
    StructType *HelperTy = StructType::create(*VarArgTys);
    unsigned SizeInt = DL->getTypeAllocSize(HelperTy) / DL->getPointerSize();
    Value *Size = ConstantInt::get(
                Type::getInt32Ty(I->getContext()),
                SizeInt);
    IRBuilder<> Builder(I);
    return Builder.CreateCall(Callee, { Size });
}
