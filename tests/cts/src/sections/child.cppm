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
    report_grants,       // the directories a starter granted are its preopens
    report_no_preopens,  // ... and a count of zero leaves it none (clause 7.13)
    end_as_requested,    // takes the request over, then ends as it would have (0.17)
    count_requests,      // takes the request over and observes two requests (0.25)
    map_beyond_shortened,    // reads a mapped page its file no longer holds (0.18)
    hold_memory,         // holds 32 MiB written through, then waits to be ended (0.20)
    grow_memory,         // grows by 8 MiB at a time to 512 MiB, then waits; ends
                         // itself with 77 after twenty seconds (0.20)
    read_and_write_null, // reads the end of input and writes in full at its three
                         // positions, given the null device there (0.22)
    report_resources,    // ends with 40 + kal_env_resources() (0.23)
    fault,               // writes through a null pointer, so that what ends it
                         // reports the fault (0.23)
    write_in_first_grant,    // creates a file in its first preopen: 70 refused
                             // as read-only, 71 created, 72 anything else (0.23)
    count_vectors,       // ends with 100, plus 1 for `vector_count' arguments, 2
                         // for `vector_count' variables of the expected name and
                         // value, 4 for an argument of `vector_long' bytes (0.26)
    declare_unit,        // declares its own unit: 80 a reference was made, 81
                         // kal_err_not_supported, 82 anything else (0.26)
};

// What the copy counts under `count_vectors' (clause 7.6): the number of
// arguments after the errand's and of variables, a variable's name and the
// length of its value, and the length of the one long argument.
inline constexpr kal_uintptr vector_count = 2000;
inline constexpr char        vector_name[] = "OKC_V";
inline constexpr kal_uintptr vector_value = 100;
inline constexpr kal_uintptr vector_long  = kal_uintptr{1} << 20;

// The names the starter grants and the copy expects, in order, and the marker
// each directory holds. The second name carries both separators an
// implementation might use to convey names, so that one that does not quote
// them is reported.
inline constexpr const char* grant_names[2]   = { "/okc-grant-a", "okc-grant-b;," };
inline constexpr const char* grant_markers[2] = { "A", "B" };
inline constexpr const char  grant_marker_file[] = "okc-grant-marker";

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

// The same, placing the copy in the unit `unit' names (a word of zero forms a
// new one and receives its identity).
bool start_copy_in_unit(const char* errand_argument, kal_job& unit, kal_process& out);

// The same, granting the copy the given directories.
bool start_copy_granting(const char* errand_argument, const kal_preopen* grants,
                         kal_uintptr count, int& status, int& terminated);

// The same, with the given flags and streams; `spawned' is what the start
// answered, and the copy is awaited only when it started (0.22).
bool start_copy_with(const char* errand_argument, kal_uintptr flags,
                     const kal_spawn_streams& streams, int& spawned,
                     int& status, int& terminated);

// The same, granting `count' directories, with the given flags and streams
// (0.23: an attenuated grant, KAL_SPAWN_CONFINE).
bool start_copy_granting_with(const char* errand_argument, const kal_preopen* grants,
                              kal_uintptr count, kal_uintptr flags, const kal_spawn_streams& streams,
                              int& spawned, int& status, int& terminated);

// The same, with the vector and the environment given whole (clause 7.6,
// 0.26): `argv[0]' and `argv[1]' are the copy's name and its errand.
bool start_copy_given(const char** argv, const kal_uintptr* lens, kal_uintptr count,
                      const char** env, const kal_uintptr* env_lens, kal_uintptr env_count,
                      int& spawned, int& status, int& terminated);

}  // namespace okc
