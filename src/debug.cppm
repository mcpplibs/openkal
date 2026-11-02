// openkal.debug --- the name of an address in a program's own code (version
// 0.20).
//
// A program finds its own frames by its own unwinding; naming an address needs
// the symbol table, which the party that loaded the program holds and the
// program's address space does not.
module;
#include <openkal/debug.h>

export module openkal.debug;
export import openkal.types;

export using ::kal_debug_symbolize;
export using ::kal_debug_props;
