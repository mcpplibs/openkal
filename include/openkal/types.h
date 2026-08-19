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
 * header, for the reason stated above. */
typedef __UINTPTR_TYPE__ kal_uintptr;

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
