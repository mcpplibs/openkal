/* openkal.prof --- what a program states about its own work, for whoever is
 * measuring it.
 *
 * Version 0.20. Clause 11 entry 45 records why it was admitted.
 *
 * A MEASUREMENT FROM OUTSIDE SEES CALLS AND SAMPLES; IT DOES NOT SEE WHAT THE
 * CALLS WERE FOR. A loader that binds an image's names can count every call
 * across this interface and sample where the program is (clause 12.3), and
 * needs nothing from the program to do so. What it cannot know is what the
 * program's own layers count --- how often an allocator went to the
 * environment, how long a context waited for a lock, which phase of the work is
 * under way. This interface is how a program states those to the measurement,
 * and it is designed so that stating them costs nothing when nobody measures.
 *
 * WHO PROVIDES IT. A loader provides it to the images it binds, in addition to
 * the implementation's interfaces (clause 12.3), and forwards samples to its own
 * implementation; an implementation provides it to a program built for its
 * environment (from version 0.23; before, it may), answering kal_prof_active
 * with zero where nothing measures that program. Where nothing provides it, it is
 * absent (clause 6.1), and a program that wants it where present takes the
 * names weakly (clause 12.2).
 *
 * IT CARRIES NOTHING OF THE ENVIRONMENT. Names are the program's, counts and
 * durations are numbers, and nothing an environment identifies crosses (clause
 * 1.1).
 *
 * Clause 4.4: S, L and X. A counter is the caller's storage; the measurement
 * reads it, as an implementation across a boundary reads any buffer passed to
 * it. */
#ifndef OPENKAL_PROF_H
#define OPENKAL_PROF_H
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Non-zero when a measurement records this program, zero otherwise; the answer
 * does not change while the program runs. A layer asks once, when it starts,
 * and registers nothing when the answer is zero --- which is how the cost of
 * this interface is nothing when nobody measures. */
int kal_prof_active(void);

/* Registers a counter: `name' (`len' bytes, copied) and the address of a cell
 * the caller keeps and updates. The measurement reads the cell when it records
 * --- at intervals and when the program ends --- and the caller does nothing
 * more: it increments its own memory, with no call per update. The cell remains
 * valid for the rest of the program and is read as one 64-bit load; a caller
 * that updates it from several contexts does so atomically.
 *
 * Returns kal_ok. Where no measurement records the program it does nothing and
 * returns kal_ok, so that a caller need not ask first. A measurement that can
 * hold no more counters reports kal_err_no_memory. Two counters of one name are
 * two counters. */
int kal_prof_counter(const char* name, kal_uintptr len, const volatile kal_u64* cell);

/* Marks the beginning (`begin' non-zero) or the end of a phase of the program's
 * work, named by `name' (`len' bytes), on the calling context. Phases nest; an
 * end closes the innermost phase of that name. Where no measurement records the
 * program it does nothing. */
void kal_prof_mark(const char* name, kal_uintptr len, int begin);

/* SAMPLES (version 0.23): where the program's processor time goes, taken by
 * the environment. While sampling, the implementation interrupts each context
 * of this program `per_second' times a second of the processor time it uses
 * and records the time, the context and the addresses of its frames, innermost
 * first, from its chain of frame pointers; kal_prof_sample_take moves up to
 * `count' of the recorded samples to `out' and returns how many, in the order
 * they were taken, and kal_prof_sample_end stops. A sample that falls while the
 * implementation holds as many as it can is not recorded: a caller that takes
 * them as they come loses none.
 *
 * WHY THE ENVIRONMENT TAKES THEM. Interrupting a running context and reading
 * where it stands needs what only the environment has --- a timer delivered to
 * the context, or another context that may suspend it --- and the
 * environment's own profilers do not name an image's addresses on two of the
 * three systems. Taken here, the same samples are had everywhere, and are named
 * from the images' symbol tables (kal_debug_symbolize) by whoever reads them.
 *
 * kal_prof_sample_begin reports kal_err_not_supported where KAL_PROF_PROP_SAMPLES
 * is not set, kal_err_invalid for a rate of zero or above 10 000, and
 * kal_err_again while sampling already. Clause 11 entry 56. */
#define KAL_PROF_SAMPLE_DEPTH 64u
struct kal_prof_sample {
    kal_u64     time_ns;     /* the monotonic clock (openkal.time) */
    kal_u64     context;     /* kal_task_current() of the sampled context */
    kal_u32     depth;       /* entries of `pc' that hold an address */
    kal_u32     reserved;    /* zero */
    kal_uintptr pc[KAL_PROF_SAMPLE_DEPTH];
};

#define KAL_PROF_PROP_SAMPLES ((kal_uintptr)1u << 0)

kal_uintptr kal_prof_props(void);
int        kal_prof_sample_begin(kal_u32 per_second);
kal_intptr kal_prof_sample_take(struct kal_prof_sample* out, kal_uintptr count);
int        kal_prof_sample_end(void);

/* TOUCHES (version 0.23): the order in which the program first executes the
 * pages of a part of a region (openkal.exec), taken by the environment. After
 * kal_prof_touch_begin, the implementation records the first execution of
 * each page of kal_exec_granularity in [`at', `at' + `size') --- a part that is
 * executable, published or a file's --- once, in the order they come;
 * kal_prof_touch_take moves up to `count' of the recorded addresses, each the
 * start of a page, to `out' and returns how many, in that order; and
 * kal_prof_touch_end stops recording in every part, as the parts were. A part
 * is added by each kal_prof_touch_begin, to as many as the implementation
 * holds (kal_err_no_memory beyond). The program runs as it would: a touch is
 * recorded and the instruction carried out.
 *
 * WHY THE ENVIRONMENT TAKES THEM. Which page a program reaches first is known
 * where the page is first reached --- the environment's fault on a page not
 * yet made executable, or on one guarded for the purpose --- and a loader
 * above this interface has neither. The order lays out an image's code so
 * that what a start executes is together, and states the pages a loader reads
 * in at once (KAL_EXEC_MAP_PREFETCH; clause 12.4, type 4).
 *
 * kal_prof_touch_begin reports kal_err_not_supported where KAL_PROF_PROP_TOUCHES
 * is not set, and kal_err_invalid for a size of zero, for `at' or `size' not a
 * multiple of kal_exec_granularity, or for a part not executable. Clause 11
 * entry 56. */
#define KAL_PROF_PROP_TOUCHES ((kal_uintptr)1u << 1)

int        kal_prof_touch_begin(const void* at, kal_uintptr size);
kal_intptr kal_prof_touch_take(kal_uintptr* out, kal_uintptr count);
int        kal_prof_touch_end(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_PROF_H */
