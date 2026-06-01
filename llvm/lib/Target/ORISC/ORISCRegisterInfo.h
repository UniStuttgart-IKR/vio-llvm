//===-- ORISCRegisterInfo.h - Objective-RISC Register Information Impl ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Objective-RISC implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ORISC_ORISCREGISTERINFO_H
#define LLVM_LIB_TARGET_ORISC_ORISCREGISTERINFO_H

#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "ORISCFrameLowering.h"
#include "MCTargetDesc/ORISCMCTargetDesc.h"

#define GET_REGINFO_HEADER
#include "ORISCGenRegisterInfo.inc"

namespace llvm {
struct ORISCRegisterInfo : public ORISCGenRegisterInfo {
  ORISCRegisterInfo() : ORISCGenRegisterInfo(ORISC::P31) {}

  /// Code Generation virtual methods...
  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;
  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                      CallingConv::ID CC) const override;

  const uint32_t* getRTCallPreservedMask(CallingConv::ID CC) const;

  BitVector getReservedRegs(const MachineFunction &MF) const override;
  bool isReservedReg(const MachineFunction &MF, MCRegister Reg) const;

  bool eliminateFrameIndex(MachineBasicBlock::iterator II,
                          int SPAdj, unsigned FIOperandNum,
                          RegScavenger *RS = nullptr) const override;

  Register getFrameRegister(const MachineFunction &MF) const override;

  const TargetRegisterClass *getPointerRegClass(unsigned Kind) const override;
};

} // end namespace llvm

#endif
