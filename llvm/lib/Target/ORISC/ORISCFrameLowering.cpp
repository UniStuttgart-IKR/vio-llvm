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

using namespace llvm;

void ORISCFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {}
                                      
void ORISCFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {}
                                      
bool ORISCFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return false;
}