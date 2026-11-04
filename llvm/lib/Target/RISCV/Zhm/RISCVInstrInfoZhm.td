//===-- RISCVInstrInfoZhm.td - Zhm instructions -------------*- tablegen -*-===//
//
// Zhm: hardware-managed objects. alc/alci allocate zeroed, 16-byte aligned
// objects; qsz queries a pointer; bso branches if two pointers reference
// the same object. itd/btd/sep/dtp/lt/st/clr are privileged (spec V0.3.2);
// the itd intrinsic is selected as an unprivileged andi/sub pair.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Operands
//===----------------------------------------------------------------------===//

// Size operand of alci / alci.d: unsigned 12 bit.
// The intrinsics take the size as an i32 ImmArg on RV32 and RV64. An ImmArg
// reaches ISel as a TargetConstant of type i32 (type legalization leaves
// target constants alone), so the operand is i32-typed on both XLENs.
def zhm_uimm12 : RISCVUImmOp<12> {
  let Type = i32;
  let MCOperandPredicate = [{
    int64_t Imm;
    if (!MCOp.evaluateAsConstantImm(Imm))
      return false;
    return isUInt<12>(Imm);
  }];
}

// Matches the ImmArg: `timm`, not `imm`.
def zhm_alci_size : TImmLeaf<i32, [{ return isUInt<12>(Imm); }]>;

//===----------------------------------------------------------------------===//
// Instruction formats
//===----------------------------------------------------------------------===//

def OPC_HM : RISCVOpcode<"HM", 0b0001011>;

class ZhmAlcI<bits<5> rs, bits<3> funct3, string opcodestr>
    : RVInstI<funct3, OPC_HM, (outs GPR:$rd), (ins zhm_uimm12:$imm12),
              opcodestr, "$rd, $imm12">,
      Sched<[]> {
  let rs1 = rs;
}

class ZhmAlc<bits<5> rs, bits<3> funct3, string opcodestr>
    : RVInstR<0b0000000, funct3, OPC_HM, (outs GPR:$rd), (ins GPR:$rs2),
              opcodestr, "$rd, $rs2">,
      Sched<[]> {
  let rs1 = rs;
}

// rd <- f(rs1), rs2 field fixed to zero.
class ZhmUnary<bits<7> funct7, bits<3> funct3, RISCVOpcode opcode,
               string opcodestr>
    : RVInstR<funct7, funct3, opcode, (outs GPR:$rd), (ins GPR:$rs1),
              opcodestr, "$rd, $rs1">,
      Sched<[]> {
  let rs2 = 0b00000;
}

//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

// Allocation: never merged, removed or moved across other side effects.
let Predicates = [HasStdExtZhm], hasSideEffects = 1, mayLoad = 0,
    mayStore = 0 in {
  def ALCI   : ZhmAlcI<0b00010, 0b010, "alci">;
  def ALCI_D : ZhmAlcI<0b00000, 0b011, "alci.d">;
  def ALC    : ZhmAlc<0b00010, 0b000, "alc">;
  def ALC_D  : ZhmAlc<0b00000, 0b001, "alc.d">;
}

// Object queries trap on operands that are not pointers. hasSideEffects
// keeps MachineLICM and friends from hoisting them above a guard such as
// `if (p) n = qsz(p);`.
let Predicates = [HasStdExtZhm], hasSideEffects = 1, mayLoad = 0,
    mayStore = 0 in {
  def QSZ : ZhmUnary<0b0000000, 0b100, OPC_HM, "qsz">;
  def ITD : ZhmUnary<0b1011111, 0b000, OPC_SYSTEM, "itd">;
  def BTD : ZhmUnary<0b1101111, 0b000, OPC_SYSTEM, "btd">;
}

// Supervisor instructions; they do not trap on their operands.
let Predicates = [HasStdExtZhm], hasSideEffects = 0, mayLoad = 0,
    mayStore = 0 in {
  def SEP : ZhmUnary<0b1111110, 0b000, OPC_SYSTEM, "sep">;
  def DTP : RVInstR<0b0111111, 0b000, OPC_SYSTEM, (outs GPR:$rd),
                    (ins GPR:$rs1, GPR:$rs2), "dtp", "$rd, $rs1, $rs2">,
            Sched<[]>;
}

// Supervisor instructions; they do trap on their operands.
let Predicates = [HasStdExtZhm], hasSideEffects = 1, mayLoad = 1, mayStore = 0 in {
  def LT  : ZhmUnary<0b1110111, 0b000, OPC_SYSTEM, "lt">;
}
let Predicates = [HasStdExtZhm], hasSideEffects = 1, mayLoad = 0, mayStore = 1 in {
  def ST  : RVInstR<0b1111011, 0b000, OPC_SYSTEM, (outs), (ins GPR:$rs1, GPR:$rs2), "st", "$rs1, $rs2">{
    let rd = 0b00000;
  }
  def CLR  : RVInstR<0b1111101, 0b000, OPC_SYSTEM, (outs), (ins GPR:$rs1), "clr", "$rs1">{
    let rd = 0b00000;
    let rs2 = 0b00000;
  }
}

// lga rd, sym on Zhm: load the GOT entry of sym relative to the GOT pointer
// in gp (spec V0.3.2), for any GOT size. Expanded by the MC code emitter to
//   lui rd, 0 ; add rd, gp, rd ; l[wd] rd, 0(rd)
// with one R_RISCV_GOT_OFF (+ R_RISCV_RELAX) on the lui; lld relaxes it to
// `l[wd] rd, off(gp)` when the offset fits 12 bits. Printed as `lga`; the asm
// parser turns `lga` and `la` into this pseudo on Zhm. Reads gp (Uses), which
// RISCVZhmGOTPointer makes valid in front of it. Not rematerialisable: it
// depends on gp.
let Predicates = [HasStdExtZhm], hasSideEffects = 0, mayLoad = 1,
    mayStore = 0, Size = 12, isCodeGenOnly = 1, Uses = [X3] in
def PseudoZhmLGA : Pseudo<(outs GPR:$dst), (ins bare_symbol:$src), [],
                          "lga", "$dst, $src">;

// bso rs1, rs2, offset: branch if both pointers reference the same object
// (spec V0.3.2: the only identity check). B-type in custom-0, funct3 111;
// encoded and relocated like beq (fixup_riscv_branch). Traps if an operand
// is a primitive. Assembler only: the compiler does not select it.
let Predicates = [HasStdExtZhm], isBranch = 1, isTerminator = 1,
    hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
def BSO : RVInstB<0b111, OPC_HM, (outs),
                  (ins GPR:$rs1, GPR:$rs2, bare_simm13_lsb0:$imm12),
                  "bso", "$rs1, $rs2, $imm12">,
          Sched<[WriteJmp, ReadJmp, ReadJmp]>;

//===----------------------------------------------------------------------===//
// Intrinsic patterns
//===----------------------------------------------------------------------===//

// The index of a pointer without the privileged itd (spec V0.3.2, runtime
// functions): a logic operation with a primitive only changes the index, so
//   andi t, p, 0     t = the pointer's base (same object, index 0)
//   sub  rd, p, t    two pointers into one object: rd = index (primitive)
// For a primitive p this yields p itself instead of trapping. The patterns
// produce machine nodes directly, so no DAG combine sees (and p, 0) and
// folds it to the constant 0.

// alc/alci are IntrHasSideEffects, i.e. chained intrinsics; TableGen matches
// the chain automatically from the intrinsic definition.
let Predicates = [HasStdExtZhm] in {
  def : Pat<(int_riscv_alci   zhm_alci_size:$sz), (ALCI   zhm_alci_size:$sz)>;
  def : Pat<(int_riscv_alci_d zhm_alci_size:$sz), (ALCI_D zhm_alci_size:$sz)>;
  def : Pat<(int_riscv_clr    GPR:$rs1),          (CLR    GPR:$rs1)>;
  def : Pat<(int_riscv_sep    GPR:$rs1),          (SEP   GPR:$rs1)>;
}

let Predicates = [HasStdExtZhm, IsRV32] in {
  def : Pat<(int_riscv_alc_32   GPR:$rs2), (ALC   GPR:$rs2)>;
  def : Pat<(int_riscv_alc_d_32 GPR:$rs2), (ALC_D GPR:$rs2)>;
  def : Pat<(int_riscv_qsz_32   GPR:$rs1), (QSZ   GPR:$rs1)>;
  def : Pat<(int_riscv_itd_32   GPR:$rs1), (SUB GPR:$rs1, (ANDI GPR:$rs1, 0))>;
  def : Pat<(int_riscv_btd_32   GPR:$rs1), (BTD   GPR:$rs1)>;
  def : Pat<(int_riscv_lt_32    GPR:$rs1), (LT    GPR:$rs1)>;
  def : Pat<(int_riscv_st_32    GPR:$rs1, GPR:$rs2), (ST  GPR:$rs1, GPR:$rs2)>;
  def : Pat<(int_riscv_dtp_32   GPR:$rs1, GPR:$rs2), (DTP GPR:$rs1, GPR:$rs2)>;
}

let Predicates = [HasStdExtZhm, IsRV64] in {
  def : Pat<(int_riscv_alc_64   GPR:$rs2), (ALC   GPR:$rs2)>;
  def : Pat<(int_riscv_alc_d_64 GPR:$rs2), (ALC_D GPR:$rs2)>;
  def : Pat<(int_riscv_qsz_64   GPR:$rs1), (QSZ   GPR:$rs1)>;
  def : Pat<(int_riscv_itd_64   GPR:$rs1), (SUB GPR:$rs1, (ANDI GPR:$rs1, 0))>;
  def : Pat<(int_riscv_btd_64   GPR:$rs1), (BTD   GPR:$rs1)>;
  def : Pat<(int_riscv_lt_64    GPR:$rs1), (LT    GPR:$rs1)>;
  def : Pat<(int_riscv_st_64    GPR:$rs1, GPR:$rs2), (ST  GPR:$rs1, GPR:$rs2)>;
  def : Pat<(int_riscv_dtp_64   GPR:$rs1, GPR:$rs2), (DTP GPR:$rs1, GPR:$rs2)>;
}

//===----------------------------------------------------------------------===//
// Equality compares as values
//===----------------------------------------------------------------------===//
//
// SelectionDAG sees pointers only as XLenVT integers. The base patterns
// lower
//   (seteq a, b) -> sltiu (xor a, b), 1   xor of two pointers: traps
//   (seteq a, 0) -> sltiu a, 1            ordered ptr/int compare: traps
// Select the beq/bne-based select pseudo instead (expanded to a small branch
// diamond by its custom inserter). Branches themselves already use beq/bne.
//
// The third operand is RISCVCC::CondCode: COND_EQ = 0, COND_NE = 1
// (RISCVInstrInfo.h); check it against your tree.

let Predicates = [HasStdExtZhm], AddedComplexity = 100 in {
  def : Pat<(XLenVT (seteq (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))),
            (Select_GPR_Using_CC_GPR GPR:$rs1, GPR:$rs2, (XLenVT 0),
                                    (ADDI (XLenVT X0), 1), (ADDI (XLenVT X0), 0))>;
  def : Pat<(XLenVT (setne (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))),
            (Select_GPR_Using_CC_GPR GPR:$rs1, GPR:$rs2, (XLenVT 1),
                                    (ADDI (XLenVT X0), 1), (ADDI (XLenVT X0), 0))>;
  def : Pat<(XLenVT (seteq (XLenVT GPR:$rs1), 0)),
            (Select_GPR_Using_CC_GPR GPR:$rs1, (XLenVT X0), (XLenVT 0),
                                    (ADDI (XLenVT X0), 1), (ADDI (XLenVT X0), 0))>;
  def : Pat<(XLenVT (setne (XLenVT GPR:$rs1), 0)),
            (Select_GPR_Using_CC_GPR GPR:$rs1, (XLenVT X0), (XLenVT 1),
                                    (ADDI (XLenVT X0), 1), (ADDI (XLenVT X0), 0))>;
} // Predicates = [HasStdExtZhm], AddedComplexity = 100
