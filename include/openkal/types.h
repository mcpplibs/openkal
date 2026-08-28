/* openkal.types --- definitions shared by every openkal interface.
 *
 * The C form of the declarations the module of the same name carries. Clause
 * 4.3 requires the two to declare the same entities; SURFACE.txt is the list
 * both are compared against, so neither is the other's source and a divergence
 * is reported by the conformance procedure rather than discovered by a
 * consumer.
 *
 * The file includes no header. openkal is required to be usable on a
 * freestanding target, and a consumer compiled with -nostdinc --- a C library
 * being ported onto openkal is exactly that consumer --- has none to include.
 */
#ifndef OPENKAL_TYPES_H
#define OPENKAL_TYPES_H

/* The width of a machine word, obtained from the compiler rather than from a
 * header, for the reason stated above.
 *
 * Two compilers state it and one does not. Where the compiler publishes its own
 * spelling of the type, that spelling is used and the definition is exact. The
 * remaining compiler publishes the property the type is defined by --- the width
 * of a pointer --- and not the type, so the type is written from the property.
 * Taking it from that compiler's own header instead would give this file an
 * include, and the consumer this file exists for has none. */
#if defined(__UINTPTR_TYPE__)
typedef __UINTPTR_TYPE__ kal_uintptr;
#elif defined(_MSC_VER)
#  if defined(_WIN64)
typedef unsigned __int64 kal_uintptr;
#  else
typedef unsigned int kal_uintptr;
#  endif
#else
#  error "openkal requires a compiler that states the width of a pointer"
#endif

/* The signed machine word, in which every operation whose whole result is a
 * count reports that count or the condition that prevented it.
 *
 * ONE WORD AND NOT TWO, AND THE REASON IS BOTH SIDES AT ONCE. An earlier form
 * returned a structure of a count and an error. Every consumer of it collapsed
 * the pair by hand and by the same rule --- report what was transferred, or the
 * condition when nothing was --- so the pair was never the shape a caller
 * wanted; and a result of two words cannot be carried by a boundary that
 * returns one, which is the second half of clause 4.4.
 *
 * The collapse is safe because the error set of this header is CLOSED (clause
 * 5.2): a negated error occupies a small known range and can never be mistaken
 * for a count. An open-ended error space would not permit it. */
#if defined(__INTPTR_TYPE__)
typedef __INTPTR_TYPE__ kal_intptr;
#elif defined(_MSC_VER)
#  if defined(_WIN64)
typedef signed __int64 kal_intptr;
#  else
typedef signed int kal_intptr;
#  endif
#else
#  error "openkal requires a compiler that states the width of a pointer"
#endif

/* The three fixed widths openkal's operations use, stated once here rather than
 * at each use.
 *
 * They are openkal's names for openkal's types. Reaching for the compiler's
 * spelling at each use had two defects: it reached for a spelling one of the
 * three compilers this specification is built with does not have, and it stated
 * in eight places a decision that belongs in one. A reader asking what width an
 * offset has now has one place to look. */
#if defined(__UINT64_TYPE__)
typedef __UINT32_TYPE__ kal_u32;
typedef __UINT64_TYPE__ kal_u64;
typedef __INT64_TYPE__  kal_i64;
#elif defined(_MSC_VER)
typedef unsigned int     kal_u32;
typedef unsigned __int64 kal_u64;
typedef signed   __int64 kal_i64;
#else
#  error "openkal requires a compiler that states a sixty-four bit type"
#endif

/* A byte.
 *
 * The three widths above are the ones openkal's OPERATIONS use --- a size, an
 * offset, a count. An address is the first datum this specification must state
 * byte by byte rather than as a number, because its meaning is the sequence of
 * its bytes and not their arithmetic value. */
#if defined(__UINT8_TYPE__)
typedef __UINT8_TYPE__ kal_u8;
#elif defined(_MSC_VER)
typedef unsigned char  kal_u8;
#else
#  error "openkal requires a compiler that states an eight bit type"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* The complete set of error conditions openkal defines. The set is closed: an
 * implementation maps its environment's error values onto these and does not
 * extend them. */
enum kal_error {
    kal_ok                = 0,
    kal_err_invalid       = 1,   /* the handle or an argument is not valid   */
    kal_err_again         = 2,   /* the operation would block                */
    kal_err_io            = 3,   /* the device or medium reported a failure  */
    kal_err_no_memory     = 4,
    kal_err_no_space      = 5,
    kal_err_permission    = 6,
    kal_err_not_supported = 7,
    kal_err_closed        = 8,   /* the peer closed the connection           */
    kal_err_not_found     = 9,   /* the name does not exist                  */
    kal_err_exists        = 10,  /* the name exists and the caller required
                                  * that it not                             */
    kal_err_not_empty     = 11,  /* a directory that is required to be empty
                                  * is not                                  */
    kal_err_is_directory  = 12,  /* the name refers to a directory and the
                                  * operation applies to a file             */
    kal_err_not_directory = 13   /* the reverse of the preceding condition   */
};

/* HOW AN OPERATION REPORTS ITS RESULT --- one rule, stated once.
 *
 *   An operation whose whole result is a COUNT OF BYTES returns `kal_intptr':
 *   the count, or the negated error value when no byte was produced. A count
 *   of zero is a count and not an error, and for a read it denotes end of
 *   input.
 *
 *   An operation that produces a RESOURCE, or that produces nothing, returns
 *   `int' from `kal_error' and writes what it produced through a pointer.
 *
 * A caller therefore never inspects two things to learn one thing, and every
 * result of either kind fits in one machine word. */

/* Where a connection or a message goes. The layout is frozen (clause 5.3).
 *
 * HERE AND NOT IN net.h, because `openkal.datagram' uses it too and either
 * interface may be provided without the other. A type shared by two interfaces
 * belongs to neither of them.
 *
 * BYTES AND A LENGTH, not a tagged union of families. The length is what
 * distinguishes one kind of address from another, and it is a VALUE rather than
 * a layout --- so the set of lengths this specification defines may grow while
 * clause 5.3 continues to hold the structure itself fixed. That is the rule
 * clause 6.2 gives a property word, applied to a value instead of to a bit.
 *
 * An implementation refuses a length it does not know (kal_err_invalid) rather
 * than reading it as one it does. Twenty-four bytes is chosen so that an
 * address carrying a scope identifier fits without a second type. */
struct kal_endpoint {
    kal_u8  addr[24];   /* network order --- the bytes of the address        */
    kal_u32 addr_len;   /* 4 = IPv4  16 = IPv6  20 = IPv6 with a scope
                         * identifier. Further values are defined by later
                         * revisions and are refused, not misread, by an
                         * implementation that does not know them.
                         *
                         * A fixed width rather than a machine word: the value
                         * is at most twenty-four, and a machine word would
                         * freeze a difference between a thirty-two and a
                         * sixty-four bit target that nothing in this structure
                         * needs. Clause 5.3 holds the layout, so the layout
                         * should not carry a distinction it has no use for. */
    kal_u32 port;       /* a number, in host order: this interface does not
                         * ask a caller to perform a protocol's byte order
                         * conversion                                        */
};

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_TYPES_H */
