#ifndef LLVM_LIB_TARGET_ORISC_ORISCSPLITMIXEDSTRUCTSPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCSPLITMIXEDSTRUCTSPASS_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include <utility>

namespace llvm {

class SplitMixedStructsPass : public PassInfoMixin<SplitMixedStructsPass>  {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    static bool isRequired() { return true; }

private:
    typedef SmallMapVector<StructType*, std::pair<StructType*, StructType*>, 32> BufferType;
    BufferType ReplaceBuffer;
    typedef SmallMapVector<StructType*, std::vector<unsigned>, 32> BufferIndices;
    BufferIndices ReplaceIndices;

    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();

    bool visitUsers(Value *User);
    bool visitGEPInst(GetElementPtrInst *GEP);

    friend PassInfoMixin<SplitMixedStructsPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCSPLITMIXEDSTRUCTSPASS_H