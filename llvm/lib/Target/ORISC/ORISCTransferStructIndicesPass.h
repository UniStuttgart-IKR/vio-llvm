#ifndef LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMSTRUCTINDICESPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMSTRUCTINDICESPASS_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include <string>
#include <utility>

namespace llvm {

class TransformStructIndicesPass : public PassInfoMixin<TransformStructIndicesPass>  {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    static bool isRequired() { return true; }

private:
    MapVector<Type *, std::pair<StructType *, StructType *>> SplitStructs;
    SmallVector<StructType *> PointerStructs;
    SmallVector<StructType *> PrimStructs;
    std::pair<StructType *, StructType *> splitStruct(Module &M, StructType *ST);
    std::pair<ArrayType *, ArrayType *> splitArray(Module &M, ArrayType *AT);
    bool hasPointerElements(ArrayType *AT);

    bool visitLoadInst(LoadInst *LI);
    bool visitStoreInst(StoreInst *SI);
    GetElementPtrInst *visitGetElementPtrInst(GetElementPtrInst *GEP, bool NeedPointers);
    friend PassInfoMixin<TransformStructIndicesPass>;
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_HELLONEW_HELLOWORLD_H