module okc.report;

import openkal.types;
import openkal.stream;
import okc.spec;

namespace okc {
namespace {

int g_held = 0;
int g_failed = 0;
int g_unobserved = 0;

kal_uintptr length(const char* s) {
    kal_uintptr n = 0; while (s && s[n]) ++n; return n;
}

void write(const char* s) {
    if (s && *s) kal_stream_write(kal_stdout(), s, length(s));
}

}  // namespace

void put(const char* s) { write(s); }

void put_signed(long long v) {
    char buf[24];
    int i = static_cast<int>(sizeof buf);
    const bool negative = v < 0;
    unsigned long long u = negative ? 0ull - static_cast<unsigned long long>(v)
                                    : static_cast<unsigned long long>(v);
    if (u == 0) buf[--i] = '0';
    while (u) { buf[--i] = static_cast<char>('0' + u % 10); u /= 10; }
    if (negative) buf[--i] = '-';
    kal_stream_write(kal_stdout(), buf + i, static_cast<kal_uintptr>(static_cast<int>(sizeof buf) - i));
}

void put_hex(kal_uintptr v) {
    char buf[18];
    int i = static_cast<int>(sizeof buf);
    if (v == 0) buf[--i] = '0';
    while (v) {
        const int d = static_cast<int>(v & 15u);
        buf[--i] = static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10);
        v >>= 4;
    }
    write("0x");
    kal_stream_write(kal_stdout(), buf + i, static_cast<kal_uintptr>(static_cast<int>(sizeof buf) - i));
}

void observe(kind k, bool held, const char* what) {
    if (held) { ++g_held;   write("  held         "); }
    else      { ++g_failed; write("  DID NOT HOLD "); }
    write("["); write(name_of(k)); write("] ");
    write(what); write("\n");
}

void unobserved(kind k, const char* what, const char* because) {
    ++g_unobserved;
    write("  not observed ["); write(name_of(k)); write("] ");
    write(what); write(" --- "); write(because); write("\n");
}

void measure(const char* what, kal_u64 total_ns, int iterations) {
    write("  measured     [cost] ");
    write(what);
    write(": ");
    put_signed(iterations ? static_cast<long long>(total_ns / static_cast<unsigned>(iterations)) : 0);
    write(" ns per operation, over ");
    put_signed(iterations);
    write("\n");
}

void claim(const char* what, kal_uintptr word) {
    write("  claimed      "); write(what); write(" = "); put_hex(word); write("\n");
}

void heading(const char* text) { write("\n"); write(text); write("\n"); }
void line(const char* text)    { write(text); write("\n"); }

int held_count()       { return g_held; }
int failed_count()     { return g_failed; }
int unobserved_count() { return g_unobserved; }

void write_inventory() {
    line("openkal conformance suite, version 0.5.0");
    line("");
    line("interface        provision  examined  select with");
    for (const auto& row : inventory) {
        write("  ");
        write(row.name);
        for (kal_uintptr i = length(row.name); i < 17; ++i) write(" ");
        write(row.core ? "core       " : "standard   ");
        write(row.selected ? "yes       " : "no        ");
        write("--features ");
        write(row.feature);
        write("\n");
    }
    line("");
    write("kinds performed: behaviour");
    if (performs(kind::abi))       write(" abi");
    if (performs(kind::stability)) write(" stability");
    if (performs(kind::cost))      write(" cost");
    write("\n");
    for (const auto& row : inventory) {
        if (row.selected) continue;
        write("  openkal is composable, and this run did not examine ");
        write(row.name);
        write("\n");
    }
}

int summarise() {
    write("\nobservations: ");
    put_signed(g_held);       write(" held, ");
    put_signed(g_failed);     write(" did not hold, ");
    put_signed(g_unobserved); write(" not observed\n");

    // An empty run is not a passing run. Every arrangement of features selects
    // at least the core set, so a run that observed nothing means the sections
    // were not built, and reporting success for it would conceal exactly the
    // defect this program exists to find.
    if (g_held == 0 && g_failed == 0) {
        line("nothing was examined; the suite was built with no section");
        return 2;
    }
    line(g_failed == 0 ? "the implementation conforms in every observation made"
                       : "the implementation does not conform");
    return g_failed == 0 ? 0 : 1;
}

}  // namespace okc
