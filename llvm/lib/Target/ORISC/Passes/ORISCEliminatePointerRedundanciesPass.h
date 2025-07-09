#ifndef LLVM_LIB_TARGET_ORISC_ORISCELIMINATEPOINTERREDUNDANCIESPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCELIMINATEPOINTERREDUNDANCIESPASS_H

#include "llvm/ADT/MapVector.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include <tuple>

namespace llvm {

//FIXME: Terribly unoptimized. Future Work (hopefully)
class EliminatePointerRedundanciesPass : public PassInfoMixin<EliminatePointerRedundanciesPass>  {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    static bool isRequired() { return true; }
    
private:
    struct StoreMapElement {
        IntrinsicInst *Inst;
        bool Live;
    };
    typedef MapVector<Value *, StoreMapElement> StoreMapVector;

    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();
    const DominatorTree *DT;
    SmallVector<StoreMapVector *> *DSV;

    bool visitLoadPointer(IntrinsicInst *I);
    bool recursivelyIterateBasicBlocks(BasicBlock *Root);
    void clearStoreMap();

    StoreMapElement *findElement(Value *Ptr);
    bool removeAllPendingInstructions();

    friend PassInfoMixin<EliminatePointerRedundanciesPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCELIMINATEPOINTERREDUNDANCIESPASS_H