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
export module openkal.time;
export import openkal.types;

// Nanoseconds. The unit is fixed rather than reported, because a program that
// must consult a unit before performing arithmetic acquires a branch that no
// environment needs.
export using kal_duration = __UINT64_TYPE__;

export extern "C" {

// Elapsed time from an unspecified origin. The origin is fixed for the lifetime
// of the program, so differences are meaningful and absolute values are not.
kal_duration kal_time_monotonic(void);

// Time since 1970-01-01T00:00:00Z, disregarding leap seconds. An implementation
// whose environment has no such notion reports zero, which the capability word
// distinguishes from an environment whose clock happens to read zero.
kal_duration kal_time_wall(void);

// The granularity of the monotonic source, in nanoseconds. A program that
// measures short intervals requires it in order to know what it has measured.
kal_duration kal_time_monotonic_granularity(void);

// Suspends the calling context for at least the given duration. An
// implementation may suspend for longer and shall not suspend for less.
void kal_time_sleep(kal_duration ns);

// Properties of this implementation's time sources.
extern const kal_uintptr kal_time_props;

}

export namespace kal::time {

// Positions in kal_time_props. A position, once assigned, retains its meaning;
// an unassigned position reads as zero, so that a program compiled against a
// later specification behaves correctly against an earlier implementation.
enum : kal_uintptr {
    prop_wall_available     = 1u << 0,  // the wall source reports a real time
    prop_monotonic_suspends = 1u << 1,  // the monotonic source stops while the machine is suspended
    prop_sleep_precise      = 1u << 2,  // suspension granularity matches the monotonic granularity
};

inline kal_duration monotonic()   { return kal_time_monotonic(); }
inline kal_duration wall()        { return kal_time_wall(); }
inline kal_duration granularity() { return kal_time_monotonic_granularity(); }
inline void         sleep(kal_duration ns) { kal_time_sleep(ns); }
inline bool         has(kal_uintptr p) { return (kal_time_props & p) != 0; }

}
