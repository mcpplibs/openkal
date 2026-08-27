// openkal.kit.endpoint --- writing an endpoint down, and reading one back.
//
// Clause 3.4 excludes name resolution from the specification in terms: an
// implementation shall not be required to parse an unbounded set of name
// schemes. That exclusion is about what an IMPLEMENTATION must provide, and it
// leaves every caller of openkal.net with the same small problem --- a
// configuration file says "127.0.0.1:8080" and kal_net_connect takes bytes and a
// number.
//
// Solving it once here is what this package is for. Nothing below reaches the
// network or the environment: this is arithmetic over a string, so it holds on a
// machine with firmware and nothing else, and it is not the resolution clause
// 3.4 declines. Turning a NAME into an address needs a resolver, which needs the
// network, and belongs above this module rather than in it.
export module openkal.kit.endpoint;

import openkal.types;

export namespace kal::kit {

// What a parse produced, and whether it produced anything.
//
// A result type rather than an out-parameter and a code, because a caller that
// forgets to test an out-parameter reads a zeroed endpoint as the address
// 0.0.0.0 --- a legitimate address, and therefore a failure that looks like
// success. Here the endpoint is not reachable without the flag being visible.
struct endpoint_result {
    kal_endpoint ep;
    bool         ok;
};

// Reads "A.B.C.D:port" or "A.B.C.D".
//
// THE ACCEPTED FORM IS EXACT AND THE REJECTIONS ARE NOT SILENT. A parser that
// accepted "1.2.3" as 1.2.0.3, or "300.1.1.1" as 44.1.1.1, would turn a
// mistyped configuration into a connection to somewhere else. Each component is
// required, each is bounded, and a component with a leading zero is refused
// because two conventions read "010" differently and neither of them is
// obviously the one the writer meant.
constexpr endpoint_result parse_v4(const char* text, kal_uintptr len) {
    kal_endpoint ep{};
    ep.addr_len = 4;

    if (text == nullptr || len == 0) return { ep, false };

    kal_uintptr i = 0;
    for (int octet = 0; octet < 4; ++octet) {
        if (octet > 0) {
            if (i >= len || text[i] != '.') return { ep, false };
            ++i;
        }
        const kal_uintptr start = i;
        unsigned value = 0;
        while (i < len && text[i] >= '0' && text[i] <= '9') {
            value = value * 10u + static_cast<unsigned>(text[i] - '0');
            if (value > 255u) return { ep, false };
            ++i;
            if (i - start > 3) return { ep, false };
        }
        if (i == start) return { ep, false };                    // no digits
        if (i - start > 1 && text[start] == '0') return { ep, false };  // leading zero
        ep.addr[octet] = static_cast<kal_u8>(value);
    }

    if (i == len) { ep.port = 0; return { ep, true }; }          // address alone

    if (text[i] != ':') return { ep, false };
    ++i;
    const kal_uintptr pstart = i;
    unsigned port = 0;
    while (i < len && text[i] >= '0' && text[i] <= '9') {
        port = port * 10u + static_cast<unsigned>(text[i] - '0');
        if (port > 65535u) return { ep, false };
        ++i;
    }
    if (i == pstart) return { ep, false };                       // a colon and no port
    if (i != len) return { ep, false };                          // trailing rubbish
    if (i - pstart > 1 && text[pstart] == '0') return { ep, false };

    ep.port = static_cast<kal_u32>(port);
    return { ep, true };
}

// The inverse, which is why it is here rather than in a caller.
//
// Clause 7.11 states that an enquiry has an inverse, and the same reasoning
// applies to a conversion: a program that reads an endpoint from a
// configuration file and later reports which endpoint it used should not have
// to write the formatting itself, and two hand-written formatters in one
// program will eventually disagree.
//
// Writes at most 22 characters and reports how many. Nothing is allocated,
// because a facility usable on a machine with firmware and nothing else cannot
// assume an allocator is present even though openkal.memory is core.
struct format_result {
    kal_uintptr n;
    bool        ok;
};

constexpr format_result format_v4(const kal_endpoint& ep, char* out, kal_uintptr cap) {
    if (out == nullptr || ep.addr_len != 4) return { 0, false };

    // "255.255.255.255:65535" is twenty-one characters.
    if (cap < 22) return { 0, false };

    kal_uintptr n = 0;
    auto digits = [&](unsigned v) {
        if (v >= 100) out[n++] = static_cast<char>('0' + (v / 100) % 10);
        if (v >= 10)  out[n++] = static_cast<char>('0' + (v / 10) % 10);
        out[n++] = static_cast<char>('0' + v % 10);
    };

    for (int octet = 0; octet < 4; ++octet) {
        if (octet > 0) out[n++] = '.';
        digits(static_cast<unsigned>(ep.addr[octet]));
    }
    if (ep.port != 0) {
        out[n++] = ':';
        digits(static_cast<unsigned>(ep.port));
    }
    return { n, true };
}

// The loopback address, which every program that opens a listener for its own
// use writes out by hand otherwise.
constexpr kal_endpoint loopback_v4(kal_u32 port) {
    kal_endpoint ep{};
    ep.addr[0] = 127; ep.addr[3] = 1;
    ep.addr_len = 4;
    ep.port = port;
    return ep;
}

// An endpoint that accepts on every address of the machine. Written out for the
// same reason: a program that means "listen everywhere" should say so rather
// than encode four zeroes whose meaning a reader must recall.
constexpr kal_endpoint any_v4(kal_u32 port) {
    kal_endpoint ep{};
    ep.addr_len = 4;
    ep.port = port;
    return ep;
}

}  // namespace kal::kit
