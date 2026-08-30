module okc.child;

// Every openkal module is imported unconditionally, in this and in every other
// section. An import creates no link-time dependency of its own --- what
// creates one is a call --- so the conditionals that select what is examined
// guard the calls and never the imports. They could not guard the imports in
// any case: an import inside a conditional block is refused by the build, and
// refused for a good reason, since a module graph that depends on the
// preprocessor is a module graph that cannot be scanned.
import openkal.types;
import openkal.abort;
import openkal.env;
import openkal.stream;
import openkal.time;
import openkal.fs;
import openkal.process;

namespace okc {
namespace {

bool same(const char* a, kal_uintptr alen, const char* b) {
    kal_uintptr n = 0; while (b[n]) ++n;
    if (n != alen) return false;
    for (kal_uintptr i = 0; i < n; ++i) if (a[i] != b[i]) return false;
    return true;
}

}  // namespace

const char* argument_for(errand e) {
    switch (e) {
        case errand::exit_with_33:       return "--child-exit";
        case errand::exit_after_writing: return "--child-exit-after-writing";
        case errand::abort_with_message: return "--child-abort";
        case errand::wait_to_be_terminated: return "--child-wait";
        case errand::none:               break;
    }
    return "";
}

errand child_errand() {
#ifdef MCPP_FEATURE_ENV
    for (kal_uintptr i = 1; i < kal_env_arg_count(); ++i) {
        char abuf[1024];
        const kal_intptr alen = kal_env_arg(i, abuf, sizeof abuf);
        if (alen < 0 || static_cast<kal_uintptr>(alen) >= sizeof abuf) continue;
        const kal_uintptr len = static_cast<kal_uintptr>(alen);
        const char* a = abuf;
        if (same(a, len, argument_for(errand::exit_with_33)))       return errand::exit_with_33;
        if (same(a, len, argument_for(errand::exit_after_writing))) return errand::exit_after_writing;
        if (same(a, len, argument_for(errand::abort_with_message))) return errand::abort_with_message;
        if (same(a, len, argument_for(errand::wait_to_be_terminated))) return errand::wait_to_be_terminated;
    }
#endif
    return errand::none;
}

namespace {
// Something the specification requires shall NOT run after kal_exit. A static
// object whose destructor writes is the shortest thing that would betray an
// implementation calling a C library's exit instead of terminating.
//
// ⚠️ GUARDED ON `openkal.process`, WHICH IS THE INTERFACE THAT MAKES THE
// OBSERVATION RATHER THAN THE ONE THE OBJECT USES.
//
// The object is armed only by a copy this program starts, and only
// `okc.abort`'s section starts one. Where `openkal.process` is absent no copy
// is ever started, the object can never be armed, and its destructor is
// unreachable — while still costing the program a `__cxa_atexit`, which is a
// facility of the C++ ABI rather than of openkal.
//
// ⚠️ Measured 2026-08-23, building this suite for `riscv64-none-elf` against
// `openkal-opensbi` — the only thing standing between the suite and a machine
// with no operating system:
//
//     ld.lld: error: undefined symbol: __cxa_atexit
//     >>> referenced by child.cpp:36 … obj/child.o:(__cxx_global_var_init)
//
// ⇒ The observation is not lost by this guard. It is already conditional on a
// copy being startable, and `okc.abort` reports it as not observed when one
// cannot be — which is the same answer, arrived at by the interface inventory
// rather than by a link failure.
#ifdef MCPP_FEATURE_PROCESS
struct after {
    bool armed = false;
    ~after() {
#ifdef MCPP_FEATURE_CORE
        if (armed) kal_stream_write(kal_stdout(), "DESTRUCTOR RAN\n", 15);
#endif
    }
};
after g_after;
#endif
}  // namespace

[[noreturn]] void perform(errand e) {
    switch (e) {
        case errand::exit_with_33:
            // Clause 7.6: the first element of the vector is the name the
            // started program observes as its own, passed unaltered. The copy
            // reports which one it saw through its status, because that is the
            // only channel openkal.process defines --- and reporting it is what
            // makes the parent's observation an observation rather than an
            // assumption.
#ifdef MCPP_FEATURE_ENV
            {
                kal_uintptr len = 0;
                char sbuf[1024];
                const kal_intptr slen = kal_env_arg(0, sbuf, sizeof sbuf);
                const char* self = (slen >= 0 && static_cast<kal_uintptr>(slen) < sizeof sbuf)
                                 ? (len = static_cast<kal_uintptr>(slen), sbuf) : nullptr;
                if (!self || !same(self, len, "openkal-conformance-child")) kal_exit(36);
            }
#endif
            kal_exit(33);
        case errand::exit_after_writing:
            // The status is 34, and the parent additionally requires that this
            // program's output contain the written bytes and not the
            // destructor's: an implementation that terminated by way of a C
            // library's exit would run the destructor, and clause 7.8 says it
            // shall not.
#ifdef MCPP_FEATURE_PROCESS
            g_after.armed = true;
#endif
#ifdef MCPP_FEATURE_CORE
            kal_stream_write(kal_stdout(), "BEFORE EXIT\n", 12);
#endif
            kal_exit(34);
        case errand::abort_with_message:
            kal_abort("openkal-conformance: the message kal_abort was given\n", 52);
        case errand::wait_to_be_terminated:
#ifdef MCPP_FEATURE_TIME
            for (;;) kal_time_sleep(5u * 1000u * 1000u);
#else
            for (;;) { }
#endif
        case errand::none:
            break;
    }
    kal_exit(35);
}

// --- where this program is ---------------------------------------------------

#if defined(MCPP_FEATURE_ENV) && defined(MCPP_FEATURE_FS)

namespace {

bool is_separator(char c) { return c == '/' || c == '\\'; }

// Whether `name' is a prefix of `whole' that ends at a component boundary.
// A supplied directory whose own name ends in a separator --- a volume, on an
// environment that names volumes --- matches without requiring another.
bool prefix_at_boundary(const char* name, kal_uintptr nlen,
                        const char* whole, kal_uintptr wlen) {
    if (nlen == 0 || nlen > wlen) return false;
    for (kal_uintptr i = 0; i < nlen; ++i) if (name[i] != whole[i]) return false;
    if (is_separator(name[nlen - 1])) return true;
    return nlen == wlen || is_separator(whole[nlen]);
}

}  // namespace

bool locate_self(kal_dir& base, const char*& relative, kal_uintptr& relative_len) {
    kal_uintptr len = 0;
    static char a0[1024];
    const kal_intptr a0len = kal_env_arg(0, a0, sizeof a0);
    const char* argv0 = (a0len >= 0 && static_cast<kal_uintptr>(a0len) < sizeof a0)
                      ? (len = static_cast<kal_uintptr>(a0len), a0) : nullptr;
    if (!argv0 || len == 0) return false;

    kal_uintptr best = 0, best_len = 0;
    bool found = false;
    for (kal_uintptr i = 0; i < kal_fs_preopen_count(); ++i) {
        kal_dir d{}; char name[1024]; kal_uintptr nlen = 0;
        if (kal_fs_preopen(i, &d, name, sizeof name, &nlen) != kal_ok) continue;
        if (nlen >= sizeof name) continue;
        if (!prefix_at_boundary(name, nlen, argv0, len)) continue;
        if (!found || nlen > best_len) { found = true; best = i; best_len = nlen; }
    }

    if (!found) {
        // No supplied directory names it, so the name is already relative to
        // the directory the program was started in --- which is the first
        // directory the environment supplied.
        base = kal::fs::working();
        relative = argv0; relative_len = len;
        return relative_len > 0;
    }

    kal_dir d{}; kal_uintptr nlen = 0;
    kal_fs_preopen(best, &d, nullptr, 0, &nlen);
    base = d;
    kal_uintptr at = best_len;
    while (at < len && is_separator(argv0[at])) ++at;
    relative = argv0 + at;
    relative_len = len - at;
    return relative_len > 0;
}

#else

bool locate_self(kal_dir&, const char*&, kal_uintptr&) { return false; }

#endif

// --- starting a copy ---------------------------------------------------------

#if defined(MCPP_FEATURE_ENV) && defined(MCPP_FEATURE_FS) && defined(MCPP_FEATURE_PROCESS)

bool start_copy_running(const char* first_element, const char* errand_argument,
                        kal_process& out) {
    kal_dir base{}; const char* rel = nullptr; kal_uintptr rel_len = 0;
    if (!locate_self(base, rel, rel_len)) return false;

    const char* argv[2] = { first_element, errand_argument };
    kal_uintptr lens[2];
    for (int i = 0; i < 2; ++i) { kal_uintptr n = 0; while (argv[i][n]) ++n; lens[i] = n; }

    const kal_spawn_streams streams{ 0, 0, 0 };
    const kal_spawn         how{ base, base, nullptr, 0, 0 };
    return kal_process_spawn(&how, rel, rel_len, argv, lens, 2, nullptr, nullptr, 0,
                             &streams, &out) == kal_ok;
}

bool start_copy(const char* first_element, const char* errand_argument,
                int& status, int& terminated) {
    kal_process child{};
    if (!start_copy_running(first_element, errand_argument, child)) return false;
    const int e = kal_process_wait(child, &status, &terminated);
    kal_process_close(child);
    return e == kal_ok;
}

#else

bool start_copy(const char*, const char*, int&, int&) { return false; }
bool start_copy_running(const char*, const char*, kal_process&) { return false; }

#endif

}  // namespace okc
