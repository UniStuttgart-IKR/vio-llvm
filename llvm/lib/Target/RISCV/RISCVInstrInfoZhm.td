// A 14-bit constant
def uimm14 : RISCVUImmOp<14>, ImmLeaf<XLenVT, [{
  return Imm <= 16380;
}]>;
// Round up Imm
def RoundUpTo4 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(((N->getZExtValue()+ 3) & -4), SDLoc(N),
                                     N->getValueType(0));
}]>;

def OPC_HM : RISCVOpcode<"HM", 0b0001011>;

let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
  class Alc_i<bits<3> funct3, string opcodestr, DAGOperand rty = GPR>
    : RVInstI<funct3, OPC_HM, (outs rty:$rd), (ins uimm14:$imm14),
              opcodestr, "$rd, ${imm14}">, Sched<[]> {
    bits<14> imm14;
    let imm12 = imm14{13-2};
    let rs1 = 0b00010;
  }
}

let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
  class Alc<bits<3> funct3, string opcodestr, DAGOperand rty = GPR>
    : RVInstR<0b0000000, funct3, OPC_HM, (outs rty:$rd), (ins GPR:$rs1),
              opcodestr, "$rd, $rs1">, Sched<[]> {
    let rs2 = 0b00000;
  }
}

let Predicates = [HasStdExtZhm] in {
  def ALCI   : Alc_i<0b010, "alci">;
  def ALCI_D : Alc_i<0b011, "alci.d">;
  def ALC    : Alc<0b000, "alc">;
  def ALC_D  : Alc<0b001, "alc.d">;
  def QSZ  : RVInstR<0b0000000, 0b100, OPC_HM, (outs GPR:$rd), (ins GPR:$rs1), "qsz", "$rd, $rs1">{
    let rs2 = 0b00000;
  }
  def DTP  : RVInstR<0b0111111, 0b000, OPC_SYSTEM, (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "dtp", "$rd, $rs1, $rs2">;
  def ITD  : RVInstR<0b1011111, 0b000, OPC_SYSTEM, (outs GPR:$rd), (ins GPR:$rs1), "itd", "$rd, $rs1">{
    let rs2 = 0b00000;
  }
  def BTD  : RVInstR<0b1101111, 0b000, OPC_SYSTEM, (outs GPR:$rd), (ins GPR:$rs1), "btd", "$rd, $rs1">{
    let rs2 = 0b00000;
  }
  def LW_X  : RVInstR<0b1110111, 0b000, OPC_SYSTEM, (outs GPR:$rd), (ins GPR:$rs1), "lw.x", "$rd, $rs1">{
    let rs2 = 0b00000;
  }
  def SW_X  : RVInstR<0b1111011, 0b000, OPC_SYSTEM, (outs), (ins GPR:$rs1, GPR:$rs2), "sw.x", "$rs1, $rs2">{
    let rd = 0b00000;
  }
  def LD_X  : RVInstR<0b1111101, 0b000, OPC_SYSTEM, (outs GPR:$rd), (ins GPR:$rs1), "ld.x", "$rd, $rs1">{
    let rs2 = 0b00000;
  }
  def SD_X  : RVInstR<0b1111110, 0b000, OPC_SYSTEM, (outs), (ins GPR:$rs1, GPR:$rs2), "sd.x", "$rs1, $rs2">{
    let rd = 0b00000;
  }

  def : Pat<(int_riscv_alci uimm14:$imm14), (ALCI uimm14:$imm14)>;
  def : Pat<(int_riscv_alci_d uimm14:$imm14), (ALCI_D uimm14:$imm14)>;

  def : Pat<(iPTR (allocate (XLenVT 0),         uimm14:$imm14)),    (ALCI_D (RoundUpTo4 uimm14:$imm14))>;
  def : Pat<(iPTR (allocate (XLenVT 0),        (XLenVT GPR:$rs1))), (ALC_D   GPR:$rs1)>;
  def : Pat<(iPTR (allocate  uimm14:$imm14,    (XLenVT 0))),        (ALCI   (RoundUpTo4 uimm14:$imm14))>;
  def : Pat<(iPTR (allocate (XLenVT GPR:$rs1), (XLenVT 0))),        (ALC     GPR:$rs1)>;
  def : Pat<(iPTR (allocate (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))), (ALC    (ADD GPR:$rs1, GPR:$rs2))>;
}

let Predicates = [HasStdExtZhm, IsRV32] in {
  def : Pat<(int_riscv_alc_32 GPR:$rs1), (ALC GPR:$rs1)>;
  def : Pat<(int_riscv_alc_d_32 GPR:$rs1), (ALC_D GPR:$rs1)>;
  def : Pat<(int_riscv_qsz_32 GPR:$rs1), (QSZ GPR:$rs1)>;

  def : Pat<(int_riscv_dtp_32 GPR:$rs1, GPR:$rs2), (DTP GPR:$rs1, GPR:$rs2)>;
  def : Pat<(int_riscv_btd_32 GPR:$rs1), (BTD GPR:$rs1)>;
  def : Pat<(int_riscv_itd_32 GPR:$rs1), (ITD GPR:$rs1)>;
  def : Pat<(int_riscv_lw_x GPR:$rs1), (LW_X GPR:$rs1)>;
  def : Pat<(int_riscv_sw_x GPR:$rs1, GPR:$rs2), (SW_X GPR:$rs1, GPR:$rs2)>;
}

let Predicates = [HasStdExtZhm, IsRV64] in {
  def : Pat<(int_riscv_alc_64 GPR:$rs1), (ALC GPR:$rs1)>;
  def : Pat<(int_riscv_alc_d_64 GPR:$rs1), (ALC_D GPR:$rs1)>;
  def : Pat<(int_riscv_qsz_64 GPR:$rs1), (QSZ GPR:$rs1)>;

  def : Pat<(int_riscv_dtp_64 GPR:$rs1, GPR:$rs2), (DTP GPR:$rs1, GPR:$rs2)>;
  def : Pat<(int_riscv_btd_64 GPR:$rs1), (BTD GPR:$rs1)>;
  def : Pat<(int_riscv_itd_64 GPR:$rs1), (ITD GPR:$rs1)>;
  def : Pat<(int_riscv_ld_x GPR:$rs1), (LD_X GPR:$rs1)>;
  def : Pat<(int_riscv_sd_x GPR:$rs1, GPR:$rs2), (SD_X GPR:$rs1, GPR:$rs2)>;
}