//===-- ORISCDisassembler.cpp - Disassembler for ORISC ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the ORISCDisassembler class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/ORISCMCTargetDesc.h"
#include "TargetInfo/ORISCTargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Endian.h"
#include <cstdint>

using namespace llvm;
using namespace llvm::MCD;

#define DEBUG_TYPE "ORISC-disassembler"

using DecodeStatus = MCDisassembler::DecodeStatus;

namespace {

class ORISCDisassembler : public MCDisassembler {

public:
  ORISCDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};
} // end anonymous namespace

static MCDisassembler *createORISCDisassembler(const Target &T,
                                                const MCSubtargetInfo &STI,
                                                MCContext &Ctx) {
  return new ORISCDisassembler(STI, Ctx);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeORISCDisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheORISCTarget(),
                                         createORISCDisassembler);
}

static DecodeStatus DecodePRRegisterClass(MCInst &Inst, uint64_t RegNo,
                                          uint64_t Address,
                                          const void *Decoder) {
  if (ORISC::P0 + RegNo > ORISC::P31)
    return MCDisassembler::Fail;

  MCPhysReg Reg = ORISC::P0 + RegNo;
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus DecodePRFRegisterClass(MCInst &Inst, uint64_t RegNo,
                                          uint64_t Address,
                                          const void *Decoder) {
  if (ORISC::P0 + RegNo == ORISC::P30)
    return MCDisassembler::Fail;

  return DecodePRRegisterClass(Inst, RegNo, Address, Decoder);
}

static DecodeStatus DecodePRFNRegisterClass(MCInst &Inst, uint64_t RegNo,
                                          uint64_t Address,
                                          const void *Decoder) {
  if (ORISC::P0 + RegNo == ORISC::P0)
    return MCDisassembler::Fail;

  return DecodePRFRegisterClass(Inst, RegNo, Address, Decoder);
}

static DecodeStatus DecodeDRRegisterClass(MCInst &Inst, uint64_t RegNo,
                                          uint64_t Address,
                                          const void *Decoder) {
  if (ORISC::D0 + RegNo > ORISC::D31)
    return MCDisassembler::Fail;

  MCPhysReg Reg = ORISC::D0 + RegNo;
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static bool tryAddingSymbolicOperand(int64_t Value, bool isBranch,
                                     uint64_t Address, uint64_t Offset,
                                     uint64_t InstSize, MCInst &MI,
                                     const void *Decoder) {
  const MCDisassembler *Dis = static_cast<const MCDisassembler *>(Decoder);
  return Dis->tryAddingSymbolicOperand(MI, Value, Address, isBranch, Offset,
                                       /*OpSize=*/0, InstSize);
}

static DecodeStatus decodeBranch12Operand(MCInst &Inst, uint64_t Imm,
                                        int64_t Address, const void *Decoder) {
    assert(isUInt<12>(Imm) && "Invalid immediate");
    if (!tryAddingSymbolicOperand(SignExtend64<12>(Imm) + 4 + Address, true,
                                  Address, 0, 4, Inst, Decoder)) {
        Inst.addOperand(MCOperand::createImm(SignExtend64<12>(Imm)));
        return MCDisassembler::Success;
    }
    return MCDisassembler::Success;
}

static DecodeStatus decodeBranch25Operand(MCInst &Inst, uint64_t Imm,
                                        int64_t Address, const void *Decoder) {
    assert(isUInt<25>(Imm) && "Invalid immediate");
    if (!tryAddingSymbolicOperand(SignExtend64<25>(Imm) + 4 + Address, true,
                                  Address, 0, 4, Inst, Decoder)) {
        Inst.addOperand(MCOperand::createImm(SignExtend64<25>(Imm)));
    }
    return MCDisassembler::Success;
}

static DecodeStatus decodeJlibIndexOperand(MCInst &Inst, uint64_t Imm,
                                      int64_t Address, const void *Decoder) {

  assert(isUInt<16>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}


/// Read two bytes from the ArrayRef and return 16 bit data sorted
/// according to the given endianness.
static DecodeStatus readInstruction32(ArrayRef<uint8_t> Bytes, uint64_t Address,
                                      uint64_t &Size, uint64_t &Insn) {
  // We want to read exactly 2 Bytes of data.
  if (Bytes.size() < 4) {
    Size = 0;
    return MCDisassembler::Fail;
  }

  Size = 4;
  Insn = (Bytes[0] << 24) | (Bytes[1] << 16) | (Bytes[2] << 8) | Bytes[3];
  return MCDisassembler::Success;
}

static DecodeStatus DecodePush(MCInst &Inst, uint32_t insn,
                                             uint64_t Address,
                                             const MCDisassembler *Decoder);

#include "ORISCGenDisassemblerTables.inc"

//All Pointer Regs Except Frame
static DecodeStatus DecodePush(MCInst &Inst, uint32_t insn, uint64_t Addr,
                                     const MCDisassembler *Decoder) {
  uint64_t FrameReg = fieldFromInstruction(insn, 20, 5);
  if (FrameReg != 30)
    return MCDisassembler::Fail;

  uint64_t Pi = fieldFromInstruction(insn, 11, 9);
  uint64_t Delta = fieldFromInstruction(insn, 0, 11);
  Inst.addOperand(MCOperand::createReg(ORISC::P30));
  Inst.addOperand(MCOperand::createImm(Pi));
  Inst.addOperand(MCOperand::createImm(Delta));
  return MCDisassembler::Success;
}

DecodeStatus ORISCDisassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                                ArrayRef<uint8_t> Bytes,
                                                uint64_t Address,
                                                raw_ostream &CS) const {
  uint64_t Insn;
  DecodeStatus Result;

  // Parse 32-bit instructions
  Result = readInstruction32(Bytes, Address, Size, Insn);
  if (Result != MCDisassembler::Fail) {
    Result = decodeInstruction(DecoderTable32, MI, Insn, Address, this, STI);
    if (Result != MCDisassembler::Fail)
      return Result;
  }

  return MCDisassembler::Fail;
}