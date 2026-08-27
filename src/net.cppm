// openkal.net --- a connection, which is a stream with a peer and a way to be
// half-closed.
//
// Not merged with openkal.fs, and clause 3.4 records why: positioning applies
// to a file and not to a connection, half-closure to a connection and not to a
// file. A connection IS a stream once established, so this interface adds no
// transfer operation of its own --- kal_stream_read and kal_stream_write move
// its bytes. What is here is what a connection has and a stream in general
// does not.
//
// Name resolution is not here. Clause 3.4 excludes it in terms: an
// implementation shall not be required to parse an unbounded set of name
// schemes.
module;
#include <openkal/net.h>

export module openkal.net;
export import openkal.types;
export import openkal.stream;

export using ::kal_net_listener;

export using ::kal_net_connect;
export using ::kal_net_listen;
export using ::kal_net_accept;
export using ::kal_net_peer;
export using ::kal_net_local;
export using ::kal_net_listener_local;
export using ::kal_net_shutdown;
export using ::kal_net_close;
export using ::kal_net_close_listener;
export using ::kal_net_props;

static_assert(sizeof(kal_net_listener) == sizeof(kal_uintptr), "clause 7.2");

export namespace kal::net {

using listener = kal_net_listener;
using endpoint = kal_endpoint;

struct props_tag;
using props = kal::props<props_tag>;

inline constexpr props ipv6     {KAL_NET_PROP_IPV6};
inline constexpr props halfclose{KAL_NET_PROP_HALFCLOSE};

inline props properties() { return props{kal_net_props}; }
inline bool  has(props p) { return properties().has(p); }

// The directions kal_net_shutdown accepts. An enumeration rather than the bare
// integers, so that a caller cannot pass a stream flag by mistake.
enum class shut { read = KAL_SHUT_READ, write = KAL_SHUT_WRITE, both = KAL_SHUT_BOTH };

inline int shutdown(kal_stream s, shut d) {
    return kal_net_shutdown(s, static_cast<int>(d));
}

// A handle and the outcome together, so that the ordinary use --- open it, test
// it, use it --- is one statement. The handle is meaningful only when e is
// kal_ok; nothing here enforces that, for the same reason the C form does not.
struct stream_result   { kal_stream   s; int e; };
struct listener_result { listener     l; int e; };
struct endpoint_result { endpoint     ep; int e; };

inline stream_result connect(const endpoint& to) {
    kal_stream s{};
    const int e = kal_net_connect(&to, &s);
    return { s, e };
}

inline listener_result listen(const endpoint& local) {
    listener l{};
    const int e = kal_net_listen(&local, &l);
    return { l, e };
}

inline stream_result accept(listener l) {
    kal_stream s{};
    const int e = kal_net_accept(l, &s);
    return { s, e };
}

inline endpoint_result peer (kal_stream s) { endpoint ep{}; const int e = kal_net_peer (s, &ep); return { ep, e }; }
inline endpoint_result local(kal_stream s) { endpoint ep{}; const int e = kal_net_local(s, &ep); return { ep, e }; }
inline endpoint_result local(listener   l) { endpoint ep{}; const int e = kal_net_listener_local(l, &ep); return { ep, e }; }

inline void close(kal_stream s) { kal_net_close(s); }
inline void close(listener   l) { kal_net_close_listener(l); }

}
