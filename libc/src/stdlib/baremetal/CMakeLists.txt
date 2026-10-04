add_header_library(
  abort_utils
  HDRS
    abort_utils.h
)

if(LIBC_TARGET_HAS_ZHM)
  set(heap_dep libc.src.__support.zhm)
else()
  set(heap_dep libc.src.__support.freelist_heap)
endif()

add_entrypoint_object(
  malloc
  SRCS
    malloc.cpp
  HDRS
    ../malloc.h
  DEPENDS
    ${heap_dep}
)

add_entrypoint_object(
  free
  SRCS
    free.cpp
  HDRS
    ../free.h
  DEPENDS
    ${heap_dep}
)

add_entrypoint_object(
  calloc
  SRCS
    calloc.cpp
  HDRS
    ../calloc.h
  DEPENDS
    ${heap_dep}
)

add_entrypoint_object(
  realloc
  SRCS
    realloc.cpp
  HDRS
    ../realloc.h
  DEPENDS
    ${heap_dep}
)

add_entrypoint_object(
  aligned_alloc
  SRCS
    aligned_alloc.cpp
  HDRS
    ../aligned_alloc.h
  DEPENDS
    ${heap_dep}
)

add_entrypoint_object(
  malloc_usable_size
  SRCS
    malloc_usable_size.cpp
  HDRS
    ../malloc_usable_size.h
  DEPENDS
    ${heap_dep}
)
