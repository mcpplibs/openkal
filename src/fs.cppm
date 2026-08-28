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
export using ::kal_fs_max_name;
export using ::kal_fs_link_create;
export using ::kal_fs_link_read;

static_assert(sizeof(kal_dir)  == sizeof(kal_uintptr), "clause 7.2");
static_assert(sizeof(kal_file) == sizeof(kal_uintptr), "clause 7.2");
// Clause 5.3: the layout is frozen. An implementation and a consumer built at
// different times must agree on where each field is, and nothing else reports
// a disagreement.
//
// ⚠️ EVERY OFFSET IS THE SAME ON A THIRTY-TWO AND A SIXTY-FOUR BIT TARGET, AND
// THAT IS A PROPERTY THAT HAD TO BE DESIGNED FOR RATHER THAN OBSERVED.
//
// Version 0.8 held the size in a `kal_uintptr` and relied on the compiler's
// padding to place the timestamp at the same offset on both widths. It worked,
// and it worked by coincidence: it stopped working the moment a second word of
// pointer width was added in front. Measured on `riscv32-none-elf` when the
// earlier form met a consumer that instantiates the 32-bit layout ---
// `mcpplibs/riscv-virt-rt`, whose rv32 leg is the only place it exists ---
// which is also where the earlier form's offset assertion first failed.
//
// So the fields are fixed widths, and the two words of positions are `kal_u32`.
// Forty-eight bytes on both widths, by construction rather than by arithmetic
// that happens to agree.
static_assert(__builtin_offsetof(kal_node_info, self_size)   ==  0);
static_assert(__builtin_offsetof(kal_node_info, present)     ==  4);
static_assert(__builtin_offsetof(kal_node_info, size)        ==  8);
static_assert(__builtin_offsetof(kal_node_info, modified_ns) == 16);
static_assert(__builtin_offsetof(kal_node_info, identity)    == 24);
static_assert(__builtin_offsetof(kal_node_info, kind)        == 40);
static_assert(__builtin_offsetof(kal_node_info, writable)    == 44);
static_assert(sizeof(kal_node_info) == 48);

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
inline constexpr props make_links    {KAL_FS_PROP_MAKE_LINKS};

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
    dir d{}; kal_uintptr l = 0;
    kal_fs_preopen(0, &d, nullptr, 0, &l);
    return d;
}

// The properties of the volume a directory is on. An enquiry taking the
// resource, because every position is a property of the format rather than of
// the environment: one machine mounts a case-sensitive volume beside a
// case-insensitive one.
inline props properties(dir d) { return props{kal_fs_props(d)}; }
inline bool  has(dir d, props p) { return properties(d).has(p); }

inline kal_uintptr max_name() { return kal_fs_max_name(); }

// The fields an enquiry may ask for and may report. A macro is not exportable
// from a module, and a consumer that reaches for one has to include the header
// it came from --- which is the seam clause 4.2 exists to remove.
namespace field {
inline constexpr kal_u32 kind     = KAL_INFO_KIND;
inline constexpr kal_u32 size     = KAL_INFO_SIZE;
inline constexpr kal_u32 modified = KAL_INFO_MODIFIED;
inline constexpr kal_u32 writable = KAL_INFO_WRITABLE;
inline constexpr kal_u32 identity = KAL_INFO_IDENTITY;
inline constexpr kal_u32 all      = KAL_INFO_ALL;
}

// Positions in the flags word of an enquiry.
inline constexpr kal_uintptr no_resolve = KAL_FS_NO_RESOLVE;
inline constexpr kal_uintptr link_directory = KAL_FS_LINK_DIRECTORY;

// An enquiry structure that states how much of itself exists on this side.
inline kal_node_info info_for_caller() {
    kal_node_info v{}; v.self_size = sizeof v; return v;
}

}
