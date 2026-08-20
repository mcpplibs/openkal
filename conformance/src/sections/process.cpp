module okc.process;

import openkal.types;
import openkal.process;
import openkal.fs;
import openkal.env;
import openkal.time;
import okc.report;
import okc.spec;
import okc.child;

namespace okc::process {
namespace {

#if defined(MCPP_FEATURE_PROCESS) && defined(MCPP_FEATURE_ENV)

kal_uintptr length(const char* s) { kal_uintptr n = 0; while (s && s[n]) ++n; return n; }

// The program's own name, expressed the way openkal names things. openkal gives
// a program no operation that resolves a global name, deliberately: that work
// belongs to a C library and is performed once against the supplied
// directories. Here it is performed by hand because this suite has no C
// library, and the twenty lines are what a program pays for doing it itself.
bool locate_self(kal_dir& base, const char*& rel, kal_uintptr& rel_len) {
    kal_uintptr len = 0;
    const char* argv0 = kal_env_arg(0, &len);
    if (!argv0 || len == 0) return false;

    if (argv0[0] != '/') {
        base = kal::fs::working();
        rel = argv0; rel_len = len;
        return true;
    }

    kal_uintptr best = 0, best_len = 0;
    bool found = false;
    for (kal_uintptr i = 0; i < kal_fs_preopen_count(); ++i) {
        kal_dir d{}; const char* name = nullptr; kal_uintptr nlen = 0;
        if (kal_fs_preopen(i, &d, &name, &nlen) != kal_ok) continue;
        if (nlen == 0 || nlen > len) continue;
        bool prefix = true;
        for (kal_uintptr k = 0; k < nlen; ++k) if (name[k] != argv0[k]) { prefix = false; break; }
        if (!prefix) continue;
        if (!(nlen == 1 && name[0] == '/') && argv0[nlen] != '/' && argv0[nlen] != '\0') continue;
        if (!found || nlen > best_len) { found = true; best = i; best_len = nlen; }
    }
    if (!found) return false;

    kal_dir d{}; const char* name = nullptr; kal_uintptr nlen = 0;
    kal_fs_preopen(best, &d, &name, &nlen);
    base = d;
    kal_uintptr at = (nlen == 1 && name[0] == '/') ? 1 : nlen;
    while (at < len && argv0[at] == '/') ++at;
    rel = argv0 + at;
    rel_len = len - at;
    return rel_len > 0;
}

bool start(const char* first_argument, const char* errand_argument, kal_process& out) {
    kal_dir base{}; const char* rel = nullptr; kal_uintptr rel_len = 0;
    if (!locate_self(base, rel, rel_len)) return false;
    const char* argv[2] = { first_argument, errand_argument };
    const kal_uintptr lens[2] = { length(argv[0]), length(argv[1]) };
    const kal_spawn_streams streams{ 0, 0, 0 };
    return kal_process_spawn(base, rel, rel_len, argv, lens, 2, nullptr, nullptr, 0,
                             &streams, &out) == kal_ok;
}

#endif

}  // namespace

void run() {
    heading("openkal.process");
#if !defined(MCPP_FEATURE_PROCESS)
    unobserved(kind::behaviour, "openkal.process", "the interface was not selected");
    return;
#elif !defined(MCPP_FEATURE_ENV)
    unobserved(kind::behaviour, "a program is started, awaited, and reports its status",
               "a started copy is told what to do through its argument vector, which is "
               "openkal.env, and that interface was not selected");
    return;
#else
    claim("kal_process_props", kal_process_props);

    // Starting, awaiting, and the status a program reports.
    {
        kal_process child{};
        if (start("openkal-conformance-child", argument_for(errand::exit_with_33), child)) {
            int status = 0, terminated = 0;
            const int e = kal_process_wait(child, &status, &terminated);
            observe(kind::behaviour, e == kal_ok, "a started program is awaited");
            if (kal::process::has(kal::process::exit_status))
                observe(kind::behaviour, terminated == 0 && status == 33,
                        "the status the program reported is the status it returned");
            else
                unobserved(kind::behaviour, "the status is the value the program returned",
                           "the implementation does not claim prop_exit_status");
            kal_process_close(child);
        } else {
            unobserved(kind::behaviour, "a program is started",
                       "a copy of this program could not be started; the program's own name did "
                       "not resolve against a supplied directory, or the operation failed");
        }
    }

    // Clause 7.6, which exists because the two sides must agree. The copy is
    // started with a first element the specification says shall be passed
    // unaltered, and the copy exits 33 only if it observed exactly that; an
    // implementation that derived the first element from the path, or prepended
    // to the vector, produces 36 instead.
    {
        kal_process child{};
        if (start("openkal-conformance-child", argument_for(errand::exit_with_33), child)) {
            int status = 0, terminated = 0;
            kal_process_wait(child, &status, &terminated);
            observe(kind::behaviour, terminated == 0 && status == 33,
                    "the argument vector is passed unaltered, and the copy read its own name");
            kal_process_close(child);
        }
        if (start("a-name-the-copy-does-not-expect", argument_for(errand::exit_with_33), child)) {
            int status = 0, terminated = 0;
            kal_process_wait(child, &status, &terminated);
            observe(kind::behaviour, terminated == 0 && status == 36,
                    "a different first element reaches the copy, which is how the first "
                    "observation is known to be observing something");
            kal_process_close(child);
        }
    }

    // Termination, if the implementation claims it can request one.
    if (kal::process::has(kal::process::terminate)) {
#ifdef MCPP_FEATURE_TIME
        kal_process child{};
        if (start("openkal-conformance-child", argument_for(errand::wait_to_be_terminated), child)) {
            kal_time_sleep(50u * 1000u * 1000u);
            const int e = kal_process_terminate(child);
            int status = 0, terminated = 0;
            kal_process_wait(child, &status, &terminated);
            observe(kind::behaviour, e == kal_ok,
                    "termination of a started program is requested");
            observe(kind::behaviour, terminated != 0 || status != 0,
                    "the terminated program did not report an ordinary success");
            kal_process_close(child);
        } else {
            unobserved(kind::behaviour, "termination is requested",
                       "a copy of this program could not be started");
        }
#else
        unobserved(kind::behaviour, "termination is requested",
                   "the copy waits to be terminated, which needs openkal.time");
#endif
    } else {
        unobserved(kind::behaviour, "termination is requested",
                   "the implementation does not claim prop_terminate");
    }

    if (performs(kind::abi)) {
        observe(kind::abi, sizeof(kal_process) == sizeof(kal_uintptr),
                "a process handle occupies one machine word");
        observe(kind::abi, sizeof(kal_spawn_streams) == 3 * sizeof(kal_uintptr),
                "the stream selection occupies three machine words");
        const kal_uintptr assigned = (kal::process::terminate | kal::process::stream_passing
                                    | kal::process::exit_status).bits;
        observe(kind::abi, (kal_process_props & ~assigned) == 0,
                "the capability word contains no position the specification has not assigned");
    }

    if (performs(kind::stability)) {
        // Fewer repetitions than elsewhere, and the number is stated: starting
        // a program is the most expensive operation the specification has, and
        // a suite that took a minute here would be run less often than one that
        // takes a second.
        constexpr int rounds = 50;
        bool all = true;
        for (int i = 0; i < rounds && all; ++i) {
            kal_process child{};
            if (!start("openkal-conformance-child", argument_for(errand::exit_with_33), child)) {
                all = false; break;
            }
            int status = 0, terminated = 0;
            if (kal_process_wait(child, &status, &terminated) != kal_ok || status != 33) all = false;
            kal_process_close(child);
        }
        observe(kind::stability, all, "a program started and awaited many times keeps starting");
        put("  programs started and awaited: "); put_signed(rounds); put("\n");
    }

    if (performs(kind::cost)) {
        constexpr int rounds = 20;
        const kal_duration t0 = kal_time_monotonic();
        for (int i = 0; i < rounds; ++i) {
            kal_process child{};
            if (!start("openkal-conformance-child", argument_for(errand::exit_with_33), child)) break;
            int status = 0, terminated = 0;
            kal_process_wait(child, &status, &terminated);
            kal_process_close(child);
        }
        measure("starting a program and awaiting it", kal_time_monotonic() - t0, rounds);
    }
#endif
}

}  // namespace okc::process
