// openkal.types --- definitions shared by every openkal interface.
//
// There is one statement of the declarations and two ways to reach it. The
// statement is the C header, because the contract is a C application binary
// interface; this module includes it in its global module fragment and exports
// the names, so a consumer that imports and a consumer that includes obtain
// the same entities rather than two declarations that agree today.
//
// What the module adds is not a second declaration. It is what C++ can check
// and C cannot: the layouts clause 5.3 freezes are asserted here, and the
// capability words become types that cannot be mixed.
module;
#include <openkal/types.h>

export module openkal.types;

// The width of a machine word, and the three fixed widths the operations use.
export using ::kal_uintptr;
export using ::kal_u32;
export using ::kal_u64;
export using ::kal_i64;
export using ::kal_u8;

// The complete set of error conditions openkal defines. The set is closed: an
// implementation maps its environment's error values onto these and does not
// extend them.
export using ::kal_error;
export using ::kal_ok;
export using ::kal_err_invalid;
export using ::kal_err_again;
export using ::kal_err_io;
export using ::kal_err_no_memory;
export using ::kal_err_no_space;
export using ::kal_err_permission;
export using ::kal_err_not_supported;
export using ::kal_err_closed;
export using ::kal_err_not_found;
export using ::kal_err_exists;
export using ::kal_err_not_empty;
export using ::kal_err_is_directory;
export using ::kal_err_not_directory;

// The result of an operation that transfers a count.
export using ::kal_io_result;
export using ::kal_endpoint;

// Clause 5.3 freezes the layout. The address is twenty-four bytes so that one
// carrying a scope identifier fits without a second type; a build in which it
// were not would read every address at the wrong offset while still linking.
static_assert(sizeof(kal_endpoint{}.addr) == 24,
              "clause 5.3: an endpoint address is twenty-four bytes");
static_assert(sizeof(kal_endpoint) >= 24 + sizeof(kal_uintptr) + sizeof(kal_u32),
              "clause 5.3: an endpoint holds its address, a length and a port");

// Clause 5.3 declares the layout of every structure immutable. A declaration
// that something shall not change is not a mechanism; this is the mechanism.
// Two machine words are returned in registers on the architectures openkal
// targets, and a wider result would be returned through a hidden pointer
// instead --- a change of calling convention that no declaration would report
// and that a consumer built against the earlier layout would not survive.
static_assert(sizeof(kal_io_result) == 2 * sizeof(kal_uintptr),
              "kal_io_result must remain two machine words: clause 5.3");
static_assert(alignof(kal_io_result) == alignof(kal_uintptr));
static_assert(__builtin_offsetof(kal_io_result, n) == 0);
static_assert(__builtin_offsetof(kal_io_result, e) == sizeof(kal_uintptr));
static_assert(sizeof(kal_uintptr) == sizeof(void*),
              "kal_uintptr must hold a pointer: clause 5.1");

export namespace kal {

// A capability word, carrying the interface it belongs to in its type.
//
// Clause 6.2 gives each interface a word named kal_<interface>_props and
// positions within it. Every such word is a kal_uintptr, so a program that
// tests a file-system position against the task word compiles, runs, and
// answers a question nobody asked. The position numbers are small and several
// interfaces have assigned the same ones, so the answer is frequently the
// plausible one.
//
// The tag makes the two words different types. The operations that compose
// positions are defined only between positions of one interface, so the
// mistake is a diagnostic rather than a result. Nothing is stored beyond the
// word itself and every operation is constant-evaluated, so the type occupies
// the same register the plain word did.
template <class Tag>
struct props {
    kal_uintptr bits;

    constexpr props() : bits(0) {}
    constexpr explicit props(kal_uintptr b) : bits(b) {}

    friend constexpr props operator|(props a, props b) { return props{a.bits | b.bits}; }
    friend constexpr props operator&(props a, props b) { return props{a.bits & b.bits}; }
    friend constexpr bool  operator==(props a, props b) { return a.bits == b.bits; }

    // Whether every position in `p` is present. The question a program asks is
    // "may I do this", and a program that requires two properties asks it once
    // rather than twice, so the test is written for a set and not for a bit.
    constexpr bool has(props p) const { return (bits & p.bits) == p.bits; }

    constexpr explicit operator bool() const { return bits != 0; }
};

// Reads a capability word an implementation defined, as the type of the
// interface it belongs to.
template <class Tag>
constexpr props<Tag> read_props(const kal_uintptr& word) { return props<Tag>{word}; }

}  // namespace kal
