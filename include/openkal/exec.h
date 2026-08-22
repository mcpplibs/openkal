/* openkal.exec --- memory a program may execute.
 *
 * Optional. The interface exists because a program above openkal otherwise has
 * no way to obtain such memory at all, and several ordinary kinds of program
 * need it: an interpreter that compiles, a pattern matcher with a compiled
 * mode, a shader compiler, a loader of any format. None of them can be written
 * above openkal without it, and none of them is unusual.
 *
 * It is deliberately not an operation that loads a program. Loading is parsing
 * a format, relocating, and binding names, and the answer to each is the same
 * in every environment --- so by clause 7.1's reasoning a loader belongs above
 * openkal, written once, and what it needs from beneath is only this.
 *
 * The interface is three operations rather than one because every environment
 * this specification targets now distinguishes writing from executing, and an
 * interface that returned memory which was both would be unimplementable on
 * two of them. A region is writable, then published, then executable; it is
 * never both.
 */
#ifndef OPENKAL_EXEC_H
#define OPENKAL_EXEC_H
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Reserves a region of at least size bytes that may be written and, after
 * kal_exec_publish, executed. Returns a null pointer if the region cannot be
 * reserved.
 *
 * The region is not executable when it is returned. A caller that executes it
 * without publishing it has undefined behaviour, and every environment this
 * specification targets will fault rather than proceed.
 *
 * Alignment is the environment's page granularity or coarser; a caller that
 * needs a particular alignment obtains it within the region. There is no
 * alignment parameter because the granularity that matters here is the one the
 * environment protects at, and a caller cannot usefully ask for less. */
void* kal_exec_alloc(kal_uintptr size);

/* Makes a region obtained from kal_exec_alloc executable and no longer
 * writable. Returns kal_ok, or an error if the environment refuses.
 *
 * The whole of the region is published; there is no partial form. An
 * implementation that had to publish in parts would be reporting its page
 * granularity through the shape of the interface, and a caller cannot act upon
 * that. */
int kal_exec_publish(void* p, kal_uintptr size);

/* Releases a region obtained from kal_exec_alloc, published or not. The size is
 * the one passed to the reservation. */
void kal_exec_free(void* p, kal_uintptr size);

/* Positions in kal_exec_props. */

/* A published region may be reserved again for writing --- kal_exec_alloc
 * followed by kal_exec_publish is not the only order the implementation
 * supports, and a caller may write to a region it has already published by
 * publishing it again. Where the position is zero, a caller that must change
 * published bytes reserves a second region and abandons the first. */
#define KAL_EXEC_PROP_REPUBLISH ((kal_uintptr)1u << 0)

extern const kal_uintptr kal_exec_props;

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_EXEC_H */
