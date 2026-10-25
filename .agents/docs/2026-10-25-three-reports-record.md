# The record of the round: a least stack size, whether input is altered, and what closing a directory ends

2026-10-25. The design is `2026-10-25-three-reports-optimisation-design.md`
(second edition); this document records what was released, what was measured,
and the review of the round at the scale of the ecosystem. The verification is
`2026-10-25-three-reports-verify.sh`.

---

## 1. The releases

| Repository | Version | Pull request | What it carries |
| --- | --- | --- | --- |
| `openkal` | 0.16.0 | #50 | `kal_task_start_sized`, `KAL_TERM_PASS_INPUT`, the enumeration paragraph of `fs.h`, clause 11 entries 22–24, two observations |
| `openkal-linux` | 0.17.0 | #37 (carries #35) | enumerations ended by the close; a guarded 8 MiB stack; the least size; the four input flags |
| `openkal-macos` | 0.14.0 | #28 | the same, with the thread attributes composed beneath a C library and compared with the system's |
| `openkal-windows` | 0.12.0 | #32 | the same, with the input position read from virtual terminal input |
| `openkal-emscripten` | 0.5.0 | #7 | the enumeration, the least size, the input position |
| `openkal-opensbi` | 0.8.3 | #21 | the pin |
| `openkal-musl` | 0.21.0 | #54 | the stack size passed on; no stack mapped for a thread; a supplied stack refused; raw input; the dispatcher's frame |
| `openkal-llvm-runtime` | 0.15.5 | #36 | the pin |
| `mcpp-index` | — | #565 | the rows; two consumer members; the candidate gate schedules a platform package on its own platform |
| `openkal-macos` | 0.14.1 | #29 | a copy of the space can start a thread (`mcpplibs/openkal-musl#55`) |
| `openkal-musl` | 0.21.1 | #56 | the pin that reaches it; a program duplicated by `fork` starts a thread, asserted on every row |
| `openkal-llvm-runtime` | 0.15.6 | #37 | the pin |
| `mcpp-index` | — | #566 | the rows for the three; the two members move |

Each release is a tag on the merge commit. The GLOBAL address is the tag
archive and the CN address is the gitcode asset in `mcpp-res`, uploaded with
`gtc release publish`. Every pair was downloaded and compared against the
recorded sha256.

Every branch was named `openkal-0.16.0` in every repository, so each
repository's continuous integration tested the others as written. One push
run of the specification's `main` failed: it was made in the seconds between
the specification's merge and the implementations' merges. The check exists to
catch this, it said the two were not in step, and a re-run after the
implementations merged passed.

## 2. What the measurements changed

| Assumed | Measured | Consequence |
| --- | --- | --- |
| A console with line input off delivers what is typed | The return key was 0x0d, and Ctrl+J, in three spellings of its key event, produced no byte; with `ENABLE_VIRTUAL_TERMINAL_INPUT` they were 0x0d and 0x0a; with line input on, `61 0d 0a` | On that environment the position is virtual terminal input with line input off |
| The C library performs `ICRNL` per descriptor where the environment does not | A program sets the mode through one standard stream and reads through another; under the measured mapping the per-descriptor condition never held | The state is the terminal's; the C library performs `ICRNL` and `ISTRIP` in raw mode without echo |
| The conformance suite can observe a descriptor leak | `RLIMIT_NOFILE` on a runner can be 1048576 | Each implementation observes the leak through its own table (clause 7.13's precedent) |
| A probe in a `fork`ed copy shows the guard on macOS | `pthread_create` in the copy returned `EAGAIN`, on `main` as on the branch | The guard is observed in a program started for the purpose; the `fork` defect is recorded in §5 |

## 3. What was measured, and where

| Claim | Instrument | Result |
| --- | --- | --- |
| PR #35's observation detects the defect | the Linux test with `main`'s `fs.cpp` | fails; passes with the change |
| An abandoned enumeration holds nothing | `/proc/self/fd`, `fcntl`, `GetProcessHandleCount` | equal before and after (Windows 104/104) |
| A close racing a read is safe | 200 races in each implementation's tests | every reader told, no fault |
| A least size is room | the suite (4 MiB, three quarters recursed) on Linux ×2, macOS, Windows ×2, and under node; each implementation's tests at 1, 16, 64 MiB | held everywhere; Windows told 8.25 MiB for 1 MiB |
| `setstacksize` is honoured above the C library | `examples/stack-size`, against `main` | `main` faults about 256 KiB into a 4 MiB request; the release recurses through 3 MiB |
| An unused stack is not committed | VmRSS around a 256 MiB request | no growth |
| A supplied stack is refused | the same probe | `EINVAL` |
| A read below a thread's stack faults | a program started by `posix_spawn`; macOS hosted tests in a `fork`ed child | faults on Linux and macOS |
| The dispatcher's frame | clang 22.1.8, `-O0` | `0x10d90` → `0x2e10` |
| Raw input above the C library matches the host's | `examples/terminal` through a pseudo terminal against the system's own C library, under `cfmakeraw` and with `ICRNL` kept | byte for byte: `0d 0a` and `0a 0a` |
| Raw input on the console | the Windows test, three toolchains | `0d 0a` with the position set |
| The suite | Linux, macOS, Windows, node | Linux 201 held, 0 not held; Windows 182 held, 0 not held |

## 4. The verification

`2026-10-25-three-reports-verify.sh` resolves every package from the published
index (artifact `ad0f019`, the index's merge of #566) with the CN mirror
selected, so each archive came from the gitcode asset. It builds the reporters'
own programs:
- a program duplicated by `fork` starting a thread (`mcpplibs/openkal-musl#55`);
- the directory scan of `mcpplibs/openkal-linux#34`;
- the recursion of `mcpplibs/openkal-musl#48`;
- the raw-mode reader of `mcpplibs/openkal#48`, typed at through a pseudo terminal.

It builds them above the C library both directly and through
`openkal-llvm-runtime` 0.15.6 on `x86_64-linux-musl` with `llvm@23.1.3`, which
is the arrangement the reports were written in.

It was run in the agent's own `xlings` environment, with the mirror chosen for
the run alone (`MCPP_VERIFY_NO_CONFIG=1`). Creating a SubOS is reserved to its
owner outside a sandbox, and this run was inside one. The script runs unchanged
in a SubOS of its own:

```
xlings subos new okl016
cp .agents/docs/2026-10-25-three-reports-verify.sh ~/.xlings/subos/okl016/tmp/v.sh
xlings subos use okl016 --sandbox --cmd "MCPP_VERIFY_VERSION=2026.10.24.1 bash /tmp/v.sh"
```

The transcript is §6.

## 5. Review of the round, at the scale of the ecosystem

**What is consistent.** Two declarations and one paragraph, answered in the
same shape by every implementation that provides the interface:
- the enumeration fix uses one construction in five implementations: a table of listings, a search rather than a dereference, and a marker for a close that races a read;
- the least size has one meaning, and the three native implementations give the same 8 MiB default;
- the input position reads the same four termios flags on three implementations, and the one environment without them reads its own switch.

Above the implementations, the C library gives a program the host's bytes in
raw mode on every system, which it can do because it knows what its caller
asked for and the implementations do not.

**What a consumer must know.** A version written without an operator is an
exact pin, so a program moves by naming the new versions. A program that
carries the C++ runtime names `openkal-llvm-runtime` 0.15.5, which locks
`openkal-musl` 0.21.0 and through it the implementations of this round.

What changes for such a program:
- An iterator used after its directory was closed is refused. `fs.h` had always said it was released.
- `pthread_attr_setstack` is refused at `pthread_create`.
- Threads that asked for nothing have 8 MiB instead of 256 KiB, 512 KiB or 1 MiB. This costs address space, not memory.
- On Windows in raw mode, arrow and function keys arrive as escape sequences.

**What the index gained.** Its candidate gate could not have accepted a
platform package: mcpp locks what the host resolves, and the gate asked every
runner to prove every package. It now schedules a candidate on the runners
where its consumer builds it, and the openkal family has consumers. Three
packages are named only for targets no runner builds (a musl environment,
WebAssembly, a RISC-V machine), so the gate schedules none of them; their
repositories measure them on their targets.

**What was found, and fixed in the same round.**
On macOS a program duplicated by `fork` could not start a thread: `pthread_create` returned `EAGAIN` in the copy (`mcpplibs/openkal-musl#55`). This was reproduced on unmodified `main`, so it predates the round. `kal_space_start` made the copy with the duplication primitive alone, and this system's `fork` does more: around the primitive each part of the system library takes its locks, and in the copy each makes its state true of the new process.

Measured on macOS 14.8.9, in a program that duplicated itself with the primitive:

| What the copy was given | What `pthread_create` returned in the copy |
| --- | --- |
| no preparation | `EAGAIN` |
| the two child preparations alone | the copy was killed |
| those, and the thread library's locks around the primitive | 0 |
| the whole set in the library's own order | 0, in five rounds of twenty threads each |

The routine `fork` calls these from (`_libSystem_atfork_*`) is not exported, but each part is. `openkal-macos` 0.14.1 calls the parts in that order, which is how the copy of `examples/stack-size` on the macOS row starts a thread (`exited=1 status=3`).

**What was found and not changed.**
1. On Windows, line assembly ends a line as `0d 0a`. That is line editing, not raw input, and is outside this round.
2. The C library's `mmap` zeroes every mapping it returns, not only thread stacks. Removing that needs `openkal.memory` to state that a fresh page-sized allocation reads as zero.

## 6. Transcript

```

== A. identity and mirror ==
ok: mcpp 2026.10.24.1 from /home/speak/.xlings/data/xpkgs/xim-x-mcpp/2026.10.24.1/bin/mcpp
ok: the CN mirror is selected for this run only

== B. openkal 0.16.0 and openkal-linux 0.17.0: a least stack size, an enumeration ended by its close, unaltered input ==
ok: version 0.16.0
ok: sized e=0 told>=16MiB=1 room=1
ok: enumeration before=8 after=8
ok: pty position=1 bytes=0d 0a
ok: the lock records openkal-linux 0.17.0

== C. openkal-musl 0.21.1: the reporters' three programs ==
ok: #34: descriptors before=7 after=7
ok: musl#48: 1024 KiB asked, 900 levels recursed
ok: musl#48: a supplied stack is refused with EINVAL
ok: musl#48: 4096 KiB asked, 3900 levels recursed
ok: musl#48: a supplied stack is refused with EINVAL
ok: musl#55: a program duplicated by fork starts a thread
ok: #48: ICRNL reads back cleared after cfmakeraw
ok: #48: Enter, Ctrl+J, q arrive as byte 0x0d byte 0x0a byte 0x71 
ok: the lock records openkal-musl 0.21.1

== D. openkal-llvm-runtime 0.15.6 on x86_64-linux-musl, the reporters' arrangement ==
ok: the lock records openkal-llvm-runtime 0.15.6 over openkal-musl 0.21.1
ok: #34 through the runtime: descriptors before=7 after=7
ok: musl#48 through the runtime: 4 MiB, 3900 levels
ok: #48 through the runtime: byte 0x0d byte 0x0a byte 0x71 
NOT RUN: macOS, Windows and WebAssembly: their implementations answered on their own runners (openkal-macos#28 and #29, openkal-windows#32, openkal-emscripten#7, openkal#50; a program duplicated by fork starting a thread on macOS, openkal-musl#56)
NOT RUN: a machine with firmware and no operating system: openkal-opensbi 0.8.3 moves a pin and provides none of the three interfaces

0 assertion(s) failed
not run:
  - macOS, Windows and WebAssembly: their implementations answered on their own runners (openkal-macos#28 and #29, openkal-windows#32, openkal-emscripten#7, openkal#50; a program duplicated by fork starting a thread on macOS, openkal-musl#56)
  - a machine with firmware and no operating system: openkal-opensbi 0.8.3 moves a pin and provides none of the three interfaces
exit=0
```
