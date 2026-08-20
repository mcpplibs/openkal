module okc.time;

import openkal.types;
import openkal.time;
import okc.report;
import okc.spec;

namespace okc::time {

void run() {
    heading("openkal.time");
#ifndef MCPP_FEATURE_TIME
    unobserved(kind::behaviour, "openkal.time", "the interface was not selected");
    return;
#else
    claim("kal_time_props", kal_time_props);

    // The monotonic source measures elapsed time and never decreases. The
    // observation is made over many reads rather than two, because a source
    // that decreased occasionally --- one assembled from two counters, or read
    // without a barrier --- would satisfy a comparison of two.
    {
        kal_duration previous = kal_time_monotonic();
        bool never_decreased = true;
        for (int i = 0; i < 4096; ++i) {
            const kal_duration now = kal_time_monotonic();
            if (now < previous) { never_decreased = false; break; }
            previous = now;
        }
        observe(kind::behaviour, never_decreased, "the monotonic source never decreases");
    }

    // A granularity of zero would tell a program that measures short intervals
    // nothing, and the specification requires the source to report what it has.
    {
        const kal_duration g = kal_time_monotonic_granularity();
        observe(kind::behaviour, g >= 1, "the monotonic source reports a granularity");
        put("  reported granularity: "); put_signed(static_cast<long long>(g));
        put(" nanoseconds\n");
    }

    // Suspension is for at least the duration asked for and never for less.
    // The comparison allows for the granularity the source reported, because a
    // source that cannot resolve the interval cannot be asked to prove it.
    {
        const kal_duration requested = 20u * 1000u * 1000u;
        const kal_duration slack = kal_time_monotonic_granularity();
        const kal_duration t0 = kal_time_monotonic();
        kal_time_sleep(requested);
        const kal_duration elapsed = kal_time_monotonic() - t0;
        observe(kind::behaviour, elapsed + slack >= requested,
                "suspension lasts at least the duration requested");
        put("  requested 20000000 ns, observed "); put_signed(static_cast<long long>(elapsed));
        put(" ns\n");
    }

    // The wall source is claimed or it is not, and the claim is checked rather
    // than assumed. An implementation whose environment has no such notion
    // reports zero, which is why a value alone cannot answer the question.
    if (kal::time::has(kal::time::wall_available)) {
        const kal_duration w = kal_time_wall();
        // 1 January 2020, as nanoseconds since the epoch. A clock that reported
        // an earlier time would be reporting something other than the time
        // agreed with the rest of the world.
        observe(kind::behaviour, w > 1577836800ull * 1000000000ull,
                "the wall source reports a time later than 2020 when it claims to be available");
    } else {
        unobserved(kind::behaviour, "the wall source reports a real time",
                   "the implementation does not claim prop_wall_available");
    }

    if (performs(kind::abi)) {
        // Clause 6.2: a position that has not been assigned reads as zero, so
        // that a program compiled against a later specification behaves
        // correctly against an earlier implementation. An implementation that
        // set an unassigned position would break that program silently.
        const kal_uintptr assigned = (kal::time::wall_available
                                    | kal::time::monotonic_suspends
                                    | kal::time::sleep_precise).bits;
        observe(kind::abi, (kal_time_props & ~assigned) == 0,
                "the capability word contains no position the specification has not assigned");
        observe(kind::abi, sizeof(kal_duration) == 8,
                "a duration is sixty-four bits, as the interface fixes it");
    }

    if (performs(kind::stability)) {
        kal_duration previous = kal_time_monotonic();
        bool ordered = true;
        for (int i = 0; i < repetitions && ordered; ++i) {
            const kal_duration now = kal_time_monotonic();
            if (now < previous) ordered = false;
            previous = now;
        }
        observe(kind::stability, ordered, "the monotonic source is ordered across many reads");
    }

    if (performs(kind::cost)) {
        const kal_duration t0 = kal_time_monotonic();
        kal_duration sink = 0;
        for (int i = 0; i < cost_iterations; ++i) sink += kal_time_monotonic();
        const kal_duration total = kal_time_monotonic() - t0;
        observe(kind::cost, sink != 0, "the measured reads produced values");
        measure("reading the monotonic source", total, cost_iterations);
    }
#endif
}

}  // namespace okc::time
