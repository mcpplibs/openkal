/* openkal.datagram --- messages with boundaries, sent without a connection.
 *
 * A SEPARATE INTERFACE FROM `openkal.net', for the reason clause 6.4 gives: a
 * datagram and a connection are two resources, and an operation that some
 * resources of an interface can never satisfy does not belong in it. A message
 * boundary is not a property a byte stream has; ordering is not a property a
 * datagram has. Merging them would produce exactly the interface this
 * specification's decomposition exists to avoid.
 *
 * AND IT IS THE EASIER HALF TO PROVIDE. A board that carries an IP stack
 * reaches datagrams in a few hundred lines and connections in a few thousand,
 * so an implementation supplying only this one is ordinary rather than
 * deficient --- clause 6.1 already expresses that by absence, and no capability
 * word is needed to say it. */
#ifndef OPENKAL_DATAGRAM_H
#define OPENKAL_DATAGRAM_H
#include "types.h"

/* An opaque handle occupying one machine word, per clause 7.2.
 *
 * NOT A STREAM. `kal_stream_read' reports a count and not a boundary, so a
 * datagram read through it would lose the one property that distinguishes this
 * interface. The handle is its own type so that the mistake cannot be made. */
struct kal_datagram { kal_uintptr h; };

/* Positions in kal_datagram_props. */
#define KAL_DGRAM_PROP_IPV6      ((kal_uintptr)1u << 0)
#define KAL_DGRAM_PROP_BROADCAST ((kal_uintptr)1u << 1)

#ifdef __cplusplus
extern "C" {
#endif

/* Opens a datagram endpoint.
 *
 * A local endpoint whose port is zero asks the environment to choose one; a
 * caller that must publish where it is reads it back with
 * `kal_datagram_local'. A null local endpoint asks for one that may send and
 * whose receiving address is unspecified. */
int kal_datagram_open(const struct kal_endpoint* local, struct kal_datagram* d);

/* Reports the endpoint this one was given, for the reason given above and
 * under clause 7.11. */
int kal_datagram_local(struct kal_datagram d, struct kal_endpoint* out);

/* Sends one message.
 *
 * A message is sent whole or not at all; a partial send is not a result this
 * interface produces. The count reported on success is therefore always the
 * length that was given, and is reported so that the result type is the one
 * every transferring operation in openkal uses. */
struct kal_io_result kal_datagram_send_to(struct kal_datagram d,
                                          const void* buf, kal_uintptr len,
                                          const struct kal_endpoint* to);

/* Reports one message and who sent it.
 *
 * A message longer than the buffer is truncated and the excess is lost, which
 * is what the medium does. The count reported is what was placed in the buffer;
 * a caller that must not lose bytes offers a buffer as large as the largest
 * message it will accept. */
struct kal_io_result kal_datagram_recv_from(struct kal_datagram d,
                                            void* buf, kal_uintptr len,
                                            struct kal_endpoint* from);

void kal_datagram_close(struct kal_datagram d);

extern const kal_uintptr kal_datagram_props;

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_DATAGRAM_H */
