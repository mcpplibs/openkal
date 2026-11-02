module okc.process;

import openkal.types;
import openkal.process;
import openkal.stream;   // the channel is read and written through it
import openkal.fs;
import openkal.env;
import openkal.time;
import openkal.memory;
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

    // ENDING AS THE REQUEST WOULD HAVE (version 0.17). With no request there is
    // nothing to reproduce, and the operation returns.
    observe(kind::behaviour, kal_process_end_as_requested() == kal_err_invalid,
            "ending as requested, with no request, returns kal_err_invalid");
#ifdef MCPP_FEATURE_TIME
    if (kal::process::has(kal::process::stop_requested) && kal::process::has(kal::process::terminate)) {
        kal_process child{};
        if (start_copy_running("openkal-conformance-child",
                               argument_for(errand::end_as_requested), child)) {
            kal_time_sleep(300u * 1000u * 1000u);
            kal_process_terminate(child);
            int status = 0, terminated = 0;
            kal_process_wait(child, &status, &terminated);
            if (terminated == 0 && status == 98)
                unobserved(kind::behaviour, "a program ends as the request would have ended it",
                           "a request to terminate did not arrive as a request on this environment");
            else
                observe(kind::behaviour, terminated != 0 && status != 99,
                        "a program that took the request over ends as the request would have ended it");
            kal_process_close(child);
        }
    }
#endif

    // WHAT IS KNOWN OF A RUNNING PROGRAM (version 0.17).
    {
        kal_process self{};
        const int e = kal_process_self(&self);
        observe(kind::behaviour, e == kal_ok, "a program has a handle to itself");
        if (e == kal_ok) {
            kal_program_info info{};
            info.self_size = sizeof info;
            observe(kind::behaviour, kal_process_info(self, kal::process::info::all, &info) == kal_ok,
                    "a program is asked about itself");
            observe(kind::behaviour, (info.present & ~kal::process::info::all) == 0,
                    "no field the specification has not assigned is reported");
            observe(kind::behaviour, (info.present & kal::process::info::ended) == 0 || info.ended == 0,
                    "a program asking about itself has not ended");
            int status = 0, terminated = 0;
            observe(kind::behaviour, kal_process_wait(self, &status, &terminated) == kal_err_invalid,
                    "a program does not wait for itself");
            // MEMORY (0.20): what the environment charges the program now, and
            // the most it has charged, which is never less.
            if ((info.present & kal::process::info::memory) != 0) {
                observe(kind::behaviour, info.memory_bytes > 0 && info.memory_peak_bytes >= info.memory_bytes,
                        "a running program is charged memory, and its peak is no less");
            } else {
                unobserved(kind::behaviour, "a running program is charged memory, and its peak is no less",
                           "the implementation does not report KAL_PINFO_MEMORY");
            }
            kal_program_info tiny{};
            tiny.self_size = 4;
            observe(kind::behaviour, kal_process_info(self, kal::process::info::all, &tiny) == kal_err_invalid,
                    "a structure too small for its fields is refused");
            if (kal::process::has(kal::process::open_id) && (info.present & kal::process::info::id) != 0) {
                kal_process again{};
                const int o = kal_process_open_id(info.id, &again);
                observe(kind::behaviour, o == kal_ok, "a program is found by its identifier");
                if (o == kal_ok) {
                    kal_program_info other{};
                    other.self_size = sizeof other;
                    kal_process_info(again, kal::process::info::all, &other);
                    observe(kind::behaviour, other.id == info.id
                            && ((info.present & other.present & kal::process::info::started) == 0
                                || other.started_ns == info.started_ns),
                            "and is the same program, started at the same moment");
                    observe(kind::behaviour, kal_process_terminate(again) == kal_err_not_supported,
                            "a program found by its identifier is not the caller's to end");
                    kal_process_close(again);
                }
            } else {
                unobserved(kind::behaviour, "a program is found by its identifier",
                           "the implementation does not claim KAL_PROCESS_PROP_OPEN_ID");
            }
            kal_process_close(self);
        }
    }
    {
        int status = 0, terminated = 0;
        kal_process child{};
#ifdef MCPP_FEATURE_TIME
        if (start_copy_running("openkal-conformance-child", argument_for(errand::exit_with_33), child)) {
            kal_process_wait(child, &status, &terminated);
            kal_program_info info{};
            info.self_size = sizeof info;
            const int e = kal_process_info(child, kal::process::info::ended | kal::process::info::cpu, &info);
            observe(kind::behaviour, e == kal_ok && ((info.present & kal::process::info::ended) == 0 || info.ended != 0),
                    "a started program that was awaited is reported as ended");
            // Version 0.21: what it used remains while its handle is held. The
            // value is not compared: a program that only ends may be charged
            // less than an environment's clock tick.
            observe(kind::behaviour, e == kal_ok && (info.present & kal::process::info::cpu) != 0,
                    "an ended program's processor time remains until its handle is released");
            kal_process_close(child);
        }
#endif
    }

    // A START THAT DID NOT HAPPEN IS REPORTED, AND BY ITS REASON. Version 0.13.
    //
    // A name that does not exist needs no file to be created, so this is
    // observed on every implementation that provides the interface. An
    // implementation that reported kal_ok and a handle here would leave the
    // caller to discover the failure as an exit status, which cannot be told
    // apart from a program that ran and returned it.
    {
        kal_process p{};
        const char* name = "okc-conformance-no-such-program";
        const kal_uintptr len = 31;
        const char* argv[1] = { name };
        const kal_uintptr lens[1] = { len };
        const kal_spawn how{ kal::fs::working(), kal::fs::working(), nullptr,
                             nullptr, 0, 0 };
        const int e = kal_process_spawn(&how, name, len, argv, lens, 1,
                                        nullptr, nullptr, 0, nullptr, &p);
        observe(kind::behaviour, e != kal_ok,
                "a start that did not happen is not reported as success");
        observe(kind::behaviour, e == kal_err_not_found,
                "and a name that is not there is reported as absent");
        if (e == kal_ok) { int st = 0, te = 0; kal_process_wait(p, &st, &te); kal_process_close(p); }
    }

    // A name that is there and is not a program. Where the volume records
    // whether a node may be started, the record is set first, so that what the
    // start meets is the form of the content and not a refusal to start it.
    // Where it does not, the environment decides from the name or the content,
    // and a text file is not a program on any of them.
    {
        const char* name = "okc-conformance-not-a-program.tmp";
        const kal_uintptr len = 33;
        kal_file f{};
        const int oe = kal::fs::open_file(kal::fs::working(), name, len,
                                          kal::fs::open::write | kal::fs::open::create
                                              | kal::fs::open::truncate, &f);
        if (oe == kal_ok) {
            const char text[] = "this file is text and is not a program\n";
            kal_stream_write(kal_fs_stream(f), text, sizeof text - 1);
            kal_fs_close_file(f);
            const bool recorded =
                (kal_fs_props(kal::fs::working()) & kal::fs::executable.bits) != 0;
            if (recorded) kal_fs_set_executable_at(kal::fs::working(), name, len, 1);

            kal_process p{};
            const char* argv[1] = { name };
            const kal_uintptr lens[1] = { len };
            const kal_spawn how{ kal::fs::working(), kal::fs::working(), nullptr,
                                 nullptr, 0, 0 };
            const int e = kal_process_spawn(&how, name, len, argv, lens, 1,
                                            nullptr, nullptr, 0, nullptr, &p);
            observe(kind::behaviour, e != kal_ok,
                    "a file that is not a program is not reported as started");
            if (recorded)
                observe(kind::behaviour, e == kal_err_not_program,
                        "and one recorded as startable is reported as not a program");
            else
                observe(kind::behaviour, e != kal_err_io,
                        "and the reason is not reported as a failure of the medium");
            if (e == kal_ok) { int st = 0, te = 0; kal_process_wait(p, &st, &te); kal_process_close(p); }
            kal_fs_remove(kal::fs::working(), name, len);
        } else {
            unobserved(kind::behaviour, "a file that is not a program is not reported as started",
                       "the file could not be created in the working directory");
        }
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

    // WHAT A UNIT USES, AND A LIMIT ON IT (version 0.20). Both are observed
    // on a copy that writes its memory through, so that every environment's
    // measure sees it.
#ifdef MCPP_FEATURE_TIME
    if (kal::process::has(kal::process::job) && kal::process::has(kal::process::job_usage)) {
        kal_job unit{};
        kal_process child{};
        if (start_copy_in_unit(argument_for(errand::hold_memory), unit, child)) {
            kal_job_usage use{};
            use.self_size = sizeof use;
            int e = kal_err_invalid;
            for (int i = 0; i < 1000; ++i) {
                e = kal_process_job_usage(unit, &use);
                if (e == kal_ok && use.memory_bytes >= (kal_u64{24} << 20)) break;
                kal_time_sleep(5u * 1000u * 1000u);
            }
            claim("the unit's members", use.members);
            claim("the unit's memory_bytes", static_cast<kal_uintptr>(use.memory_bytes));
            observe(kind::behaviour, e == kal_ok && use.members >= 1,
                    "a unit's use is reported, and counts the program started in it");
            observe(kind::behaviour, e == kal_ok && use.memory_bytes >= (kal_u64{24} << 20),
                    "the unit is charged the memory its program wrote");
            kal_process_job_terminate(unit);
            int status = 0, terminated = 0;
            kal_process_wait(child, &status, &terminated);
            observe(kind::behaviour, terminated == 1,
                    "a program its unit's termination ended is reported as terminated, not as limited");
            // What it used remains while its handle is held, waited for or not.
            kal_program_info after{};
            after.self_size = sizeof after;
            if (kal_process_info(child, kal::process::info::memory, &after) == kal_ok
                && (after.present & kal::process::info::memory) != 0) {
                claim("the ended program's memory_bytes", static_cast<kal_uintptr>(after.memory_bytes));
                claim("the ended program's memory_peak_bytes", static_cast<kal_uintptr>(after.memory_peak_bytes));
                observe(kind::behaviour, after.memory_bytes == 0 && after.memory_peak_bytes >= (kal_u64{24} << 20),
                        "an ended program is charged nothing now, and its peak remains until its handle is released");
            } else {
                unobserved(kind::behaviour,
                           "an ended program is charged nothing now, and its peak remains until its handle is released",
                           "the implementation does not report KAL_PINFO_MEMORY for an ended program");
            }
            kal_process_close(child);
            kal_process_job_close(unit);
        } else {
            unobserved(kind::behaviour, "a copy is started in a unit",
                       "a copy of this program could not be started");
        }
    } else {
        kal_job_usage use{};
        use.self_size = sizeof use;
        observe(kind::behaviour, kal_process_job_usage(kal_job{}, &use) == kal_err_not_supported,
                "an unclaimed unit usage is refused rather than half provided");
    }

    if (kal::process::has(kal::process::job)
        && (kal::process::has(kal::process::job_limit_hard)
            || kal::process::has(kal::process::job_limit_supervised))) {
        kal_job unit{};
        kal_process child{};
        if (start_copy_in_unit(argument_for(errand::grow_memory), unit, child)) {
            observe(kind::behaviour, kal_process_job_limit(unit, kal_uintptr{99}, 1) == kal_err_invalid,
                    "a kind of limit this version does not assign is refused");
            const int e = kal_process_job_limit(unit, kal::process::limit_memory, kal_u64{96} << 20);
            observe(kind::behaviour, e == kal_ok, "a limit is placed on a unit");
            int status = 0, terminated = 0;
            kal_process_wait(child, &status, &terminated);
            claim("the limited program's status", static_cast<kal_uintptr>(status));
            claim("the limited program's terminated", static_cast<kal_uintptr>(terminated));
            observe(kind::behaviour, terminated == kal::process::terminated_limit,
                    "a unit that exceeds its limit is ended, and its program reported as limited");
            kal_process_close(child);
            kal_process_job_close(unit);
        } else {
            unobserved(kind::behaviour, "a copy is started in a unit",
                       "a copy of this program could not be started");
        }
    } else {
        observe(kind::behaviour,
                kal_process_job_limit(kal_job{}, kal::process::limit_memory, 1) == kal_err_not_supported,
                "an unclaimed unit limit is refused rather than half provided");
    }
#endif

    if (performs(kind::abi)) {
        observe(kind::abi, sizeof(kal_process) == sizeof(kal_uintptr),
                "a process handle occupies one machine word");
        observe(kind::abi, sizeof(kal_spawn_streams) == 3 * sizeof(kal_uintptr),
                "the stream selection occupies three machine words");
        const kal_uintptr assigned = (kal::process::terminate | kal::process::stream_passing
                                    | kal::process::exit_status | kal::process::channel
                                    | kal::process::grant_dir
                                    | kal::process::bound_lifetime
                                    | kal::process::job
                                    | kal::process::stop_requested
                                    | kal::process::open_id
                                    | kal::process::one_string
                                    | kal::process::job_usage
                                    | kal::process::job_limit_hard
                                    | kal::process::job_limit_supervised
                                    | kal::process::null_streams
                                    | kal::process::confine).bits;
        observe(kind::abi, sizeof(kal_preopen) == 3 * sizeof(kal_uintptr),
                "a directory grant occupies three machine words");
        observe(kind::abi, sizeof(kal_spawn) == 6 * sizeof(kal_uintptr),
                "the description of a start occupies six machine words");
        observe(kind::abi, sizeof(kal_job) == sizeof(kal_uintptr),
                "a unit handle occupies one machine word");
        observe(kind::abi, (kal_process_props() & ~assigned) == 0,
                "the capability word contains no position the specification has not assigned");

        // A FLAG THAT IS NOT CLAIMED SHALL REFUSE RATHER THAN PERFORM
        // SOMETHING ELSE. A caller that asks for a bound lifetime asked for it;
        // a program started WITHOUT the binding is not the program it asked to
        // start, and an implementation that quietly starts one anyway is the
        // failure the flag exists to remove.
        //
        // ONE LOOP OVER BOTH FLAGS RATHER THAN A BLOCK EACH, which is the
        // shape 0.11 made possible: they are two positions in one word now, so
        // the observation is written once and reads the same for the next flag
        // that arrives.
        // THIS SUITE OBSERVES THE REFUSAL AND NOT THE EFFECT, AND THAT IS A
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
        // THE MODULE SPELLINGS AND NOT THE MACROS. `KAL_SPAWN_*' are macros,
        // and a macro does not cross a module boundary --- this suite consumes
        // openkal as a module, so the C spelling is simply not in scope here.
        // The same thing caught `KAL_LOCK_*' one release ago, which is why
        // `kal::process::*_flag' exists at all.
        if ((kal_process_props() & kal::process::bound_lifetime.bits) == 0) {
            kal_process p{};
            const char* argv[1] = { "x" };
            const kal_uintptr lens[1] = { 1 };
            const kal_spawn how{ kal::fs::working(), kal::fs::working(), nullptr,
                                 nullptr, 0, kal::process::bound_lifetime_flag.bits };
            const int e = kal_process_spawn(&how, "x", 1, argv, lens, 1,
                                            nullptr, nullptr, 0, nullptr, &p);
            observe(kind::behaviour, e == kal_err_not_supported,
                    "a lifetime this implementation cannot bind is refused, not ignored");
        } else {
            unobserved(kind::behaviour,
                       "a lifetime this implementation cannot bind is refused, not ignored",
                       "the implementation claims prop_bound_lifetime");
        }

        // KAL_SPAWN_ONE_STRING (0.17.1): an environment that gives a program a
        // vector, and so does not claim the position, refuses the request.
        if ((kal_process_props() & kal::process::one_string.bits) == 0) {
            kal_process p{};
            const char* argv[1] = { "x" };
            const kal_uintptr lens[1] = { 1 };
            const kal_spawn how{ kal::fs::working(), kal::fs::working(), nullptr,
                                 nullptr, 0, kal::process::one_string_flag.bits };
            const int e = kal_process_spawn(&how, "x", 1, argv, lens, 1,
                                            nullptr, nullptr, 0, nullptr, &p);
            observe(kind::behaviour, e == kal_err_not_supported,
                    "a joined command line where programs receive vectors is refused");
        } else {
            unobserved(kind::behaviour,
                       "a joined command line where programs receive vectors is refused",
                       "the implementation claims prop_one_string");
        }

        // THE NULL DEVICE AT A POSITION (0.22, clause 7.15). A copy given it at
        // all three positions reads the end of input and has every write
        // accepted; a flag beside a stream at its position is invalid; an
        // implementation that does not claim the flags refuses them.
        {
            const kal_uintptr all = (kal::process::null_in_flag | kal::process::null_out_flag
                                     | kal::process::null_err_flag).bits;
            if (kal::process::has(kal::process::null_streams)) {
                const kal_spawn_streams inherit{ {0}, {0}, {0} };
                int spawned = -1, status = -1, terminated = -1;
                const bool ran = start_copy_with(argument_for(errand::read_and_write_null), all,
                                                 inherit, spawned, status, terminated);
                observe(kind::behaviour, ran && spawned == kal_ok && terminated == 0 && status == 60,
                        "a position given the null device reads the end of input and accepts every write");
                kal_stream mine{}, theirs{};
                if (kal::process::has(kal::process::channel)
                    && kal_process_channel(&mine, &theirs) == kal_ok) {
                    const kal_spawn_streams placed{ {0}, theirs, {0} };
                    spawned = -1;
                    start_copy_with(argument_for(errand::exit_with_33),
                                    kal::process::null_out_flag.bits, placed, spawned, status, terminated);
                    kal_process_channel_close(theirs);
                    kal_process_channel_close(mine);
                    observe(kind::behaviour, spawned == kal_err_invalid,
                            "the null device beside a stream at one position is invalid");
                } else {
                    unobserved(kind::behaviour, "the null device beside a stream at one position is invalid",
                               "the implementation provides no channel");
                }
            } else {
                kal_process p{};
                const char* argv[1] = { "x" };
                const kal_uintptr lens[1] = { 1 };
                const kal_spawn how{ kal::fs::working(), kal::fs::working(), nullptr,
                                     nullptr, 0, all };
                const int e = kal_process_spawn(&how, "x", 1, argv, lens, 1,
                                                nullptr, nullptr, 0, nullptr, &p);
                observe(kind::behaviour, e == kal_err_not_supported,
                        "the null device this implementation cannot give is refused, not ignored");
            }
        }

        // THE UNIT IS ASKED FOR BY A POINTER AND NOT BY A FLAG, so an
        // implementation that cannot form one refuses a NON-NULL `job' --- there
        // is no bit to set and none to test.
        if ((kal_process_props() & kal::process::job.bits) == 0) {
            kal_process p{};
            kal_job unit{};
            const char* argv[1] = { "x" };
            const kal_uintptr lens[1] = { 1 };
            const kal_spawn how{ kal::fs::working(), kal::fs::working(), &unit,
                                 nullptr, 0, 0 };
            const int e = kal_process_spawn(&how, "x", 1, argv, lens, 1,
                                            nullptr, nullptr, 0, nullptr, &p);
            observe(kind::behaviour, e == kal_err_not_supported,
                    "a unit this implementation cannot form is refused, not ignored");
            observe(kind::behaviour, unit.h == 0,
                    "and the caller's unit handle is left as it was");
        } else {
            unobserved(kind::behaviour,
                       "a unit this implementation cannot form is refused, not ignored",
                       "the implementation claims prop_job");
        }

        // A GRANT IS OBSERVED BY ITS EFFECT, WHICH IS VISIBLE TO AN openkal
        // PROGRAM: the directories a started program receives are the ones it
        // reads back through `kal_fs_preopen'. The copy is granted two
        // directories, each holding a marker of its own, and reports whether it
        // enumerates exactly those, in order, under the names given. A count of
        // zero is a request for none, which is different from not asking.
        //
        // An implementation that does not claim the position refuses a grant
        // rather than starting a program without it (clause 6.2). Version 0.14.1
        // added both observations, because two implementations claimed the
        // position for three releases while the program they started read back
        // the directories it would have had anyway.
        if (kal::process::has(kal::process::grant_dir)) {
            const char* dirs[2] = { "okc-grant-a.tmp", "okc-grant-b.tmp" };
            kal_preopen grants[2]{};
            bool made = true;
            for (int i = 0; i < 2; ++i) {
                kal_uintptr n = 0; while (dirs[i][n]) ++n;
                kal_fs_mkdir(kal::fs::working(), dirs[i], n);
                kal_dir d{};
                kal_file f{};
                made = made && kal_fs_open_dir(kal::fs::working(), dirs[i], n, &d) == kal_ok
                     && kal::fs::open_file(d, grant_marker_file, sizeof grant_marker_file - 1,
                                           kal::fs::open::write | kal::fs::open::create
                                               | kal::fs::open::truncate, &f) == kal_ok
                     && kal_stream_write(kal_fs_stream(f), grant_markers[i], 1) == 1;
                if (f.h) kal_fs_close_file(f);
                kal_uintptr nl = 0; while (grant_names[i][nl]) ++nl;
                grants[i] = kal_preopen{ d, grant_names[i], nl };
            }
            // A start that a claimed grant prevents is itself the failure: the
            // copy is started from the directory that holds it, and placing a
            // grant over that directory is one of the ways this went wrong.
            int status = -1, terminated = -1;
            if (made) {
                const bool started = start_copy_granting(argument_for(errand::report_grants),
                                                         grants, 2, status, terminated);
                observe(kind::behaviour, started && terminated == 0 && status == 0,
                        "granted directories are the started program's preopens, in order and by name");
            } else {
                unobserved(kind::behaviour, "granted directories are the started program's preopens",
                           "the directories to grant could not be made");
            }
            const bool started = start_copy_granting(argument_for(errand::report_no_preopens),
                                                     grants, 0, status, terminated);
            observe(kind::behaviour, started && terminated == 0 && status == 0,
                    "a count of zero starts a program with no preopens");
            for (int i = 0; i < 2; ++i) {
                kal_fs_remove(grants[i].dir, grant_marker_file, sizeof grant_marker_file - 1);
                if (grants[i].dir.h) kal_fs_close_dir(grants[i].dir);
                kal_uintptr n = 0; while (dirs[i][n]) ++n;
                kal_fs_remove(kal::fs::working(), dirs[i], n);
            }
        } else {
            kal_process p{};
            const char* argv[1] = { "x" };
            const kal_uintptr lens[1] = { 1 };
            const kal_preopen grant{ kal::fs::working(), "x", 1 };
            const kal_spawn how{ kal::fs::working(), kal::fs::working(), nullptr,
                                 &grant, 1, 0 };
            const int e = kal_process_spawn(&how, "x", 1, argv, lens, 1,
                                            nullptr, nullptr, 0, nullptr, &p);
            observe(kind::behaviour, e == kal_err_not_supported,
                    "a grant this implementation cannot convey is refused, not ignored");
        }
    }

    // THE RESOURCE PREFERENCE (0.23): the started program reads what it was
    // started with, the caller's where nothing was said, and two at once are
    // refused.
    {
        const kal_spawn_streams inherit{ {0}, {0}, {0} };
        auto reported = [&](kal_uintptr flags, int& spawned) {
            int status = -1, terminated = -1;
            spawned = -1;
            const bool ran = start_copy_with(argument_for(errand::report_resources), flags, inherit,
                                             spawned, status, terminated);
            return ran && spawned == kal_ok && terminated == 0 ? status - 40 : -1;
        };
        int spawned = -1;
        observe(kind::behaviour, reported(kal::process::economy_flag.bits, spawned) == 2,
                "a program started for economy reads economy");
        observe(kind::behaviour, reported(kal::process::performance_flag.bits, spawned) == 1,
                "a program started for performance reads performance");
        observe(kind::behaviour, reported(kal::process::balanced_flag.bits, spawned) == 0,
                "a program started balanced reads balanced");
        observe(kind::behaviour, reported(0, spawned) == kal_env_resources(),
                "a program started without a preference has its caller's");
        reported((kal::process::economy_flag | kal::process::performance_flag).bits, spawned);
        observe(kind::behaviour, spawned == kal_err_invalid, "two preferences at once are refused");
    }

    // CONFINEMENT (0.23): refused where the environment cannot hold a program
    // to its grants; where it can, a program granted a read-only directory and
    // confined cannot create a file in it.
    {
        const kal_spawn_streams inherit{ {0}, {0}, {0} };
        int spawned = -1, status = -1, terminated = -1;
        if (!kal::process::has(kal::process::confine)) {
            start_copy_with(argument_for(errand::exit_with_33), kal::process::confine_flag.bits, inherit,
                            spawned, status, terminated);
            observe(kind::behaviour, spawned == kal_err_not_supported,
                    "confinement this environment cannot give is refused, not ignored");
        } else if ((kal_fs_props(kal::fs::working()) & kal::fs::attenuate.bits) != 0) {
            // Every directory this program was given, read-only, the working
            // one first: a program of the environment reads what it needs to
            // start --- its loader, its libraries --- and writes nowhere.
            kal_dir_rights r{};
            r.self_size = sizeof r;
            r.flags = kal::fs::dir_read_only.bits;
            kal_preopen grants[16];
            char names[16][256];
            kal_uintptr n = 0;
            const kal_uintptr count = kal_fs_preopen_count();
            for (kal_uintptr i = 0; i < count && n < 16; ++i) {
                kal_dir d{}; kal_uintptr len = 0;
                if (kal_fs_preopen(i, &d, names[n], sizeof names[n], &len) != kal_ok || len > sizeof names[n]) continue;
                kal_dir ro{};
                if (kal_fs_dir_attenuate(d, &r, &ro) != kal_ok) continue;
                grants[n] = kal_preopen{ ro, names[n], len };
                ++n;
            }
            if (n > 0) {
                const bool ran = start_copy_granting_with(argument_for(errand::write_in_first_grant), grants, n,
                                                          kal::process::confine_flag.bits, inherit,
                                                          spawned, status, terminated);
                observe(kind::behaviour, ran && spawned == kal_ok && terminated == 0 && status == 70,
                        "a confined program granted read-only directories cannot create a file in them");
            }
            for (kal_uintptr i = 0; i < n; ++i) kal_fs_close_dir(grants[i].dir);
        } else {
            unobserved(kind::behaviour, "a confined program granted read-only directories cannot create a file in them",
                       "the volume does not attenuate");
        }
    }

    // PAGE FAULTS (0.23): a program that touches pages it had not is counted
    // for them.
    {
        kal_process self{};
        if (kal_process_self(&self) == kal_ok) {
            kal_program_info before{}, after{};
            before.self_size = after.self_size = sizeof before;
            const int a = kal_process_info(self, kal::process::info::faults, &before);
            constexpr kal_uintptr pages = 256, page = 4096;
            auto* p = static_cast<volatile unsigned char*>(kal_alloc(pages * page, page));
            if (p) for (kal_uintptr i = 0; i < pages; ++i) p[i * page] = 1;
            const int b = kal_process_info(self, kal::process::info::faults, &after);
            if (a == kal_ok && b == kal_ok && (before.present & kal::process::info::faults)) {
                observe(kind::behaviour, after.faults_minor >= before.faults_minor + pages / 8,
                        "a program is counted for the pages it touches");
            } else {
                unobserved(kind::behaviour, "a program is counted for the pages it touches",
                           "the implementation does not report KAL_PINFO_FAULTS");
            }
            if (p) kal_free(const_cast<unsigned char*>(p), pages * page, page);
            kal_process_close(self);
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
