// openkal.map --- a part of a file's contents in the address space.
//
// An interface of its own for clause 6.2's reason: an environment can provide
// files and be unable to map them, and placing the operations in `openkal.fs'
// would take that interface from it. The region is an address and a length,
// as an allocation is, because a handle would oblige an implementation to keep
// a table from handles to views (clause 7.1). No region of a file is
// executable: instructions run only from memory `openkal.exec' published.
module;
#include <openkal/map.h>

export module openkal.map;
export import openkal.types;
export import openkal.fs;

export using ::kal_map_file;
export using ::kal_map_sync;
export using ::kal_map_release;

export namespace kal::map {

// The alignment of an offset: a constant, because a file is laid out for
// mapping by a program that does not know where it will be mapped.
inline constexpr kal_u64 alignment = KAL_MAP_ALIGNMENT;

// How a region is made, in its own kind for the reason `kal::fs::open_flags'
// is in one: an intent passed to the wrong operation should not compile.
struct how_tag;
using how = kal::props<how_tag>;

inline constexpr how read  {0};
inline constexpr how write {KAL_MAP_WRITE};
inline constexpr how shared{KAL_MAP_SHARED};

struct region_result { void* region; int e; };

inline region_result file(kal_file f, kal_u64 offset, kal_uintptr length, how h) {
    void* p = nullptr;
    const int e = kal_map_file(f, offset, length, h.bits, &p);
    return { p, e };
}
inline int  sync(void* region, kal_uintptr length) { return kal_map_sync(region, length); }
inline void release(void* region, kal_uintptr length) { kal_map_release(region, length); }

}
