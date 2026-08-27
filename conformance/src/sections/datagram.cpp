module okc.datagram;

import openkal.types;
import openkal.datagram;
import okc.report;
import okc.spec;

namespace okc::datagram {

#ifdef MCPP_FEATURE_DATAGRAM
namespace {

kal_endpoint loopback_v4(kal_u32 port) {
    kal_endpoint ep{};
    ep.addr[0] = 127; ep.addr[1] = 0; ep.addr[2] = 0; ep.addr[3] = 1;
    ep.addr_len = 4;
    ep.port = port;
    return ep;
}

}  // namespace
#endif

void run() {
    heading("openkal.datagram");
#ifndef MCPP_FEATURE_DATAGRAM
    unobserved(kind::behaviour, "openkal.datagram", "the interface was not selected");
    return;
#else
    claim("kal_datagram_props", kal_datagram_props);

    kal_endpoint want = loopback_v4(0);
    kal_datagram receiver{};
    const int orc = kal_datagram_open(&want, &receiver);
    if (orc != kal_ok) {
        unobserved(kind::behaviour, "openkal.datagram",
                   "a loopback endpoint could not be opened in this environment");
        return;
    }
    observe(kind::behaviour, true, "an endpoint opens on the loopback address");

    kal_endpoint bound{};
    const int brc = kal_datagram_local(receiver, &bound);
    observe(kind::behaviour, brc == kal_ok && bound.port != 0,
            "an endpoint opened on port zero reports the port it was given");
    if (brc != kal_ok || bound.port == 0) { kal_datagram_close(receiver); return; }

    kal_datagram sender{};
    const int src = kal_datagram_open(nullptr, &sender);
    observe(kind::behaviour, src == kal_ok,
            "an endpoint that only sends is opened without a local address");
    if (src != kal_ok) { kal_datagram_close(receiver); return; }

    kal_endpoint to = loopback_v4(bound.port);

    // A MESSAGE IS SENT WHOLE OR NOT AT ALL, which is the property that
    // distinguishes this interface from a stream. The count reported is
    // therefore always the length that was given.
    {
        const char msg[] = "openkal";
        const kal_io_result w = kal_datagram_send_to(sender, msg, sizeof msg - 1, &to);
        observe(kind::behaviour, w.e == kal_ok && w.n == sizeof msg - 1,
                "a message is sent whole and the count is the length given");

        kal_endpoint from{};
        char buf[16] = {0};
        const kal_io_result r = kal_datagram_recv_from(receiver, buf, sizeof buf, &from);
        bool same = r.e == kal_ok && r.n == sizeof msg - 1;
        for (kal_uintptr i = 0; same && i < r.n; ++i)
            if (buf[i] != msg[i]) same = false;
        observe(kind::behaviour, same, "the message received is the message sent");
        observe(kind::behaviour, r.e != kal_ok || from.addr_len == 4,
                "the sender of a received message is reported");
    }

    // A MESSAGE LONGER THAN THE BUFFER IS TRUNCATED AND THE EXCESS IS LOST,
    // which is what the medium does. What is observed is that the operation
    // reports what it placed in the buffer rather than what was sent, because a
    // caller that trusted the larger number would read beyond its own buffer.
    {
        char big[64];
        for (auto& c : big) c = 'x';
        const kal_io_result w = kal_datagram_send_to(sender, big, sizeof big, &to);
        if (w.e == kal_ok) {
            char small[8] = {0};
            kal_endpoint from{};
            const kal_io_result r = kal_datagram_recv_from(receiver, small, sizeof small, &from);
            observe(kind::behaviour, r.e == kal_ok && r.n <= sizeof small,
                    "a truncated message reports the count placed in the buffer");
        } else {
            unobserved(kind::behaviour, "truncation reports the buffered count",
                       "the larger message could not be sent");
        }
    }

    kal_datagram_close(sender);
    kal_datagram_close(receiver);

    // An endpoint of a length this implementation does not know is refused
    // rather than read as one it does, for the same reason as in openkal.net.
    {
        kal_endpoint odd{};
        odd.addr_len = 7;
        odd.port = 9;
        kal_datagram d{};
        const int rc = kal_datagram_open(&odd, &d);
        observe(kind::behaviour, rc == kal_err_invalid,
                "an endpoint of unknown length is refused, not misread");
        if (rc == kal_ok) kal_datagram_close(d);
    }
#endif
}

}
