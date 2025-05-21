#include "ORISCTransferStructIndicesPass.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Analysis.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <utility>

using namespace llvm;

PreservedAnalyses TransformStructIndicesPass::run(Module &M, ModuleAnalysisManager &AM) {
    for (StructType *ST : M.getIdentifiedStructTypes())
        splitStruct(M, ST);
        //GlobalVariable(ST, true, GlobalValue::InternalLinkage);
        
    
    bool Changed = false;
    for (Function &F : M)
        for (BasicBlock &BB : F)
            for (Instruction &I : BB)
                if (auto *L = dyn_cast<LoadInst>(&I))
                    Changed |= visitLoadInst(L);
                else if (auto *S = dyn_cast<StoreInst>(&I))
                    Changed |= visitStoreInst(S);
    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

std::pair<StructType *, StructType *> TransformStructIndicesPass::splitStruct(Module &M, StructType *STy){
    if (SplitStructs.contains(STy))
        return SplitStructs[STy];

    for (StructType *S : PrimStructs)
        if (S == STy) return {nullptr, STy};
    for (StructType *S : PointerStructs)
        if (S == STy) return {STy, nullptr};

    SmallVector<Type *> PointerElements;
    SmallVector<Type *> PrimitiveElements;

    for (Type *Ty : STy->elements()){
        if (Ty->isArrayTy()) {
            auto Split = splitArray(M, cast<ArrayType>(Ty));
            if (Split.first)
                PointerElements.push_back(Split.first);
            if (Split.second)
                PrimitiveElements.push_back(Split.second);
        } else if (Ty->isPointerTy()) {
            PointerElements.push_back(Ty);
        } else if (Ty->isStructTy()) {
            auto Split = splitStruct(M, cast<StructType>(Ty));
            if (Split.first)
                PointerElements.push_back(Split.first);
            if (Split.second)
                PrimitiveElements.push_back(Split.second);
        } else {
            PrimitiveElements.push_back(Ty);
        }
    }

    if (PointerElements.empty()) {
        PrimStructs.push_back(STy);
        return {nullptr, STy};
    }
    if (PrimitiveElements.empty()) {
        PointerStructs.push_back(STy);
        return {STy, nullptr};
    }

    StructType *Pointers = StructType::create(PointerElements, 
                                                    STy->getName());
    StructType *Prims = StructType::create(PrimitiveElements,
                                                STy->getName());
    GlobalVariable *PtrGV = new GlobalVariable(Pointers, 
                                        true, 
                                            GlobalValue::ExternalLinkage);
    GlobalVariable *PriGV = new GlobalVariable(Prims, 
                                        true, 
                                            GlobalValue::ExternalLinkage);
    M.insertGlobalVariable(PtrGV);
    M.insertGlobalVariable(PriGV);
    SplitStructs.insert_or_assign(STy, std::make_pair(Pointers, Prims));
    return {Pointers, Prims};
}

std::pair<ArrayType *, ArrayType *> TransformStructIndicesPass::splitArray(Module &M, ArrayType *ATy){
    std::pair<Type *, Type *> Result;
    if (ATy->getElementType()->isStructTy()) {
        Result = splitStruct(M, cast<StructType>(ATy->getElementType()));
    } else if (ATy->getElementType()->isArrayTy()) {
        Result = splitArray(M, cast<ArrayType>(ATy->getElementType()));
    } else if (ATy->getElementType()->isPointerTy()) {
        return std::make_pair(ATy, nullptr);
    } else {
        return std::make_pair(nullptr, ATy);
    }

    if (!Result.first)
        return std::make_pair(nullptr, ATy);
    if (!Result.second)
        return std::make_pair(ATy, nullptr);

    ArrayType *PointersArray = ArrayType::get(Result.first, ATy->getNumElements());
    ArrayType *PrimitivesArray = ArrayType::get(Result.second, ATy->getNumElements());
    return {PointersArray, PrimitivesArray};
}

GetElementPtrInst *TransformStructIndicesPass::visitGetElementPtrInst(GetElementPtrInst *GEP, bool NeedPointers){
    Value *Pointer = GEP->getPointerOperand();
    if (auto *G = dyn_cast<GetElementPtrInst>(GEP->getPointerOperand()))
        Pointer = visitGetElementPtrInst(G, NeedPointers);

    if (!SplitStructs.contains(GEP->getSourceElementType()))
        return GEP;

    auto NewTypes = SplitStructs[GEP->getSourceElementType()];
    Type *ResTy = NeedPointers ? NewTypes.first : NewTypes.second;
    SmallVector<Value *> NewIndexList;
    IRBuilder<> Builder(GEP);
    return cast<GetElementPtrInst>(Builder.CreateGEP(ResTy, Pointer, NewIndexList));
}

bool TransformStructIndicesPass::visitLoadInst(LoadInst *LI){
    GetElementPtrInst *OldGEP = dyn_cast<GetElementPtrInst>(LI->getPointerOperand());
    if (!OldGEP)
        return false;

    GetElementPtrInst *NewGEP = visitGetElementPtrInst(OldGEP, LI->getPointerOperandType()->isPointerTy());
    if (NewGEP == OldGEP)
        return false;

    LI->setOperand(0, NewGEP);
    return true;
}

bool TransformStructIndicesPass::visitStoreInst(StoreInst *SI){
    GetElementPtrInst *OldGEP = dyn_cast<GetElementPtrInst>(SI->getPointerOperand());
    if (!OldGEP)
        return false;

    GetElementPtrInst *NewGEP = visitGetElementPtrInst(OldGEP, SI->getPointerOperandType()->isPointerTy());
    if (NewGEP == OldGEP)
        return false;

    SI->setOperand(1, NewGEP);
    return true;
}