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

/* Positions in kal_process_props --- what an IMPLEMENTATION can do. What a
 * CALLER asks for is the KAL_SPAWN_ word below, and the two are deliberately
 * separate: one describes the environment, the other one start. */
#define KAL_PROCESS_PROP_TERMINATE      ((kal_uintptr)1u << 0)
#define KAL_PROCESS_PROP_STREAM_PASSING ((kal_uintptr)1u << 1)
#define KAL_PROCESS_PROP_EXIT_STATUS    ((kal_uintptr)1u << 2)
#define KAL_PROCESS_PROP_CHANNEL        ((kal_uintptr)1u << 3)
/* A non-empty `grants' is answered here. */
#define KAL_PROCESS_PROP_GRANT_DIR      ((kal_uintptr)1u << 4)
/* KAL_SPAWN_BOUND_LIFETIME is answered here. Version 0.10. */
#define KAL_PROCESS_PROP_BOUND_LIFETIME ((kal_uintptr)1u << 5)
/* KAL_SPAWN_OWN_JOB is answered here. Version 0.11. */
#define KAL_PROCESS_PROP_OWN_JOB        ((kal_uintptr)1u << 6)

/* ⚠️⚠️ HOW A PROGRAM IS STARTED, AND IT IS ONE OPERATION BECAUSE 0.11 STOPPED
 * MAKING IT A FAMILY.
 *
 * Until 0.11 there were three declarations --- `kal_process_spawn',
 * `..._with' and `..._bound' --- and they were never three operations. They were
 * ONE operation and three modifiers of it, spelled as separate declarations for
 * exactly one reason: clause 8 forbids adding an argument to a declaration that
 * exists, so each new modifier had to arrive as a new name.
 *
 * ⭐ THE 0.10 COMMENT ON `..._bound' PREDICTED WHERE THAT ENDS, IN TERMS:
 * "Declaring every combination is how an interface acquires four spawns and then
 * eight, so the combination is declared when something needs it and not before."
 *
 * ⚠️ Something needed it, and the evidence was a single function signature in a
 * consumer --- `run_shell_stream(argv, cwd, …, timeout_ms)', whose child calls
 * `setpgid(0, 0)' and `chdir(cwd)' on ADJACENT LINES. Two more modifiers, wanted
 * together. Four orthogonal modifiers is sixteen declarations, each of which must
 * then be written in every implementation.
 *
 * ⇒ So the modifiers moved into a record and the family collapsed to its first
 * member. `..._with' and `..._bound' are GONE rather than kept beside it: openkal
 * has no users outside this ecosystem yet, so there is no one to keep a second
 * spelling for, and an interface that offers two ways to say one thing has to
 * explain the difference for ever. A caller written against the old shape fails
 * to COMPILE --- different arity, different types --- which is the loud failure and
 * not the quiet one.
 *
 * The layout is frozen (clause 5.3). */
struct kal_spawn {
    /* What `path' is resolved against. NAMING the program, and nothing else. */
    struct kal_dir base;

    /* ⭐ THE DIRECTORY THE PROGRAM RUNS IN, AND IT IS REQUIRED.
     *
     * openkal deliberately has no ambient working directory --- "wherever I happen
     * to be" is not something this interface can name --- so there is no default
     * that would be true, and a caller that does not care passes `base'.
     *
     * ⚠️ NAMING A PROGRAM AND NAMING WHERE IT RUNS ARE TWO DIRECTORIES. Before
     * 0.11 there was one, and a C library above could not answer
     * `posix_spawn_file_actions_addchdir_np' at all: `chdir' moved what the
     * LIBRARY resolved names against and the started program still ran where its
     * caller had been. Measured against a host, which answers the directory the
     * caller entered.
     *
     * ⇒ This does not weaken the refusal openkal already makes. Declining to
     * CHANGE a running program's working directory is right --- that is shared
     * mutable state between execution contexts --- and that reason has never
     * applied to stating, once, at the moment of starting, where a program runs. */
    struct kal_dir work;

    /* The directories the started program receives, read back through
     * `kal_fs_preopen'. A count of zero starts a program with no preopens at
     * all, which is a different thing from not asking. */
    const struct kal_preopen* grants;
    kal_uintptr               grant_count;

    kal_uintptr flags;   /* KAL_SPAWN_* */
};

/* The started program does not outlive its caller: when the calling image ends,
 * however it ends, the started program ends too.
 *
 * ⚠️ ADDED IN 0.10 BECAUSE A CALLER WAS TOLD A FALSEHOOD. Clause 7.1 declines to
 * replace a running image, so a C library asked for `execve' composes it --- and
 * the composition leaves THREE images where a system with the operation has two:
 * the caller, a copy that waits, and the program. A signal reaches the middle
 * one. Measured with a host as control: identical status words, opposite
 * outcomes --- the caller is told the program died on the signal it sent while the
 * program runs to completion, unsupervised. openkal-linux#13. */
#define KAL_SPAWN_BOUND_LIFETIME ((kal_uintptr)1u << 0)

/* The started program and everything IT starts form one unit, which
 * `kal_process_terminate' ends as a unit.
 *
 * ⚠️ ASKED FOR PER START, AND THAT IS NOT A CONVENIENCE. On the systems beneath
 * this interface the unit is a process group or a job, and entering a new one
 * LEAVES THE TERMINAL'S FOREGROUND GROUP --- so a program with an interface, whose
 * child then reads the terminal, stops on SIGTTIN. A caller that has given the
 * started program pipes for all three streams wants this; an interactive caller
 * must never get it. Neither a default nor a property of the implementation can
 * express that difference; a bit the caller sets can.
 *
 * ⭐ It is not what BOUND_LIFETIME does. That binds the started program to the
 * caller's life and reaches no further --- a grandchild outlives both. This is the
 * one a timeout needs: kill what was started, and what it started. */
#define KAL_SPAWN_OWN_JOB        ((kal_uintptr)1u << 1)

#ifdef __cplusplus
extern "C" {
#endif

/* Starts a program. The argument vector is complete and is passed unaltered:
 * argv[0] is the name the started program observes as its own, and an
 * implementation neither derives it from path nor prepends anything to the
 * vector. Clause 7.6.
 *
 * Everything that varies between one start and another is in `kal_spawn', which
 * is why this is one declaration and not a family --- see the comment there.
 *
 * An implementation reports kal_err_not_supported, and starts nothing, when the
 * request names something it cannot do: a flag whose position it does not claim
 * in `kal_process_props', or a non-empty `grants' without
 * KAL_PROCESS_PROP_GRANT_DIR. ⚠️ STARTING THE PROGRAM WITHOUT THE THING ASKED FOR
 * IS NOT AN OPTION --- a caller that asked for a bound lifetime and got a program
 * without one has been given a program that outlives it, which is the failure the
 * flag exists to remove. */
int kal_process_spawn(const struct kal_spawn* how,
                      const char*  path,  kal_uintptr path_len,
                      const char** argv,  const kal_uintptr* argv_lens, kal_uintptr argc,
                      const char** envp,  const kal_uintptr* envp_lens, kal_uintptr envc,
                      const struct kal_spawn_streams* streams,
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

kal_uintptr kal_process_props(void);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_PROCESS_H */
