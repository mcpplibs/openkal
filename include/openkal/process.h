/* openkal.process --- a program image that has been started. The operations
 * are to start one, to wait for it, and to request its termination.
 * Duplication of the calling image is not among them: duplicating an address
 * space and its execution state cannot be performed faithfully on every
 * environment this specification targets. */
#ifndef OPENKAL_PROCESS_H
#define OPENKAL_PROCESS_H
#include "types.h"
#include "fs.h"

struct kal_process { kal_uintptr h; };

/* How a started program's standard streams are supplied. A stream handle of
 * zero denotes that the program inherits the corresponding stream of its
 * parent, which is what an environment without a general mechanism for
 * passing handles can always provide. */
struct kal_spawn_streams {
    kal_uintptr in;
    kal_uintptr out;
    kal_uintptr err;
};

/* Positions in kal_process_props. */
#define KAL_PROCESS_PROP_TERMINATE      ((kal_uintptr)1u << 0)
#define KAL_PROCESS_PROP_STREAM_PASSING ((kal_uintptr)1u << 1)
#define KAL_PROCESS_PROP_EXIT_STATUS    ((kal_uintptr)1u << 2)

#ifdef __cplusplus
extern "C" {
#endif

/* Starts the program named relative to a directory. The argument vector is
 * complete and is passed unaltered: argv[0] is the name the started program
 * observes as its own, and an implementation neither derives it from path nor
 * prepends anything to the vector. Clause 7.6. */
int kal_process_spawn(struct kal_dir base,
                      const char*  path,  kal_uintptr path_len,
                      const char** argv,  const kal_uintptr* argv_lens, kal_uintptr argc,
                      const char** envp,  const kal_uintptr* envp_lens, kal_uintptr envc,
                      const struct kal_spawn_streams* streams,
                      struct kal_process* out);

int kal_process_wait(struct kal_process, int* status, int* terminated);
int kal_process_terminate(struct kal_process);
void kal_process_close(struct kal_process);

extern const kal_uintptr kal_process_props;

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_PROCESS_H */
