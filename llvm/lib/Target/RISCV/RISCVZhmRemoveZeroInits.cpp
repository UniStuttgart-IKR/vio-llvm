#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-zhm-remove-zero-initializers"

static cl::opt<bool>
    DisableRemove("disable-riscv-zhm-remove-zero-initializers", cl::Hidden, cl::init(false),
                    cl::desc("Leave all Zero Initializers like a standard RISC-V Target would handle them"));

namespace {
class RISCVZhmRemoveZeroInits : public MachineFunctionPass {
public:
  static char ID;
  RISCVZhmRemoveZeroInits() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const RISCVSubtarget &STI = MF.getSubtarget<RISCVSubtarget>();
    if (!STI.hasStdExtZhm() || DisableRemove)
        return false;

    bool Modified = false;
    const RISCVInstrInfo *TII = STI.getInstrInfo();

    
    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB) {
        if (MI.getOpcode() == RISCV::ALC || MI.getOpcode() == RISCV::ALC_D
          || MI.getOpcode() == RISCV::ALCI || MI.getOpcode() == RISCV::ALCI_D) {
          MI.all_defs();
        }
      }
    }

    return Modified;
  }
};
} // end anonymous namespace

char RISCVZhmRemoveZeroInits::ID = 0;

INITIALIZE_PASS_BEGIN(RISCVZhmRemoveZeroInits, "riscv-preemit-zhm-zero-initializers",
                    "RISC-V pre-emit Zhm Remove Zero Initializers", false, false)
INITIALIZE_PASS_END(RISCVZhmRemoveZeroInits, "riscv-preemit-zhm-zero-initializers",
                    "RISC-V pre-emit Zhm Remove Zero Initializers", false, false)

// Function to create the pass
FunctionPass *llvm::createRISCVZhmRemoveZeroInitsPass() {
  return new RISCVZhmRemoveZeroInits();
}