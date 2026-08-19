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
export module openkal.memory;
export import openkal.types;

export extern "C" {

// Returns a region of at least `size` bytes aligned to `align`, or a null
// pointer. `align` shall be a power of two.
//
// Where the environment already provides an allocator, an implementation of
// this function is required to be built upon it rather than beside it. Two
// allocators drawing on one region of memory is a defect that appears only
// under load: a C library's formatted output is commonly coupled to its own
// allocator, so an implementation that introduces a second one places two
// independent claimants on the same memory.
void* kal_alloc(kal_uintptr size, kal_uintptr align);

// Releases a region obtained from kal_alloc. The size and alignment are those
// passed to the allocation, and an implementation that does not need them
// discards them at no cost.
void kal_free(void* p, kal_uintptr size, kal_uintptr align);

}

export namespace kal {

inline void* alloc(kal_uintptr size, kal_uintptr align) { return kal_alloc(size, align); }
inline void  free (void* p, kal_uintptr size, kal_uintptr align) { kal_free(p, size, align); }

}
