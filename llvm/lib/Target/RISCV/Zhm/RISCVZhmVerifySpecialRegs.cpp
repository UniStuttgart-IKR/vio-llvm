//===-- RISCVZhmVerifySpecialRegs.cpp - Enforce the Zhm ra/sp/gp rules ------------===//
//
// On Zhm, sp, gp and ra must not be written by hand, and ra may only be the
// source or destination of a jump. The hardware additionally supervises the
// save and restore of ra in its frame slot at offset XLEN.
//
// The rest of the backend is set up so that these rules hold by construction:
//   * ra is a reserved register for Zhm (RISCVRegisterInfo), so the register
//     allocator never uses it as a general register;
//   * calls use t1, not ra, as the temporary of `auipc` + `jalr`
//     (RISCVMCCodeEmitter);
//   * sp is only changed by the frame protocol (RISCVZhmFrameLowering);
//   * gp is reserved on RISC-V anyway.
//
// This pass runs last, before emission, and reports anything that still
// breaks a rule, e.g. __builtin_return_address(0) (a copy of ra) or inline
// assembly. It changes nothing.
//
// Allowed, explicitly:
//   ra  def: jal/jalr rd; `l[wd] ra, XLEN(sp)` (restore)
//       use: jalr rs1 (ret); `s[wd] ra, XLEN(sp)` (save)
//   sp  def: alc[i][.d] sp; `l[wd] sp, 0(sp)`; addi sp, sp, imm;
//            add/sub sp, sp, rX (sp walk)
//   gp  def: never
// Implicit ra/sp/gp operands are allowed on calls and returns (they describe
// the jump itself). Meta and debug instructions are ignored.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-zhm-verify-regs"

namespace {

class RISCVZhmVerifySpecialRegs : public MachineFunctionPass {
public:
  static char ID;
  RISCVZhmVerifySpecialRegs() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "RISC-V Zhm verify ra/sp/gp usage";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  unsigned XLenBytes = 4;

  bool isWordLoad(unsigned Opc) const {
    return Opc == (XLenBytes == 8 ? RISCV::LD : RISCV::LW);
  }
  bool isWordStore(unsigned Opc) const {
    return Opc == (XLenBytes == 8 ? RISCV::SD : RISCV::SW);
  }
  static bool isImm(const MachineInstr &MI, unsigned Idx, int64_t V) {
    return MI.getNumOperands() > Idx && MI.getOperand(Idx).isImm() &&
           MI.getOperand(Idx).getImm() == V;
  }
  static bool isReg(const MachineInstr &MI, unsigned Idx, Register R) {
    return MI.getNumOperands() > Idx && MI.getOperand(Idx).isReg() &&
           MI.getOperand(Idx).getReg() == R;
  }

  bool allowedRaDef(const MachineInstr &MI) const;
  bool allowedRaUse(const MachineInstr &MI, unsigned OpNo) const;
  bool allowedSpDef(const MachineInstr &MI) const;
};

} // end anonymous namespace

bool RISCVZhmVerifySpecialRegs::allowedRaDef(const MachineInstr &MI) const {
  const unsigned Opc = MI.getOpcode();
  if (Opc == RISCV::JAL || Opc == RISCV::JALR)
    return true;
  // Restore from the supervised slot: l[wd] ra, XLEN(sp)
  return isWordLoad(Opc) && isReg(MI, 1, RISCV::X2) &&
         isImm(MI, 2, XLenBytes);
}

bool RISCVZhmVerifySpecialRegs::allowedRaUse(const MachineInstr &MI,
                                     unsigned OpNo) const {
  const unsigned Opc = MI.getOpcode();
  if (Opc == RISCV::JALR && OpNo == 1) // jump target, e.g. ret
    return true;
  // Save to the supervised slot: s[wd] ra, XLEN(sp)
  return isWordStore(Opc) && OpNo == 0 && isReg(MI, 1, RISCV::X2) &&
         isImm(MI, 2, XLenBytes);
}

bool RISCVZhmVerifySpecialRegs::allowedSpDef(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  case RISCV::ALC:
  case RISCV::ALC_D:
  case RISCV::ALCI:
  case RISCV::ALCI_D:
    return true;
  case RISCV::ADDI: // addi sp, sp, imm
    return isReg(MI, 1, RISCV::X2);
  case RISCV::ADD:
  case RISCV::SUB: // sp walk: add/sub sp, sp, rX
    return isReg(MI, 1, RISCV::X2);
  default:
    // Frame release: l[wd] sp, 0(sp)
    return isWordLoad(MI.getOpcode()) && isReg(MI, 1, RISCV::X2) &&
           isImm(MI, 2, 0);
  }
}

bool RISCVZhmVerifySpecialRegs::runOnMachineFunction(MachineFunction &MF) {
  const RISCVSubtarget &STI = MF.getSubtarget<RISCVSubtarget>();
  if (!STI.hasStdExtZhm())
    return false;
  XLenBytes = STI.getXLen() / 8;
  const Function &F = MF.getFunction();

  auto report = [&](const MachineInstr &MI, const Twine &What) {
    F.getContext().diagnose(DiagnosticInfoUnsupported(
        F, "Zhm: " + What, DiagnosticLocation(MI.getDebugLoc())));
  };

  for (const MachineBasicBlock &MBB : MF)
    for (const MachineInstr &MI : MBB) {
      if (MI.isMetaInstruction() || MI.isDebugInstr() || MI.isCFIInstruction())
        continue;
      const bool IsJump = MI.isCall() || MI.isReturn();

      for (const MachineOperand &MO : MI.operands()) {
        if (!MO.isReg())
          continue;
        const Register R = MO.getReg();
        if (R != RISCV::X1 && R != RISCV::X2 && R != RISCV::X3)
          continue;
        if (MO.isImplicit()) {
          // Calls and returns carry ra/sp as implicit operands; anything
          // else that implicitly writes them is a violation.
          if (MO.isDef() && !IsJump)
            report(MI, "instruction implicitly writes ra, sp or gp");
          continue;
        }
        const unsigned OpNo = MO.getOperandNo();
        if (R == RISCV::X3) {
          if (MO.isDef())
            report(MI, "gp may not be written");
        } else if (R == RISCV::X1) {
          if (MO.isDef() ? !allowedRaDef(MI) : !allowedRaUse(MI, OpNo))
            report(MI, MO.isDef()
                           ? "ra may only be written by a jump"
                           : "ra may only be read by a jump (e.g. "
                             "__builtin_return_address is not supported)");
        } else if (MO.isDef() && !allowedSpDef(MI)) {
          report(MI, "sp may only be changed by the frame protocol");
        }
      }
    }
  return false;
}

char RISCVZhmVerifySpecialRegs::ID = 0;

INITIALIZE_PASS(RISCVZhmVerifySpecialRegs, DEBUG_TYPE,
                "RISC-V Zhm verify ra/sp/gp usage", false, true)

FunctionPass *llvm::createRISCVZhmVerifySpecialRegsPass() {
  return new RISCVZhmVerifySpecialRegs();
}