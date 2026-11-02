// openkal.exec --- memory a program may execute.
//
// Optional, and the reason it is an interface of its own rather than a position
// in a capability word is clause 6.2: an operation an implementation may lack
// is expressed by its absence, so that a program using it without a supporting
// implementation is told by the linker. A position in a word would express the
// same fact later and less usefully, and an operation that was present and
// always refused would be the defect clause 6.1 names.
//
// Whether an implementation provides this interface can depend on how the
// artifact is produced rather than on the implementation alone --- one system
// grants a program such memory only when the program carries a signed
// declaration that it will ask. That is settled at dependency resolution, which
// clause 6.2's table already names as the earliest time such a question can be
// answered, and it is expressed as a feature of the implementation package
// rather than as anything visible here. Clause 6.5 records the arrangement.
module;
#include <openkal/exec.h>

export module openkal.exec;
export import openkal.types;
export import openkal.fs;

export using ::kal_exec_alloc;
export using ::kal_exec_alloc_at;
export using ::kal_exec_seal_part;
export using ::kal_exec_publish;
export using ::kal_exec_free;
export using ::kal_exec_props;
export using ::kal_exec_publish_part;
export using ::kal_exec_granularity;
export using ::kal_exec_map_part;

export namespace kal::exec {

struct props_tag;
using props = kal::props<props_tag>;

// A published region may be reserved for writing again. Where the position is
// zero, a caller that must change published bytes reserves a second region and
// abandons the first, so a caller that generates code more than once reads this
// before deciding how to hold what it has generated.
inline constexpr props republish{KAL_EXEC_PROP_REPUBLISH};

// Whether memory this program may execute is available to THIS ARTIFACT in
// THIS environment. Clause 6.5 settles availability at dependency resolution,
// which is the right answer for an artifact produced for its own machine and
// no answer at all for one produced once and run in many.
inline constexpr props available{KAL_EXEC_PROP_AVAILABLE};

// Part of a region may be made a file's, read or executed (version 0.21). Where
// the position is zero, a loader copies the file's bytes into the region.
inline constexpr props file{KAL_EXEC_PROP_FILE};

// A region may be reserved at the address asked for, where nothing is there
// (version 0.23). Where the position is zero, kal_exec_alloc_at answers with a
// null pointer.
inline constexpr props place{KAL_EXEC_PROP_PLACE};

// kal_exec_seal_part holds KAL_EXEC_SEAL_FOREVER; KAL_EXEC_MAP_BTI is enforced
// (version 0.23).
inline constexpr props seal_forever{KAL_EXEC_PROP_SEAL_FOREVER};
inline constexpr props bti{KAL_EXEC_PROP_BTI};

// How a part is made a file's, in its own kind for the reason
// `kal::map::how' is in one.
struct map_how_tag;
using map_how = kal::props<map_how_tag>;

inline constexpr map_how read   {0};
inline constexpr map_how execute{KAL_EXEC_MAP_EXECUTE};
inline constexpr map_how prefetch{KAL_EXEC_MAP_PREFETCH};   // 0.23
inline constexpr map_how bti_marked{KAL_EXEC_MAP_BTI};     // 0.23
inline constexpr map_how write{KAL_EXEC_MAP_WRITE};         // 0.23: private, written, never executed

// What kal_exec_seal_part asks beyond read-only (0.23).
struct seal_tag;
using seal_flags = kal::props<seal_tag>;
inline constexpr seal_flags seal_once{0};
inline constexpr seal_flags forever{KAL_EXEC_SEAL_FOREVER};

inline props properties() { return props{kal_exec_props()}; }
inline bool  has(props p) { return properties().has(p); }

inline void* alloc(kal_uintptr size) { return kal_exec_alloc(size); }
inline void* alloc_at(void* at, kal_uintptr size) { return kal_exec_alloc_at(at, size); }
inline int   seal_part(void* region, kal_uintptr offset, kal_uintptr size, seal_flags how = seal_once) {
    return kal_exec_seal_part(region, offset, size, how.bits);
}
inline int   publish(void* p, kal_uintptr size) { return kal_exec_publish(p, size); }
inline int   publish_part(void* region, kal_uintptr offset, kal_uintptr size) {
    return kal_exec_publish_part(region, offset, size);
}
inline kal_uintptr granularity() { return kal_exec_granularity(); }
inline int   map_part(void* region, kal_uintptr offset, kal_uintptr size, kal_file f, kal_u64 at,
                      map_how h) {
    return kal_exec_map_part(region, offset, size, f, at, h.bits);
}
inline void  free(void* p, kal_uintptr size) { kal_exec_free(p, size); }

}
