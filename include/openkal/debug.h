/* openkal.debug --- the name of an address in a program's own code.
 *
 * Version 0.20. Clause 11 entry 45 records why it was admitted.
 *
 * WHAT AN IMAGE CANNOT DO FOR ITSELF. A program finds the addresses of its own
 * frames by its own unwinding: the tables it needs are in the program, and an
 * image keeps them (clause 12.2). Naming an address needs the program's symbol
 * table, which a loader reads from the image's file and never places in the
 * image's address space --- so a program that reports where it failed could
 * state only numbers, and a number in an image depends on where it was loaded.
 * The party that holds the table answers.
 *
 * WHO PROVIDES IT. A loader provides it to the images it binds (clause 12.3);
 * an implementation may provide it to a program built for its environment.
 * Where nothing provides it, it is absent (clause 6.1).
 *
 * Clause 4.4: S, L and X. */
#ifndef OPENKAL_DEBUG_H
#define OPENKAL_DEBUG_H
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The name of the function or object that holds `address', as the symbol table
 * of the program or image that contains it states the name, and where it
 * begins.
 *
 * Writes min(cap, length) bytes of the name at `out' and returns its length, as
 * `kal_env_arg' does; no terminator is written. `start', when not null,
 * receives the address at which the named entity begins, so that the caller
 * can state the offset. An address no symbol covers --- outside every image, or
 * in an image without a symbol table --- reports kal_err_not_found, negated.
 *
 * The name is the table's, not a rendering of it: a name a compiler encoded is
 * returned encoded, and a caller that wants it decoded does so itself. */
kal_intptr kal_debug_symbolize(kal_uintptr address, char* out, kal_uintptr cap,
                               kal_uintptr* start);

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_DEBUG_H */
