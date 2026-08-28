/* openkal.random --- a source of unpredictable bytes.
 *
 * ⚠️ NOT A GENERATOR. This interface answers "give me bytes this environment
 * considers unpredictable". It does not define a pseudo-random algorithm, hold
 * state between calls, or promise a distribution. A program wanting a
 * reproducible sequence seeds its own generator from these bytes once and does
 * not come back; a program wanting unpredictability comes back.
 *
 * ⭐ THE INTERFACE EXISTS BECAUSE NOTHING ELSE HERE CAN SUPPLY IT. Entropy is
 * not derivable from the other eight: a clock is not a source (a reading is
 * unpredictable to a reader of the source and not to an adversary), and
 * `openkal.fs` deliberately cannot open a platform-named object such as
 * `/dev/urandom` --- a capability-oriented filesystem hands over roots, not
 * absolute paths, and that refusal is the model working.
 *
 * ⚠️ AND IT IS NOT UNIVERSAL, WHICH IS WHY IT IS ITS OWN INTERFACE. Every
 * hosted platform has one; a bare-metal machine has one only if its board does.
 * Clause 6.1 already expresses that: an implementation providing no source
 * omits these names, and a program that asks fails to link rather than
 * receiving zeros.
 */
#ifndef OPENKAL_RANDOM_H
#define OPENKAL_RANDOM_H
#include "types.h"

/* Positions in kal_random_props.
 *
 * ⚠️ THERE IS NO `AVAILABLE` POSITION, AND ITS ABSENCE IS THE DESIGN. Whether
 * an environment has a source is answered by whether this interface is present
 * at all (clause 6.1), not by a word a program reads after linking. A backend
 * that defined `kal_random_props = 0` while providing no operation would let a
 * program past the point the linker exists to stop it at. */
#define KAL_RANDOM_PROP_BLOCKING ((kal_uintptr)1u << 0)
#define KAL_RANDOM_PROP_HARDWARE ((kal_uintptr)1u << 1)

#ifdef __cplusplus
extern "C" {
#endif

/* Fills `len` bytes at `out`.
 *
 * ⚠️ NO PARTIAL SUCCESS. Either every byte is filled or none is. A caller that
 * had to loop would have to distinguish "short read" from "no more entropy",
 * and the second is not a state this interface has: an environment either has a
 * source or does not provide the interface.
 *
 * `kal_err_again` reports a source that is momentarily empty --- a machine
 * early in boot whose pool has not been seeded. It is not "this environment has
 * no source", which is answered by the interface's absence. */
int kal_random_fill(void* out, kal_uintptr len);

kal_uintptr kal_random_props(void);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_RANDOM_H */
