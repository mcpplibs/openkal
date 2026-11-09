module okc.protocol;

import openkal.types;
import openkal.env;
import openkal.fs;
import openkal.stream;
import openkal.time;
import okc.spec;

namespace okc::protocol {
namespace {

kal_uintptr length(const char* s) { kal_uintptr n = 0; while (s && s[n]) ++n; return n; }

bool same(const char* a, kal_uintptr alen, const char* b, kal_uintptr blen) {
    if (alen != blen) return false;
    for (kal_uintptr i = 0; i < alen; ++i) if (a[i] != b[i]) return false;
    return true;
}

// THE MARKER (protocol P2.3): the bytes a runner finds in the program's file,
// followed by a byte that is not a digit. Read once below, so that no link
// that keeps only what it reaches removes it.
extern "C" const char okc_mtest_marker[] = "mtest-protocol=1;";

bool g_active = false;
bool g_listing = false;
kal_stream g_report{};
kal_u64 g_start = 0;

// The selection: identifiers, one per line, and whether each named a case.
char g_select[262144];
kal_uintptr g_select_len = 0;
bool g_selecting = false;
constexpr int kMaxSelected = 4096;
bool g_matched[kMaxSelected];

// The case running, and the cases within it whose identifiers were given, so
// that two observations of one text are two identifiers (P3.2).
char g_case[64];
bool g_case_open = false;
kal_u64 g_case_began = 0;
bool g_case_failed = false;
int g_case_parts = 0;
constexpr int kMaxParts = 1024;
kal_u64 g_part_hash[kMaxParts];
int g_part_seen[kMaxParts];

// ---- the bytes of one event --------------------------------------------------
struct line {
    char buf[4096];
    kal_uintptr n = 0;
    bool overflow = false;
    void raw(const char* s, kal_uintptr len) {
        for (kal_uintptr i = 0; i < len; ++i) {
            if (n + 1 >= sizeof buf) { overflow = true; return; }
            buf[n++] = s[i];
        }
    }
    void raw(const char* s) { raw(s, length(s)); }
    void str(const char* s, kal_uintptr len) {
        raw("\"");
        for (kal_uintptr i = 0; i < len; ++i) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            if (c == '"')       raw("\\\"");
            else if (c == '\\') raw("\\\\");
            else if (c == '\n') raw("\\n");
            else if (c == '\r') raw("\\r");
            else if (c == '\t') raw("\\t");
            else if (c < 0x20) {
                const char hex[] = "0123456789abcdef";
                const char e[6] = { '\\', 'u', '0', '0', hex[c >> 4], hex[c & 15] };
                raw(e, 6);
            } else {
                const char b = static_cast<char>(c);
                raw(&b, 1);
            }
        }
        raw("\"");
    }
    void str(const char* s) { str(s, length(s)); }
    void num(kal_u64 v) {
        char d[24]; int i = 24;
        if (v == 0) d[--i] = '0';
        while (v) { d[--i] = static_cast<char>('0' + v % 10); v /= 10; }
        raw(d + i, static_cast<kal_uintptr>(24 - i));
    }
    void send() {
        if (!g_active || overflow) return;
        raw("\n");
        kal_stream_write(g_report, buf, n);   // one write per event (P5.1)
    }
};

kal_u64 elapsed_ms() {
#ifdef MCPP_FEATURE_TIME
    return (kal_time_monotonic() - g_start) / 1000000u;
#else
    return 0;
#endif
}

kal_u64 hash(const char* s) {
    kal_u64 h = 1469598103934665603ull;
    for (kal_uintptr i = 0; s[i]; ++i) { h ^= static_cast<unsigned char>(s[i]); h *= 1099511628211ull; }
    return h;
}

// The case identifier a heading denotes: its first word, and the heading of
// the implementation's self-description is `openkal.version'.
void id_of(const char* heading_text, char* out, kal_uintptr cap) {
    kal_uintptr n = 0;
    while (heading_text[n] && heading_text[n] != ' ' && n + 1 < cap) { out[n] = heading_text[n]; ++n; }
    out[n] = 0;
    if (same(out, n, "openkal", 7)) {
        const char v[] = "openkal.version";
        for (kal_uintptr i = 0; i < sizeof v && i < cap; ++i) out[i] = v[i];
    }
}

void begin_event(const char* id) {
    line e; e.raw("{\"e\":\"begin\",\"id\":"); e.str(id); e.raw(",\"t\":"); e.num(elapsed_ms());
    e.raw(",\"kind\":\"cts\",\"clauses\":["); e.str(id); e.raw("],\"resources\":[\"serial:openkal-conformance\"]}"); e.send();
}

void end_event(const char* id, const char* status, const char* reason, kal_u64 ms = 0) {
    line e; e.raw("{\"e\":\"end\",\"id\":"); e.str(id); e.raw(",\"status\":"); e.str(status);
    e.raw(",\"ms\":"); e.num(ms);
    if (reason) { e.raw(",\"reason\":"); e.str(reason); }
    e.raw("}"); e.send();
}

void close_case() {
    if (!g_case_open) return;
    end_event(g_case, g_case_failed ? "fail" : "pass", nullptr, elapsed_ms() - g_case_began);
    g_case_open = false;
}

#if defined(MCPP_FEATURE_ENV)
kal_intptr variable(const char* name, char* out, kal_uintptr cap) {
    return kal_env_var(name, length(name), out, cap);
}
#endif

#if defined(MCPP_FEATURE_ENV) && defined(MCPP_FEATURE_FS)
// A name the environment gave, opened through the grant whose name is the
// longest prefix of it: openkal has no ambient root, and the grants are how a
// program reaches a name it was told.
bool open_by_name(const char* name, kal_uintptr len, kal_uintptr flags, kal_file* out) {
    char path[1024];
    if (len >= sizeof path) return false;
    for (kal_uintptr i = 0; i < len; ++i) path[i] = name[i] == '\\' ? '/' : name[i];
    kal_dir best{}; kal_uintptr best_len = 0; bool found = false;
    const kal_uintptr grants = kal_fs_preopen_count();
    for (kal_uintptr g = 0; g < grants; ++g) {
        char gname[1024]; kal_uintptr glen = 0; kal_dir d{};
        if (kal_fs_preopen(g, &d, gname, sizeof gname, &glen) != kal_ok || glen == 0 || glen >= sizeof gname)
            continue;
        for (kal_uintptr i = 0; i < glen; ++i) if (gname[i] == '\\') gname[i] = '/';
        while (glen > 1 && gname[glen - 1] == '/') --glen;
        if (glen > len) continue;
        bool prefix = true;
        for (kal_uintptr i = 0; i < glen && prefix; ++i) prefix = gname[i] == path[i];
        if (!prefix) continue;
        if (glen < len && path[glen] != '/' && !(glen == 1 && gname[0] == '/')) continue;
        if (!found || glen > best_len) { best = d; best_len = glen; found = true; }
    }
    if (!found) return false;
    kal_uintptr at = best_len;
    while (at < len && path[at] == '/') ++at;
    return kal_fs_open(best, path + at, len - at, flags, out) == kal_ok;
}
#endif

}  // namespace

bool begin() {
    if (*static_cast<const volatile char*>(okc_mtest_marker) != 'm') return false;
#if defined(MCPP_FEATURE_ENV) && defined(MCPP_FEATURE_FS)
    char value[1024];
    const kal_intptr p = variable("MTEST_PROTOCOL", value, sizeof value);
    if (p != 1 || value[0] != '1') return false;
    const kal_intptr r = variable("MTEST_REPORT", value, sizeof value);
    if (r <= 0 || static_cast<kal_uintptr>(r) >= sizeof value) return false;
    kal_file f{};
    if (!open_by_name(value, static_cast<kal_uintptr>(r),
                      (kal::fs::open::write | kal::fs::open::create | kal::fs::open::append).bits, &f))
        return false;
    g_report = kal_fs_stream(f);
    g_active = true;
#ifdef MCPP_FEATURE_TIME
    g_start = kal_time_monotonic();
#endif
    const kal_intptr m = variable("MTEST_MODE", value, sizeof value);
    g_listing = m == 4 && same(value, 4, "list", 4);

    const kal_intptr s = variable("MTEST_SELECT", value, sizeof value);
    if (s > 0 && static_cast<kal_uintptr>(s) < sizeof value) {
        kal_file sf{};
        if (open_by_name(value, static_cast<kal_uintptr>(s), kal::fs::open::read.bits, &sf)) {
            g_selecting = true;
            const kal_stream in = kal_fs_stream(sf);
            for (;;) {
                const kal_intptr got = kal_stream_read(in, g_select + g_select_len,
                                                       sizeof g_select - 1 - g_select_len);
                if (got <= 0) break;
                g_select_len += static_cast<kal_uintptr>(got);
                if (g_select_len + 1 >= sizeof g_select) break;
            }
            kal_fs_close_file(sf);
        }
    }

    line e;
    e.raw("{\"e\":\"program\",\"protocol\":1,\"level\":2,\"framework\":\"openkal-conformance\","
          "\"version\":\"0.9.0\",\"isolation\":[\"process\"]}");
    e.send();
    return true;
#else
    return false;
#endif
}

bool active()  { return g_active; }
bool listing() { return g_listing; }

bool wanted(const char* id) {
    if (!g_selecting) return true;
    // A line names the section's case, or a part within it (`<id>/<what>'):
    // selecting a part runs the case that declares it (protocol P3.4).
    const kal_uintptr idlen = length(id);
    bool any = false;
    kal_uintptr at = 0; int index = 0;
    while (at < g_select_len) {
        kal_uintptr end = at;
        while (end < g_select_len && g_select[end] != '\n') ++end;
        kal_uintptr len = end - at;
        if (len > 0 && g_select[at + len - 1] == '\r') --len;
        if (len > 0) {
            const bool whole = same(g_select + at, len, id, idlen);
            const bool part = len > idlen && g_select[at + idlen] == '/'
                              && same(g_select + at, idlen, id, idlen);
            if (whole || part) {
                if (index < kMaxSelected) g_matched[index] = true;
                any = true;
            }
            ++index;
        }
        at = end + 1;
    }
    return any;
}

// EVERY CASE HOLDS ONE SERIAL RESOURCE (protocol P7.1). The sections create
// and remove fixed names in the working directory they were given, so two
// copies of this program running at once would observe each other's files; a
// runner that divides the cases among workers runs them one at a time.
void list_case(const char* id) {
    line e; e.raw("{\"e\":\"case\",\"id\":"); e.str(id); e.raw(",\"name\":"); e.str(id);
    e.raw(",\"kind\":\"cts\",\"clauses\":["); e.str(id); e.raw("],\"resources\":[\"serial:openkal-conformance\"]}"); e.send();
}

void section(const char* heading_text) {
    if (!g_active) return;
    close_case();
    id_of(heading_text, g_case, sizeof g_case);
    g_case_open = true;
    g_case_failed = false;
    g_case_parts = 0;
    g_case_began = elapsed_ms();
    begin_event(g_case);
}

void observation(kind k, const char* what, bool held, const char* because) {
    if (!g_active || !g_case_open) return;
    // The part's identifier: the case's, the observation's text, and its
    // ordinal where the text recurs in the case.
    char id[1024]; kal_uintptr n = 0;
    for (kal_uintptr i = 0; g_case[i] && n + 1 < sizeof id; ++i) id[n++] = g_case[i];
    if (n + 1 < sizeof id) id[n++] = '/';
    for (kal_uintptr i = 0; what && what[i] && n + 8 < sizeof id; ++i) id[n++] = what[i];
    id[n] = 0;
    const kal_u64 h = hash(id);
    int seen = 0;
    for (int i = 0; i < g_case_parts; ++i) if (g_part_hash[i] == h) { seen = ++g_part_seen[i]; break; }
    if (seen == 0 && g_case_parts < kMaxParts) { g_part_hash[g_case_parts] = h; g_part_seen[g_case_parts] = 1; ++g_case_parts; }
    if (seen > 1 && n + 8 < sizeof id) {
        id[n++] = ' '; id[n++] = '#';
        char d[8]; int i = 8; int v = seen;
        while (v) { d[--i] = static_cast<char>('0' + v % 10); v /= 10; }
        for (; i < 8; ++i) id[n++] = d[i];
        id[n] = 0;
    }

    line b; b.raw("{\"e\":\"begin\",\"id\":"); b.str(id); b.raw(",\"t\":"); b.num(elapsed_ms());
    b.raw(",\"kind\":\"cts\",\"tags\":["); b.str(name_of(k)); b.raw("],\"clauses\":["); b.str(g_case);
    b.raw("],\"resources\":[\"serial:openkal-conformance\"]}"); b.send();
    if (because != nullptr) {
        end_event(id, "skip", because);
        return;
    }
    if (!held) {
        g_case_failed = true;
        line f; f.raw("{\"e\":\"failure\",\"id\":"); f.str(id);
        f.raw(",\"file\":\"\",\"line\":0,\"message\":"); f.str(what); f.raw("}"); f.send();
    }
    end_event(id, held ? "pass" : "fail", nullptr);
}

void metric(const char* what, kal_u64 ns) {
    if (!g_active || !g_case_open) return;
    line e; e.raw("{\"e\":\"metric\",\"id\":"); e.str(g_case); e.raw(",\"name\":"); e.str(what);
    e.raw(",\"value\":"); e.num(ns); e.raw(",\"unit\":\"ns\",\"better\":\"lower\"}"); e.send();
}

int finish(int held, int failed, int unobserved) {
    if (!g_active) return -1;
    close_case();
    int missing = 0;
    if (g_selecting) {
        kal_uintptr at = 0; int index = 0;
        while (at < g_select_len) {
            kal_uintptr end = at;
            while (end < g_select_len && g_select[end] != '\n') ++end;
            kal_uintptr len = end - at;
            if (len > 0 && g_select[at + len - 1] == '\r') --len;
            if (len > 0) {
                if (index >= kMaxSelected || !g_matched[index]) {
                    line e; e.raw("{\"e\":\"missing\",\"id\":"); e.str(g_select + at, len); e.raw("}"); e.send();
                    ++missing;
                }
                ++index;
            }
            at = end + 1;
        }
    }
    const bool ok = failed == 0 && missing == 0;
    line e; e.raw("{\"e\":\"done\",\"status\":"); e.str(ok ? "pass" : "fail");
    e.raw(",\"counts\":{\"pass\":"); e.num(static_cast<kal_u64>(held));
    e.raw(",\"fail\":"); e.num(static_cast<kal_u64>(failed));
    e.raw(",\"skip\":"); e.num(static_cast<kal_u64>(unobserved)); e.raw("}}");
    e.send();
    return ok ? 0 : 1;
}

}  // namespace okc::protocol
