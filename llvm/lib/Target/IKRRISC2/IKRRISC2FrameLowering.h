//=- IKRRISC2FrameLowering.h - TargetFrameLowering for IKRRISC2 -*- C++ -*--//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class implements IKRRISC2-specific bits of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_IKRRISC2_IKRRISC2FRAMELOWERING_H
#define LLVM_LIB_TARGET_IKRRISC2_IKRRISC2FRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {
class IKRRISC2FrameLowering : public TargetFrameLowering {

public:
  explicit IKRRISC2FrameLowering()
      : TargetFrameLowering(StackGrowsDown,
                            /*StackAlignment=*/Align(16),
                            /*LocalAreaOffset=*/0) {}

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

protected:
  bool hasFPImpl(const MachineFunction &MF) const override;
};
} // end namespace llvm
#endif // LLVM_LIB_TARGET_IKRRISC2_IKRRISC2FRAMELOWERING_H
