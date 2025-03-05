//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVSELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_RISCV_RISCVSELECTIONDAGINFO_H

#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

namespace llvm {

class RISCVSelectionDAGInfo : public SelectionDAGTargetInfo {
public:
  ~RISCVSelectionDAGInfo() override;

  bool isTargetMemoryOpcode(unsigned Opcode) const override;

  bool isTargetStrictFPOpcode(unsigned Opcode) const override;

  std::pair<SDValue, SDValue>
  EmitTargetCodeForMalloc(SelectionDAG &DAG, const SDLoc &DL, SDValue Chain,
                          SDValue Size) const override;

  std::pair<SDValue, SDValue>
  EmitTargetCodeForRealloc(SelectionDAG &DAG, const SDLoc &DL, SDValue Chain,
                          SDValue Ptr, SDValue Size) const override;

  std::pair<SDValue, SDValue>
  EmitTargetCodeForCalloc(SelectionDAG &DAG, const SDLoc &DL, SDValue Chain,
                          SDValue Num, SDValue Size) const override;

  bool
  EmitTargetCodeForFree(SelectionDAG &DAG, const SDLoc &DL, SDValue Chain,
                          SDValue Ptr) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_RISCV_RISCVSELECTIONDAGINFO_H
