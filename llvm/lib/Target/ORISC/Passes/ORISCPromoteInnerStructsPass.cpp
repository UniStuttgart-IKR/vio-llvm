#include "Passes/ORISCPromoteInnerStructsPass.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "ORISC-Inner-Struct-Promote"

PreservedAnalyses PromoteInnerStructsPass::run(Module &M, ModuleAnalysisManager &AM){
    LLVMContext &Ctx = M.getContext();
    PtrTy = PointerType::get(Ctx, 0);
    GepBuffer = SmallVector<GetElementPtrInst*, 64>();

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
                    splitAllocaInst(AI);
                else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I))
                    splitGEPInst(GEP);

    for (StructType *ST : StructTypes) {
        std::vector<Type*> *Types = new std::vector<Type*>();
        for (unsigned i = 0; i < ST->getNumElements(); ++i) {
            if (ReplacementRecorder[ST][i] && ST->getElementType(i)->isArrayTy()) {
                unsigned NumElements = ST->getElementType(i)->getArrayNumElements();
                ArrayType *NewATy = ArrayType::get(PtrTy, NumElements);
                Types->push_back(NewATy);
            } else if (ReplacementRecorder[ST][i]) {
                Types->push_back(PtrTy);
            } else if (ST->getElementType(i)->isStructTy()) {
                StructType *S = cast<StructType>(ST->getElementType(i));
                if (ReplaceBuffer.contains(S))
                    Types->push_back(ReplaceBuffer[S]);
                else
                    Types->push_back(S);
            } else {
                Types->push_back(ST->getElementType(i));
            }
        }
        StringRef NewName = ST->getStructName();
        ST->setName(NewName.str().append(".old"));
        StructType *NewStruct = StructType::create(ArrayRef<Type*>(*Types), NewName);
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

void PromoteInnerStructsPass::insertStructAlloca(AllocaInst *AI, StructType *Outer, unsigned IndexInOuter) {
        IRBuilder<> Builder(AI);
        StructType *Inner = cast<StructType>(Outer->getElementType(IndexInOuter));
        Builder.SetInsertPointPastAllocas(AI->getFunction());
        AllocaInst *InnerAI = Builder.CreateAlloca(Inner);
        Value *InnerGEP = Builder.CreateStructGEP(Outer, AI, IndexInOuter);
        Builder.CreateStore(InnerAI, InnerGEP, true);
        GepBuffer.push_back(cast<GetElementPtrInst>(InnerGEP));
}

void PromoteInnerStructsPass::insertArrayAlloca(AllocaInst *AI, StructType *Outer, unsigned IndexInOuter,
                                            ArrayType *Array, unsigned IndexInArray) {
        IRBuilder<> Builder(AI);
        StructType *Inner = cast<StructType>(Outer->getElementType(IndexInOuter)->getArrayElementType());
        AllocaInst *InnerAI = Builder.CreateAlloca(Inner);
        Builder.SetInsertPointPastAllocas(AI->getFunction());
        Value *InnerGEP = Builder.CreateStructGEP(Outer, AI, IndexInOuter);
        Value *ArrayGEP = Builder.CreateConstInBoundsGEP2_32(Array, InnerGEP, 0, IndexInArray);
        Builder.CreateStore(InnerAI, ArrayGEP, true);
        GepBuffer.push_back(cast<GetElementPtrInst>(InnerGEP));
        GepBuffer.push_back(cast<GetElementPtrInst>(ArrayGEP));
}

bool PromoteInnerStructsPass::splitAllocaInst(AllocaInst *AI) {
    if (!AI->getAllocatedType()->isStructTy())
        return false;

    StructType *Outer = cast<StructType>(AI->getAllocatedType());
    for (unsigned i = 0; i < Outer->getNumElements(); ++i) {
        if (Outer->getElementType(i)->isStructTy()) {
            insertStructAlloca(AI, Outer, i);
            ReplacementRecorder[Outer][i] = true;
            continue;
        }
        unsigned Layer = 0;
        Type *CTy = Outer->getElementType(i);
        while (ArrayType *ATy = dyn_cast<ArrayType>(CTy)) {
            ++Layer;
            CTy = ATy->getElementType();
            if (ATy->getElementType()->isArrayTy())
                continue;
            if (!ATy->getElementType()->isStructTy())
                continue;
            if (Layer > 1) {
                AI->dump();
                Outer->getElementType(i)->dump();
                dbgs() << "Layer: " << Layer << "\n";
                llvm_unreachable("Nested Arrays inside a Struct with a Struct as Element not implemented yet!");
            }
            for (unsigned j = 0; j < ATy->getNumElements(); ++j) {
                insertArrayAlloca(AI, Outer, i, ATy, j);
            }
            ReplacementRecorder[Outer][i] = true;
        }
    }
    return true;
}

bool PromoteInnerStructsPass::splitGEPInst(GetElementPtrInst *GEP) {
    if (GEP->getNumIndices() <= 2)
        return false;

    GEP->dump();
    llvm_unreachable("Splitting of GEPs not implemented yet");
    return true;    
}

bool PromoteInnerStructsPass::visitAllocaInst(AllocaInst *AI) {
    StructType *AITy = dyn_cast<StructType>(AI->getAllocatedType());
    if (!AITy)
        return false;
    if (!ReplaceBuffer.contains(AITy))
        return false;
    AI->setAllocatedType(ReplaceBuffer[AITy]);
    return true;
}

//This checks if this is a GEP that is part of an Object initialization.
//These GEPs obviously do not need a load-ptr between them and the store that
//is supposed to initialize the ptr field.
static inline bool containsGEP(SmallVector<GetElementPtrInst*> Buffer, GetElementPtrInst *GEP) {
    for (GetElementPtrInst* G : Buffer)
        if (G == GEP) return true;
    return false;
}

bool PromoteInnerStructsPass::visitGEPInst(GetElementPtrInst *GEP) {
    StructType *STy = dyn_cast<StructType>(GEP->getSourceElementType());
    ArrayType *ATy = dyn_cast<ArrayType>(GEP->getSourceElementType());
    if (ATy)
        STy = dyn_cast<StructType>(ATy->getElementType());
    if (!STy)
        return false;
    if (!ReplaceBuffer.contains(STy))
        return false;

    Type *NewType; 
    if (ATy && STy->isStructTy())
        NewType = ArrayType::get(PtrTy, ATy->getNumElements());
    else if (ATy)
        NewType = ArrayType::get(ReplaceBuffer[STy], ATy->getNumElements());
    else
        NewType = ReplaceBuffer[STy];
    GEP->setSourceElementType(NewType);
    
    Type *NewResTy = GetElementPtrInst::getIndexedType(GEP->getSourceElementType(), SmallVector<Value *, 16>(GEP->indices()));
    Type *OldResTy = GEP->getResultElementType();
    if (NewResTy != OldResTy) {
        GEP->setResultElementType(NewResTy);
        if (!containsGEP(GepBuffer, GEP) && NewResTy->isPointerTy()) { //We changed a ResTy that was not a Ptr to a Ptr, so we have to load whats at that position
            IRBuilder<> Builder(GEP->getNextNode()); //GEP is never last instruction of a basic block
            LoadInst *LI = Builder.CreateLoad(PtrTy, GEP);
            GEP->replaceAllUsesWith(LI);
            LI->setOperand(0, GEP);
        }
    }
    return false;
}