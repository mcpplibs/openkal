// openkal.region --- a range of the address space reserved first and backed as
// it is used (version 0.20).
//
// An interface of its own for clause 6.2's reason: an environment without a
// page table has memory and cannot reserve addresses apart from backing them,
// and placed in the core interface the operations would be required of it.
// The region is an address and a length, as an allocation is.
module;
#include <openkal/region.h>

export module openkal.region;
export import openkal.types;

export using ::kal_region_reserve;
export using ::kal_region_commit;
export using ::kal_region_decommit;
export using ::kal_region_discard;
export using ::kal_region_release;
export using ::kal_region_props;

export namespace kal::region {

struct props_tag;
using props = kal::props<props_tag>;

// The environment does not charge backing apart from reservation.
inline constexpr props overcommit{KAL_REGION_PROP_OVERCOMMIT};

inline props properties() { return props{kal_region_props()}; }
inline bool  has(props p) { return properties().has(p); }

// The whole range backed at once.
inline constexpr kal_uintptr commit_now = KAL_REGION_COMMIT;

}
