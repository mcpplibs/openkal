// openkal.space --- an address space, and a context executing in one.
//
// This is not fork. Clause 7.1 refuses to require the duplication of an address
// space AND its execution state; what is here is the first half alone. A
// context started in a cloned space begins at a function the caller names, not
// at the instruction the caller was executing --- which is what lets this be
// stated in a C application binary interface at all.
//
// A library above this interface reaches fork by saving its own execution state
// before the call and restoring it in the started context. That is composition,
// and it belongs above this line rather than in it: the saving is done with the
// compiler's own facilities, differs per architecture, and is not something a
// kernel interface can perform on a caller's behalf.
//
// One operation and not two. An earlier form separated the copying of the space
// from the starting of a context in it, so that a caller held a space as a
// handle. No environment this specification targets has that pair as a
// primitive, and an implementation asked to separate them would have to park a
// started context and build a channel by which to tell it what to run --- a
// mechanism reconstructed, which clause 7.1 identifies as a fault in the shape
// of the specification rather than in the implementation.
module;
#include <openkal/space.h>

export module openkal.space;
export import openkal.types;
export import openkal.process;

export using ::kal_space_start;
export using ::kal_space_props;

export namespace kal::space {

struct props_tag;
using props = kal::props<props_tag>;

inline constexpr props clone_handles{KAL_SPACE_PROP_CLONE_HANDLES};
inline constexpr props deferred_copy{KAL_SPACE_PROP_DEFERRED_COPY};

inline props properties() { return props{kal_space_props()}; }
inline bool  has(props p) { return properties().has(p); }

struct process_result { kal_process p; int e; };

inline process_result start(void (*entry)(void*), void* arg, void* stack_top) {
    kal_process p{};
    const int e = kal_space_start(entry, arg, stack_top, &p);
    return { p, e };
}

}
