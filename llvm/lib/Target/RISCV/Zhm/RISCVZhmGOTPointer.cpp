//===-- RISCVZhmGOTPointer.cpp - Load and clear the GOT pointer in gp -----===//
//
// Zhm, spec V0.3.2: the hardware never writes gp. Code loads the GOT pointer
// from the GOT slot (index 0) of the executable object it runs in:
//
//   1:  auipc   gp, %pcrel_hi(__got_slot)        (lld defines __got_slot
//       l[wd]   gp, %pcrel_lo(1b)(gp)             XLEN/8 bytes in front of
//                                                 the code segment)
//
// ISel emits every GOT access, addresses as well as GOT calls, as
// `lga rd, sym` (PseudoZhmLGA: lui/add/l[wd] relative to gp, relaxed by the
// linker to one load) or, with -riscv-zhm-short-got, as
// `l[wd] rd, %got_off(sym)(gp)` (operand flag MO_GOT_OFF). gp is reserved, so
// it is the only register that holds the GOT pointer for longer than one
// access (in lga, rd holds a GOT reference only until the load overwrites
// it). After register allocation, this pass
//
// 1. loads gp in front of every GOT access where gp is not certainly valid.
//    gp is not valid at function entry, and not after a call or inline
//    assembly (the callee may clear or replace it). If the access sits in a
//    loop without calls, the load goes into the preheader of the outermost
//    such loop instead.
//
// 2. clears gp (`li gp, 0`) where the GOT pointer may not stay in a register,
//    as selected by -riscv-zhm-got-policy:
//      standard      what the spec requires: before every call that may
//                    leave this executable object (calls through a register,
//                    i.e. GOT calls and function pointers, and direct calls
//                    to callees that are not dso_local), and before the
//                    returns and tail calls of a function that may be called
//                    from another object
//      conservative  before every call and every return
//      liberal       never
//    It only clears where gp may hold a GOT pointer: after a load of gp, at
//    function entry and after calls that stay in this object (a caller or
//    callee in this object may have left it there), and after inline
//    assembly.
//
// Clearing at a return costs one instruction: the caller reloads gp after
// every call anyway, since the callee may have cleared or replaced it.
// Checking the return target with bso would cost more (a pointer into the
// own code object, then the branch). A function is only called from this
// object, and needs no clearing at its returns, if it has local linkage, its
// address is not taken and it is never tail-called (a tail-called function
// returns to its caller's caller).
//
// Runs in addPreEmitPass after copy propagation and before branch
// relaxation, since it inserts instructions.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/RISCVBaseInfo.h"
#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-zhm-got-pointer"

namespace {
enum class GOTPolicy { Standard, Conservative, Liberal };
} // namespace

static cl::opt<GOTPolicy> ZhmGOTPolicy(
    "riscv-zhm-got-policy", cl::init(GOTPolicy::Standard), cl::Hidden,
    cl::desc("Zhm: where gp, which holds the GOT pointer, is cleared"),
    cl::values(
        clEnumValN(GOTPolicy::Standard, "standard",
                   "before calls that may leave the executable object"),
        clEnumValN(GOTPolicy::Conservative, "conservative",
                   "before every call and every return"),
        clEnumValN(GOTPolicy::Liberal, "liberal", "never")));

static constexpr const char *GOTSlotSymbol = "__got_slot";

namespace {

class RISCVZhmGOTPointer : public MachineFunctionPass {
public:
  static char ID;
  RISCVZhmGOTPointer() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "RISC-V Zhm GOT pointer";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.addPreserved<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  const RISCVInstrInfo *TII = nullptr;
  MachineLoopInfo *MLI = nullptr;
  bool IsRV64 = false;

  bool insertGPLoads(MachineFunction &MF);
  bool insertGPClears(MachineFunction &MF);
  void buildGPLoad(MachineBasicBlock &MBB, MachineBasicBlock::iterator At,
                   const DebugLoc &DL);
  MachineBasicBlock *hoistTarget(MachineBasicBlock &MBB);
  bool loopKillsGP(const MachineLoop &L);
};

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Instruction classes
//===----------------------------------------------------------------------===//

static bool isLoadWord(const MachineInstr &MI) {
  return MI.getOpcode() == RISCV::LW || MI.getOpcode() == RISCV::LD;
}

// lga rd, sym  or  l[wd] rd, %got_off(sym)(gp)
static bool isGOTAccess(const MachineInstr &MI) {
  if (MI.getOpcode() == RISCV::PseudoZhmLGA)
    return true;
  if (!isLoadWord(MI) || MI.getNumOperands() < 3)
    return false;
  const MachineOperand &Base = MI.getOperand(1);
  return Base.isReg() && Base.getReg() == RISCV::X3 &&
         MI.getOperand(2).getTargetFlags() == RISCVII::MO_GOT_OFF;
}

// auipc gp, %pcrel_hi(__got_slot): first half of a gp load.
static bool isGPAuipc(const MachineInstr &MI) {
  if (MI.getOpcode() != RISCV::AUIPC || MI.getOperand(0).getReg() != RISCV::X3)
    return false;
  const MachineOperand &Sym = MI.getOperand(1);
  return Sym.isSymbol() && StringRef(Sym.getSymbolName()) == GOTSlotSymbol;
}

// l[wd] gp, %pcrel_lo(1b)(gp): second half; afterwards gp is valid.
static bool isGPLoad(const MachineInstr &MI) {
  if (!isLoadWord(MI) || MI.getOperand(0).getReg() != RISCV::X3)
    return false;
  const MachineOperand &Base = MI.getOperand(1), &Lo = MI.getOperand(2);
  return Base.isReg() && Base.getReg() == RISCV::X3 && Lo.isMCSymbol() &&
         Lo.getTargetFlags() == RISCVII::MO_PCREL_LO;
}

// Anything after which gp may no longer hold the GOT pointer.
static bool killsGP(const MachineInstr &MI) {
  if (MI.isCall() || MI.isInlineAsm())
    return true;
  return MI.modifiesRegister(RISCV::X3, /*TRI=*/nullptr) && !isGPLoad(MI);
}

// May this call leave the executable object? Must agree with LowerCall
// (zhmCalleeMayLeaveImage): it turns such direct calls into GOT calls, so
// direct calls that remain are local.
static bool callMayLeave(const MachineInstr &MI) {
  if (ZhmGOTPolicy == GOTPolicy::Conservative)
    return true;
  for (const MachineOperand &MO : MI.operands()) {
    if (MO.isGlobal())
      return !MO.getGlobal()->isDSOLocal();
    if (MO.isSymbol())
      // compiler-rt is linked statically into every image; its functions
      // all start with "__" (spec V0.3.2, runtime functions).
      return !StringRef(MO.getSymbolName()).starts_with("__");
    if (MO.isMCSymbol())
      return true;
  }
  return true; // through a register
}

// May F be called from another executable object, i.e. may its returns go
// there? Not if every use of F is a plain (non-tail) direct call in this
// module and F cannot be reached by name from outside.
static bool mayBeCalledFromOutside(const Function &F) {
  if (!F.hasLocalLinkage())
    return true;
  for (const User *U : F.users()) {
    const auto *CB = dyn_cast<CallBase>(U);
    if (!CB || CB->getCalledOperand() != &F || CB->isTailCall() ||
        CB->isMustTailCall())
      return true; // address taken, or tail-called
  }
  return false;
}

//===----------------------------------------------------------------------===//
// Loading gp
//===----------------------------------------------------------------------===//

void RISCVZhmGOTPointer::buildGPLoad(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator At,
                                     const DebugLoc &DL) {
  MachineFunction &MF = *MBB.getParent();
  MCSymbol *Label = MF.getContext().createNamedTempSymbol("zhm_got_slot");
  MachineInstr *Auipc = BuildMI(MBB, At, DL, TII->get(RISCV::AUIPC), RISCV::X3)
                            .addExternalSymbol(GOTSlotSymbol,
                                               RISCVII::MO_PCREL_HI);
  Auipc->setPreInstrSymbol(MF, Label);
  BuildMI(MBB, At, DL, TII->get(IsRV64 ? RISCV::LD : RISCV::LW), RISCV::X3)
      .addReg(RISCV::X3)
      .addSym(Label, RISCVII::MO_PCREL_LO);
}

bool RISCVZhmGOTPointer::loopKillsGP(const MachineLoop &L) {
  for (const MachineBasicBlock *MBB : L.blocks())
    for (const MachineInstr &MI : *MBB)
      if (killsGP(MI) && !isGPAuipc(MI))
        return true;
  return false;
}

// The preheader of the outermost loop around MBB in which nothing kills gp,
// or nullptr.
MachineBasicBlock *RISCVZhmGOTPointer::hoistTarget(MachineBasicBlock &MBB) {
  MachineBasicBlock *Target = nullptr;
  for (MachineLoop *L = MLI->getLoopFor(&MBB); L; L = L->getParentLoop()) {
    MachineBasicBlock *Pre = L->getLoopPreheader();
    if (!Pre || loopKillsGP(*L))
      break;
    Target = Pre;
  }
  return Target;
}

// Forward "gp is valid" analysis (must: AND over predecessors), then a load
// in front of every GOT access that is not covered. Blocks are processed in
// reverse post-order and their result is updated right away, so a load
// covers the blocks after it. Insertions only make gp valid in more places,
// so the values across back edges, from before an insertion, are safe (at
// worst a load is placed redundantly). The outer loop repeats until nothing
// is inserted.
bool RISCVZhmGOTPointer::insertGPLoads(MachineFunction &MF) {
  ReversePostOrderTraversal<MachineFunction *> RPOT(&MF);
  bool Changed = false;

  for (unsigned Round = 0; Round < 8; ++Round) {
    DenseMap<const MachineBasicBlock *, bool> Out;
    for (MachineBasicBlock *MBB : RPOT)
      Out[MBB] = true; // optimistic
    auto TransferIn = [&](MachineBasicBlock *MBB) {
      if (MBB == &MF.front() || MBB->pred_empty() || MBB->isEHPad())
        return false;
      for (MachineBasicBlock *P : MBB->predecessors()) {
        auto It = Out.find(P);
        if (It == Out.end() || !It->second)
          return false;
      }
      return true;
    };
    for (bool Again = true; Again;) {
      Again = false;
      for (MachineBasicBlock *MBB : RPOT) {
        bool S = TransferIn(MBB);
        for (const MachineInstr &MI : *MBB)
          if (isGPLoad(MI))
            S = true;
          else if (killsGP(MI))
            S = false;
        if (Out[MBB] != S) {
          Out[MBB] = S;
          Again = true;
        }
      }
    }

    bool Inserted = false;
    SmallPtrSet<MachineBasicBlock *, 4> LoadedPreheaders;
    for (MachineBasicBlock *MBB : RPOT) {
      bool S = TransferIn(MBB);
      for (auto I = MBB->begin(), E = MBB->end(); I != E; ++I) {
        MachineInstr &MI = *I;
        if (isGOTAccess(MI) && !S) {
          if (MachineBasicBlock *Pre = hoistTarget(*MBB)) {
            // The loop does not kill gp: one load at the end of its
            // preheader covers every iteration and every access in it.
            if (LoadedPreheaders.insert(Pre).second) {
              buildGPLoad(*Pre, Pre->getFirstTerminator(), MI.getDebugLoc());
              Out[Pre] = true;
            }
          } else {
            buildGPLoad(*MBB, I, MI.getDebugLoc());
          }
          Inserted = true;
          S = true;
        }
        if (isGPLoad(MI))
          S = true;
        else if (killsGP(MI))
          S = false;
      }
      Out[MBB] = S;
    }
    Changed |= Inserted;
    if (!Inserted)
      break;
  }
  return Changed;
}

//===----------------------------------------------------------------------===//
// Clearing gp
//===----------------------------------------------------------------------===//

// Forward "gp may hold a GOT pointer" analysis (may: OR over predecessors),
// then `li gp, 0` in front of calls (and, conservatively, returns) where it
// may.
bool RISCVZhmGOTPointer::insertGPClears(MachineFunction &MF) {
  if (ZhmGOTPolicy == GOTPolicy::Liberal)
    return false;
  // Returns, and tail calls (they return to this function's caller).
  const bool AtReturns = ZhmGOTPolicy == GOTPolicy::Conservative ||
                         mayBeCalledFromOutside(MF.getFunction());

  auto Transfer = [](const MachineInstr &MI, bool S) {
    if (MI.isCall())
      // A callee in this object may leave the GOT pointer in gp; across a
      // call that may leave the object, gp was cleared in front of it.
      return ZhmGOTPolicy != GOTPolicy::Conservative && !callMayLeave(MI);
    if (MI.isInlineAsm() || isGPLoad(MI) || isGPAuipc(MI))
      return true;
    if (MI.modifiesRegister(RISCV::X3, /*TRI=*/nullptr))
      return false; // li gp, 0
    return S;
  };

  ReversePostOrderTraversal<MachineFunction *> RPOT(&MF);
  DenseMap<const MachineBasicBlock *, bool> Out;
  auto In = [&](MachineBasicBlock *MBB) {
    if (MBB == &MF.front() || MBB->pred_empty() || MBB->isEHPad())
      return true; // a caller may have left it
    for (MachineBasicBlock *P : MBB->predecessors())
      if (Out.lookup(P))
        return true;
    return false;
  };
  for (bool Again = true; Again;) {
    Again = false;
    for (MachineBasicBlock *MBB : RPOT) {
      bool S = In(MBB);
      for (const MachineInstr &MI : *MBB)
        S = Transfer(MI, S);
      if (Out.lookup(MBB) != S) {
        Out[MBB] = S;
        Again = true;
      }
    }
  }

  bool Changed = false;
  for (MachineBasicBlock *MBB : RPOT) {
    bool S = In(MBB);
    for (auto I = MBB->begin(), E = MBB->end(); I != E; ++I) {
      MachineInstr &MI = *I;
      const bool Clear =
          S && ((MI.isCall() && callMayLeave(MI)) ||
                (AtReturns && MI.isReturn()));
      if (Clear) {
        BuildMI(*MBB, I, MI.getDebugLoc(), TII->get(RISCV::ADDI), RISCV::X3)
            .addReg(RISCV::X0)
            .addImm(0);
        S = false;
        Changed = true;
      }
      S = Transfer(MI, S);
    }
  }
  return Changed;
}

//===----------------------------------------------------------------------===//
// Driver
//===----------------------------------------------------------------------===//

bool RISCVZhmGOTPointer::runOnMachineFunction(MachineFunction &MF) {
  const auto &ST = MF.getSubtarget<RISCVSubtarget>();
  if (!ST.hasStdExtZhm())
    return false;
  TII = ST.getInstrInfo();
  IsRV64 = ST.is64Bit();
  MLI = &getAnalysis<MachineLoopInfoWrapperPass>().getLI();

  bool Changed = insertGPLoads(MF);
  Changed |= insertGPClears(MF);
  return Changed;
}

char RISCVZhmGOTPointer::ID = 0;

INITIALIZE_PASS_BEGIN(RISCVZhmGOTPointer, DEBUG_TYPE, "RISC-V Zhm GOT pointer",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(RISCVZhmGOTPointer, DEBUG_TYPE, "RISC-V Zhm GOT pointer",
                    false, false)

FunctionPass *llvm::createRISCVZhmGOTPointerPass() {
  return new RISCVZhmGOTPointer();
}
