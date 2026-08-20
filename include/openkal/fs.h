/* openkal.fs --- directories and open files.
 *
 * Every operation is relative to a directory the program holds. There is no
 * global namespace of paths: a global namespace is unavailable in a
 * capability-based kernel and an implementation upon one would have to
 * construct it, which clause 7.1 excludes. Resolving an absolute path is
 * therefore work a C library performs against a directory the environment
 * supplied, once, rather than work each program performs. */
#ifndef OPENKAL_FS_H
#define OPENKAL_FS_H
#include "types.h"

/* A directory, or an open file. Both are owned: the program obtained them and
 * releases them. */
struct kal_dir  { kal_uintptr h; };
struct kal_file { kal_uintptr h; };

/* What a name refers to. */
enum kal_node_kind {
    kal_node_absent    = 0,
    kal_node_file      = 1,
    kal_node_directory = 2,
    kal_node_link      = 3,
    kal_node_other     = 4
};

struct kal_node_info {
    kal_uintptr     size;
    kal_u64 modified_ns;   /* wall time, as openkal.time defines it */
    int             kind;
    int             writable;
};

/* Positions in kal_fs_props. */
#define KAL_FS_PROP_CASE_SENSITIVE ((kal_uintptr)1u << 0)
#define KAL_FS_PROP_LINKS          ((kal_uintptr)1u << 1)
#define KAL_FS_PROP_MODIFIED_TIME  ((kal_uintptr)1u << 2)
#define KAL_FS_PROP_ATOMIC_RENAME  ((kal_uintptr)1u << 3)

/* Positions in the flags word of kal_fs_open. */
#define KAL_OPEN_READ      ((kal_uintptr)1u << 0)
#define KAL_OPEN_WRITE     ((kal_uintptr)1u << 1)
#define KAL_OPEN_CREATE    ((kal_uintptr)1u << 2)
#define KAL_OPEN_EXCLUSIVE ((kal_uintptr)1u << 3)  /* fail if the name exists */
#define KAL_OPEN_TRUNCATE  ((kal_uintptr)1u << 4)
#define KAL_OPEN_APPEND    ((kal_uintptr)1u << 5)

/* Positions in the whence argument of kal_fs_seek. */
#define KAL_SEEK_SET 0
#define KAL_SEEK_CURRENT 1
#define KAL_SEEK_END 2

#ifdef __cplusplus
extern "C" {
#endif

/* The directories the environment supplied at inception, and the only ones a
 * program can reach. A set rather than a single directory: a program that
 * starts another must reach the program it starts, and the two are commonly
 * not beneath one root.
 *
 * Each has a name, which is how the environment identifies it and how a C
 * library above openkal decides which one an absolute path belongs to. The
 * names are the environment's; this specification requires only that they be
 * distinct. An implementation whose environment has a global path namespace
 * reports names drawn from it, so that a C library can both resolve an
 * absolute path and report one. */
kal_uintptr kal_fs_preopen_count(void);
int         kal_fs_preopen(kal_uintptr index, struct kal_dir* out,
                           const char** name, kal_uintptr* len);

/* Opening. A name is a single component or a sequence separated by a forward
 * slash; it shall not begin with a separator and shall not contain a
 * component that ascends.
 *
 * One name is reserved: "." denotes the directory itself. Without it a program
 * holding a directory has no way to ask an operation about that directory ---
 * what it is, when it changed, whether it can be written --- and the operations
 * that answer those questions all take a name. It is one reserved word rather
 * than five more operations, every environment can express it, and it does not
 * introduce a way to ascend. Clause 7.12. */
int kal_fs_open_dir (struct kal_dir base, const char* name, kal_uintptr len,
                     struct kal_dir* out);
int kal_fs_open_file(struct kal_dir base, const char* name, kal_uintptr len,
                     int write, int create, struct kal_file* out);

/* Opening, stating the whole of the intent in one word.
 *
 * The two-flag form above cannot express three conditions a C library must
 * express, and each of them, emulated, leaves the caller silently wrong rather
 * than merely bounded: truncation performed after opening leaves the tail of
 * a shorter rewrite behind if the program stops in between; exclusion tested
 * before opening is not exclusion; and appending performed by seeking is not
 * appending when a second writer exists. Clause 3.1 classifies each of those
 * as a simulation, so the specification states the intent instead. */
int kal_fs_open(struct kal_dir base, const char* name, kal_uintptr len,
                kal_uintptr flags, struct kal_file* out);

/* Release. An implementation shall not treat a released handle as valid. */
void kal_fs_close_dir (struct kal_dir);
void kal_fs_close_file(struct kal_file);

/* A file is read and written through openkal.stream. The stream remains valid
 * while the file is open and is not separately released; the file owns it. */
kal_uintptr kal_fs_stream(struct kal_file);

/* Positioning. It appears here and not in openkal.stream because on a hosted
 * system whether a stream can be repositioned is a property of the individual
 * stream, and an interface offering it on every stream would contain an
 * operation some of its resources can never satisfy. */
int kal_fs_seek(struct kal_file, kal_i64 offset, int whence,
                kal_u64* result);

/* Sets the length of an open file, extending it with zero bytes or discarding
 * what lies beyond. */
int kal_fs_truncate(struct kal_file, kal_u64 size);

/* Enquiry, creation and removal, all relative to a directory. Enquiry about a
 * name that does not exist succeeds and reports kal_node_absent rather than
 * failing: a caller that asks what a name refers to has been answered when
 * told that it refers to nothing. Clause 7.7. */
int kal_fs_info  (struct kal_dir base, const char* name, kal_uintptr len,
                  struct kal_node_info* out);
int kal_fs_mkdir (struct kal_dir base, const char* name, kal_uintptr len);
int kal_fs_remove(struct kal_dir base, const char* name, kal_uintptr len);
int kal_fs_rename(struct kal_dir from, const char* a, kal_uintptr alen,
                  struct kal_dir to,   const char* b, kal_uintptr blen);

/* Enquiry about an open file. It is not expressible through kal_fs_info: the
 * name a file was opened by may since have been removed or reused, and a C
 * library answering fstat from the name would answer about a different file. */
int kal_fs_file_info(struct kal_file, struct kal_node_info* out);

/* Sets the time kal_fs_file_info reports for an open file.
 *
 * The inverse of an enquiry that already exists, and the interface is
 * incomplete without it: a program that copies a file and preserves its dates,
 * or that extracts an archive, or that marks a file as current, has no way to
 * say so. Each of those is a program a C library above openkal is expected to
 * host, and none of them can be written from the operations above.
 *
 * The file rather than the name, for the reason stated at kal_fs_file_info: the
 * name may since refer to something else, and setting the time of the wrong
 * file is worse than not setting it.
 *
 * The file shall have been opened with KAL_OPEN_WRITE. One environment decides
 * at the point of opening what may afterwards be done with the file, and cannot
 * be asked later; requiring the intent to be stated when the file is opened is
 * the same rule clause 7.8 states for the other three conditions.
 *
 * An implementation whose environment does not record a modification time does
 * not claim KAL_FS_PROP_MODIFIED_TIME and reports kal_err_not_supported here.
 * An implementation that claims the position shall be able to perform this. */
int kal_fs_set_modified(struct kal_file, kal_u64 modified_ns);

/* Enumeration. The iterator is owned and is released by reading past the end
 * or by closing the directory that produced it. */
int kal_fs_list_begin(struct kal_dir, kal_uintptr* iter);
int kal_fs_list_next (struct kal_dir, kal_uintptr* iter, const char** name,
                      kal_uintptr* len, int* kind);

extern const kal_uintptr kal_fs_props;

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_FS_H */
