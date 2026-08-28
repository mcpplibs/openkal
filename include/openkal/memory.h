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

/* The quantum this environment allocates and protects memory in.
 *
 * An address and a length that are multiples of this value are acceptable to
 * every operation of this specification that takes memory; a smaller quantum
 * may or may not be. An implementation with more than one such quantum reports
 * THE COARSEST, so that a caller which rounds to this value is never wrong. An
 * environment with no such quantum reports 1, which is the same statement said
 * of nothing: every address and every length is acceptable.
 *
 * ⭐ NOT A PAGE SIZE, AND THE NAME IS THE POINT. A page is an operating
 * system's mechanism, and this specification has no operation upon one. What a
 * caller needs is the granularity of THIS interface's operations, which is a
 * different question with a different answer: one system allocates in units of
 * sixty-four kilobytes while protecting in units of four, and a value taken
 * from either alone is wrong for the other. The coarsest is correct for both.
 *
 * ⚠️ AND IT IS AN OPERATION BECAUSE IT IS A PROPERTY OF THE RUN. A C library
 * above this interface reports it as its own page size; a library that fixed it
 * when it was built is wrong on every machine whose quantum differs from the
 * one it was built for, which is what a distributed binary meets.
 *
 * ⚠️ NO PROTECTION GRANULARITY IS REPORTED. This specification has no operation
 * upon a mapping's protection, so a second value would be a fact about the
 * machine that no operation here could act upon. */
kal_uintptr kal_memory_granularity(void);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_MEMORY_H */
