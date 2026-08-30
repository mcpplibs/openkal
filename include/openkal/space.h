/* openkal.space --- starting a context in a copy of the calling address space.
 *
 * THIS IS NOT `fork'. Clause 7.1 refuses to require the duplication of an
 * address space AND ITS EXECUTION STATE; what is here is the first half alone.
 * The started context begins at a function the caller names, not at the
 * instruction the caller was executing --- which is what lets this be stated in
 * a C application binary interface at all.
 *
 * A library above this interface reaches `fork' by saving its own execution
 * state before the call and restoring it in the started context. That is
 * composition, and it belongs above this line rather than in it: the saving is
 * done with the compiler's own facilities, differs per architecture, and is not
 * something a kernel interface can perform on a caller's behalf.
 *
 * ONE OPERATION AND NOT TWO, AND CLAUSE 7.1 IS WHY. An earlier form of this
 * interface separated the copying of the space from the starting of a context
 * in it, so that a caller held a space as a handle. No environment this
 * specification targets has that pair as a primitive: the copy and the start are
 * one act, and an implementation asked to separate them would have to start a
 * context anyway, park it upon a waiting primitive, and build a channel by which
 * to tell it what to run. That is a mechanism reconstructed rather than a
 * facility conveyed, which clause 7.1 identifies as a fault in the shape of the
 * specification rather than in the implementation. Clause 6.3 records the
 * separated form among the mechanisms considered and not adopted.
 *
 * THERE IS NO OPERATION THAT CREATES AN EMPTY SPACE. An empty address space
 * contains no code, so the entry function a caller would name is not in it. A
 * program that wants a child holding only what it grants uses
 * `kal_process_spawn' with `grants' set, which starts a named program rather
 * than a function of the caller's. */
#ifndef OPENKAL_SPACE_H
#define OPENKAL_SPACE_H
#include "types.h"
#include "process.h"

/* Positions in kal_space_props. */

/* Whether the started context holds the handles the caller holds.
 *
 * An environment whose copying primitive carries memory and not handles reports
 * zero here, and a library above it cannot reach POSIX fork semantics. Stating
 * it lets that library refuse at the point of the attempt rather than produce a
 * child that is subtly not the one POSIX describes. */
#define KAL_SPACE_PROP_CLONE_HANDLES ((kal_uintptr)1u << 0)

/* Whether the copy may be completed lazily, so that a store to copied memory may
 * fail after the operation has already reported success.
 *
 * A program that cannot tolerate a deferred failure adapts to this; it cannot
 * ask for the other behaviour. The word says which environment the program is
 * in, and nothing here promises to change it: an implementation whose
 * environment defers the copy cannot undefer it, and one that does not cannot
 * pretend to. */
#define KAL_SPACE_PROP_DEFERRED_COPY ((kal_uintptr)1u << 1)

#ifdef __cplusplus
extern "C" {
#endif

/* Starts a context in a copy of the calling address space.
 *
 * The result is a process: a context with an address space of its own is what
 * that word means, so `kal_process_wait', `kal_process_terminate' and
 * `kal_process_close' apply to it unchanged rather than being restated here.
 *
 * The copy is taken at this call. What the caller stores afterwards is not seen
 * by the started context, and what the started context stores is not seen by the
 * caller; two contexts sharing an address space is `openkal.task' and not this
 * interface.
 *
 * The stack is the caller's to provide, and its top is passed rather than its
 * base because that is the end execution begins from on every architecture this
 * specification targets. An implementation whose environment gives the started
 * context a stack of its own ignores the argument, and reports that by
 * withholding nothing: a caller cannot observe which of the two occurred, and
 * has no decision resting upon it.
 *
 * ⭐ `entry' IS AN ADDRESS AT WHICH A CONTEXT BEGINS, NOT A FUNCTION THAT IS
 * CALLED. NO RETURN ADDRESS EXISTS: the started context stands at the top of a
 * stack with nothing beneath it, and returning from `entry' ends the context
 * with a status saying that it returned rather than choosing one. This was
 * already the behaviour every implementation had --- one of them records that
 * the transfer "cannot be written in C" for exactly this reason --- and saying
 * it here is what lets an implementation on the far side of a boundary
 * establish the context by setting a program counter and a stack pointer. */
int kal_space_start(void (*entry)(void*), void* arg, void* stack_top,
                    struct kal_process* out);

kal_uintptr kal_space_props(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_SPACE_H */
