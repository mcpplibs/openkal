// openkal.task --- execution contexts, and the primitive they are built upon.
//
// Two resources appear here and they are of different kinds. The first is an
// execution context sharing the address space. The second is not a resource at
// all but an operation upon ordinary memory: a context suspends itself upon a
// word until another context wakes those suspended upon it.
//
// Mutexes and condition variables are absent. They are constructions above the
// primitive rather than facilities of the boundary, which is observable in any
// C library that implements them: the synchronisation objects are library code
// and the boundary provides the wait. An interface offering mutexes would place
// a library at the kernel boundary, and would oblige an implementation whose
// environment has no kernel mutex to construct one.
export module openkal.task;
export import openkal.types;

export struct kal_task { kal_uintptr h; };

export extern "C" {

// Starts a context executing the given function with the given argument. The
// stack is provided by the implementation; its size is a property rather than a
// parameter, because an environment that does not allocate stacks separately
// cannot honour a request for one.
int kal_task_start(void (*entry)(void*), void* arg, kal_task* out);

// Waits for a context to finish. A context is waited for at most once.
int kal_task_join(kal_task);

// Relinquishes the processor. An implementation without a scheduler returns
// immediately, which is a supply and not a simulation: the operation promises
// only that the caller may be descheduled, not that it will be.
void kal_task_yield(void);

// The identity of the calling context, for a program that must distinguish
// them. The value is unique among contexts running at the same moment and may
// be reused after one finishes.
kal_uintptr kal_task_current(void);

// --- The suspension primitive ---------------------------------------------
//
// Suspends the calling context while the word at the given address holds the
// given value, until another context wakes it or the timeout elapses. The
// comparison and the suspension occur without an intervening opportunity for
// the value to change unobserved, which is the property that makes the
// primitive usable and which a caller cannot construct for itself.
//
// A timeout of zero denotes no timeout.
int kal_task_wait(const __UINT32_TYPE__* word, __UINT32_TYPE__ expected,
                  __UINT64_TYPE__ timeout_ns);

// Wakes at most `count` contexts suspended upon the given address, and reports
// how many were woken. A count of zero wakes none and is permitted.
int kal_task_wake(const __UINT32_TYPE__* word, kal_uintptr count, kal_uintptr* woken);

extern const kal_uintptr kal_task_props;

}

export namespace kal::task {

using task = kal_task;

enum : kal_uintptr {
    prop_preemptive   = 1u << 0,  // a context may be descheduled without yielding
    prop_parallel     = 1u << 1,  // contexts may execute simultaneously
    prop_wait_timeout = 1u << 2,  // kal_task_wait honours a timeout
};

inline bool has(kal_uintptr p) { return (kal_task_props & p) != 0; }

}
