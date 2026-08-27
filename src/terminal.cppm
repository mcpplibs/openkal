// openkal.terminal --- what an interactive stream does with what is typed at
// it.
//
// A separate interface rather than operations upon openkal.stream, for the
// reason clause 6.4 gives when it places positioning in openkal.fs: the
// behaviour varies between the RESOURCES of the stream interface rather than
// between implementations. The same implementation answers one way for a
// terminal and another for a file, so an implementation could neither claim
// these operations honestly nor withhold them usefully.
//
// The pair is get/set and not two setters. A program that turns line editing
// off must be able to put back what was there, and a setter alone would let it
// restore only a default --- the terminal a user returns to would then not be
// the one they had. Clause 7.11 states the rule; this is an instance of it.
module;
#include <openkal/terminal.h>

export module openkal.terminal;
export import openkal.types;
export import openkal.stream;

export using ::kal_terminal_get_mode;
export using ::kal_terminal_set_mode;
export using ::kal_terminal_size;
export using ::kal_terminal_props;

export namespace kal::terminal {

// Positions in the MODE word. Distinct from the props word below: one says
// what the terminal is doing, the other what this implementation can be asked.
// They are separate types so that a program cannot test one against the other.
struct mode_tag;
using mode = kal::props<mode_tag>;

inline constexpr mode line_edit{KAL_TERM_LINE_EDIT};
inline constexpr mode echo     {KAL_TERM_ECHO};

struct props_tag;
using props = kal::props<props_tag>;

inline constexpr props has_mode{KAL_TERM_PROP_MODE};
inline constexpr props has_size{KAL_TERM_PROP_SIZE};

inline props properties(kal_stream s) { return props{kal_terminal_props(s)}; }
inline bool  has(kal_stream s, props p) { return properties(s).has(p); }

// The mode currently in effect. Reported alongside the error rather than
// through an out-parameter, so that a caller which restores what it found
// writes one statement and not three.
struct mode_result { mode m; int e; };

inline mode_result get_mode(kal_stream s) {
    kal_uintptr bits = 0;
    const int e = kal_terminal_get_mode(s, &bits);
    return { mode{bits}, e };
}

inline int set_mode(kal_stream s, mode m) { return kal_terminal_set_mode(s, m.bits); }

struct size_result { kal_uintptr cols; kal_uintptr rows; int e; };

inline size_result size(kal_stream s) {
    kal_uintptr c = 0, r = 0;
    const int e = kal_terminal_size(s, &c, &r);
    return { c, r, e };
}

}
