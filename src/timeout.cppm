// openkal.timeout --- a bound upon operations that would otherwise wait
// without end.
//
// Every operation here is the operation of the same name in another interface,
// with one argument added. Clause 7.8 already establishes that a second form of
// one operation is admissible when the first cannot state the whole of an
// intent: kal_fs_open_file and kal_fs_open stand beside each other for that
// reason, and these stand beside their originals for the same one.
//
// The argument is a duration, not an instant, and the name of this interface
// says so. kal_task_wait already takes a timeout_ns and already defines zero as
// no timeout; a second spelling of the same idea would be the one thing this
// specification most consistently refuses.
//
// An expired bound is reported as kal_err_again --- "the operation would
// block" --- which is what an expiry is. The error set is closed (clause 5.2)
// and required no addition.
module;
#include <openkal/timeout.h>

export module openkal.timeout;
export import openkal.types;
export import openkal.stream;
export import openkal.net;
export import openkal.datagram;
export import openkal.process;

export using ::kal_timeout_read;
export using ::kal_timeout_write;
export using ::kal_timeout_accept;
export using ::kal_timeout_recv_from;
export using ::kal_timeout_wait_process;
export using ::kal_timeout_granularity;

export namespace kal::timeout {

// The smallest bound this implementation distinguishes. A caller that asks for
// less is not refused and does not get less.
inline kal_u64 granularity_ns() { return kal_timeout_granularity(); }

inline kal_intptr read (kal_stream s, void* p, kal_uintptr n, kal_u64 ns) {
    return kal_timeout_read(s, p, n, ns);
}
inline kal_intptr write(kal_stream s, const void* p, kal_uintptr n, kal_u64 ns) {
    return kal_timeout_write(s, p, n, ns);
}

struct conn_result { kal_net_conn c; int e; };

inline conn_result accept(kal_net_listener l, kal_u64 ns) {
    kal_net_conn c{};
    const int e = kal_timeout_accept(l, ns, &c);
    return { c, e };
}

// Shaped like kal::datagram::recv_from, because it is that operation with a
// bound: a caller that adds a timeout should not also have to change how it
// reads the result.
struct recv_result { kal_intptr n; kal_endpoint from; };

inline recv_result recv_from(kal_datagram d, void* p, kal_uintptr n, kal_u64 ns) {
    kal_endpoint from{};
    const kal_intptr r = kal_timeout_recv_from(d, p, n, &from, ns);
    return { r, from };
}

struct wait_result { int status; int terminated; int e; };

inline wait_result wait(kal_process p, kal_u64 ns) {
    int status = 0, terminated = 0;
    const int e = kal_timeout_wait_process(p, ns, &status, &terminated);
    return { status, terminated, e };
}

}
