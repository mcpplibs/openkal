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

    // SAMPLES (0.23): taken by the environment, at the rate asked, of this
    // program's contexts, with the frames of each.
    claim("kal_prof_props()", kal_prof_props());
    observe(kind::abi, (kal_prof_props() & ~kal_uintptr{1}) == 0,
            "the capability word contains no position the specification has not assigned");
    if ((kal_prof_props() & kal_uintptr{1}) == 0) {
        observe(kind::behaviour, kal_prof_sample_begin(1000) == kal_err_not_supported,
                "sampling is refused where the implementation does not state it");
    } else {
        observe(kind::behaviour, kal_prof_sample_begin(0) == kal_err_invalid
                                 && kal_prof_sample_begin(20000) == kal_err_invalid,
                "a rate of zero, or above ten thousand a second, is refused");
        observe(kind::behaviour, kal_prof_sample_begin(1000) == kal_ok, "sampling begins");
        observe(kind::behaviour, kal_prof_sample_begin(1000) == kal_err_again, "and is not begun twice");
        // Processor time, so that a timer of processor time fires.
        volatile kal_u64 spin = 0;
        const kal_duration until = kal_time_monotonic() + 200000000;
        while (kal_time_monotonic() < until) for (int i = 0; i < 10000; ++i) spin = spin + 1;
        static kal_prof_sample got[32];
        const kal_intptr n = kal_prof_sample_take(got, 32);
        observe(kind::behaviour, n > 0 && got[0].depth >= 1 && got[0].depth <= 64 && got[0].pc[0] != 0,
                "samples are taken, each with at least its innermost frame");
        observe(kind::behaviour, kal_prof_sample_end() == kal_ok, "sampling ends");
    }
#endif
}

}  // namespace okc::prof
