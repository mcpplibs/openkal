/* openkal.process --- a program image that has been started. The operations
 * are to start one, to wait for it, and to request its termination.
 * Duplication of the calling image is not among them: duplicating an address
 * space and its execution state cannot be performed faithfully on every
 * environment this specification targets. */
#ifndef OPENKAL_PROCESS_H
#define OPENKAL_PROCESS_H
#include "types.h"
#include "fs.h"
/* For `kal_process_channel', whose pair of streams is what a parent speaks to
 * a started program through. A sibling header, per the rule this specification
 * keeps for its own headers. */
#include "stream.h"

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

/* One directory a started program shall receive among its preopens. The layout
 * is frozen (clause 5.3). */
struct kal_preopen {
    struct kal_dir dir;
    const char*    name;
    kal_uintptr    len;
};

/* Positions in kal_process_props. */
#define KAL_PROCESS_PROP_TERMINATE      ((kal_uintptr)1u << 0)
#define KAL_PROCESS_PROP_STREAM_PASSING ((kal_uintptr)1u << 1)
#define KAL_PROCESS_PROP_EXIT_STATUS    ((kal_uintptr)1u << 2)
#define KAL_PROCESS_PROP_CHANNEL        ((kal_uintptr)1u << 3)
#define KAL_PROCESS_PROP_GRANT_DIR      ((kal_uintptr)1u << 4)

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

/* Starts a program that receives exactly the directories named.
 *
 * The started program reads them back through `kal_fs_preopen', which is the
 * operation this one is the inverse of --- clause 7.11. A count of zero starts
 * a program with no preopens at all, which is a different thing from
 * `kal_process_spawn' and is the whole reason a caller reaches for this.
 *
 * A SECOND DECLARATION RATHER THAN AN ARGUMENT ADDED TO THE FIRST, because
 * clause 8 forbids altering an existing one. `kal_process_spawn' remains, and a
 * program that does not grant directories keeps using it. */
int kal_process_spawn_with(struct kal_dir base,
                           const char*  path,  kal_uintptr path_len,
                           const char** argv,  const kal_uintptr* argv_lens, kal_uintptr argc,
                           const char** envp,  const kal_uintptr* envp_lens, kal_uintptr envc,
                           const struct kal_spawn_streams* streams,
                           const struct kal_preopen* grants, kal_uintptr grant_count,
                           struct kal_process* out);

/* A pair of streams of which one end is intended to cross a spawn boundary.
 *
 * The caller holds `mine'; `theirs' is what it places in a
 * `kal_spawn_streams'. Bytes written to one end are read from the other, and
 * the pair is the mechanism a parent uses to speak to a child it started ---
 * which openkal otherwise has no way to express, because the standard streams
 * are borrowed and cannot be manufactured.
 *
 * BOTH ENDS ARE OWNED, AND THE RELEASE IS HERE. A parent that does not release
 * `theirs' after the spawn never observes the end of input on `mine' --- the
 * classic deadlock of this arrangement --- so an interface that hands out these
 * streams must also take them back. `kal_stream' has no release of its own
 * precisely because a stream in general has no owner. */
int  kal_process_channel(struct kal_stream* mine, struct kal_stream* theirs);
void kal_process_channel_close(struct kal_stream s);

int kal_process_wait(struct kal_process, int* status, int* terminated);
int kal_process_terminate(struct kal_process);
void kal_process_close(struct kal_process);

extern const kal_uintptr kal_process_props;

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_PROCESS_H */
