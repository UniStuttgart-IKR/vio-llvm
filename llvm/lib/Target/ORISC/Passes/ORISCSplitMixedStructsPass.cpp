#include "Passes/ORISCSplitMixedStructsPass.h"
#include "Passes/ORISCStructLayoutHelper.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DataLayout.h"
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
#include "llvm/Support/MathExtras.h"
#include <cinttypes>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "ORISC-Mixed-Struct-Split"

PreservedAnalyses SplitMixedStructsPass::run(Module &M, ModuleAnalysisManager &AM){
    //We can assume that there are no inner Structs inside a Struct (handled by PromoteInnerStrucsPass)
    //Only primitives, pointers and (nested & single) arrays of such
    std::vector<StructType *> StructTypes = M.getIdentifiedStructTypes();
    splitStructs(M, StructTypes, &ReplaceBuffer, &ReplaceIndices);

    bool Changed = false;
    for (Function &F : M)
        for (BasicBlock &BB : F)
            for (Instruction &I : BB)
                if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I))
                    Changed |= visitGEPInst(GEP);

    for (Instruction *I : RemoveFromParentList) {
        I->eraseFromParent();
        Changed = true;
    }
    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

bool SplitMixedStructsPass::visitGEPInst(GetElementPtrInst *GEP) {
    StructType *STy = dyn_cast<StructType>(GEP->getSourceElementType());
    if (!STy)
        return false;
    if (!ReplaceBuffer.contains(STy))
        return false;
    
    ConstantInt *I = dyn_cast<ConstantInt>(GEP->getOperand(2));
    if (!I) //Structs have to be indexed by a constant
        return false;

    unsigned Index = I->getZExtValue();
    if (STy->getTypeAtIndex(Index) != GEP->getResultElementType()) {
        GEP->getSourceElementType()->dump();
        GEP->getResultElementType()->dump();
        llvm_unreachable("Did not find ResultElementType in Source!?");
        return false;
    }

    //Array of Pointers should also emit reference to ptr struct
    Type *ResTy = GEP->getResultElementType();
    while (ArrayType *ATy = dyn_cast<ArrayType>(ResTy))
        ResTy = ATy->getArrayElementType();
    assert(!ResTy->isStructTy() && "PromoteInnerStructs Pass should have eliminated this inner Struct!");

    //At this Point, ResTy can only be single Pointer or Primitive
    IRBuilder<> Builder(GEP);
    StructType *NewStructTy = ResTy->isPointerTy() ? ReplaceBuffer[STy].first : ReplaceBuffer[STy].second;
    unsigned NewIndex = ReplaceIndices[STy][Index];
    Value *NewGEP = Builder.CreateStructGEP(NewStructTy, GEP->getPointerOperand(), NewIndex);
    GEP->replaceAllUsesWith(NewGEP);
    RemoveFromParentList.push_back(GEP);
    return true;
}