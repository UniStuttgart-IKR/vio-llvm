#ifndef LLVM_LIB_TARGET_ORISC_ORISCPROMOTEINNERSTRUCTSPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCPROMOTEINNERSTRUCTSPASS_H

#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include <map>
#include <vector>

namespace llvm {

class PromoteInnerStructsPass : public PassInfoMixin<PromoteInnerStructsPass>  {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    static bool isRequired() { return true; }

private:
    typedef SmallMapVector<StructType*, std::vector<bool>, 32> RecorderType;
    RecorderType ReplacementRecorder;
    typedef SmallMapVector<StructType*, StructType*, 32> BufferType;
    BufferType ReplaceBuffer;

    SmallVector<GetElementPtrInst*> GepBuffer;

    Type *PtrTy;

    bool splitAllocaInst(AllocaInst *AI);
    bool splitGEPInst(GetElementPtrInst *GEP);
    bool visitAllocaInst(AllocaInst *I);
    bool visitGEPInst(GetElementPtrInst *GEP);

    void insertStructAlloca(AllocaInst *AI, StructType *Outer, unsigned int IndexInOuter);
    void insertArrayAlloca(AllocaInst *AI, StructType *Outer, unsigned int IndexInOuter,
                           ArrayType *Array, unsigned int IndexInArray);

    friend PassInfoMixin<PromoteInnerStructsPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCPROMOTEINNERSTRUCTSPASS_H