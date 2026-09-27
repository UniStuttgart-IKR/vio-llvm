/*===---- riscv_zhm.h - Zhm object-type queries ----------------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 *
 * Ask what kind of object a pointer points into. The answer comes from the
 * object's header, which only the supervisor can read (`lt`):
 *
 *   user code        __zhm_type_flags() becomes an ecall to the supervisor.
 *                    Allowed only when compiling with
 *                      -mllvm -riscv-zhm-type-traps
 *                    (otherwise a compile error), because the supervisor has
 *                    to provide that service.
 *   supervisor code  (functions with the "riscv-zhm-supervisor" attribute,
 *                    or -mllvm -riscv-zhm-supervisor) it becomes a direct
 *                    call to __zhm_supervisor_type_flags().
 *
 * A non-pointer, or a pointer without a valid header, yields 0: every
 * predicate is false.
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __RISCV_ZHM_H
#define __RISCV_ZHM_H

#if !defined(__riscv) || !defined(__riscv_zhm)
#error "riscv_zhm.h requires a RISC-V target with the Zhm extension"
#endif

#define ZHM_TYPE_READABLE 0x01u
#define ZHM_TYPE_WRITABLE 0x02u
#define ZHM_TYPE_DATA_ONLY 0x04u  /* storing a pointer traps */
#define ZHM_TYPE_EXECUTABLE 0x08u /* code object */
#define ZHM_TYPE_VALID 0x10u      /* p is a pointer to a valid object */

#ifdef __cplusplus
extern "C" {
#endif

/* Lowered by the compiler (RISCVZhmLegalizeIR); never defined as a function
 * in user code. */
unsigned __zhm_type_flags(const void *p) __attribute__((pure));

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

#ifdef __cplusplus
}
#endif

#endif /* __RISCV_ZHM_H */