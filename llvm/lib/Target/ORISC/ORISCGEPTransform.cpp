//===----- RISCVCodeGenPrepare.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is a RISC-V specific version of CodeGenPrepare.
// It munges the code in the input function to better prepare it for
// SelectionDAG-based code generation. This works around limitations in it's
// basic-block-at-a-time approach.
//
//===----------------------------------------------------------------------===//

#include "ORISC.h"
#include "ORISCTargetMachine.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <cstdint>

using namespace llvm;

#define DEBUG_TYPE "orisc-geptransform"
#define PASS_NAME "ORISC GEP Transform"

namespace {

class ORISCGEPTransform : public FunctionPass,
                            public InstVisitor<ORISCGEPTransform, bool> {
  const DataLayout *DL;
  const DominatorTree *DT;
  const ORISCSubtarget *ST;

public:
  static char ID;

  ORISCGEPTransform() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override { return PASS_NAME; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<TargetPassConfig>();
  }

  bool visitInstruction(Instruction &I) { return false; }
  bool visitGetElementPtrInst(GetElementPtrInst &I);
  //bool visitAllocaInst(GetElementPtrInst &I);

private:

};

} // end anonymous namespace

bool ORISCGEPTransform::visitGetElementPtrInst(GetElementPtrInst &GEPI) {
  auto Ty = GEPI.getSourceElementType();
  LLVM_DEBUG(Ty->dump());
  return false;
}

bool ORISCGEPTransform::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  auto &TPC = getAnalysis<TargetPassConfig>();
  auto &TM = TPC.getTM<ORISCTargetMachine>();
  ST = &TM.getSubtarget<ORISCSubtarget>(F);

  DL = &F.getDataLayout();
  DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();

  bool MadeChange = false;
  for (unsigned i = 0; i < F.arg_size(); i++){
    LLVM_DEBUG(dbgs() << F.getArg(i)->getName() << " is: ");
    if (F.getArg(i)->hasByValAttr())
      LLVM_DEBUG(F.getArg(i)->getParamByValType()->dump());
    else if (F.getArg(i)->hasByRefAttr())
      LLVM_DEBUG(F.getArg(i)->getParamByRefType()->dump());
  }

  for (auto &BB : F)
    for (Instruction &I : llvm::make_early_inc_range(BB))
      MadeChange |= visit(I);

  return MadeChange;
}

INITIALIZE_PASS(ORISCGEPTransform, DEBUG_TYPE, PASS_NAME, false, false)

char ORISCGEPTransform::ID = 0;

FunctionPass *llvm::createORISCGEPTransformPass() {
  return new ORISCGEPTransform();
}
