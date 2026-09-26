//===-- RISCVZhmLegalizeFrameAddr.cpp - Reroute frame addresses to alc -----===//
//
// A Zhm frame may only be touched as `load/store imm12(sp)`. But SelectionDAG,
// soft-float / illegal-type expansion, byval lowering and
// LocalStackSlotAllocation all create stack objects whose ADDRESS is put into
// a register, e.g.
//
//     addi a0, sp, 16        ; &tmp, passed to a soft-float helper
//
// which is a frame pointer in a register other than sp and traps on Zhm.
// These objects do not exist at IR level, so RISCVZhmEscapeAlloca cannot see
// them. This machine pass, run before register allocation, catches them:
//
//   * a stack object accessed ONLY as the base of a load/store with an
//     immediate offset stays in the sp frame (the frame lowering handles it);
//   * a stack object whose address is materialised any other way is moved to
//     alc memory. An alc result is a first-class pointer that may live in any
//     register, so `addi a0, <alcptr>, 16` is legal.
//
// Running pre-RA lets us use virtual registers and the register allocator for
// the alc pointer; alc memory lives as long as a reference exists, so nothing
// has to be freed.
//
// Together with requiresVirtualBaseRegisters()==false for Zhm (see
// RISCVRegisterInfo) this removes every non-sp frame pointer before PEI.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-zhm-legalize-frame-addr"

namespace {

class RISCVZhmLegalizeFrameAddr : public MachineFunctionPass {
  // Width of a plain load/store whose base is operand 1 and offset operand 2;
  // 0 for anything else.
  static unsigned baseLoadStoreWidth(unsigned Opc) {
    switch (Opc) {
    case RISCV::LB: case RISCV::LBU: case RISCV::SB:
      return 1;
    case RISCV::LH: case RISCV::LHU: case RISCV::SH:
    case RISCV::FLH: case RISCV::FSH:
      return 2;
    case RISCV::LW: case RISCV::LWU: case RISCV::SW:
    case RISCV::FLW: case RISCV::FSW:
      return 4;
    case RISCV::LD: case RISCV::SD: case RISCV::FLD: case RISCV::FSD:
      return 8;
    default:
      return 0;
    }
  }

  // True if this operand use keeps the object addressable through sp alone:
  // it is the base of a plain load/store with an immediate offset.
  static bool isFrameBaseUse(const MachineInstr &MI, const MachineOperand &MO) {
    if (!baseLoadStoreWidth(MI.getOpcode()))
      return false;
    if (MI.getNumExplicitOperands() < 3 || !MI.getOperand(2).isImm())
      return false;
    return MO.getOperandNo() == 1; // operand 0 is the loaded/stored value
  }

public:
  static char ID;
  RISCVZhmLegalizeFrameAddr() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "RISC-V Zhm legalize frame addresses";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  MachineFunctionProperties getRequiredProperties() const override {
    // Needs virtual registers (pre-RA, still in SSA).
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::IsSSA);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const RISCVSubtarget &STI = MF.getSubtarget<RISCVSubtarget>();
    if (!STI.hasStdExtZhm())
      return false;

    MachineFrameInfo &MFI = MF.getFrameInfo();
    if (MFI.getNumObjects() == 0)
      return false;

    const RISCVInstrInfo *TII = STI.getInstrInfo();
    MachineRegisterInfo &MRI = MF.getRegInfo();

    // 1. Find objects whose address is materialised in a register.
    DenseSet<int> Escaping;
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : MBB)
        for (const MachineOperand &MO : MI.operands()) {
          if (!MO.isFI())
            continue;
          int FI = MO.getIndex();
          if (MFI.isDeadObjectIndex(FI) ||
              MFI.getStackID(FI) != TargetStackID::Default)
            continue; // RVV etc.: not ours to reroute
          if (!isFrameBaseUse(MI, MO))
            Escaping.insert(FI);
        }
    if (Escaping.empty())
      return false;

    auto Fail = [&](const Twine &Why) {
      MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported(
          MF.getFunction(), "Zhm: " + Why));
    };

    // 2. Allocate alc memory for each escaping object at function entry.
    MachineBasicBlock &Entry = MF.front();
    MachineBasicBlock::iterator IP = Entry.begin();
    const DebugLoc DL;
    DenseMap<int, Register> Ptr;
    for (int FI : Escaping) {
      if (MFI.getObjectAlign(FI).value() > 16) {
        Fail("stack object with its address taken is aligned to more than 16 "
             "bytes");
        continue;
      }
      int64_t Size = MFI.getObjectSize(FI);
      if (Size <= 0) {
        Fail("stack object with unknown size has its address taken");
        continue;
      }

      Register P = MRI.createVirtualRegister(&RISCV::GPRRegClass);
      if (isUInt<12>(Size)) {
        BuildMI(Entry, IP, DL, TII->get(RISCV::ALCI), P).addImm(Size);
      } else {
        Register Sz = MRI.createVirtualRegister(&RISCV::GPRRegClass);
        TII->movImm(Entry, IP, DL, Sz, Size, MachineInstr::NoFlags);
        BuildMI(Entry, IP, DL, TII->get(RISCV::ALC), P).addReg(Sz);
      }
      Ptr[FI] = P;
      LLVM_DEBUG(dbgs() << "Zhm: rerouting fi#" << FI << " (" << Size
                        << " bytes) to alc in " << MF.getName() << "\n");
    }

    // 3. Replace every reference to a rerouted object by its alc pointer.
    //    A load/store base becomes a register base; an `addi rd, fi, off`
    //    becomes `addi rd, <alcptr>, off` (pointer + integer, legal).
    bool Changed = false;
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : MBB)
        for (MachineOperand &MO : MI.operands()) {
          if (!MO.isFI())
            continue;
          auto It = Ptr.find(MO.getIndex());
          if (It == Ptr.end())
            continue;
          MO.ChangeToRegister(It->second, /*isDef=*/false);
          Changed = true;
        }

    // The rerouted frame objects are now unreferenced. They keep their (small)
    // frame slot; if MachineFrameInfo in your tree can drop an object, do so
    // here to reclaim it.
    return Changed;
  }
};

} // end anonymous namespace

char RISCVZhmLegalizeFrameAddr::ID = 0;

INITIALIZE_PASS(RISCVZhmLegalizeFrameAddr, DEBUG_TYPE,
                "RISC-V Zhm legalize frame addresses", false, false)

FunctionPass *llvm::createRISCVZhmLegalizeFrameAddrPass() {
  return new RISCVZhmLegalizeFrameAddr();
}