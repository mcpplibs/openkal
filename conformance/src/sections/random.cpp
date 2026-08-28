module okc.random;

import openkal.types;
import openkal.random;
import okc.report;
import okc.spec;

namespace okc::random {

void run() {
    heading("openkal.random");
#ifndef MCPP_FEATURE_RANDOM
    unobserved(kind::behaviour, "openkal.random", "the interface was not selected");
    return;
#else
    claim("kal_random_props()", kal_random_props());

    // A fill either succeeds completely or changes nothing. The buffer is
    // pre-set to a value the source is unlikely to produce for every byte, so
    // that a partial fill --- the state this interface does not have --- would
    // be visible rather than being mistaken for entropy.
    {
        unsigned char buf[32];
        for (auto& b : buf) b = 0xA5;
        const int rc = kal_random_fill(buf, sizeof buf);
        observe(kind::behaviour, rc == kal_ok || rc == kal_err_again,
                "a fill reports success or a momentarily empty source");
        if (rc == kal_ok) {
            bool all_untouched = true;
            for (auto b : buf) if (b != 0xA5) { all_untouched = false; break; }
            observe(kind::behaviour, !all_untouched,
                    "a successful fill wrote the buffer");
        }
    }

    // ⚠️ TWO FILLS DIFFER, AND THE CHANCE OF A FALSE REPORT IS STATED RATHER
    // THAN LEFT FOR A READER TO WONDER ABOUT.
    //
    // A source that returned a constant would satisfy every check above. Two
    // fills of 32 bytes agreeing by chance has probability 2^-256, which is not
    // reachable; a source that agrees is returning a constant, and that is what
    // this observes.
    {
        unsigned char a[32], b[32];
        const int ra = kal_random_fill(a, sizeof a);
        const int rb = kal_random_fill(b, sizeof b);
        if (ra == kal_ok && rb == kal_ok) {
            bool identical = true;
            for (unsigned i = 0; i < sizeof a; ++i)
                if (a[i] != b[i]) { identical = false; break; }
            observe(kind::behaviour, !identical,
                    "two fills do not return the same bytes");
        } else {
            unobserved(kind::behaviour, "two fills differ",
                       "a fill reported no entropy");
        }
    }

    // A zero-length fill is not an error: a caller computing a length may
    // legitimately arrive at zero, and refusing would oblige every caller to
    // branch before calling.
    {
        const int rc = kal_random_fill(nullptr, 0);
        observe(kind::behaviour, rc == kal_ok,
                "a fill of zero bytes succeeds");
    }
#endif
}

}
