/* openkal.env --- the parameters a program receives at inception.
 *
 * ⭐ EVERY VALUE IS COPIED INTO THE CALLER'S BUFFER, and none is returned by
 * pointer. An earlier form answered with a pointer into the implementation's
 * own storage, which is meaningful only while the implementation shares the
 * caller's address space. Copying costs one buffer and makes the interface say
 * the same thing whether the implementation is linked in, loaded beside, or on
 * the far side of a boundary (clause 4.4).
 *
 * ⭐ THE SHAPE IS THE ONE EVERY COUNTING OPERATION HAS. Each of these returns
 * the length the value HAS --- not the length it wrote --- or the negated error
 * value. So a caller with a buffer large enough is done in one call, a caller
 * that wants to size first passes a capacity of zero, and a caller whose buffer
 * was too small learns it by comparing. Nothing is truncated silently and
 * nothing needs a second out-parameter to say so.
 *
 * ⚠️ THE SET DOES NOT CHANGE while the program runs. It is what the program was
 * started with. A consumer may therefore enumerate names and then look each one
 * up, which is what makes two small operations sufficient where one large one
 * with two buffers would otherwise be needed.
 */
#ifndef OPENKAL_ENV_H
#define OPENKAL_ENV_H
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The number of arguments, and the argument at a given position. Position zero
 * is the name by which the program was started; an environment that has no such
 * name reports an empty string rather than omitting it.
 *
 * Writes min(cap, length) bytes at out and returns the length. No terminator is
 * written: a length is what this interface carries, and a caller that wants one
 * appends it having been told how long the value is. */
kal_uintptr kal_env_arg_count(void);
kal_intptr  kal_env_arg(kal_uintptr index, char* out, kal_uintptr cap);

/* The value of a named variable. Reports kal_err_not_found, negated, when there
 * is no such name --- which is distinct from a name whose value is empty, and
 * that distinction is why this does not report a length of zero for both. */
kal_intptr kal_env_var(const char* name, kal_uintptr name_len,
                       char* out, kal_uintptr cap);

/* Enumeration, for a program that must copy the whole set. This answers the
 * NAME at a position; the value is then obtained by kal_env_var.
 *
 * Two operations rather than one that answers both: an operation answering a
 * name and a value at once needs two buffers, two capacities and two lengths,
 * and the second half of it is kal_env_var written a second time. The order of
 * positions is unspecified but does not change while the program runs, so a
 * caller may hold an index across the two calls. */
kal_uintptr kal_env_var_count(void);
kal_intptr  kal_env_var_at(kal_uintptr index, char* out, kal_uintptr cap);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_ENV_H */
