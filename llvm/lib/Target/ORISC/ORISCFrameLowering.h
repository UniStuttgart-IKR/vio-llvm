//=- ORISCFrameLowering.h - TargetFrameLowering for ORISC -*- C++ -*--//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class implements ORISC-specific bits of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ORISC_ORISCFRAMELOWERING_H
#define LLVM_LIB_TARGET_ORISC_ORISCFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {
class ORISCSubtarget;
class ORISCFrameLowering : public TargetFrameLowering {

public:
  explicit ORISCFrameLowering(const ORISCSubtarget &STI)
      : TargetFrameLowering(StackGrowsDown,
                            /*StackAlignment=*/Align(16),
                            /*LocalAreaOffset=*/0), STI(STI) {}

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  StackOffset getFrameIndexReference(const MachineFunction &MF, int FI, Register &FrameReg) const override;
  void processFunctionBeforeFrameIndicesReplaced(MachineFunction &MF, RegScavenger *RS = nullptr) const override;

protected:
  const ORISCSubtarget &STI;
  bool hasFPImpl(const MachineFunction &MF) const override;
};
} // end namespace llvm
#endif // LLVM_LIB_TARGET_ORISC_ORISCFRAMELOWERING_H
