/*===---- riscv_zhm.h - Zhm object queries and runtime functions -----------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 *
 * Object-type queries: what kind of object a pointer points into. The
 * answer comes from the object's tags, which only the supervisor can read
 * (`lt`):
 *
 *   user code        __zhm_type_flags() becomes a call to __vio_type(), the
 *                    compiler-rt runtime function of the type query service.
 *   supervisor code  (functions with the "riscv-zhm-supervisor" attribute,
 *                    or -mllvm -riscv-zhm-supervisor) it becomes a direct
 *                    call to __zhm_supervisor_type_flags().
 *
 * A non-pointer, or a pointer without a valid object, yields 0: every
 * predicate is false.
 *
 * Runtime functions (RISC-ViO spec V0.3.2): the RISC-ViO specific services
 * and weak cells, for user code. Defined in the compiler-rt builtins, which
 * are linked into every image. Names and signatures are proposals of the
 * spec and may change.
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __RISCV_ZHM_H
#define __RISCV_ZHM_H

#if !defined(__riscv) || !defined(__riscv_zhm)
#error "riscv_zhm.h requires a RISC-V target with the Zhm extension"
#endif

/* Marks a local variable (typically a char buffer) as never holding
 * pointers, so the compiler places it in data-only memory when it has to
 * leave the stack frame:
 *     ZHM_DATA_ONLY char buf[256];
 * Storing a pointer into it traps. */
#define ZHM_DATA_ONLY __attribute__((annotate("zhm_data_only")))

#define ZHM_TYPE_READABLE 0x01u
#define ZHM_TYPE_WRITABLE 0x02u
#define ZHM_TYPE_DATA_ONLY 0x04u  /* storing a pointer traps */
#define ZHM_TYPE_EXECUTABLE 0x08u /* code object */
#define ZHM_TYPE_VALID 0x10u      /* p is a pointer to a valid object */
/* Spec V0.3.2 lists these as well; the supervisor does not report them yet,
 * and the spec has not fixed the bit layout. */
#define ZHM_TYPE_READ_THROUGH 0x20u
#define ZHM_TYPE_WRITE_THROUGH 0x40u
#define ZHM_TYPE_WEAK_CELL 0x80u

#ifdef __cplusplus
extern "C" {
#endif

/* Lowered by the compiler (RISCVZhmLegalizeIR); never defined as a function
 * in user code. */
unsigned long __zhm_type_flags(const void *p) __attribute__((pure));

static __inline__ int zhm_is_valid(const void *p) {
  return (__zhm_type_flags(p) & ZHM_TYPE_VALID) != 0;
}
static __inline__ int zhm_is_readable(const void *p) {
  return (__zhm_type_flags(p) & ZHM_TYPE_READABLE) != 0;
}
static __inline__ int zhm_is_writable(const void *p) {
  return (__zhm_type_flags(p) & ZHM_TYPE_WRITABLE) != 0;
}
static __inline__ int zhm_is_data_only(const void *p) {
  return (__zhm_type_flags(p) & ZHM_TYPE_DATA_ONLY) != 0;
}
static __inline__ int zhm_is_executable(const void *p) {
  return (__zhm_type_flags(p) & ZHM_TYPE_EXECUTABLE) != 0;
}

/* --- Runtime functions (compiler-rt) ------------------------------------ */

/* Normalized flags of p's object (ZHM_TYPE_*), 0 for a primitive. */
unsigned long __vio_type(const void *p);

/* Identity number of p's object, assigned on the first request; 0 for a
 * primitive. Stable while the object lives, also when it is moved: use it to
 * order and hash objects by identity. Whether two pointers reference the
 * same object is cheaper to ask with bso (zhm_same_object below). */
unsigned long __vio_identity(const void *p);

/* Weak cells: n weak references that do not keep their objects alive. Once
 * an object has been collected, loading its weak reference yields a null
 * pointer. */
typedef void (*__vio_weak_notify_fn)(void *cell, __SIZE_TYPE__ i);
void *__vio_weak_alloc(__SIZE_TYPE__ n);           /* null on failure */
void __vio_weak_store(void *cell, __SIZE_TYPE__ i, const void *p);
void *__vio_weak_load(const void *cell, __SIZE_TYPE__ i);
/* Call fn(cell, i) once reference i has been cleared. 0 or -errno. */
int __vio_weak_notify(void *cell, __SIZE_TYPE__ i, __vio_weak_notify_fn fn);

/* Do a and b reference the same object? Both must be pointers: bso traps on
 * a primitive, as any access through it would. */
static __inline__ int zhm_same_object(const void *a, const void *b) {
  __asm__ goto("bso %0, %1, %l[same]" : : "r"(a), "r"(b) : : same);
  return 0;
same:
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* __RISCV_ZHM_H */