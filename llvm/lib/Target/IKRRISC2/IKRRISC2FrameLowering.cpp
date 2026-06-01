//===-- IKRRISC2FrameLowering.cpp - IKRRISC2 Frame Information -*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the IKRRISC2 implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "IKRRISC2FrameLowering.h"

using namespace llvm;

void IKRRISC2FrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {}
                                      
void IKRRISC2FrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {}
                                      
bool IKRRISC2FrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return false;
}