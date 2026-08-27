/* openkal.net --- a connection, which is a stream with a peer and a way to be
 * half-closed.
 *
 * NOT MERGED WITH `openkal.fs', and clause 3.4 records why: positioning applies
 * to a file and not to a connection, half-closure to a connection and not to a
 * file. Two interfaces whose operations do not both apply to both resources are
 * two interfaces.
 *
 * A connection IS a stream once established --- `kal_stream_read' and
 * `kal_stream_write' are the operations that move its bytes, and this interface
 * adds none of its own. What is here is what a connection has and a stream in
 * general does not: a peer, an accept, and an end that can be closed in one
 * direction.
 *
 * NAME RESOLUTION IS NOT HERE. Clause 3.4 excludes it in terms: an
 * implementation shall not be required to parse an unbounded set of name
 * schemes. An endpoint is an address and a port; turning "example.com" into one
 * is a library above this interface and `openkal.datagram'. */
#ifndef OPENKAL_NET_H
#define OPENKAL_NET_H
#include "types.h"
#include "stream.h"

/* An opaque handle occupying one machine word, per clause 7.2. A listener is
 * not a stream: nothing is transferred through it, and giving it the stream
 * operations would be an interface whose resource can never satisfy them. */
struct kal_net_listener { kal_uintptr h; };

/* Directions for kal_net_shutdown. */
#define KAL_SHUT_READ  1
#define KAL_SHUT_WRITE 2
#define KAL_SHUT_BOTH  3

/* Positions in kal_net_props. */
#define KAL_NET_PROP_IPV6      ((kal_uintptr)1u << 0)
#define KAL_NET_PROP_HALFCLOSE ((kal_uintptr)1u << 1)

#ifdef __cplusplus
extern "C" {
#endif

int kal_net_connect(const struct kal_endpoint* to,    struct kal_stream* out);
int kal_net_listen (const struct kal_endpoint* local, struct kal_net_listener* out);
int kal_net_accept (struct kal_net_listener l,        struct kal_stream* out);

/* Reports the endpoint of the peer, and the endpoint this end was given.
 *
 * The second is not the one that was asked for: a listener opened on port zero
 * is given a port by the environment, and a program that must publish where it
 * is listening has no other way to learn it. Clause 7.11 --- an enquiry whose
 * inverse exists --- is why both are here rather than only the first. */
int kal_net_peer  (struct kal_stream s, struct kal_endpoint* out);
int kal_net_local (struct kal_stream s, struct kal_endpoint* out);
int kal_net_listener_local(struct kal_net_listener l, struct kal_endpoint* out);

/* Ends transfer in one direction while the other continues.
 *
 * This is the operation that distinguishes a connection from a file, and it is
 * why the two are separate interfaces. An implementation that cannot express it
 * reports kal_err_not_supported and withholds KAL_NET_PROP_HALFCLOSE; a caller
 * that needs the peer to observe end-of-input must then close the whole
 * connection. */
int kal_net_shutdown(struct kal_stream s, int direction);

/* AN OWNED STREAM, UNLIKE THE THREE `openkal.stream' PROVIDES. A connection is
 * obtained and must be released; the standard streams are borrowed and are not.
 * This is the same division `openkal.fs' already draws with
 * `kal_fs_close_file', and the reason the release lives here rather than in
 * `openkal.stream': a stream in general has no owner to return it to. */
void kal_net_close         (struct kal_stream s);
void kal_net_close_listener(struct kal_net_listener l);

/* Properties of this implementation.
 *
 * A word rather than an enquiry, because these do not vary between the
 * resources of the interface: an implementation either speaks IPv6 or does not
 * (clause 6.2). */
extern const kal_uintptr kal_net_props;

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_NET_H */
