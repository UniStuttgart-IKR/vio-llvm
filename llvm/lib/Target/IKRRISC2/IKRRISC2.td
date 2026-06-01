//===-- IKRRISC2.td - Describe the Objective-RISC Target Machine ------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

let Namespace = "IKRRISC2" in {
  
}

include "IKRRISC2RegisterInfo.td"
include "IKRRISC2CallingConv.td"
include "IKRRISC2InstrInfo.td"

def : ProcessorModel<"generic", NoSchedModel, []>;

def IKRRISC2InstrInfo : InstrInfo {
  let guessInstructionProperties = 0;
}

def IRKRISC2AsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
  bit isMCAsmWriter = 1;
}

def IKRRISC2 : Target {
  let InstructionSet = IKRRISC2InstrInfo;
//   let AssemblyParsers = [RISCVAsmParser];
  let AssemblyWriters = [IRKRISC2AsmWriter];
  let AllowRegisterRenaming = 1;
}
