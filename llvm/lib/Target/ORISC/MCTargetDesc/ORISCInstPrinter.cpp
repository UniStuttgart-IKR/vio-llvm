//===- ORISCInstPrinter.cpp - Convert ORISC MCInst to asm syntax --------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class prints an ORISC MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#include "ORISCInstPrinter.h"
#include "ORISCMCTargetDesc.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"
#include "ORISCGenAsmWriter.inc"

bool ORISCInstPrinter::applyTargetSpecificCLOption(StringRef Opt) {
  return false;
}

void ORISCInstPrinter::printOperand(const MCOperand &MC, raw_ostream &O) {
  if (MC.isReg())
    O << getRegisterName(MC.getReg());
  else if (MC.isImm())
    O << MC.getImm();
  else if (MC.isExpr())
    MC.getExpr()->print(O, &MAI);
  else
    report_fatal_error("Invalid operand");
}

void ORISCInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                  StringRef Annot, const MCSubtargetInfo &STI,
                                  raw_ostream &O) {
  //if (getOpcodeName(MI->getOpcode()).size() < 3)
  //  O << "\t";
  printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

void ORISCInstPrinter::printRegName(raw_ostream &O, MCRegister Reg) {
  O << getRegisterName(Reg);
}

void ORISCInstPrinter::printOperand(const MCInst *MI, unsigned OpNo, raw_ostream &O) {
  printOperand(MI->getOperand(OpNo), O);
}

void ORISCInstPrinter::printSymbol(const MCInst *MI, unsigned OpNo,
                                          raw_ostream &O) {
  if (OpNo >= MI->size()) {
    O << "<unknown>";
    return;
  }
  const MCOperand &MC = MI->getOperand(OpNo);
  if (MC.isExpr())
    MC.getExpr()->print(O, &MAI);
  else if (MC.isImm())
    O << MC.getImm();
  else {
    MI->dump();
    llvm_unreachable("Invalid Branch Operand");
  }
}

void ORISCInstPrinter::printImmBitmap(const MCInst *MI, unsigned OpNo,
                                          raw_ostream &O) {
  if (OpNo >= MI->size()) {
    O << "<unknown>";
    return;
  }
  const MCOperand &MC = MI->getOperand(OpNo);
  if (MC.isImm()) {
    O << "$" << format_hex_no_prefix(MC.getImm(), 4, true);
  }
  else
    llvm_unreachable("Invalid Bitmap Operand");
}

const char *ORISCInstPrinter::getRegisterName(MCRegister Reg) {
  return getRegisterName(Reg, ORISC::ABIRegAltName);
}