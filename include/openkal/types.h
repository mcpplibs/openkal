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

/* The result of an operation that transfers a count. The layout is frozen:
 * two machine words are returned in registers on the architectures openkal
 * targets, and a wider result would be returned through a hidden pointer. */
struct kal_io_result {
    kal_uintptr n;
    int         e;
};

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_TYPES_H */
