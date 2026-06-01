#ifndef LLVM_LIB_TARGET_ORISC_ORISCESCAPEOUTGOINGLOCALPOINTERSPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCESCAPEOUTGOINGLOCALPOINTERSPASS_H

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"

namespace llvm {

class EscapeOutgoingLocalPointersPass : public PassInfoMixin<EscapeOutgoingLocalPointersPass>  {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    static bool isRequired() { return true; }
    
private:
    FunctionCallee BoxFn;
    PointerType *IndexTy;

    bool visitReturnInst(ReturnInst *I);
    bool makeAllStoresVolatile(Value *V);
    
    friend PassInfoMixin<EscapeOutgoingLocalPointersPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCESCAPEOUTGOINGLOCALPOINTERSPASS_H