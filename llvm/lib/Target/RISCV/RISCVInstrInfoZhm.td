// A 14-bit constant
def uimm12 : RISCVUImmOp<12>, ImmLeaf<XLenVT, [{
  return Imm < 4096;
}]>;

def OPC_HM : RISCVOpcode<"HM", 0b0001011>;

let Predicates = [HasStdExtZhm], hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
  class Alc_i<bits<5> rs, bits<3> funct3, string opcodestr, DAGOperand rty = GPR>
    : RVInstI<funct3, OPC_HM, (outs rty:$rd), (ins uimm12:$imm12),
              opcodestr, "$rd, ${imm12}">, Sched<[]> {
    let rs1 = rs;
  }
}

let Predicates = [HasStdExtZhm], hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
  class Alc<bits<5> rs, bits<3> funct3, string opcodestr, DAGOperand rty = GPR>
    : RVInstR<0b0000000, funct3, OPC_HM, (outs rty:$rd), (ins GPR:$rs2),
              opcodestr, "$rd, $rs2">, Sched<[]> {
    let rs1 = rs;
  }
  def ALCI   : Alc_i<0b00010, 0b010, "alci">;
  def ALCI_D : Alc_i<0b00000, 0b011, "alci.d">;
  def ALC    : Alc<0b00010, 0b000, "alc">;
  def ALC_D  : Alc<0b00000, 0b001, "alc.d">;
}

let Predicates = [HasStdExtZhm], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
  def QSZ  : RVInstR<0b0000000, 0b100, OPC_HM, (outs GPR:$rd), (ins GPR:$rs1), "qsz", "$rd, $rs1">{
    let rs2 = 0b00000;
  }
  def SEP  : RVInstR<0b1111110, 0b000, OPC_SYSTEM, (outs GPR:$rd), (ins GPR:$rs1), "sep", "$rd, $rs1">{
    let rs2 = 0b00000;
  }
  def DTP  : RVInstR<0b0111111, 0b000, OPC_SYSTEM, (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "dtp", "$rd, $rs1, $rs2">;
  def ITD  : RVInstR<0b1011111, 0b000, OPC_SYSTEM, (outs GPR:$rd), (ins GPR:$rs1), "itd", "$rd, $rs1">{
    let rs2 = 0b00000;
  }
  def BTD  : RVInstR<0b1101111, 0b000, OPC_SYSTEM, (outs GPR:$rd), (ins GPR:$rs1), "btd", "$rd, $rs1">{
    let rs2 = 0b00000;
  }
}
let Predicates = [HasStdExtZhm], hasSideEffects = 1, mayLoad = 1, mayStore = 0 in {
  def LT  : RVInstR<0b1110111, 0b000, OPC_SYSTEM, (outs GPR:$rd), (ins GPR:$rs1), "lt", "$rd, $rs1">{
    let rs2 = 0b00000;
  }
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

let Predicates = [HasStdExtZhm] in {
  def : Pat<(int_riscv_alci   uimm12:$imm12), (ALCI   uimm12:$imm12)>;
  def : Pat<(int_riscv_alci_d uimm12:$imm12), (ALCI_D uimm12:$imm12)>;
  def : Pat<(int_riscv_clr    GPR:$rs1),      (CLR    GPR:$rs1)>;
  def : Pat<(int_riscv_sep    GPR:$rs1),      (SEP    GPR:$rs1)>;
}

let Predicates = [HasStdExtZhm, IsRV32] in {
  def : Pat<(int_riscv_alc_32   GPR:$rs1),           (ALC   GPR:$rs1)>;
  def : Pat<(int_riscv_alc_d_32 GPR:$rs1),           (ALC_D GPR:$rs1)>;
  def : Pat<(int_riscv_qsz_32   GPR:$rs1),           (QSZ   GPR:$rs1)>;

  def : Pat<(int_riscv_dtp_32   GPR:$rs1, GPR:$rs2), (DTP   GPR:$rs1, GPR:$rs2)>;
  def : Pat<(int_riscv_btd_32   GPR:$rs1),           (BTD   GPR:$rs1)>;
  def : Pat<(int_riscv_itd_32   GPR:$rs1),           (ITD   GPR:$rs1)>;
  def : Pat<(int_riscv_lt_32    GPR:$rs1),           (LT    GPR:$rs1)>;
  def : Pat<(int_riscv_st_32    GPR:$rs1, GPR:$rs2), (ST    GPR:$rs1, GPR:$rs2)>;
}

let Predicates = [HasStdExtZhm, IsRV64] in {
  def : Pat<(int_riscv_alc_64   GPR:$rs1),           (ALC GPR:$rs1)>;
  def : Pat<(int_riscv_alc_d_64 GPR:$rs1),           (ALC_D GPR:$rs1)>;
  def : Pat<(int_riscv_qsz_64   GPR:$rs1),           (QSZ GPR:$rs1)>;

  def : Pat<(int_riscv_dtp_64   GPR:$rs1, GPR:$rs2), (DTP GPR:$rs1, GPR:$rs2)>;
  def : Pat<(int_riscv_btd_64   GPR:$rs1),           (BTD GPR:$rs1)>;
  def : Pat<(int_riscv_itd_64   GPR:$rs1),           (ITD GPR:$rs1)>;
  def : Pat<(int_riscv_lt_64    GPR:$rs1),           (LT  GPR:$rs1)>;
  def : Pat<(int_riscv_st_64    GPR:$rs1, GPR:$rs2), (ST  GPR:$rs1, GPR:$rs2)>;
}