/* openkal.fs --- directories and open files.
 *
 * Every operation is relative to a directory the program holds. There is no
 * global namespace of paths: a global namespace is unavailable in a
 * capability-based kernel and an implementation upon one would have to
 * construct it, which clause 7.1 excludes. Resolving an absolute path is
 * therefore work a C library performs against a directory the environment
 * supplied, once, rather than work each program performs.
 *
 * ⭐ WHAT A NAME REFERS TO, AND WHAT A NAME FINALLY REFERS TO, ARE TWO
 * QUESTIONS. A filesystem may hold a node whose content is another name.
 * `kal_fs_info' answers the second by default and the first when asked; the
 * operations that OPEN answer the second always. Clause 11 item 7 stated this
 * and no header did, and two call sites of one implementation consequently
 * chose opposite directions --- so it is stated here, beside each operation it
 * governs, rather than only in the specification.
 */
#ifndef OPENKAL_FS_H
#define OPENKAL_FS_H
#include "types.h"
#include "stream.h"

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

/* What is known about a node.
 *
 * ⭐⭐ THREE MECHANISMS, BECAUSE THERE ARE THREE QUESTIONS, AND CONFLATING THEM
 * WAS THE DEFECT THIS REPLACES.
 *
 *   `self_size'  the caller sets it to `sizeof(struct kal_node_info)' as the
 *                caller knows it, and an implementation writes no more than
 *                that many bytes. This answers HOW MUCH OF THIS STRUCTURE
 *                EXISTS ON YOUR SIDE, so a consumer built against a later
 *                revision may ask an earlier implementation without reading its
 *                own uninitialised memory.
 *
 *   `present'    the implementation sets one position per field it filled. This
 *                answers WHICH OF IT IS TRUE FOR THIS RESOURCE. Whether a node
 *                has an identity, or a modification time, is a property of the
 *                format the node is on and not of the implementation, which
 *                clause 6.2 says can be neither an interface nor a word.
 *
 *   `wanted'     the caller passes it to the operation. This answers WHICH OF
 *                IT I NEED, so an implementation need not compute an identity
 *                for a caller that asked for a size.
 *
 * ⚠️ AN IMPLEMENTATION THAT ALWAYS HAS EVERYTHING IGNORES `wanted' AND WRITES A
 * CONSTANT INTO `present'. That is one line, and if it is not one line the shape
 * is wrong. */
struct kal_node_info {
    kal_u32 self_size;    /* set by the caller before the call                */
    kal_u32 present;      /* set by the implementation: which fields hold     */
    kal_u64 size;         /* a length is not a property of a machine word     */
    kal_u64 modified_ns;  /* wall time, as openkal.time defines it            */
    kal_u64 identity[2];  /* see KAL_INFO_IDENTITY                            */
    int     kind;
    int     writable;
};

/* ⚠️ EVERY FIELD IS A FIXED WIDTH, AND `self_size' AND `present' ARE `kal_u32'
 * FOR THE REASON `kal_endpoint.addr_len' IS. Clause 5.3 freezes this layout, so
 * a machine word here would freeze a difference between a thirty-two and a
 * sixty-four bit target that nothing in the structure needs: the size of a
 * structure is not a pointer, and thirty-two positions is more than this
 * enquiry will assign. The layout is consequently forty-eight bytes on both
 * widths, and a consumer and an implementation built for different widths of
 * the same target agree on where each field is. A word of positions that is
 * only ever RETURNED --- every `kal_<interface>_props' --- keeps the machine
 * word, because a register has no layout to freeze. */

/* Positions in `wanted' and in `present'. A position, once assigned, retains
 * its meaning; a position that has not been assigned reads as zero. */
#define KAL_INFO_KIND     ((kal_u32)1u << 0)
#define KAL_INFO_SIZE     ((kal_u32)1u << 1)
#define KAL_INFO_MODIFIED ((kal_u32)1u << 2)
#define KAL_INFO_WRITABLE ((kal_u32)1u << 3)

/* The identity of the node, as two words.
 *
 * ⭐ OPAQUE, AND COMPARABLE, AND NOTHING ELSE. Two nodes are the same node when
 * both words are equal. The words are not interpretable, not ordered, and are
 * not required to survive a restart. An implementation whose environment has an
 * inode, a file index or an object identifier uses it; one that cannot
 * distinguish nodes leaves this position clear in `present', and a caller is
 * then TOLD that it does not know rather than being told that two different
 * nodes are the same.
 *
 * It presupposes no principal and no format feature: no resource can fail to
 * answer whether it is the same resource as another, which is why this is
 * admissible where a permission is not (clause 6.4, clause 11 item 6). */
#define KAL_INFO_IDENTITY ((kal_u32)1u << 4)

#define KAL_INFO_ALL (KAL_INFO_KIND | KAL_INFO_SIZE | KAL_INFO_MODIFIED \
                    | KAL_INFO_WRITABLE | KAL_INFO_IDENTITY)

/* Positions in the flags word of kal_fs_info. */
#define KAL_FS_NO_RESOLVE ((kal_uintptr)1u << 0)  /* the name itself, not what
                                                   * it finally refers to     */

/* Positions in the result of kal_fs_props.
 *
 * ⚠️ AN ENQUIRY TAKING A DIRECTORY, NOT A WORD PER IMPLEMENTATION. Every
 * position below is a property of the FORMAT a resource is on and not of the
 * environment: one machine mounts a case-sensitive volume beside a
 * case-insensitive one, a volume with links beside one without, and a rename
 * that is atomic within a volume and not across two. A single word per
 * implementation could state none of them honestly --- measured: one
 * implementation claimed case sensitivity unconditionally while offering a
 * preopen on a volume that has none, and no implementation ever claimed links
 * while all three met them. Clause 6.2 names this case and names the remedy. */
#define KAL_FS_PROP_CASE_SENSITIVE ((kal_uintptr)1u << 0)
#define KAL_FS_PROP_LINKS          ((kal_uintptr)1u << 1)
#define KAL_FS_PROP_MODIFIED_TIME  ((kal_uintptr)1u << 2)
#define KAL_FS_PROP_ATOMIC_RENAME  ((kal_uintptr)1u << 3)
#define KAL_FS_PROP_MAKE_LINKS     ((kal_uintptr)1u << 4)  /* kal_fs_link_create
                                                            * is answered here */

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
 * distinct.
 *
 * ⭐ THE NAME IS COPIED, AND `name_len' REPORTS THE LENGTH IT HAS. An operation
 * that produces a RESOURCE returns `int' and writes the resource; the length of
 * a name it also produces goes in an out-parameter, because the return is
 * already spoken for. Where an operation's whole result is a length, the length
 * is the return (`kal_env_arg', and the transfers). */
kal_uintptr kal_fs_preopen_count(void);
int         kal_fs_preopen(kal_uintptr index, struct kal_dir* out,
                           char* name_out, kal_uintptr name_cap,
                           kal_uintptr* name_len);

/* Properties of the volume a directory is on. See the positions above. */
kal_uintptr kal_fs_props(struct kal_dir);

/* Opening. A name is a single component or a sequence separated by a forward
 * slash; it shall not begin with a separator and shall not contain a component
 * that ascends. A name shall be at most `kal_fs_max_name()' bytes.
 *
 * One name is reserved: "." denotes the directory itself. Without it a program
 * holding a directory has no way to ask an operation about that directory ---
 * what it is, when it changed, whether it can be written --- and the operations
 * that answer those questions all take a name. It is one reserved word rather
 * than five more operations, every environment can express it, and it does not
 * introduce a way to ascend. Clause 7.12.
 *
 * ⭐ OPENING RESOLVES. Where a component of the name, or the name itself, is a
 * node whose content is another name, these operations act upon what it finally
 * refers to. A caller that wants the node itself asks `kal_fs_info' with
 * KAL_FS_NO_RESOLVE and `kal_fs_link_read'; there is no form of opening that
 * declines to resolve, because a program that opens a link in order to read its
 * bytes is asking the question `kal_fs_link_read' answers. */
int kal_fs_open_dir(struct kal_dir base, const char* name, kal_uintptr len,
                    struct kal_dir* out);

/* Opening a file, stating the whole of the intent in one word.
 *
 * Truncation performed after opening leaves the tail of a shorter rewrite
 * behind if the program stops in between; exclusion tested before opening is
 * not exclusion; and appending performed by seeking is not appending when a
 * second writer exists. Clause 3.1 classifies each of those as a simulation, so
 * the specification states the intent instead. */
int kal_fs_open(struct kal_dir base, const char* name, kal_uintptr len,
                kal_uintptr flags, struct kal_file* out);

/* The greatest length, in bytes, of a name this implementation accepts.
 *
 * ⚠️ A BOUND A CALLER CANNOT LEARN PRODUCES A FAILURE THE CALLER CANNOT
 * ATTRIBUTE. Measured: an implementation held names in a fixed buffer and
 * refused a longer one as `kal_err_invalid', which is the same answer it gives
 * for a name that ascends --- so a program meeting the bound was told that its
 * name was malformed. An implementation that imposes no bound reports the
 * greatest value of the type. */
kal_uintptr kal_fs_max_name(void);

/* Release. An implementation shall not treat a released handle as valid. */
void kal_fs_close_dir (struct kal_dir);
void kal_fs_close_file(struct kal_file);

/* A file is read and written through openkal.stream. The stream remains valid
 * while the file is open and is not separately released; the file owns it.
 *
 * It carries its type rather than a bare word. This and `kal_spawn_streams' are
 * the two places a handle passes between interfaces, and they were the two
 * places its type was dropped. */
struct kal_stream kal_fs_stream(struct kal_file);

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
 * failing: a caller that asks what a name refers to has been answered when told
 * that it refers to nothing. Clause 7.7.
 *
 * ⭐ RESOLVES BY DEFAULT. Without KAL_FS_NO_RESOLVE this answers about what the
 * name finally refers to, so it agrees with what `kal_fs_open' would act upon;
 * with it, about the node the name is. A name that finally refers to nothing is
 * `kal_node_absent' in the first form and `kal_node_link' in the second, and
 * both are answers rather than failures.
 *
 * `out->self_size' shall be set by the caller before the call. `wanted' names
 * the fields the caller needs; the implementation reports in `out->present'
 * which of them it filled. */
int kal_fs_info(struct kal_dir base, const char* name, kal_uintptr len,
                kal_uintptr flags, kal_u32 wanted,
                struct kal_node_info* out);
int kal_fs_mkdir (struct kal_dir base, const char* name, kal_uintptr len);
int kal_fs_remove(struct kal_dir base, const char* name, kal_uintptr len);
int kal_fs_rename(struct kal_dir from, const char* a, kal_uintptr alen,
                  struct kal_dir to,   const char* b, kal_uintptr blen);

/* Enquiry about an open file. It is not expressible through kal_fs_info: the
 * name a file was opened by may since have been removed or reused, and a C
 * library answering fstat from the name would answer about a different file.
 * There is no resolution to choose --- the file is already what it is. */
int kal_fs_file_info(struct kal_file, kal_u32 wanted,
                     struct kal_node_info* out);

/* Sets the time kal_fs_file_info reports for an open file.
 *
 * The inverse of an enquiry that already exists, and the interface is
 * incomplete without it: a program that copies a file and preserves its dates,
 * or that extracts an archive, or that marks a file as current, has no way to
 * say so.
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
 * An implementation whose volume does not record a modification time does not
 * claim KAL_FS_PROP_MODIFIED_TIME for it and reports kal_err_not_supported
 * here. An implementation that claims the position shall be able to perform
 * this. */
int kal_fs_set_modified(struct kal_file, kal_u64 modified_ns);

/* Nodes whose content is another name.
 *
 * ⚠️ THESE ARE OPERATIONS OF THIS INTERFACE AND NOT AN INTERFACE OF THEIR OWN,
 * AND CLAUSE 6.2 IS WHY. Whether a volume has such nodes is a property of the
 * format rather than of the environment: one implementation succeeds on one
 * volume and fails on another. A property that varies between the RESOURCES of
 * an interface can be neither an interface nor a word, and is answered by an
 * enquiry taking the resource --- which is `kal_fs_props', and which is what
 * makes these admissible: an operation that is present and cannot be performed
 * here is not clause 6.2's defect, because a caller is able to ask first.
 *
 * `kal_fs_link_read' copies the content of the node into the caller's buffer
 * and reports the length it has, in the shape every counting operation has.
 * `kal_fs_link_create' makes a node at `name' whose content is `target'; the
 * target is not resolved, is not required to exist, and is not required to be a
 * name this interface would accept.
 *
 * KAL_FS_LINK_DIRECTORY states that the target is a directory. One environment
 * requires that at creation and cannot infer it, and cannot make such a node
 * for a target that does not yet exist; another ignores it. A caller that knows
 * says so, and a caller that does not omits it and may be refused on the first
 * environment. */
#define KAL_FS_LINK_DIRECTORY ((kal_uintptr)1u << 0)

int        kal_fs_link_create(struct kal_dir base, const char* name, kal_uintptr len,
                              const char* target, kal_uintptr target_len,
                              kal_uintptr flags);
kal_intptr kal_fs_link_read  (struct kal_dir base, const char* name, kal_uintptr len,
                              char* out, kal_uintptr cap);

/* Enumeration. The iterator is owned and is released by reading past the end or
 * by closing the directory that produced it. The end is reported by the
 * iterator becoming zero.
 *
 * The name is copied into the caller's buffer for the reason given at
 * kal_fs_preopen; `name_len' reports the length it has. `kind' describes the
 * node the entry IS --- enumeration does not resolve, because a directory
 * containing a node that refers to itself would otherwise not terminate. */
int kal_fs_list_begin(struct kal_dir, kal_uintptr* iter);
int kal_fs_list_next (struct kal_dir, kal_uintptr* iter,
                      char* name_out, kal_uintptr name_cap,
                      kal_uintptr* name_len, int* kind);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_FS_H */
