/* openkal.timeout --- a bound upon operations that would otherwise wait
 * without end.
 *
 * Every operation here is the operation of the same name in another interface,
 * with one argument added. Clause 7.8 already establishes that a second form of
 * one operation is admissible when the first cannot state the whole of an
 * intent: `kal_fs_open_file' and `kal_fs_open' stand beside each other for that
 * reason, and these stand beside their originals for the same one.
 *
 * THE ARGUMENT IS A DURATION, NOT AN INSTANT, and the name of this interface
 * says so. `kal_task_wait' already takes a `timeout_ns' and already defines zero
 * as no timeout; a second spelling of the same idea would be the one thing this
 * specification most consistently refuses. A caller that holds a deadline
 * subtracts the current time itself, which it can do with `openkal.time'.
 *
 * An expired bound is reported as `kal_err_again' --- "the operation would
 * block" --- which is what an expiry is. The error set is closed (clause 5.2)
 * and required no addition.
 *
 * AN IMPLEMENTATION MAY PROVIDE THIS FOR SOME OF ITS RESOURCES AND NOT OTHERS,
 * and reports `kal_err_not_supported' for the rest. That is not the defect
 * clause 6.4 describes: the operations are stated once per resource TYPE here,
 * so an implementation withholding the datagram form still answers the stream
 * form honestly. */
#ifndef OPENKAL_TIMEOUT_H
#define OPENKAL_TIMEOUT_H
#include "types.h"
#include "stream.h"
#include "net.h"
#include "datagram.h"
#include "process.h"

#ifdef __cplusplus
extern "C" {
#endif

struct kal_io_result kal_timeout_read (struct kal_stream s, void*       buf, kal_uintptr len, kal_u64 timeout_ns);
struct kal_io_result kal_timeout_write(struct kal_stream s, const void* buf, kal_uintptr len, kal_u64 timeout_ns);

int kal_timeout_accept(struct kal_net_listener l, kal_u64 timeout_ns, struct kal_stream* out);

struct kal_io_result kal_timeout_recv_from(struct kal_datagram d, void* buf, kal_uintptr len,
                                           struct kal_endpoint* from, kal_u64 timeout_ns);

int kal_timeout_wait_process(struct kal_process p, kal_u64 timeout_ns,
                             int* status, int* terminated);

/* The smallest bound this implementation distinguishes, in nanoseconds.
 *
 * A board whose only clock ticks at a millisecond reports 1000000. A caller
 * that asks for less is not refused and does not get less --- the bound is a
 * bound, and rounding it up is the only honest answer an environment with a
 * coarse clock can give.
 *
 * Per implementation rather than per resource, so a word rather than an enquiry
 * (clause 6.2). */
extern const kal_uintptr kal_timeout_granularity_ns;

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_TIMEOUT_H */
