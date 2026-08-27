// openkal.datagram --- messages with boundaries, sent without a connection.
//
// A separate interface from openkal.net, for the reason clause 6.4 gives: a
// datagram and a connection are two resources, and an operation that some
// resources of an interface can never satisfy does not belong in it. A message
// boundary is not a property a byte stream has; ordering is not a property a
// datagram has.
//
// And it is the easier half to provide. A board that carries an IP stack
// reaches datagrams in a few hundred lines and connections in a few thousand,
// so an implementation supplying only this one is ordinary rather than
// deficient --- clause 6.1 expresses that by absence, and no capability word is
// needed to say it.
module;
#include <openkal/datagram.h>

export module openkal.datagram;
export import openkal.types;

export using ::kal_datagram;

export using ::kal_datagram_open;
export using ::kal_datagram_local;
export using ::kal_datagram_send_to;
export using ::kal_datagram_recv_from;
export using ::kal_datagram_close;
export using ::kal_datagram_props;

static_assert(sizeof(kal_datagram) == sizeof(kal_uintptr), "clause 7.2");

export namespace kal::datagram {

using datagram = kal_datagram;
using endpoint = kal_endpoint;

struct props_tag;
using props = kal::props<props_tag>;

inline constexpr props ipv6     {KAL_DGRAM_PROP_IPV6};
inline constexpr props broadcast{KAL_DGRAM_PROP_BROADCAST};

inline props properties() { return props{kal_datagram_props}; }
inline bool  has(props p) { return properties().has(p); }

struct open_result     { datagram d; int e; };
struct endpoint_result { endpoint ep; int e; };

// A null local endpoint asks for one that may send and whose receiving address
// is unspecified. Two overloads rather than a defaulted pointer, so that the
// two intents are two calls at the call site.
inline open_result open(const endpoint& local) {
    datagram d{};
    const int e = kal_datagram_open(&local, &d);
    return { d, e };
}
inline open_result open() {
    datagram d{};
    const int e = kal_datagram_open(nullptr, &d);
    return { d, e };
}

inline endpoint_result local(datagram d) {
    endpoint ep{};
    const int e = kal_datagram_local(d, &ep);
    return { ep, e };
}

inline kal_io_result send_to(datagram d, const void* p, kal_uintptr n, const endpoint& to) {
    return kal_datagram_send_to(d, p, n, &to);
}

// The sender is reported beside the result rather than through an
// out-parameter: every caller of a receive wants both, and none of them wants
// to declare an endpoint first.
struct recv_result { kal_io_result r; endpoint from; };

inline recv_result recv_from(datagram d, void* p, kal_uintptr n) {
    endpoint from{};
    const kal_io_result r = kal_datagram_recv_from(d, p, n, &from);
    return { r, from };
}

inline void close(datagram d) { kal_datagram_close(d); }

}
