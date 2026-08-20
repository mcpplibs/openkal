// okc.child --- the copy of itself the suite starts.
//
// Two of the specification's requirements cannot be observed from within the
// observing program, because satisfying them ends it: kal_exit terminates
// immediately, and kal_abort does not return. They are therefore observed in a
// started copy, whose status is the only channel openkal.process defines.
//
// The copy must be able to tell that it is one, and the only thing that
// distinguishes it is the argument vector it was started with --- which is
// openkal.env. An arrangement that did not select openkal.env therefore cannot
// observe termination at all, and says so rather than passing.
export module okc.child;

export namespace okc {

enum class errand {
    none = 0,
    exit_with_33,        // kal_exit shall terminate immediately with the status
    exit_after_writing,  // ... and shall not run what a C library would run after
    abort_with_message,  // kal_abort shall report the message and not return
    wait_to_be_terminated,   // so that a request to terminate has something to reach
};

// What this program was started to do, or `none' if it was started by a person.
errand child_errand();

// Performs it. Does not return.
[[noreturn]] void perform(errand e);

// The name of the argument that selects each errand, for the parent to pass.
const char* argument_for(errand e);

}  // namespace okc
