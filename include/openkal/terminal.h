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
 * earlier implementation (clause 6.2). A POSITION IS THEREFORE SPELLED IN THE
 * SENSE IN WHICH ZERO IS THE WEAKER CLAIM: an implementation that has never
 * heard of a position says something true about itself by reading as zero, and
 * the stronger statement is one only an implementation that distinguishes the
 * position can make. */
#define KAL_TERM_LINE_EDIT    ((kal_uintptr)1u << 0)  /* the environment assembles lines       */
#define KAL_TERM_ECHO         ((kal_uintptr)1u << 1)  /* the environment shows what is typed   */
#define KAL_TERM_PASS_CONTROL ((kal_uintptr)1u << 2)  /* the environment reserves no keystroke */

/* KAL_TERM_PASS_CONTROL IS A GUARANTEE IN ONE DIRECTION AND A PERMISSION IN THE
 * OTHER. Set, every keystroke reaches the program as the bytes it produces,
 * including the ones an environment ordinarily keeps for itself. Clear, the
 * environment may reserve an agreed set of keystrokes for actions of its own.
 * Which keystrokes those are belongs to the environment: an interface that
 * required every environment to reserve the same ones would require of all of
 * them what one of them happens to spell, which is the shape clause 7.1
 * excludes.
 *
 * A PROGRAM THAT READS KEYSTROKES CANNOT BE WRITTEN WITHOUT IT. openkal has no
 * signals, so a program that has turned line assembly off and reads what is
 * typed still cannot survive the keystroke its environment reserves for
 * interruption --- the environment acts, and nothing above this interface can
 * decline the action. Clearing KAL_TERM_LINE_EDIT does not imply this position:
 * a program that performs line editing of its own turns line assembly off and
 * keeps the interruption, and that combination is what every environment this
 * specification has been implemented on spells natively.
 *
 * THE POSITION COVERS EVERY MECHANISM BY WHICH THE ENVIRONMENT KEEPS A
 * KEYSTROKE, and not the interruption alone. An environment that released the
 * interruption and kept the keystroke that stops output would leave the program
 * unable to say what it needs, and would leave one mode word meaning two things
 * on two environments.
 *
 * WHAT IT COSTS, RECORDED HERE SO THAT IT IS NOT DISCOVERED. One position
 * cannot record which of several mechanisms an environment had already
 * released. A terminal where some keystrokes were reserved and others were not
 * reads as clear, and is restored to the set the environment ordinarily
 * reserves. A position for each class of keystroke would record it, and would
 * require of an environment whose single switch governs them together a
 * distinction it does not have --- the defect clause 6.4 describes. */

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
 * at a call that reports success having done nothing.
 *
 * A POSITION WHOSE REQUESTED VALUE IS THE ONE IN EFFECT SHALL NOT BE WRITTEN.
 * Where a position stands for several of the environment's own mechanisms,
 * establishing it again would settle mechanisms the caller did not ask about:
 * a program turning the echo off would restore keystrokes its user had
 * released.
 *
 * AND A MODE IS NOT A WAY TO END THE INPUT. An implementation that turns line
 * assembly off shall not thereby cause a read of the stream to report zero
 * while input has not ended: zero denotes end of input (clause 7.4), and a
 * change of mode does not make it denote anything else. Where the environment
 * states this as a least number of bytes a read waits for, the implementation
 * establishes that number; the position says that the program reads
 * keystrokes, not that it is willing to be told there are none. A program that
 * wants a read which gives up asks for one: a bound upon waiting is stated by
 * `openkal.timeout' and not by a mode. */
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
