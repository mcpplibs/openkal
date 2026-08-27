/* openkal.terminal --- what an interactive stream does with what is typed at
 * it.
 *
 * The resource is a stream for which `kal_stream_props' reports
 * KAL_STREAM_PROP_INTERACTIVE. Every operation here reports
 * kal_err_not_supported for any other stream.
 *
 * A SEPARATE INTERFACE RATHER THAN OPERATIONS UPON `openkal.stream', for the
 * reason clause 6.4 gives when it places positioning in `openkal.fs': the
 * behaviour varies between the RESOURCES of the stream interface rather than
 * between implementations. An implementation could neither claim these
 * operations honestly for a file nor withhold them usefully for a terminal, and
 * an interface containing an operation some of its resources can never satisfy
 * is the defect this specification's decomposition exists to avoid.
 *
 * THE PAIR IS get/set AND NOT TWO SETTERS. A program that turns line editing
 * off must be able to put back what was there, and a setter alone gives it
 * nothing to put back --- it would restore a default, and the terminal a user
 * returns to is then not the one they had. Clause 7.11 states the general rule
 * that an enquiry has an inverse; this is an instance of it. */
#ifndef OPENKAL_TERMINAL_H
#define OPENKAL_TERMINAL_H
#include "types.h"
#include "stream.h"

/* Positions in the mode word.
 *
 * A position that has not been assigned reads as zero, so a program compiled
 * against a later revision of this specification behaves correctly against an
 * earlier implementation (clause 6.2). */
#define KAL_TERM_LINE_EDIT ((kal_uintptr)1u << 0)  /* the environment assembles lines  */
#define KAL_TERM_ECHO      ((kal_uintptr)1u << 1)  /* the environment shows what is typed */

/* Positions in the result of kal_terminal_props. */
#define KAL_TERM_PROP_MODE ((kal_uintptr)1u << 0)  /* get_mode/set_mode are answered */
#define KAL_TERM_PROP_SIZE ((kal_uintptr)1u << 1)  /* the display size is known      */

#ifdef __cplusplus
extern "C" {
#endif

/* Reports the mode currently in effect, and establishes a mode.
 *
 * A position this implementation does not distinguish is reported as zero by
 * the first and ignored by the second; neither is an error. An implementation
 * that distinguishes no position at all withholds the whole interface instead
 * (clause 6.1), so that a program discovers the absence at the link rather than
 * at a call that reports success having done nothing. */
int kal_terminal_get_mode(struct kal_stream s, kal_uintptr* mode);
int kal_terminal_set_mode(struct kal_stream s, kal_uintptr  mode);

/* The size of the display, in character cells.
 *
 * An environment that does not know --- a serial line has no way to ask ---
 * reports kal_err_not_supported and leaves both outputs untouched.
 *
 * THERE IS NO NOTIFICATION. openkal has no signals, so a program learns of a
 * change by asking again. A program with an event loop already has somewhere to
 * ask from; one without does not need to know. */
int kal_terminal_size(struct kal_stream s, kal_uintptr* cols, kal_uintptr* rows);

/* Properties of one terminal.
 *
 * An enquiry rather than a word, for the same reason `kal_stream_props' is one:
 * the answer varies between the resources of the interface and not between
 * implementations. The same implementation answers differently for a pseudo
 * terminal and for a serial line. */
kal_uintptr kal_terminal_props(struct kal_stream s);

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_TERMINAL_H */
