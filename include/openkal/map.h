/* openkal.map --- a part of a file's contents in the address space.
 *
 * Version 0.18. Clause 11 entry 40 records why it was admitted; the design is
 * .agents/docs/2026-10-31-file-mapping-design.md.
 *
 * AN INTERFACE OF ITS OWN AND NOT OPERATIONS OF `openkal.fs', for clause 6.2's
 * reason: an operation an implementation may lack is expressed by its absence.
 * An environment that keeps files in linear memory, or on a block device beneath
 * no page table, provides files and cannot map them; placed in `openkal.fs' the
 * operations would take that interface from it. Nor of `openkal.space', whose
 * resource is an address space and a context executing in one: an environment
 * that can copy an address space and has no files would lose that interface in
 * turn.
 *
 * THE REGION IS AN ADDRESS AND A LENGTH, AS AN ALLOCATION IS. There is no
 * handle: one environment's mapping object may be closed as soon as its view
 * exists, and the other two release a mapping by its address and length, which
 * the caller already holds. A handle would oblige every implementation to keep
 * a table from handles to views --- the compatibility layer clause 7.1 names.
 *
 * NO REGION OF A FILE IS EXECUTABLE. Instructions are executed only from memory
 * `openkal.exec' published, which is where this specification keeps the rule
 * that no region is both writable and executable.
 *
 * Clause 4.4: S, L and X. The region lies in the caller's address space and is
 * the caller's storage, not the implementation's, as an allocation is; an
 * implementation across a boundary maps the caller's space, which is what an
 * environment's own mapping call does. */
#ifndef OPENKAL_MAP_H
#define OPENKAL_MAP_H
#include "types.h"
#include "fs.h"

/* The alignment of an offset into a file.
 *
 * A CONSTANT AND NOT AN ENQUIRY. A file is laid out for mapping when it is
 * written --- data embedded beside a program, the segments of an image, the
 * members of an archive stored rather than compressed --- and the program that
 * writes it does not know in which environment it will be mapped. A value each
 * implementation reported would align a file for one environment and leave it
 * unmappable in another; clause 6.5's second paragraph records the same
 * inversion for an artifact that is distributed.
 *
 * It is the largest granularity among the environments this specification
 * targets: one environment maps from offsets that are multiples of sixty-four
 * kibibytes, the others from multiples of their page. Clause 12.2 aligns an
 * image's segments to the same value for the same reason. An environment whose
 * granularity does not divide it does not provide this interface (clause 6.1)
 * rather than answering it falsely. */
#define KAL_MAP_ALIGNMENT ((kal_u64)65536u)

/* Positions in the `how' argument of kal_map_file. Absent both, the region is
 * read and private. */

/* The region may be written. Without KAL_MAP_SHARED a write is seen by this
 * region alone and never reaches the file. */
#define KAL_MAP_WRITE  ((kal_uintptr)1u << 0)

/* The region is the file's: what is written to it reaches the file and every
 * other shared region of the file, in this program or another. */
#define KAL_MAP_SHARED ((kal_uintptr)1u << 1)

#ifdef __cplusplus
extern "C" {
#endif

/* Maps `length' bytes of `file', from `offset', into the address space, and
 * writes the address of the first through `out'.
 *
 * `offset' is a multiple of KAL_MAP_ALIGNMENT; `length' is not zero; and
 * `offset' + `length' does not exceed the length of the file at the call. A
 * region never extends a file: one environment could not map beyond its end
 * without doing so, and an access beyond the end in the others is a fault.
 * A file is lengthened with `kal_fs_truncate' first. Otherwise, and for a
 * position in `how' this version does not assign, kal_err_invalid.
 *
 * A region to be read requires that the file was opened with KAL_OPEN_READ, and
 * a shared region to be written that it was also opened with KAL_OPEN_WRITE;
 * otherwise kal_err_permission. A private region may be written whatever the
 * file was opened for, since nothing written reaches it.
 *
 * A file whose volume cannot be mapped --- a device, a file the environment
 * synthesises --- reports kal_err_not_supported. That is a property of the
 * resource and not of the implementation, as a volume's answers are in
 * `openkal.fs'.
 *
 * THE REGION OUTLIVES THE HANDLE. It remains valid until kal_map_release,
 * whether or not the file has been closed.
 *
 * THE LAST PAGE. Where the region reaches the end of the file, the bytes after
 * the end, up to the next multiple of 4096 from `offset', may be read and are
 * zero. 4096 is the smallest page among the environments this specification
 * targets and every one of their pages is a multiple of it, so those bytes lie
 * in the last page mapped. A program that reads a file through a terminating
 * zero byte after its contents relies on this, and may.
 *
 * SHARED REGIONS. A write to one is visible at once in every shared region of
 * the file, in any program. It is visible to a read of the file through
 * `openkal.fs' when kal_map_sync has returned. When a write through
 * `openkal.fs' to a part of the file a region holds appears in the region is
 * not specified.
 *
 * PRIVATE REGIONS. Whether a change to the file appears in a page the region
 * has not written is not specified.
 *
 * A FILE SHORTENED BENEATH A REGION. While a region exists, shortening its file
 * below the region's end is either refused by `kal_fs_truncate' with
 * kal_err_permission, or succeeds; after it succeeds, an access to a page that
 * lies wholly beyond the new end is a fault, and the program ends with it. The
 * bytes of the page that holds the new end, after it, are zero. An
 * implementation does not supply zero pages in place of what was removed. An
 * access is a load or a store and not a call, so a fault is the only report it
 * can have; a portable program does not shorten a file it maps. */
int kal_map_file(struct kal_file file, kal_u64 offset, kal_uintptr length,
                 kal_uintptr how, void** out);

/* Makes the writes to a shared region visible to reads of its file through
 * `openkal.fs', for the region as kal_map_file produced it: `region' is its
 * address and `length' the length that was passed. On a private region it does
 * nothing and reports kal_ok.
 *
 * It does not make the writes durable against the loss of the machine, which
 * `openkal.fs' states for no write either. A failure of the medium is
 * kal_err_io.
 *
 * THE WHOLE REGION, NOT A PART OF IT. A part would have to begin on a page, and
 * a page is not a fact this interface states. */
int kal_map_sync(void* region, kal_uintptr length);

/* Releases a region: `region' is the address kal_map_file wrote and `length'
 * the length that was passed. Neither may name a part of a region; one
 * environment releases a view only whole. The addresses may be given out again
 * afterwards. Releasing a valid region does not fail in any environment this
 * specification targets, so nothing is returned, as `kal_free' returns
 * nothing. */
void kal_map_release(void* region, kal_uintptr length);

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_MAP_H */
