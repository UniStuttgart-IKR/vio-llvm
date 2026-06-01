#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"

using namespace llvm;

static bool parseNestedNames(StringRef Mangled,
                             SmallVectorImpl<StringRef> &Out) {
  if (!Mangled.starts_with("_ZN"))
    return false;

  Mangled = Mangled.drop_front(3); // _ZN

  while (!Mangled.empty() && isdigit(Mangled[0])) {
    size_t Len = 0;
    size_t I = 0;

    while (I < Mangled.size() && isdigit(Mangled[I])) {
      Len = Len * 10 + (Mangled[I] - '0');
      ++I;
    }

    if (Len == 0 || I + Len > Mangled.size())
      return false;

    Out.push_back(Mangled.substr(I, Len));
    Mangled = Mangled.drop_front(I + Len);
  }

  return true;
}