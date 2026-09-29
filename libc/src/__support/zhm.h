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
//                             through the supervisor are allowed).
//   LIBC_ZHM_CHECKED_STRINGS  option, always defined (0/1), test with #if:
//                             abort when a string write overflows its object.
//   LIBC_ZHM_DATA_ONLY        marks a local buffer as never holding pointers,
//                             so the compiler gives it data-only memory.
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
#ifndef LIBC_ZHM_CHECKED_STRINGS
#define LIBC_ZHM_CHECKED_STRINGS 0
#endif

#ifdef LIBC_TARGET_HAS_ZHM
#define LIBC_ZHM_DATA_ONLY __attribute__((annotate("zhm_data_only")))
#else
#define LIBC_ZHM_DATA_ONLY
#endif

#ifdef LIBC_TARGET_HAS_ZHM

#if LIBC_ZHM_TYPE_TRAPS
// Lowered by the compiler to the supervisor's type query (riscv_zhm.h).
extern "C" unsigned __zhm_type_flags(const void *p) __attribute__((pure));
#endif

namespace LIBC_NAMESPACE_DECL {
namespace zhm {

LIBC_INLINE_VAR constexpr size_t WORD = sizeof(void *);

// Word types that may alias anything: a word slot is copied or compared
// whole, whether it holds a pointer or data.
typedef void *PtrWord __attribute__((may_alias));
typedef size_t DataWord __attribute__((may_alias));

// Index of p within its object.
LIBC_INLINE size_t itd(const void *p) {
  size_t idx;
  asm("itd %0, %1" : "=r"(idx) : "r"(p));
  return idx;
}

// Size of p's object in bytes.
LIBC_INLINE size_t qsz(const void *p) {
  size_t n;
  asm("qsz %0, %1" : "=r"(n) : "r"(p));
  return n;
}

// Bytes from p to the end of its object: the bound for string functions.
LIBC_INLINE size_t remaining(const void *p) { return qsz(p) - itd(p); }

// Objects are 16-byte aligned, so the low bits of the index are the low
// bits of the address.
LIBC_INLINE bool word_aligned(const void *p) {
  return (itd(p) & (WORD - 1)) == 0;
}
LIBC_INLINE size_t misalignment(const void *p) { return itd(p) & (WORD - 1); }

// Pointer to index 0 of p's object (pointer minus integer: always legal).
LIBC_INLINE const unsigned char *base(const void *p) {
  return static_cast<const unsigned char *>(p) - itd(p);
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

// strlen bounded by the object: never reads past its end. Without a
// terminator in the object, the remaining size is returned.
LIBC_INLINE size_t bounded_strlen(const char *s) {
  const size_t max = remaining(s);
  size_t n = 0;
  while (n < max && s[n])
    ++n;
  return n;
}

// Hardening mode (LIBC_ZHM_CHECKED_STRINGS): abort if `needed` bytes do not
// fit into dst's object.
LIBC_INLINE void check_capacity([[maybe_unused]] const void *dst,
                                [[maybe_unused]] size_t needed) {
#if LIBC_ZHM_CHECKED_STRINGS
  if (needed > remaining(dst))
    abort();
#endif
}

} // namespace zhm
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TARGET_HAS_ZHM
#endif // LLVM_LIBC_SRC___SUPPORT_ZHM_H
