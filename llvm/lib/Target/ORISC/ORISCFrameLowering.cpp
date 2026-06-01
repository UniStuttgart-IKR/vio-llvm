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
#include "MCTargetDesc/ORISCMCTargetDesc.h"
#include "ORISCInstrInfo.h"
#include "ORISCSubtarget.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include <cassert>
#include <cstdint>
#include <sys/types.h>

using namespace llvm;

static int64_t roundUpToRegSize(u_int16_t RegSizeBytes, int64_t ToRound) {
  int64_t Rest = ToRound % RegSizeBytes;
  if (Rest != 0)
    return ToRound-Rest+RegSizeBytes;
  return ToRound;
}

void ORISCFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  DebugLoc DL;
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const ORISCInstrInfo *TII = STI.getInstrInfo();
  const ORISCRegisterInfo *TRI = STI.getRegisterInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();

  if (MFI.getStackSize() == 0 && MFI.getCalleeSavedInfo().empty())
    return;

  unsigned PointersOnStack = 0;
  unsigned PrimitivesOnStack = 0;

  for (auto CS : MFI.getCalleeSavedInfo()) {
    if (ORISC::PRRegClass.contains(CS.getReg())) {
      if (CS.getReg() == ORISC::P31)
        continue;
      PointersOnStack += 1;
    } else {
      PrimitivesOnStack += 4;
    }
  }

  for (int I = 0; I < MFI.getObjectIndexEnd(); ++I) {
    const AllocaInst *Alc = MFI.getObjectAllocation(I);
    if (Alc) {
      // 1: Allocating Pointer on Stack
      if (Alc->getAllocatedType()->isPointerTy()) {
        PointersOnStack += 1;
      // 2: Allocating Array on Stack
      } else if (Alc->getAllocatedType()->isArrayTy()) {
        // 2.1: Array of Pointers
        if (Alc->getAllocatedType()->getArrayElementType()->isPointerTy()) {
          PointersOnStack += Alc->getAllocatedType()->getArrayNumElements();
        // 2.2: Array of Primitives
        } else {
          assert(!Alc->getAllocatedType()->getArrayElementType()->isAggregateType() && "Arrays of Structs not handled yet");
          PrimitivesOnStack += roundUpToRegSize(4, MFI.getObjectSize(I));
        }
      // 3: Allocating Struct on Stack
      } else if (Alc->getAllocatedType()->isAggregateType()) {
        //We assume that Mixed Structs have been eliminated by an IR Pass
        if (Alc->getAllocatedType()->getStructElementType(0)->isPointerTy()) {
          PointersOnStack += Alc->getAllocatedType()->getStructNumElements();
        } else {
          PrimitivesOnStack += roundUpToRegSize(4, MFI.getObjectSize(I));
        }
      // 4: Only Primitives would be left here(?)
      } else {
        PrimitivesOnStack += roundUpToRegSize(4, MFI.getObjectSize(I));
      }
    }
  }
  
  unsigned int Opcode = MFI.hasCalls() ? ORISC::PUSH : ORISC::PUSHT;
  BuildMI(MBB, MBBI, DL, TII->get(Opcode), TRI->getFrameRegister(MF))
      .addImm(PointersOnStack)
      .addImm(PrimitivesOnStack)
      .setMIFlags(MachineInstr::FrameSetup);
  return;
}
                                      
void ORISCFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  DebugLoc DL;
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const ORISCInstrInfo *TII = STI.getInstrInfo();
  const ORISCRegisterInfo *TRI = STI.getRegisterInfo();

  if (MFI.getStackSize() == 0 && MFI.getCalleeSavedInfo().empty())
    return;

  MachineBasicBlock::iterator MBBI = MBB.end();
  if (!MBB.empty()) {
    MBBI = MBB.getLastNonDebugInstr();
    if (MBBI != MBB.end())
      DL = MBBI->getDebugLoc();

    MBBI = MBB.getFirstTerminator();
  }

  BuildMI(MBB, MBBI, DL, TII->get(ORISC::POP), TRI->getFrameRegister(MF))
      .setMIFlags(MachineInstr::FrameDestroy);
}
                                      
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
  uint64_t Offset = MFI.getObjectOffset(FI);

  return StackOffset::getFixed(Offset);
}

void
ORISCFrameLowering::processFunctionBeforeFrameIndicesReplaced(MachineFunction &MF,
                                                              RegScavenger *RS) const {
  DebugLoc DL;
  MachineFrameInfo &MFI = MF.getFrameInfo();

  bool SpillHandled[64] = { false };

  unsigned PointerCounter = 0;
  unsigned PrimitivesCounter = 0;

  for (auto CS : MFI.getCalleeSavedInfo()) {
    if (ORISC::PRRegClass.contains(CS.getReg())) {
      if (CS.getReg() == ORISC::P31)
        continue;
      MFI.setObjectOffset(CS.getFrameIdx(), PointerCounter);
      if (CS.getFrameIdx() >= 0)
        SpillHandled[CS.getFrameIdx()] = true;
      PointerCounter += 4;
    } else {
      MFI.setObjectOffset(CS.getFrameIdx(), PrimitivesCounter);
      if (CS.getFrameIdx() >= 0)
        SpillHandled[CS.getFrameIdx()] = true;
      PrimitivesCounter += 4;
    }
  }

  for (int I = 0; I < MFI.getObjectIndexEnd(); ++I) {
    if (MFI.isSpillSlotObjectIndex(I) && SpillHandled[I])
      continue;

    const AllocaInst *Alc = MFI.getObjectAllocation(I);
    if (Alc) {
      // 1: Allocating Pointer on Stack
      if (Alc->getAllocatedType()->isPointerTy()) {
        MFI.setObjectOffset(I, PointerCounter);
        PointerCounter += 4;
      // 2: Allocating Array on Stack
      } else if (Alc->getAllocatedType()->isArrayTy()) {
        // 2.1: Array of Pointers
        if (Alc->getAllocatedType()->getArrayElementType()->isPointerTy()) {
          MFI.setObjectOffset(I, PointerCounter);
          PointerCounter += (Alc->getAllocatedType()->getArrayNumElements()*4);
        // 2.2: Array of Primitives
        } else {
          assert(!Alc->getAllocatedType()->getArrayElementType()->isAggregateType() && "Arrays of Structs not handled yet");
          MFI.setObjectOffset(I, PrimitivesCounter);
          PrimitivesCounter += roundUpToRegSize(4, MFI.getObjectSize(I));
        }
      // 3: Allocating Struct on Stack
      } else if (Alc->getAllocatedType()->isAggregateType()) {
        //We assume that Mixed Structs have been eliminated by an IR Pass
        if (Alc->getAllocatedType()->getStructElementType(0)->isPointerTy()) {
          MFI.setObjectOffset(I, PointerCounter);
          PointerCounter += (Alc->getAllocatedType()->getStructNumElements()*4);
        } else {
          MFI.setObjectOffset(I, PrimitivesCounter);
          PrimitivesCounter += roundUpToRegSize(4, MFI.getObjectSize(I));
        }
      // 4: Only Primitives would be left here(?)
      } else {
        MFI.setObjectOffset(I, PrimitivesCounter);
        PrimitivesCounter += roundUpToRegSize(4, MFI.getObjectSize(I));
      }
    }
  }
}