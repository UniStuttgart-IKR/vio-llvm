//===-- Zhm memory functions ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// memcpy, memmove, memset, bcmp and memcmp for Zhm, selected in the
// riscv/inline_*.h headers when LIBC_TARGET_HAS_ZHM is defined.
//
// Rules:
//   * a pointer fills a whole XLEN-aligned word and may only be read with a
//     word load; a narrower load of a pointer slot traps. Words are therefore
//     copied and compared whole; bytes are used only where no pointer can be
//     (before the operands are aligned, in the tail, or when the operands are
//     misaligned against each other);
//   * alignment comes from itd (objects are 16-byte aligned);
//   * ordered compares of pointers into different objects trap: memmove
//     decides the direction from bases and indices;
//   * memcmp: if both objects are known to be data-only (shared libc),
//     words are loaded and ordered as integers; otherwise they are loaded
//     as pointer words and the first difference is ordered by the
//     supervisor's word-compare service (exact for data, 1 if a pointer is
//     involved). The same memory is never loaded both as a pointer and as
//     bytes: LLVM's load forwarding would turn the second load into
//     integer operations on the pointer (ptrtoint + shifts), which the
//     Zhm verifier rejects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_RISCV_ZHM_MEM_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_RISCV_ZHM_MEM_H

#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/zhm.h"

#include <stddef.h>

#ifdef LIBC_TARGET_HAS_ZHM

namespace LIBC_NAMESPACE_DECL {

namespace zhm_detail {

using zhm::PtrWord;
using zhm::DataWord;
using zhm::WORD;

LIBC_INLINE void copy_forward(unsigned char *d, const unsigned char *s,
                              size_t n) {
  while (n && !zhm::word_aligned(d)) {
    *d++ = *s++;
    --n;
  }
  if (zhm::word_aligned(s)) {
    PtrWord *dw = reinterpret_cast<PtrWord *>(d);
    const PtrWord *sw = reinterpret_cast<const PtrWord *>(s);
    for (; n >= WORD; n -= WORD)
      *dw++ = *sw++;
    d = reinterpret_cast<unsigned char *>(dw);
    s = reinterpret_cast<const unsigned char *>(sw);
  }
  while (n--)
    *d++ = *s++;
}

LIBC_INLINE void copy_backward(unsigned char *d, const unsigned char *s,
                               size_t n) {
  d += n;
  s += n;
  while (n && !zhm::word_aligned(d)) {
    *--d = *--s;
    --n;
  }
  if (zhm::word_aligned(s)) {
    PtrWord *dw = reinterpret_cast<PtrWord *>(d);
    const PtrWord *sw = reinterpret_cast<const PtrWord *>(s);
    for (; n >= WORD; n -= WORD)
      *--dw = *--sw;
    d = reinterpret_cast<unsigned char *>(dw);
    s = reinterpret_cast<const unsigned char *>(sw);
  }
  while (n--)
    *--d = *--s;
}

LIBC_INLINE int byte_order(const unsigned char *a, const unsigned char *b,
                           size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (a[i] != b[i])
      return static_cast<int>(a[i]) - static_cast<int>(b[i]);
  return 0;
}

// Order of two differing data words, as memcmp sees their bytes: the
// lowest differing byte decides (little endian). Works on the loaded
// integers, so the memory is never read a second time.
LIBC_INLINE int data_word_order(DataWord x, DataWord y) {
  const DataWord diff = x ^ y;
  const unsigned shift =
      static_cast<unsigned>(__builtin_ctzl(static_cast<unsigned long>(diff))) &
      ~7u;
  return static_cast<int>((x >> shift) & 0xff) -
         static_cast<int>((y >> shift) & 0xff);
}

} // namespace zhm_detail

LIBC_INLINE void inline_memcpy_zhm(void *__restrict dst,
                                   const void *__restrict src, size_t n) {
  zhm_detail::copy_forward(static_cast<unsigned char *>(dst),
                           static_cast<const unsigned char *>(src), n);
}

LIBC_INLINE void inline_memmove_zhm(void *dst, const void *src, size_t n) {
  auto *d = static_cast<unsigned char *>(dst);
  const auto *s = static_cast<const unsigned char *>(src);
  // Only ranges in the same object can overlap; there, the indices order
  // the pointers without an ordered pointer compare.
  if (n && zhm::base(d) == zhm::base(s) && zhm::itd(d) > zhm::itd(s))
    zhm_detail::copy_backward(d, s, n);
  else
    zhm_detail::copy_forward(d, s, n);
}

// sb into a pointer slot clears the pointer, which is the correct result of
// overwriting it with data; word stores are only a speed-up.
LIBC_INLINE void inline_memset_zhm(void *dst, unsigned char value, size_t n) {
  auto *d = static_cast<unsigned char *>(dst);
  while (n && !zhm::word_aligned(d)) {
    *d++ = value;
    --n;
  }
  const zhm::DataWord pattern =
      static_cast<zhm::DataWord>(value) * (~zhm::DataWord(0) / 0xff);
  auto *dw = reinterpret_cast<zhm::DataWord *>(d);
  for (; n >= zhm::WORD; n -= zhm::WORD)
    *dw++ = pattern;
  d = reinterpret_cast<unsigned char *>(dw);
  while (n--)
    *d++ = value;
}

// Equality only: word equality works for pointers and data alike.
LIBC_INLINE int inline_bcmp_zhm(const void *lhs, const void *rhs, size_t n) {
  const auto *a = static_cast<const unsigned char *>(lhs);
  const auto *b = static_cast<const unsigned char *>(rhs);
  if (zhm::misalignment(a) == zhm::misalignment(b)) {
    while (n && !zhm::word_aligned(a)) {
      if (*a++ != *b++)
        return 1;
      --n;
    }
    const auto *wa = reinterpret_cast<const zhm::PtrWord *>(a);
    const auto *wb = reinterpret_cast<const zhm::PtrWord *>(b);
    for (; n >= zhm::WORD; n -= zhm::WORD)
      if (*wa++ != *wb++)
        return 1;
    a = reinterpret_cast<const unsigned char *>(wa);
    b = reinterpret_cast<const unsigned char *>(wb);
  }
  while (n--)
    if (*a++ != *b++)
      return 1;
  return 0;
}

LIBC_INLINE int inline_memcmp_zhm(const void *lhs, const void *rhs, size_t n) {
  const auto *a = static_cast<const unsigned char *>(lhs);
  const auto *b = static_cast<const unsigned char *>(rhs);
  if (zhm::misalignment(a) == zhm::misalignment(b)) {
    while (n && !zhm::word_aligned(a)) {
      if (*a != *b)
        return static_cast<int>(*a) - static_cast<int>(*b);
      ++a, ++b, --n;
    }
    if (zhm::is_data_only(a) && zhm::is_data_only(b)) {
      // Data only: words as integers, no pointer is ever loaded.
      const auto *wa = reinterpret_cast<const zhm::DataWord *>(a);
      const auto *wb = reinterpret_cast<const zhm::DataWord *>(b);
      for (; n >= zhm::WORD; n -= zhm::WORD, ++wa, ++wb)
        if (*wa != *wb)
          return zhm_detail::data_word_order(*wa, *wb);
      a = reinterpret_cast<const unsigned char *>(wa);
      b = reinterpret_cast<const unsigned char *>(wb);
    } else {
      // May hold pointers: word equality, order from the supervisor.
      const auto *wa = reinterpret_cast<const zhm::PtrWord *>(a);
      const auto *wb = reinterpret_cast<const zhm::PtrWord *>(b);
      for (; n >= zhm::WORD; n -= zhm::WORD, ++wa, ++wb)
        if (*wa != *wb)
          return zhm::wordcmp(*wa, *wb);
      a = reinterpret_cast<const unsigned char *>(wa);
      b = reinterpret_cast<const unsigned char *>(wb);
    }
  }
  return zhm_detail::byte_order(a, b, n);
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TARGET_HAS_ZHM
#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_RISCV_ZHM_MEM_H
