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
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/raw_ostream.h"
#include <map>

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
  switch (Node->getOpcode()) {
    case ISD::SHL:
    case ISD::SRL:
    case ISD::SRA:
    case ISD::ROTL:
    case ISD::ROTR:
      selectShiftLikes(Node);
      break;

    // Select the default instruction.
    default:
      SelectCode(Node);
  }
}

void IKRRISC2DAGToDAGISel::
selectShiftLikes(SDNode *Node) {
  SDValue Op = SDValue(Node, 0);
  SDLoc DL(Op);

  SDValue Value = Node->getOperand(0);
  SDValue Shamt = Node->getOperand(1);
  SDValue OneNode = CurDAG->getConstant(1, DL, MVT::i32);

  //Expand constant shifts to multiple single shifts
  ConstantSDNode *ConstShamt = dyn_cast<ConstantSDNode>(Shamt);
  if (ConstShamt) {
    //if shamt is already const 1, shift is already legal
    if (ConstShamt->getZExtValue() == 1){
      SelectCode(Node);
      return;
    }

    SDValue Res = Value;
    //Replace multi-shift with single shifts
    for (unsigned i = 0; i < ConstShamt->getZExtValue(); ++i){
      Res = CurDAG->getNode(Node->getOpcode(), DL, Op.getValueType(), Res, OneNode);
    }
    ReplaceNode(Node, Res.getNode());
    //Convert Target Independant single shifts to machine shifts
    for (SDValue Current = Res; Current != Value; Current = Current->getOperand(0)) {
      SelectCode(Current.getNode());
    }
    return;
  }
}

/// This pass converts a legalized DAG into a M68k-specific DAG,
/// ready for instruction scheduling.
FunctionPass *llvm::createIKRRISC2ISelDag(IKRRISC2TargetMachine &TM) {
  return new IKRRISC2DAGToDAGISelLegacy(TM);
}