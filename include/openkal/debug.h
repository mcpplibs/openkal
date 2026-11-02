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
 * an implementation provides it to a program built for its environment (from
 * version 0.23; before, it may). Where nothing provides it, it is absent
 * (clause 6.1).
 *
 * A FAULT IS REPORTED BY WHAT ENDS THE PROGRAM (version 0.23). An
 * implementation that states KAL_DEBUG_PROP_FAULT_REPORT, when it ends a
 * program for a fault --- an access it may not make, an instruction it may not
 * execute, its stack exhausted --- first writes to the program's error stream
 * the fault and its address, and each frame of the faulting context that its
 * chain of frame pointers gives: the address; the file that holds it and the
 * offset in that file, where the environment maps files; and, where that file
 * is an image (clause 12) or the program itself, the name of the symbol that
 * covers it and the distance into it. It then ends the program as the
 * environment ends one for that fault, so that a record the environment keeps
 * of it --- a core file --- is kept as before. Lines begin `openkal: fault: '
 * and `openkal:   #N ', one per frame. A program does nothing to have it:
 * whatever subverts or breaks a program, the party that ends it is the one that
 * can still say where.
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

/* Positions in kal_debug_props. Version 0.23. */
#define KAL_DEBUG_PROP_FAULT_REPORT ((kal_uintptr)1u << 0)  /* see above */

kal_uintptr kal_debug_props(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENKAL_DEBUG_H */
