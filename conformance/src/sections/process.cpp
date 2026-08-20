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

void run() {
    heading("openkal.process");
#if !defined(MCPP_FEATURE_PROCESS)
    unobserved(kind::behaviour, "openkal.process", "the interface was not selected");
    return;
#elif !defined(MCPP_FEATURE_ENV) || !defined(MCPP_FEATURE_FS)
    unobserved(kind::behaviour, "a program is started, awaited, and reports its status",
               "a started copy is told what to do through its argument vector, which is "
               "openkal.env, and that interface was not selected");
    return;
#else
    claim("kal_process_props", kal_process_props);

    // Starting, awaiting, and the status a program reports.
    {
        int status = 0, terminated = 0;
        if (start_copy("openkal-conformance-child", argument_for(errand::exit_with_33),
                       status, terminated)) {
            observe(kind::behaviour, true, "a started program is awaited");
            if (kal::process::has(kal::process::exit_status))
                observe(kind::behaviour, terminated == 0 && status == 33,
                        "the status the program reported is the status it returned");
            else
                unobserved(kind::behaviour, "the status is the value the program returned",
                           "the implementation does not claim prop_exit_status");
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
        int status = 0, terminated = 0;
        if (start_copy("openkal-conformance-child", argument_for(errand::exit_with_33),
                       status, terminated))
            observe(kind::behaviour, terminated == 0 && status == 33,
                    "the argument vector is passed unaltered, and the copy read its own name");
        if (start_copy("a-name-the-copy-does-not-expect", argument_for(errand::exit_with_33),
                       status, terminated))
            observe(kind::behaviour, terminated == 0 && status == 36,
                    "a different first element reaches the copy, which is how the first "
                    "observation is known to be observing something");
    }

    // Termination, if the implementation claims it can request one.
    if (kal::process::has(kal::process::terminate)) {
#ifdef MCPP_FEATURE_TIME
        kal_process child{};
        if (start_copy_running("openkal-conformance-child",
                               argument_for(errand::wait_to_be_terminated), child)) {
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
            int status = 0, terminated = 0;
            if (!start_copy("openkal-conformance-child", argument_for(errand::exit_with_33),
                            status, terminated) || status != 33) all = false;
        }
        observe(kind::stability, all, "a program started and awaited many times keeps starting");
        put("  programs started and awaited: "); put_signed(rounds); put("\n");
    }

    if (performs(kind::cost)) {
        constexpr int rounds = 20;
        const kal_duration t0 = kal_time_monotonic();
        for (int i = 0; i < rounds; ++i) {
            int status = 0, terminated = 0;
            if (!start_copy("openkal-conformance-child", argument_for(errand::exit_with_33),
                            status, terminated)) break;
        }
        measure("starting a program and awaiting it", kal_time_monotonic() - t0, rounds);
    }
#endif
}

}  // namespace okc::process
