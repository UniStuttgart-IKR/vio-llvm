//===-- RISCVZhmFrameLowering.cpp - Zhm frame lowering ----------------------===//
//
// Frame (grows UP from the new sp; laid out by PEI itself):
//
//   [0, XLEN)        caller's sp          <- LocalAreaOffset = XLEN
//   [XLEN, ...)      callee-saved slots   (PEI allocates these first)
//   [..., Size)      all other objects
//
//   prologue:  alci sp, Size          (Size > 4095: li tmp, Size; alc sp, tmp)
//              sd   sp, 0(sp)         fused pair, stores the caller's sp
//   epilogue:  ld   sp, 0(sp)
//
// Zhm rules, enforced in processFunctionBeforeFrameIndicesReplaced():
//   * the frame is only accessed sp-relative with a 12-bit immediate,
//   * sp is never copied into another register,
//   * sp is only stored by the fused store directly after alc(i),
//   * sp is only written by alc(i) and the epilogue load.
// Violations are reported as regular compiler errors with source location.
//
// Assumptions: alc/alci return an sp aligned to the ABI stack alignment,
// ALCI is (rd, uimm12), ALC is (rd, rs1).
//
//===----------------------------------------------------------------------===//

#include "Zhm/RISCVZhmFrameLowering.h"
#include "RISCVInstrInfo.h"
#include "RISCVMachineFunctionInfo.h"
#include "RISCVRegisterInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>

using namespace llvm;


//===----------------------------------------------------------------------===//
// Options
//===----------------------------------------------------------------------===//

static cl::opt<bool>
    EnableExplicitSPStore("riscv-zhm-explicit-sp-store",
                          cl::desc("Enables that the stack pointer is moved and then "
                            "explicitly saved onto the stack instead of "
                            "implictly with the alc-instruction."),
                          cl::init(false), cl::NotHidden);

//===----------------------------------------------------------------------===//
// Helpers
//===----------------------------------------------------------------------===//

static void zhmError(const MachineFunction &MF, const Twine &Msg,
                     const MachineInstr *MI = nullptr) {
  const Function &F = MF.getFunction();
  F.getContext().diagnose(DiagnosticInfoUnsupported(
      F, "Zhm: " + Msg,
      MI ? DiagnosticLocation(MI->getDebugLoc()) : DiagnosticLocation()));
}

// Width of plain loads/stores (op0 = value/def, op1 = base, op2 = offset).
static unsigned getAccessWidth(unsigned Opc) {
  switch (Opc) {
  case RISCV::SB: case RISCV::LB: case RISCV::LBU:
    return 1;
  case RISCV::SH: case RISCV::LH: case RISCV::LHU:
  case RISCV::FSH: case RISCV::FLH:
    return 2;
  case RISCV::SW: case RISCV::LW: case RISCV::LWU:
  case RISCV::FSW: case RISCV::FLW:
    return 4;
  case RISCV::SD: case RISCV::LD: case RISCV::FSD: case RISCV::FLD:
    return 8;
  default:
    return 0;
  }
}

static bool isAlloc(const MachineInstr *MI) {
  return MI && (MI->getOpcode() == RISCV::ALCI || MI->getOpcode() == RISCV::ALC);
}

// The instructions of the Zhm frame protocol itself.
static bool isZhmFrameInstr(const MachineInstr &MI) {
  const Register SP = RISCV::X2;
  const unsigned Opc = MI.getOpcode();
  if (isAlloc(&MI))
    return true;
  if (MI.getNumExplicitOperands() < 3)
    return false;
  const bool IsSPAtZeroOfSP =
      MI.getOperand(0).isReg() && MI.getOperand(0).getReg() == SP &&
      MI.getOperand(1).isReg() && MI.getOperand(1).getReg() == SP &&
      MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0;
  if (!IsSPAtZeroOfSP)
    return false;
  // sd sp, 0(sp) -- only as second half of the fused pair.
  if (Opc == RISCV::SD || Opc == RISCV::SW)
    return isAlloc(MI.getPrevNode());
  // ld sp, 0(sp) -- epilogue.
  if (Opc == RISCV::LD || Opc == RISCV::LW)
    return MI.getFlag(MachineInstr::FrameDestroy);
  return false;
}

static void emitCFI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                    const DebugLoc &DL, const TargetInstrInfo &TII,
                    const MCCFIInstruction &I, MachineInstr::MIFlag Flag) {
  MachineFunction &MF = *MBB.getParent();
  BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
      .addCFIIndex(MF.addFrameInst(I))
      .setMIFlag(Flag);
}

// CFA = *(sp + 0): the fused alc(i)/sd pair stored the caller's sp there.
static MCCFIInstruction createDefCfaDeref(unsigned DwarfSP) {
  SmallString<8> Expr;
  raw_svector_ostream E(Expr);
  E << uint8_t(dwarf::DW_OP_breg0 + DwarfSP);
  encodeSLEB128(0, E);
  E << uint8_t(dwarf::DW_OP_deref);

  SmallString<16> Buf;
  raw_svector_ostream B(Buf);
  B << uint8_t(dwarf::DW_CFA_def_cfa_expression);
  encodeULEB128(Expr.size(), B);
  B << Expr.str();
  return MCCFIInstruction::createEscape(nullptr, Buf.str());
}

// Register DwarfReg is saved at address sp + Off.
static MCCFIInstruction createSavedAtSP(unsigned DwarfReg, unsigned DwarfSP,
                                        int64_t Off) {
  SmallString<8> Expr;
  raw_svector_ostream E(Expr);
  E << uint8_t(dwarf::DW_OP_breg0 + DwarfSP);
  encodeSLEB128(Off, E);

  SmallString<16> Buf;
  raw_svector_ostream B(Buf);
  B << uint8_t(dwarf::DW_CFA_expression);
  encodeULEB128(DwarfReg, B);
  encodeULEB128(Expr.size(), B);
  B << Expr.str();
  return MCCFIInstruction::createEscape(nullptr, Buf.str());
}

//===----------------------------------------------------------------------===//
// RISCVZhmFrameLowering
//===----------------------------------------------------------------------===//

RISCVZhmFrameLowering::RISCVZhmFrameLowering(const RISCVSubtarget &STI)
    : RISCVFrameLowering(STI, StackGrowsUp,
                         /*LocalAreaOffset=*/STI.getXLen() / 8),
      Subtarget(STI) {}

StackOffset
RISCVZhmFrameLowering::getFrameIndexReference(const MachineFunction &MF, int FI,
                                              Register &FrameReg) const {
  FrameReg = RISCV::X2;
  return StackOffset::getFixed(MF.getFrameInfo().getObjectOffset(FI));
}

bool RISCVZhmFrameLowering::checkSupportedFrame(
    const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();

  if (MF.getFunction().isVarArg()) {
    zhmError(MF, "variadic functions are not supported");
    return false;
  }
  if (MFI.hasVarSizedObjects()) {
    zhmError(MF, "variable sized stack objects are not supported");
    return false;
  }
  if (MFI.getMaxAlign() > getStackAlign()) {
    zhmError(MF, "stack objects aligned beyond the stack alignment");
    return false;
  }
  // The caller's frame can only be reached through a copy of the old sp.
  if (MFI.getMaxCallFrameSize() != 0) {
    zhmError(MF, "calls that pass arguments on the stack");
    return false;
  }
  for (int FI = MFI.getObjectIndexBegin(); FI < 0; ++FI)
    if (!MFI.isDeadObjectIndex(FI)) {
      zhmError(MF, "arguments passed on the stack");
      return false;
    }
  if (RVFI->getRVVStackSize() != 0) {
    zhmError(MF, "RVV stack objects are not supported");
    return false;
  }
  if (RVFI->isPushable(MF) || RVFI->useSaveRestoreLibCalls(MF)) {
    zhmError(MF, "Zcmp push/pop and -msave-restore are not supported");
    return false;
  }
  return true;
}

//===----------------------------------------------------------------------===//
// Callee-saved slot order
//===----------------------------------------------------------------------===//
//
// Security model: from sp upward the frame must read [sp][ra][others...], i.e.
// ra directly after the saved sp at offset XLEN. PEI's grows-up layout gives
// the FIRST callee-saved entry the HIGHEST offset and fills down to the sp
// slot, so reversing the list puts the last entry at the lowest offset. We
// reverse and then force ra to the very end, so ra gets offset XLEN and the
// rest follow in ascending register order. Everything else is the standard
// PEI assignment.
bool RISCVZhmFrameLowering::assignCalleeSavedSpillSlots(
    MachineFunction &MF, const TargetRegisterInfo *TRI,
    std::vector<CalleeSavedInfo> &CSI) const {
  std::reverse(CSI.begin(), CSI.end());
  auto It = llvm::find_if(CSI, [](const CalleeSavedInfo &I) {
    return I.getReg() == RISCV::X1; // ra
  });
  if (It != CSI.end())
    std::rotate(It, std::next(It), CSI.end()); // move ra to the back
  // Return false: PEI creates the slots in this (reordered) order.
  return false;
}

void RISCVZhmFrameLowering::emitPrologue(MachineFunction &MF,
                                         MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  if (!checkSupportedFrame(MF) || MFI.getStackSize() == 0)
    return; // error reported, or no stack objects -> no frame

  const RISCVInstrInfo *TII = Subtarget.getInstrInfo();
  const RISCVRegisterInfo *RI = Subtarget.getRegisterInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const Register SP = RISCV::X2;
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL;

  // PEI laid the objects out (grows-up, sp slot reserved by LocalAreaOffset,
  // ra pinned to offset XLEN by assignCalleeSavedSpillSlots). We only round.
  const int64_t Size = (int64_t)alignTo(
      (uint64_t)(getOffsetOfLocalArea() + MFI.getStackSize()), getStackAlign());
  MFI.setStackSize(Size); // for -Wframe-larger-than and the epilogue

  if (EnableExplicitSPStore)
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI), RISCV::X5)
        .addReg(SP)
        .addImm(0)
        .setMIFlag(MachineInstr::FrameSetup);

  // --- alc(i) + old-sp store, adjacent so the pipeline can fuse them ------
  if (isUInt<12>(Size)) {
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::ALCI), SP)
        .addImm(Size)
        .setMIFlag(MachineInstr::FrameSetup);
  } else {
    // li tmp, Size ; alc sp, tmp. The virtual register is resolved by PEI's
    // register scavenger, like the scratch RISCVRegisterInfo::adjustReg uses
    // for large standard frames. Unsaved callee-saved registers are pristine
    // and therefore never picked.
    Register Tmp = MRI.createVirtualRegister(&RISCV::GPRRegClass);
    TII->movImm(MBB, MBBI, DL, Tmp, Size, MachineInstr::FrameSetup);
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::ALC), SP)
        .addReg(Tmp, RegState::Kill)
        .setMIFlag(MachineInstr::FrameSetup);
  }
  if (EnableExplicitSPStore)
    BuildMI(MBB, MBBI, DL,
            TII->get(Subtarget.is64Bit() ? RISCV::SD : RISCV::SW))
        .addReg(RISCV::X5, RegState::Kill)
        .addReg(SP)
        .addImm(0)
        .setMIFlag(MachineInstr::FrameSetup);

  const bool EmitCFI = MF.needsFrameMoves();
  const unsigned DwarfSP = RI->getDwarfRegNum(SP, true);
  if (EmitCFI)
    emitCFI(MBB, MBBI, DL, *TII, createDefCfaDeref(DwarfSP),
            MachineInstr::FrameSetup);

  // --- Skip the CSR spills inserted by spillCalleeSavedRegisters ----------
  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  for (size_t I = 0; I < CSI.size() && MBBI != MBB.end(); ++I)
    ++MBBI;

  if (EmitCFI)
    for (const CalleeSavedInfo &CS : CSI)
      emitCFI(MBB, MBBI, DL, *TII,
              createSavedAtSP(RI->getDwarfRegNum(CS.getReg(), true), DwarfSP,
                              MFI.getObjectOffset(CS.getFrameIdx())),
              MachineInstr::FrameSetup);
}

void RISCVZhmFrameLowering::emitEpilogue(MachineFunction &MF,
                                         MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  if (MFI.getStackSize() == 0)
    return;

  const RISCVInstrInfo *TII = Subtarget.getInstrInfo();
  const RISCVRegisterInfo *RI = Subtarget.getRegisterInfo();
  const Register SP = RISCV::X2;

  // CSR restores were already inserted in front of the terminator.
  MachineBasicBlock::iterator MBBI = MBB.getFirstTerminator();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  BuildMI(MBB, MBBI, DL,
          TII->get(Subtarget.is64Bit() ? RISCV::LD : RISCV::LW), SP)
      .addReg(SP)
      .addImm(0)
      .setMIFlag(MachineInstr::FrameDestroy);

  if (MF.needsFrameMoves()) {
    const unsigned DwarfSP = RI->getDwarfRegNum(SP, true);
    emitCFI(MBB, MBBI, DL, *TII,
            MCCFIInstruction::cfiDefCfa(nullptr, DwarfSP, 0),
            MachineInstr::FrameDestroy);
    for (const CalleeSavedInfo &CS : MFI.getCalleeSavedInfo())
      emitCFI(MBB, MBBI, DL, *TII,
              MCCFIInstruction::createRestore(
                  nullptr, RI->getDwarfRegNum(CS.getReg(), true)),
              MachineInstr::FrameDestroy);
  }
}

void RISCVZhmFrameLowering::processFunctionBeforeFrameIndicesReplaced(
    MachineFunction &MF, RegScavenger *RS) const {
  RISCVFrameLowering::processFunctionBeforeFrameIndicesReplaced(MF, RS);

  const Register SP = RISCV::X2;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.isDebugInstr() || MI.isCFIInstruction())
        continue;
      const unsigned W = getAccessWidth(MI.getOpcode());

      // 1. Frame indices may only be the base of a plain load/store.
      //    Anything else (an ADDI address, a frame base register, an RVV
      //    spill) needs a copy of sp in another register. Address-taken
      //    objects were moved to alc by RISCVZhmLegalizeFrameAddr, so this is
      //    only a safety net. The offset need not fit imm12: a large offset
      //    is handled by eliminateFrameIndex, which walks sp to the slot.
      for (const MachineOperand &MO : MI.operands()) {
        if (!MO.isFI())
          continue;
        if (!W || MO.getOperandNo() != 1 || !MI.getOperand(2).isImm()) {
          zhmError(MF,
                   MI.getOpcode() == RISCV::ADDI
                       ? "address of a stack object is taken; Zhm frames can "
                         "only be accessed directly through sp"
                       : "unsupported access to a stack object",
                   &MI);
          return;
        }
      }

      // 2. Explicit sp operands are reserved for the frame protocol.
      //    Implicit uses/defs (calls, returns, call frame pseudos) are fine.
      bool TouchesSP = any_of(MI.explicit_operands(), [&](const MachineOperand &MO) {
        return MO.isReg() && MO.getReg() == SP;
      });
      if (TouchesSP && !isZhmFrameInstr(MI)) {
        zhmError(MF,
                 "instruction reads or writes sp directly (frame address, "
                 "stack-passed argument or inline asm?)",
                 &MI);
        return;
      }
    }
  }
}

//===----------------------------------------------------------------------===//
// Frame index elimination
//===----------------------------------------------------------------------===//
//
// After RISCVZhmLegalizeFrameAddr only load/store base uses reach here. When
// the sp-relative offset fits imm12 the access becomes off(sp). Otherwise sp
// is walked to the slot and back, so no frame pointer ever lives in another
// register:
//
//     li   t, Off
//     add  sp, sp, t          ; sp -> slot (sp was at the frame base)
//     <mem> ..., 0(sp)
//     itd  t, sp              ; t = Off again (index of sp within its object)
//     sub  sp, sp, t          ; sp -> frame base
//
// The restore uses itd instead of the saved Off so it needs no value kept
// across the access. Two independent one-point scavenges supply the scratch:
// before the access (dead once sp is moved; may even be a load's result reg)
// and after it (avoids the loaded value, which the scavenger sees live).

bool RISCVZhmFrameLowering::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                               int SPAdj, unsigned FIOperandNum,
                                               RegScavenger *RS) const {
  assert(SPAdj == 0 && "Zhm reserves the call frame; SPAdj must be 0");
  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const RISCVInstrInfo *TII = STI.getInstrInfo();
  const Register SP = RISCV::X2;
  const DebugLoc &DL = MI.getDebugLoc();

  const int FI = MI.getOperand(FIOperandNum).getIndex();
  Register FrameReg;
  const int64_t Off =
      getFrameIndexReference(MF, FI, FrameReg).getFixed() +
      MI.getOperand(FIOperandNum + 1).getImm();
  assert(FrameReg == SP && "Zhm frames are addressed through sp");

  const bool IsMem = MI.mayLoad() || MI.mayStore();
  const bool BaseIsFI = FIOperandNum == 1 && MI.getNumOperands() > 2 &&
                        MI.getOperand(2).isImm();
  if (!IsMem || !BaseIsFI) {
    MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported(
        MF.getFunction(),
        "Zhm: frame index used other than as a load/store base survived to "
        "frame lowering",
        DiagnosticLocation(MI.getDebugLoc())));
    return false;
  }

  // Common case: fits the 12-bit immediate.
  if (isInt<12>(Off)) {
    MI.getOperand(FIOperandNum).ChangeToRegister(SP, /*isDef=*/false);
    MI.getOperand(FIOperandNum + 1).setImm(Off);
    return false;
  }

  assert(RS && "frame index scavenging must be enabled for Zhm");

  // Pre-part with a scratch valid up to II.
  Register T1 = RS->scavengeRegisterBackwards(RISCV::GPRRegClass, II,
                                              /*RestoreAfter=*/false, 0);
  TII->movImm(MBB, II, DL, T1, Off, MachineInstr::NoFlags);
  BuildMI(MBB, II, DL, TII->get(RISCV::ADD), SP).addReg(SP).addReg(T1, RegState::Kill);

  MI.getOperand(FIOperandNum).ChangeToRegister(SP, false);
  MI.getOperand(FIOperandNum + 1).setImm(0);

  // Post-part: build sub first as an anchor, scavenge at it, then itd before it.
  MachineInstr *Sub =
      BuildMI(MBB, std::next(II), DL, TII->get(RISCV::SUB), SP)
          .addReg(SP).addReg(SP).getInstr();      // placeholder second operand
  //Register T2 = RS->scavengeRegisterBackwards(RISCV::GPRRegClass,
  //                                            Sub->getIterator(),
  //                                            /*RestoreAfter=*/false, 0);
  //BuildMI(MBB, Sub->getIterator(), DL, TII->get(RISCV::ITD), T2).addReg(SP);
  Sub->getOperand(2).setReg(T1);
  Sub->getOperand(2).setIsKill(true);
  return false;
}
