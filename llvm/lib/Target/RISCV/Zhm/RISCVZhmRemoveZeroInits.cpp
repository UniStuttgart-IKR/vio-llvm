//===-- RISCVZhmRemoveZeroInits.cpp - Remove redundant zero stores -------===//
//
// The Zhm allocation instructions (ALC, ALC_D, ALCI, ALCI_D) return a pointer
// to freshly allocated, zero-initialised memory. Any store of zero into that
// memory before it has been written with something else is redundant.
//
// The pass is deliberately conservative. Starting at an allocation it walks
// forward through the same basic block and stops at the first instruction it
// cannot reason about. A store is only removed if ALL of these hold:
//   * the stored value is provably zero,
//   * its address is <allocation pointer (or a tracked copy) + known offset>,
//   * the accessed bytes lie completely inside the allocation (size known),
//   * no earlier store in the scanned range wrote to any of those bytes,
//   * the pointer has not escaped (so nothing else can write to the region),
//   * the store is not volatile / ordered.
//
// ASSUMPTIONS (adjust to your instruction definitions):
//   * operand 0 of every ALC* instruction is the returned pointer (rd),
//   * operand 1 is the size in bytes: a register for ALC/ALC_D, an immediate
//     for ALCI/ALCI_D. (_D = data-only memory, same size encoding.)
//
// Best placed before register allocation (addPreRegAlloc): alc results are
// then virtual registers and spills cannot make the pointer look escaped.
// The IR pass RISCVZhmEscapeAlloca already removes zero memsets right after
// an allocation; this pass catches the zero stores that remain after
// instruction selection (inline expansions, `x = 0` initialisers).
//
// Works both before register allocation (SSA / virtual registers) and after.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "riscv-zhm-remove-zero-inits"

static cl::opt<bool> DisableRemove(
    "riscv-disable-zhm-remove-zero-inits", cl::init(false), cl::Hidden,
    cl::desc("Disable removal of redundant zero stores after alc/alci"));

namespace {

class RISCVZhmRemoveZeroInits : public MachineFunctionPass {
  const TargetRegisterInfo *TRI = nullptr;
  const MachineRegisterInfo *MRI = nullptr;

  // Half-open byte interval [Begin, End) relative to the allocation start.
  struct ByteRange {
    int64_t Begin, End;
    bool overlaps(const ByteRange &O) const {
      return Begin < O.End && O.Begin < End;
    }
  };

  static bool isAlloc(unsigned Opc) {
    return Opc == RISCV::ALC || Opc == RISCV::ALC_D || Opc == RISCV::ALCI ||
           Opc == RISCV::ALCI_D;
  }

  // Width in bytes of a plain integer store, 0 if not one.
  // Layout of all of them: op0 = value (rs2), op1 = base (rs1), op2 = offset.
  static unsigned getStoreWidth(unsigned Opc) {
    switch (Opc) {
    case RISCV::SB: return 1;
    case RISCV::SH: return 2;
    case RISCV::SW: return 4;
    case RISCV::SD: return 8;
    default:        return 0;
    }
  }

  // Plain loads: op0 = def, op1 = base, op2 = offset. Loads never modify
  // memory, so using the pointer as their base is harmless.
  static bool isSimpleLoad(unsigned Opc) {
    switch (Opc) {
    case RISCV::LB: case RISCV::LBU:
    case RISCV::LH: case RISCV::LHU:
    case RISCV::LW: case RISCV::LWU:
    case RISCV::LD:
    case RISCV::FLW: case RISCV::FLD:
      return true;
    default:
      return false;
    }
  }

  // Try to prove that register R holds a constant at instruction Pos.
  std::optional<int64_t> getConstantAt(Register R, const MachineInstr &Pos,
                                       unsigned Depth = 0) const {
    if (R == RISCV::X0)
      return 0;
    if (!R || Depth > 4)
      return std::nullopt;

    const MachineInstr *Def = nullptr;
    if (R.isVirtual()) {
      // Only trust a single, dominating definition.
      Def = MRI->getUniqueVRegDef(R);
    } else {
      // Physical register: find the closest preceding def in this block.
      const MachineBasicBlock &MBB = *Pos.getParent();
      for (auto I = Pos.getIterator(); I != MBB.begin();) {
        --I;
        if (I->isDebugInstr())
          continue;
        if (I->modifiesRegister(R, TRI)) { // also catches call regmasks
          Def = &*I;
          break;
        }
      }
    }
    if (!Def)
      return std::nullopt;

    // li rd, imm  ==  addi rd, <const>, imm
    if (Def->getOpcode() == RISCV::ADDI && Def->getOperand(0).getReg() == R &&
        Def->getOperand(1).isReg() && Def->getOperand(2).isImm()) {
      if (auto Base = getConstantAt(Def->getOperand(1).getReg(), *Def,
                                    Depth + 1))
        return *Base + Def->getOperand(2).getImm();
      return std::nullopt;
    }
    // %r = COPY $x0 (typical pre-RA zero materialisation)
    if (Def->isCopy() && Def->getOperand(0).getReg() == R)
      return getConstantAt(Def->getOperand(1).getReg(), *Def, Depth + 1);

    return std::nullopt;
  }

  // Size of the allocation in bytes, if it is statically known.
  std::optional<int64_t> getAllocSize(const MachineInstr &Alloc) const {
    const MachineOperand &SizeOp = Alloc.getOperand(1);
    std::optional<int64_t> Size;
    if (SizeOp.isImm())
      Size = SizeOp.getImm();
    else if (SizeOp.isReg())
      Size = getConstantAt(SizeOp.getReg(), Alloc);
    if (!Size)
      return std::nullopt;

    return Size;
  }

  // Scan forward from one allocation. Returns true if anything was found.
  bool processAlloc(MachineInstr &Alloc,
                    SmallSetVector<MachineInstr *, 16> &ToErase) const {
    if (!Alloc.getOperand(0).isReg())
      return false;
    Register Ptr = Alloc.getOperand(0).getReg();
    if (!Ptr || Ptr == RISCV::X0)
      return false;

    std::optional<int64_t> Size = getAllocSize(Alloc);
    if (!Size || *Size <= 0)
      return false;

    // Registers that currently hold <allocation start + offset>.
    DenseMap<Register, int64_t> Tracked;
    Tracked[Ptr] = 0;
    // Bytes that may have been written with a non-zero value.
    SmallVector<ByteRange, 8> Dirty;
    bool Found = false;

    MachineBasicBlock &MBB = *Alloc.getParent();
    auto E = MBB.end();
    for (auto I = std::next(Alloc.getIterator()); I != E && !Tracked.empty(); ++I) {
      MachineInstr &MI = *I;
      unsigned Opc = MI.getOpcode();
      if (MI.isDebugInstr())
        continue;

      // Anything we cannot model ends the analysis.
      if (MI.isCall() || MI.isInlineAsm() || MI.hasUnmodeledSideEffects() ||
          MI.isTerminator() || (MI.mayLoad() && MI.mayStore()) || isAlloc(Opc))
        break;

      // --- Plain stores ---------------------------------------------------
      if (unsigned Width = getStoreWidth(Opc)) {
        const MachineOperand &ValOp = MI.getOperand(0);
        const MachineOperand &BaseOp = MI.getOperand(1);
        const MachineOperand &OffOp = MI.getOperand(2);

        // Pointer itself is written to memory -> it escapes.
        if (ValOp.isReg() && Tracked.count(ValOp.getReg()))
          break;

        auto It = BaseOp.isReg() ? Tracked.find(BaseOp.getReg())
                                 : Tracked.end();
        if (It == Tracked.end()) {
          // Different base register. Since the pointer has not escaped,
          // this store cannot touch the fresh allocation.
          continue;
        }

        // Store into our allocation: we need an exact, in-bounds offset.
        if (!OffOp.isImm())
          break;
        int64_t Off = It->second + OffOp.getImm();
        if (Off < 0 || Off + (int64_t)Width > *Size)
          break;

        ByteRange Range{Off, Off + (int64_t)Width};
        bool Overwrites = any_of(
            Dirty, [&](const ByteRange &D) { return D.overlaps(Range); });
        std::optional<int64_t> Val =
            ValOp.isReg() ? getConstantAt(ValOp.getReg(), MI) : std::nullopt;

        if (!Overwrites && Val && *Val == 0 && !MI.hasOrderedMemoryRef()) {
          LLVM_DEBUG(dbgs() << "Zhm: removing redundant zero store: " << MI);
          ToErase.insert(&MI);
          Found = true;
        } else {
          Dirty.push_back(Range);
        }
        continue;
      }

      // --- Every other use of a tracked register must be harmless -------
      bool Escapes = false;
      for (const MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.isUse() || !MO.getReg() ||
            !Tracked.count(MO.getReg()))
          continue;
        unsigned Idx = MO.getOperandNo();
        if (isSimpleLoad(Opc) && Idx == 1)
          continue; // used as load address
        if (Opc == RISCV::ADDI && Idx == 1 && MI.getOperand(2).isImm())
          continue; // pointer arithmetic with known offset
        if (MI.isCopy() && Idx == 1)
          continue; // plain copy
        Escapes = true; // add, sub, branch, custom instr, ... -> give up
        break;
      }
      if (Escapes)
        break;

      // --- Update the set of registers holding (derived) pointers -------
      std::optional<std::pair<Register, int64_t>> NewDerived;
      if (Opc == RISCV::ADDI && MI.getOperand(1).isReg() &&
          MI.getOperand(2).isImm()) {
        auto It = Tracked.find(MI.getOperand(1).getReg());
        if (It != Tracked.end())
          NewDerived = {MI.getOperand(0).getReg(),
                        It->second + MI.getOperand(2).getImm()};
      } else if (MI.isCopy()) {
        auto It = Tracked.find(MI.getOperand(1).getReg());
        if (It != Tracked.end())
          NewDerived = {MI.getOperand(0).getReg(), It->second};
      }

      SmallVector<Register, 4> Clobbered;
      for (const auto &KV : Tracked)
        if (MI.modifiesRegister(KV.first, TRI))
          Clobbered.push_back(KV.first);
      for (Register R : Clobbered)
        Tracked.erase(R);

      if (NewDerived && NewDerived->first && NewDerived->first != RISCV::X0)
        Tracked[NewDerived->first] = NewDerived->second;
    }

    return Found;
  }

public:
  static char ID;
  RISCVZhmRemoveZeroInits() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "RISC-V Zhm remove redundant zero initialisers";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const RISCVSubtarget &STI = MF.getSubtarget<RISCVSubtarget>();
    if (!STI.hasStdExtZhm() || DisableRemove || skipFunction(MF.getFunction()))
      return false;

    TRI = STI.getRegisterInfo();
    MRI = &MF.getRegInfo();

    // Collect first, erase afterwards, so iterators stay valid.
    SmallSetVector<MachineInstr *, 16> ToErase;
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : MBB)
        if (isAlloc(MI.getOpcode()))
          processAlloc(MI, ToErase);

    for (MachineInstr *MI : ToErase)
      MI->eraseFromParent();

    return !ToErase.empty();
  }
};

} // end anonymous namespace

char RISCVZhmRemoveZeroInits::ID = 0;

INITIALIZE_PASS(RISCVZhmRemoveZeroInits, DEBUG_TYPE,
                "RISC-V Zhm remove redundant zero initialisers", false, false)

FunctionPass *llvm::createRISCVZhmRemoveZeroInitsPass() {
  return new RISCVZhmRemoveZeroInits();
}