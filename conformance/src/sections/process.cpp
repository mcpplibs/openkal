module okc.process;

import openkal.types;
import openkal.process;
import openkal.stream;   // the channel is read and written through it
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
    claim("kal_process_props()", kal_process_props());

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

    // THE THREE OPERATIONS VERSION 0.8 ADDS TO THIS INTERFACE.
    //
    // They are examined here and not in a section of their own, because they are
    // part of openkal.process: clause 6.1 makes an interface provided IN PART a
    // deviation, so an implementation reaching this section has undertaken to
    // provide them.
    {
        kal_stream mine{}, theirs{};
        const int rc = kal_process_channel(&mine, &theirs);
        observe(kind::behaviour, rc == kal_ok, "a channel is created");
        if (rc == kal_ok) {
            const char msg[] = "through the channel";
            const kal_intptr w = kal_stream_write(theirs, msg, sizeof msg - 1);
            observe(kind::behaviour, w == static_cast<kal_intptr>(sizeof msg - 1),
                    "the far end of a channel accepts bytes");

            char buf[64] = {0};
            const kal_intptr r = kal_stream_read(mine, buf, sizeof buf);
            bool same = r == static_cast<kal_intptr>(sizeof msg - 1);
            for (kal_intptr i = 0; same && i < r; ++i)
                if (buf[i] != msg[i]) same = false;
            observe(kind::behaviour, same, "the near end reads what the far end wrote");

            // THE END OF INPUT IS WHAT THE RELEASE IS FOR. A parent that does not
            // release the far end after a spawn never observes it, which is the
            // deadlock this pair invites and the reason the release is declared
            // beside the operation rather than left to openkal.stream.
            kal_process_channel_close(theirs);
            const kal_intptr eof = kal_stream_read(mine, buf, sizeof buf);
            observe(kind::behaviour, eof == 0,
                    "closing the far end is observed as end of input on the near one");
            kal_process_channel_close(mine);
        }

        // The property word and the operations must agree, which is what clause
        // 6.2 requires of a word and is the only thing that can be observed of
        // an operation an implementation declines.
        if (kal::process::has(kal::process::channel))
            observe(kind::behaviour, rc == kal_ok,
                    "a claimed channel is provided when asked for");
        else
            observe(kind::behaviour, rc == kal_err_not_supported,
                    "an unclaimed channel is refused rather than half provided");
    }

    if (performs(kind::abi)) {
        observe(kind::abi, sizeof(kal_process) == sizeof(kal_uintptr),
                "a process handle occupies one machine word");
        observe(kind::abi, sizeof(kal_spawn_streams) == 3 * sizeof(kal_uintptr),
                "the stream selection occupies three machine words");
        const kal_uintptr assigned = (kal::process::terminate | kal::process::stream_passing
                                    | kal::process::exit_status | kal::process::channel
                                    | kal::process::grant_dir
                                    | kal::process::bound_lifetime
                                    | kal::process::own_job).bits;
        observe(kind::abi, sizeof(kal_preopen) == 3 * sizeof(kal_uintptr),
                "a directory grant occupies three machine words");
        observe(kind::abi, sizeof(kal_spawn) == 5 * sizeof(kal_uintptr),
                "the description of a start occupies five machine words");
        observe(kind::abi, (kal_process_props() & ~assigned) == 0,
                "the capability word contains no position the specification has not assigned");

        // ⚠️ A FLAG THAT IS NOT CLAIMED SHALL REFUSE RATHER THAN PERFORM
        // SOMETHING ELSE. A caller that asks for a bound lifetime asked for it;
        // a program started WITHOUT the binding is not the program it asked to
        // start, and an implementation that quietly starts one anyway is the
        // failure the flag exists to remove.
        //
        // ⭐ ONE LOOP OVER BOTH FLAGS RATHER THAN A BLOCK EACH, which is the
        // shape 0.11 made possible: they are two positions in one word now, so
        // the observation is written once and reads the same for the next flag
        // that arrives.
        // ⚠️⚠️ THIS SUITE OBSERVES THE REFUSAL AND NOT THE EFFECT, AND THAT IS A
        // LIMIT OF WHAT openkal CAN SEE RATHER THAN AN OMISSION HERE.
        //
        // `work' sets the directory a started program runs in, and openkal has no
        // operation that READS a working directory --- deliberately: it has no
        // ambient one. `KAL_SPAWN_OWN_JOB' makes a started program and its
        // descendants one unit, and openkal has no operation that enumerates
        // processes. So a conforming implementation could honour both, or
        // neither, and nothing written against openkal alone could tell.
        //
        // ⇒ They are in the same category as the streams a spawn is given:
        // configuration of a program that openkal does not introspect, because a
        // started program need not be an openkal program at all. The effects are
        // observed where they can be --- openkal-musl's probes call `getcwd' and
        // start a process tree --- and this suite observes what IT can, which is
        // that an implementation not claiming a position refuses rather than
        // starting a program that lacks what was asked for.
        //
        // ⚠️ THE MODULE SPELLINGS AND NOT THE MACROS. `KAL_SPAWN_*' are macros,
        // and a macro does not cross a module boundary --- this suite consumes
        // openkal as a module, so the C spelling is simply not in scope here.
        // The same thing caught `KAL_LOCK_*' one release ago, which is why
        // `kal::process::*_flag' exists at all.
        struct { kal_uintptr bit; const char* what; } const asks[] = {
            { kal::process::bound_lifetime_flag.bits,
              "a lifetime this implementation cannot bind is refused, not ignored" },
            { kal::process::own_job_flag.bits,
              "a job this implementation cannot form is refused, not ignored"      },
        };
        for (const auto& ask : asks) {
            if ((kal_process_props() & ask.bit) == 0) {
                kal_process p{};
                const char* argv[1] = { "x" };
                const kal_uintptr lens[1] = { 1 };
                const kal_spawn how{ kal::fs::working(), kal::fs::working(), nullptr, 0, ask.bit };
                const int e = kal_process_spawn(&how, "x", 1, argv, lens, 1,
                                                nullptr, nullptr, 0, nullptr, &p);
                observe(kind::behaviour, e == kal_err_not_supported, ask.what);
            } else {
                unobserved(kind::behaviour, ask.what,
                           "the implementation claims this position");
            }
        }
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
