/* openkal.memory --- allocation. Core: any environment with writable memory
 * can supply an allocator, and one that can be exhausted bounds what is
 * available rather than making its callers silently wrong. */
#ifndef OPENKAL_MEMORY_H
#define OPENKAL_MEMORY_H
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns a region of at least size bytes aligned to align, or a null
 * pointer. align shall be a power of two. Where the environment already
 * provides an allocator, an implementation is built upon it and not beside
 * it: two allocators drawing on one region of memory is a defect that appears
 * only under load. */
void* kal_alloc(kal_uintptr size, kal_uintptr align);

/* Releases a region obtained from kal_alloc. The size and alignment are those
 * passed to the allocation. */
void  kal_free(void* p, kal_uintptr size, kal_uintptr align);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_MEMORY_H */
