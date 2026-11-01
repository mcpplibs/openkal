/* openkal.region --- a range of the address space reserved first and backed as
 * it is used.
 *
 * Version 0.20. Clause 11 entry 44 records why it was admitted.
 *
 * WHAT `openkal.memory' DOES NOT STATE. `kal_alloc' obtains memory that can be
 * used at once, and an implementation builds it on the environment's allocator
 * (clause 7.3). The other half of what an address space offers --- a range
 * reserved at a large alignment without being backed, backed part by part as it
 * is used, its backing returned while the addresses stay reserved, and the whole
 * released at once --- has had no statement. It is the part every allocator of
 * a general kind, every collected heap and every arena of a compiler or an
 * interpreter is built upon, and it is the common part of the mapping calls of
 * one family of systems and the reservation calls of the other.
 *
 * AN INTERFACE OF ITS OWN AND NOT OPERATIONS OF `openkal.memory', for clause
 * 6.2's reason and clause 3.2's. An environment without a page table has
 * memory and cannot reserve addresses apart from backing them; placed in the
 * core interface, the operations would be required of it and would always
 * fail.
 *
 * THE REGION IS AN ADDRESS AND A LENGTH, as an allocation and a mapped part of
 * a file are, so that no implementation keeps a table (clause 7.1).
 *
 * NO STATEMENT OF PROTECTION. A range is reserved, backed, or neither; this
 * specification has no operation on a mapping's protection (`memory.h'), and a
 * range that is backed may be read and written. Instructions run only from
 * memory `openkal.exec' published.
 *
 * Clause 4.4: S, L and X. The region lies in the caller's address space, as an
 * allocation does. */
#ifndef OPENKAL_REGION_H
#define OPENKAL_REGION_H
#include "types.h"

/* Positions in the `flags' argument of kal_region_reserve. */

/* The whole range is backed when it is reserved, as if kal_region_commit had
 * been called upon it. */
#define KAL_REGION_COMMIT ((kal_uintptr)1u << 0)

/* Positions in kal_region_props --- a property of the implementation. */

/* The environment does not charge backing apart from reservation: a reserved
 * range may be used as soon as it is reserved, kal_region_commit makes no
 * request of the environment, and the backing of a page is found when the page
 * is first written. A program that reserves more than it will use relies on
 * this to keep the reservation cheap; one that must know the backing exists
 * before it writes cannot learn it where this is claimed, and that is the
 * environment's own property, stated rather than hidden. */
#define KAL_REGION_PROP_OVERCOMMIT ((kal_uintptr)1u << 0)

#ifdef __cplusplus
extern "C" {
#endif

/* Reserves `size' bytes of the address space at an address that is a multiple
 * of `align', and returns the address, or a null pointer when the environment
 * has no such range to give.
 *
 * `size' is a multiple of `kal_memory_granularity()', and `align' is a power of
 * two no smaller than it; otherwise a null pointer. Without KAL_REGION_COMMIT
 * the range is reserved and not backed, and an access to a part that has not
 * been backed is undefined. A position in `flags' this version does not assign
 * is refused with a null pointer.
 *
 * Memory the range holds when it is first backed reads as zero. */
void* kal_region_reserve(kal_uintptr size, kal_uintptr align, kal_uintptr flags);

/* Backs a part of a reserved range so that it may be read and written; it
 * reads as zero where it was not backed before, and where kal_region_decommit
 * returned its backing. Backing a part already backed leaves its contents.
 *
 * `addr' and `len' are multiples of `kal_memory_granularity()', or the call is
 * kal_err_invalid; that the part lies within one range kal_region_reserve
 * returned is the caller's to ensure, as the extent of an allocation is. An
 * environment that has no backing to give reports kal_err_no_memory and leaves
 * the part as it was. */
int kal_region_commit(void* addr, kal_uintptr len);

/* Returns the backing of a part of a reserved range to the environment: the
 * memory it occupied and, where the environment charges backing, the charge.
 * The addresses stay reserved, and the part is used again only after
 * kal_region_commit, after which it reads as zero. The arguments are as for
 * kal_region_commit. */
int kal_region_decommit(void* addr, kal_uintptr len);

/* Tells the environment that the contents of a backed part are no longer
 * needed: the memory it occupies may be taken back, and the part stays backed
 * and may be read and written at once. What a read finds there afterwards is
 * not specified --- the old bytes, zero, or either at each page --- until it is
 * written. This is the cheaper of the two returns: it makes no promise of
 * zero, so an environment that can take memory back lazily need do nothing
 * until it wants the memory. The arguments are as for kal_region_commit. */
int kal_region_discard(void* addr, kal_uintptr len);

/* Releases a whole reserved range: `addr' is what kal_region_reserve returned
 * and `size' what was passed to it. A part of a range is not released; one
 * environment releases a reservation only whole. The addresses may be given out
 * again afterwards. Releasing a valid range does not fail in any environment
 * this specification targets, so nothing is returned, as `kal_free' returns
 * nothing. */
void kal_region_release(void* addr, kal_uintptr size);

/* Properties of this implementation's regions. See the positions above. */
kal_uintptr kal_region_props(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_REGION_H */
