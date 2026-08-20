/* openkal.time --- time sources. Two are distinguished because they answer
 * different questions and an environment may provide one and not the other. */
#ifndef OPENKAL_TIME_H
#define OPENKAL_TIME_H
#include "types.h"

/* Nanoseconds. The unit is fixed rather than reported, because a program that
 * must consult a unit before performing arithmetic acquires a branch that no
 * environment needs. */
typedef kal_u64 kal_duration;

/* Positions in kal_time_props. */
#define KAL_TIME_PROP_WALL_AVAILABLE     ((kal_uintptr)1u << 0)
#define KAL_TIME_PROP_MONOTONIC_SUSPENDS ((kal_uintptr)1u << 1)
#define KAL_TIME_PROP_SLEEP_PRECISE      ((kal_uintptr)1u << 2)

#ifdef __cplusplus
extern "C" {
#endif

kal_duration kal_time_monotonic(void);
kal_duration kal_time_wall(void);
kal_duration kal_time_monotonic_granularity(void);
void         kal_time_sleep(kal_duration ns);

extern const kal_uintptr kal_time_props;

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_TIME_H */
