/* openkal.stream --- byte streams. Core. */
#ifndef OPENKAL_STREAM_H
#define OPENKAL_STREAM_H
#include "types.h"

/* An opaque handle occupying one machine word. The width is fixed and the
 * interpretation is not, which is what allows an implementation to be placed
 * above a C library, beneath one, or without one. */
struct kal_stream { kal_uintptr h; };

#ifdef __cplusplus
extern "C" {
#endif

/* The program's standard streams. These handles are borrowed. */
struct kal_stream kal_stdin (void);
struct kal_stream kal_stdout(void);
struct kal_stream kal_stderr(void);

/* Transfers the whole buffer, or reports the condition that prevented it. On
 * failure, n reports how many bytes were transferred before the failure. */
struct kal_io_result kal_stream_write(struct kal_stream s, const void* buf, kal_uintptr len);

/* Transfers at most len bytes and reports how many were transferred. Zero
 * bytes with kal_ok denotes end of input. */
struct kal_io_result kal_stream_read(struct kal_stream s, void* buf, kal_uintptr len);

/* Commits any buffering the implementation performs. */
int kal_stream_flush(struct kal_stream s);

/* Properties of one stream.
 *
 * An enquiry rather than a word, because the property varies between the
 * resources of the interface and not between implementations: the same
 * implementation answers differently for a terminal and for a file. It is not
 * an operation upon the stream and therefore is not the defect clause 6.4
 * describes; nothing is transferred and no resource can fail to answer.
 *
 * A C library requires the answer before it has transferred anything: it must
 * choose a buffering discipline, and one that defers a prompt until a buffer
 * fills leaves an interactive program appearing not to respond. A library that
 * had to assume would be silently wrong for one of the two cases. */
kal_uintptr kal_stream_props(struct kal_stream s);

/* Positions in the result of kal_stream_props. */
#define KAL_STREAM_PROP_INTERACTIVE ((kal_uintptr)1u << 0)

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_STREAM_H */
