#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-zhm-transform-stack"

static cl::opt<bool>
    EnableExplicitSpStore("enable-riscv-zhm-explicit-sp-store", cl::Hidden, cl::init(false),
                    cl::desc("Enable Move from sp to t0 before Stack Allocation"));

namespace {
class RISCVZhmTransformStack : public MachineFunctionPass {
public:
  static char ID;
  RISCVZhmTransformStack() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const RISCVSubtarget &STI = MF.getSubtarget<RISCVSubtarget>();
    if (!STI.hasStdExtZhm())
        return false;

    bool Modified = false;
    const RISCVInstrInfo *TII = STI.getInstrInfo();

    MachineInstr *Push;
    unsigned LargestOffset = 0;
    unsigned OriginalStackSize = 0;

    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB) {
        
        // 1. Replace `addi sp, sp, size` with custom instruction
        if (MI.getOpcode() == RISCV::ADDI) {
          MachineOperand &Dest = MI.getOperand(0);
          MachineOperand &Src = MI.getOperand(1);
          
          // Check if both destination and source are SP (X2)
          if (Dest.isReg() && Dest.getReg() == RISCV::X2 &&
              Src.isReg() && Src.getReg() == RISCV::X2 &&
              MI.getOperand(2).isImm()) {
            int64_t Imm = MI.getOperand(2).getImm();
            if (Imm < 0) {
              OriginalStackSize = -Imm;
              MI.setDesc(TII->get(RISCV::ALCI));
              MI.removeOperand(1);
              Push = &MI;
              Modified = true;
              continue; 
            } 
            if (Imm > 0) {
              MI.setDesc(TII->get(STI.is64Bit() ? RISCV::LD : RISCV::LW));
              MI.getOperand(2).setImm(0);
              Modified = true;
              continue; 
            }
            MI.getParent()->dump();
            llvm_unreachable("ADDI 0 onto Stack Pointer?!");
          }
        }

        // Assign new Stack Offsets to Load/Stores
        if (MI.mayLoadOrStore()) {
          if (MI.getNumExplicitOperands() >= 3) {
            MachineOperand &BaseRegOp = MI.getOperand(1);
            MachineOperand &CurrOffsOp = MI.getOperand(2);

            if (BaseRegOp.isReg() && BaseRegOp.getReg() == RISCV::X2 && 
                CurrOffsOp.isImm()) {
              int64_t NewOffset = OriginalStackSize - CurrOffsOp.getImm();
              CurrOffsOp.setImm(NewOffset);
              if (LargestOffset < NewOffset)
                  LargestOffset = NewOffset;
              Modified = true;
            }
          }
        }
      }
    }

    // Do this after Loop to not accidentaly shift around iterators
    if (Push) {
      if (EnableExplicitSpStore) {
        BuildMI(*Push->getParent(), Push->getIterator(), Push->getDebugLoc(), TII->get(RISCV::ADDI))
            .addReg(RISCV::X5)
            .addReg(RISCV::X2)
            .addImm(0);
      }

      //We did our own little probe and we write it here
      if (STI.is64Bit()) {
        LargestOffset += 8; //Space for sp
        if (LargestOffset % 8 > 0)
          LargestOffset += (8-(LargestOffset % 8));
      } else {
        LargestOffset += 4; //Space for sp
        if (LargestOffset % 4 > 0)
          LargestOffset += (4-(LargestOffset % 4));
      }
      Push->getOperand(1).setImm(LargestOffset);

      //Emit Store of SP after Push on offset 0
      unsigned Opcode = STI.is64Bit() ? RISCV::SD : RISCV::SW;
      BuildMI(*Push->getParent(), ++(Push->getIterator()), Push->getDebugLoc(), TII->get(Opcode))
          .addReg(EnableExplicitSpStore ? RISCV::X5 : RISCV::X2)
          .addReg(RISCV::X2)
          .addImm(0);
    }

    return Modified;
  }
};
} // end anonymous namespace

char RISCVZhmTransformStack::ID = 0;

INITIALIZE_PASS_BEGIN(RISCVZhmTransformStack, "riscv-preemit-zhm-stack",
                    "RISC-V pre-emit Zhm Stack Transformation", false, false)
INITIALIZE_PASS_END(RISCVZhmTransformStack, "riscv-preemit-zhm-stack",
                    "RISC-V pre-emit Zhm Stack Transformation", false, false)

// Function to create the pass
FunctionPass *llvm::createRISCVZhmTransformStackPass() {
  return new RISCVZhmTransformStack();
}