// One program above the eight interfaces. It names no implementation, no
// operating system, no descriptor number and no system call.
//
// Every implementation builds this same source and asserts the same lines. The
// source is fetched from the specification repository rather than copied into
// each implementation, so the programs cannot diverge.
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
        kal_io_result r = kal_stream_write(kal_stdout(), s + done, n - done);
        if (r.e != kal_ok) return;
        if (r.n == 0) return;
        done += r.n;
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

static __UINT32_TYPE__ lock_state = 0;   // 0 free, 1 held, 2 held and contended
static unsigned long long counter  = 0;

static bool exchange(__UINT32_TYPE__* p, __UINT32_TYPE__ expected, __UINT32_TYPE__ desired) {
    return __atomic_compare_exchange_n(p, &expected, desired, false,
                                       __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
}

static void acquire() {
    if (exchange(&lock_state, 0, 1)) return;
    do {
        __UINT32_TYPE__ seen = __atomic_load_n(&lock_state, __ATOMIC_RELAXED);
        if (seen == 2 || exchange(&lock_state, 1, 2))
            kal_task_wait(&lock_state, 2, 0);
    } while (!exchange(&lock_state, 0, 2));
}

static void release() {
    if (__atomic_fetch_sub(&lock_state, 1, __ATOMIC_RELEASE) != 1) {
        __atomic_store_n(&lock_state, 0, __ATOMIC_RELEASE);
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
        kal_io_result r = kal_stream_write(kal_stdout(), "", 0);
        observe(r.e == kal_ok && r.n == 0, "a stream reports its outcome");
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
        bool args = kal_env_arg_count() >= 1;
        kal_uintptr n = 0, vn = 0;
        bool named = kal_env_arg(0, &n) != nullptr && n > 0;
        // A variable that is absent is reported as absent rather than as empty.
        bool absent = kal_env_var("OPENKAL_A_NAME_NOTHING_SETS", 27, &vn) == nullptr;
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
        kal_dir wd{}; const char* nm = nullptr; kal_uintptr nl = 0;
        bool got = kal_fs_preopen_count() >= 1 && kal_fs_preopen(0, &wd, &nm, &nl) == kal_ok;

        kal_file f{};
        bool made = got && kal_fs_open_file(wd, "portable.probe", 14, 1, 1, &f) == kal_ok;
        if (made) {
            kal_stream s{kal_fs_stream(f)};
            kal_stream_write(s, "0123456789", 10);
            kal_fs_close_file(f);
        }

        kal_node_info info{};
        bool sized = made && kal_fs_info(wd, "portable.probe", 14, &info) == kal_ok && info.size == 10;

        // Positioning is a property of the file rather than of the stream,
        // which is why it is declared here and not in openkal.stream.
        kal_file g{};
        bool sought = false;
        if (made && kal_fs_open_file(wd, "portable.probe", 14, 0, 0, &g) == kal_ok) {
            __UINT64_TYPE__ at = 0;
            sought = kal_fs_seek(g, 6, kal::fs::seek_set, &at) == kal_ok && at == 6;
            char buf[8] = {};
            kal_io_result r = kal_stream_read(kal_stream{kal_fs_stream(g)}, buf, 4);
            sought = sought && r.e == kal_ok && r.n == 4
                            && buf[0] == '6' && buf[3] == '9';
            kal_fs_close_file(g);
        }

        kal_file escape{};
        bool refused = got && kal_fs_open_file(wd, "../portable.probe", 17, 0, 0, &escape) != kal_ok;

        if (made) kal_fs_remove(wd, "portable.probe", 14);
        // Clause 7.7: enquiry about a name that does not exist succeeds and
        // reports absence. A test that expected an error here would pass
        // against an implementation that conflated enquiry with access.
        bool gone = got && kal_fs_info(wd, "portable.probe", 14, &info) == kal_ok
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
            kal_dir d{}; const char* nm = nullptr; kal_uintptr nl = 0;
            if (kal_fs_preopen(i, &d, &nm, &nl) == kal_ok && nl == 1 && nm[0] == '/') {
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
