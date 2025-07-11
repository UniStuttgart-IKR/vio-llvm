#include "Passes/ORISCRejectUnsupportedIRPass.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/Casting.h"
#include <string>

using namespace llvm;

static void emitError(Instruction &I, std::string Message) {
    I.getContext().diagnose(DiagnosticInfoUnsupported(
        *I.getFunction(), Message, I.getDebugLoc(), llvm::DS_Error
    ));
}

static void emitWarning(Instruction &I, std::string Message) {
    I.getContext().diagnose(DiagnosticInfoUnsupported(
        *I.getFunction(), Message, I.getDebugLoc(), llvm::DS_Warning
    ));
}

PreservedAnalyses RejectUnsupportedIRPass::run(Function &F, FunctionAnalysisManager &AM){
    DL = F.getParent()->getDataLayout();
    for (Instruction &I : instructions(F)){
        if (isa<PtrToIntInst>(I) || isa<IntToPtrInst>(I))
            visitPointerConversion(&I);
        else if (auto *ICmp = dyn_cast<ICmpInst>(&I)){
            visitPointerCompare(ICmp);
        }
    }
    
    bool Changed = false;
    for (Instruction *I : RemoveFromParentList) {
        I->eraseFromParent();
        Changed = true;
    }
    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

void RejectUnsupportedIRPass::visitPointerConversion(Instruction *I){
    /*if (Value *Folded = simplifyInstruction(I, {DL})) {
        I->replaceAllUsesWith(Folded);
        RemoveFromParentList.push_back(I);
        return;
    }*/
    
    if (isa<PtrToIntInst>(I))
        emitError(*I, "Casting Pointers to Integers is not supported by the Objective-RISC Target");
    else
        emitError(*I, "Casting Integers to Pointers is not supported by the Objective-RISC Target");
    report_fatal_error("unsupported pointer ↔ integer conversion");
}

void RejectUnsupportedIRPass::visitPointerCompare(ICmpInst *ICmp){
    Value *LHS = ICmp->getOperand(0);
    Value *RHS = ICmp->getOperand(1);
    Type *LTy = LHS->getType();
    Type *RTy = RHS->getType();
    ICmpInst::Predicate Pred = ICmp->getPredicate();

    bool LIsPtr = LTy->isPointerTy();
    bool RIsPtr = RTy->isPointerTy();

    //Pointer vs Integer: warn for EQ and NEQ, else error
    if (LIsPtr != RIsPtr) {
        if (ICmp->isEquality()) {
            emitWarning(*ICmp, "Pointers and Integers are always unequal for the Objective-RISC Target");

            bool IsNE = Pred == ICmpInst::ICMP_NE;
            Value *Const = ConstantInt::get(ICmp->getType(), IsNE);
            ICmp->replaceAllUsesWith(Const);
            RemoveFromParentList.push_back(ICmp);
            return;
        }
    }

    //Pointers vs Pointer: allowed for EQ and NEQ, else error
    if (LIsPtr && RIsPtr)
        if (ICmp->isEquality())
            return;

            /*
    //Check if we can simplify this, resolving the issue
    if (Value *Folded = simplifyInstruction(ICmp, {DL})) {
        ICmp->replaceAllUsesWith(Folded);
        RemoveFromParentList.push_back(ICmp);
        return;
    }*/

    emitError(*ICmp, "Ordered Compares (<, <=, >, >=) using Pointers are illegal for the Objective-RISC Target");
    report_fatal_error("ordered pointer comparison not supported");
}