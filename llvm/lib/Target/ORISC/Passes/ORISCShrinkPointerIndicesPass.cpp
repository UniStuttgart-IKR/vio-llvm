#include "ORISC.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/IPO.h"
using namespace llvm;

namespace {
class ORISCShrinkPointerIndices : public ModulePass {
public:
  static char ID; // Pass identification.

  ORISCShrinkPointerIndices() : ModulePass(ID) {
    initializeORISCShrinkPointerIndicesPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override;
  bool transformPointerIndices(GetElementPtrInst *GEP);
};
}


bool ORISCShrinkPointerIndices::runOnModule(Module &M) {
    bool Changed = false;
    for (Function &F : M)
        for (BasicBlock &BB : F)
            for (Instruction &I : BB)
                if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I)) {
                    Changed |= transformPointerIndices(GEP);
                }

    return Changed;
}

bool ORISCShrinkPointerIndices::transformPointerIndices(GetElementPtrInst *GEP) {
    if (!GEP->getResultElementType()->isPointerTy())
        return false;

    unsigned LastGepOperandNum = GEP->getNumOperands()-1;
    Value *V = GEP->getOperand(LastGepOperandNum);
    if (ConstantInt *I = dyn_cast<ConstantInt>(V)) { //is it constant, just statically divide by 4
        uint64_t NewIndex = I->getZExtValue() / GEP->getDataLayout().getPointerSize();
        Constant *NewIndexVal = ConstantInt::get(I->getType(), NewIndex);
        GEP->setOperand(LastGepOperandNum, NewIndexVal);
    } else { //else, squeeze sra between source and gep
        Constant *Shamt = ConstantInt::get(I->getType(), GEP->getDataLayout().getPointerSize());
        IRBuilder<> Builder(GEP);
        Value *NewIndex = Builder.CreateAShr(V, Shamt);
        GEP->setOperand(LastGepOperandNum, NewIndex);
    }
    return true;
}

ModulePass *llvm::createORISCShrinkPointerIndicesPass() { return new ORISCShrinkPointerIndices(); }

char ORISCShrinkPointerIndices::ID = 0;
INITIALIZE_PASS(ORISCShrinkPointerIndices, "pointer-indices", "Shrinks Pointer Indices to single byte width",
                false, false)
