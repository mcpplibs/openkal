// One program above the eight interfaces. It names no implementation, no
// operating system, no descriptor number and no system call.
//
// Every implementation builds this same source and asserts the same lines. The
// source is fetched from the specification repository rather than copied into
// each implementation, so the programs cannot diverge.
//
// ⚠️⚠️ NAMING NO SYSTEM CALL IS NOT THE SAME AS NAMING NO COMPILER.
//
// This file used `__atomic_load_n`, `__ATOMIC_RELAXED` and `__UINT32_TYPE__`,
// which are GCC and Clang spellings. MSVC has none of them, so the program
// whose whole premise is portability did not compile on one of the three
// compilers its own specification is tested against --- and nothing noticed,
// because until 2026-08-27 no continuous integration anywhere built this file.
//
// The integer types are now the specification's own: openkal/types.h already
// carries the MSVC branch, so `kal_u32` and `kal_u64` are the portable
// spellings and using them keeps this program inside the vocabulary it exists
// to demonstrate.
//
// The atomics are the LANGUAGE's. `std::atomic_ref` is freestanding in C++23,
// so depending on it does not contradict what this program shows: suspension
// is a kernel facility and openkal offers it, while an atomic operation is a
// language facility and openkal is right not to.
#include <atomic>

import openkal.types;
import openkal.stream;
import openkal.memory;
import openkal.env;
import openkal.time;
import openkal.fs;
import openkal.process;
import openkal.task;

// --- Output, written through the interface under test -----------------------

static kal_uintptr length(const char* s) { kal_uintptr n = 0; while (s[n]) ++n; return n; }

static void say(const char* s) {
    kal_uintptr n = length(s);
    kal_uintptr done = 0;
    while (done < n) {
        // The count, or a negated condition. A partial write is not a failure
        // and is why this loops; a count of zero would loop forever, so it
        // ends the attempt.
        const kal_intptr r = kal_stream_write(kal_stdout(), s + done, n - done);
        if (r <= 0) return;
        done += (kal_uintptr)r;
    }
}

static void say_unsigned(unsigned long long v) {
    char buf[24];
    int i = 24;
    if (v == 0) buf[--i] = '0';
    while (v) { buf[--i] = char('0' + (v % 10)); v /= 10; }
    kal_stream_write(kal_stdout(), buf + i, kal_uintptr(24 - i));
}

static int failures = 0;

static void observe(bool held, const char* what) {
    say(held ? "openkal: " : "openkal: NOT HELD: ");
    say(what);
    say("\n");
    if (!held) ++failures;
}

// --- The contended counter, to exercise the suspension primitive ------------
//
// A mutex is not part of the specification. It is constructed here from the
// primitive that is, which is the relation a C library has to a kernel.

static kal_u32 lock_state = 0;           // 0 free, 1 held, 2 held and contended
static unsigned long long counter  = 0;

// The word is waited upon through openkal and modified through the language, so
// both must see the same object. `atomic_ref` refers to it rather than
// replacing it, which is what lets `kal_task_wait` take its address.
static bool exchange(kal_u32* p, kal_u32 expected, kal_u32 desired) {
    return std::atomic_ref<kal_u32>(*p).compare_exchange_strong(
        expected, desired, std::memory_order_acquire, std::memory_order_acquire);
}

static void acquire() {
    if (exchange(&lock_state, 0, 1)) return;
    do {
        const kal_u32 seen =
            std::atomic_ref<kal_u32>(lock_state).load(std::memory_order_relaxed);
        if (seen == 2 || exchange(&lock_state, 1, 2))
            kal_task_wait(&lock_state, 2, 0);
    } while (!exchange(&lock_state, 0, 2));
}

static void release() {
    if (std::atomic_ref<kal_u32>(lock_state)
            .fetch_sub(1, std::memory_order_release) != 1) {
        std::atomic_ref<kal_u32>(lock_state).store(0, std::memory_order_release);
        kal_uintptr woken = 0;
        kal_task_wake(&lock_state, 1, &woken);
    }
}

static void bump(void*) {
    for (int i = 0; i < 20000; ++i) { acquire(); ++counter; release(); }
}

// --- The observations -------------------------------------------------------

int main() {
    say("openkal: the portable program, above eight interfaces\n");

    // openkal.stream — the writes above have already exercised it; the
    // observation is that the interface reports its outcome rather than
    // returning a count that must be interpreted.
    {
        const kal_intptr r = kal_stream_write(kal_stdout(), "", 0);
        observe(r == 0, "a stream reports the count it moved");
    }

    // openkal.memory
    {
        void* p = kal_alloc(4096, 64);
        bool aligned = p != nullptr && (kal_uintptr(p) % 64) == 0;
        if (p) { *static_cast<unsigned char*>(p) = 0xA5; aligned = aligned && *static_cast<unsigned char*>(p) == 0xA5; }
        kal_free(p, 4096, 64);
        observe(aligned, "memory is obtained at the alignment requested and released");
    }

    // openkal.env
    {
        char buf[4096];
        bool args = kal_env_arg_count() >= 1;
        // The length reported is the value's, not the buffer's, so a caller may
        // ask with no buffer at all in order to size one.
        const kal_intptr n = kal_env_arg(0, buf, sizeof buf);
        bool named = n > 0 && n == kal_env_arg(0, nullptr, 0);
        // A variable that is absent is reported as absent rather than as empty.
        bool absent = kal_env_var("OPENKAL_A_NAME_NOTHING_SETS", 27, buf, sizeof buf)
                          == -kal_err_not_found;
        observe(args && named && absent, "the environment supplies arguments, and absence differs from emptiness");
    }

    // openkal.time
    {
        kal_duration a = kal_time_monotonic();
        kal_time_sleep(1000000);            // one millisecond
        kal_duration b = kal_time_monotonic();
        bool advanced = b > a;
        bool granular = kal_time_monotonic_granularity() > 0;
        bool wall     = kal_time_wall() > 0;
        observe(advanced && granular && wall, "the monotonic clock advances, reports its granularity, and a wall clock exists");
    }

    // openkal.fs — every operation is relative to a directory the environment
    // supplied, and a name that leaves it is refused rather than followed.
    {
        kal_dir wd{}; char nm[256]; kal_uintptr nl = 0;
        bool got = kal_fs_preopen_count() >= 1
                && kal_fs_preopen(0, &wd, nm, sizeof nm, &nl) == kal_ok;

        kal_file f{};
        const auto rw = kal::fs::open::read | kal::fs::open::write
                      | kal::fs::open::create | kal::fs::open::truncate;
        bool made = got && kal::fs::open_file(wd, "portable.probe", 14, rw, &f) == kal_ok;
        if (made) {
            // The stream of a file is a stream. It is not converted, it is not
            // a number reinterpreted --- the operation's type says so.
            kal_stream s = kal_fs_stream(f);
            kal_stream_write(s, "0123456789", 10);
            kal_fs_close_file(f);
        }

        // The caller states how much of the structure exists on its side, and
        // the implementation reports which fields it filled. Both are what let
        // this program run against an implementation built at another version.
        kal_node_info info = kal::fs::info_for_caller();
        bool sized = made
                  && kal_fs_info(wd, "portable.probe", 14, 0,
                                 kal::fs::field::size, &info) == kal_ok
                  && (info.present & kal::fs::field::size) != 0
                  && info.size == 10;

        // Positioning is a property of the file rather than of the stream,
        // which is why it is declared here and not in openkal.stream.
        kal_file g{};
        bool sought = false;
        if (made && kal::fs::open_file(wd, "portable.probe", 14,
                                       kal::fs::open::read, &g) == kal_ok) {
            kal_u64 at = 0;
            sought = kal_fs_seek(g, 6, kal::fs::seek_set, &at) == kal_ok && at == 6;
            char buf[8] = {};
            const kal_intptr r = kal_stream_read(kal_fs_stream(g), buf, 4);
            sought = sought && r == 4 && buf[0] == '6' && buf[3] == '9';
            kal_fs_close_file(g);
        }

        kal_file escape{};
        bool refused = got && kal::fs::open_file(wd, "../portable.probe", 17,
                                                 kal::fs::open::read, &escape) != kal_ok;

        if (made) kal_fs_remove(wd, "portable.probe", 14);
        // Clause 7.7: enquiry about a name that does not exist succeeds and
        // reports absence. A test that expected an error here would pass
        // against an implementation that conflated enquiry with access.
        info = kal::fs::info_for_caller();
        bool gone = got && kal_fs_info(wd, "portable.probe", 14, 0,
                                       kal::fs::field::kind, &info) == kal_ok
                        && info.kind == kal_node_absent;

        observe(got && made && sized && sought && refused && gone,
                "a file is created, sized, repositioned, removed, and absence is reported as an answer");
    }

    // openkal.task
    {
        kal_task a{}, b{};
        bool started = kal_task_start(bump, nullptr, &a) == kal_ok
                    && kal_task_start(bump, nullptr, &b) == kal_ok;
        bump(nullptr);
        if (started) { kal_task_join(a); kal_task_join(b); }
        observe(started && counter == 60000,
                "three contexts increment a counter 60000 times and none is lost");
    }

    // openkal.process — a program is started relative to a directory, and its
    // status is read. The specification defines starting a program rather than
    // duplicating the caller, because duplication cannot be performed
    // faithfully everywhere.
    //
    // The program started is not this one. A conformance program that started
    // itself would, if the marker distinguishing parent from child failed to
    // arrive, start itself without end — and the marker arriving is one of the
    // things under test. The failure of an observation must not be unbounded.
    {
        // A directory named "/" among those supplied, if the environment
        // supplies one. The specification guarantees only the first entry, so
        // its absence is reported as unobservable rather than as a failure.
        kal_dir root{}; bool haveRoot = false;
        for (kal_uintptr i = 0, n = kal_fs_preopen_count(); i < n && !haveRoot; ++i) {
            kal_dir d{}; char nm[256]; kal_uintptr nl = 0;
            if (kal_fs_preopen(i, &d, nm, sizeof nm, &nl) == kal_ok
                    && nl == 1 && nm[0] == '/') {
                root = d; haveRoot = true;
            }
        }

        if (!haveRoot) {
            say("openkal: not observed: no directory denoting the whole of the name space was supplied\n");
        } else {
            kal_process p{};
            const char*       argv[] = { "sh", "-c", "exit 0" };
            const kal_uintptr lens[] = { 2, 2, 6 };
            bool spawned = kal_process_spawn(root, "bin/sh", 6, argv, lens, 3,
                                             nullptr, nullptr, 0, nullptr, &p) == kal_ok;
            int status = -1, terminated = 1;
            if (spawned) { kal_process_wait(p, &status, &terminated); kal_process_close(p); }

            // And a program that fails is distinguished from one that succeeds,
            // so that a status of zero is evidence rather than a default.
            kal_process q{};
            const char*       argv2[] = { "sh", "-c", "exit 3" };
            bool second = kal_process_spawn(root, "bin/sh", 6, argv2, lens, 3,
                                            nullptr, nullptr, 0, nullptr, &q) == kal_ok;
            int status2 = -1, terminated2 = 1;
            if (second) { kal_process_wait(q, &status2, &terminated2); kal_process_close(q); }

            observe(spawned && status == 0 && terminated == 0
                 && second  && status2 == 3 && terminated2 == 0,
                    "a program is started relative to a directory, awaited, and a failing status is distinguished from a succeeding one");
        }
    }

    // openkal.abort is exercised by not being exercised: a program that reached
    // this line did not abort. Calling it would end the program, so the
    // observation is that the interface exists and the program chose not to.
    observe(true, "the program completed without terminating abnormally");

    say("openkal: observations that did not hold: ");
    say_unsigned((unsigned long long)failures);
    say("\n");
    kal_stream_flush(kal_stdout());
    return failures;
}
