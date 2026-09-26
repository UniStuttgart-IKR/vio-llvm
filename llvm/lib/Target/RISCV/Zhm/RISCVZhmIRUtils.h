//===-- RISCVZhmIRUtils.h - Shared helpers for the Zhm IR passes -*- C++ -*-===//
//
// Hardware model shared by all Zhm IR passes:
//   * every object (alc, alci, frames) is 16-byte aligned;
//   * itd(p) = index of p within its object, qsz(p) = size of the object,
//     btd(p) = base of the object (all integers);
//     dtp(a, b) = integers -> pointer, unchecked (supervisor only);
//     sep(x) = 1 if x is a pointer (supervisor only);
//   * pointer +/- integer   -> pointer
//     pointer - pointer     -> integer if both are in the same object,
//                              trap otherwise
//     any other arithmetic involving a pointer is not allowed;
//   * pointers compare with beq/bne/blt/... and seq/sne; an ordered compare
//     between a pointer and an integer traps;
//   * a pointer fills a whole aligned XLEN word; a sub-XLEN load of a
//     pointer slot traps, XLEN ld/sd work on pointer and data slots;
//   * storing a pointer into .d memory traps.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVZHMIRUTILS_H
#define LLVM_LIB_TARGET_RISCV_RISCVZHMIRUTILS_H

#include "RISCVSubtarget.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
namespace RISCVZhm {

// Alignment of every Zhm object.
constexpr uint64_t AllocAlign = 16;
// Largest size alci can encode (uimm12).
constexpr uint64_t MaxAlciSize = 4095;

// Without a TargetMachine (e.g. plain `opt`), every function counts as Zhm.
inline bool isZhmFunction(const TargetMachine *TM, const Function &F) {
  return !TM || TM->getSubtarget<RISCVSubtarget>(F).hasStdExtZhm();
}

// Supervisor code may fabricate pointers with dtp. Enabled per function with
// the "riscv-zhm-supervisor" attribute or globally with -riscv-zhm-supervisor
// (defined in RISCVZhmLegalizeIR.cpp).
bool isZhmSupervisorFunction(const Function &F);

inline void error(const Instruction &I, const Twine &Msg) {
  const Function &F = *I.getFunction();
  F.getContext().diagnose(
      DiagnosticInfoUnsupported(F, "Zhm: " + Msg, I.getDebugLoc()));
}

inline bool is64Bit(const Module &M) {
  return M.getDataLayout().getPointerSizeInBits() == 64;
}

// XLEN-specific intrinsic IDs.
inline Intrinsic::ID itdID(const Module &M) {
  return is64Bit(M) ? Intrinsic::riscv_itd_64 : Intrinsic::riscv_itd_32;
}
inline Intrinsic::ID dtpID(const Module &M) {
  return is64Bit(M) ? Intrinsic::riscv_dtp_64 : Intrinsic::riscv_dtp_32;
}

// Declaration exactly as defined in IntrinsicsRISCV.td (none of the Zhm
// intrinsics is overloaded). Intrinsic::getDeclaration before LLVM 20.
inline Function *getZhmIntrinsic(Module &M, Intrinsic::ID ID) {
  return Intrinsic::getOrInsertDeclaration(&M, ID);
}

inline bool isIntrinsicCall(const Value *V, Intrinsic::ID ID) {
  const auto *CB = dyn_cast<CallBase>(V);
  return CB && CB->getIntrinsicID() == ID;
}

// Read-only queries on a pointer that return integers.
inline bool isQueryCall(const Value *V) {
  const auto *CB = dyn_cast<CallBase>(V);
  if (!CB)
    return false;
  switch (CB->getIntrinsicID()) {
  case Intrinsic::riscv_itd_32:
  case Intrinsic::riscv_itd_64:
  case Intrinsic::riscv_qsz_32:
  case Intrinsic::riscv_qsz_64:
  case Intrinsic::riscv_btd_32:
  case Intrinsic::riscv_btd_64:
  case Intrinsic::riscv_sep:
    return true;
  default:
    return false;
  }
}

// Recognises alc/alci calls; optionally reports the data-only (.d) variants.
inline bool isAllocCall(const Value *V, bool *IsDataOnly = nullptr) {
  const auto *CB = dyn_cast<CallBase>(V);
  if (!CB)
    return false;
  bool D;
  switch (CB->getIntrinsicID()) {
  case Intrinsic::riscv_alci:
  case Intrinsic::riscv_alc_32:
  case Intrinsic::riscv_alc_64:
    D = false;
    break;
  case Intrinsic::riscv_alci_d:
  case Intrinsic::riscv_alc_d_32:
  case Intrinsic::riscv_alc_d_64:
    D = true;
    break;
  default:
    return false;
  }
  if (IsDataOnly)
    *IsDataOnly = D;
  return true;
}

// Masks of address bits that the 16-byte object alignment makes derivable
// from itd: 1, 3, 7, 15.
inline bool isLowBitMask(uint64_t M) {
  return M != 0 && M < AllocAlign && isPowerOf2_64(M + 1);
}

// True if values of this type contain no pointers.
inline bool isDataOnly(Type *Ty) {
  if (auto *STy = dyn_cast<StructType>(Ty))
    return all_of(STy->elements(), [](Type *E) { return isDataOnly(E); });
  if (auto *ATy = dyn_cast<ArrayType>(Ty))
    return isDataOnly(ATy->getElementType());
  if (auto *VTy = dyn_cast<VectorType>(Ty))
    return isDataOnly(VTy->getElementType());
  return !Ty->isPointerTy();
}

// Memory that provably never holds pointers: .d allocations and constant
// globals with a pointer-free type (e.g. string literals).
inline bool isKnownDataOnlyMemory(const Value *Ptr) {
  const Value *Obj = getUnderlyingObject(Ptr);
  bool DataOnly = false;
  if (isAllocCall(Obj, &DataOnly))
    return DataOnly;
  if (const auto *GV = dyn_cast<GlobalVariable>(Obj))
    return GV->isConstant() && isDataOnly(GV->getValueType());
  return false;
}

// True if V holds a pointer in hardware: pointer-typed (not null), or an
// integer derived from ptrtoint. p - q is an integer, p +/- n a pointer;
// other operations propagate "pointer" so the verifier can report them.
inline bool isPointerDerived(const Value *V, unsigned Depth = 0) {
  if (V->getType()->isPtrOrPtrVectorTy())
    return !isa<ConstantPointerNull>(V);
  const auto *Op = dyn_cast<Operator>(V);
  if (!Op || Depth > 8)
    return false;
  switch (Op->getOpcode()) {
  case Instruction::PtrToInt:
    return true;
  case Instruction::Sub:
    return isPointerDerived(Op->getOperand(0), Depth + 1) !=
           isPointerDerived(Op->getOperand(1), Depth + 1);
  case Instruction::Add:
  case Instruction::And:
  case Instruction::Or:
  case Instruction::Xor:
  case Instruction::Mul:
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::URem:
  case Instruction::SRem:
  case Instruction::Shl:
  case Instruction::LShr:
  case Instruction::AShr:
    return isPointerDerived(Op->getOperand(0), Depth + 1) ||
           isPointerDerived(Op->getOperand(1), Depth + 1);
  case Instruction::Trunc:
  case Instruction::ZExt:
  case Instruction::SExt:
  case Instruction::Freeze:
    return isPointerDerived(Op->getOperand(0), Depth + 1);
  default:
    return false;
  }
}

// --- Fabricated pointers: integers used as pointers -----------------------

// inttoptr of a value that holds no pointer, seen through GEPs.
// With OnlyConstants, only integer constants count (anything else may carry
// a pointer that was stored as an integer).
inline bool isFabricatedPointer(const Value *P, bool OnlyConstants) {
  for (unsigned Depth = 0; Depth < 8; ++Depth) {
    if (const auto *GEP = dyn_cast<GEPOperator>(P)) {
      P = GEP->getPointerOperand();
      continue;
    }
    const auto *Op = dyn_cast<Operator>(P);
    if (!Op || Op->getOpcode() != Instruction::IntToPtr)
      return false;
    const Value *Int = Op->getOperand(0);
    if (isPointerDerived(Int))
      return false;
    if (const auto *C = dyn_cast<Constant>(Int))
      return !C->isNullValue();
    return !OnlyConstants;
  }
  return false;
}

// True if the use is the address of a memory access or the callee of a call.
inline bool isAddressUse(const Use &U) {
  const User *Usr = U.getUser();
  if (const auto *LI = dyn_cast<LoadInst>(Usr))
    return U.getOperandNo() == LI->getPointerOperandIndex();
  if (isa<StoreInst>(Usr))
    return U.getOperandNo() == StoreInst::getPointerOperandIndex();
  if (const auto *RMW = dyn_cast<AtomicRMWInst>(Usr))
    return U.getOperandNo() == RMW->getPointerOperandIndex();
  if (const auto *CX = dyn_cast<AtomicCmpXchgInst>(Usr))
    return U.getOperandNo() == CX->getPointerOperandIndex();
  if (isa<AnyMemIntrinsic>(Usr))
    return U.get()->getType()->isPointerTy();
  if (const auto *CB = dyn_cast<CallBase>(Usr))
    return CB->isCallee(&U);
  return false;
}

// True if the pointer is dereferenced, directly or through GEPs.
inline bool isDereferenced(const Value *P, unsigned Depth = 0) {
  if (Depth > 8)
    return false;
  for (const Use &U : P->uses()) {
    if (isa<GEPOperator>(U.getUser())) {
      if (isDereferenced(U.getUser(), Depth + 1))
        return true;
      continue;
    }
    if (isAddressUse(U))
      return true;
  }
  return false;
}

} // end namespace RISCVZhm
} // end namespace llvm

#endif