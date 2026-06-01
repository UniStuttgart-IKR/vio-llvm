#ifndef LLVM_LIB_TARGET_ORISC_ORISCREJECTUNSUPPORTEDIRPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCREJECTUNSUPPORTEDIRPASS_H

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class RejectUnsupportedIRPass : public PassInfoMixin<RejectUnsupportedIRPass>  {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    static bool isRequired() { return true; }
    
private:
    DataLayout DL;
    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();
    
    void visitPointerConversion(Instruction *I);
    void visitPointerCompare(ICmpInst *I);
    friend PassInfoMixin<RejectUnsupportedIRPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCREJECTUNSUPPORTEDIRPASS_H