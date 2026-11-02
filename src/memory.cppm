// openkal.memory --- allocation.
//
// This interface belongs to the core set. The judgement rests on a distinction
// the specification applies throughout: an implementation that would make its
// callers silently wrong is a simulation and disqualifies the interface from
// the core set, whereas one that merely bounds what is available is an
// implementation. A bump allocator over a fixed region can exhaust, and
// exhaustion is a defined outcome on every platform; a clock that does not
// advance, by contrast, makes every timed wait wrong without reporting it.
//
// Whether an environment "has a heap" is therefore not a property of the
// hardware. Any environment with writable memory can supply one.
module;
#include <openkal/memory.h>

export module openkal.memory;
export import openkal.types;

export using ::kal_alloc;
export using ::kal_free;
export using ::kal_memory_granularity;
export using ::kal_memory_info;
export using ::kal_memory_status;

export namespace kal {

inline void* alloc(kal_uintptr size, kal_uintptr align) { return kal_alloc(size, align); }
inline void  free (void* p, kal_uintptr size, kal_uintptr align) { kal_free(p, size, align); }

// The quantum this environment allocates and protects memory in. An address and
// a length that are multiples of it are acceptable everywhere; an environment
// with more than one such quantum reports the coarsest, and one with none
// reports 1. It is an operation and not a constant because it is a property of
// the machine the program runs on rather than of the machine it was built for.
inline kal_uintptr granularity() { return kal_memory_granularity(); }

static_assert(sizeof(kal_memory_status) == 24, "clause 5.3");
static_assert(__builtin_offsetof(kal_memory_status, available_bytes) == 16, "clause 5.3");

// The fields of kal_memory_status, for `present' (0.23).
namespace mstatus {
inline constexpr kal_u32 total     = KAL_MSTATUS_TOTAL;
inline constexpr kal_u32 available = KAL_MSTATUS_AVAILABLE;
}

// What the environment can give this program now (0.23); `present' says which
// of the two it filled.
inline kal_memory_status memory_status() {
    kal_memory_status s{};
    s.self_size = sizeof s;
    kal_memory_info(&s);
    return s;
}

}
