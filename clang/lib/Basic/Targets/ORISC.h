//===--- ORISC.h - Declare ORISC target feature support -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares ORISC TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_ORISC_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_ORISC_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

#include "clang/Basic/Builtins.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetBuiltins.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY ORISCTargetInfo : public TargetInfo {
  static const Builtin::Info BuiltinInfo[];

protected:
  std::string CPU;

public:
  ORISCTargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    // no big-endianess support yet
    BigEndian = true;
    NoAsmVariants = true;
    LongLongAlign = 64;
    SuitableAlign = 32;
    DoubleAlign = LongDoubleAlign = 64;
    SizeType = UnsignedInt;
    PtrDiffType = SignedInt;
    IntPtrType = SignedInt;
    WCharType = SignedInt;
    WIntType = UnsignedInt;
    UseZeroLengthBitfieldAlignment = true;
    MaxAtomicPromoteWidth = MaxAtomicInlineWidth = 32;
    resetDataLayout("E-p:32:32-i32:32:32-i16:16:16-i8:8:8-n32");
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override {
    return {};
  }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  std::string_view getClobbers() const override { return ""; }

  ArrayRef<const char *> getGCCRegNames() const override {
    static const char *const GCCRegNames[] = {
        // General register name
        "zero", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "d10",
        "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18", "d19", "d20",
        "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31",
        "zero", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10",
        "p11", "p12", "p13", "p14", "p15", "p16", "p17", "p18", "p19", "p20",
        "p21", "p22", "p23", "p24", "p25", "p26", "p27",
        // Special register name
        "cnst", "ctxt", "frame", "rpc"};
    return llvm::ArrayRef(GCCRegNames);
  }

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    return {};
  }
  
  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override {
    switch (*Name) {
    default:
      return false;
    case 'a':
      Info.setAllowsRegister();
      return true;
    }
    return false;
  }

  int getEHDataRegisterNumber(unsigned RegNo) const override {
    return (RegNo < 2) ? RegNo : -1;
  }

  bool isValidCPUName(StringRef Name) const override {
    return llvm::StringSwitch<bool>(Name).Case("generic", true).Default(false);
  }

  bool setCPU(const std::string &Name) override {
    CPU = Name;
    return isValidCPUName(Name);
  }
};

} // namespace targets
} // namespace clang
#endif // LLVM_CLANG_LIB_BASIC_TARGETS_ORISC_H
