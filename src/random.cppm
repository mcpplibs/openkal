// openkal.random --- a source of unpredictable bytes.
//
// ⭐ THE INTERFACE EXISTS BECAUSE NOTHING ELSE HERE CAN SUPPLY IT, WHICH IS THE
// ONLY REASON ANY INTERFACE HERE EXISTS.
//
// Entropy is not derivable from the other eight. A clock reading is
// unpredictable to a reader of the source and not to an adversary — a
// distinction a port that tried it has already had to write down. And
// `openkal.fs` deliberately cannot open a platform-named object such as
// `/dev/urandom`: a capability-oriented filesystem hands a program its roots
// rather than the whole namespace, so that refusal is the model working rather
// than a gap in it.
//
// ⚠️ NOT UNIVERSAL, AND THAT IS WHY IT IS ITS OWN INTERFACE RATHER THAN AN
// OPERATION ON AN EXISTING ONE. Every hosted platform has a source; a
// bare-metal machine has one only if its board does. Clause 6.1 already
// expresses exactly that shape: an implementation with no source omits these
// names, and a program that asks fails to LINK rather than receiving zeros at
// run time.
module;
#include <openkal/random.h>

export module openkal.random;
export import openkal.types;

export using ::kal_random_fill;
export using ::kal_random_props;

export namespace kal::random {

struct props_tag;
using props = kal::props<props_tag>;

// Positions in kal_random_props. A position, once assigned, retains its
// meaning; an unassigned position reads as zero, so that a program compiled
// against a later specification behaves correctly against an earlier
// implementation.
//
// ⚠️ THERE IS NO `available` POSITION. Whether an environment has a source is
// answered by whether this module can be imported and its names linked, not by
// a word read after linking — see the note on the header.
inline constexpr props blocking{KAL_RANDOM_PROP_BLOCKING};
inline constexpr props hardware{KAL_RANDOM_PROP_HARDWARE};

// Fills `out` with `len` unpredictable bytes, or fills none of it.
inline int fill(void* out, kal_uintptr len) {
    return kal_random_fill(out, len);
}

inline props properties() { return props{kal_random_props}; }
inline bool  has(props p) { return properties().has(p); }

}
