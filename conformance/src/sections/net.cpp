module okc.net;

import openkal.types;
import openkal.stream;
import openkal.net;
import openkal.task;
import openkal.time;
import openkal.fs;     // a rendezvous at a name is in a directory (0.17)
import okc.report;
import okc.spec;

namespace okc::net {

#ifdef MCPP_FEATURE_NET
namespace {

// The loopback address, written byte by byte because that is what an endpoint
// carries. Constructing it here rather than parsing a string is the whole point
// of clause 3.4: this suite resolves no name.
kal_endpoint loopback_v4(kal_u32 port) {
    kal_endpoint ep{};
    ep.addr[0] = 127; ep.addr[1] = 0; ep.addr[2] = 0; ep.addr[3] = 1;
    ep.addr_len = 4;
    ep.port = port;
    return ep;
}

#if defined(MCPP_FEATURE_TASK) && defined(MCPP_FEATURE_TIME)
// The two contexts of the observation that a connection's directions are
// independent. One reads from the client end and waits; the other, after a
// bound, writes from the server end so that the reader is released whatever
// the implementation did. The bound is what turns a serialised implementation
// into an observation that does not hold rather than a suite that never ends.
kal_stream g_reader_stream{};
kal_stream g_releaser_stream{};
constexpr kal_u64 kReleaseAfterNs = 2000ull * 1000ull * 1000ull;

void reads_and_waits(void*) {
    char b[8];
    kal_stream_read(g_reader_stream, b, sizeof b);
}

void releases_the_reader(void*) {
    kal_time_sleep(kReleaseAfterNs);
    const char x = 'r';
    kal_stream_write(g_releaser_stream, &x, 1);
}
#endif

}  // namespace
#endif

void run() {
    heading("openkal.net");
#ifndef MCPP_FEATURE_NET
    unobserved(kind::behaviour, "openkal.net", "the interface was not selected");
    return;
#else
    claim("kal_net_props()", kal_net_props());

    // A LISTENER ON PORT ZERO, AND THE PORT READ BACK. The environment chooses
    // the port, so a suite that named one would fail on a machine where that
    // port was in use --- and would be examining the machine rather than the
    // implementation. Reading it back is also the observation that
    // kal_net_listener_local answers, which is the inverse clause 7.11 requires.
    kal_endpoint want = loopback_v4(0);
    kal_net_listener listener{};
    const int lrc = kal_net_listen(&want, &listener);
    if (lrc != kal_ok) {
        unobserved(kind::behaviour, "openkal.net",
                   "a loopback listener could not be opened in this environment");
        return;
    }
    observe(kind::behaviour, true, "a listener opens on the loopback address");

    kal_endpoint bound{};
    const int brc = kal_net_listener_local(listener, &bound);
    observe(kind::behaviour, brc == kal_ok && bound.port != 0,
            "a listener opened on port zero reports the port it was given");
    observe(kind::behaviour, brc == kal_ok && bound.addr_len == 4,
            "the reported endpoint carries the length of the address it holds");

    if (brc != kal_ok || bound.port == 0) {
        kal_net_close_listener(listener);
        return;
    }

    // A connection to that listener, accepted, and bytes carried across it. The
    // transfer uses kal_stream_read and kal_stream_write and not an operation of
    // this interface, which is the property that makes a connection a stream.
    kal_endpoint to = loopback_v4(bound.port);
    kal_net_conn client{};
    const int crc = kal_net_connect(&to, &client);
    observe(kind::behaviour, crc == kal_ok, "a connection to the listener is established");
    if (crc != kal_ok) { kal_net_close_listener(listener); return; }

    kal_net_conn server{};
    const int arc = kal_net_accept(listener, &server);
    observe(kind::behaviour, arc == kal_ok, "the listener accepts the connection");
    if (arc != kal_ok) { kal_net_close(client); kal_net_close_listener(listener); return; }

    // The streams the connections own. Borrowed and released with the
    // connection, which is the arrangement openkal.fs already uses.
    const kal_stream cs{kal_net_stream(client)};
    const kal_stream ss{kal_net_stream(server)};

    {
        const char msg[] = "openkal";
        const kal_intptr w = kal_stream_write(cs, msg, sizeof msg - 1);
        observe(kind::behaviour, w == static_cast<kal_intptr>(sizeof msg - 1),
                "a connection carries bytes through the stream operations");

        char buf[16] = {0};
        const kal_intptr r = kal_stream_read(ss, buf, sizeof buf);
        bool same = r == static_cast<kal_intptr>(sizeof msg - 1);
        for (kal_intptr i = 0; same && i < r; ++i)
            if (buf[i] != msg[i]) same = false;
        observe(kind::behaviour, same, "the bytes read are the bytes written");
    }

    // A CONNECTION HAS TWO INDEPENDENT DIRECTIONS. Version 0.13, clause 6.6.
    //
    // A context waits in a read on the client end. Another context writes on
    // the same end, and that write shall complete without waiting for the read.
    // A third context releases the reader after two seconds, so an
    // implementation that serialises the directions makes the write take that
    // long, which is observed, instead of waiting for ever.
#if defined(MCPP_FEATURE_TASK) && defined(MCPP_FEATURE_TIME)
    {
        g_reader_stream = cs;
        g_releaser_stream = ss;
        kal_task reader{}, releaser{};
        const int re = kal_task_start(reads_and_waits, nullptr, &reader);
        const int le = re == kal_ok
            ? kal_task_start(releases_the_reader, nullptr, &releaser) : re;
        if (re == kal_ok && le == kal_ok) {
            kal_time_sleep(200ull * 1000ull * 1000ull);
            const kal_duration t0 = kal_time_monotonic();
            const char y = 'w';
            const kal_intptr w = kal_stream_write(cs, &y, 1);
            const kal_duration elapsed = kal_time_monotonic() - t0;
            observe(kind::behaviour,
                    w == 1 && elapsed < kReleaseAfterNs / 2,
                    "a write is not delayed by a read waiting on the same connection");
            char b[4];
            kal_stream_read(ss, b, 1);
            kal_task_join(releaser);
            kal_task_join(reader);
        } else {
            if (re == kal_ok) {
                const char x = 'r';
                kal_stream_write(ss, &x, 1);
                kal_task_join(reader);
            }
            unobserved(kind::behaviour,
                       "a write is not delayed by a read waiting on the same connection",
                       "an execution context could not be started");
        }
    }
#else
    unobserved(kind::behaviour,
               "a write is not delayed by a read waiting on the same connection",
               "the observation needs openkal.task and openkal.time");
#endif

    // The peer of each end is the other end. Observed on the accepted side,
    // whose peer is the client's local address; the two are the same machine
    // here, so what is asserted is that the lengths agree and the call answers.
    {
        kal_endpoint peer{};
        const int rc = kal_net_peer(server, &peer);
        observe(kind::behaviour, rc == kal_ok && peer.addr_len == 4,
                "an accepted connection reports its peer");
    }

    // HALF-CLOSURE IS WHAT DISTINGUISHES A CONNECTION FROM A FILE, and it is
    // observed only where the implementation claims it. An implementation that
    // cannot express it refuses and withholds the position, and both of those
    // are conforming; what would not be conforming is claiming the position and
    // then refusing.
    {
        const bool claims = kal::net::has(kal::net::halfclose);
        const int rc = kal::net::shutdown(client, kal::net::shut::write);
        if (claims) {
            observe(kind::behaviour, rc == kal_ok,
                    "a claimed half-closure is performed when asked for");
            char buf[4] = {0};
            const kal_intptr r = kal_stream_read(ss, buf, sizeof buf);
            observe(kind::behaviour, r == 0,
                    "the peer observes end of input after a half-closure");
        } else {
            observe(kind::behaviour, rc == kal_err_not_supported,
                    "an unclaimed half-closure is refused rather than ignored");
        }
    }

    kal_net_close(server);
    kal_net_close(client);
    kal_net_close_listener(listener);

    // An endpoint whose length this implementation does not know is refused
    // rather than read as one it does. This is the evolution rule of the
    // endpoint type, and an implementation that ignored the length would
    // misread every address a later revision defines.
    {
        kal_endpoint odd{};
        odd.addr_len = 7;     // not 4, not 16, not 20
        odd.port = 9;
        kal_net_conn c{};
        const int rc = kal_net_connect(&odd, &c);
        observe(kind::behaviour, rc == kal_err_invalid,
                "an endpoint of unknown length is refused, not misread");
        if (rc == kal_ok) kal_net_close(c);
    }

    // A RENDEZVOUS AT A NAME (version 0.17), where the working directory's
    // volume claims it. The listener does not remove its name, so the name is
    // removed before and after.
#ifdef MCPP_FEATURE_FS
    {
        const kal_dir here = kal::fs::working();
        const char name[] = "okc-rendezvous";
        const kal_uintptr len = sizeof name - 1;
        if ((kal_fs_props(here) & KAL_FS_PROP_LOCAL_ENDPOINT) != 0) {
            kal_fs_remove(here, name, len);
            kal_net_listener l{};
            const int lr = kal_net_listen_at(here, name, len, &l);
            observe(kind::behaviour, lr == kal_ok, "a listener is opened at a name");
            if (lr == kal_ok) {
                kal_net_listener second{};
                observe(kind::behaviour, kal_net_listen_at(here, name, len, &second) == kal_err_exists,
                        "a second listener at the same name is refused with kal_err_exists");
                kal_net_conn c{}, s{};
                const int cr = kal_net_connect_at(here, name, len, &c);
                observe(kind::behaviour, cr == kal_ok, "a connection is made at the name");
                if (cr == kal_ok && kal_net_accept(l, &s) == kal_ok) {
                    const kal_stream cs = kal_net_stream(c), sst = kal_net_stream(s);
                    char b[4] = {};
                    const bool moved = kal_stream_write(cs, "okc", 3) == 3
                                       && kal_stream_read(sst, b, 3) == 3 && b[0] == 'o' && b[2] == 'c';
                    observe(kind::behaviour, moved, "bytes cross a connection made at a name");
                    kal_endpoint ep{};
                    observe(kind::behaviour, kal_net_peer(s, &ep) == kal_err_not_supported,
                            "a connection made at a name has no address to report");
                    kal_net_close(s);
                }
                if (cr == kal_ok) kal_net_close(c);
                kal_net_close_listener(l);
            }
            kal_fs_remove(here, name, len);
            kal_net_conn none{};
            observe(kind::behaviour, kal_net_connect_at(here, name, len, &none) == kal_err_not_found,
                    "a connection at a name nothing listens at is kal_err_not_found");
        } else {
            unobserved(kind::behaviour, "a rendezvous at a name",
                       "the working directory does not claim KAL_FS_PROP_LOCAL_ENDPOINT");
        }
    }
#endif
#endif
}

}
