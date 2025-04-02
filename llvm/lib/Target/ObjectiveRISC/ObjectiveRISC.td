//===-- ObjectiveRISC.td - Describe the Objective-RISC Target Machine ------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

let Namespace = "ObjectiveRISC" in {
	def ptr: PtrValueType<i32, 0>;
}

include "ObjectiveRISCRegisterInfo.td"
include "ObjectiveRISCCallingConv.td"
include "ObjectiveRISCInstrInfo.td"


def ObjectiveRISCInstrInfo : InstrInfo {
  let guessInstructionProperties = 0;
}

def ObjectiveRISC : Target {
   let InstructionSet = ObjectiveRISCInstrInfo;
//   let AssemblyParsers = [RISCVAsmParser];
//   let AssemblyWriters = [RISCVAsmWriter];
//   let AllowRegisterRenaming = 1;
}
