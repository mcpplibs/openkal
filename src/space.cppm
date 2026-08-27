// openkal.space --- an address space, and a context executing in one.
//
// This is not fork. Clause 7.1 refuses to require the duplication of an address
// space AND its execution state; what is here is the first half alone. A
// context started in a cloned space begins at a function the caller names, not
// at the instruction the caller was executing --- which is what lets this be
// stated in a C application binary interface at all.
//
// A library above this interface reaches fork by saving its own execution state
// before the clone and restoring it in the new context. That is composition,
// and it belongs above this line rather than in it: the saving is done with the
// compiler's own facilities, differs per architecture, and is not something a
// kernel interface can perform on a caller's behalf.
module;
#include <openkal/space.h>

export module openkal.space;
export import openkal.types;
export import openkal.process;

export using ::kal_space;

export using ::kal_space_clone;
export using ::kal_space_start;
export using ::kal_space_destroy;
export using ::kal_space_props;

static_assert(sizeof(kal_space) == sizeof(kal_uintptr), "clause 7.2");

export namespace kal::space {

using space = kal_space;

struct props_tag;
using props = kal::props<props_tag>;

inline constexpr props clone_handles{KAL_SPACE_PROP_CLONE_HANDLES};
inline constexpr props deferred_copy{KAL_SPACE_PROP_DEFERRED_COPY};

inline props properties() { return props{kal_space_props}; }
inline bool  has(props p) { return properties().has(p); }

struct clone_result   { space          sp; int e; };
struct process_result { kal_process    p;  int e; };

inline clone_result clone() {
    space sp{};
    const int e = kal_space_clone(&sp);
    return { sp, e };
}

inline process_result start(space sp, void (*entry)(void*), void* arg, void* stack_top) {
    kal_process p{};
    const int e = kal_space_start(sp, entry, arg, stack_top, &p);
    return { p, e };
}

inline void destroy(space sp) { kal_space_destroy(sp); }

}
