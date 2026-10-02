# Stack bounds of a context — execution plan and task dependencies

Companion to `2026-10-02-stack-bounds-of-a-context-design.md` (second edition),
which decides *what* the interface is. This document decides *in what order it
is built, what each repository changes, and what measures each claim*. It also
records the four questions the design left open, now answered, because the
implementation cannot start until they are.

Status: execution. The specification change is the critical path; everything
else waits on it.

---

## 1. What is delivered

One declaration — `int kal_task_stack(void** base, kal_uintptr* size);` — in
`openkal.task`, implemented by every implementation that provides that
interface, and used by the C library port to answer `pthread_getattr_np` for
the calling thread instead of refusing it. Five repositories change, one
(`openkal-opensbi`) does not, and two artefacts outside them (the package index
and the environment manager) are how the result is verified rather than where
it is written.

---

## 2. Dependency graph

```
        S: openkal 0.15.0  (declaration, clause, SURFACE.txt, suite observation)
                 |
     +-----------+-----------+-----------+-----------+
     |           |           |           |           |
   L: linux   M: macos   W: windows   E: emscripten  (opensbi: nothing)
     |           |           |           |
     +-----------+-----------+-----------+
                 |
        C: openkal-musl 0.20.0  (real pthread_getattr_np; absent table; examples)
                 |
        R: releases -> gitcode resources -> mcpp-index -> xlings subos sandbox
```

The edges are real dependencies, not sequence for its own sake:

| Edge | Why it cannot be reordered |
| --- | --- |
| S → L, M, W, E | each implementation's continuous integration resolves `openkal` by version from the index; a declaration that exists only in a working tree is not resolvable there. The check that the implementations are *correct* can be run before the release by pointing them at a path, and that is how the work is developed; the check that they are *released* cannot. |
| L, M, W → C | `openkal-musl` selects an implementation per target, and its conformance run builds the selected one. A C library that answers a real range above an implementation that answers a wrong one is the defect this round exists to remove. |
| C → R | the port is the consumer whose observable behaviour is the reason for the change, and its release note is where a reader meets it. |
| R → index → xlings | the environment manager installs what the index names; verifying before the index names it verifies the previous release. |

Within a tier the four implementations are independent and are done in
parallel. `openkal-opensbi` provides no `openkal.task` and its row is a
statement that it does not change.

---

## 3. Tasks

Each numbered task is one pull request unless the row says otherwise, and every
pull request carries the whole of that repository's change — a declaration
without its implementation is not a smaller change, it is a red build.

### Tier S — the specification

| | Task | Files | Verified by |
| --- | --- | --- | --- |
| S1 | the declaration, with the semantics beside it | `include/openkal/task.h` | `tools/check-declarations.sh` compiles a translation unit naming every entity in `SURFACE.txt` without the environment's headers |
| S2 | the name on the normative surface | `SURFACE.txt` | `tools/check-surface.sh`, and the generated `provides-interfaces` arrays of the implementations |
| S3 | the clause that records it | `SPEC.md` (version 0.15; clause 11 entry 21) | `tools/check-version.sh`, `tools/check-readme-versions.sh` |
| S4 | the version the ecosystem states about itself | `mcpp.toml`, `include/openkal/version.h`, `README.md` | the two tools above, and every implementation's `kal_version` observation |
| S5 | the observation | `conformance/src/sections/task.cpp` | run against `openkal-linux` by path, then against each implementation in its own continuous integration |
| S6 | the record | `.agents/docs/2026-10-02-stack-bounds-of-a-context-design.md`, this file, `-record.md`, `-verify.sh` | the run transcript in `-record.md` |

### Tier L, M, W, E — the implementations

| | Task | Mechanism | Files |
| --- | --- | --- | --- |
| L1 | a context answers for itself | the initial context: the mapping is *measured* by `mincore` probing, and the floor is the higher of the first mapping below and `mapping_end - RLIMIT_STACK` (`prlimit64`); a started context: the region `kal_alloc` gave it | `src/sys.h` (two numbers), `src/task.cpp`, `src/start.cpp` |
| M1 | a context answers for itself | `pthread_get_stackaddr_np`/`pthread_get_stacksize_np` on the calling thread, declared as the file already declares the one other name it takes from that library | `src/task.cpp` |
| W1 | a context answers for itself | `GetCurrentThreadStackLimits`, declared in the package's own Win32 declarations and named in the import library the CI gate diffs against them | `src/win32.h`, `src/task.cpp`, `port/kernel32.def` |
| E1 | a context answers for itself, where the interface exists | `emscripten_stack_get_base`/`_end`, which the pthread runtime maintains per thread | `src/threads/task.cpp` |
| L2 M2 W2 E2 | the release, the README row and the pins | version bump in `mcpp.toml`, the interface table where the package keeps one | `mcpp.toml`, `README.md` |

### Tier C — the consumer

| | Task | Files |
| --- | --- | --- |
| C1 | `pthread_getattr_np` answers from `kal_task_stack` when `t` is the calling thread and reports `ENOSYS` otherwise, uniformly | `port/src/okm_thread.c` |
| C2 | the absence row changes shape or is withdrawn, and the prose table follows it | `mcpp.toml` (`[c-abi-absent]`), `README.md`, `musl/PATCHES.md` |
| C3 | the example asserts the real range instead of the refusal, and the continuous integration step with it | `examples/stack-bounds/*`, `.github/workflows/ci.yml` |
| C4 | the pins move to the implementations that carry the declaration | `mcpp.toml` |

### Tier R — release and verification

| | Task | Instrument |
| --- | --- | --- |
| R1 | tag and release each repository, artifacts built by its own workflow | `git tag`, the release workflow, `gtc release` |
| R2 | each release is mirrored to the resource host the index names | `gtc release create/upload` against gitcode |
| R3 | the index names the new versions | the index repository's per-package files |
| R4 | installation and execution through the manager, in a sandbox, against the mirror | `xlings subos <name> --sandbox --cmd "..."` |
| R5 | the whole-ecosystem review | `.agents/docs/2026-10-02-stack-bounds-of-a-context-record.md` |

---

## 4. The angles this is planned against

| Angle | What it means here |
| --- | --- |
| architecture | the operation is a property of a resource, answered by the resource, and needs no registry (clause 7.1) and no capability word (6.2). No structure is added (5.3) and the result is one word (4.4) |
| stability | the answer is computed once per context and does not change while it runs; a started context's region is fixed by construction; a probe that fails returns an error rather than a range |
| elegance | one declaration, two output words, no new type, no new error, no new position in a capability word |
| user experience | the C library's caller gets real bounds for the common spelling `pthread_getattr_np(pthread_self(), &a)` where it previously got an error, and the same error where it previously got a **wrong range** |
| compatibility | a declaration is added to an existing interface, which clause 8 admits; every existing declaration is untouched; an implementation that cannot answer declines `openkal.task` whole (clause 3), which is a link-time fact rather than a run-time one |
| cross-platform | Linux, macOS and Windows each answer, measured on the platform by that platform's runner; wasm answers where the threads feature is selected and declines the interface where it is not; a machine with firmware and no operating system is unaffected |
| consistency | every implementation answers the same question in the same shape; the suite observes it once for all of them |
| seamless upgrade | the suite is versioned with the specification; an implementation that has not moved still builds and still conforms at 0.14, and one that moves gains an observation rather than an incompatibility. Nothing that compiled stops compiling |
| test coverage | the observation checks the property a caller relies on — the range contains the calling context's own stack — for the initial context and for a started one, and that the answer is stable |

---

## 5. What measures what

| Claim | Measured by | Where |
| --- | --- | --- |
| the initial context answers a range containing its own stack | the conformance suite | x86_64 Linux, macOS runner, Windows runner, wasm |
| a started context answers a range containing *its* stack, distinct from the starter's | the conformance suite | the same four |
| the answer is stable while the context runs | the conformance suite | the same four |
| the range is the reservation and not a moving bound | `openkal-linux`: the probe is the mapping itself; `openkal-macos`: the library states the limit; `openkal-windows`: the call returns reservation bounds | documented per implementation |
| the C library answers a real range for the calling thread | `examples/stack-bounds`, and the same program run above the system's own C library as control | x86_64 Linux, macOS, Windows |
| the C library refuses for another thread rather than answering wrongly | `examples/stack-bounds` | the same three |
| installation and execution through the ecosystem | `xlings subos --sandbox --cmd` | the machine that holds the mirror |

A claim this round cannot measure is not claimed. In particular the Windows
implementation's reservation semantics rest on the call's documentation and on
Wine's implementation of it, both of which are stated in the code rather than
presented as a measurement, and the conformance observation is what shows the
part that matters to a caller.

---

## 6. Decisions the design left open

**D1. Linux: the bound is measured, not inferred.** `RLIMIT_STACK` is a policy,
and the region the kernel mapped at inception is not derivable from it: the
initial stack's mapping ends above the highest string the kernel placed, by a
random shift of up to four megabytes on x86_64, and its floor is enforced
against that mapping's end rather than against the address the program's first
instruction sees. A bound computed from the latter is therefore up to four
megabytes too low, and the space below the stack's reservation is where the
dynamic loader's own mappings are placed — the exact defect class this round
exists to remove. So the implementation measures the mapping: `mincore` answers
whether a page is mapped, an exponential probe downward and upward from a local
variable's address finds the ends of the contiguous mapping that contains it,
and the answer is the higher of that mapping's floor and `mapping_end -
RLIMIT_STACK`. The rlimit is read with `prlimit64`. Both numbers are already
absent from `src/sys.h` and are added.

**D2. macOS: the library's own self-enquiry, and the package's rule is not
broken by it.** The rule in `src/task.cpp` is that only a name *no C library
defines* is reachable, because a program above may define every ordinary name;
`pthread_get_stackaddr_np` and `pthread_get_stacksize_np` are defined by this
system's library and by no C library in this ecosystem — musl defines
`pthread_getattr_np` and neither of these — so a program carrying musl above
this implementation leaves them reachable. The alternative, a class-1 Mach
trap, would need a second calling convention inside `src/sys.h` for one
enquiry, and is not taken.

**D3. Windows: `GetCurrentThreadStackLimits`, with the floor stated.** The call
is Windows 8 and later, which is below every target this ecosystem names, and
the package already requires a version floor of its own. The alternative reads
the thread environment block with inline assembly or an intrinsic, which would
be the first in that package and is refused on that ground. Microsoft does not
publish whether the returned bounds are the reservation or the committed
extent; Wine implements the reservation, WebKit reads them as the
OS-maintained limits, and the implementation states both facts at the call
rather than asserting either.

**D4. The self-only restriction is the interface's shape and not a gap in it.**
`kal_task_stack` asks about the calling context because clause 7.2 makes a
handle meaningful only to the party that obtained it and `kal_task_current`
returns an identity rather than a handle. The C library above it must therefore
answer `pthread_getattr_np` for the calling thread and refuse every other
`pthread_t` with `ENOSYS`, uniformly. That is stated in the port's own
documentation and in its absent-capability prose, because a caller that reads
the return value is the caller this serves and nobody else is served by a
guess.

---

## 7. Risks, stated rather than discovered

1. **The macOS main thread's answer is the least certain.** Whether that
   library reports the reservation or a value that does not contain the current
   stack is settled by a run on the platform, which is why the observation is
   written before the implementation is trusted.
2. **The Linux probe adds syscalls to the first enquiry per context.** It is
   computed once and cached in the context's own thread-local record, and a
   context that never asks never pays.
3. **`mincore` under a seccomp filter.** A program that filters it away gets an
   error from the enquiry, which is mapped to the closed set and documented;
   the alternative — a range that may be wrong — is what this round removes.
4. **Windows and macOS cannot be run from this machine.** Their measurements
   are their runners'; where a claim rests on documentation it says so.
5. **The port still refuses for another thread.** A program that asks about a
   thread it holds gets `ENOSYS` where the system's own C library would answer.
   That is a capability this ecosystem does not have, stated in the table whose
   whole purpose is to state such things, and not a silent wrong answer.
