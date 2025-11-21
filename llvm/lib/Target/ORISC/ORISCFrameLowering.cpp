//===-- ORISCFrameLowering.cpp - ORISC Frame Information -*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the ORISC implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "ORISCFrameLowering.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"

using namespace llvm;

void ORISCFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {}
                                      
void ORISCFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {}
                                      
bool ORISCFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return false;
}

/// Returns the displacement from the frame register to the stack
/// frame of the specified index, along with the frame register used
/// (in output arg FrameReg). This is the default implementation which
/// is overridden for some targets.
StackOffset
ORISCFrameLowering::getFrameIndexReference(const MachineFunction &MF, int FI,
                                            Register &FrameReg) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *RI = MF.getSubtarget().getRegisterInfo();

  // By default, assume all frame indices are referenced via whatever
  // getFrameRegister() says. The target can override this if it's doing
  // something different.
  FrameReg = RI->getFrameRegister(MF);

  return StackOffset::getFixed(MFI.getObjectOffset(FI));
}