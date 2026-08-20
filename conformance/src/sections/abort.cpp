module okc.abort;

import openkal.types;
import openkal.abort;
import openkal.process;
import openkal.fs;
import openkal.env;
import okc.report;
import okc.spec;
import okc.child;

// Termination cannot be observed by the program that performs it, so it is
// observed in a started copy. That requires three interfaces beyond the one
// under examination --- one to start the copy, one to name the program, and one
// to tell the copy what it is --- and an arrangement that selected none of them
// reports that rather than reporting nothing.
namespace okc::termination {
namespace detail {

#if defined(MCPP_FEATURE_PROCESS) && defined(MCPP_FEATURE_ENV)

namespace {

kal_uintptr length(const char* s) { kal_uintptr n = 0; while (s && s[n]) ++n; return n; }

// The name this program was started by, expressed the way openkal names things:
// a directory the environment supplied, and a remainder relative to it.
//
// A program's own name is the one path every program has and the one openkal
// gives it no operation to resolve --- deliberately, because resolving a global
// name is work a C library performs once against the supplied directories.
// Here it is performed by hand, and the length of the doing is the measurement.
bool locate_self(kal_dir& base, const char*& rel, kal_uintptr& rel_len) {
    kal_uintptr len = 0;
    const char* argv0 = kal_env_arg(0, &len);
    if (!argv0 || len == 0) return false;

    if (argv0[0] != '/') {
        // Relative already: it names the program from the directory the program
        // was started in, which is the first directory the environment supplied.
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

// Starts a copy with one argument and reports how it ended.
bool start_and_wait(const char* errand_argument, int& status, int& terminated)
{
    kal_dir base{}; const char* rel = nullptr; kal_uintptr rel_len = 0;
    if (!locate_self(base, rel, rel_len)) return false;

    const char* argv[2] = { "openkal-conformance-child", errand_argument };
    const kal_uintptr lens[2] = { length(argv[0]), length(argv[1]) };

    kal_process child{};
    const kal_spawn_streams streams{ 0, 0, 0 };
    if (kal_process_spawn(base, rel, rel_len, argv, lens, 2, nullptr, nullptr, 0,
                          &streams, &child) != kal_ok) return false;
    const int e = kal_process_wait(child, &status, &terminated);
    kal_process_close(child);
    return e == kal_ok;
}

}  // namespace

#endif

}  // namespace detail

using namespace detail;

void run() {
    heading("openkal.abort");
#ifndef MCPP_FEATURE_CORE
    unobserved(kind::behaviour, "openkal.abort", "the core set was not selected");
    return;
#elif !defined(MCPP_FEATURE_PROCESS) || !defined(MCPP_FEATURE_ENV)
    unobserved(kind::behaviour, "kal_exit terminates with the status it was given",
               "termination ends the observing program, so it is observed in a started copy; "
               "that needs openkal.process to start one and openkal.env to tell it what to do");
    unobserved(kind::behaviour, "kal_exit runs no static destructor",
               "the same");
    unobserved(kind::behaviour, "kal_abort does not return",
               "the same");
#else
    int status = 0, terminated = 0;

    if (start_and_wait(argument_for(errand::exit_with_33), status, terminated)) {
        observe(kind::behaviour, terminated == 0 && status == 33,
                "kal_exit terminates with the status it was given");
    } else {
        unobserved(kind::behaviour, "kal_exit terminates with the status it was given",
                   "a copy of this program could not be started; openkal.process reported a failure "
                   "or the program's own name did not resolve against a supplied directory");
    }

    // Clause 7.8: registered exit handlers and static destructors shall not
    // run. The copy arms a destructor that would announce itself, and the
    // observation is that the copy still reported the status: an
    // implementation that ended through a C library's exit would run the
    // destructor first, and the destructor writes to the copy's output.
    if (start_and_wait(argument_for(errand::exit_after_writing), status, terminated)) {
        observe(kind::behaviour, terminated == 0 && status == 34,
                "kal_exit terminates immediately, and the status survives");
        line("  (the copy's output above shows BEFORE EXIT and shall not show DESTRUCTOR RAN)");
    } else {
        unobserved(kind::behaviour, "kal_exit runs no static destructor",
                   "a copy of this program could not be started");
    }

    // kal_abort does not return, and what it does instead is the environment's
    // to decide: the specification requires only that the program stop after
    // the message. A status of 33 or 34 would mean it returned into the copy's
    // ordinary path, which is the thing being excluded.
    if (start_and_wait(argument_for(errand::abort_with_message), status, terminated)) {
        observe(kind::behaviour, terminated != 0 || (status != 33 && status != 34 && status != 0),
                "kal_abort ends the program rather than returning");
    } else {
        unobserved(kind::behaviour, "kal_abort does not return",
                   "a copy of this program could not be started");
    }
#endif
}

}  // namespace okc::termination
