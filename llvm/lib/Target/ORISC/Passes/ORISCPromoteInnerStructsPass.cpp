#include "Passes/ORISCPromoteInnerStructsPass.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "ORISC-Inner-Struct-Promote"

PreservedAnalyses PromoteInnerStructsPass::run(Module &M, ModuleAnalysisManager &AM){
    LLVMContext &Ctx = M.getContext();
    Type *PtrTy = PointerType::get(Ctx, 0);

    std::vector<StructType *> StructTypes = M.getIdentifiedStructTypes();
    for (StructType *st : StructTypes){
        auto arr = std::vector<bool>(st->getNumElements());
        for (unsigned i = 0; i < st->getNumElements(); ++i)
            arr[i] = false;
        ReplacementRecorder.insert({st, arr});
    }

    for (Function &F : M)
        for (BasicBlock &BB : F)
            for (Instruction &I : BB)
                if (AllocaInst *AI = dyn_cast<AllocaInst>(&I))
                    createAllocaInst(AI);
                else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I))
                    splitGEPInst(GEP);

    for (StructType *ST : StructTypes) {
        std::vector<Type*> *Types = new std::vector<Type*>(ST->getNumElements());
        for (unsigned i = 0; i < ST->getNumElements(); ++i) {
            if (ReplacementRecorder[ST][i])
                Types->push_back(PtrTy);
            else
                Types->push_back(ST->getElementType(i));
        }
        StructType *NewStruct = StructType::get(Ctx, Types);
        ReplaceBuffer.insert({ST, NewStruct});
    }

    for (Function &F : M)
        for (BasicBlock &BB : F)
            for (Instruction &I : BB)
                if (AllocaInst *AI = dyn_cast<AllocaInst>(&I))
                    visitAllocaInst(AI);
                else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I))
                    visitGEPInst(GEP);

    return PreservedAnalyses::none();
}

bool PromoteInnerStructsPass::createAllocaInst(AllocaInst *AI) {
    if (!AI->getAllocatedType()->isStructTy())
        return false;

    StructType *Outer = cast<StructType>(AI->getAllocatedType());
    for (unsigned i = 0; i < Outer->getNumElements(); ++i) {
        if (!Outer->getElementType(i)->isStructTy())
            continue;
        IRBuilder<> Builder(AI->getNextNonDebugInstruction());
        StructType *Inner = cast<StructType>(Outer->getElementType(i));
        AllocaInst *InnerAI = Builder.CreateAlloca(Inner);
        Value *InnerGEP = Builder.CreateStructGEP(Outer, AI, i);
        Builder.CreateStore(InnerAI, InnerGEP);

        ReplacementRecorder[Outer][i] = true;
    }
    return true;
}

bool PromoteInnerStructsPass::splitGEPInst(GetElementPtrInst *GEP) {
    return false;
}

bool PromoteInnerStructsPass::visitAllocaInst(AllocaInst *AI) {
    return false;
}

bool PromoteInnerStructsPass::visitGEPInst(GetElementPtrInst *GEP) {
    return false;
}