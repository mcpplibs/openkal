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
module;
#include <openkal/process.h>

export module openkal.process;
export import openkal.types;
export import openkal.fs;
// For kal_process_channel, whose pair of streams is what a parent speaks to a
// started program through.
export import openkal.stream;

export using ::kal_process;
export using ::kal_spawn_streams;
export using ::kal_spawn;
export using ::kal_job;
export using ::kal_preopen;

export using ::kal_process_spawn;
export using ::kal_process_channel;
export using ::kal_process_channel_close;
export using ::kal_process_wait;
export using ::kal_process_terminate;
export using ::kal_process_close;
export using ::kal_process_job_enter;
export using ::kal_process_job_terminate;
export using ::kal_process_job_close;
export using ::kal_process_stop_requested;
export using ::kal_process_props;

static_assert(sizeof(kal_process) == sizeof(kal_uintptr), "clause 7.2");
static_assert(sizeof(kal_spawn_streams) == 3 * sizeof(kal_uintptr), "clause 5.3");
static_assert(sizeof(kal_preopen) == 3 * sizeof(kal_uintptr), "clause 5.3");

export namespace kal::process {

using process = kal_process;
using streams = kal_spawn_streams;

struct props_tag;
using props = kal::props<props_tag>;

inline constexpr props terminate     {KAL_PROCESS_PROP_TERMINATE};
inline constexpr props stream_passing{KAL_PROCESS_PROP_STREAM_PASSING};
inline constexpr props exit_status   {KAL_PROCESS_PROP_EXIT_STATUS};
inline constexpr props channel       {KAL_PROCESS_PROP_CHANNEL};
inline constexpr props grant_dir     {KAL_PROCESS_PROP_GRANT_DIR};
inline constexpr props bound_lifetime{KAL_PROCESS_PROP_BOUND_LIFETIME};
inline constexpr props job           {KAL_PROCESS_PROP_JOB};
inline constexpr props stop_requested{KAL_PROCESS_PROP_STOP_REQUESTED};

// ⚠️ WHAT A CALLER ASKS FOR, WHICH IS A DIFFERENT WORD FROM WHAT AN
// IMPLEMENTATION CAN DO. `props' above answers the second; these set the first.
// A module consumer sees neither unless both are named here --- the C spellings
// are macros, and a macro is invisible across a module boundary.
struct spawn_flag_tag;
using spawn_flags = kal::props<spawn_flag_tag>;

inline constexpr spawn_flags bound_lifetime_flag{KAL_SPAWN_BOUND_LIFETIME};

using how = kal_spawn;
using job_handle = kal_job;

static_assert(sizeof(kal_job) == sizeof(kal_uintptr), "clause 7.2");
static_assert(sizeof(kal_spawn) == 6 * sizeof(kal_uintptr), "clause 5.3");

inline props properties() { return props{kal_process_props()}; }
inline bool  has(props p) { return properties().has(p); }

using preopen = kal_preopen;

// Both ends of a channel, returned together. A parent that does not release
// `theirs' after the spawn never observes the end of input on `mine', so the
// two are handed over as one value to make the pairing hard to overlook.
struct channel_result { kal_stream mine; kal_stream theirs; int e; };

inline channel_result channel_open() {
    kal_stream mine{}, theirs{};
    const int e = kal_process_channel(&mine, &theirs);
    return { mine, theirs, e };
}

inline void channel_close(kal_stream s) { kal_process_channel_close(s); }

}
