#ifndef LLVM_LIB_TARGET_ORISC_ORISCREPLACEDEALLOCLIBCALLSPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCREPLACEDEALLOCLIBCALLSPASS_H

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class ReplaceDeAllocLibCallsPass : public PassInfoMixin<ReplaceDeAllocLibCallsPass>  {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    static bool isRequired() { return true; }
    
private:
    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();
    bool isMalloc(LibFunc LF);
    bool isCalloc(LibFunc LF);
    bool isRealloc(LibFunc LF);
    bool isFree(LibFunc LF);

    bool isReturningNew(LibFunc LF);
    bool isNew(LibFunc LF);
    bool isDelete(LibFunc LF);

    friend PassInfoMixin<ReplaceDeAllocLibCallsPass>;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCREPLACEDEALLOCLIBCALLSPASS_H