//===-- PPCCallingConv.cpp - Custom Calling Convention -------------------===//
//
// Implements custom argument assignment for PPC backend
//
//===----------------------------------------------------------------------===//

#include "ORISCCallingConv.h"
#include "MCTargetDesc/ORISCMCTargetDesc.h"
#include "ORISC.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/CodeGenTypes/MachineValueType.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "orisc-callingconv"

using namespace llvm;
inline bool CC_ORISC_AssignPointerIndexPair(unsigned &ValNo, MVT &ValVT,
                                        MVT &LocVT,
                                        CCValAssign::LocInfo &LocInfo,
                                        ISD::ArgFlagsTy &ArgFlags,
                                        CCState &State) {
    static const MCPhysReg PointerRegs[] = { ORISC::P10, ORISC::P11, ORISC::P12, ORISC::P3, 
                                            ORISC::P4, ORISC::P5, ORISC::P6, ORISC::P7};
    static const MCPhysReg DataRegs[] = { ORISC::D10, ORISC::D11, ORISC::D12, ORISC::D3, 
                                            ORISC::D4, ORISC::D5, ORISC::D6, ORISC::D7};

    unsigned Pi = State.getFirstUnallocated(PointerRegs);
    if (Pi >= std::size(PointerRegs))
        return false;
    MCRegister Pointer = State.AllocateReg(PointerRegs[Pi]);
    
    unsigned Di = State.getFirstUnallocated(DataRegs);
    if (Di >= std::size(DataRegs)) {
        State.DeallocateReg(Pointer);
        return false;
    }
    MCRegister Data = State.AllocateReg(DataRegs[Di]);
    if (!Data) {
        State.DeallocateReg(Pointer);
        return false;
    }
    // Register both as locations for this argument
    State.addLoc(CCValAssign::getCustomReg(ValNo, ValVT, Pointer, MVT::pointer, LocInfo));
    State.addLoc(CCValAssign::getCustomReg(ValNo, ValVT, Data, MVT::i32, LocInfo));

    LLVM_DEBUG(dbgs() << "Assigned Pointer Arg to (" << printReg(Pointer, State.getMachineFunction().getRegInfo().getTargetRegisterInfo())
                    << ", " << printReg(Data, State.getMachineFunction().getRegInfo().getTargetRegisterInfo()) << ")\n");
    return true; // handled successfully
}

#include "ORISCGenCallingConv.inc"