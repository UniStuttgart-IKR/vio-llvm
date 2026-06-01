#include "Passes/ORISCReplaceDeAllocLibCallsPass.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Analysis.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/WithColor.h"
#include <cstdlib>
#include <string>

using namespace llvm;

PreservedAnalyses ReplaceDeAllocLibCallsPass::run(Function &F, FunctionAnalysisManager &AM){
    bool Changed = false;

    //TargetLibraryInfo &TLI = AM.getResult<TargetLibraryAnalysis>(F);

    for (BasicBlock &BB : F) 
        for (Instruction &I : BB)
            if (CallInst *CI = dyn_cast<CallInst>(&I)) {
                LibFunc LF;
                //if (TLI.getLibFunc(*CI->getCalledFunction(), LF))
                //    CI->getCalledFunction()->dump();
            }

    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

bool ReplaceDeAllocLibCallsPass::isNew(LibFunc LF) {
    //TODO: Add all other Functions to this!!
    return LF == LibFunc_Znaj;
}