// openkal.time --- time sources.
//
// Two sources are distinguished because they answer different questions and
// because an environment may provide one and not the other. A monotonic source
// measures elapsed time and never decreases; a wall source reports the time
// agreed with the rest of the world and may be adjusted, including backwards.
//
// A program that measures a duration requires the first, and a program that
// records when something happened requires the second. Merging them would
// oblige an implementation whose clock cannot be adjusted to claim that it can,
// or the reverse.
module;
#include <openkal/time.h>

export module openkal.time;
export import openkal.types;

// Nanoseconds. The unit is fixed rather than reported, because a program that
// must consult a unit before performing arithmetic acquires a branch that no
// environment needs.
export using ::kal_duration;

export using ::kal_time_monotonic;
export using ::kal_time_wall;
export using ::kal_time_monotonic_granularity;
export using ::kal_time_sleep;
export using ::kal_time_props;

export namespace kal::time {

struct props_tag;
using props = kal::props<props_tag>;

// Positions in kal_time_props. A position, once assigned, retains its meaning;
// an unassigned position reads as zero, so that a program compiled against a
// later specification behaves correctly against an earlier implementation.
inline constexpr props wall_available    {KAL_TIME_PROP_WALL_AVAILABLE};
inline constexpr props monotonic_suspends{KAL_TIME_PROP_MONOTONIC_SUSPENDS};
inline constexpr props sleep_precise     {KAL_TIME_PROP_SLEEP_PRECISE};

inline kal_duration monotonic()   { return kal_time_monotonic(); }
inline kal_duration wall()        { return kal_time_wall(); }
inline kal_duration granularity() { return kal_time_monotonic_granularity(); }
inline void         sleep(kal_duration ns) { kal_time_sleep(ns); }

inline props properties() { return props{kal_time_props()}; }
inline bool  has(props p) { return properties().has(p); }

}
