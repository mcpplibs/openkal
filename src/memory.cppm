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

export namespace kal {

inline void* alloc(kal_uintptr size, kal_uintptr align) { return kal_alloc(size, align); }
inline void  free (void* p, kal_uintptr size, kal_uintptr align) { kal_free(p, size, align); }

}
