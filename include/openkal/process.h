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
/* A non-null `job' in kal_spawn is answered here. Version 0.11. */
#define KAL_PROCESS_PROP_JOB            ((kal_uintptr)1u << 6)

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

    /* ⭐⭐ THE UNIT THIS PROGRAM BELONGS TO, AND IT IS THE ONE FIELD HERE THAT IS
     * WRITTEN AS WELL AS READ.
     *
     *   null            this program belongs to no unit of the caller's making
     *   *job == 0       a new unit is formed, and its identity is written here
     *   *job != 0       this program joins the unit that word names
     *
     * ⚠️ IN AND OUT BECAUSE NEITHER KIND OF SYSTEM CAN DO IT THE OTHER WAY. This
     * system creates the unit first and puts members into it; that one has the
     * unit created BY its first member --- a process group's identity is a
     * process's --- so there is nothing to create beforehand. An operation that
     * opened an empty unit would be natural to one and impossible to the other,
     * and clause 7.1 says which of the two is then at fault. Establishing the
     * identity at the first start is the only shape both perform without keeping
     * a table.
     *
     * ⇒ Unchanged if the start fails. An implementation that does not claim
     * KAL_PROCESS_PROP_JOB reports kal_err_not_supported for a non-null `job'
     * rather than starting a program outside the unit that was asked for.
     *
     * ⚠️ ENTERING A UNIT HAS A COST ON SOME SYSTEMS AND THAT IS WHY IT IS PER
     * START. Where the unit is a process group, entering a new one LEAVES THE
     * TERMINAL'S FOREGROUND GROUP, so a program with an interface whose child
     * then reads the terminal stops on SIGTTIN. A caller that has given the
     * started program pipes for all three streams wants a unit; an interactive
     * caller must never be given one silently.
     *
     * ⚠️ AND THE TWO KINDS OF UNIT ARE NOT EQUALLY STRONG. A job is named by a
     * handle that is never reused; a process group is named by a process
     * identifier, which is reused once the leader has ended and the numbers have
     * wrapped. Terminating a unit whose leader is long gone can therefore reach a
     * different unit on such a system. This is what those systems do --- every
     * program that calls `killpg' lives with it --- and it is recorded rather than
     * hidden behind an interface that reads as though it were not so. */
    struct kal_job* job;

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

/* A set of started programs that end together --- see `kal_spawn.job'. One
 * machine word, as every handle here is. */
struct kal_job { kal_uintptr h; };

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

/* Requests the termination of ONE started program, whatever unit it is in.
 *
 * ⭐ ITS MEANING DOES NOT DEPEND ON HOW THAT PROGRAM WAS STARTED, and 0.11 is
 * where that became true. The shape this replaced was a flag on the spawn, after
 * which this operation reached one program or a whole tree according to a
 * property of the handle that no caller could see. An operation whose meaning
 * turns on invisible state is the thing this interface exists to avoid, so the
 * unit has an operation of its own and the caller says which it means. */
int kal_process_terminate(struct kal_process);
void kal_process_close(struct kal_process);

/* Puts THE CALLING program into a unit, on the same terms as `kal_spawn.job':
 * a word of zero forms a new unit and receives its identity, and a word that
 * names one joins it.
 *
 * ⭐⭐ THE CALLER AND NOT A PROGRAM IT STARTS, WHICH IS THE WHOLE DIFFERENCE, and
 * `kal_spawn.job' alone could not express it.
 *
 * A C library above this interface answers `fork'. The copy then wishes to be
 * the start of a unit BEFORE it replaces itself --- which is what
 * `setpgid(0, 0); exec…' means, and what every shell and every runner with a
 * timeout is written as. Expressed only through the spawn, the unit formed
 * belongs to the program the copy STARTS, whose identity the original never
 * learns, so the original's request to end that unit names one that does not
 * exist.
 *
 * ⚠️ THIS IS NOT THE MUTABLE AMBIENT STATE openkal DECLINES ELSEWHERE. A working
 * directory that can be changed is shared between execution contexts and is
 * refused for that reason. A unit is not shared and not read back: a program
 * states once which unit it belongs to, and the only thing that can be done with
 * the answer afterwards is to end the unit.
 *
 * An implementation that does not claim KAL_PROCESS_PROP_JOB reports
 * kal_err_not_supported and leaves the word as it was. */
int kal_process_job_enter(struct kal_job*);

/* Requests the termination of every program in a unit, including programs
 * started by its members that the caller never held a handle to. That last part
 * is the whole reason a unit exists: a program that starts work in the
 * background leaves nothing for a caller to terminate one at a time. */
int kal_process_job_terminate(struct kal_job);

/* Releases the caller's reference to a unit. ⚠️ IT DOES NOT END THE UNIT, and an
 * implementation must take care that it does not: this system's job objects can
 * be asked to end their members when the last handle closes, and one that asked
 * for that would make this operation mean something different here from what it
 * means where the unit is a process group and closing is releasing a number.
 * Terminating is `kal_process_job_terminate' and nothing else is. */
void kal_process_job_close(struct kal_job);

kal_uintptr kal_process_props(void);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_PROCESS_H */
