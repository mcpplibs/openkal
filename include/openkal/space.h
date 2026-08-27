/* openkal.space --- an address space, and a context executing in one.
 *
 * THIS IS NOT `fork'. Clause 7.1 refuses to require the duplication of an
 * address space AND ITS EXECUTION STATE; what is here is the first half alone.
 * A context started in a cloned space begins at a function the caller names,
 * not at the instruction the caller was executing --- which is what lets this
 * be stated in a C application binary interface at all.
 *
 * A library above this interface reaches `fork' by saving its own execution
 * state before the clone and restoring it in the new context. That is
 * composition, and it belongs above this line rather than in it: the saving is
 * done with the compiler's own facilities, differs per architecture, and is not
 * something a kernel interface can perform on a caller's behalf.
 *
 * THERE IS NO `create'. An empty address space contains no code, so the entry
 * function a caller would name is not in it. A program that wants a child
 * holding only what it grants uses `kal_process_spawn_with', which starts a
 * named program rather than a function of the caller's. */
#ifndef OPENKAL_SPACE_H
#define OPENKAL_SPACE_H
#include "types.h"
#include "process.h"

/* An opaque handle occupying one machine word, per clause 7.2. */
struct kal_space { kal_uintptr h; };

/* Positions in kal_space_props. */

/* Whether a clone carries the handles the original holds.
 *
 * An environment whose cloning primitive copies memory and not handles reports
 * zero here, and a library above it cannot reach POSIX fork semantics. Stating
 * it lets that library refuse at the point of the attempt rather than produce a
 * child that is subtly not the one POSIX describes. */
#define KAL_SPACE_PROP_CLONE_HANDLES ((kal_uintptr)1u << 0)

/* Whether the copy may be completed lazily, so that a store to cloned memory
 * may fail after the clone has already reported success.
 *
 * A program that cannot tolerate a deferred failure adapts to this; it cannot
 * ask for the other behaviour. The word says which environment it is in, and
 * nothing here promises to change it --- an implementation whose environment
 * defers the copy cannot undefer it, and one that does not cannot pretend to. */
#define KAL_SPACE_PROP_DEFERRED_COPY ((kal_uintptr)1u << 1)

#ifdef __cplusplus
extern "C" {
#endif

/* Clones the CALLER's address space. */
int kal_space_clone(struct kal_space* out);

/* Starts a context in the given space.
 *
 * The result is a process: a context with an address space of its own is what
 * that word means, so `kal_process_wait', `kal_process_terminate' and
 * `kal_process_close' apply to it unchanged rather than being restated here.
 *
 * The stack is the caller's to provide, and its top is passed rather than its
 * base because that is the end execution begins from on every architecture this
 * specification targets. */
int kal_space_start(struct kal_space sp,
                    void (*entry)(void*), void* arg, void* stack_top,
                    struct kal_process* out);

/* Releases the space.
 *
 * A space whose context is running is not released until that process ends. The
 * two handles are independent and either may be closed first, which is the same
 * ownership rule clause 6.7 states generally. */
void kal_space_destroy(struct kal_space sp);

extern const kal_uintptr kal_space_props;

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_SPACE_H */
