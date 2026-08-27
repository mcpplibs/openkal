// What this package composes, examined by using it.
//
// The observations are of behaviour. A test that named each entity and did
// nothing with it would compile against a package whose every operation did
// nothing, and would report that as working.
#include <cstdio>
#include <cstring>
import openkal.types;
import openkal.task;
import openkal.kit.endpoint;
import openkal.kit.channel;

namespace {

int failures = 0;
void check(bool held, const char* what) {
    if (!held) { std::printf("FAIL: %s\n", what); ++failures; }
}

void endpoint_section() {
    using namespace kal::kit;

    // The ordinary forms.
    {
        const auto r = parse_v4("127.0.0.1:8080", 14);
        check(r.ok, "an address with a port parses");
        check(r.ep.addr_len == 4, "the length says which kind of address it is");
        check(r.ep.addr[0] == 127 && r.ep.addr[1] == 0 &&
              r.ep.addr[2] == 0   && r.ep.addr[3] == 1,
              "the octets are the octets, in order");
        check(r.ep.port == 8080, "the port is a number in host order");
    }
    {
        const auto r = parse_v4("10.0.0.255", 10);
        check(r.ok && r.ep.port == 0, "an address without a port parses, with no port");
        check(r.ep.addr[3] == 255, "the largest octet is accepted");
    }

    // THE REJECTIONS ARE WHAT MAKES THIS WORTH HAVING. A parser that accepted
    // these would turn a mistyped configuration into a connection to somewhere
    // else, which is a defect that reports nothing at the time.
    struct { const char* text; const char* why; } bad[] = {
        { "1.2.3",            "three components are not an address" },
        { "1.2.3.4.5",        "five components are not an address" },
        { "300.1.1.1",        "an octet above 255 is refused" },
        { "1.2.3.4:70000",    "a port above 65535 is refused" },
        { "01.2.3.4",         "a leading zero is refused rather than guessed at" },
        { "1.2.3.4:",         "a colon with no port is refused" },
        { "1.2.3.4:80x",      "trailing text is refused" },
        { "1..3.4",           "an empty component is refused" },
        { "",                 "an empty string is refused" },
    };
    for (auto& b : bad) {
        const auto r = parse_v4(b.text, std::strlen(b.text));
        check(!r.ok, b.why);
    }

    // The inverse. Clause 7.11's reasoning applied to a conversion: a program
    // that reads an endpoint and later reports it should not write the
    // formatting itself.
    {
        char buf[32] = {};
        const auto ep = loopback_v4(443);
        const auto f = format_v4(ep, buf, sizeof buf);
        check(f.ok && f.n == 13 && std::memcmp(buf, "127.0.0.1:443", 13) == 0,
              "an endpoint formats back to what it was written as");

        // And round trips, which is the property the pair exists for.
        const auto again = parse_v4(buf, f.n);
        check(again.ok && again.ep.port == ep.port &&
              std::memcmp(again.ep.addr, ep.addr, 4) == 0,
              "formatting and parsing are inverses");
    }
    {
        char small[8] = {};
        const auto f = format_v4(loopback_v4(443), small, sizeof small);
        check(!f.ok, "a buffer too small is refused rather than half filled");
    }
    {
        const auto any = any_v4(80);
        check(any.addr[0] == 0 && any.addr[3] == 0 && any.port == 80,
              "the any-address is four zeroes and a port");
    }
}

// The channel, exercised across two contexts, which is the only way to observe
// that a reader sleeps rather than spins and that a writer wakes it.
kal::kit::channel_end g_writer{};
constexpr kal_uintptr kPayload = 100000;   // far larger than the ring

void producer(void*) {
    unsigned char block[997];              // deliberately not a divisor of the ring
    for (auto& b : block) b = 0xA5;
    kal_uintptr sent = 0;
    while (sent < kPayload) {
        kal_uintptr n = kPayload - sent;
        if (n > sizeof block) n = sizeof block;
        const kal_uintptr put = kal::kit::channel_write(g_writer, block, n);
        if (put == 0) break;
        sent += put;
    }
    kal::kit::channel_close(g_writer);
}

void channel_section() {
    using namespace kal::kit;

    auto c = channel_open();
    check(c.ok, "a channel is created");
    if (!c.ok) return;

    g_writer = c.writer;

    kal_task t{};
    const int rc = kal_task_start(&producer, nullptr, &t);
    check(rc == kal_ok, "a second context starts to write into it");
    if (rc != kal_ok) { channel_destroy(c); return; }

    // MORE THAN THE RING HOLDS, WHICH IS THE POINT. A payload that fitted would
    // never make the writer wait, and the sleeping and waking this module exists
    // for would go unobserved.
    unsigned char buf[1024];
    kal_uintptr got = 0;
    bool all_a5 = true;
    for (;;) {
        const kal_uintptr n = channel_read(c.reader, buf, sizeof buf);
        if (n == 0) break;                 // the writer closed
        for (kal_uintptr i = 0; i < n; ++i)
            if (buf[i] != 0xA5) all_a5 = false;
        got += n;
    }

    kal_task_join(t);
    check(got == kPayload, "every byte written is read");
    if (got != kPayload)
        std::printf("       read %llu of %llu\n",
                    (unsigned long long)got, (unsigned long long)kPayload);
    check(all_a5, "the bytes read are the bytes written");

    // A read after the close reports end of input rather than waiting.
    const kal_uintptr after = channel_read(c.reader, buf, sizeof buf);
    check(after == 0, "a read after the far end closed reports end of input");

    channel_close(c.reader);
    channel_destroy(c);
}

}  // namespace

int main() {
    endpoint_section();
    channel_section();
    if (failures == 0) std::printf("openkal-kit: every observation held\n");
    return failures == 0 ? 0 : 1;
}
