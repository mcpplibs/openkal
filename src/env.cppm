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
export module openkal.env;
export import openkal.types;

export extern "C" {

// The number of arguments, and the argument at a given position. Position zero
// is the name by which the program was started, which an environment that has
// no such name reports as an empty string rather than omitting.
kal_uintptr   kal_env_arg_count(void);
const char*   kal_env_arg(kal_uintptr index, kal_uintptr* len);

// The value of a named variable, or a null pointer. The name is passed with an
// explicit length so that the interface does not require a string function.
const char*   kal_env_var(const char* name, kal_uintptr name_len, kal_uintptr* value_len);

// Enumeration, for a program that must copy the whole set. The order is
// unspecified and is not required to be stable between calls.
kal_uintptr   kal_env_var_count(void);
const char*   kal_env_var_at(kal_uintptr index, kal_uintptr* name_len,
                             const char** value, kal_uintptr* value_len);

}

export namespace kal::env {

inline kal_uintptr arg_count() { return kal_env_arg_count(); }
inline const char* arg(kal_uintptr i, kal_uintptr* len) { return kal_env_arg(i, len); }
inline const char* var(const char* name, kal_uintptr n, kal_uintptr* len) {
    return kal_env_var(name, n, len);
}

}
