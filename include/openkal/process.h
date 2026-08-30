/* openkal.process --- a program image that has been started. The operations
 * are to start one, to wait for it, and to request its termination.
 * Duplication of the calling image is not among them: duplicating an address
 * space and its execution state cannot be performed faithfully on every
 * environment this specification targets. */
#ifndef OPENKAL_PROCESS_H
#define OPENKAL_PROCESS_H
#include "types.h"
#include "fs.h"
/* For `kal_process_channel', whose pair of streams is what a parent speaks to
 * a started program through. A sibling header, per the rule this specification
 * keeps for its own headers. */
#include "stream.h"

struct kal_process { kal_uintptr h; };

/* How a started program's standard streams are supplied. A stream handle of
 * zero denotes that the program inherits the corresponding stream of its
 * parent, which is what an environment without a general mechanism for
 * passing handles can always provide.
 *
 * A STREAM PLACED HERE IS THE STARTED PROGRAM'S FOR AS LONG AS THAT PROGRAM
 * RUNS, AND THE CALLER MAY RELEASE ITS OWN REFERENCE AS SOON AS THE SPAWN HAS
 * RETURNED. `kal_process_channel' below already requires this of a caller in as
 * many words --- a parent that does not release `theirs' after the spawn never
 * observes the end of input on `mine' --- so an implementation that did not
 * carry the stream across would make that instruction impossible to follow. It
 * is stated here for streams in general, because a caller that places the stream
 * of a file it opened for the purpose is in the same position and had nothing to
 * read.
 *
 * ⚠️ ZERO IS RESERVED HERE AND IS NOT RESERVED IN `openkal.stream', WHICH IS A
 * COLLISION AND IS RECORDED RATHER THAN REPAIRED.
 *
 * `kal_stream' has no distinguished value: an implementation whose streams are
 * its environment's own descriptors answers `kal_stdin()' with zero, and
 * openkal-linux does. The two readings agree at position `in' --- placing
 * standard input at standard input and inheriting it are the same act --- and
 * cannot be told apart anywhere else, so a caller that places its own standard
 * input at position `out' or `err' is asking for something this structure cannot
 * express.
 *
 * ⇒ A caller that cannot tolerate the ambiguity DOES NOT PASS THE VALUE: it
 * reports the request as unsupported, which is what a library above this
 * interface can act upon, rather than passing on a word that will be read as
 * inheritance. An implementation MAY remove the ambiguity for its own resources
 * by not answering any stream enquiry with zero, and one that does so removes it
 * for every caller.
 *
 * Repairing it in this structure would mean a second declaration --- clause 8
 * forbids altering this one --- and the case it would serve is a caller that
 * sends a program's output to its own standard input. Recorded here so that the
 * next implementation meets it in the specification rather than in a program
 * that wrote to the wrong stream. */
struct kal_spawn_streams {
    struct kal_stream in;
    struct kal_stream out;
    struct kal_stream err;
};

/* One directory a started program shall receive among its preopens. The layout
 * is frozen (clause 5.3). */
struct kal_preopen {
    struct kal_dir dir;
    const char*    name;
    kal_uintptr    len;
};

/* Positions in kal_process_props. */
#define KAL_PROCESS_PROP_TERMINATE      ((kal_uintptr)1u << 0)
#define KAL_PROCESS_PROP_STREAM_PASSING ((kal_uintptr)1u << 1)
#define KAL_PROCESS_PROP_EXIT_STATUS    ((kal_uintptr)1u << 2)
#define KAL_PROCESS_PROP_CHANNEL        ((kal_uintptr)1u << 3)
#define KAL_PROCESS_PROP_GRANT_DIR      ((kal_uintptr)1u << 4)
/* kal_process_spawn_bound is answered here. Version 0.10. */
#define KAL_PROCESS_PROP_BOUND_LIFETIME ((kal_uintptr)1u << 5)

#ifdef __cplusplus
extern "C" {
#endif

/* Starts the program named relative to a directory. The argument vector is
 * complete and is passed unaltered: argv[0] is the name the started program
 * observes as its own, and an implementation neither derives it from path nor
 * prepends anything to the vector. Clause 7.6. */
int kal_process_spawn(struct kal_dir base,
                      const char*  path,  kal_uintptr path_len,
                      const char** argv,  const kal_uintptr* argv_lens, kal_uintptr argc,
                      const char** envp,  const kal_uintptr* envp_lens, kal_uintptr envc,
                      const struct kal_spawn_streams* streams,
                      struct kal_process* out);

/* Starts a program that receives exactly the directories named.
 *
 * The started program reads them back through `kal_fs_preopen', which is the
 * operation this one is the inverse of --- clause 7.11. A count of zero starts
 * a program with no preopens at all, which is a different thing from
 * `kal_process_spawn' and is the whole reason a caller reaches for this.
 *
 * A SECOND DECLARATION RATHER THAN AN ARGUMENT ADDED TO THE FIRST, because
 * clause 8 forbids altering an existing one. `kal_process_spawn' remains, and a
 * program that does not grant directories keeps using it. */
int kal_process_spawn_with(struct kal_dir base,
                           const char*  path,  kal_uintptr path_len,
                           const char** argv,  const kal_uintptr* argv_lens, kal_uintptr argc,
                           const char** envp,  const kal_uintptr* envp_lens, kal_uintptr envc,
                           const struct kal_spawn_streams* streams,
                           const struct kal_preopen* grants, kal_uintptr grant_count,
                           struct kal_process* out);

/* A pair of streams of which one end is intended to cross a spawn boundary.
 *
 * The caller holds `mine'; `theirs' is what it places in a
 * `kal_spawn_streams'. Bytes written to one end are read from the other, and
 * the pair is the mechanism a parent uses to speak to a child it started ---
 * which openkal otherwise has no way to express, because the standard streams
 * are borrowed and cannot be manufactured.
 *
 * BOTH ENDS ARE OWNED, AND THE RELEASE IS HERE. A parent that does not release
 * `theirs' after the spawn never observes the end of input on `mine' --- the
 * classic deadlock of this arrangement --- so an interface that hands out these
 * streams must also take them back. `kal_stream' has no release of its own
 * precisely because a stream in general has no owner. */
int  kal_process_channel(struct kal_stream* mine, struct kal_stream* theirs);
void kal_process_channel_close(struct kal_stream s);

int kal_process_wait(struct kal_process, int* status, int* terminated);
int kal_process_terminate(struct kal_process);
void kal_process_close(struct kal_process);

/* Starts a program whose lifetime is BOUND to the caller's: when the calling
 * image ends, however it ends, the started program ends too. Version 0.10.
 *
 * ⚠️⚠️ ADDED BECAUSE A LIBRARY ABOVE THIS INTERFACE HAD TO COMPOSE SOMETHING IT
 * COULD NOT THEN CONTROL, AND A CALLER WAS TOLD A FALSEHOOD ABOUT IT.
 *
 * Clause 7.1 declines to replace a running image, correctly. A C library asked
 * for `execve' therefore composes it: start the program, wait for it, end with
 * its status. That composition leaves THREE images where a system with the
 * operation has two --- the caller, the copy that is waiting, and the program.
 *
 * ⭐ AND THE MIDDLE ONE IS THE ONE A SIGNAL REACHES. `kal_process_terminate'
 * upon the identifier the caller holds terminates the WAITER. Measured, with a
 * host as control: identical status words, opposite outcomes --- the caller is
 * told the program died on the signal it sent, while the program runs to
 * completion, unsupervised. openkal-linux#13.
 *
 * ⚠️ `kal_process_terminate' is not at fault: it was asked to terminate one
 * started program and did. What was missing was a way to SAY the thing `execve'
 * means --- this program stands in for me, so it does not outlive me.
 *
 * Otherwise identical to kal_process_spawn. An implementation that cannot bind a
 * lifetime does not claim KAL_PROCESS_PROP_BOUND_LIFETIME and reports
 * kal_err_not_supported here rather than starting a program it cannot bind: a
 * caller that asked for the binding asked for it, and a program started without
 * it is not the program the caller asked to start.
 *
 * A SECOND DECLARATION RATHER THAN A FLAG ON THE FIRST, because clause 8 forbids
 * altering an existing one --- the same reason `kal_process_spawn_with' is a
 * declaration and not an argument.
 *
 * ⚠️ WHICH MEANS THE TWO ADDITIONS DO NOT COMBINE, AND THAT IS DELIBERATE. There
 * is no form that both grants directories and binds a lifetime. Declaring every
 * combination is how an interface acquires four spawns and then eight, so the
 * combination is declared when something needs it and not before; clause 8
 * permits adding it then. What a caller must not do meanwhile is take
 * `kal_process_spawn_with' and assume the binding: it does not bind, and a
 * program that outlives its caller is exactly the failure this exists to
 * remove. */
int kal_process_spawn_bound(struct kal_dir base,
                            const char*  path,  kal_uintptr path_len,
                            const char** argv,  const kal_uintptr* argv_lens, kal_uintptr argc,
                            const char** envp,  const kal_uintptr* envp_lens, kal_uintptr envc,
                            const struct kal_spawn_streams* streams,
                            struct kal_process* out);

kal_uintptr kal_process_props(void);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_PROCESS_H */
