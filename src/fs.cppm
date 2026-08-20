// openkal.fs --- directories and open files.
//
// Every operation is relative to a directory the program holds. There is no
// global namespace of paths.
//
// The reason is the admission criterion of clause 7.1. A global namespace is
// unavailable in a capability-based kernel, and an implementation upon one would
// have to construct it. Relative operations are natural everywhere considered:
// they are the primitive on capability systems, and on a hosted system they are
// the family of calls that take a directory, which a C library already uses to
// implement the global forms.
//
// The consequence is that resolving an absolute path is work performed by a C
// library against a root directory the environment supplies, once, rather than
// by each program or by this specification. It is also what allows a program to
// be confined without its cooperation.
module;
#include <openkal/fs.h>

export module openkal.fs;
export import openkal.types;

// A directory, or an open file. Both are owned: the program obtained them and
// releases them.
export using ::kal_dir;
export using ::kal_file;

// What a name refers to, and what is known about it.
export using ::kal_node_kind;
export using ::kal_node_absent;
export using ::kal_node_file;
export using ::kal_node_directory;
export using ::kal_node_link;
export using ::kal_node_other;
export using ::kal_node_info;

export using ::kal_fs_preopen_count;
export using ::kal_fs_preopen;
export using ::kal_fs_open_dir;
export using ::kal_fs_open_file;
export using ::kal_fs_open;
export using ::kal_fs_close_dir;
export using ::kal_fs_close_file;
export using ::kal_fs_stream;
export using ::kal_fs_seek;
export using ::kal_fs_truncate;
export using ::kal_fs_info;
export using ::kal_fs_file_info;
export using ::kal_fs_set_modified;
export using ::kal_fs_mkdir;
export using ::kal_fs_remove;
export using ::kal_fs_rename;
export using ::kal_fs_list_begin;
export using ::kal_fs_list_next;
export using ::kal_fs_props;

static_assert(sizeof(kal_dir)  == sizeof(kal_uintptr), "clause 7.2");
static_assert(sizeof(kal_file) == sizeof(kal_uintptr), "clause 7.2");
// Clause 5.3: the layout is frozen. An implementation and a consumer built at
// different times must agree on where each field is, and nothing else reports
// a disagreement.
static_assert(__builtin_offsetof(kal_node_info, size) == 0);

// ⚠️ `8`, NOT `sizeof(kal_uintptr)`, AND THE DIFFERENCE IS AN ABI FACT RATHER
// THAN A SPELLING.
//
// `modified_ns` is a `kal_u64` and is naturally aligned. On a 64-bit target
// `size` occupies eight bytes and the timestamp follows at eight. On a 32-bit
// target `size` occupies four, the compiler inserts four bytes of padding, and
// the timestamp still lands at eight. The offset is therefore the SAME on both
// widths, which is exactly the property a frozen layout wants — and
// `sizeof(kal_uintptr)` described only the 64-bit case.
//
// Measured on `riscv32-none-elf`:
//
//     fs.cppm:63: static assertion failed due to requirement
//     '__builtin_offsetof(kal_node_info, modified_ns) == sizeof(unsigned int)'
//
// ⚠️ Nothing in this repository's own CI could have found it: openkal is built
// and tested hosted, and every hosted target it serves is 64-bit. The failure
// surfaced in a consumer — `mcpplibs/riscv-virt-rt`, whose rv32 leg activates
// the `openkal` feature — which is the only place the 32-bit layout is
// instantiated at all.
//
// `sizeof(kal_node_info)` is 24 on both widths for the same reason: 8+8+4+4 and
// 4+4(padding)+8+4+4.
static_assert(__builtin_offsetof(kal_node_info, modified_ns) == 8);
static_assert(sizeof(kal_node_info) == 24);

export namespace kal::fs {

using dir  = kal_dir;
using file = kal_file;
using info = kal_node_info;

struct props_tag;
using props = kal::props<props_tag>;

inline constexpr props case_sensitive{KAL_FS_PROP_CASE_SENSITIVE};
inline constexpr props links         {KAL_FS_PROP_LINKS};
inline constexpr props modified_time {KAL_FS_PROP_MODIFIED_TIME};
inline constexpr props atomic_rename {KAL_FS_PROP_ATOMIC_RENAME};

enum : int { seek_set = KAL_SEEK_SET, seek_current = KAL_SEEK_CURRENT, seek_end = KAL_SEEK_END };

// The flags of kal_fs_open, composable and distinguishable from a capability
// word: an intent and a property are different kinds of thing, and a word that
// serves as both is a word a program can pass to the wrong operation.
struct open_tag;
using open_flags = kal::props<open_tag>;

namespace open {
inline constexpr open_flags read     {KAL_OPEN_READ};
inline constexpr open_flags write    {KAL_OPEN_WRITE};
inline constexpr open_flags create   {KAL_OPEN_CREATE};
inline constexpr open_flags exclusive{KAL_OPEN_EXCLUSIVE};
inline constexpr open_flags truncate {KAL_OPEN_TRUNCATE};
inline constexpr open_flags append   {KAL_OPEN_APPEND};
}

inline int open_file(dir base, const char* name, kal_uintptr len,
                     open_flags flags, file* out) {
    return kal_fs_open(base, name, len, flags.bits, out);
}

inline kal_uintptr preopen_count() { return kal_fs_preopen_count(); }

// The first entry, which every implementation supplies and which denotes the
// directory the program was started in.
inline dir working() {
    dir d{}; const char* n = nullptr; kal_uintptr l = 0;
    kal_fs_preopen(0, &d, &n, &l);
    return d;
}

inline props properties() { return props{kal_fs_props}; }
inline bool  has(props p) { return properties().has(p); }

}
