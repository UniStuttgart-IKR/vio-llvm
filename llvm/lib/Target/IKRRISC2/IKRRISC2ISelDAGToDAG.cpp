//=- IKRRISC2ISelDAGToDAG.cpp - A dag to dag inst selector for IKRRISC2 -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the IKRRISC2 target.
//
//===----------------------------------------------------------------------===//

#include "IKRRISC2.h"

#include "IKRRISC2ISelDAGToDAG.h"
#include "MCTargetDesc/IKRRISC2MCTargetDesc.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "IKRRISC2-isel"
#define PASS_NAME "IKRRISC2 DAG->DAG Pattern Instruction Selection"

char IKRRISC2DAGToDAGISelLegacy::ID;

IKRRISC2DAGToDAGISelLegacy::IKRRISC2DAGToDAGISelLegacy(
    IKRRISC2TargetMachine &TM)
    : SelectionDAGISelLegacy(ID, std::make_unique<IKRRISC2DAGToDAGISel>(TM)) {}

INITIALIZE_PASS(IKRRISC2DAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false,
                false)

void IKRRISC2DAGToDAGISel::Select(SDNode *Node) {
  // Select the default instruction.
  SelectCode(Node);
}

/// This pass converts a legalized DAG into a M68k-specific DAG,
/// ready for instruction scheduling.
FunctionPass *llvm::createIKRRISC2ISelDag(IKRRISC2TargetMachine &TM) {
  return new IKRRISC2DAGToDAGISelLegacy(TM);
}