#include "Passes/ORISCReplaceNullPointersPass.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Analysis.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Function.h"
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

PreservedAnalyses ReplaceNullPointersPass::run(Module &M, ModuleAnalysisManager &AM){
    bool Changed = false;

    PointerType *PtrTy = PointerType::get(M.getContext(), 0);
    FunctionType *NullFnTy = FunctionType::get(PtrTy, {}, false);
    FunctionCallee NullFn = M.getOrInsertFunction("llvm.orisc.null", NullFnTy);

    for (Function &F : M) {
        Value *Null = nullptr;
        for (BasicBlock &BB : F) 
            for (Instruction &I : BB) 
                for (unsigned OI = 0; OI < I.getNumOperands(); ++OI)
                    if (I.getOperand(OI)->getType()->isPointerTy() 
                        && I.getOperand(OI)->getType()->getPointerAddressSpace() == 0 
                        && isa<ConstantPointerNull>(I.getOperand(OI))) {
                            if (!Null) {
                                IRBuilder<> Builder(M.getContext());
                                Builder.SetInsertPointPastAllocas(&F);
                                Null = Builder.CreateCall(NullFn, {}, "null");
                            }
                            I.setOperand(OI, Null);
                            Changed = true;
                        }
    }

    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}