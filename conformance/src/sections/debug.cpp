module okc.debug;

import openkal.types;
import openkal.debug;
import okc.report;
import okc.spec;

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
#endif
}

}  // namespace okc::debug
