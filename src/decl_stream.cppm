// openkal.decl.stream --- byte streams.
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
export module openkal.decl.stream;
export import openkal.decl.types;

// An opaque handle occupying one machine word.
//
// The width is fixed and the interpretation is not. An implementation stores
// whatever it natively uses: a descriptor, an operating-system handle, a
// pointer to a driver structure, or a capability index. No implementation is
// required to maintain a translation table, and the absence of that requirement
// is what allows openkal to be implemented above a C library, below one, or
// without one.
export struct kal_stream { kal_uintptr h; };

export extern "C" {

// The program's standard streams. These handles are borrowed: the caller does
// not own them and does not close them.
kal_stream kal_stdin (void);
kal_stream kal_stdout(void);
kal_stream kal_stderr(void);

// Transfers the whole buffer, or reports the condition that prevented it.
//
// A partial transfer is not a successful outcome. The alternative convention,
// in which the caller inspects the count and repeats the call, places the same
// loop in every caller and has been a recurring source of defects in interfaces
// that adopted it. The loop belongs in the implementation, which is written
// once.
//
// On failure, `n` reports how many bytes were transferred before the failure.
kal_io_result kal_stream_write(kal_stream s, const void* buf, kal_uintptr len);

// Transfers at most `len` bytes and reports how many were transferred. A
// result of zero bytes with `kal_ok` indicates end of input; unlike a partial
// write, a partial read carries information the caller needs.
kal_io_result kal_stream_read(kal_stream s, void* buf, kal_uintptr len);

// Commits any buffering the implementation performs. An implementation that
// does not buffer returns `kal_ok`.
int kal_stream_flush(kal_stream s);

}

export namespace kal {

using stream = kal_stream;

inline stream in ()  { return kal_stdin();  }
inline stream out()  { return kal_stdout(); }
inline stream err()  { return kal_stderr(); }

inline kal_io_result write(stream s, const void* p, kal_uintptr n) { return kal_stream_write(s, p, n); }
inline kal_io_result read (stream s, void* p, kal_uintptr n)       { return kal_stream_read(s, p, n); }
inline int           flush(stream s)                                { return kal_stream_flush(s); }

// --- Optional capabilities -------------------------------------------------
//
// The declarations below are the fallbacks an implementation displaces by
// providing its own. They are reached by ordinary argument-dependent lookup, so
// an implementation supplies a capability by declaring it and withholds one by
// remaining silent. No separate capability record is required, and none is
// permitted: a record can disagree with the code it describes, whereas a
// declaration cannot.
//
// The return type distinguishes the fallback from a real implementation. If the
// two agreed, the detection predicate below would be satisfied by the fallback
// itself, because a requires-expression does not instantiate a function body
// and the assertion inside it would never be reached.
struct unsupported_t {};
template <class> inline constexpr bool always_false = false;

template <class S>
unsupported_t write_vectored(S, const void*, kal_uintptr) {
    static_assert(always_false<S>,
        "this openkal implementation does not provide vectored writes");
    return {};
}

template <class S> concept has_write_vectored =
    requires(S s, const void* p, kal_uintptr n) {
        requires __is_same(decltype(write_vectored(s, p, n)), kal_io_result);
    };

}
