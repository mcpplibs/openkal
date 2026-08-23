/* openkal --- the whole interface, for a consumer that cannot import a module.
 *
 * The specification states its contract as a C application binary interface
 * and distributes C++ modules that declare it. A C++ consumer imports the
 * module for the interface it uses. A consumer written in C cannot: a C
 * translation unit has no import. The canonical such consumer is a C library
 * being ported onto openkal, which is the case the specification exists to
 * support, so the declarations are distributed in both forms. Clause 4.3
 * requires the two to declare the same entities and states how that is
 * verified. */
#ifndef OPENKAL_H
#define OPENKAL_H

#include "openkal/types.h"
#include "openkal/abort.h"
#include "openkal/stream.h"
#include "openkal/memory.h"
#include "openkal/env.h"
#include "openkal/time.h"
#include "openkal/fs.h"
#include "openkal/process.h"
#include "openkal/task.h"
#include "openkal/exec.h"

#endif /* OPENKAL_H */
