//===-- ObjectiveRISCISelLowering.h - ObjectiveRISC DAG Lowering Interface ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that ObjectiveRISC uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_OBJECTIVERISC_OBJECTIVERISCISELLOWERING_H
#define LLVM_LIB_TARGET_OBJECTIVERISC_OBJECTIVERISCISELLOWERING_H


#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

	namespace ORISCISD {
	enum NodeType : unsigned {

	};
	}

	class ObjectiveRISCTargetLowering : public TargetLowering {
	public:
		ObjectiveRISCTargetLowering(const TargetMachine &TM, const ObjectiveRISCSubtarget &STI);
		SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
	};
} // end namespace llvm

#endif // LLVM_LIB_TARGET_OBJECTIVERISC_OBJECTIVERISCISELLOWERING_H