module okc.task;

import openkal.types;
import openkal.task;
import openkal.time;
import okc.report;
import okc.spec;

namespace okc::task {
namespace {

#ifdef MCPP_FEATURE_TASK

// A mutex, built here from the suspension primitive.
//
// openkal has no mutex, deliberately: a mutex is a construction above the
// primitive rather than a facility of the boundary, and an interface offering
// one would oblige an implementation whose environment has no kernel mutex to
// construct one. This is the construction, and its being twenty lines is the
// evidence for the decomposition.
//
// Three states rather than two. A mutex that recorded only "held" and "free"
// would have to wake a waiter on every release, because it could not know
// whether one existed; the third state records that one does.
struct mutex {
    volatile kal_u32 state = 0;   // 0 free, 1 held, 2 held and wanted

    void lock() {
        kal_u32 expected = 0;
        if (__atomic_compare_exchange_n(&state, &expected, 1u, false,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return;
        for (;;) {
            const kal_u32 previous = __atomic_exchange_n(&state, 2u, __ATOMIC_ACQUIRE);
            if (previous == 0) return;
            kal_task_wait(const_cast<const kal_u32*>(&state), 2u, 0);
        }
    }

    void unlock() {
        if (__atomic_exchange_n(&state, 0u, __ATOMIC_RELEASE) == 2u) {
            kal_uintptr woken = 0;
            kal_task_wake(const_cast<const kal_u32*>(&state), 1, &woken);
        }
    }
};

mutex           g_mutex;
volatile long long g_counter = 0;
constexpr int   kContexts   = 4;
constexpr int   kIncrements = 20000;

void increment(void*) {
    for (int i = 0; i < kIncrements; ++i) {
        g_mutex.lock();
        g_counter = g_counter + 1;      // deliberately not atomic: the mutex is under test
        g_mutex.unlock();
    }
}

volatile kal_u32 g_word = 0;
volatile int             g_ran  = 0;
kal_uintptr              g_identity = 0;

void sets_the_word(void*) {
    g_identity = kal_task_current();
    __atomic_store_n(&g_ran, 1, __ATOMIC_RELEASE);
    __atomic_store_n(&g_word, 1u, __ATOMIC_RELEASE);
    kal_uintptr woken = 0;
    kal_task_wake(const_cast<const kal_u32*>(&g_word), 1, &woken);
}

thread_local int g_per_context = 0;
volatile int     g_saw = -1;

void records_its_own(void*) {
    g_per_context = 7;
    __atomic_store_n(&g_saw, g_per_context, __ATOMIC_RELEASE);
}

void does_nothing(void*) { }

#endif

}  // namespace

void run() {
    heading("openkal.task");
#ifndef MCPP_FEATURE_TASK
    unobserved(kind::behaviour, "openkal.task", "the interface was not selected");
    return;
#else
    claim("kal_task_props", kal_task_props);

    // A context runs, and the context that started it can tell that it did.
    {
        kal_task t{};
        const int e = kal_task_start(sets_the_word, nullptr, &t);
        observe(kind::behaviour, e == kal_ok, "an execution context starts");
        if (e == kal_ok) {
            // The suspension primitive. The comparison and the suspension occur
            // without an intervening opportunity for the value to change
            // unobserved, which is what a caller cannot construct for itself
            // and is the whole reason the operation exists. The loop
            // re-examines the condition after waking, because waking is
            // permitted to be spurious.
            while (__atomic_load_n(&g_word, __ATOMIC_ACQUIRE) == 0)
                kal_task_wait(const_cast<const kal_u32*>(&g_word), 0u, 0);
            observe(kind::behaviour, __atomic_load_n(&g_ran, __ATOMIC_ACQUIRE) == 1,
                    "the started context ran");
            observe(kind::behaviour, kal_task_join(t) == kal_ok, "the context is awaited");
            observe(kind::behaviour, g_identity != kal_task_current(),
                    "a started context has an identity distinct from the starting one");
        }
    }

    // The construction the interface exists to support. The count is exact:
    // an increment lost to a race is what an unsound mutex produces, and the
    // sum is the only thing that shows it.
    {
        kal_task t[kContexts]{};
        int started = 0;
        g_counter = 0;
        for (int i = 0; i < kContexts; ++i)
            if (kal_task_start(increment, nullptr, &t[i]) == kal_ok) ++started;
        for (int i = 0; i < started; ++i) kal_task_join(t[i]);
        observe(kind::behaviour, started == kContexts,
                "four execution contexts start");
        observe(kind::behaviour, g_counter == static_cast<long long>(started) * kIncrements,
                "a mutex built from the suspension primitive loses no increment");
        put("  increments: "); put_signed(g_counter);
        put(" of "); put_signed(static_cast<long long>(started) * kIncrements); put("\n");
    }

    // Relinquishing the processor is permitted to do nothing, and an
    // implementation without a scheduler returns immediately --- which is a
    // supply and not a simulation, so the observation is that it returns.
    { kal_task_yield(); observe(kind::behaviour, true, "relinquishing the processor returns"); }

    // A property that is claimed is a property that can be checked.
    if (kal::task::has(kal::task::wait_timeout)) {
        volatile kal_u32 never = 0;
        const kal_duration t0 = kal_time_monotonic();
        const int e = kal_task_wait(const_cast<const kal_u32*>(&never), 0u,
                                    30u * 1000u * 1000u);
        const kal_duration elapsed = kal_time_monotonic() - t0;
        observe(kind::behaviour, e == kal_err_again,
                "a wait with a timeout reports that it elapsed when nothing woke it");
        observe(kind::behaviour, elapsed >= 20u * 1000u * 1000u,
                "the wait lasted approximately the timeout it was given");
    } else {
        unobserved(kind::behaviour, "a wait honours a timeout",
                   "the implementation does not claim prop_wait_timeout");
    }

    if (kal::task::has(kal::task::thread_local_storage)) {
        g_per_context = 3;
        kal_task t{};
        if (kal_task_start(records_its_own, nullptr, &t) == kal_ok) {
            kal_task_join(t);
            observe(kind::behaviour,
                    __atomic_load_n(&g_saw, __ATOMIC_ACQUIRE) == 7 && g_per_context == 3,
                    "a started context observes its own thread-local storage");
        } else {
            unobserved(kind::behaviour, "a started context observes its own thread-local storage",
                       "a context could not be started");
        }
    } else {
        unobserved(kind::behaviour, "a started context observes its own thread-local storage",
                   "the implementation does not claim prop_thread_local");
    }

    if (performs(kind::abi)) {
        observe(kind::abi, sizeof(kal_task) == sizeof(kal_uintptr),
                "a task handle occupies one machine word");
        const kal_uintptr assigned = (kal::task::preemptive | kal::task::parallel
                                    | kal::task::wait_timeout
                                    | kal::task::thread_local_storage).bits;
        observe(kind::abi, (kal_task_props & ~assigned) == 0,
                "the capability word contains no position the specification has not assigned");
        observe(kind::abi, kal_task_current() == kal_task_current(),
                "the identity of the calling context is stable within it");

        // A wake of no contexts is permitted and shall report that it woke
        // none, which is what a caller uses to decide whether to try again.
        volatile kal_u32 nobody = 0;
        kal_uintptr woken = 99;
        observe(kind::abi,
                kal_task_wake(const_cast<const kal_u32*>(&nobody), 0, &woken) == kal_ok
                    && woken == 0,
                "waking no contexts succeeds and reports that none were woken");
    }

    if (performs(kind::stability)) {
        // Starting and awaiting many contexts. An implementation that leaked a
        // stack or a handle per context stops here and nowhere earlier.
        bool all = true;
        const int rounds = repetitions / 10;
        for (int i = 0; i < rounds && all; ++i) {
            kal_task t{};
            if (kal_task_start(does_nothing, nullptr, &t) != kal_ok) all = false;
            else if (kal_task_join(t) != kal_ok) all = false;
        }
        observe(kind::stability, all, "contexts started and awaited many times keep starting");
        put("  contexts started and awaited: "); put_signed(rounds); put("\n");
    }

    if (performs(kind::cost)) {
        const int rounds = cost_iterations / 10;
        const kal_duration t0 = kal_time_monotonic();
        for (int i = 0; i < rounds; ++i) {
            kal_task t{};
            if (kal_task_start(does_nothing, nullptr, &t) == kal_ok) kal_task_join(t);
        }
        measure("starting and awaiting an execution context", kal_time_monotonic() - t0, rounds);

        volatile kal_u32 w = 0;
        const kal_duration t1 = kal_time_monotonic();
        for (int i = 0; i < cost_iterations; ++i) {
            kal_uintptr woken = 0;
            kal_task_wake(const_cast<const kal_u32*>(&w), 1, &woken);
        }
        measure("waking an address nothing waits upon", kal_time_monotonic() - t1, cost_iterations);
    }
#endif
}

}  // namespace okc::task
