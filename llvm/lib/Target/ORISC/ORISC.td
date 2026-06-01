//===-- ORISC.td - Describe the Objective-RISC Target Machine ------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

let Namespace = "ORISC" in {
  // def orisc_pointer : PtrValueType <i32, 0>;
}

def orisc_pointer : PtrValueType <i32, 0>;

include "ORISCRegisterInfo.td"
include "ORISCCallingConv.td"
include "ORISCInstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<PR>;

def : ProcessorModel<"generic", NoSchedModel, []>;

def ORISCInstrInfo : InstrInfo {
  let guessInstructionProperties = 0;
}

def ORISC2AsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
  bit isMCAsmWriter = 1;
}

def ORISC : Target {
  let InstructionSet = ORISCInstrInfo;
//   let AssemblyParsers = [RISCVAsmParser];
  let AssemblyWriters = [ORISC2AsmWriter];
  let AllowRegisterRenaming = 1;
}
