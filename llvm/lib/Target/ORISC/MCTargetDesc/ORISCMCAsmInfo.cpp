//===-- ORISCMCAsmInfo.cpp - ORISC Asm properties ------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the ORISCMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "ORISCMCAsmInfo.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void ORISCMCAsmInfo::anchor() {}

ORISCMCAsmInfo::ORISCMCAsmInfo(const Triple &TT) {
  CodePointerSize = CalleeSaveStackSlotSize = 4;
  AlignmentIsInBytes = false;
  Data8bitsDirective = "\t.byte\t";
  Data16bitsDirective = "\t.half\t";
  Data32bitsDirective = "\t.word\t";
  Data64bitsDirective = "\t.dword\t";
  ZeroDirective = "\t.space\t";
  HasIdentDirective = false;
  IsLittleEndian = false;
  HasFunctionAlignment = false;
  HasDotTypeDotSizeDirective = false;
  HasSingleParameterDotFile = false;
  UseMotorolaIntegers = true;
  CommentString = ";";
  SupportsDebugInformation = false;
  //DwarfRegNumForCFI = true;
  //ExceptionsType = ExceptionHandling::DwarfCFI;
}
