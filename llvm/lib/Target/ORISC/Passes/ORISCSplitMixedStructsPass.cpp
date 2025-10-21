#include "Passes/ORISCSplitMixedStructsPass.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "ORISC-Mixed-Struct-Split"

PreservedAnalyses SplitMixedStructsPass::run(Module &M, ModuleAnalysisManager &AM){
    LLVMContext &Ctx = M.getContext();

    //We can assume that there are no inner Structs inside a Struct (handled by PromoteInnerStrucsPass)
    //Only primitives, pointers and (nested & single) arrays of such
    std::vector<StructType *> StructTypes = M.getIdentifiedStructTypes();
    for (StructType *ST : StructTypes){
        std::vector<Type*> Primitives = std::vector<Type*>();
        std::vector<Type*> Pointers = std::vector<Type*>();
        std::vector<unsigned> NewIndices = std::vector<unsigned>();
        for (unsigned i = 0; i < ST->getNumElements(); ++i) {
            if (ST->getElementType(i)->isPointerTy()) {
                NewIndices.push_back(Pointers.size());
                Pointers.push_back(ST->getElementType(i));
            } else if (!ST->getElementType(i)->isArrayTy()) { //aka is Primitive
                NewIndices.push_back(Primitives.size());
                Primitives.push_back(ST->getElementType(i));
            } else {
                ArrayType *ATy = cast<ArrayType>(ST->getElementType(i));
                while (ATy->getElementType()->isArrayTy())
                    ATy = cast<ArrayType>(ATy->getElementType());
                if (ATy->getElementType()->isPointerTy()) {
                    NewIndices.push_back(Pointers.size());
                    Pointers.push_back(ST->getElementType(i));
                } else {
                    NewIndices.push_back(Primitives.size());
                    Primitives.push_back(ST->getElementType(i));
                }
            }
        }
        if (Pointers.size() > 0 && Primitives.size() > 0) {
            ReplaceIndices.insert_or_assign(ST, NewIndices);
            std::string OldName = ST->getStructName().str();
            StructType *NewPrmStruct = StructType::create(ArrayRef<Type*>(Primitives), OldName.append(".prm"));
            StructType *NewPtrStruct = StructType::create(ArrayRef<Type*>(Pointers), OldName.substr(0, OldName.size()-4).append(".ptr"));
            std::pair<StructType*, StructType*> NewPair = { NewPrmStruct, NewPtrStruct };
            ReplaceBuffer.insert_or_assign(ST, NewPair);
        }
    }

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
    
    unsigned Index;
    for (Index = 0; Index < STy->getNumElements(); ++Index){
        if (STy->getTypeAtIndex(Index) == GEP->getResultElementType())
            break;
    }
    if (STy->getTypeAtIndex(Index) != GEP->getResultElementType()) {
        GEP->getSourceElementType()->dump();
        GEP->getResultElementType()->dump();
        llvm_unreachable("Did not find ResultElementType in Source!?");
        return false;
    }
    IRBuilder<> Builder(GEP);
    StructType *NewStructTy = GEP->getResultElementType()->isPointerTy() ? ReplaceBuffer[STy].first : ReplaceBuffer[STy].second;
    unsigned NewIndex = ReplaceIndices[STy][Index];
    Value *NewGEP = Builder.CreateStructGEP(NewStructTy, GEP->getPointerOperand(), NewIndex);
    GEP->replaceAllUsesWith(NewGEP);
    RemoveFromParentList.push_back(GEP);
    return true;
}