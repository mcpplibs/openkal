module okc.prof;

import openkal.types;
import openkal.prof;
import openkal.time;
import okc.report;
import okc.spec;

namespace okc::prof {
namespace {
volatile kal_u64 g_cell = 0;
}

void run() {
    heading("openkal.prof");
#ifndef MCPP_FEATURE_PROF
    unobserved(kind::behaviour, "openkal.prof", "the feature `prof' was not selected");
    return;
#else
    const int active = kal_prof_active();
    claim("kal_prof_active", static_cast<kal_uintptr>(active != 0));
    observe(kind::behaviour, (kal_prof_active() != 0) == (active != 0),
            "whether the program is measured does not change while it runs");

    static const char name[] = "okc.conformance.counter";
    observe(kind::behaviour, kal_prof_counter(name, sizeof name - 1, &g_cell) == kal_ok,
            "a counter is registered, measured or not");
    for (int i = 0; i < 1000; ++i) g_cell = g_cell + 1;

    static const char phase[] = "okc.conformance.phase";
    kal_prof_mark(phase, sizeof phase - 1, 1);
    kal_prof_mark(phase, sizeof phase - 1, 1);   // phases nest
    kal_prof_mark(phase, sizeof phase - 1, 0);
    kal_prof_mark(phase, sizeof phase - 1, 0);
    observe(kind::behaviour, true, "a phase is marked, nested and closed");

    if (performs(kind::cost)) {
        const kal_duration t0 = kal_time_monotonic();
        for (int i = 0; i < cost_iterations; ++i) {
            kal_prof_mark(phase, sizeof phase - 1, 1);
            kal_prof_mark(phase, sizeof phase - 1, 0);
        }
        measure("marking a phase's beginning and end", kal_time_monotonic() - t0, cost_iterations);
    }
#endif
}

}  // namespace okc::prof
