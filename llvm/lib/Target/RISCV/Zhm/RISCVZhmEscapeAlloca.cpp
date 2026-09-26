//===-- RISCVZhmEscapeAlloca.cpp - Move frame-escaping allocas to alc -----===//
//
// Zhm frames may only be accessed as `load/store imm12(sp)`. An alloca can
// stay in the frame only if EVERY use of its address is
//   * the pointer operand of a simple scalar load or store, possibly through
//     GEPs with all-constant indices,
//   * a lifetime marker, debug intrinsic or droppable use.
// Everything else needs the address in a register other than sp, so such
// allocas are replaced by alc/alci memory. Large objects are moved as well,
// so the frame stays within the 12-bit offset range.
//
// .d memory traps on pointer stores, so .d is only used if it is PROVEN that
// no pointer can ever be stored into the object.
//
// alc memory lives as long as a pointer to it exists, so objects allocated
// in loops (former VLAs, vararg buffers) are reclaimed once unreachable.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVZhmEscapeAlloca.h"
#include "RISCVZhmIRUtils.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include <optional>

using namespace llvm;
using namespace llvm::RISCVZhm;

#define DEBUG_TYPE "riscv-zhm-escape-alloca"

static cl::opt<unsigned> ZhmMaxFrameObject(
    "riscv-zhm-max-frame-object", cl::init(256), cl::Hidden,
    cl::desc("Allocas larger than this (bytes) are moved to alc memory"));

static cl::opt<unsigned> ZhmFrameBudget(
    "riscv-zhm-frame-budget", cl::init(1024), cl::Hidden,
    cl::desc("Bytes of allocas kept in the sp frame; the rest of the 2 KiB "
             "offset range is left for callee-saved registers and spills"));

namespace {
struct AllocaInfo {
  AllocaInst *AI;
  uint64_t Size = 0; // bytes, only meaningful if static
  bool CanStayInFrame = true;
  bool MayHoldPointers = false;
};
} // end anonymous namespace

static AllocaInfo analyzeAlloca(AllocaInst *AI, const DataLayout &DL) {
  AllocaInfo Info;
  Info.AI = AI;
  Info.MayHoldPointers = !isDataOnly(AI->getAllocatedType());

  std::optional<TypeSize> TS = AI->getAllocationSize(DL);
  if (!AI->isStaticAlloca() || !TS || TS->isScalable())
    Info.CanStayInFrame = false;
  else
    Info.Size = TS->getFixedValue();
  if (Info.Size > ZhmMaxFrameObject || AI->getAlign().value() > AllocAlign)
    Info.CanStayInFrame = false;

  auto Escape = [&] { Info.CanStayInFrame = false; };
  auto Taint = [&] { Info.MayHoldPointers = true; };

  // Walk all (transitive) users of the address. The walk continues after an
  // escape so that everything stored through derived pointers is still seen.
  SmallVector<Value *, 8> Work{AI};
  SmallPtrSet<Value *, 8> Seen{AI};
  auto Follow = [&](Instruction *I) {
    if (Seen.insert(I).second)
      Work.push_back(I);
  };

  while (!Work.empty()) {
    Value *V = Work.pop_back_val();
    for (Use &U : V->uses()) {
      auto *I = dyn_cast<Instruction>(U.getUser());
      if (!I) {
        Escape();
        Taint();
        continue;
      }

      if (auto *LI = dyn_cast<LoadInst>(I)) {
        if (LI->isAtomic() || LI->getType()->isVectorTy())
          Escape();
        continue;
      }

      if (auto *SI = dyn_cast<StoreInst>(I)) {
        Type *VT = SI->getValueOperand()->getType();
        if (U.getOperandNo() != StoreInst::getPointerOperandIndex()) {
          Escape(); // the address itself goes to memory
          Taint();
          continue;
        }
        if (!isDataOnly(VT))
          Taint();
        if (SI->isAtomic() || VT->isVectorTy())
          Escape();
        continue;
      }

      if (auto *GEP = dyn_cast<GetElementPtrInst>(I)) {
        if (!GEP->hasAllConstantIndices())
          Escape(); // needs `add rX, sp, idx`
        Follow(GEP);
        continue;
      }

      if (I->isLifetimeStartOrEnd() || isa<DbgInfoIntrinsic>(I) ||
          I->isDroppable())
        continue;

      // Everything below needs the address in a register other than sp.
      Escape();

      // Pointer-forwarding instructions (including integer arithmetic on a
      // ptrtoint, which stays a pointer in hardware): keep tracking.
      if (isa<PHINode>(I) || isa<SelectInst>(I) || isa<FreezeInst>(I) ||
          isa<CastInst>(I) || isa<BinaryOperator>(I) ||
          isIntrinsicCall(I, Intrinsic::ptrmask)) {
        Follow(I);
        continue;
      }

      // Only read the address value.
      if (isa<ICmpInst>(I) || isQueryCall(I))
        continue;

      // Writes a byte pattern, never a pointer. (Checked before CallBase.)
      if (isa<MemSetInst>(I))
        continue;

      // Source is only read; the destination may receive pointers.
      if (isa<AnyMemTransferInst>(I)) {
        if (U.getOperandNo() == 0)
          Taint();
        continue;
      }

      // A call can only be trusted if it neither writes through nor keeps
      // the pointer.
      if (auto *CB = dyn_cast<CallBase>(I)) {
        if (CB->isArgOperand(&U)) {
          unsigned ArgNo = CB->getArgOperandNo(&U);
          if (CB->onlyReadsMemory(ArgNo) && CB->doesNotCapture(ArgNo))
            continue;
        }
        Taint();
        continue;
      }

      // atomicrmw, cmpxchg, ret, ...
      Taint();
    }
  }
  return Info;
}

static Intrinsic::ID pickAllocIntrinsic(bool Imm, bool DataOnly, bool Is64) {
  if (Imm)
    return DataOnly ? Intrinsic::riscv_alci_d : Intrinsic::riscv_alci;
  if (DataOnly)
    return Is64 ? Intrinsic::riscv_alc_d_64 : Intrinsic::riscv_alc_d_32;
  return Is64 ? Intrinsic::riscv_alc_64 : Intrinsic::riscv_alc_32;
}

// Replaces AI by alc memory. Returns the allocation call, or nullptr if the
// alloca cannot be replaced.
static CallInst *replaceWithAlc(const AllocaInfo &Info, const DataLayout &DL) {
  AllocaInst *AI = Info.AI;
  Module &M = *AI->getModule();
  LLVMContext &Ctx = AI->getContext();

  TypeSize ElemTS = DL.getTypeAllocSize(AI->getAllocatedType());
  if (ElemTS.isScalable())
    return nullptr; // left for the frame lowering to diagnose

  // The base of every object is only 16-byte aligned; itd cannot reveal
  // coarser address alignment, so the pointer cannot be rounded further.
  if (AI->getAlign().value() > AllocAlign) {
    error(*AI, "stack object aligned to more than " + Twine(AllocAlign) +
                   " bytes is not supported");
    return nullptr;
  }

  IRBuilder<> B(AI);
  IntegerType *IntTy = DL.getIntPtrType(Ctx);
  Value *Bytes =
      B.CreateMul(B.CreateZExtOrTrunc(AI->getArraySize(), IntTy),
                  ConstantInt::get(IntTy, ElemTS.getFixedValue()));

  auto *CSize = dyn_cast<ConstantInt>(Bytes);
  const bool UseImm = CSize && CSize->getZExtValue() <= MaxAlciSize;
  Function *Fn = getZhmIntrinsic(
      M, pickAllocIntrinsic(UseImm, !Info.MayHoldPointers,
                            DL.getPointerSizeInBits() == 64));
  Value *Arg =
      B.CreateZExtOrTrunc(Bytes, Fn->getFunctionType()->getParamType(0));
  CallInst *Alc = B.CreateCall(Fn, {Arg});

  // Lets later passes see the alignment and that the object is fresh.
  // Assumes alc never returns null.
  Alc->addRetAttr(Attribute::NoAlias);
  Alc->addRetAttr(Attribute::NonNull);
  Alc->addRetAttr(Attribute::getWithAlignment(Ctx, Align(AllocAlign)));
  if (CSize)
    Alc->addRetAttr(
        Attribute::getWithDereferenceableBytes(Ctx, CSize->getZExtValue()));

  // Lifetime markers must refer to allocas.
  SmallVector<Instruction *, 4> Lifetimes;
  for (User *U : AI->users())
    if (auto *II = dyn_cast<IntrinsicInst>(U))
      if (II->isLifetimeStartOrEnd())
        Lifetimes.push_back(II);
  for (Instruction *I : Lifetimes)
    I->eraseFromParent();

  Alc->takeName(AI);
  AI->replaceAllUsesWith(Alc);
  AI->eraseFromParent();
  return Alc;
}

// alc returns zeroed memory. A zero memset of it is redundant if nothing can
// have written to the memory in between. Conservatively only look at the
// same block and stop at the first instruction that may write memory.
static bool removeZeroMemsetsAfter(CallInst *Alc) {
  bool Changed = false;
  for (auto It = std::next(Alc->getIterator()), E = Alc->getParent()->end();
       It != E;) {
    Instruction &I = *It++;
    if (auto *MS = dyn_cast<MemSetInst>(&I)) {
      auto *Val = dyn_cast<ConstantInt>(MS->getValue());
      if (Val && Val->isZero() && !MS->isVolatile() &&
          getUnderlyingObject(MS->getDest()) == Alc) {
        MS->eraseFromParent();
        Changed = true;
        continue; // writing zero keeps the memory zero
      }
    }
    if (isAllocCall(&I)) // other fresh allocations don't touch ours
      continue;
    if (I.mayWriteToMemory())
      break;
  }
  return Changed;
}

// stacksave/stackrestore lower to `mv rX, sp` / `mv sp, rX`, which Zhm
// forbids. Once no dynamic alloca is left they have no effect.
static bool removeStackSaveRestore(Function &F) {
  for (Instruction &I : instructions(F))
    if (auto *AI = dyn_cast<AllocaInst>(&I))
      if (!AI->isStaticAlloca())
        return false;

  SmallVector<IntrinsicInst *, 8> Saves, Restores;
  for (Instruction &I : instructions(F))
    if (auto *II = dyn_cast<IntrinsicInst>(&I)) {
      if (II->getIntrinsicID() == Intrinsic::stacksave)
        Saves.push_back(II);
      else if (II->getIntrinsicID() == Intrinsic::stackrestore)
        Restores.push_back(II);
    }
  for (IntrinsicInst *II : Restores)
    II->eraseFromParent();
  for (IntrinsicInst *II : Saves) {
    II->replaceAllUsesWith(PoisonValue::get(II->getType()));
    II->eraseFromParent();
  }
  return !Saves.empty() || !Restores.empty();
}

static bool processFunction(Function &F, const DataLayout &DL) {
  SmallVector<AllocaInfo, 16> Keep, Escape;
  for (Instruction &I : instructions(F))
    if (auto *AI = dyn_cast<AllocaInst>(&I)) {
      AllocaInfo Info = analyzeAlloca(AI, DL);
      (Info.CanStayInFrame ? Keep : Escape).push_back(Info);
    }

  // Keep the small objects in the frame until the budget is used up.
  llvm::sort(Keep, [](const AllocaInfo &A, const AllocaInfo &B) {
    return A.Size < B.Size;
  });
  uint64_t Used = 0;
  for (const AllocaInfo &Info : Keep) {
    uint64_t Slot = alignTo(Info.Size, Info.AI->getAlign());
    if (Used + Slot <= ZhmFrameBudget)
      Used += Slot;
    else
      Escape.push_back(Info);
  }

  bool Changed = false;
  SmallVector<CallInst *, 8> Allocs;
  for (const AllocaInfo &Info : Escape) {
    LLVM_DEBUG(dbgs() << "Zhm: moving to alc"
                      << (Info.MayHoldPointers ? "" : ".d") << " memory in "
                      << F.getName() << ": " << *Info.AI << "\n");
    if (CallInst *Alc = replaceWithAlc(Info, DL)) {
      Allocs.push_back(Alc);
      Changed = true;
    }
  }
  for (CallInst *Alc : Allocs)
    Changed |= removeZeroMemsetsAfter(Alc);
  Changed |= removeStackSaveRestore(F);
  return Changed;
}

bool llvm::runRISCVZhmEscapeAlloca(Module &M, const TargetMachine *TM) {
  bool Changed = false;
  for (Function &F : M)
    if (!F.isDeclaration() && isZhmFunction(TM, F))
      Changed |= processFunction(F, M.getDataLayout());
  return Changed;
}

PreservedAnalyses RISCVZhmEscapeAlloca::run(Module &M,
                                            ModuleAnalysisManager &) {
  if (!runRISCVZhmEscapeAlloca(M, TM))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

namespace {
class RISCVZhmEscapeAllocaLegacy : public ModulePass {
public:
  static char ID;
  RISCVZhmEscapeAllocaLegacy() : ModulePass(ID) {}
  StringRef getPassName() const override {
    return "RISC-V Zhm move escaping allocas to alc memory";
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetPassConfig>();
    AU.setPreservesCFG();
  }
  bool runOnModule(Module &M) override {
    auto &TM = getAnalysis<TargetPassConfig>().getTM<TargetMachine>();
    return runRISCVZhmEscapeAlloca(M, &TM);
  }
};
} // end anonymous namespace

char RISCVZhmEscapeAllocaLegacy::ID = 0;
INITIALIZE_PASS_BEGIN(RISCVZhmEscapeAllocaLegacy, DEBUG_TYPE,
                      "RISC-V Zhm move escaping allocas to alc memory", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(TargetPassConfig)
INITIALIZE_PASS_END(RISCVZhmEscapeAllocaLegacy, DEBUG_TYPE,
                    "RISC-V Zhm move escaping allocas to alc memory", false,
                    false)

ModulePass *llvm::createRISCVZhmEscapeAllocaLegacyPass() {
  return new RISCVZhmEscapeAllocaLegacy();
}