// openkal.abort --- termination.
//
// This interface belongs to the core set. Every implementation provides it,
// because every environment can terminate: an implementation with nothing else
// available may halt the processor, and halting is an implementation rather
// than a simulation.
export module openkal.abort;
export import openkal.types;

export extern "C" {

// Terminates the program after reporting the given message. The message is
// passed with an explicit length so that the interface does not depend on a
// string function being available.
//
// The function does not return. Returning would continue into the state the
// caller has just declared impossible.
[[noreturn]] void kal_abort(const char* msg, kal_uintptr len);

// Terminates the program with the given status. Termination is immediate:
// registered exit handlers and static destructors do not run. An
// implementation that runs them is not conforming, because a caller cannot
// then reason about what executes after the call.
[[noreturn]] void kal_exit(int code);

}

export namespace kal {

[[noreturn]] inline void abort(const char* msg, kal_uintptr len) { kal_abort(msg, len); }
[[noreturn]] inline void exit(int code) { kal_exit(code); }

}
