//===-- Zhm support for llvm-libc -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Helpers for the Zhm extension (hardware-managed objects).
//
//   LIBC_TARGET_HAS_ZHM       target property, llvm-libc style: defined only
//                             on Zhm. Test with #ifdef / #if defined(...);
//                             every file that tests it must include this
//                             header.
//   LIBC_ZHM_TYPE_TRAPS       option, always defined (0/1), test with #if:
//                             1 for the shared libc only (object-type queries
//                             through compiler-rt's __vio_type are allowed).
//   LIBC_ZHM_DATA_ONLY        marks a local buffer as never holding pointers,
//                             so the compiler gives it data-only memory.
//
// Spec V0.3.2: itd, btd, sep and lt are privileged, so user code (the libc
// runs in user mode) splits pointers with ordinary instructions: `andi t, p,
// 0` yields the pointer's base (index 0), `sub i, p, t` its index. bso checks
// whether two pointers reference the same object.
//
// No function here or in the Zhm memory/string code checks object bounds
// itself: an access beyond an object traps in the hardware (IndexOutBounds)
// at exactly that access, as the program's own access would. Checking sizes
// up front would turn that crash into different behaviour.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_ZHM_H
#define LLVM_LIBC_SRC___SUPPORT_ZHM_H

#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

#include <stddef.h>

#if defined(__riscv) && defined(__riscv_zhm)
#define LIBC_TARGET_HAS_ZHM
#endif

#ifndef LIBC_ZHM_TYPE_TRAPS
#define LIBC_ZHM_TYPE_TRAPS 0
#endif

#ifdef LIBC_TARGET_HAS_ZHM
#define LIBC_ZHM_DATA_ONLY __attribute__((annotate("zhm_data_only")))
#else
#define LIBC_ZHM_DATA_ONLY
#endif

#ifdef LIBC_TARGET_HAS_ZHM

#if LIBC_ZHM_TYPE_TRAPS
// Lowered by the compiler to a call of compiler-rt's __vio_type (type query
// service); same declaration as in riscv_zhm.h.
extern "C" unsigned long __zhm_type_flags(const void *p)
    __attribute__((pure));
#endif

namespace LIBC_NAMESPACE_DECL {
namespace zhm {

LIBC_INLINE_VAR constexpr size_t WORD = sizeof(void *);

// Word types that may alias anything: a word slot is copied or compared
// whole, whether it holds a pointer or data.
typedef void *PtrWord __attribute__((may_alias));
typedef size_t DataWord __attribute__((may_alias));

// Index of p within its object (for a primitive, the value itself), without
// the privileged itd.
LIBC_INLINE size_t index_of(const void *p) {
  size_t idx;
  const void *b;
  asm("andi %1, %2, 0\n\t"
      "sub  %0, %2, %1"
      : "=r"(idx), "=&r"(b)
      : "r"(p));
  return idx;
}

// Size of p's object in bytes. Information (e.g. for realloc), never a
// bounds check.
LIBC_INLINE size_t qsz(const void *p) {
  size_t n;
  asm("qsz %0, %1" : "=r"(n) : "r"(p));
  return n;
}

// Objects are 16-byte aligned, so the low bits of the index are the low
// bits of the address.
LIBC_INLINE bool word_aligned(const void *p) {
  return (index_of(p) & (WORD - 1)) == 0;
}
LIBC_INLINE size_t misalignment(const void *p) {
  return index_of(p) & (WORD - 1);
}

// Do a and b reference the same object? Both must be pointers: bso traps on
// a primitive, as any access through it would.
LIBC_INLINE bool same_object(const void *a, const void *b) {
  asm goto("bso %0, %1, %l[same]" : : "r"(a), "r"(b) : : same);
  return false;
same:
  return true;
}

// New zeroed objects. `alloc` may hold pointers, `alloc_data` may not.
LIBC_INLINE void *alloc(size_t size) {
  void *p;
  asm volatile("alc %0, %1" : "=r"(p) : "r"(size ? size : 1) : "memory");
  return __builtin_assume_aligned(p, 16);
}
LIBC_INLINE void *alloc_data(size_t size) {
  void *p;
  asm volatile("alc.d %0, %1" : "=r"(p) : "r"(size ? size : 1) : "memory");
  return __builtin_assume_aligned(p, 16);
}

// Order of two differing memory words, as their bytes compare; 1 if either
// holds a pointer (supervisor service, allowed in both libc variants).
// (Explicit moves instead of register variables, which C++17 does not
// allow. a0/a1/a7 are clobbered, so the compiler keeps other values out.)
LIBC_INLINE int wordcmp(void *a, void *b) {
  long out;
  asm volatile("mv a0, %1\n\t"
               "mv a1, %2\n\t"
               "li a7, 0x7a63\n\t"
               "ecall\n\t"
               "mv %0, a0"
               : "=r"(out)
               : "r"(a), "r"(b)
               : "a0", "a1", "a7", "memory");
  return static_cast<int>(out);
}

[[noreturn]] LIBC_INLINE void abort() {
  asm volatile("li a7, 0x7a61\n\t"
               "ecall"
               :
               :
               : "a7", "memory");
  __builtin_unreachable();
}

// Object-type knowledge. Without the type trap nothing is known.
LIBC_INLINE bool is_data_only([[maybe_unused]] const void *p) {
#if LIBC_ZHM_TYPE_TRAPS
  return (__zhm_type_flags(p) & 0x04u) != 0;
#else
  return false;
#endif
}

} // namespace zhm
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TARGET_HAS_ZHM
#endif // LLVM_LIBC_SRC___SUPPORT_ZHM_H
