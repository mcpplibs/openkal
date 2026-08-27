# openkal

openkal is a portable kernel application binary interface. This repository
contains the normative specification, the declarations in both of the forms it
distributes them in, and the suite an implementation runs against itself.

The specification is [`SPEC.md`](SPEC.md).

## What this package contains

Declarations, and no definitions. Every function declared here is supplied by an
implementation package; building this package alone produces a library with
undefined references, which is the intended outcome.

| Module | Header | Interface | Class |
| --- | --- | --- | --- |
| `openkal.types` | `openkal/types.h` | machine word, error values, transfer result | — |
| `openkal.abort` | `openkal/abort.h` | termination | core |
| `openkal.stream` | `openkal/stream.h` | byte streams | core |
| `openkal.memory` | `openkal/memory.h` | allocation | core |
| `openkal.env` | `openkal/env.h` | the parameters a program receives at inception | standard |
| `openkal.time` | `openkal/time.h` | monotonic and wall time sources | standard |
| `openkal.fs` | `openkal/fs.h` | directories and open files, relative throughout | standard |
| `openkal.process` | `openkal/process.h` | starting a program and waiting for it | standard |
| `openkal.task` | `openkal/task.h` | execution contexts, and the primitive they are built upon | standard |
| `openkal.random` | `openkal/random.h` | a source of unpredictable bytes | optional |
| `openkal.exec` | `openkal/exec.h` | a region of the address space a program may execute | optional |

### One statement of the declarations, two ways to reach it

The contract is a C application binary interface, and a C translation unit has
no `import`. The canonical consumer of this contract — a C library ported onto
openkal — *is* a C translation unit, so the declarations are distributed in both
forms.

The header is the statement. The module includes it in its global module
fragment and exports the names, so a consumer that imports and a consumer that
includes obtain the same entities rather than two declarations that agree today.
What the module adds is not a second declaration: it is what C++ can check and C
cannot — the layouts clause 5.3 freezes are asserted there, and the capability
words become types that cannot be mixed.

`SURFACE.txt` is normative, and both forms are compared against it:

| | |
| --- | --- |
| `tools/check-declarations.sh` | compiles a translation unit naming every entity, with `-nostdinc` — because the consumer the header exists for is compiled that way |
| a test in each implementation | the same list, reached through `import openkal.*` |
| `conformance/src/declarations.c` | the same list again, compiled by every toolchain that builds the suite |

The header includes nothing. openkal must be usable on a freestanding target,
and a consumer compiled with `-nostdinc` has nothing to include.

## How a program uses openkal

A program declares two dependencies. The first fixes the version of the contract
it is written against; the second selects an implementation and is ordinarily
conditional on the target.

```toml
[dependencies]
openkal = "0.5.1"

[target.'cfg(os = "linux")'.dependencies]
openkal-linux = "0.5.1"

[target.'cfg(os = "macos")'.dependencies]
openkal-macos = "0.3.1"

[target.'cfg(windows)'.dependencies]
openkal-windows = "0.1.1"
```

The program imports the interface and names no implementation.

```cpp
import openkal.stream;

int main() {
    const char greeting[] = "hello\n";
    kal::write(kal::out(), greeting, sizeof(greeting) - 1);
    return 0;
}
```

Changing the implementation is a change to one line of the manifest. The source
does not change, and this property is the reason the specification exists.

## How an implementation is written

An implementation provides definitions and no modules. It reaches the
declarations it is defining and exports nothing: the interface belongs to this
package.

| | |
| --- | --- |
| [`openkal-linux`](https://github.com/mcpplibs/openkal-linux) | on the kernel's own system-call interface |
| [`openkal-macos`](https://github.com/mcpplibs/openkal-macos) | on the kernel's own calls, and two names no C library defines |
| [`openkal-windows`](https://github.com/mcpplibs/openkal-windows) | on Win32 and the object manager beneath it, using no C runtime symbol |

Clause 4 states how the declarations are organised, clause 6 states how the
absence of an interface is expressed, and clause 7 states the requirements an
implementation must satisfy.

## Conformance

Clause 9 has two halves, and both are here.

**The artefact.** `tools/check-surface.sh` compares an implementation's exported
names against `SURFACE.txt`. It detects the one freedom an implementation retains
after the language has removed the others: the addition of names.

**The behaviour.** [`conformance/`](conformance/) is a program an implementation
runs against itself — 97 observations across eight interfaces, in four kinds:
behaviour, ABI, stability and cost.

```bash
# from an implementation's working tree
bash /path/to/openkal/tools/run-conformance.sh openkal-linux . full
```

It is composable, because openkal is: an implementation provides an interface in
whole or not at all, so each interface is a feature and a run reports on what was
selected. It reports three counts, and the third is the one to read —
`97 held, 0 did not hold, 0 not observed` — because a suite that reported only
the first two cannot distinguish an interface that behaved from one it never
examined.

It depends on openkal and the language. There is no `import std`: the suite must
run against an implementation in a program that carries no other runtime, and a
suite resting on facilities that implementation may be the only supplier of would
be reporting on itself.

## What is built, and by what

Three compiler families across three systems, because a contract that holds only
under the compiler its author used is a description of that compiler.

| | Linux | macOS | Windows |
| --- | --- | --- | --- |
| gcc | ✓ | — | ✓ (PE, GNU CRT) |
| llvm | ✓ | ✓ | ✓ (MSVC ABI) |
| msvc | — | — | ✓ |

## License

Apache-2.0.

## openkal-kit

`kit/` holds facilities composed from the interfaces this specification defines.
It is a separate package, `mcpplibs/openkal-kit`, and it is **not** part of the
specification.

The specification admits an interface only when it is a minimal capability every
kernel has and cannot be composed from the interfaces already present. That rule
is what keeps openkal implementable on a machine with firmware and nothing else,
and it leaves a gap: a program that wants to carry bytes between two of its own
contexts, or to turn `"127.0.0.1:8080"` into an endpoint, has an answer in POSIX
and no answer here — because both are composed rather than primitive.

That gap was being filled by the port layer. There is one port layer today and
what it composes is POSIX, so a native openkal program either wrote the
composition again or took a whole C library. The kit is where the composition is
written once.

**The contract form is what makes the two unmistakable.** Clause 10 states that
openkal's contract is a C application binary interface. The kit deliberately is
not one: it is C++ modules in `namespace kal::kit`, and it exports no name
beginning with `kal_`. Measured on its objects: the defined names are C++ mangled
module initialisers such as `_ZGIW7openkalW3kitW7channel`, and the operations are
inline and emitted into consumers rather than exported at all. So
`tools/check-surface.sh --complete` does not read a program that links the kit as
an implementation which has added names — the rule that checker enforces is about
the C surface, and the kit has none.

So "is this normative" is answered by the shape of what is exported rather than
by a sentence saying it is not. A sentence can be overlooked; a mangled name
cannot become a C symbol.

The consequence is the one that matters. Clause 8 forbids the specification from
altering a declaration it has published, which is what makes openkal safe to
depend upon and what makes it the wrong place for a facility still finding its
shape. **The kit may evolve.**
