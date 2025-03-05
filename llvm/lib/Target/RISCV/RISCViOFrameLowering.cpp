//===-- RISCViOFrameLowering.cpp - RISC-V Frame Information -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the RISC-V implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "RISCViOFrameLowering.h"
#include "RISCVMachineFunctionInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/Debug.h"

#include <algorithm>

#define DEBUG_TYPE "codegen"

using namespace llvm;

static Align getABIStackAlignment(RISCVABI::ABI ABI) {
  if (ABI == RISCVABI::ABI_ILP32E)
    return Align(4);
  if (ABI == RISCVABI::ABI_LP64E)
    return Align(8);
  return Align(16);
}

RISCViOFrameLowering::RISCViOFrameLowering(const RISCVSubtarget &STI)
    : TargetFrameLowering(
          StackGrowsDown, getABIStackAlignment(STI.getTargetABI()),
          /*LocalAreaOffset=*/0,
          /*TransientStackAlignment=*/getABIStackAlignment(STI.getTargetABI()),
          false),
      STI(STI) {}

// The register used to hold the stack pointer.
static constexpr Register SPReg = RISCV::X2;

// The register used to hold the return address.
static constexpr Register RAReg = RISCV::X1;

// Offsets which need to be scale by XLen representing locations of CSRs which
// are given a fixed location by save/restore libcalls or Zcmp Push/Pop.
static const std::pair<MCPhysReg, int8_t> FixedCSRFIMap[] = {
    {/*ra*/ RAReg, -1},       {/*s0*/ RISCV::X8, -2},
    {/*s1*/ RISCV::X9, -3},   {/*s2*/ RISCV::X18, -4},
    {/*s3*/ RISCV::X19, -5},  {/*s4*/ RISCV::X20, -6},
    {/*s5*/ RISCV::X21, -7},  {/*s6*/ RISCV::X22, -8},
    {/*s7*/ RISCV::X23, -9},  {/*s8*/ RISCV::X24, -10},
    {/*s9*/ RISCV::X25, -11}, {/*s10*/ RISCV::X26, -12},
    {/*s11*/ RISCV::X27, -13}};


// Return true if the specified function should have a dedicated frame
// pointer register.  This is true if frame pointer elimination is
// disabled, if it needs dynamic stack realignment, if the function has
// variable sized allocas, or if the frame address is taken.
bool RISCViOFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return false;
}

bool RISCViOFrameLowering::hasBP(const MachineFunction &MF) const {
  return false;
}

// Allocate stack space and probe it if necessary.
void RISCViOFrameLowering::allocateStack(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI,
                                       MachineFunction &MF, uint64_t Offset,
                                       uint64_t RealStackSize, bool EmitCFI,
                                       bool NeedProbe,
                                       uint64_t ProbeSize) const {
  DebugLoc DL;
  const RISCVRegisterInfo *RI = STI.getRegisterInfo();
  const RISCVInstrInfo *TII = STI.getInstrInfo();

}

void RISCViOFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();
  const RISCVRegisterInfo *RI = STI.getRegisterInfo();
  const RISCVInstrInfo *TII = STI.getInstrInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();

  
}

void RISCViOFrameLowering::deallocateStack(MachineFunction &MF,
                                         MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MBBI,
                                         const DebugLoc &DL,
                                         uint64_t &StackSize,
                                         int64_t CFAOffset) const {
  const RISCVRegisterInfo *RI = STI.getRegisterInfo();
  const RISCVInstrInfo *TII = STI.getInstrInfo();

  
}

StackOffset
RISCViOFrameLowering::getFrameIndexReference(const MachineFunction &MF, int FI,
                                           Register &FrameReg) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *RI = MF.getSubtarget().getRegisterInfo();
  const auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();
  const RISCVSubtarget &Subtarget = MF.getSubtarget<RISCVSubtarget>();

  
}

void RISCViOFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                              BitVector &SavedRegs,
                                              RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
}


void RISCViOFrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *RS) const {
  const RISCVRegisterInfo *RegInfo =
      MF.getSubtarget<RISCVSubtarget>().getRegisterInfo();
  const RISCVInstrInfo *TII = MF.getSubtarget<RISCVSubtarget>().getInstrInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterClass *RC = &RISCV::GPRRegClass;
  auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();

  //Get spills size and add callee save size to it. save it as callee saved stack size
  unsigned Size = RVFI->getReservedSpillsSize();
  for (const auto &Info : MFI.getCalleeSavedInfo()) {
    int FrameIdx = Info.getFrameIdx();
    if (FrameIdx < 0 || MFI.getStackID(FrameIdx) != TargetStackID::Default)
      continue;

    Size += MFI.getObjectSize(FrameIdx);
  }
  RVFI->setCalleeSavedStackSize(Size);
}

// Not preserve stack space within prologue for outgoing variables when the
// function contains variable size objects or there are vector objects accessed
// by the frame pointer.
// Let eliminateCallFramePseudoInstr preserve stack space for it.
bool RISCViOFrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  return true;
}


bool RISCViOFrameLowering::assignCalleeSavedSpillSlots(
    MachineFunction &MF, const TargetRegisterInfo *TRI,
    std::vector<CalleeSavedInfo> &CSI, unsigned &MinCSFrameIndex,
    unsigned &MaxCSFrameIndex) const {

  return true;
}

bool RISCViOFrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {

  return true;
}

bool RISCViOFrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {

  return true;
}

bool RISCViOFrameLowering::enableShrinkWrapping(const MachineFunction &MF) const {
  // Keep the conventional code flow when not optimizing.
  if (MF.getFunction().hasOptNone())
    return false;

  return true;
}

bool RISCViOFrameLowering::canUseAsPrologue(const MachineBasicBlock &MBB) const {
  MachineBasicBlock *TmpMBB = const_cast<MachineBasicBlock *>(&MBB);
  const MachineFunction *MF = MBB.getParent();
  const auto *RVFI = MF->getInfo<RISCVMachineFunctionInfo>();

  if (!RVFI->useSaveRestoreLibCalls(*MF))
    return true;

  // Inserting a call to a __riscv_save libcall requires the use of the register
  // t0 (X5) to hold the return address. Therefore if this register is already
  // used we can't insert the call.

  RegScavenger RS;
  RS.enterBasicBlock(*TmpMBB);
  return !RS.isRegUsed(RISCV::X5);
}

bool RISCViOFrameLowering::canUseAsEpilogue(const MachineBasicBlock &MBB) const {
  const MachineFunction *MF = MBB.getParent();
  MachineBasicBlock *TmpMBB = const_cast<MachineBasicBlock *>(&MBB);
  const auto *RVFI = MF->getInfo<RISCVMachineFunctionInfo>();

  if (!RVFI->useSaveRestoreLibCalls(*MF))
    return true;

  // Using the __riscv_restore libcalls to restore CSRs requires a tail call.
  // This means if we still need to continue executing code within this function
  // the restore cannot take place in this basic block.

  if (MBB.succ_size() > 1)
    return false;

  MachineBasicBlock *SuccMBB =
      MBB.succ_empty() ? TmpMBB->getFallThrough() : *MBB.succ_begin();

  // Doing a tail call should be safe if there are no successors, because either
  // we have a returning block or the end of the block is unreachable, so the
  // restore will be eliminated regardless.
  if (!SuccMBB)
    return true;

  // The successor can only contain a return, since we would effectively be
  // replacing the successor with our own tail return at the end of our block.
  return SuccMBB->isReturnBlock() && SuccMBB->size() == 1;
}

bool RISCViOFrameLowering::isSupportedStackID(TargetStackID::Value ID) const {
  switch (ID) {
  case TargetStackID::Default:
  case TargetStackID::ScalableVector:
    return true;
  case TargetStackID::NoAlloc:
  case TargetStackID::SGPRSpill:
  case TargetStackID::WasmLocal:
    return false;
  }
  llvm_unreachable("Invalid TargetStackID::Value");
}

TargetStackID::Value RISCViOFrameLowering::getStackIDForScalableVectors() const {
  return TargetStackID::ScalableVector;
}

// Synthesize the probe loop.
static void emitStackProbeInline(MachineFunction &MF, MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 DebugLoc DL) {

  auto &Subtarget = MF.getSubtarget<RISCVSubtarget>();
  const RISCVInstrInfo *TII = Subtarget.getInstrInfo();
  bool IsRV64 = Subtarget.is64Bit();
  Align StackAlign = Subtarget.getFrameLowering()->getStackAlign();
  const RISCVTargetLowering *TLI = Subtarget.getTargetLowering();
  uint64_t ProbeSize = TLI->getStackProbeSize(MF, StackAlign);

  MachineFunction::iterator MBBInsertPoint = std::next(MBB.getIterator());
  MachineBasicBlock *LoopTestMBB =
      MF.CreateMachineBasicBlock(MBB.getBasicBlock());
  MF.insert(MBBInsertPoint, LoopTestMBB);
  MachineBasicBlock *ExitMBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
  MF.insert(MBBInsertPoint, ExitMBB);
  MachineInstr::MIFlag Flags = MachineInstr::FrameSetup;
  Register TargetReg = RISCV::X6;
  Register ScratchReg = RISCV::X7;

  // ScratchReg = ProbeSize
  TII->movImm(MBB, MBBI, DL, ScratchReg, ProbeSize, Flags);

  // LoopTest:
  //   SUB SP, SP, ProbeSize
  BuildMI(*LoopTestMBB, LoopTestMBB->end(), DL, TII->get(RISCV::SUB), SPReg)
      .addReg(SPReg)
      .addReg(ScratchReg)
      .setMIFlags(Flags);

  //   s[d|w] zero, 0(sp)
  BuildMI(*LoopTestMBB, LoopTestMBB->end(), DL,
          TII->get(IsRV64 ? RISCV::SD : RISCV::SW))
      .addReg(RISCV::X0)
      .addReg(SPReg)
      .addImm(0)
      .setMIFlags(Flags);

  //   BNE SP, TargetReg, LoopTest
  BuildMI(*LoopTestMBB, LoopTestMBB->end(), DL, TII->get(RISCV::BNE))
      .addReg(SPReg)
      .addReg(TargetReg)
      .addMBB(LoopTestMBB)
      .setMIFlags(Flags);

  ExitMBB->splice(ExitMBB->end(), &MBB, std::next(MBBI), MBB.end());

  LoopTestMBB->addSuccessor(ExitMBB);
  LoopTestMBB->addSuccessor(LoopTestMBB);
  MBB.addSuccessor(LoopTestMBB);
  // Update liveins.
  fullyRecomputeLiveIns({ExitMBB, LoopTestMBB});
}

void RISCViOFrameLowering::inlineStackProbe(MachineFunction &MF,
                                          MachineBasicBlock &MBB) const {
  auto Where = llvm::find_if(MBB, [](MachineInstr &MI) {
    return MI.getOpcode() == RISCV::PROBED_STACKALLOC;
  });
  if (Where != MBB.end()) {
    DebugLoc DL = MBB.findDebugLoc(Where);
    emitStackProbeInline(MF, MBB, Where, DL);
    Where->eraseFromParent();
  }
}
