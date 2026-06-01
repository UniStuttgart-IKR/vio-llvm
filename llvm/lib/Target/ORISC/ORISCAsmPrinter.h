//===- ORISCAsmPrinter.h - ORISC LLVM Assembly Printer --------*- C++-*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ORISC Assembly printer class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ORISC_ORISCASMPRINTER_H
#define LLVM_LIB_TARGET_ORISC_ORISCASMPRINTER_H

#include "ORISCTargetMachine.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Compiler.h"

namespace llvm {
class MCStreamer;
class MachineBasicBlock;
class MachineInstr;
class Module;
class raw_ostream;

class LLVM_LIBRARY_VISIBILITY ORISCAsmPrinter : public AsmPrinter {
  const MCSubtargetInfo *STI;

public:
    explicit ORISCAsmPrinter(TargetMachine &TM,
                            std::unique_ptr<MCStreamer> Streamer)
        : AsmPrinter(TM, std::move(Streamer)), STI(&TM.getMCSubtargetInfo()) {}

    StringRef getPassName() const override { return "ORISC Assembly Printer"; }
    
    void emitStartOfAsmFile(Module &M) override;
    void emitEndOfAsmFile(Module &M) override;

    void emitInstruction(const MachineInstr *MI) override;
    void emitLinkage(const GlobalValue *GV, MCSymbol *GVSym) const override;
    void emitFunctionEntryLabel() override;

    void printOperand(const MachineInstr *MI, int opNum, raw_ostream &O);

    bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                         const char *ExtraCode, raw_ostream &O) override;

    bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                               const char *ExtraCode, raw_ostream &OS) override;

    MCOperand lowerOperand(const MachineOperand &MO, unsigned Offset = 0) const;
    // Lower MachineInstr MI to MCInst OutMI.
    void lowerToMCInst(const MachineInstr *MI, MCInst &OutMI) const;

    bool runOnMachineFunction(MachineFunction &MF) override {
      AsmPrinter::runOnMachineFunction(MF);
      // Emit the XRay table for this function.
      return false;
    }

private:
      SmallVector<const MCSymbol *, 16> PublicFunctions;

      MCOperand lowerSymbolOperand(const MachineOperand &MO,
                                    MachineOperand::MachineOperandType MOTy,
                                    unsigned Offset) const;

      Twine startString =
      ";o   '   |   o       '     o       ' '  _|_  * .  '        .-.  '     |       \n"
      ";+'    --+--   + .             o         |           *    ( (     . --o-- * * \n"
      ";-       |   '      '    o   .    *   '   '    '   '    o  `-' '      |       \n"
      ";* '  '   *    '   Created by Leyla's beautiful LLVM Backend     . . *        \n"
      ";-   *  +   '     '     *       *  '   ' '          _|_           '   o '  '  \n"
      ";+ .           .    ' _|_ '         '      .         |   * .   '   o          \n"
      ";o   + ' ' ' *    '    |    *     *  *     . .   '                +    '    ' \n"
      "\n";

      Twine endString =
      "\n\n"
      "\n;             +    '                                      o                             +"
      "\n;      '    .                            *        +   +  o'        o o    . *        +             o"
      "\n;                   '  *                            '              .  o."
      "\n;            '        .                 *       .              o                    '  _.."
      "\n;  .     o      |            '   o                 +  .                      o       '`-. `. .   '  .'"
      "\n;        *    --o--            *        '              +      .             .   .        \  \        ."
      "\n; .o ' .   _|   |        '       *  *                +* '   *           o    *   +.      |  |    '."
      "\n;           |                   .     *                                             '    /  /     *."
      "\n;     o                    +            _|_   .       +         +   .           '    _.-`_.`     |"
      "\n;              .           *'  .         |  +                   +  + o+     o         '''      - o -'"
      "\n;            '        .  .   .       '    .        '         .'               o                  | o'"
      "\n;    .      *.       '        +.+  '  .    '  o    . '      +.    + '     . * .+  .'        . '"
      "\n;   .   'o+.        .    +     Created by Leyla's beautiful LLVM Backend         '    *     .'.   .*   ."
      "\n;        ...  .  |        .     .   o   +           .                            .  .            o"
      "\n;'    .  '     --o--    .                          ' .    .      o       '             _|_ .    *     ."
      "\n;    +   * .     |          + '           *             .                            .  |      ."
      "\n;.'       .       *               .     .   *' o  +   '           *              '                 + +"
      "\n;      '          .        '           + +         '.     .         *  .                       +      |"
      "\n;      .                                                             o         +      '     .        -+-"
      "\n;                                     .+    o'                 .       '              '               |"
      "\n;      .   o        *                                                           '"
      "\n;                +   +                        o                       '   .     .        ."
      "\n;                                                                                       +"
      "\n;            '                                                     '";
};
} // end namespace llvm

#endif /* LLVM_LIB_TARGET_ORISC_ORISCASMPRINTER_H */
