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
 * The whole of the region is published. Version 0.17 adds the partial form
 * below, and clause 11 entry 28 records why the reason this paragraph used to
 * give against it --- that a partial form reports the environment's granularity
 * through the shape of the interface --- did not survive a loader. */
int kal_exec_publish(void* p, kal_uintptr size);

/* Makes PART of a region obtained from kal_exec_alloc executable and no longer
 * writable, and leaves the rest as it was. Version 0.17.
 *
 * A loader needs it, and this header has said since the interface was added
 * that a loader belongs above openkal and needs only this interface. An image's
 * instructions and its data lie at fixed distances from each other in one
 * region --- the instructions address the data relative to themselves --- so
 * the instructions must become executable while the data stays writable, and a
 * whole-region publication cannot express that.
 *
 * `offset' and `size' are measured from the start of the region and are
 * multiples of kal_exec_granularity; a part that is not is kal_err_invalid, and
 * nothing is published. A part may be published once; parts that do not
 * overlap may be published in any order. */
int kal_exec_publish_part(void* region, kal_uintptr offset, kal_uintptr size);

/* The quantum in which kal_exec_publish_part publishes: the environment's
 * protection granularity. Version 0.17.
 *
 * It is not kal_memory_granularity, which is the quantum of reservation: on one
 * environment that is sixty-four kibibytes while protection is per four, and an
 * image laid out for the second would be refused by the first. An image that
 * aligns its parts to the largest granularity among the environments it will
 * run upon (clause 12) is published unchanged on each. */
kal_uintptr kal_exec_granularity(void);

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

/* Whether memory this program may execute is available to THIS ARTIFACT in THIS
 * environment.
 *
 * CLAUSE 6.5 SETTLED THIS AT DEPENDENCY RESOLUTION, AND THAT ANSWER DOES
 * NOT SURVIVE AN ARTIFACT THAT IS DISTRIBUTED. One system grants such memory
 * only to a program carrying a signed declaration, applied after the link by
 * whoever produces the artifact --- so when the artifact is built for its own
 * machine, the party that decides is the party that resolves the dependency and
 * 6.5 is right. When the artifact is produced once and run in many
 * environments, that party decided for all of them, possibly years earlier, and
 * the consumer resolves nothing.
 *
 * ⇒ The interface is provided either way; whether it can be exercised is read
 * here. This is not the shape clause 6.2 forbids --- an operation present and
 * always failing is a defect BECAUSE THE CALLER CANNOT TELL, and a position the
 * caller reads first is exactly what tells it.
 *
 * 6.5's own objection stands and is answered elsewhere: a path few artifacts
 * take is a path little verified. The conformance arrangement answers this
 * position both ways, which is the only way the unavailable path is exercised
 * at all. */
#define KAL_EXEC_PROP_AVAILABLE ((kal_uintptr)1u << 1)

kal_uintptr kal_exec_props(void);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_EXEC_H */
