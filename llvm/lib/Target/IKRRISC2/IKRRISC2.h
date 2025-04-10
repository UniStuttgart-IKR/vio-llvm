//===-- IKRRISC2.h - Top-level interface for IKRRISC2 representation ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the entry points for global functions defined in the
/// IKRRISC2 target library, as used by the LLVM JIT.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_IKRRISC2_IKRRISC2_H
#define LLVM_LIB_TARGET_IKRRISC2_IKRRISC2_H

namespace llvm {

class FunctionPass;
class InstructionSelector;
//class IKRRISC2RegisterBankInfo;
class IKRRISC2Subtarget;
class IKRRISC2TargetMachine;
class PassRegistry;

/// This pass converts a legalized DAG into a IKRRISC2-specific DAG, ready for
/// instruction scheduling.
FunctionPass *createIKRRISC2ISelDag(IKRRISC2TargetMachine &TM);

/// Return a Machine IR pass that expands IKRRISC2-specific pseudo
/// instructions into a sequence of actual instructions. This pass
/// must run after prologue/epilogue insertion and before lowering
/// the MachineInstr to MC.
FunctionPass *createIKRRISC2ExpandPseudoPass();

/// This pass initializes a global base register for PIC on IKRRISC2.
FunctionPass *createIKRRISC2GlobalBaseRegPass();

/// Finds sequential MOVEM instruction and collapse them into a single one. This
/// pass has to be run after all pseudo expansions and prologue/epilogue
/// emission so that all possible MOVEM are already in place.
FunctionPass *createIKRRISC2CollapseMOVEMPass();

//InstructionSelector *
//createIKRRISC2InstructionSelector(const IKRRISC2TargetMachine &, const IKRRISC2Subtarget &,
//                              const IKRRISC2RegisterBankInfo &);

void initializeIKRRISC2DAGToDAGISelLegacyPass(PassRegistry &);
//void initializeIKRRISC2ExpandPseudoPass(PassRegistry &);
//void initializeIKRRISC2GlobalBaseRegPass(PassRegistry &);
//void initializeIKRRISC2CollapseMOVEMPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_IKRRISC2_IKRRISC2_H
