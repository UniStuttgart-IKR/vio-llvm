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
// Zhm rules this code follows (spec V0.3.2):
//   * a pointer fills a whole XLEN-aligned word and may only be read with an
//     XLEN load; a narrower load of a pointer slot traps. So whole words are
//     copied as pointer-sized values (l[wd]/s[wd] keep pointers intact and
//     work for data too), and bytes only where no pointer can be: before dst
//     is aligned, in the tail, or when src and dst are misaligned against
//     each other (then a pointer could not be copied anyway, and the byte
//     load traps, which is the right outcome);
//   * no bounds checks of their own: a copy that leaves an object traps in
//     the hardware at exactly the access that leaves it (IndexOutBounds),
//     just like the program's own access would. Checking sizes up front
//     (qsz) would turn that crash into different behaviour;
//   * the index of a pointer comes from `andi t, p, 0` (the pointer's base,
//     index 0) and `sub i, p, t` (spec: runtime functions), not from the
//     privileged itd. Objects are 16-byte aligned, so the low bits of the
//     index are the low bits of the address;
//   * ordered compares between pointers into different objects trap. memmove
//     therefore first asks bso whether both pointers reference the same
//     object, and only then compares their indices. Different objects never
//     overlap.
//
// Built only for Zhm targets (see lib/builtins/CMakeLists.txt); the #if keeps
// the file empty anywhere else. no_builtin keeps the loops from being turned
// back into memcpy calls.
//
//===----------------------------------------------------------------------===//

#if defined(__riscv) && defined(__riscv_zhm)

#include <stddef.h>

#define ZHM_WORD sizeof(void *)
#define ZHM_NO_BUILTIN __attribute__((no_builtin))

// Index of p within its object. For a primitive, the value itself.
static inline size_t zhm_index(const void *p) {
  size_t idx;
  const void *base;
  __asm__("andi %1, %2, 0\n\t"
          "sub  %0, %2, %1"
          : "=r"(idx), "=&r"(base)
          : "r"(p));
  return idx;
}

// Do a and b reference the same object? Both must be pointers (bso traps on
// a primitive, as any access through it would).
static inline int zhm_same_object(const void *a, const void *b) {
  __asm__ goto("bso %0, %1, %l[same]" : : "r"(a), "r"(b) : : same);
  return 0;
same:
  return 1;
}

static inline int zhm_word_aligned(const void *p) {
  return (zhm_index(p) & (ZHM_WORD - 1)) == 0;
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
  // n == 0 touches nothing, so it must not trap either (memmove(0, 0, 0)).
  // Only ranges inside the same object can overlap; there, the indices
  // order the pointers without an ordered pointer compare.
  if (n && zhm_same_object(d, s) && zhm_index(d) > zhm_index(s))
    zhm_copy_backward(d, s, n);
  else
    zhm_copy_forward(d, s, n);
  return dst;
}

#endif // __riscv && __riscv_zhm
