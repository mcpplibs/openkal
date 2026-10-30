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
import openkal.map;

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
        case errand::report_grants:      return "--child-grants";
        case errand::report_no_preopens: return "--child-no-preopens";
        case errand::end_as_requested:   return "--child-end-as-requested";
        case errand::map_beyond_shortened: return "--child-map-beyond-shortened";
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
        if (same(a, len, argument_for(errand::report_grants)))      return errand::report_grants;
        if (same(a, len, argument_for(errand::report_no_preopens))) return errand::report_no_preopens;
        if (same(a, len, argument_for(errand::end_as_requested)))   return errand::end_as_requested;
        if (same(a, len, argument_for(errand::map_beyond_shortened))) return errand::map_beyond_shortened;
    }
#endif
    return errand::none;
}

namespace {
// Something the specification requires shall NOT run after kal_exit. A static
// object whose destructor writes is the shortest thing that would betray an
// implementation calling a C library's exit instead of terminating.
//
// GUARDED ON `openkal.process`, WHICH IS THE INTERFACE THAT MAKES THE
// OBSERVATION RATHER THAN THE ONE THE OBJECT USES.
//
// The object is armed only by a copy this program starts, and only
// `okc.abort`'s section starts one. Where `openkal.process` is absent no copy
// is ever started, the object can never be armed, and its destructor is
// unreachable — while still costing the program a `__cxa_atexit`, which is a
// facility of the C++ ABI rather than of openkal.
//
// Measured 2026-08-23, building this suite for `riscv64-none-elf` against
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
        case errand::end_as_requested:
            // The word is taken over, awaited for two seconds, and the copy then
            // ends as the request would have. 98: no request arrived as one ---
            // the environment ended this copy outright, or never asked. 99: the
            // operation returned although a request had arrived.
#if defined(MCPP_FEATURE_PROCESS) && defined(MCPP_FEATURE_TIME)
            if (const kal_u32* word = kal_process_stop_requested()) {
                kal_stream_write(kal_stdout(), "ARMED\n", 6);
                for (int i = 0; i < 400 && *word == 0; ++i) kal_time_sleep(5u * 1000u * 1000u);
                if (*word == 0) kal_exit(98);
                kal_process_end_as_requested();
                kal_exit(99);
            }
#endif
            kal_exit(98);
        case errand::wait_to_be_terminated:
#ifdef MCPP_FEATURE_TIME
            for (;;) kal_time_sleep(5u * 1000u * 1000u);
#else
            for (;;) { }
#endif
        case errand::report_grants:
            // The copy reports through its status which part disagreed: the
            // number of preopens, a name, or the directory behind a name.
#ifdef MCPP_FEATURE_FS
            {
                if (kal_fs_preopen_count() != 2) kal_exit(40);
                for (kal_uintptr i = 0; i < 2; ++i) {
                    kal_dir d{}; char name[64]; kal_uintptr nlen = 0;
                    if (kal_fs_preopen(i, &d, name, sizeof name, &nlen) != kal_ok) kal_exit(41);
                    if (!same(name, nlen, grant_names[i])) kal_exit(42);
                    kal_file f{}; char buf[8];
                    if (kal_fs_open(d, grant_marker_file, sizeof grant_marker_file - 1,
                                    kal::fs::open::read.bits, &f) != kal_ok) kal_exit(43);
                    const kal_intptr r = kal_stream_read(kal_fs_stream(f), buf, sizeof buf);
                    kal_fs_close_file(f);
                    if (r < 0 || !same(buf, static_cast<kal_uintptr>(r), grant_markers[i])) kal_exit(44);
                }
                kal_exit(0);
            }
#endif
            kal_exit(45);
        case errand::map_beyond_shortened:
            // A file of three alignments, every byte 0xA5, is mapped whole as a
            // SHARED region and shortened to 100 bytes (a private region may keep
            // what it showed, as it may for any change to its file). 50: the
            // shortening was refused. Otherwise the bytes after the new end in its
            // page are read (51 if one is not zero) and then a page wholly beyond
            // it: the copy ends with a fault, or reads zero (52), or reads what was
            // removed (55, which the specification forbids). 100 + the error: the
            // file could not be made (56: written; 57: mapped).
#if defined(MCPP_FEATURE_MAP) && defined(MCPP_FEATURE_FS)
            {
                const char name[] = "okc-conformance-map-short.tmp";
                kal_file f{};
                const auto flags = kal::fs::open::read | kal::fs::open::write
                                 | kal::fs::open::create | kal::fs::open::truncate;
                if (const int oe = kal_fs_open(kal::fs::working(), name, sizeof name - 1, flags.bits, &f); oe != kal_ok) kal_exit(100 + oe);
                const kal_uintptr size = 3 * static_cast<kal_uintptr>(kal::map::alignment);
                unsigned char block[4096];
                for (unsigned char& b : block) b = 0xA5;
                for (kal_uintptr at = 0; at < size; at += sizeof block)
                    if (kal_stream_write(kal_fs_stream(f), block, sizeof block) != static_cast<kal_intptr>(sizeof block)) kal_exit(56);
                void* region = nullptr;
                if (kal_map_file(f, 0, size, kal::map::shared.bits, &region) != kal_ok) kal_exit(57);
                const int e = kal_fs_truncate(f, 100);
                if (e != kal_ok) kal_exit(e == kal_err_permission ? 50 : 54);
                volatile const unsigned char* bytes = static_cast<const unsigned char*>(region);
                for (kal_uintptr i = 100; i < 4096; ++i) if (bytes[i] != 0) kal_exit(51);
                const unsigned char beyond = bytes[2 * static_cast<kal_uintptr>(kal::map::alignment)];
                kal_exit(beyond == 0 ? 52 : 55);
            }
#endif
            kal_exit(45);
        case errand::report_no_preopens:
#ifdef MCPP_FEATURE_FS
            kal_exit(kal_fs_preopen_count() == 0 ? 0 : 46);
#endif
            kal_exit(45);
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
    const kal_spawn         how{ base, base, nullptr, nullptr, 0, 0 };
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

bool start_copy_granting(const char* errand_argument, const kal_preopen* grants,
                         kal_uintptr count, int& status, int& terminated) {
    kal_dir base{}; const char* rel = nullptr; kal_uintptr rel_len = 0;
    if (!locate_self(base, rel, rel_len)) return false;

    const char* argv[2] = { "openkal-conformance-child", errand_argument };
    kal_uintptr lens[2];
    for (int i = 0; i < 2; ++i) { kal_uintptr n = 0; while (argv[i][n]) ++n; lens[i] = n; }

    const kal_spawn_streams streams{ 0, 0, 0 };
    const kal_spawn         how{ base, base, nullptr, grants, count, 0 };
    kal_process child{};
    if (kal_process_spawn(&how, rel, rel_len, argv, lens, 2, nullptr, nullptr, 0,
                          &streams, &child) != kal_ok)
        return false;
    const int e = kal_process_wait(child, &status, &terminated);
    kal_process_close(child);
    return e == kal_ok;
}

#else

bool start_copy(const char*, const char*, int&, int&) { return false; }
bool start_copy_running(const char*, const char*, kal_process&) { return false; }
bool start_copy_granting(const char*, const kal_preopen*, kal_uintptr, int&, int&) { return false; }

#endif

}  // namespace okc
