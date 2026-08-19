// openkal.process --- a program image that has been started.
//
// The operations are to start one, to wait for it, and to request its
// termination. Duplication of the calling image is not among them.
//
// The omission follows from clause 7.1 rather than from preference. Duplicating
// an address space and its execution state cannot be performed faithfully on
// every environment this specification targets, and an implementation obliged to
// reproduce it would be constructing a compatibility layer. The observed
// behaviour of a large portable program corroborates the choice: it starts its
// subordinate programs by spawning them and calls neither of the duplicating
// operations.
export module openkal.process;
export import openkal.types;
export import openkal.fs;

export struct kal_process { kal_uintptr h; };

// How a started program's standard streams are supplied. A stream handle of
// zero denotes that the program inherits the corresponding stream of its
// parent, which is what an environment without a general mechanism for passing
// handles can always provide.
export struct kal_spawn_streams {
    kal_uintptr in;
    kal_uintptr out;
    kal_uintptr err;
};

export extern "C" {

// Starts the program named relative to a directory, with the given arguments
// and named values. Both are passed as counted arrays of counted strings, so
// that the interface requires neither a terminator convention nor a string
// function.
//
// The started program's working directory is the directory supplied here, and
// there is no operation that changes it afterwards: a working directory that
// can be changed is shared mutable state between execution contexts, and the
// specification declines to impose a synchronisation rule upon it.
// The argument vector is complete and is passed unaltered: argv[0] is the name
// the started program observes as its own, and an implementation neither
// derives it from `path` nor prepends anything to the vector. Clause 7.6. The
// two sides must agree, because the started program reads argv[0] through
// kal_env_arg(0).
int kal_process_spawn(kal_dir base,
                      const char*  path,     kal_uintptr path_len,
                      const char** argv,     const kal_uintptr* argv_lens, kal_uintptr argc,
                      const char** envp,     const kal_uintptr* envp_lens, kal_uintptr envc,
                      const kal_spawn_streams* streams,
                      kal_process* out);

// Waits for the program to finish and reports the status it finished with. A
// program terminated by the environment reports a status this specification does
// not interpret beyond its being distinguishable from an ordinary status.
int kal_process_wait(kal_process, int* status, int* terminated);

// Requests termination. An implementation that cannot terminate a program
// reports that the operation is unsupported rather than reporting success.
int kal_process_terminate(kal_process);

// Release. Releasing a handle does not affect the program it refers to; a
// program that has not been waited for continues, and an implementation that
// must collect it does so.
void kal_process_close(kal_process);

extern const kal_uintptr kal_process_props;

}

export namespace kal::process {

using process = kal_process;
using streams = kal_spawn_streams;

enum : kal_uintptr {
    prop_terminate      = 1u << 0,   // termination can be requested
    prop_stream_passing = 1u << 1,   // streams other than the parent's can be supplied
    prop_exit_status    = 1u << 2,   // the status is the value the program returned
};

inline bool has(kal_uintptr p) { return (kal_process_props & p) != 0; }

}
