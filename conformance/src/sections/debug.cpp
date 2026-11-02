module okc.debug;

import openkal.types;
import openkal.debug;
import openkal.process;
import openkal.stream;
import okc.report;
import okc.spec;
import okc.child;

namespace okc::debug {
namespace {
// A function of this suite whose address is asked about. Not inlined, so that
// it has an address of its own.
[[gnu::noinline]] int named_for_the_suite(int x) { return x * 3 + 1; }
}

void run() {
    heading("openkal.debug");
#ifndef MCPP_FEATURE_DEBUG
    unobserved(kind::behaviour, "openkal.debug", "the feature `debug' was not selected");
    return;
#else
    int (*volatile fn)(int) = &named_for_the_suite;
    const kal_uintptr at = reinterpret_cast<kal_uintptr>(fn) + 1;
    char name[256];
    kal_uintptr start = 0;
    const kal_intptr n = kal_debug_symbolize(at, name, sizeof name, &start);
    observe(kind::behaviour, n > 0 || n == -static_cast<kal_intptr>(kal_err_not_found),
            "an address in the program is named, or reported as one no symbol covers");
    if (n > 0) {
        observe(kind::behaviour, start != 0 && start <= at,
                "the named entity begins at or before the address");
        bool mentions = false;
        static const char part[] = "named_for_the_suite";
        const kal_uintptr have = static_cast<kal_uintptr>(n) < sizeof name ? static_cast<kal_uintptr>(n) : sizeof name;
        for (kal_uintptr i = 0; i + (sizeof part - 1) <= have && !mentions; ++i) {
            kal_uintptr k = 0;
            while (k < sizeof part - 1 && name[i + k] == part[k]) ++k;
            mentions = k == sizeof part - 1;
        }
        observe(kind::behaviour, mentions, "the name is the function's");
        char one[1];
        observe(kind::behaviour, kal_debug_symbolize(at, one, 1, nullptr) == n,
                "the length returned is the whole name's, whatever capacity was given");
    }
    observe(kind::behaviour, kal_debug_symbolize(0, name, sizeof name, nullptr) == -static_cast<kal_intptr>(kal_err_not_found),
            "address zero is in no image");
    (void)named_for_the_suite(1);

    // WHAT ENDS A PROGRAM FOR A FAULT SAYS WHERE (0.23): the copy writes through
    // a null pointer, and what it wrote to its error stream before it was ended
    // names the fault and its frames.
    claim("kal_debug_props()", kal_debug_props());
    observe(kind::abi, (kal_debug_props() & ~kal_uintptr{1}) == 0,
            "the capability word contains no position the specification has not assigned");
#if defined(MCPP_FEATURE_PROCESS) && defined(MCPP_FEATURE_ENV) && defined(MCPP_FEATURE_FS)
    if ((kal_debug_props() & kal_uintptr{1}) != 0 && kal::process::has(kal::process::channel)) {
        kal_stream mine{}, theirs{};
        if (kal_process_channel(&mine, &theirs) == kal_ok) {
            const kal_spawn_streams streams{ {0}, {0}, theirs };
            int spawned = -1, status = -1, terminated = -1;
            // The copy is awaited before its error stream is read: a report is
            // a few hundred bytes, which a channel holds.
            const bool ran = start_copy_with(argument_for(errand::fault), 0, streams, spawned, status, terminated);
            kal_process_channel_close(theirs);
            char report[8192];
            kal_uintptr have = 0;
            for (;;) {
                if (have >= sizeof report) break;
                const kal_intptr r = kal_stream_read(mine, report + have, sizeof report - have);
                if (r <= 0) break;
                have += static_cast<kal_uintptr>(r);
            }
            kal_process_channel_close(mine);
            auto holds = [&](const char* part) {
                kal_uintptr n = 0; while (part[n]) ++n;
                for (kal_uintptr i = 0; i + n <= have; ++i) {
                    kal_uintptr k = 0;
                    while (k < n && report[i + k] == part[k]) ++k;
                    if (k == n) return true;
                }
                return false;
            };
            observe(kind::behaviour, ran && spawned == kal_ok && terminated != 0,
                    "a program that faults is ended by its environment");
            observe(kind::behaviour, holds("openkal: fault: ") && holds("openkal:   #0 "),
                    "and what ends it reports the fault and its frames on its error stream");
        }
    } else {
        unobserved(kind::behaviour, "what ends a program for a fault reports the fault and its frames",
                   "the implementation does not state KAL_DEBUG_PROP_FAULT_REPORT, or provides no channel");
    }
#endif
#endif
}

}  // namespace okc::debug
