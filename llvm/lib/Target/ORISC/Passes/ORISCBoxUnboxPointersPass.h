#ifndef LLVM_LIB_TARGET_ORISC_ORISCBOXUNBOXPOINTERSPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCBOXUNBOXPOINTERSPASS_H

#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include <cstdint>
#include <utility>

namespace llvm {

class BoxUnboxPointersPass : public RequiredPassInfoMixin<BoxUnboxPointersPass>  {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    static bool isRequired() { return true; }

private:
    typedef std::pair<Value *, Value *> FatPtr;
    typedef std::pair<Value *, uint8_t> MemCpyMovArgument;

    FunctionCallee GepPFn;
    FunctionCallee GepIFn;
    FunctionCallee BoxFn;
    FunctionCallee UnboxBaseFn;
    FunctionCallee UnboxIndexFn;
    Type *IntTy;
    PointerType *BaseTy;
    PointerType *IndexTy;
    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();
    MapVector<Instruction *, MemCpyMovArgument> MemCpyMovMap = MapVector<Instruction *, MemCpyMovArgument>();

    bool visitPointerArgument(Value *A, Function *Parent);
    bool visitAlloc(Instruction *I);
    bool visitOther(Instruction *I);
    bool handleUser(Value *Base, Value *CurrentIndex, Value *Parent, User *U);
    bool handleMemIntrinsics(Value *Parent, CallInst *Call);

    inline Value *createGep(IRBuilder<> *Builder, Value *Base, Value * CurrentIndex);

    friend RequiredPassInfoMixin<BoxUnboxPointersPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCBOXUNBOXPOINTERSPASS_H