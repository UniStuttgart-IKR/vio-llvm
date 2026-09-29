//===-- Implementation for freelist_malloc --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/stdlib/calloc.h"
#include "src/__support/zhm.h"
#include "src/__support/freelist_heap.h"
#include "src/__support/macros/config.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(void *, calloc, (size_t num, size_t size)) {
#if defined(LIBC_TARGET_HAS_ZHM)
  size_t total;
  if (__builtin_mul_overflow(num, size, &total))
    return nullptr;
  return zhm::alloc(total); // already zeroed
#else
  return freelist_heap->calloc(num, size);
#endif
}

} // namespace LIBC_NAMESPACE_DECL
