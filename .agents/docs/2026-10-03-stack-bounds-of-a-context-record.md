# Stack bounds of a context — record of the round

Date: 2026-10-03. The design is `2026-10-02-stack-bounds-of-a-context-design.md`,
the task breakdown and the decisions are
`2026-10-02-stack-bounds-of-a-context-execution-plan.md`, and the ecosystem
verification against the published index is
`2026-10-03-stack-bounds-of-a-context-verify.sh` beside this file.

What this round delivered: one declaration (`kal_task_stack`), its
specification, its observation in the conformance suite, five implementations
that answer it, the C library port that uses it in place of a refusal, and the
release, index and sandbox verification of the whole set.

---

## 1. The releases

| Repository | Version | Pull request | What it carries |
| --- | --- | --- | --- |
| `openkal` | 0.15.0 | #45 | the declaration, clause 11 entry 21, `SURFACE.txt`, the suite's observation |
| `openkal-linux` | 0.16.0 | #32 | the measured region; the TLS-image defect it exposed, fixed |
| `openkal-macos` | 0.13.0 | #26 | `_pthread_self` and the two `_np` enquiries; three names into `port/libSystem.tbd` |
| `openkal-windows` | 0.11.0 | #30 | `GetCurrentThreadStackLimits`, declared and exported |
| `openkal-emscripten` | 0.4.0 | #5 | the platform's per-context pair |
| `openkal-opensbi` | 0.8.2 | #19 | the pin, and nothing else |
| `openkal-musl` | 0.20.0 | #50 | the real answer for the calling thread, the refusal for another, the withdrawn absent row |
| `openkal-linux` | 0.16.1 | #33 | the thread-local image placed where the linker measured it (a repair; §3.1) |
| `openkal-musl` | 0.20.1 | #51 | the pin that reaches the repair |
| `openkal-llvm-runtime` | 0.15.4 | #34 | the pin that reaches the repaired C library |

Every branch was named `openkal-0.15.0` in every repository, which is what makes
each repository's continuous integration test the other halves as written rather
than as published: the specification clones the implementation at that branch,
and the implementation clones the specification at it.

## 2. What was measured, and where

| Claim | Instrument | Result |
| --- | --- | --- |
| the region the kernel stops the stack at is `mapping_end - RLIMIT_STACK` | a descending write in a probe on 6.8.0 | exact: the floor writable, one page below it not, one page above it writable |
| the region below is bounded by one guard gap above the nearest mapping | the same probe with a page mapped by itself inside the reservation | exact: 256 pages above the planted page |
| the same, on the released implementation | the conformance suite's five observations, in `openkal-linux`'s own tests, and end to end through `openkal-musl` | held |
| the macOS names resolve | a cross link of a program that calls the enquiry, aarch64 and x86_64 | no undefined symbols; the four imports are exactly the ones the code names |
| the Windows name resolves | the declared-vs-exported gate, the generated `libkernel32.a`, and a cross build for `x86_64-windows-gnu` | pass; `__imp_GetCurrentThreadStackLimits` present |
| the WebAssembly pair answers per context | the task gate's program under node | exits 0; both contexts are told a region containing themselves |
| the C library answers for the caller and refuses another thread | `examples/stack-bounds` above `openkal-musl` 0.20.0 | first and started contained, another thread refused |

## 3. The two defects the round exposed, and their fixes

### 3.1 The thread-local image was placed eight bytes below the variables

**This is the one that mattered, and 0.16.0 shipped it.** Every thread-local
address is `tp + st_value - tls_size`, where `tls_size` is the block the LINKER
laid out: `p_memsz` rounded up to the segment's alignment. `describe_tls`
clamped that alignment up to sixteen *before recording it*, so the clamp reached
the SIZE, which must not have it. Measured on a segment stating `p_align = 8,
p_memsz = 56`: the linker put the variables at `tp - 56` and the region was
built 64 bytes deep, so the image of the program's thread-local storage sat
eight bytes below the variables that name it.

From above: two thread-local variables declared with different values read each
other's bytes, and a C++ program's `thread_local` object can find its guard byte
non-zero and never run its constructor at all. That is how it was found — not by
this round's own tests, which passed, but by `openkal-llvm-runtime`'s probe
against the released packages, on the runner:

```
FAIL: a thread_local is constructed in a spawned thread
FAIL: and its destructor runs when that thread ends
```

**Why it surfaced now, stated rather than glossed.** The clamp is as old as the
file. This implementation's own thread-local storage had been one four-byte
variable, which sits at the very end of the segment and was copied correctly by
luck; the twenty-four bytes 0.16.0 added moved the image far enough to be read
by the wrong variable. The kit test therefore passed throughout, and the test
0.16.0 added for exactly this area — `tests/conformance_task_tls.cpp`, with a
non-zero initialiser and a four-kilobyte block — did not catch it either,
because the alignment its own link happened to produce was sixteen. What did
catch it was a C++ `thread_local` object with a destructor, in another
repository's continuous integration, against the published packages.

The repair: `describe_tls` keeps the segment's alignment as the loader stated
it; `make_tls` rounds the size by that and asks the ALLOCATOR for sixteen.
Verified against a probe of two initialised thread-local variables — in the
context the program was started on and in a started one — by the kit, by this
package's tests, by the conformance suite (197 held, 0 did not hold), and by the
runtime's own example, which now reports `-- failures: 0 --`.

### 3.2 A context's storage was laid out from an undescribed image

`openkal-linux` described its own thread-local image only from the entry point it
supplies, and a program that carries a runtime never runs that entry point. A
context was therefore given a storage region laid out from a segment of size
zero: its thread pointer sat at the START of the region rather than at its end,
and every thread-local variable of that context was addressed below the
allocation — into whatever the allocator kept beside it. It had been invisible
while this implementation's own storage was one four-byte variable, and it became
fatal when the storage for this round's operation made it twenty-four: the
specification package's own kit test died at the first instruction of a context.

`make_tls` now describes the image if nothing has, from the vectors `env.cpp`
records in either arrangement, and refuses to lay out a region it cannot size
rather than laying out a wrong one.

## 4. What this round could not measure

- **macOS and Windows at run time.** Their mechanisms are exercised by the
  conformance suite on their own runners; what was done here is the cross link
  and the symbol gates. `GetCurrentThreadStackLimits`' reservation-versus-commit
  question remains unpublished by the vendor and is stated at the declaration.
- **A machine with firmware and no operating system.** `openkal-opensbi` provides
  no `openkal.task`, and the round's change to it is one version pin.
- **The `stack_guard_gap` a kernel is booted with.** The default is applied; a
  kernel configured with a larger gap stops the stack above the base reported,
  which is the direction a caller must not be wrong in, and it is stated at the
  code.

## 5. The verification transcript

From `2026-10-03-stack-bounds-of-a-context-verify.sh`, run inside an `xlings`
sandbox (`xlings subos use okl015 --sandbox --cmd ...`) against the published
index with the CN mirror selected, using the released engine:

```
▸ entering subos okl015  (exit to leave)

== A. identity and mirror ==
ok: mcpp 2026.10.1.3 from /home/speak/.xlings/data/xpkgs/xim-x-mcpp/2026.10.1.3/bin/mcpp
ok: xlings mirror is CN

== B. openkal 0.15.0 and openkal-linux 0.16.1 resolve, and a context is told its own region ==
proot warning: ptrace(PEEKDATA): Bad address
ok: version 0.15.0 first e=0 contains=1 size=8376320
ok: started contains=1 ran=1
ok: the lock records openkal-linux 0.16.1

== C. openkal-musl 0.20.1: the calling thread is told its region, another thread is refused ==
ok: musl first e=0 contains=1 size=3280896
ok: musl started contains=1 size=262144 another thread refused=1
ok: the lock records openkal-musl 0.20.1
NOT RUN: macOS, Windows and WebAssembly: their implementations answer on their own runners (openkal-macos#26, openkal-windows#30, openkal-emscripten#5), and the two cross links are made on this machine by their own continuous integration
NOT RUN: a machine with firmware and no operating system: openkal-opensbi 0.8.2 moves a pin and provides no openkal.task

0 assertion(s) failed
not run:
  - macOS, Windows and WebAssembly: their implementations answer on their own runners (openkal-macos#26, openkal-windows#30, openkal-emscripten#5), and the two cross links are made on this machine by their own continuous integration
  - a machine with firmware and no operating system: openkal-opensbi 0.8.2 moves a pin and provides no openkal.task
exit=0
```

## 6. Review of the round, at the scale of the ecosystem

**What is consistent.** One declaration, five implementations answering the same
question in the same shape, one suite observing it once for all of them, and one
C library asking it on behalf of the callers that cannot. Every implementation
that answers was measured answering on its own system: Linux here and in
`openkal-musl`'s example, macOS and Windows on their runners through the
conformance suite, WebAssembly under node in the emscripten gate.

**What the round changed that was not this feature.** One latent defect in the
hosted arrangement of `openkal-linux` — a context's thread-local storage laid out
from an undiscribed image, which had been silently writing below its own
allocation since the implementation was written. It was found because this
feature made the implementation's own storage large enough to fault. That is the
honest account: the round's own change did not cause it, and the round could not
have been finished without it.

**What a consumer must know.** A version written without an operator is an exact
pin in mcpp, so a consumer moves by naming the new versions, and any package that
locks `openkal` and can share a graph with the consumer must move too. Within
this round that is `openkal-musl` 0.20.1 and `openkal-llvm-runtime` 0.15.4; a
program that carries the C++ runtime names both, and the runtime's own release
is a consequence of the repair rather than of the feature.

**What remains open, with the reason.** The macOS and Windows mechanisms cannot
be run from this machine, and their reservation-versus-commit question is
answered by the vendor's documentation on one system and by a reimplementation
on the other; both are stated at the declaration. The `stack_guard_gap` a kernel
is booted with cannot be read by an implementation, so the default is applied and
the direction of the error is stated. And `pthread_getattr_np` still refuses for
a thread that is not the caller: a context can only be asked about itself, which
is the shape the specification chose and not a gap left by the port.
