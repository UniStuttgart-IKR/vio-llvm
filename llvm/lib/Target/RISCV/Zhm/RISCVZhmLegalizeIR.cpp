//===-- RISCVZhmLegalizeIR.cpp - Rewrite pointer operations for Zhm -------===//
//
// Rewrites IR that is valid LLVM but illegal on Zhm into legal forms. Runs
// after RISCVZhmEscapeAlloca, before RISCVZhmVerifyIR.
//
// 1. Pointer bits. Objects are 16-byte aligned, so the low 4 address bits of
//    a pointer equal the low 4 bits of its index within the object:
//      (uintptr_t)p & C          -> itd(p) & C          (C in {1,3,7,15})
//      (uintptr_t)p % 2^k        -> itd(p) & (2^k - 1)  (2^k <= 16)
//      (uintptr_t)p & ~C         -> p - (itd(p) & C)    (align down)
//      llvm.ptrmask(p, ~C)       -> p - (itd(p) & C)
//      inttoptr(ptrtoint p +/- n)-> gep p, +/-n
//      inttoptr(ptrtoint p)      -> p
//    This covers __builtin_is_aligned / __builtin_align_{up,down} and the
//    usual hand-written alignment idioms.
//
//    In supervisor functions (attribute "riscv-zhm-supervisor" or
//    -riscv-zhm-supervisor), integers that are dereferenced as pointers
//    (MMIO addresses, physical addresses, ...) are turned into pointers with
//    dtp. Elsewhere the verifier reports them.
//
// 2. Copies. A sub-XLEN load of a pointer slot traps, so every memcpy /
//    memmove that may copy pointers is rewritten:
//      provably pointer-free source or destination -> unchanged
//      word-aligned, small constant length          -> inline word copy
//      otherwise                                     -> __zhm_memcpy /
//                                                       __zhm_memmove
//    The helpers live in compiler-rt and choose word or byte copies at run
//    time via itd. The calls are nobuiltin, so SelectionDAG never expands
//    them bytewise.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVZhmLegalizeIR.h"
#include "RISCVZhmIRUtils.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/ReplaceConstant.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Utils/Local.h"
#include <functional>
#include <optional>

using namespace llvm;
using namespace llvm::RISCVZhm;

#define DEBUG_TYPE "riscv-zhm-legalize-ir"

static cl::opt<bool> ZhmSupervisor(
    "riscv-zhm-supervisor", cl::init(false), cl::Hidden,
    cl::desc("Treat all functions as Zhm supervisor code, which may turn "
             "integers into pointers with llvm.riscv.dtp"));

bool llvm::RISCVZhm::isZhmSupervisorFunction(const Function &F) {
  return ZhmSupervisor || F.hasFnAttribute("riscv-zhm-supervisor");
}

static cl::opt<unsigned> ZhmInlineCopyWords(
    "riscv-zhm-inline-copy-words", cl::init(8), cl::Hidden,
    cl::desc("Word-aligned memcpy/memmove of up to this many XLEN words are "
             "expanded inline"));

//===----------------------------------------------------------------------===//
// Pointer bits
//===----------------------------------------------------------------------===//

namespace {
class PointerBitsRewriter {
  Module &M;
  IntegerType *IntTy;
  Function *Itd = nullptr, *Dtp = nullptr;
  bool Supervisor;

public:
  PointerBitsRewriter(Function &F)
      : M(*F.getParent()),
        IntTy(M.getDataLayout().getIntPtrType(M.getContext())),
        Supervisor(isZhmSupervisorFunction(F)) {}

  // itd(P): index of P within its object.
  Value *index(IRBuilder<> &B, Value *P) {
    if (!Itd)
      Itd = getZhmIntrinsic(M, itdID(M));
    return B.CreateCall(Itd, {P});
  }

  // Integer -> pointer, unchecked.
  // ASSUMPTION: dtp(Base, Index) builds the pointer Base + Index, so an
  // absolute address is passed as base with index 0. Adjust here if the
  // operands mean something else.
  Value *dataToPointer(IRBuilder<> &B, Value *Addr) {
    if (!Dtp)
      Dtp = getZhmIntrinsic(M, dtpID(M));
    return B.CreateCall(Dtp, {B.CreateZExtOrTrunc(Addr, IntTy),
                              ConstantInt::get(IntTy, 0)});
  }

  // P aligned down to C+1 bytes: P - (itd(P) & C).
  Value *alignDown(IRBuilder<> &B, Value *P, uint64_t C) {
    Value *Mis = B.CreateAnd(B.CreateZExtOrTrunc(index(B, P), IntTy),
                             ConstantInt::get(IntTy, C));
    return B.CreateGEP(B.getInt8Ty(), P, B.CreateNeg(Mis));
  }

  // If C is ~LowMask in V's width, returns LowMask.
  static std::optional<uint64_t> invertedLowMask(const ConstantInt *C) {
    APInt Inv = ~C->getValue();
    if (Inv.getActiveBits() <= 64 && isLowBitMask(Inv.getZExtValue()))
      return Inv.getZExtValue();
    return std::nullopt;
  }

  // Rebuilds an integer derived from exactly one pointer as a pointer
  // computation, emitting new instructions at B. nullptr if not possible.
  Value *rebuildPointer(IRBuilder<> &B, Value *V, unsigned Depth = 0) {
    if (Depth > 8 || V->getType() != IntTy)
      return nullptr;
    if (auto *PTI = dyn_cast<PtrToIntInst>(V))
      return PTI->getPointerOperand();
    auto *BO = dyn_cast<BinaryOperator>(V);
    if (!BO)
      return nullptr;
    Value *A = BO->getOperand(0), *N = BO->getOperand(1);

    switch (BO->getOpcode()) {
    case Instruction::Add:
      if (isPointerDerived(N))
        std::swap(A, N);
      if (isPointerDerived(N))
        return nullptr; // p + q
      if (Value *P = rebuildPointer(B, A, Depth + 1))
        return B.CreateGEP(B.getInt8Ty(), P, N);
      return nullptr;
    case Instruction::Sub:
      if (isPointerDerived(N))
        return nullptr; // p - q is an integer, n - p is illegal
      if (Value *P = rebuildPointer(B, A, Depth + 1))
        return B.CreateGEP(B.getInt8Ty(), P, B.CreateNeg(N));
      return nullptr;
    case Instruction::And: {
      if (isa<ConstantInt>(A))
        std::swap(A, N);
      auto *C = dyn_cast<ConstantInt>(N);
      std::optional<uint64_t> Low = C ? invertedLowMask(C) : std::nullopt;
      if (!Low)
        return nullptr;
      if (Value *P = rebuildPointer(B, A, Depth + 1))
        return alignDown(B, P, *Low);
      return nullptr;
    }
    default:
      return nullptr;
    }
  }

  // Returns the replacement for I, or nullptr.
  Value *rewrite(Instruction &I) {
    IRBuilder<> B(&I);

    if (auto *ITP = dyn_cast<IntToPtrInst>(&I)) {
      if (Value *P = rebuildPointer(B, ITP->getOperand(0)))
        return P->getType() == ITP->getType() ? P : nullptr;
      // An integer that is dereferenced as a pointer.
      if (Supervisor && ITP->getType() == B.getPtrTy() &&
          isFabricatedPointer(ITP, /*OnlyConstants=*/false) &&
          isDereferenced(ITP))
        return dataToPointer(B, ITP->getOperand(0));
      return nullptr;
    }

    if (auto *II = dyn_cast<IntrinsicInst>(&I))
      if (II->getIntrinsicID() == Intrinsic::ptrmask) {
        auto *C = dyn_cast<ConstantInt>(II->getArgOperand(1));
        if (C && C->isMinusOne())
          return II->getArgOperand(0);
        if (std::optional<uint64_t> Low = C ? invertedLowMask(C) : std::nullopt)
          return alignDown(B, II->getArgOperand(0), *Low);
        return nullptr;
      }

    auto *BO = dyn_cast<BinaryOperator>(&I);
    if (!BO || BO->getType() != IntTy)
      return nullptr;
    Value *A = BO->getOperand(0);
    auto *C = dyn_cast<ConstantInt>(BO->getOperand(1));
    if (!C || !isPointerDerived(A))
      return nullptr;

    uint64_t Low = 0;
    if (BO->getOpcode() == Instruction::And) {
      // Align down, result used as integer: keep it a pointer-derived value.
      if (std::optional<uint64_t> Inv = invertedLowMask(C)) {
        if (Value *P = rebuildPointer(B, A))
          return B.CreatePtrToInt(alignDown(B, P, *Inv), IntTy);
        return nullptr;
      }
      Low = C->getZExtValue();
    } else if (BO->getOpcode() == Instruction::URem && C->getValue().isPowerOf2()) {
      Low = C->getZExtValue() - 1;
    }
    if (!isLowBitMask(Low))
      return nullptr;

    // Low address bits as a plain integer.
    if (Value *P = rebuildPointer(B, A))
      return B.CreateAnd(B.CreateZExtOrTrunc(index(B, P), IntTy),
                         ConstantInt::get(IntTy, Low));
    return nullptr;
  }
};
} // end anonymous namespace

// Supervisor code: `*(volatile T *)0x1000` is a constant expression operand,
// not an instruction. Expand inttoptr constant expressions into
// instructions so they can be turned into dtp calls.
static bool expandFabricatedConstantExprs(Function &F) {
  SmallVector<Constant *, 8> CEs;
  SmallPtrSet<const Constant *, 16> Seen;
  std::function<void(Constant *)> Collect = [&](Constant *C) {
    if (!Seen.insert(C).second || isa<GlobalValue>(C))
      return;
    if (auto *CE = dyn_cast<ConstantExpr>(C))
      if (CE->getOpcode() == Instruction::IntToPtr &&
          !isPointerDerived(CE->getOperand(0)) && !CE->isNullValue())
        CEs.push_back(CE);
    for (Use &Op : C->operands())
      if (auto *OC = dyn_cast<Constant>(Op.get()))
        Collect(OC);
  };
  for (Instruction &I : instructions(F))
    for (Use &Op : I.operands())
      if (auto *C = dyn_cast<Constant>(Op.get()))
        Collect(C);
  if (CEs.empty())
    return false;
  // (Signature as of LLVM 19+; older versions lack RestrictToFunc.)
  return convertUsersOfConstantsToInstructions(CEs, &F,
                                               /*RemoveDeadConstants=*/true,
                                               /*IncludeSelf=*/true);
}

static bool legalizePointerBits(Function &F) {
  bool Changed = false;
  if (isZhmSupervisorFunction(F))
    Changed |= expandFabricatedConstantExprs(F);

  SmallVector<Instruction *, 16> Candidates;
  for (Instruction &I : instructions(F))
    if (isa<IntToPtrInst>(I) || isa<BinaryOperator>(I) ||
        isIntrinsicCall(&I, Intrinsic::ptrmask))
      Candidates.push_back(&I);

  PointerBitsRewriter RW(F);
  SmallVector<WeakTrackingVH, 16> MaybeDead;
  // In program order: an inner align-down is rewritten before the inttoptr
  // that consumes it, which then folds via ptrtoint(gep ...).
  for (Instruction *I : Candidates) {
    Value *New = RW.rewrite(*I);
    if (!New)
      continue;
    New->takeName(I);
    I->replaceAllUsesWith(New);
    MaybeDead.push_back(I);
    Changed = true;
  }
  for (WeakTrackingVH &VH : MaybeDead)
    if (Value *V = VH)
      RecursivelyDeleteTriviallyDeadInstructions(V);
  return Changed;
}

//===----------------------------------------------------------------------===//
// memcpy / memmove
//===----------------------------------------------------------------------===//

// Whole words with XLEN loads/stores, then the tail with naturally aligned
// narrower accesses. A tail can only be data: a pointer always fills a whole
// aligned word. All loads precede all stores, so this is also correct for an
// overlapping memmove.
static void expandAlignedCopy(MemTransferInst *MT, uint64_t Len, Align A,
                              uint64_t XLen) {
  IRBuilder<> B(MT);
  const bool Vol = MT->isVolatile();
  struct Part {
    uint64_t Off;
    Value *V;
  };
  SmallVector<Part, 16> Parts;
  for (uint64_t Off = 0; Off < Len;) {
    uint64_t Size = XLen;
    while (Size > Len - Off)
      Size /= 2;
    Type *Ty = B.getIntNTy(Size * 8);
    Value *Src =
        B.CreateConstInBoundsGEP1_64(B.getInt8Ty(), MT->getRawSource(), Off);
    Parts.push_back(
        {Off, B.CreateAlignedLoad(Ty, Src, commonAlignment(A, Off), Vol)});
    Off += Size;
  }
  for (const Part &P : Parts) {
    Value *Dst =
        B.CreateConstInBoundsGEP1_64(B.getInt8Ty(), MT->getRawDest(), P.Off);
    B.CreateAlignedStore(P.V, Dst, commonAlignment(A, P.Off), Vol);
  }
  MT->eraseFromParent();
}

static bool legalizeMemTransfers(Function &F) {
  SmallVector<MemTransferInst *, 8> Worklist;
  for (Instruction &I : instructions(F))
    if (auto *MT = dyn_cast<MemTransferInst>(&I))
      Worklist.push_back(MT);

  Module &M = *F.getParent();
  const DataLayout &DL = M.getDataLayout();
  LLVMContext &Ctx = F.getContext();
  const uint64_t XLen = DL.getPointerSize();
  IntegerType *IntTy = DL.getIntPtrType(Ctx);
  PointerType *PtrTy = PointerType::getUnqual(Ctx);

  bool Changed = false;
  for (MemTransferInst *MT : Worklist) {
    if (isKnownDataOnlyMemory(MT->getRawSource()) ||
        isKnownDataOnlyMemory(MT->getRawDest()))
      continue; // any access width is fine

    const Align DstA = std::max(MT->getDestAlign().valueOrOne(),
                                getKnownAlignment(MT->getRawDest(), DL, MT));
    const Align SrcA = std::max(MT->getSourceAlign().valueOrOne(),
                                getKnownAlignment(MT->getRawSource(), DL, MT));
    const Align A = std::min(DstA, SrcA);
    const auto *CLen = dyn_cast<ConstantInt>(MT->getLength());

    if (A.value() >= XLen && CLen &&
        CLen->getZExtValue() <= uint64_t(ZhmInlineCopyWords) * XLen) {
      expandAlignedCopy(MT, CLen->getZExtValue(), A, XLen);
      Changed = true;
      continue;
    }
    if (isa<AnyMemCpyInst>(MT) && cast<AnyMemCpyInst>(MT)->getIntrinsicID() != Intrinsic::memcpy_inline)
      continue; // must not become a call; the verifier reports it

    StringRef Name = isa<MemMoveInst>(MT) ? "__zhm_memmove" : "__zhm_memcpy";
    FunctionCallee Fn = M.getOrInsertFunction(
        Name, FunctionType::get(PtrTy, {PtrTy, PtrTy, IntTy}, false));
    IRBuilder<> B(MT);
    CallInst *CI =
        B.CreateCall(Fn, {MT->getRawDest(), MT->getRawSource(),
                          B.CreateZExtOrTrunc(MT->getLength(), IntTy)});
    CI->addFnAttr(Attribute::NoBuiltin);
    MT->eraseFromParent();
    Changed = true;
  }
  return Changed;
}

//===----------------------------------------------------------------------===//
// Driver
//===----------------------------------------------------------------------===//

bool llvm::runRISCVZhmLegalizeIR(Module &M, const TargetMachine *TM) {
  bool Changed = false;
  for (Function &F : M) {
    if (F.isDeclaration() || !isZhmFunction(TM, F))
      continue;
    Changed |= legalizePointerBits(F);
    Changed |= legalizeMemTransfers(F);
  }
  return Changed;
}

PreservedAnalyses RISCVZhmLegalizeIR::run(Module &M, ModuleAnalysisManager &) {
  if (!runRISCVZhmLegalizeIR(M, TM))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

namespace {
class RISCVZhmLegalizeIRLegacy : public ModulePass {
public:
  static char ID;
  RISCVZhmLegalizeIRLegacy() : ModulePass(ID) {}
  StringRef getPassName() const override {
    return "RISC-V Zhm legalize pointer operations";
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetPassConfig>();
    AU.setPreservesCFG();
  }
  bool runOnModule(Module &M) override {
    auto &TM = getAnalysis<TargetPassConfig>().getTM<TargetMachine>();
    return runRISCVZhmLegalizeIR(M, &TM);
  }
};
} // end anonymous namespace

char RISCVZhmLegalizeIRLegacy::ID = 0;
INITIALIZE_PASS_BEGIN(RISCVZhmLegalizeIRLegacy, DEBUG_TYPE,
                      "RISC-V Zhm legalize pointer operations", false, false)
INITIALIZE_PASS_DEPENDENCY(TargetPassConfig)
INITIALIZE_PASS_END(RISCVZhmLegalizeIRLegacy, DEBUG_TYPE,
                    "RISC-V Zhm legalize pointer operations", false, false)

ModulePass *llvm::createRISCVZhmLegalizeIRLegacyPass() {
  return new RISCVZhmLegalizeIRLegacy();
}