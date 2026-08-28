module okc.stream;

import openkal.types;
import openkal.stream;
import openkal.time;
import okc.report;
import okc.spec;

namespace okc::stream {

void run() {
    heading("openkal.stream");
#ifndef MCPP_FEATURE_CORE
    unobserved(kind::behaviour, "openkal.stream", "the core set was not selected");
    return;
#else
    const kal_stream out = kal_stdout();

    // Clause 7.4: the whole buffer is transferred or the condition that
    // prevented it is reported. A partial transfer is not a successful
    // outcome, so the count and the error are examined together --- a suite
    // that looked only at the error would accept an implementation that wrote
    // half the bytes and said nothing.
    {
        const char msg[] = "  (openkal.stream: this line was written by the suite)\n";
        const kal_uintptr n = sizeof msg - 1;
        const kal_intptr r = kal_stream_write(out, msg, n);
        observe(kind::behaviour, r == static_cast<kal_intptr>(n),
                "a write transfers the whole buffer and reports it");
    }

    // A transfer of nothing is a transfer. An implementation that refused it
    // would break every caller that writes a computed length.
    {
        const kal_intptr r = kal_stream_write(out, "", 0);
        observe(kind::behaviour, r == 0,
                "a write of no bytes succeeds and reports no bytes");
    }

    // Clause 6.6: the standard streams are borrowed, so they remain usable
    // after any number of operations and there is nothing to release.
    observe(kind::behaviour,
            kal_stdin().h != kal_stdout().h || kal_stdout().h == kal_stderr().h,
            "the three standard streams are obtainable");

    // Committing what is not buffered is not a failure. An implementation that
    // reported one would oblige every caller to distinguish it from a real
    // failure to reach the medium.
    observe(kind::behaviour, kal_stream_flush(out) == kal_ok,
            "committing an unbuffered stream reports success");

    // The property a consumer must have before it has transferred anything.
    claim("kal_stream_props(stdout)", kal::properties(out).bits);
    observe(kind::behaviour,
            (kal::properties(out).bits & ~kal::stream_prop::interactive.bits) == 0,
            "a stream reports no position the specification has not assigned");

    if (performs(kind::abi)) {
        // The width of a handle is what allows an implementation to store a
        // descriptor, an operating-system handle or a capability index in it.
        observe(kind::abi, sizeof(kal_stream) == sizeof(kal_uintptr),
                "a stream handle occupies one machine word");
        observe(kind::abi, sizeof(kal_intptr) == sizeof(kal_uintptr),
                "a transfer result occupies two machine words");
        // The same handle answers the same way twice. An implementation that
        // packed a generation into the word and incremented it on use would
        // pass every behavioural observation above and fail here.
        observe(kind::abi, kal_stdout().h == kal_stdout().h,
                "the standard stream handle is stable between enquiries");
    }

    if (performs(kind::stability)) {
        bool all = true;
        for (int i = 0; i < repetitions && all; ++i) {
            const kal_intptr r = kal_stream_write(out, "", 0);
            if (r < 0) all = false;
        }
        observe(kind::stability, all, "a stream serves the operation repeated many times");
    }

    if (performs(kind::cost)) {
        const kal_duration t0 = kal_time_monotonic();
        for (int i = 0; i < cost_iterations; ++i) kal_stream_write(out, "", 0);
        measure("a transfer of no bytes", kal_time_monotonic() - t0, cost_iterations);
    }
#endif
}

}  // namespace okc::stream
