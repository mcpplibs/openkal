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
 * ZERO IS RESERVED HERE AND IS NOT RESERVED IN `openkal.stream', WHICH IS A
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
/* kal_process_stop_requested answers a word rather than null. Version 0.11. */
#define KAL_PROCESS_PROP_STOP_REQUESTED ((kal_uintptr)1u << 7)
/* kal_process_open_id finds a program by its identifier. Version 0.17. */
#define KAL_PROCESS_PROP_OPEN_ID        ((kal_uintptr)1u << 8)
/* KAL_SPAWN_ONE_STRING is answered here. Version 0.17.1. */
#define KAL_PROCESS_PROP_ONE_STRING     ((kal_uintptr)1u << 9)

/* HOW A PROGRAM IS STARTED, AND IT IS ONE OPERATION BECAUSE 0.11 STOPPED
 * MAKING IT A FAMILY.
 *
 * Until 0.11 there were three declarations --- `kal_process_spawn',
 * `..._with' and `..._bound' --- and they were never three operations. They were
 * ONE operation and three modifiers of it, spelled as separate declarations for
 * exactly one reason: clause 8 forbids adding an argument to a declaration that
 * exists, so each new modifier had to arrive as a new name.
 *
 * THE 0.10 COMMENT ON `..._bound' PREDICTED WHERE THAT ENDS, IN TERMS:
 * "Declaring every combination is how an interface acquires four spawns and then
 * eight, so the combination is declared when something needs it and not before."
 *
 * Something needed it, and the evidence was a single function signature in a
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

    /* THE DIRECTORY THE PROGRAM RUNS IN, AND IT IS REQUIRED.
     *
     * openkal deliberately has no ambient working directory --- "wherever I happen
     * to be" is not something this interface can name --- so there is no default
     * that would be true, and a caller that does not care passes `base'.
     *
     * NAMING A PROGRAM AND NAMING WHERE IT RUNS ARE TWO DIRECTORIES. Before
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

    /* THE UNIT THIS PROGRAM BELONGS TO, AND IT IS THE ONE FIELD HERE THAT IS
     * WRITTEN AS WELL AS READ.
     *
     *   null            this program belongs to no unit of the caller's making
     *   *job == 0       a new unit is formed, and its identity is written here
     *   *job != 0       this program joins the unit that word names
     *
     * IN AND OUT BECAUSE NEITHER KIND OF SYSTEM CAN DO IT THE OTHER WAY. This
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
     * ENTERING A UNIT HAS A COST ON SOME SYSTEMS AND THAT IS WHY IT IS PER
     * START. Where the unit is a process group, entering a new one LEAVES THE
     * TERMINAL'S FOREGROUND GROUP, so a program with an interface whose child
     * then reads the terminal stops on SIGTTIN. A caller that has given the
     * started program pipes for all three streams wants a unit; an interactive
     * caller must never be given one silently.
     *
     * AND THE TWO KINDS OF UNIT ARE NOT EQUALLY STRONG. A job is named by a
     * handle that is never reused; a process group is named by a process
     * identifier, which is reused once the leader has ended and the numbers have
     * wrapped. Terminating a unit whose leader is long gone can therefore reach a
     * different unit on such a system. This is what those systems do --- every
     * program that calls `killpg' lives with it --- and it is recorded rather than
     * hidden behind an interface that reads as though it were not so.
     *
     * AND THIS IS WHERE A UNIT IS ESTABLISHED --- AT THE START, BY WHOEVER
     * STARTS. A caller names the unit here and the implementation performs the
     * placement, so the unit and the program that can name it are on the same
     * side of the boundary. `kal_process_job_enter' is the other half and is not
     * a way around this: it places THE CALLER, for the sake of the spawns the
     * caller goes on to perform.
     *
     * IT FOLLOWS THAT A COPY'S UNIT CANNOT BE NAMED BY THE ORIGINAL, and a C
     * library composing job control meets this rather than reading it. Duplicate
     * the image, have the copy form a unit, then signal that unit from the
     * original: the copy's handle is the copy's --- clause 6.7 builds it from an
     * index into the copy's own table --- and clause 11 entry 9 conveys nothing
     * back. The composition has no closure, and the entry records what an
     * implementation did when it met the dead end rather than leaving the next
     * one to repeat it. */
    struct kal_job* job;

    /* The directories the started program receives, read back through
     * `kal_fs_preopen' in the order given and under the names given. A count
     * of zero starts a program with no preopens at all, which is a different
     * thing from not asking (a null `grants'), which leaves the directories an
     * implementation supplies by default.
     *
     * The first grant is the directory the started program regards as the one
     * it was started in, as the first preopen is for every program; a C
     * library above openkal resolves a relative name against it and an absolute
     * name against the grant whose name is the longest prefix, so names that
     * are absolute paths serve such a program best. A grant confines a program
     * that uses only its preopens; one that reaches beyond them through its
     * environment is confined by the environment or not at all. How a grant is
     * conveyed is the implementation's concern. Clause 7.13. */
    const struct kal_preopen* grants;
    kal_uintptr               grant_count;

    kal_uintptr flags;   /* KAL_SPAWN_* */
};

/* The started program does not outlive its caller: when the calling image ends,
 * however it ends, the started program ends too.
 *
 * ADDED IN 0.10 BECAUSE A CALLER WAS TOLD A FALSEHOOD. Clause 7.1 declines to
 * replace a running image, so a C library asked for `execve' composes it --- and
 * the composition leaves THREE images where a system with the operation has two:
 * the caller, a copy that waits, and the program. A signal reaches the middle
 * one. Measured with a host as control: identical status words, opposite
 * outcomes --- the caller is told the program died on the signal it sent while the
 * program runs to completion, unsupervised. openkal-linux#13. */
#define KAL_SPAWN_BOUND_LIFETIME ((kal_uintptr)1u << 0)

/* THE VECTOR JOINED AS IT IS, WHERE A PROGRAM RECEIVES ONE STRING. Version
 * 0.17.1, clause 7.6.
 *
 * An environment that gives a started program one string, which the program
 * splits by rules of its own, ordinarily quotes each element so that the
 * conventional splitting recovers the vector. A program whose rules are not the
 * convention cannot be given every argument through that quoting: the command
 * interpreter takes the rest of the string after `/c' as a command line of its
 * own grammar, and a quoted element arrives with the quoting's escapes in it.
 * With this flag the first element is quoted as usual and the others are joined
 * after it by single spaces, as they are. An environment that gives a program a
 * vector answers kal_err_not_supported: the request means nothing there. */
#define KAL_SPAWN_ONE_STRING ((kal_uintptr)1u << 1)

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
 * EACH ENVIRONMENT ENTRY IS `NAME=value', and an entry that holds no `=', or
 * whose name is empty, is kal_err_invalid and starts nothing. Version 0.17
 * states it: one implementation refused such an entry and another passed it to
 * the started program, so a caller could not know which program it was
 * starting. `kal_env_var_at' answers a NAME, so a caller copying its own
 * environment composes the entry from `kal_env_var_at' and `kal_env_var'.
 *
 * An implementation reports kal_err_not_supported, and starts nothing, when the
 * request names something it cannot do: a flag whose position it does not claim
 * in `kal_process_props', or a non-empty `grants' without
 * KAL_PROCESS_PROP_GRANT_DIR. STARTING THE PROGRAM WITHOUT THE THING ASKED FOR
 * IS NOT AN OPTION --- a caller that asked for a bound lifetime and got a program
 * without one has been given a program that outlives it, which is the failure the
 * flag exists to remove.
 *
 * A START THAT DID NOT HAPPEN IS REPORTED, AND IS REPORTED BY ITS REASON. An
 * implementation shall not report kal_ok, and shall not yield a process, for a
 * program that was not started; a caller that receives a handle for a start
 * that failed can discover the failure only as an exit status, which cannot be
 * told apart from a program that ran and returned that status. The reason is
 * the one the environment gave: a name that is not there is kal_err_not_found,
 * one that may not be started is kal_err_permission, a directory is
 * kal_err_is_directory, and a name that is there and may be started but is not
 * in a form this environment can start is kal_err_not_program (version 0.13).
 * The last is distinct from kal_err_io because no device reported a failure,
 * and a C library above maps it back to the condition its callers act upon. */
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
 * THE PAIR HAS ONE DIRECTION, AND IT IS STATED BECAUSE IT WAS NOT. Version
 * 0.17. `mine' is the end that READS and `theirs' the end that WRITES: placed
 * as a started program's `out' or `err', `theirs' carries what the program
 * writes to `mine'. A caller that writes to a started program's input asks for
 * a second pair and uses it the other way about --- places `mine' at `in' and
 * writes to `theirs'. Before 0.17 the text named no direction, both
 * implementations of the time made `mine' the reading end, and a caller that
 * guessed the other way wrote into an end that refused (measured: a consumer's
 * first program to feed a child's input read nothing).
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
 * ITS MEANING DOES NOT DEPEND ON HOW THAT PROGRAM WAS STARTED, and 0.11 is
 * where that became true. The shape this replaced was a flag on the spawn, after
 * which this operation reached one program or a whole tree according to a
 * property of the handle that no caller could see. An operation whose meaning
 * turns on invisible state is the thing this interface exists to avoid, so the
 * unit has an operation of its own and the caller says which it means. */
int kal_process_terminate(struct kal_process);
void kal_process_close(struct kal_process);

/* A WORD THIS PROGRAM'S ENVIRONMENT SETS WHEN SOMEBODY HAS ASKED IT TO END.
 * Zero until then, non-zero afterwards, and never cleared. Null where an
 * implementation does not claim KAL_PROCESS_PROP_STOP_REQUESTED.
 *
 * THIS IS NOT A SIGNAL INTERFACE, AND THE DIFFERENCE IS WHY IT CAN EXIST AT
 * ALL. Signals are one environment's mechanism: a numbered set, a disposition per
 * program, a handler that interrupts whatever was running. This system has them;
 * that one has console control events, which arrive on a NEW EXECUTION CONTEXT
 * and cover a different set of causes; two of the environments this
 * specification targets have no processes to signal. An interface shaped like
 * signals would be the borrowed shape clause 7.1 names.
 *
 * WHAT WAS ACTUALLY MISSING WAS ONE SENTENCE, and it is the only one of the
 * uses of signals that this interface could not already express:
 *
 *     terminating a program            kal_process_terminate
 *     terminating a unit               kal_process_job_terminate
 *     the far end of a stream is gone  reported by kal_stream_write
 *     a started program ended          kal_process_wait
 *     a bound upon waiting             openkal.timeout
 *     waking another context           kal_task_wake
 *     THIS PROGRAM HAS BEEN ASKED TO END        --- nothing, until now
 *
 * ⇒ A WORD AND NOT A HANDLER. A handler is a call into arbitrary code at an
 * arbitrary point, which is the part of signals that does not travel: the other
 * system runs its notification on a context of its own, and reproducing an
 * interruption there would be a compatibility layer. A word is read when the
 * program chooses, or WAITED UPON with `kal_task_wait' --- which this
 * specification already has, and which `openkal.timeout' already bounds. No new
 * concept, and none of the re-entrancy rules a handler forces on every caller.
 *
 * IT IS A NOTICE AND NOT A VETO. The program is told; whether it ends is its
 * own affair, and it may still be ended afterwards by something it cannot
 * observe --- which is true on every system this targets and is why the word says
 * "requested" rather than "will happen".
 *
 * IT DOES NOT SAY WHO ASKED, OR HOW. One word, because a program that is
 * asked to end does the same thing whoever asked. An implementation that can
 * distinguish causes may add an enquiry later; this one does not need altering
 * for that.
 *
 * ONE OWNER FROM THE FIRST CALL. Version 0.17. From the first call of this
 * operation in a program, a request to end that reaches the program is
 * delivered through the word and through nothing else: the implementation
 * replaces whatever its environment would have done upon the request,
 * including an arrangement the program itself made earlier by means outside
 * this interface. A request that the party which STARTED the program arranged
 * not to be delivered at all --- an environment lets a starter say so, and a
 * program started in the background is commonly started that way --- remains
 * undelivered: taking it over would end a program its starter meant to be
 * immune, and the word would read zero for ever, which is true. Before 0.17
 * the text said neither, and two implementations restored delivery of a
 * request their starter had declined.
 *
 * AND IT SAYS NOTHING ABOUT AN END THAT CANNOT BE OBSERVED. Where a program is
 * ended outright --- the un-declinable signal here, the abrupt termination there,
 * `kal_process_job_terminate' through this interface --- nothing is set, because
 * nothing anywhere gets to notice. That symmetry is the reason this is
 * implementable on both kinds of system rather than on one. */
const kal_u32* kal_process_stop_requested(void);

/* ENDS THIS PROGRAM AS THE REQUEST TO END WOULD HAVE ENDED IT, had the program
 * not taken the request over. Version 0.17.
 *
 * A program that has been asked to end, and has finished what it does before
 * ending, should end in the way that tells its starter it was asked to: a
 * starter that waits observes `terminated' from `kal_process_wait', and a shell
 * running a loop stops the loop rather than starting the next program. Ending
 * through `kal_exit' with any status tells the starter something else ---
 * that the program chose to end --- and nothing above this interface can
 * produce the other report, because it is the environment's own report of an
 * end it caused.
 *
 * Returns only if the program has not been asked to end, with
 * kal_err_invalid: there is no request whose manner could be reproduced. Where
 * the program has been asked to end, it does not return. Every environment that
 * provides `kal_process_stop_requested' can do it --- restoring the request's
 * default treatment and delivering it again, or ending with the status the
 * environment records for such a request. Clause 11 entry 26. */
int kal_process_end_as_requested(void);

/* WHAT IS KNOWN OF A PROGRAM THAT HAS BEEN STARTED. Version 0.17.
 *
 * The shape is `kal_node_info's (clause 5.3): the caller sets `self_size' and
 * names in `wanted' the fields it needs; the implementation fills what it can
 * and reports in `present' which, and writes no more of the structure than
 * `self_size' states. Fields may be appended.
 *
 * `id' is the identifier the environment gives the program, an opaque value
 * that MAY BE GIVEN AGAIN to another program once this one has ended and been
 * waited for. `started_ns' is when the program started, on a clock of the
 * environment's choosing that does not repeat, so the pair (id, started_ns)
 * names one program for as long as the environment runs; a caller that keeps an
 * identifier keeps the pair, and compares both. `cpu_ns' is the processor time
 * the program has used, in every context it ran. `ended' is non-zero once the
 * program has ended. */
struct kal_program_info {
    kal_u32 self_size;
    kal_u32 present;
    kal_u64 id;
    kal_u64 started_ns;
    kal_u64 cpu_ns;
    kal_u32 ended;
    kal_u32 reserved;
};

/* Positions in `wanted' and `present'. */
#define KAL_PINFO_ID      ((kal_u32)1u << 0)
#define KAL_PINFO_STARTED ((kal_u32)1u << 1)
#define KAL_PINFO_CPU     ((kal_u32)1u << 2)
#define KAL_PINFO_ENDED   ((kal_u32)1u << 3)
#define KAL_PINFO_ALL     (KAL_PINFO_ID | KAL_PINFO_STARTED | KAL_PINFO_CPU | KAL_PINFO_ENDED)

/* A handle to THIS program, owned and released by kal_process_close. It may be
 * asked about with kal_process_info, and is refused by kal_process_wait and by
 * kal_process_terminate with kal_err_invalid: a program does not wait for
 * itself, and ending itself is kal_exit. Version 0.17. */
int kal_process_self(struct kal_process* out);

/* A handle to the program whose identifier is `id', owned and released by
 * kal_process_close. Version 0.17.
 *
 * A program learns another's identifier from outside this interface --- a
 * client states its own in a message, a file names the program that holds it
 * --- and wants to know whether that program is still there and what it has
 * used. The handle may be asked about with kal_process_info, and is refused by
 * kal_process_wait and kal_process_terminate with kal_err_not_supported: a
 * program the caller did not start is not the caller's to wait for or to end,
 * and on environments where it could be, the caller would be ending a program
 * by a number that may since name another (the reason `started_ns' exists).
 *
 * No such program is kal_err_not_found; one the caller may not ask about is
 * kal_err_permission. An implementation that does not claim
 * KAL_PROCESS_PROP_OPEN_ID reports kal_err_not_supported. Where the environment
 * can hold the program itself --- a handle that keeps its identifier from being
 * given again --- the handle does; where it cannot, the implementation records
 * `started_ns' at the open and reports `ended' set once the identifier names a
 * program that started at another time. */
int kal_process_open_id(kal_u64 id, struct kal_process* out);

/* Fills what is known of a program. Any handle this interface issued may be
 * asked about: one from kal_process_spawn, kal_process_self, or
 * kal_process_open_id. `out->self_size' shall be set by the caller; a size
 * smaller than the fields that precede `id' is kal_err_invalid. */
int kal_process_info(struct kal_process p, kal_u32 wanted, struct kal_program_info* out);

/* Puts THE CALLING program into a unit, on the same terms as `kal_spawn.job':
 * a word of zero forms a new unit and receives its identity, and a word that
 * names one joins it.
 *
 * THE CALLER AND NOT A PROGRAM IT STARTS, WHICH IS THE WHOLE DIFFERENCE, and
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
 * THIS IS NOT THE MUTABLE AMBIENT STATE openkal DECLINES ELSEWHERE. A working
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

/* Releases the caller's reference to a unit. IT DOES NOT END THE UNIT, and an
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
