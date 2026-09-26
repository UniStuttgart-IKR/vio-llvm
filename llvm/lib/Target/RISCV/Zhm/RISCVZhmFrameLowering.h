//===-- RISCVZhmFrameLowering.h - Zhm frame lowering -------------*- C++ -*-===//
//
// Frame lowering for the Zhm extension. RISCVSubtarget instantiates this
// class instead of RISCVFrameLowering when Zhm is enabled. Everything that is
// not about the *shape* of the frame (which callee-saved registers to save,
// spill/restore code, scavenging slots, call frame pseudos) is inherited.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVZHMFRAMELOWERING_H
#define LLVM_LIB_TARGET_RISCV_RISCVZHMFRAMELOWERING_H

#include "RISCVFrameLowering.h"

namespace llvm {

class RISCVZhmFrameLowering final : public RISCVFrameLowering {
public:
  explicit RISCVZhmFrameLowering(const RISCVSubtarget &STI);

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  // The only frame register is sp; object offsets are sp-relative already.
  StackOffset getFrameIndexReference(const MachineFunction &MF, int FI,
                                     Register &FrameReg) const override;

  // We round (old-sp slot + objects) ourselves, so a function without
  // stack objects keeps StackSize == 0 and gets no frame at all.
  bool targetHandlesStackFrameRounding() const override { return true; }

  // sp may never be adjusted around calls.
  bool hasReservedCallFrame(const MachineFunction &MF) const override {
    return true;
  }

  // Enforces the Zhm frame rules right before frame indices are replaced.
  void processFunctionBeforeFrameIndicesReplaced(
      MachineFunction &MF, RegScavenger *RS = nullptr) const override;

  // Rewrites a frame-index load/store: off(sp) when the offset fits imm12,
  // otherwise a sp-walk sequence. Called from RISCVRegisterInfo. Returns true
  // if MI was erased (never, here). RS must be non-null for large offsets.
  bool eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj,
                           unsigned FIOperandNum, RegScavenger *RS) const;

  // Reorders callee-saved registers so ra is spilled at offset XLEN; the rest
  // keep the standard PEI layout.
  bool assignCalleeSavedSpillSlots(MachineFunction &MF,
                                   const TargetRegisterInfo *TRI,
                                   std::vector<CalleeSavedInfo> &CSI) const override;

protected:
  // A frame pointer would need `mv s0, sp`, which Zhm forbids.
  // (LLVM < 20: override `bool hasFP(const MachineFunction &) const` instead.)
  bool hasFPImpl(const MachineFunction &MF) const override { return false; }

private:
  const RISCVSubtarget &Subtarget;

  bool checkSupportedFrame(const MachineFunction &MF) const;

};

} // end namespace llvm

#endif