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
    __UINT64_TYPE__ modified_ns;   /* wall time, as openkal.time defines it */
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
 * component that ascends. */
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
int kal_fs_seek(struct kal_file, __INT64_TYPE__ offset, int whence,
                __UINT64_TYPE__* result);

/* Sets the length of an open file, extending it with zero bytes or discarding
 * what lies beyond. */
int kal_fs_truncate(struct kal_file, __UINT64_TYPE__ size);

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
