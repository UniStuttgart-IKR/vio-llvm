//===-- Implementation for freelist_malloc --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/stdlib/aligned_alloc.h"
#include "src/__support/zhm.h"
#include "src/__support/freelist_heap.h"
#include "src/__support/macros/config.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(void *, aligned_alloc, (size_t alignment, size_t size)) {
#if defined(LIBC_TARGET_HAS_ZHM)
  // Objects are 16-byte aligned; itd cannot reveal finer address bits.
  if (alignment == 0 || (alignment & (alignment - 1)) || alignment > 16)
    return nullptr;
  return zhm::alloc(size);
#else
  return freelist_heap->aligned_allocate(alignment, size);
#endif
}

} // namespace LIBC_NAMESPACE_DECL
