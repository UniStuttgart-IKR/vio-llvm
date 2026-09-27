//===-- riscv/zhm_mem.c - Pointer-safe copies for Zhm ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// __zhm_memcpy / __zhm_memmove are emitted by the RISC-V backend
// (RISCVZhmLegalizeIR) for memcpy/memmove that may copy pointers and whose
// operands are not known to be word-aligned or whose size is not a small
// constant.
//
// Zhm rules this code follows:
//   * a pointer fills a whole XLEN-aligned word and may only be read with an
//     XLEN load; a narrower load of a pointer slot traps. So whole words are
//     copied as pointer-sized values (ld/sd keep pointers intact and work for
//     data too), and bytes only where no pointer can be: before dst is
//     aligned, in the tail, or when src and dst are misaligned against each
//     other (then a pointer could not be copied anyway, and the byte load
//     traps, which is the right outcome);
//   * alignment comes from itd: objects are 16-byte aligned, so the low bits
//     of a pointer's index equal the low bits of its address;
//   * ordered compares and subtraction between pointers into different
//     objects trap. memmove therefore first checks whether both pointers are
//     in the same object (same base: p - itd(p)), and only then compares
//     their indices. Different objects never overlap.
//
// Built only for Zhm targets (see lib/builtins/CMakeLists.txt); the #if keeps
// the file empty anywhere else. -ffreestanding / no_builtin keep the loops
// from being turned back into memcpy calls.
//
//===----------------------------------------------------------------------===//

#if defined(__riscv) && defined(__riscv_zhm)

#include <stddef.h>

#define ZHM_WORD sizeof(void *)
#define ZHM_NO_BUILTIN __attribute__((no_builtin))

// Index of p within its object.
static inline size_t zhm_itd(const void *p) {
  size_t idx;
  __asm__("itd %0, %1" : "=r"(idx) : "r"(p));
  return idx;
}

static inline int zhm_word_aligned(const void *p) {
  return (zhm_itd(p) & (ZHM_WORD - 1)) == 0;
}

// Pointer to index 0 of p's object: pointer minus integer, always legal.
static inline const unsigned char *zhm_base(const void *p) {
  return (const unsigned char *)p - zhm_itd(p);
}

// Forward copy. Also correct for overlapping ranges with dst before src.
ZHM_NO_BUILTIN
static void zhm_copy_forward(unsigned char *d, const unsigned char *s,
                             size_t n) {
  while (n && !zhm_word_aligned(d)) {
    *d++ = *s++;
    --n;
  }
  if (zhm_word_aligned(s)) {
    void **dw = (void **)d;
    void *const *sw = (void *const *)s;
    for (; n >= ZHM_WORD; n -= ZHM_WORD)
      *dw++ = *sw++;
    d = (unsigned char *)dw;
    s = (const unsigned char *)sw;
  }
  while (n--)
    *d++ = *s++;
}

// Backward copy, for overlapping ranges with dst after src.
ZHM_NO_BUILTIN
static void zhm_copy_backward(unsigned char *d, const unsigned char *s,
                              size_t n) {
  d += n;
  s += n;
  while (n && !zhm_word_aligned(d)) {
    *--d = *--s;
    --n;
  }
  if (zhm_word_aligned(s)) {
    void **dw = (void **)d;
    void *const *sw = (void *const *)s;
    for (; n >= ZHM_WORD; n -= ZHM_WORD)
      *--dw = *--sw;
    d = (unsigned char *)dw;
    s = (const unsigned char *)sw;
  }
  while (n--)
    *--d = *--s;
}

ZHM_NO_BUILTIN
void *__zhm_memcpy(void *dst, const void *src, size_t n) {
  zhm_copy_forward((unsigned char *)dst, (const unsigned char *)src, n);
  return dst;
}

ZHM_NO_BUILTIN
void *__zhm_memmove(void *dst, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;
  // Only ranges inside the same object can overlap; there, indices order
  // the pointers without an ordered pointer compare.
  if (n && zhm_base(d) == zhm_base(s) && zhm_itd(d) > zhm_itd(s))
    zhm_copy_backward(d, s, n);
  else
    zhm_copy_forward(d, s, n);
  return dst;
}

#endif // __riscv && __riscv_zhm