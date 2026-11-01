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
 * the implementation's interfaces (clause 12.3); an implementation may provide
 * it to a program built for its environment. Where nothing provides it, it is
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

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_PROF_H */
