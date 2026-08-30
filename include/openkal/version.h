/* openkal --- what an implementation says about itself before it is used.
 *
 * ⚠️ THIS IS NOT AN INTERFACE. It provides no resource, and clause 3.2 closes
 * the set of core interfaces rather than the set of things every implementation
 * must export. What is here is the specification's own self-description: two
 * operations every conforming implementation exports, belonging to none of the
 * interfaces, so that a consumer can ask before it calls.
 *
 * ⭐ WHY IT CANNOT BE A NOTE IN THE ARTIFACT. The earlier answer to version skew
 * was a record placed in the object file. Object files are of three kinds here
 * --- ELF, Mach-O and PE --- and a boundary crossed by a trap has no object file
 * at all. An operation is the one form every boundary already has.
 *
 * ⭐ WHY IT ANSWERS TWO QUESTIONS AND NOT ONE. A consumer that is linked learns
 * an interface's absence from the linker (clause 6.1). A consumer that is bound
 * at load or crosses a boundary has no linker to learn it from, and asking each
 * interface in turn requires calling into it, which is the thing that must not
 * happen first. One word of interface positions answers it for all of them
 * before anything else is called.
 *
 * ⚠️ AND NEITHER ANSWER IS A CAPABILITY. `kal_interfaces' says which interfaces
 * exist, not how they behave; how an implementation behaves within an interface
 * it provides is a property word, which is clause 6.2's own division and is not
 * restated here.
 */
#ifndef OPENKAL_VERSION_H
#define OPENKAL_VERSION_H
#include "types.h"

/* The version of the specification this consumer was compiled against. A
 * consumer compares it with what `kal_version' answers and refuses to proceed
 * against an implementation older than the declarations it holds --- because an
 * older implementation reports conditions this consumer distinguishes as
 * conditions it does not, which is a wrong answer rather than a refusal. */
#define KAL_VERSION_MAJOR 0u
#define KAL_VERSION_MINOR 11u
#define KAL_VERSION_PATCH 0u

#define KAL_VERSION_MAKE(major, minor, patch)             \
    (((kal_u64)(major) << 32) | ((kal_u64)(minor) << 16) | \
     ((kal_u64)(patch)))

#define KAL_VERSION \
    KAL_VERSION_MAKE(KAL_VERSION_MAJOR, KAL_VERSION_MINOR, KAL_VERSION_PATCH)

/* Positions in the result of kal_interfaces.
 *
 * A position, once assigned, retains its meaning; a position that has not been
 * assigned reads as zero, so a consumer compiled against a later specification
 * behaves correctly against an earlier implementation. This is clause 6.2's
 * rule for a property word, applied to the presence of interfaces.
 *
 * The three core interfaces have positions of their own even though every
 * implementation provides them. A word in which the answer for some interfaces
 * could not be expressed would be a word a reader has to know the exceptions
 * to. */
#define KAL_IFACE_ABORT    ((kal_u64)1u << 0)
#define KAL_IFACE_STREAM   ((kal_u64)1u << 1)
#define KAL_IFACE_MEMORY   ((kal_u64)1u << 2)
#define KAL_IFACE_ENV      ((kal_u64)1u << 3)
#define KAL_IFACE_TIME     ((kal_u64)1u << 4)
#define KAL_IFACE_RANDOM   ((kal_u64)1u << 5)
#define KAL_IFACE_FS       ((kal_u64)1u << 6)
#define KAL_IFACE_PROCESS  ((kal_u64)1u << 7)
#define KAL_IFACE_TASK     ((kal_u64)1u << 8)
#define KAL_IFACE_EXEC     ((kal_u64)1u << 9)
#define KAL_IFACE_TERMINAL ((kal_u64)1u << 10)
#define KAL_IFACE_NET      ((kal_u64)1u << 11)
#define KAL_IFACE_DATAGRAM ((kal_u64)1u << 12)
#define KAL_IFACE_SPACE    ((kal_u64)1u << 13)
#define KAL_IFACE_TIMEOUT  ((kal_u64)1u << 14)

#ifdef __cplusplus
extern "C" {
#endif

/* The version of the specification this implementation was written against,
 * encoded by KAL_VERSION_MAKE. */
kal_u64 kal_version(void);

/* The interfaces this implementation provides, as positions above. */
kal_u64 kal_interfaces(void);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_VERSION_H */
