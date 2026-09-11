# The portable program

One program that exercises the eight interfaces the specification defines. It
names no implementation, no operating system, no descriptor number and no
system call.

Its purpose is to be built over every implementation. The specification's own
continuous integration does that: the conformance job builds this source against
each implementation it tests and requires the final count to be zero, so the one
source is exercised over all of them rather than copied into each.

That sentence used to describe the implementations' own workflows doing it. None
of them did, and the manifest here had drifted three specification versions
behind without anything noticing --- which is what a claim with no criterion
behind it costs.

It prints one line per interface, each beginning `openkal: `, and a final line
reporting the number of observations that did not hold. An implementation
passes when that number is zero.

The program requires an implementation that provides every interface it uses.
An implementation that omits one fails to link, and the linker names the
operations it did not define — which is the diagnostic clause 4.2 describes.

## Five platforms, and the three ways one is reached

The manifest names an implementation per platform and the program names none.
What is worth reading in it is that the five platforms are reached in three
different ways:

| platform | line in the manifest | what supplies it |
|---|---|---|
| Linux (glibc, musl) | `cfg(os = "linux")` | `openkal-linux` |
| Android, both ABIs | the SAME line | `openkal-linux`, unchanged |
| macOS | `cfg(os = "macos")` | `openkal-macos` |
| iOS, both simulator arches | `cfg(os = "ios")` | `openkal-macos`, unchanged |
| Windows | `cfg(windows)` | `openkal-windows` |
| Web (Emscripten) | `cfg(os = "emscripten")` | `openkal-emscripten` |

**Android needs no line of its own**, because `aarch64-linux-android` has
`os = "linux"`: the kernel IS Linux, bionic is a C library above it, and an
implementation written on the kernel's own system-call interface does not know
which C library sits above it. Adding an Android line would be adding a second
name for one answer.

**iOS needs a line and not a package.** Darwin is Darwin — the same traps, the
same call numbers, the same calling convention — and what differs between macOS
and iOS is the SDK and the deployment-target flag, which belong to the build
tool. So one `cfg` line selects the macOS implementation for all three iOS rows.

**The Web needed new software.** Emscripten has no kernel to issue a call to,
so an implementation there cannot be written beneath a C library and has to sit
above one. `openkal-emscripten` provides twelve of the fifteen interfaces; this
program uses eight, all of which are among them. A program that used
`openkal.process` would fail at link naming the symbol, which is how a partial
surface reports itself.

## The version pins here are a claim, and they were wrong

`src/main.cpp` was updated for 0.11's `kal_spawn` — one struct where there had
been three declarations — and these pins were not, so the example built
NOWHERE:

```
src/main.cpp:256:19: error: 'kal_spawn' does not name a type
```

It failed identically for the host and for every cross target, because a
version pin in an example is a claim about what that example builds against and
nothing was checking it. The program is not in any package's build, so no
repository's continuous integration compiled it.

That is what the platform lines above are for as much as demonstration: each
one is a target something can be asked to build, and the specification's own
conformance job builds this source over each implementation it tests.
