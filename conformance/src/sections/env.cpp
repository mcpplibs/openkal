module okc.env;

import openkal.types;
import openkal.env;
import okc.report;
import okc.spec;

namespace okc::env {
namespace {

kal_uintptr length(const char* s) { kal_uintptr n = 0; while (s && s[n]) ++n; return n; }

bool same(const char* a, const char* b, kal_uintptr n) {
    for (kal_uintptr i = 0; i < n; ++i) if (a[i] != b[i]) return false;
    return true;
}

// The buffer every enquiry in this section copies into. The operations report
// the length the value HAS rather than the length they wrote, so a value longer
// than this is still counted correctly and is simply not read.
constexpr kal_uintptr room = 4096;

}  // namespace

void run() {
    heading("openkal.env");
#ifndef MCPP_FEATURE_ENV
    unobserved(kind::behaviour, "openkal.env", "the interface was not selected");
    return;
#else
    char buf[room];
    const kal_uintptr count = kal_env_arg_count();

    // Position zero is the name by which the program was started, and an
    // environment that has no such name reports an empty string rather than
    // omitting it --- so the count is at least one on every environment.
    observe(kind::behaviour, count >= 1, "the argument vector has at least one element");

    {
        const kal_intptr n = kal_env_arg(0, buf, room);
        observe(kind::behaviour, n >= 0,
                "the first argument is reported with its own length");
    }

    // ⭐ THE LENGTH IS THE VALUE'S AND NOT THE BUFFER'S, WHICH IS WHAT LETS A
    // CALLER SIZE FIRST AND WHAT MAKES TRUNCATION IMPOSSIBLE TO MISS.
    {
        const kal_intptr full  = kal_env_arg(0, buf, room);
        const kal_intptr sized = kal_env_arg(0, nullptr, 0);
        observe(kind::behaviour, full >= 0 && full == sized,
                "a capacity of zero reports the length without writing");
    }
    if (const kal_intptr full = kal_env_arg(0, buf, room); full > 1) {
        char small[2] = { 0, 0 };
        const kal_intptr again = kal_env_arg(0, small, 1);
        observe(kind::behaviour, again == full && small[0] == buf[0],
                "a capacity smaller than the value reports the length the value has");
    } else {
        unobserved(kind::behaviour,
                   "a capacity smaller than the value reports the length the value has",
                   "the first argument is too short to be truncated");
    }

    // Reading past the end is answered, not undefined: a program that walks the
    // vector must be able to stop.
    observe(kind::behaviour, kal_env_arg(count, buf, room) < 0,
            "reading past the last argument reports a condition");

    // The distinction the interface exists to preserve. A caller that cannot
    // tell an absent variable from one whose value is empty cannot act
    // correctly upon either --- which is why an absent one is a condition and an
    // empty one is a length of zero.
    {
        const char* name = "OPENKAL_CONFORMANCE_NO_SUCH_VARIABLE_EXISTS";
        observe(kind::behaviour,
                kal_env_var(name, length(name), buf, room) == -kal_err_not_found,
                "a variable that is absent is reported as absent");
    }
    {
        const char* name = "OPENKAL_CONFORMANCE_EMPTY";
        const kal_intptr n = kal_env_var(name, length(name), buf, room);
        if (n < 0) {
            unobserved(kind::behaviour,
                       "a variable whose value is empty is distinguished from an absent one",
                       "openkal.env has no operation that sets a variable, so the runner must "
                       "set OPENKAL_CONFORMANCE_EMPTY to the empty string for this to be examined");
        } else {
            observe(kind::behaviour, n == 0,
                    "a variable whose value is empty is reported as present and empty");
        }
    }

    // Enumeration reaches the same values as the enquiry. The order is
    // unspecified, so the observation is that a name found by enumeration is
    // found again by name --- not that the two agree position by position.
    {
        const kal_uintptr n = kal_env_var_count();
        bool consistent = true;
        bool examined = false;
        char name[room];
        for (kal_uintptr i = 0; i < n && i < 64; ++i) {
            const kal_intptr nlen = kal_env_var_at(i, name, room);
            if (nlen < 0 || static_cast<kal_uintptr>(nlen) >= room) { consistent = false; break; }
            if (kal_env_var(name, static_cast<kal_uintptr>(nlen), buf, room) < 0) {
                consistent = false; break;
            }
            examined = true;
        }
        if (examined)
            observe(kind::behaviour, consistent,
                    "every enumerated variable is found again by its own name");
        else
            unobserved(kind::behaviour, "enumeration reaches the same values as the enquiry",
                       "the environment supplied no variables to enumerate");

        observe(kind::behaviour, kal_env_var_at(n, buf, room) < 0,
                "reading past the last variable reports a condition");
    }

    if (performs(kind::abi)) {
        // The strings are counted, and the count is what a caller uses. An
        // implementation that reported a length excluding or including a
        // terminator inconsistently would be caught by comparing the two.
        const kal_intptr len = kal_env_arg(0, buf, room);
        bool consistent = len >= 0 && static_cast<kal_uintptr>(len) < room;
        if (consistent)
            for (kal_intptr i = 0; i < len; ++i) if (buf[i] == '\0') consistent = false;
        observe(kind::abi, consistent,
                "a counted string contains no terminator within its own length");

        // ⚠️ THE OPERATION WRITES NO MORE THAN THE CAPACITY IT WAS GIVEN. An
        // implementation that copied the whole value regardless would corrupt
        // the caller past a buffer the caller sized correctly, which is the
        // defect this shape exists to make impossible.
        char guarded[64];
        for (auto& c : guarded) c = '\x7f';
        const kal_intptr n = kal_env_arg(0, guarded, 8);
        bool untouched = true;
        for (int i = 8; i < 64; ++i) if (guarded[i] != '\x7f') untouched = false;
        observe(kind::abi, n >= 0 && untouched,
                "a copy writes no more than the capacity it was given");
    }

    if (performs(kind::stability)) {
        bool all = true;
        for (int i = 0; i < repetitions && all; ++i)
            if (kal_env_arg(0, buf, room) < 0) all = false;
        observe(kind::stability, all, "the parameters remain readable after many enquiries");

        // The values a program receives remain what they were: a program that
        // read one twice must be told the same thing, and openkal offers no
        // operation that would have changed it in between.
        char first[room], second[room];
        const kal_intptr l1 = kal_env_arg(0, first,  room);
        const kal_intptr l2 = kal_env_arg(0, second, room);
        observe(kind::stability,
                l1 >= 0 && l1 == l2 && static_cast<kal_uintptr>(l1) < room
                    && same(first, second, static_cast<kal_uintptr>(l1)),
                "an argument reads the same on a later enquiry");
    }
#endif
}

}  // namespace okc::env
