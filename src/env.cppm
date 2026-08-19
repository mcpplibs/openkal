// openkal.env --- the parameters a program receives at inception.
//
// The resource is not a handle. A program is started with arguments and with a
// set of named values, and those are supplied to it rather than obtained by it.
// An interface that offered them through a handle would imply that a program
// could obtain a second set, which no environment provides.
//
// Modification is not among the operations. A program that alters its own
// environment does so in its own storage, which is a facility of a C library
// and not of the boundary; and an environment shared between execution contexts
// cannot be altered without a synchronisation rule this specification declines
// to impose. The values a started program receives are supplied at the point of
// starting it, which is openkal.process.
module;
#include <openkal/env.h>

export module openkal.env;
export import openkal.types;

export using ::kal_env_arg_count;
export using ::kal_env_arg;
export using ::kal_env_var;
export using ::kal_env_var_count;
export using ::kal_env_var_at;

export namespace kal::env {

inline kal_uintptr arg_count() { return kal_env_arg_count(); }
inline const char* arg(kal_uintptr i, kal_uintptr* len) { return kal_env_arg(i, len); }
inline const char* var(const char* name, kal_uintptr n, kal_uintptr* len) {
    return kal_env_var(name, n, len);
}

}
