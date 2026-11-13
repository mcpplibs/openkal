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
export using ::kal_program_identity;
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
export using ::kal_process_job_usage;
export using ::kal_process_job_create;
export using ::kal_process_job_open;
export using ::kal_process_job_observe;
export using ::kal_process_job_retire;
export using ::kal_process_job_limit;
export using ::kal_job_usage;
export using ::kal_process_stop_requested;
export using ::kal_process_props;
export using ::kal_process_end_as_requested;
export using ::kal_program_info;
export using ::kal_process_self;
export using ::kal_process_open_id;
export using ::kal_process_info;

static_assert(sizeof(kal_process) == sizeof(kal_uintptr), "clause 7.2");
static_assert(sizeof(kal_program_identity) == 16, "clause 5.3");
static_assert(__builtin_offsetof(kal_program_identity, started_ns) == 8, "clause 5.3");
static_assert(sizeof(kal_spawn_streams) == 3 * sizeof(kal_uintptr), "clause 5.3");
static_assert(sizeof(kal_preopen) == 3 * sizeof(kal_uintptr), "clause 5.3");
// kal_program_info grows by appending, as kal_node_info does; the offsets of
// the fields that exist are what is frozen.
static_assert(__builtin_offsetof(kal_program_info, id) == 8, "clause 5.3");
static_assert(__builtin_offsetof(kal_program_info, started_ns) == 16, "clause 5.3");
static_assert(__builtin_offsetof(kal_program_info, cpu_ns) == 24, "clause 5.3");
static_assert(__builtin_offsetof(kal_program_info, ended) == 32, "clause 5.3");
static_assert(__builtin_offsetof(kal_program_info, memory_bytes) == 40, "clause 5.3");
static_assert(__builtin_offsetof(kal_program_info, memory_peak_bytes) == 48, "clause 5.3");
static_assert(__builtin_offsetof(kal_program_info, faults_minor) == 56, "clause 5.3");   // 0.23
static_assert(__builtin_offsetof(kal_program_info, faults_major) == 64, "clause 5.3");   // 0.23
static_assert(sizeof(kal_program_info) == 72, "clause 5.3");
static_assert(__builtin_offsetof(kal_job_usage, memory_bytes) == 8, "clause 5.3");
static_assert(sizeof(kal_job_usage) == 16, "clause 5.3");

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
inline constexpr props open_id       {KAL_PROCESS_PROP_OPEN_ID};
inline constexpr props one_string    {KAL_PROCESS_PROP_ONE_STRING};   // 0.17.1
inline constexpr props job_usage     {KAL_PROCESS_PROP_JOB_USAGE};    // 0.20
inline constexpr props job_limit_hard{KAL_PROCESS_PROP_JOB_LIMIT_HARD};             // 0.20
inline constexpr props job_limit_supervised{KAL_PROCESS_PROP_JOB_LIMIT_SUPERVISED}; // 0.20
inline constexpr props null_streams  {KAL_PROCESS_PROP_NULL_STREAMS};  // 0.22
inline constexpr props confine       {KAL_PROCESS_PROP_CONFINE};       // 0.23
inline constexpr props job_observe   {KAL_PROCESS_PROP_JOB_OBSERVE};   // 0.24
inline constexpr props runs_to_end   {KAL_PROCESS_PROP_RUNS_TO_END};   // 0.25.1
namespace job_state {
inline constexpr kal_u32 unknown = KAL_JOB_UNKNOWN;
inline constexpr kal_u32 occupied = KAL_JOB_OCCUPIED;
inline constexpr kal_u32 empty = KAL_JOB_EMPTY;
}

// The fields of kal_program_info, for `wanted' and `present' (0.17).
namespace info {
inline constexpr kal_u32 id      = KAL_PINFO_ID;
inline constexpr kal_u32 started = KAL_PINFO_STARTED;
inline constexpr kal_u32 cpu     = KAL_PINFO_CPU;
inline constexpr kal_u32 ended   = KAL_PINFO_ENDED;
inline constexpr kal_u32 memory  = KAL_PINFO_MEMORY;   // 0.20
inline constexpr kal_u32 faults  = KAL_PINFO_FAULTS;   // 0.23
inline constexpr kal_u32 major   = KAL_PINFO_MAJOR;    // 0.23
inline constexpr kal_u32 all     = KAL_PINFO_ALL;
}

// WHAT A CALLER ASKS FOR, WHICH IS A DIFFERENT WORD FROM WHAT AN
// IMPLEMENTATION CAN DO. `props' above answers the second; these set the first.
// A module consumer sees neither unless both are named here --- the C spellings
// are macros, and a macro is invisible across a module boundary.
struct spawn_flag_tag;
using spawn_flags = kal::props<spawn_flag_tag>;

inline constexpr spawn_flags bound_lifetime_flag{KAL_SPAWN_BOUND_LIFETIME};
inline constexpr spawn_flags one_string_flag{KAL_SPAWN_ONE_STRING};   // 0.17.1
inline constexpr spawn_flags null_in_flag {KAL_SPAWN_NULL_IN};    // 0.22
inline constexpr spawn_flags null_out_flag{KAL_SPAWN_NULL_OUT};   // 0.22
inline constexpr spawn_flags null_err_flag{KAL_SPAWN_NULL_ERR};   // 0.22
inline constexpr spawn_flags confine_flag {KAL_SPAWN_CONFINE};    // 0.23
inline constexpr spawn_flags balanced_flag   {KAL_SPAWN_RESOURCES_BALANCED};     // 0.23
inline constexpr spawn_flags performance_flag{KAL_SPAWN_RESOURCES_PERFORMANCE};  // 0.23
inline constexpr spawn_flags economy_flag    {KAL_SPAWN_RESOURCES_ECONOMY};      // 0.23

using how = kal_spawn;
using job_handle = kal_job;

// The kinds of limit a unit takes, and what `terminated' reports for a program
// a limit ended (0.20).
inline constexpr kal_uintptr limit_memory = KAL_JOB_LIMIT_MEMORY;
inline constexpr int terminated_limit = KAL_TERMINATED_LIMIT;

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
