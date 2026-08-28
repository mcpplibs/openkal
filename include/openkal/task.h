/* openkal.task --- execution contexts, and the primitive they are built upon.
 *
 * Mutexes and condition variables are absent: they are constructions above the
 * primitive rather than facilities of the boundary, which is observable in any
 * C library that implements them. */
#ifndef OPENKAL_TASK_H
#define OPENKAL_TASK_H
#include "types.h"

struct kal_task { kal_uintptr h; };

/* Positions in kal_task_props. */
#define KAL_TASK_PROP_PREEMPTIVE   ((kal_uintptr)1u << 0)
#define KAL_TASK_PROP_PARALLEL     ((kal_uintptr)1u << 1)
#define KAL_TASK_PROP_WAIT_TIMEOUT ((kal_uintptr)1u << 2)
/* A context started by kal_task_start observes the thread-local storage of the
 * toolchain that compiled the program: a variable declared thread_local has a
 * distinct instance in it, correctly initialised. The register convention that
 * delivers this belongs to openarch rather than to openkal, so the
 * specification reports whether the implementation's contexts have it rather
 * than providing an operation that establishes it. A C library ported onto
 * openkal keeps its per-context state in one such variable, and cannot be
 * ported onto an implementation that lacks the property. */
#define KAL_TASK_PROP_THREAD_LOCAL ((kal_uintptr)1u << 3)

#ifdef __cplusplus
extern "C" {
#endif

/* Starts a context.
 *
 * ⭐ `entry' IS AN ADDRESS AT WHICH A CONTEXT BEGINS, NOT A FUNCTION THAT IS
 * CALLED, AND THE DIFFERENCE IS WHAT LETS THIS CROSS A BOUNDARY. An
 * implementation on the far side of one does not call into the program; it
 * establishes a context whose program counter is `entry' and whose first
 * argument is `arg', which is what a kernel's own context-creating primitive
 * does. Consequently:
 *
 *   - NO RETURN ADDRESS EXISTS. The started context has nothing to return to,
 *     and a caller shall not write an entry that relies on returning to
 *     anything. Returning from `entry' ends the context, which is the only
 *     defined thing that can happen and is not an error.
 *
 *   - `arg' is a value the started context receives, and is meaningful in the
 *     address space the context runs in --- which is the caller's, because that
 *     is what this interface provides. Copying an address space is
 *     `openkal.space'.
 *
 * The stack is provided by the implementation; its size is a property rather
 * than a parameter, because an environment that does not allocate stacks
 * separately cannot honour a request for one. An implementation across a
 * boundary allocates it too, so the crossed form asks for nothing the linked
 * form does not. */
int kal_task_start(void (*entry)(void*), void* arg, struct kal_task* out);

/* Waits for a context to finish. A context is waited for at most once. */
int kal_task_join(struct kal_task);

/* Relinquishes the processor. An implementation without a scheduler returns
 * immediately, which is a supply and not a simulation. */
void kal_task_yield(void);

/* The identity of the calling context. Unique among contexts running at the
 * same moment; may be reused after one finishes. */
kal_uintptr kal_task_current(void);

/* Suspends the calling context while the word at the given address holds the
 * given value, until another context wakes it or the timeout elapses. The
 * comparison and the suspension occur without an intervening opportunity for
 * the value to change unobserved. A timeout of zero denotes no timeout. */
int kal_task_wait(const kal_u32* word, kal_u32 expected,
                  kal_u64 timeout_ns);

/* Wakes at most count contexts suspended upon the given address and reports
 * how many were woken. A count of zero wakes none and is permitted. */
int kal_task_wake(const kal_u32* word, kal_uintptr count, kal_uintptr* woken);

kal_uintptr kal_task_props(void);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_TASK_H */
