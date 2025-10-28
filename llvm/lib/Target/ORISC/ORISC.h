//===-- ORISC.h - Top-level interface for ORISC representation ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the entry points for global functions defined in the
/// ORISC target library, as used by the LLVM JIT.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ORISC_ORISC_H
#define LLVM_LIB_TARGET_ORISC_ORISC_H

#include "llvm/Pass.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

class FunctionPass;
class InstructionSelector;
//class ORISCRegisterBankInfo;
class ORISCSubtarget;
class ORISCTargetMachine;
class PassRegistry;

/// This pass converts a legalized DAG into a ORISC-specific DAG, ready for
/// instruction scheduling.
FunctionPass *createORISCISelDag(ORISCTargetMachine &TM);

/// Return a Machine IR pass that expands ORISC-specific pseudo
/// instructions into a sequence of actual instructions. This pass
/// must run after prologue/epilogue insertion and before lowering
/// the MachineInstr to MC.
FunctionPass *createORISCExpandPseudoPass();

/// This pass initializes a global base register for PIC on ORISC.
FunctionPass *createORISCGlobalBaseRegPass();

/// Finds sequential MOVEM instruction and collapse them into a single one. This
/// pass has to be run after all pseudo expansions and prologue/epilogue
/// emission so that all possible MOVEM are already in place.
FunctionPass *createORISCCollapseMOVEMPass();

//InstructionSelector *
//createORISCInstructionSelector(const ORISCTargetMachine &, const ORISCSubtarget &,
//                              const ORISCRegisterBankInfo &);

void initializeORISCDAGToDAGISelLegacyPass(PassRegistry &);
//void initializeORISCExpandPseudoPass(PassRegistry &);
//void initializeORISCGlobalBaseRegPass(PassRegistry &);
//void initializeORISCCollapseMOVEMPass(PassRegistry &);

ModulePass *createORISCShrinkPointerIndicesPass();
void initializeORISCShrinkPointerIndicesPass(PassRegistry &);
} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISC_H
