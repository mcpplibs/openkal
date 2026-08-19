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
export module openkal.fs;
export import openkal.types;

// A directory, or an open file. Both are owned: the program obtained them and
// releases them.
export struct kal_dir  { kal_uintptr h; };
export struct kal_file { kal_uintptr h; };

// What a name refers to, and what is known about it.
export enum kal_node_kind : int {
    kal_node_absent    = 0,
    kal_node_file      = 1,
    kal_node_directory = 2,
    kal_node_link      = 3,
    kal_node_other     = 4,   // a device, a socket, or anything the environment does not classify
};

export struct kal_node_info {
    kal_uintptr    size;
    __UINT64_TYPE__ modified_ns;   // wall time, as openkal.time defines it
    int            kind;
    int            writable;
};

export extern "C" {

// The directories the environment supplied at inception, and the only ones a
// program can reach: every other directory is opened relative to one of these.
//
// A set rather than a single directory. One directory is insufficient, and the
// insufficiency is not hypothetical: a program that starts another must reach
// the program it starts, and the two are commonly not beneath one root. An
// interface offering a single directory would oblige such a program to receive
// the whole file system as its root, which would defeat the confinement the
// arrangement exists to provide.
//
// Each has a name, which is how the environment identifies it and how a C
// library above openkal decides which one an absolute path belongs to. The
// names are the environment's, and this specification does not constrain them
// beyond requiring that they be distinct.
kal_uintptr kal_fs_preopen_count(void);
int         kal_fs_preopen(kal_uintptr index, kal_dir* out,
                           const char** name, kal_uintptr* len);

// Opening. A name is a single component or a sequence separated by a forward
// slash; it shall not begin with a separator and shall not contain a component
// that ascends. An implementation shall reject a name that does, because a
// program that could ascend from the directory it was given would not be
// confined by having been given it.
int kal_fs_open_dir (kal_dir base, const char* name, kal_uintptr len, kal_dir*  out);
int kal_fs_open_file(kal_dir base, const char* name, kal_uintptr len, int write, int create, kal_file* out);

// Release. An implementation shall not treat a released handle as valid.
void kal_fs_close_dir (kal_dir);
void kal_fs_close_file(kal_file);

// A file is read and written through openkal.stream. This function obtains the
// stream, which remains valid while the file is open and is not separately
// released; the file owns it.
kal_uintptr kal_fs_stream(kal_file);

// Positioning. It appears here and not in openkal.stream because whether a
// stream can be repositioned is a property of the individual stream on a hosted
// system, and an interface that offered it on every stream would contain an
// operation some of its resources can never satisfy.
int kal_fs_seek(kal_file, __INT64_TYPE__ offset, int whence, __UINT64_TYPE__* result);

// Enquiry, creation and removal, all relative to a directory.
//
// Enquiry about a name that does not exist succeeds and reports kal_node_absent
// rather than failing: a caller that asks what a name refers to has been
// answered when told that it refers to nothing. Clause 7.7. Opening is access
// rather than enquiry and reports kal_err_not_found instead.
int kal_fs_info  (kal_dir base, const char* name, kal_uintptr len, kal_node_info* out);
int kal_fs_mkdir (kal_dir base, const char* name, kal_uintptr len);
int kal_fs_remove(kal_dir base, const char* name, kal_uintptr len);
int kal_fs_rename(kal_dir from, const char* a, kal_uintptr alen,
                  kal_dir to,   const char* b, kal_uintptr blen);

// Enumeration. The iterator is owned and is released by reading past the end or
// by closing the directory that produced it.
int kal_fs_list_begin(kal_dir, kal_uintptr* iter);
int kal_fs_list_next (kal_dir, kal_uintptr* iter, const char** name, kal_uintptr* len, int* kind);

// Properties of this implementation's file system.
extern const kal_uintptr kal_fs_props;

}

export namespace kal::fs {

using dir  = kal_dir;
using file = kal_file;
using info = kal_node_info;

enum : kal_uintptr {
    prop_case_sensitive   = 1u << 0,
    prop_links            = 1u << 1,   // openkal.fs.links is provided
    prop_modified_time    = 1u << 2,   // kal_node_info::modified_ns is meaningful
    prop_atomic_rename    = 1u << 3,
};

enum : int { seek_set = 0, seek_current = 1, seek_end = 2 };

inline kal_uintptr preopen_count() { return kal_fs_preopen_count(); }

// The first entry, which every implementation supplies and which denotes the
// directory the program was started in.
inline dir working() {
    dir d{}; const char* n = nullptr; kal_uintptr l = 0;
    kal_fs_preopen(0, &d, &n, &l);
    return d;
}
inline bool has(kal_uintptr p) { return (kal_fs_props & p) != 0; }

}
