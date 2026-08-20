// openkal.stream --- byte streams.
//
// A stream is the resource through which bytes are transferred sequentially.
// Files, network connections, pipes and serial ports differ in how they are
// named and in what else they support; they do not differ in this. openkal
// therefore treats the stream as the shared currency and leaves naming to the
// interfaces that own the named resources.
//
// Positioning is deliberately absent. On a hosted system, whether a descriptor
// can be repositioned is a property of the individual descriptor rather than of
// the implementation: the same backend reports success for a regular file and
// failure for a pipe. An interface that offered positioning on every stream
// would therefore contain an operation that some streams can never satisfy,
// which is the defect this specification's decomposition exists to avoid.
module;
#include <openkal/stream.h>

export module openkal.stream;
export import openkal.types;

// An opaque handle occupying one machine word. The width is fixed and the
// interpretation is not: an implementation stores whatever it natively uses ---
// a descriptor, an operating-system handle, a pointer to a driver structure, a
// capability index. No implementation is required to maintain a translation
// table, and the absence of that requirement is what allows openkal to be
// implemented above a C library, below one, or without one.
export using ::kal_stream;

export using ::kal_stdin;
export using ::kal_stdout;
export using ::kal_stderr;
export using ::kal_stream_write;
export using ::kal_stream_read;
export using ::kal_stream_flush;
export using ::kal_stream_props;

static_assert(sizeof(kal_stream) == sizeof(kal_uintptr),
              "a handle occupies one machine word: clause 7.2");

export namespace kal {

using stream = kal_stream;

// The tag exists only to distinguish this interface's capability word from
// another's; it is never defined and never instantiated.
struct stream_props_tag;
using stream_props = props<stream_props_tag>;

namespace stream_prop {
// Positions in the result of kal_stream_props.
inline constexpr stream_props interactive{KAL_STREAM_PROP_INTERACTIVE};
}

inline stream in ()  { return kal_stdin();  }
inline stream out()  { return kal_stdout(); }
inline stream err()  { return kal_stderr(); }

inline kal_io_result write(stream s, const void* p, kal_uintptr n) { return kal_stream_write(s, p, n); }
inline kal_io_result read (stream s, void* p, kal_uintptr n)       { return kal_stream_read(s, p, n); }
inline int           flush(stream s)                               { return kal_stream_flush(s); }

// The properties of one stream, as a set that cannot be confused with another
// interface's. A caller asks whether a stream is interactive before it has
// transferred anything, because that is when the answer changes what it does.
inline stream_props properties(stream s) { return stream_props{kal_stream_props(s)}; }

}
