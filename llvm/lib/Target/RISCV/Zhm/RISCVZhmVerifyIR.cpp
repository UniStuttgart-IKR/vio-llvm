//===-- RISCVZhmVerifyIR.cpp - Report IR that is illegal on Zhm -----------===//
//
// Last IR pass before instruction selection. Changes nothing; reports every
// construct that would trap on Zhm and that RISCVZhmLegalizeIR could not
// rewrite, as a regular compile error with source location:
//
//   * arithmetic on two pointers other than p - q, and p - q where the two
//     pointers provably point into different objects,
//   * any operation on a pointer other than +/- an integer (and, or, xor,
//     mul, div, rem, shifts, int - p, truncation, extension); the offset is
//     available through llvm.riscv.itd,
//   * ordered comparisons between a pointer and an integer or null,
//   * llvm.ptrmask that masks more than the 16-byte object alignment,
//   * integer constants dereferenced as pointers outside supervisor code
//     (sentinels like (void *)-1 that are only compared are fine), and
//     integer constants used as pointers in global initializers,
//   * memcpy/memmove that may copy pointers but was not legalized.
//
// The hardware only sees operations, not IR types, so "pointer" here means
// "pointer-typed or derived from ptrtoint" (see isPointerDerived).
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVZhmVerifyIR.h"
#include "RISCVZhmIRUtils.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;
using namespace llvm::RISCVZhm;

#define DEBUG_TYPE "riscv-zhm-verify-ir"

// Static data cannot be initialised with a fabricated pointer: there is no
// dtp at load time.
static bool hasFabricatedPointer(const Constant *C,
                                 SmallPtrSetImpl<const Constant *> &Seen) {
  if (!Seen.insert(C).second || isa<GlobalValue>(C))
    return false;
  if (isFabricatedPointer(C, /*OnlyConstants=*/true))
    return true;
  for (const Use &Op : C->operands())
    if (const auto *OC = dyn_cast<Constant>(Op.get()))
      if (hasFabricatedPointer(OC, Seen))
        return true;
  return false;
}

// The pointer an integer was derived from via ptrtoint and +/- integer.
static const Value *pointerRoot(const Value *V) {
  for (unsigned Depth = 0; Depth < 8; ++Depth) {
    if (V->getType()->isPointerTy())
      return V;
    const auto *Op = dyn_cast<Operator>(V);
    if (!Op)
      return nullptr;
    const unsigned Opc = Op->getOpcode();
    if (Opc == Instruction::PtrToInt) {
      V = Op->getOperand(0);
    } else if ((Opc == Instruction::Add || Opc == Instruction::Sub) &&
               isPointerDerived(Op->getOperand(0)) &&
               !isPointerDerived(Op->getOperand(1))) {
      V = Op->getOperand(0);
    } else if (Opc == Instruction::Add &&
               isPointerDerived(Op->getOperand(1))) {
      V = Op->getOperand(1);
    } else {
      return nullptr;
    }
  }
  return nullptr;
}

static bool provablyDifferentObjects(const Value *A, const Value *B) {
  const Value *RA = pointerRoot(A), *RB = pointerRoot(B);
  if (!RA || !RB)
    return false;
  const Value *OA = getUnderlyingObject(RA), *OB = getUnderlyingObject(RB);
  return OA != OB && isIdentifiedObject(OA) && isIdentifiedObject(OB);
}

static void verifyInstruction(Instruction &I, bool Supervisor) {
  if (auto *MT = dyn_cast<MemTransferInst>(&I)) {
    if (!isKnownDataOnlyMemory(MT->getRawSource()) &&
        !isKnownDataOnlyMemory(MT->getRawDest()))
      error(I, isa<MemCpyInst>(MT) && (cast<MemCpyInst>(MT)->getIntrinsicID() == Intrinsic::memcpy_inline)
                   ? "memcpy.inline that may copy pointers needs word-aligned "
                     "operands and a small constant size"
                   : "memcpy/memmove that may copy pointers was not legalized");
    return;
  }

  if (isIntrinsicCall(&I, Intrinsic::ptrmask)) {
    error(I, "masking a pointer beyond the " + Twine(AllocAlign) +
                 "-byte object alignment");
    return;
  }

  if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
    const Value *A = BO->getOperand(0), *B = BO->getOperand(1);
    const bool PA = isPointerDerived(A), PB = isPointerDerived(B);
    if (!PA && !PB)
      return;
    const unsigned Opc = BO->getOpcode();
    if (PA && PB) {
      if (Opc != Instruction::Sub)
        error(I, "arithmetic on two pointers; only their difference is "
                 "defined");
      else if (provablyDifferentObjects(A, B))
        error(I, "difference of pointers into different objects");
      return;
    }
    if (Opc == Instruction::Add || (Opc == Instruction::Sub && PA))
      return; // pointer +/- integer
    error(I, "integer operation on a pointer; use llvm.riscv.itd for its "
             "offset within the object");
    return;
  }

  if ((isa<TruncInst>(I) || isa<ZExtInst>(I) || isa<SExtInst>(I)) &&
      isPointerDerived(I.getOperand(0))) {
    error(I, "truncating or extending a pointer");
    return;
  }

  if (auto *IC = dyn_cast<ICmpInst>(&I)) {
    if (!IC->isRelational())
      return;
    auto IsInt = [](const Value *V) {
      return isa<ConstantPointerNull>(V) || isa<ConstantInt>(V);
    };
    const Value *A = IC->getOperand(0), *B = IC->getOperand(1);
    if ((isPointerDerived(A) && IsInt(B)) || (IsInt(A) && isPointerDerived(B)))
      error(I, "ordered comparison between a pointer and an integer");
    return;
  }

  // Dereferencing an integer constant as a pointer. Supervisor code had its
  // fabricated pointers turned into dtp calls by the legalizer.
  if (Supervisor)
    return;
  for (const Use &U : I.operands())
    if (isAddressUse(U) && isFabricatedPointer(U.get(), /*OnlyConstants=*/true)) {
      error(I, "integer constant dereferenced as a pointer; only supervisor "
               "code may create pointers (llvm.riscv.dtp)");
      return;
    }
}

bool llvm::runRISCVZhmVerifyIR(Module &M, const TargetMachine *TM) {
  bool AnyZhm = false;
  for (Function &F : M) {
    if (F.isDeclaration() || !isZhmFunction(TM, F))
      continue;
    AnyZhm = true;
    const bool Supervisor = isZhmSupervisorFunction(F);
    for (Instruction &I : instructions(F))
      if (!I.isDebugOrPseudoInst())
        verifyInstruction(I, Supervisor);
  }

  if (!AnyZhm)
    return false;
  for (GlobalVariable &GV : M.globals()) {
    SmallPtrSet<const Constant *, 8> Seen;
    if (GV.hasInitializer() && hasFabricatedPointer(GV.getInitializer(), Seen))
      M.getContext().emitError("Zhm: initializer of '" + GV.getName() +
                               "' uses an integer constant as a pointer; "
                               "create it at run time with llvm.riscv.dtp");
  }

  return false;
}

PreservedAnalyses RISCVZhmVerifyIR::run(Module &M, ModuleAnalysisManager &) {
  runRISCVZhmVerifyIR(M, TM);
  return PreservedAnalyses::all();
}

namespace {
class RISCVZhmVerifyIRLegacy : public ModulePass {
public:
  static char ID;
  RISCVZhmVerifyIRLegacy() : ModulePass(ID) {}
  StringRef getPassName() const override { return "RISC-V Zhm verify IR"; }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetPassConfig>();
    AU.setPreservesAll();
  }
  bool runOnModule(Module &M) override {
    auto &TM = getAnalysis<TargetPassConfig>().getTM<TargetMachine>();
    runRISCVZhmVerifyIR(M, &TM);
    return false;
  }
};
} // end anonymous namespace

char RISCVZhmVerifyIRLegacy::ID = 0;
INITIALIZE_PASS_BEGIN(RISCVZhmVerifyIRLegacy, DEBUG_TYPE,
                      "RISC-V Zhm verify IR", false, true)
INITIALIZE_PASS_DEPENDENCY(TargetPassConfig)
INITIALIZE_PASS_END(RISCVZhmVerifyIRLegacy, DEBUG_TYPE, "RISC-V Zhm verify IR",
                    false, true)

ModulePass *llvm::createRISCVZhmVerifyIRLegacyPass() {
  return new RISCVZhmVerifyIRLegacy();
}