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
module;
#include <openkal/task.h>

export module openkal.task;
export import openkal.types;

export using ::kal_task;

export using ::kal_task_start;
export using ::kal_task_join;
export using ::kal_task_yield;
export using ::kal_task_current;
export using ::kal_task_wait;
export using ::kal_task_wake;
export using ::kal_task_props;

static_assert(sizeof(kal_task) == sizeof(kal_uintptr), "clause 7.2");

export namespace kal::task {

using task = kal_task;

struct props_tag;
using props = kal::props<props_tag>;

inline constexpr props preemptive  {KAL_TASK_PROP_PREEMPTIVE};
inline constexpr props parallel    {KAL_TASK_PROP_PARALLEL};
inline constexpr props wait_timeout{KAL_TASK_PROP_WAIT_TIMEOUT};

// A context started by kal_task_start observes the thread-local storage of the
// toolchain that compiled the program. Clause 7.10 states why the property is
// reported rather than provided: the register convention belongs to openarch,
// and a C library ported onto openkal keeps its per-context state in one such
// variable and therefore cannot be ported onto an implementation without it.
inline constexpr props thread_local_storage{KAL_TASK_PROP_THREAD_LOCAL};

inline props properties() { return props{kal_task_props}; }
inline bool  has(props p) { return properties().has(p); }

}
