/* openkal.env --- the parameters a program receives at inception.
 *
 * EVERY VALUE IS COPIED INTO THE CALLER'S BUFFER, and none is returned by
 * pointer. An earlier form answered with a pointer into the implementation's
 * own storage, which is meaningful only while the implementation shares the
 * caller's address space. Copying costs one buffer and makes the interface say
 * the same thing whether the implementation is linked in, loaded beside, or on
 * the far side of a boundary (clause 4.4).
 *
 * THE SHAPE IS THE ONE EVERY COUNTING OPERATION HAS. Each of these returns
 * the length the value HAS --- not the length it wrote --- or the negated error
 * value. So a caller with a buffer large enough is done in one call, a caller
 * that wants to size first passes a capacity of zero, and a caller whose buffer
 * was too small learns it by comparing. Nothing is truncated silently and
 * nothing needs a second out-parameter to say so.
 *
 * THE SET DOES NOT CHANGE while the program runs. It is what the program was
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

/* The name of the image this program was started from, as the environment
 * states it to the program: the name the environment resolved when it started
 * the program, not the name in argument zero, which the starter chooses (clause
 * 7.6). Version 0.17.
 *
 * Every environment that starts programs from named images states it --- the
 * name the kernel resolved, the path the loader was given, the name recorded in
 * the process's own parameters --- and a program cannot recover it otherwise:
 * argument zero is whatever its starter wrote, and a search of the directories
 * a starter searched is a guess about a decision already made. A program that
 * finds its own data beside itself needs it.
 *
 * The name is in the environment's own spelling and is meaningful to the
 * environment rather than to this interface: it is not resolved against a
 * preopen, and a program uses it as it uses any name it was given from outside.
 * An environment that starts programs from no named image reports
 * kal_err_not_found, negated. Where a program is an image started by a loader
 * above openkal (clause 12), the loader's implementation states the image's
 * name, not its own. */
kal_intptr kal_env_image(char* out, kal_uintptr cap);

/* The name of the environment's system: `linux', `macos', `windows', and
 * further names as implementations are written, lower case, from an open set.
 * Version 0.17.
 *
 * IT EXISTS FOR CONVENTIONS AND FOR NOTHING ELSE. The conventions of a system
 * --- where it keeps a user's configuration, how it separates the entries of a
 * list of directories, the suffix of a program it starts --- are decided by the
 * system and are not resources of this interface, and a program built once and
 * run upon several systems (clause 12) learns which system's conventions to
 * follow here. A program shall not use the name to change how it calls an
 * operation of this specification, or to infer what an operation can do: what
 * an implementation can do is answered by its capability words (clause 6.2),
 * which are true of the implementation in front of the program, and a name is
 * true of a class of environment, which is the thing clause 3.3 records a name
 * cannot be. An implementation whose environment has no such name reports
 * kal_err_not_found, negated. */
kal_intptr kal_env_system(char* out, kal_uintptr cap);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_ENV_H */
