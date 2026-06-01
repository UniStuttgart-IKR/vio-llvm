//=- ORISCISelDAGToDAG.cpp - A dag to dag inst selector for ORISC -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the ORISC target.
//
//===----------------------------------------------------------------------===//

#include "ORISC.h"

#include "ORISCISelDAGToDAG.h"
#include "MCTargetDesc/ORISCMCTargetDesc.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/raw_ostream.h"
#include <map>

using namespace llvm;

#define DEBUG_TYPE "ORISC-isel"
#define PASS_NAME "ORISC DAG->DAG Pattern Instruction Selection"

char ORISCDAGToDAGISelLegacy::ID;

ORISCDAGToDAGISelLegacy::ORISCDAGToDAGISelLegacy(
    ORISCTargetMachine &TM)
    : SelectionDAGISelLegacy(ID, std::make_unique<ORISCDAGToDAGISel>(TM)) {}

INITIALIZE_PASS(ORISCDAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false,
                false)

void ORISCDAGToDAGISel::Select(SDNode *Node) {
  switch (Node->getOpcode()) {
    
    // Select the default instruction.
    default:
      SelectCode(Node);
  }
}
/// This pass converts a legalized DAG into a M68k-specific DAG,
/// ready for instruction scheduling.
FunctionPass *llvm::createORISCISelDag(ORISCTargetMachine &TM) {
  return new ORISCDAGToDAGISelLegacy(TM);
}