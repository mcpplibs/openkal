// openkal --- what an implementation says about itself before it is used.
//
// Not an interface: it provides no resource. Clause 3.2 closes the set of core
// INTERFACES, and this closes nothing, because what is here is the
// specification's own self-description --- two operations every conforming
// implementation exports and that belong to none of the interfaces.
//
// A consumer that is linked learns an interface's absence from the linker
// (clause 6.1). A consumer bound at load, or across a boundary, has no linker
// to learn it from, and asking an interface by calling into it is the thing
// that must not happen first. These two operations are what it asks instead.
module;
#include <openkal/version.h>

export module openkal.version;
export import openkal.types;

export using ::kal_version;
export using ::kal_interfaces;

export namespace kal {

// The version of the specification the consumer holds declarations for.
inline constexpr kal_u64 header_version = KAL_VERSION;

// What the implementation answers.
inline kal_u64 version() { return kal_version(); }

// Whether the implementation is at least as new as the declarations this
// consumer was compiled against.
//
// A consumer that proceeds against an older implementation is not merely
// missing a facility: the error set grew, so conditions it distinguishes are
// reported to it as conditions it does not distinguish. That is a wrong answer
// rather than a refusal, and a refusal is what a caller can act upon.
inline bool satisfies_header() { return kal_version() >= header_version; }

// The interfaces the implementation provides.
struct interfaces_tag;
using interfaces_set = props<interfaces_tag>;

namespace iface {
inline constexpr interfaces_set abort_   {KAL_IFACE_ABORT};
inline constexpr interfaces_set stream   {KAL_IFACE_STREAM};
inline constexpr interfaces_set memory   {KAL_IFACE_MEMORY};
inline constexpr interfaces_set env      {KAL_IFACE_ENV};
inline constexpr interfaces_set time     {KAL_IFACE_TIME};
inline constexpr interfaces_set random   {KAL_IFACE_RANDOM};
inline constexpr interfaces_set fs       {KAL_IFACE_FS};
inline constexpr interfaces_set process  {KAL_IFACE_PROCESS};
inline constexpr interfaces_set task     {KAL_IFACE_TASK};
inline constexpr interfaces_set exec     {KAL_IFACE_EXEC};
inline constexpr interfaces_set terminal {KAL_IFACE_TERMINAL};
inline constexpr interfaces_set net      {KAL_IFACE_NET};
inline constexpr interfaces_set datagram {KAL_IFACE_DATAGRAM};
inline constexpr interfaces_set space    {KAL_IFACE_SPACE};
inline constexpr interfaces_set timeout  {KAL_IFACE_TIMEOUT};
}

inline interfaces_set interfaces() {
    return interfaces_set{(kal_uintptr)kal_interfaces()};
}
inline bool provides(interfaces_set s) { return interfaces().has(s); }

}  // namespace kal
