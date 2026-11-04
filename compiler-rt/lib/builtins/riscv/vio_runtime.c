//===-- riscv/vio_runtime.c - RISC-ViO runtime functions ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The runtime functions of the RISC-ViO spec (V0.3.2, "Runtime functions"):
// programs reach the RISC-ViO specific supervisor services and weak cells
// only through these. They are part of the compiler-rt builtins, which are
// linked statically into every image, so calls to them stay inside the
// caller's executable object.
//
//   __vio_type(p)                  type query service        (a7 = 0x7a68)
//   __vio_identity(p)              identity number service   (a7 = 0x7a69)
//   __vio_weak_alloc(n)            weak cell service         (a7 = 0x7a6b)
//   __vio_weak_store(cell, i, p)   store weak reference i
//   __vio_weak_load(cell, i)       load weak reference i
//   __vio_weak_notify(cell, i, fn) weak notify service       (a7 = 0x7a6e)
//
// Services: a7 = number, a0..a2 = arguments, result in a0; the supervisor
// preserves every other register. Names, signatures and the weak cell /
// weak notify numbers are proposals of the spec and may change.
//
// A weak reference occupies two elements of a weak cell: its base (the
// pointer with index 0) and its index (a primitive). Splitting and joining a
// pointer needs no privileged instruction: `andi t, p, 0` yields its base,
// `sub i, p, t` its index, and base + index the pointer again. The garbage
// collector replaces the base by the primitive 0 once the object has been
// collected.
//
// Only for user code: the supervisor does not use services (an ecall would
// trap into itself).
//
//===----------------------------------------------------------------------===//

#if defined(__riscv) && defined(__riscv_zhm)

#include <stddef.h>
#include <stdint.h>

#define VIO_SVC_TYPE 0x7a68
#define VIO_SVC_IDENTITY 0x7a69
#define VIO_SVC_WEAK_CELL 0x7a6b
#define VIO_SVC_WEAK_NOTIFY 0x7a6e

// A notification function: called by the supervisor once weak reference i
// of cell has been cleared (spec: details still open).
typedef void (*__vio_weak_notify_fn)(void *cell, size_t i);

// The base of p (index 0), or 0 for a primitive.
static inline void *vio_base(const void *p) {
  void *base;
  __asm__("andi %0, %1, 0" : "=r"(base) : "r"(p));
  return base;
}

// A pointer stays a pointer under `andi 0` and never equals zero; a
// primitive becomes 0. Used to tell a pointer from a -errno result.
static inline int vio_is_pointer(const void *v) { return vio_base(v) != 0; }

// The index of p within its object (for a primitive, the value itself).
static inline size_t vio_index(const void *p) {
  size_t idx;
  const void *base;
  __asm__("andi %1, %2, 0\n\t"
          "sub  %0, %2, %1"
          : "=r"(idx), "=&r"(base)
          : "r"(p));
  return idx;
}

unsigned long __vio_type(const void *p) {
  register const void *arg __asm__("a0") = p;
  register unsigned long res __asm__("a0");
  register long svc __asm__("a7") = VIO_SVC_TYPE;
  __asm__ volatile("ecall" : "=r"(res) : "r"(arg), "r"(svc) : "memory");
  return res;
}

unsigned long __vio_identity(const void *p) {
  register const void *arg __asm__("a0") = p;
  register unsigned long res __asm__("a0");
  register long svc __asm__("a7") = VIO_SVC_IDENTITY;
  __asm__ volatile("ecall" : "=r"(res) : "r"(arg), "r"(svc) : "memory");
  return res;
}

// A new weak cell holding n weak references (2 elements each, all null), or
// a null pointer if the service fails (it returns -errno, a primitive).
void *__vio_weak_alloc(size_t n) {
  if (n > SIZE_MAX / (2 * sizeof(void *)))
    return 0;
  register size_t size __asm__("a0") = n * 2 * sizeof(void *);
  register void *res __asm__("a0");
  register long svc __asm__("a7") = VIO_SVC_WEAK_CELL;
  __asm__ volatile("ecall" : "=r"(res) : "r"(size), "r"(svc) : "memory");
  return vio_is_pointer(res) ? res : 0;
}

// Weak reference i of cell = p (any pointer, or null). The index is stored
// first, so a concurrent load that finds the new base also finds its index.
void __vio_weak_store(void *cell, size_t i, const void *p) {
  void *volatile *slot = (void *volatile *)cell + 2 * i;
  *(volatile size_t *)(slot + 1) = vio_index(p);
  slot[0] = vio_base(p);
}

// Weak reference i of cell, or a null pointer once its object has been
// collected (or if it was never set). The base is loaded first: once it is
// in a register, it keeps the object alive.
void *__vio_weak_load(const void *cell, size_t i) {
  void *const volatile *slot = (void *const volatile *)cell + 2 * i;
  void *base = slot[0];
  if (base == 0)
    return 0;
  const size_t idx = *(const volatile size_t *)(slot + 1);
  return (char *)base + idx;
}

// Register fn to be called once weak reference i of cell has been cleared.
// 0 or -errno.
int __vio_weak_notify(void *cell, size_t i, __vio_weak_notify_fn fn) {
  register void *a0 __asm__("a0") = cell;
  register size_t a1 __asm__("a1") = i;
  register __vio_weak_notify_fn a2 __asm__("a2") = fn;
  register long res __asm__("a0");
  register long svc __asm__("a7") = VIO_SVC_WEAK_NOTIFY;
  __asm__ volatile("ecall"
                   : "=r"(res)
                   : "r"(a0), "r"(a1), "r"(a2), "r"(svc)
                   : "memory");
  return (int)res;
}

#endif // __riscv && __riscv_zhm
