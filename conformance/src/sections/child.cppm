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

import openkal.types;
import openkal.fs;
import openkal.process;

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

// Where this program itself is, expressed the way openkal names things: a
// directory the environment supplied and a remainder relative to it.
//
// openkal gives a program no operation that resolves a global name --- that
// work belongs to a C library and is performed once against the supplied
// directories --- so a program without one performs it, and this is it. It asks
// the supplied directories which of them is a prefix of the name the program
// was started by, and makes no assumption about how an absolute name is
// spelled: one environment writes a leading separator and another writes a
// volume first, and a suite that knew which would be a suite for one of them.
bool locate_self(kal_dir& base, const char*& relative, kal_uintptr& relative_len);

// Starts a copy with the given first element and errand, and reports how it
// ended. False means no copy could be started.
bool start_copy(const char* first_element, const char* errand_argument,
                int& status, int& terminated);

// The same, without awaiting it, for the observation that needs a copy still
// running when it is made.
bool start_copy_running(const char* first_element, const char* errand_argument,
                        kal_process& out);

}  // namespace okc
