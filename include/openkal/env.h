/* openkal.env --- the parameters a program receives at inception. */
#ifndef OPENKAL_ENV_H
#define OPENKAL_ENV_H
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The number of arguments, and the argument at a given position. Position
 * zero is the name by which the program was started; an environment that has
 * no such name reports an empty string rather than omitting it. */
kal_uintptr kal_env_arg_count(void);
const char* kal_env_arg(kal_uintptr index, kal_uintptr* len);

/* The value of a named variable, or a null pointer. */
const char* kal_env_var(const char* name, kal_uintptr name_len, kal_uintptr* value_len);

/* Enumeration, for a program that must copy the whole set. The order is
 * unspecified and is not required to be stable between calls. */
kal_uintptr kal_env_var_count(void);
const char* kal_env_var_at(kal_uintptr index, kal_uintptr* name_len,
                           const char** value, kal_uintptr* value_len);

#ifdef __cplusplus
}
#endif
#endif /* OPENKAL_ENV_H */
