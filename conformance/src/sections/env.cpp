module okc.env;

import openkal.types;
import openkal.env;
import okc.report;
import okc.spec;

namespace okc::env {
namespace {

kal_uintptr length(const char* s) { kal_uintptr n = 0; while (s && s[n]) ++n; return n; }

bool same(const char* a, const char* b) {
    kal_uintptr i = 0;
    while (a[i] && a[i] == b[i]) ++i;
    return a[i] == b[i];
}

}  // namespace

void run() {
    heading("openkal.env");
#ifndef MCPP_FEATURE_ENV
    unobserved(kind::behaviour, "openkal.env", "the interface was not selected");
    return;
#else
    const kal_uintptr count = kal_env_arg_count();

    // Position zero is the name by which the program was started, and an
    // environment that has no such name reports an empty string rather than
    // omitting it --- so the count is at least one on every environment.
    observe(kind::behaviour, count >= 1, "the argument vector has at least one element");

    {
        kal_uintptr len = 999;
        const char* a0 = kal_env_arg(0, &len);
        observe(kind::behaviour, a0 != nullptr && len == length(a0),
                "the first argument is reported with its own length");
    }

    // Reading past the end is answered, not undefined: a program that walks the
    // vector must be able to stop.
    {
        kal_uintptr len = 999;
        observe(kind::behaviour, kal_env_arg(count, &len) == nullptr && len == 0,
                "reading past the last argument reports nothing");
    }

    // The distinction the interface exists to preserve. A caller that cannot
    // tell an absent variable from one whose value is empty cannot act
    // correctly upon either.
    {
        const char* name = "OPENKAL_CONFORMANCE_NO_SUCH_VARIABLE_EXISTS";
        kal_uintptr len = 999;
        observe(kind::behaviour,
                kal_env_var(name, length(name), &len) == nullptr,
                "a variable that is absent is reported as absent");
    }
    {
        const char* name = "OPENKAL_CONFORMANCE_EMPTY";
        kal_uintptr len = 999;
        const char* v = kal_env_var(name, length(name), &len);
        if (v == nullptr) {
            unobserved(kind::behaviour,
                       "a variable whose value is empty is distinguished from an absent one",
                       "openkal.env has no operation that sets a variable, so the runner must "
                       "set OPENKAL_CONFORMANCE_EMPTY to the empty string for this to be examined");
        } else {
            observe(kind::behaviour, len == 0,
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
        for (kal_uintptr i = 0; i < n && i < 64; ++i) {
            kal_uintptr nlen = 0, vlen = 0;
            const char* value = nullptr;
            const char* entry = kal_env_var_at(i, &nlen, &value, &vlen);
            if (!entry) { consistent = false; break; }
            kal_uintptr again = 0;
            const char* found = kal_env_var(entry, nlen, &again);
            if (!found || again != vlen) { consistent = false; break; }
            examined = true;
        }
        if (examined)
            observe(kind::behaviour, consistent,
                    "every enumerated variable is found again by its own name");
        else
            unobserved(kind::behaviour, "enumeration reaches the same values as the enquiry",
                       "the environment supplied no variables to enumerate");

        kal_uintptr nlen = 0, vlen = 0;
        const char* value = nullptr;
        observe(kind::behaviour, kal_env_var_at(n, &nlen, &value, &vlen) == nullptr,
                "reading past the last variable reports nothing");
    }

    if (performs(kind::abi)) {
        // The strings are counted, and the count is what a caller uses. An
        // implementation that reported a length excluding or including a
        // terminator inconsistently would be caught by comparing the two.
        kal_uintptr len = 0;
        const char* a0 = kal_env_arg(0, &len);
        bool consistent = a0 != nullptr;
        if (consistent) for (kal_uintptr i = 0; i < len; ++i) if (a0[i] == '\0') consistent = false;
        observe(kind::abi, consistent,
                "a counted string contains no terminator within its own length");
    }

    if (performs(kind::stability)) {
        bool all = true;
        for (int i = 0; i < repetitions && all; ++i) {
            kal_uintptr len = 0;
            const char* a = kal_env_arg(0, &len);
            if (!a) all = false;
        }
        observe(kind::stability, all, "the parameters remain readable after many enquiries");

        // The pointers a program receives are the environment's and remain
        // valid: a program that kept one across other work would otherwise
        // read freed memory, and openkal offers no operation that would have
        // told it not to.
        kal_uintptr l1 = 0, l2 = 0;
        const char* first  = kal_env_arg(0, &l1);
        const char* second = kal_env_arg(0, &l2);
        observe(kind::stability, first && second && l1 == l2 && same(first, second),
                "an argument reads the same on a later enquiry");
    }
#endif
}

}  // namespace okc::env
