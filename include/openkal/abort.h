/* openkal.abort --- termination. Core: every environment can terminate. */
#ifndef OPENKAL_ABORT_H
#define OPENKAL_ABORT_H
#include "types.h"

/* The attribute is spelled differently in the two languages and is written out
 * here rather than omitted. A caller that does not know the function cannot
 * return is a caller the compiler will warn about at the point where control
 * appears to continue, which is the point the function exists to remove. */
#if defined(__cplusplus)
#  define KAL_NORETURN [[noreturn]]
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define KAL_NORETURN _Noreturn
#else
#  define KAL_NORETURN
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Terminates the program after reporting the given message. The length is
 * explicit so that the interface does not depend on a string function. */
KAL_NORETURN void kal_abort(const char* msg, kal_uintptr len);

/* Terminates the program with the given status. Termination is immediate:
 * registered exit handlers and static destructors do not run. */
KAL_NORETURN void kal_exit(int code);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_ABORT_H */
