#include "Passes/ORISCEliminatePointerRedundanciesPass.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsORISC.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include <tuple>

using namespace llvm;

PreservedAnalyses EliminatePointerRedundanciesPass::run(Function &F, FunctionAnalysisManager &AM){
    DT = &AM.getResult<DominatorTreeAnalysis>(F);
    bool Changed = false;

    //First Iteration: Replace all load-pointers with the stored value if there was a
    //store to that address before the load in this function.
    //We hold a Map[Addr] = Value that is appended if a StorePointer is encountered.
    //If we see a LoadPointer, we check if the Addr is contained in the Map. If yes, the
    //LoadPointer is replaced by the Value. The Map is cleared at the beginning of every
    //new Function (FIXME: is this enough? Do we need to clr at every call as well?)
    auto S = SmallVector<StoreMapVector *>();
    DSV = &S;
    Changed |= recursivelyIterateBasicBlocks(DT->getRoot());
    Changed |= removeAllPendingInstructions();

    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

bool EliminatePointerRedundanciesPass::recursivelyIterateBasicBlocks(BasicBlock *Root){
    bool Changed = false;
    StoreMapVector CurrentStoreMap = StoreMapVector();
    DSV->push_back(&CurrentStoreMap);

    for (Instruction &I : *Root)
        if (auto *II = dyn_cast<IntrinsicInst>(&I)){
            if (II->getIntrinsicID() == Intrinsic::orisc_loadpointer){
                Changed |= visitLoadPointer(II);
            } else if (II->getIntrinsicID() == Intrinsic::orisc_storepointer){
                Value *Ptr = II->getArgOperand(1);
                if (StoreMapElement *El = findElement(Ptr))
                    if (!El->Live)
                        RemoveFromParentList.push_back(El->Inst);
                
                CurrentStoreMap.insert_or_assign(Ptr, StoreMapElement{II, false});
            }
        } else if (auto *CI = dyn_cast<CallInst>(&I)){
            for (unsigned i = 0; i < CI->getNumOperands(); ++i) {
                StoreMapElement *El = findElement(CI->getOperand(i));
                if (El) 
                    El->Live = true;
            }
        }

    SmallVector<BasicBlock *> Descendants = SmallVector<BasicBlock *>();
    DT->getDescendants(Root, Descendants);
    if (Descendants.size() > 1) {
        for (BasicBlock *BB : Descendants)
            if (BB != Root)
                Changed |= recursivelyIterateBasicBlocks(BB);
    }

    //Remove all store that are not live and the address originates from an alloca
    for (auto P : CurrentStoreMap)
        if (!P.second.Live){
            Value *V = P.second.Inst->getArgOperand(1);
            while (isa_and_nonnull<Instruction>(V)) {
                if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)){
                    V = GEP->getPointerOperand();
                } else if (isa<AllocaInst>(V)) {
                    RemoveFromParentList.push_back(P.second.Inst);
                    break;
                } else {
                    break;
                }
            }
        }
    
    DSV->pop_back();
    return Changed;
}

EliminatePointerRedundanciesPass::StoreMapElement *EliminatePointerRedundanciesPass::findElement(Value *Ptr){
    for (int i = DSV->size()-1; i >= 0; --i) {
        StoreMapVector *CurrentMap = DSV->data()[i];
        if (CurrentMap->contains(Ptr)) {
            return &(*CurrentMap)[Ptr];
        }
    }
    return nullptr;
}

bool EliminatePointerRedundanciesPass::visitLoadPointer(IntrinsicInst *I){
    StoreMapElement *El = findElement(I->getArgOperand(0));
    if (!El)
        return false;

    El->Live = true;
    Value *Val = El->Inst->getArgOperand(0);
    I->replaceAllUsesWith(Val);
    RemoveFromParentList.push_back(I);
    return true;
}

bool EliminatePointerRedundanciesPass::removeAllPendingInstructions() {
    bool Changed = false;
    for (Instruction *I : RemoveFromParentList) {
        I->eraseFromParent();
        Changed = true;
    }
    RemoveFromParentList.clear();
    return Changed;
}