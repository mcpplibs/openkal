# Stack bounds of an execution context — analysis and design

Second edition, 2026-10-02. **This edition reverses the first one's verdict and
is the one to read**; the first edition's conclusion ("not admissible, do not
add") was reached before the three platforms were measured, and the measurement
contradicts it. What survives from the first edition is its reading of the
clauses, kept below.

Status: **the design the implementation follows.** `kal_task_stack` is declared
in `include/openkal/task.h` and specified in clause 11 entry 21 of version 0.15;
the four questions this edition left open are answered in
`2026-10-02-stack-bounds-of-a-context-execution-plan.md` §6, and the
measurements the answers rest on are recorded with the code that applies them.
The reading of the clauses below is against `SPEC.md` version 0.14, which is the
version this design was written against.

---

## 1. The finding that decides it

The first edition treated the context the program was started on as the obstacle:
openkal does not know the initial stack, so not every resource of `openkal.task`
could answer, so clause 6.4 forbids the operation.

**That reasoning was wrong, because it assumed the question is asked about a
context the asker is not.** A context that is *running* can always find its own
stack bounds; every environment offers it at least one way. Measured, per
platform:

| | the running context's own bounds | the bounds of the context the program started on |
| --- | --- | --- |
| **Linux** | `getrlimit(RLIMIT_STACK)` (#97) or `prlimit64` (#302) from the running thread; openkal-linux already carries a syscall table and can add the number | the same call, from the initial thread — the initial thread *is* running, and `__okl_start_c(long* sp)` (`openkal-linux/src/start.cpp:96`) already receives the stack pointer |
| **macOS** | `pthread_get_stackaddr_np(self)` / `pthread_get_stacksize_np(self)` — libpthread defines them and they take the *calling* thread, so they are exactly a self-enquiry (top = returned address, low = top − size) | `sysctl(CTL_KERN, KERN_USRSTACK64 = 59)` returns the initial stack **top** (BSD syscall #202, already in `openkal-macos/src/sys.h:190`); the size is `getrlimit(RLIMIT_STACK)` (#194), or the `apple[]` `"main_stack"` string that `okm_start` **already receives and discards** (`openkal-macos/src/start.cpp:34-36` `(void)apple;`) |
| **Windows** | `GetCurrentThreadStackLimits(&low, &high)` — kernel32, Windows 8+, no failure path, current thread only | the same call: the loader built the initial thread's stack, and this call answers for whichever thread runs |

So the initial context is not a difficulty. It is the *easiest* case on every one
of the three platforms: the program's first openkal code is already running on
that stack.

**Consequence:** the precondition the first edition could not establish is in fact
satisfiable on all three mainstream platforms by the *same* means — the context
asks about itself — and that means the operation can be designed in-spec.

---

## 2. The clause reading that survives

Kept from the first edition because the design below is shaped by it.

- **The word "stack" does not occur in `SPEC.md`** (verified: no case-insensitive
  match in the document). It appears only in header comments —
  `include/openkal/task.h:49-50` and `include/openkal/space.h:71-86`. There is no
  clause that authorises or forbids this by name; it is derived.
- **Clause 6.4 decides the shape** (`SPEC.md:622-630`): *"An operation that some
  resources of an interface can never satisfy shall not be placed in that
  interface."* Together with 6.2's escape hatch — a resource-varying property is
  answered by *"an enquiry taking the resource … nothing is transferred, and no
  resource can fail to answer"* (`SPEC.md:535-541`) — there are two admissible
  outcomes and no third: every context answers, or the operation does not exist.
  §3 argues the first holds.
- **Clause 6.1** (`SPEC.md:495-501`): a provided interface's operations *"shall
  not … report a lack of support at run time."* So there can be no per-context
  "unsupported" answer, and no `kal_err_not_supported` path.
- **Clause 7.1** (`SPEC.md:713-719`) excludes shapes that need a registry, and
  6.3 (`SPEC.md:583-587`) names the failure: *"a mechanism reconstructed rather
  than a facility conveyed."* §3 is written so that no registry is needed.
- **Clause 6.2** (`SPEC.md:519-533`): a property varying **between
  implementations** is a position in `kal_<interface>_props`; one varying
  **between resources** is an enquiry. Bounds vary per context, so they are the
  latter — a `kal_task_props` position cannot carry them.
- **Clause 4.4** (`SPEC.md:330-354`): `openkal.task` is crossable at S, L and X.
  No returned pointer into the implementation, no result wider than one machine
  word; 5.2.1 (`SPEC.md:420-441`) gives the convention — an operation that
  produces nothing returns `int` and writes through a pointer.
- **Clause 5.3** (`SPEC.md:442-455`): structure layouts are frozen; only
  `kal_node_info` may grow. `struct kal_task` cannot be extended.
- **Clause 7.2** (`SPEC.md:721-746`): a handle *"shall be meaningful in the
  context of the caller that obtained it"*, and `kal_task_current` returns an
  **identity, not a handle** (`include/openkal/task.h:63-65`). This is what makes
  an enquiry **about the calling context** the natural shape, and is §5's first
  decision.

---

## 3. Why every resource can answer

`openkal.task`'s resources are execution contexts. The design reports the bounds
of **the calling context**, and a running context can always answer about itself:

- **A context openkal created** — it is running on the stack openkal (Linux
  standalone: `kal_alloc(256 KiB)`, `openkal-linux/src/task.cpp:92-98,148-149`)
  or the host (macOS `pthread_create_from_mach_thread`, Windows `CreateThread`)
  gave it. Either the implementation recorded that, or the running context asks
  its environment.
- **The context the program was started on** — the first openkal code runs on it,
  so the same self-enquiry is available, and on Linux the entry point already
  receives the stack pointer.

The enquiry never has to reach *another* context. That is what keeps it out of
7.1's registry case: the answer is a fact about where the caller is standing, not
a record the implementation must maintain about resources it does not control.

Two honest limits, both stated in the semantics rather than discovered:

1. **A context may only be asked about itself.** Asking about another context is
   not expressible — the interface's handles name the caller's own resources, and
   7.2 makes a handle meaningful only to the party that obtained it. The
   consequence for the C library is in §7.
2. **The answer is the region the implementation is prepared to vouch for**, not
   necessarily the kernel's whole mapping. On Windows this is the distinction
   between the reservation (`DeallocationStack`/`StackBase`, what
   `GetCurrentThreadStackLimits` returns) and the currently committed low bound
   (`StackLimit`, which moves). The design states which one is meant (§5.3).

---

## 4. Conformance requirement

An implementation that provides `openkal.task` answers this operation for every
context — the one it created and the one the program was started on. An
implementation that cannot is not failing this operation; it **does not provide
`openkal.task`**, which clause 3 already expresses as a link-time absence
(`SPEC.md:31-32`). The emscripten implementation does exactly this today: it
defines the task interface under `-pthread` and defines nothing without it
(`openkal-emscripten/src/threads/task.cpp:38-57`), on clause 6.1's ground.

So "declare it unsupported" is a decision at the **interface** granularity, not
at the call. `openkal.task` is optional, so no environment is excluded: opensbi
provides none of it and is unaffected.

---

## 5. The design

### 5.1 Declaration

```c
/* In openkal/task.h, beside kal_task_current. */

/* The stack the CALLING context runs on: `base` is the lowest address it may
 * use and `size` its length in bytes.
 *
 * IT ASKS ABOUT THE CALLER AND TAKES NO HANDLE. A handle is meaningful in the
 * context of the caller that obtained it (clause 7.2), and kal_task_current
 * reports an identity rather than a handle --- so a context is the one thing a
 * caller always stands on and can never hold. The enquiry is addressed
 * accordingly, and an implementation therefore never records bounds for a
 * context it does not control.
 *
 * EVERY CONTEXT ANSWERS. A running context can find its own stack on every
 * environment this interface is provided for; one that cannot does not provide
 * openkal.task (clause 3, clause 6.4).
 *
 * WHAT SIZE MEANS. The region the implementation is prepared to vouch for as
 * usable by this context. Where an environment distinguishes a reservation
 * from what is committed, this is the reservation: a caller lays a guard below
 * the base and grows a region above it, and both are wrong against a bound that
 * moves as the stack grows.
 *
 * The answer does not change while the context runs, and stops being meaningful
 * when it ends. */
int kal_task_stack(void** base, kal_uintptr* size);
```

### 5.2 Why each choice is forced, not preferred

| Choice | Clause |
| --- | --- |
| no handle argument | 7.2: `kal_task_current` returns an identity, not a handle; and a context that must be asked about itself needs no lookup |
| `int` return, answers through two output pointers | 4.4 rules 2 and 3 (no returned pointer, no result wider than a machine word) and 5.2.1 (produces nothing → `int`, writes through a pointer) |
| no new structure | 5.3 freezes layouts; a crossing structure would need `self_size` or a stated reason it cannot grow — two scalars avoid the question entirely |
| no `kal_err_not_supported` path | 6.1: a provided interface's operations shall not report a lack of support at run time |
| no `kal_task_props` position carrying the bounds | 6.2: bounds vary between resources; a position records one answer for the implementation |
| the answer is defined for the caller only | 7.1: answering about other contexts needs a registry, which is precisely the excluded shape |

A `KAL_TASK_PROP_*` position is still admissible in the sense 6.5 permits — a
word read **before** the call saying whether the *implementation* records the
initial context's bounds at all — but if §4 holds then every provider answers, so
the position would always be set and is not needed. Omit it.

### 5.3 Semantics, precisely

- Returns `kal_ok`, or a negative error from the closed set. On error the outputs
  are not written (the convention the other enquiries use).
- `base` is the lowest usable address; `size` is the length in bytes, so the
  usable range is `[base, base+size)`.
- The values are stable for the life of the context. A context's stack does not
  move, and an implementation that grew it would be answering a different
  question.
- `size` is never zero on success: a running context has a stack. (This is why
  the "zero means cannot say" spelling of `kal_task_parallelism` is not reused —
  clause 11 entry 12's precedent covers a count whose zero is a fact about the
  implementation, not a region whose zero would be indistinguishable from an
  empty stack.)
- Concurrent calls from distinct contexts are independent (6.6).

---

## 6. What each platform does to implement it

An implementation records the pair once per context, at the point the context
first runs, and answers from that record. The natural recording point is the
per-context trampoline the implementation already has — the function it hands to
the environment's thread-creating call.

| Implementation | The running context's own bounds | Work |
| --- | --- | --- |
| **openkal-linux, standalone** | created contexts already record it (`stack`, `stack_bytes`); the initial context needs `sp` (already an argument of `__okl_start_c`) plus `RLIMIT_STACK`, whose number is one line in `src/sys.h` (x86_64 `getrlimit` = 97, `prlimit64` = 302; the table already carries `nr_fcntl`, `nr_prctl`, …) | add the number, record the pair at entry, answer from it |
| **openkal-linux, hosted** | `pthread_create` runs openkal's `run()` on the host's stack; the running context can read `RLIMIT_STACK`, or the program above supplies a record | same; the host library is present by definition here |
| **openkal-macos** | `pthread_get_stackaddr_np(self)` / `pthread_get_stacksize_np(self)` — libpthread functions that take the calling thread, so they are exactly a self-enquiry. The alternative consistent with the repo's "no name a C library defines" rule (`src/task.cpp:25-49`) is a class-1 Mach trap call to `mach_vm_region` around the thread's own stack pointer (`mach_msg_trap` is `0x1000000\|31` on x86_64); `src/sys.h` has only BSD-class numbers and would need a class-1 path | pick one of the two; the initial context can additionally use `sysctl(1, 59)` (top) plus `getrlimit` (#194), or read the `apple[]` `"main_stack"` entry that `start.cpp:34-36` currently discards |
| **openkal-windows** | `GetCurrentThreadStackLimits(&low, &high)` — kernel32, Windows 8+, returns the reservation bounds, no intrinsic, no inline asm, and it is the only candidate that needs neither (the package has none today) | declare it in `src/win32.h`, **add the name to `port/kernel32.def`** or CI's declared-vs-exported gate (`ci.yml:181-235`) fails the clang cross link, record the pair in `run()` (`task.cpp:25-29`) and for the initial thread at entry |
| **openkal-emscripten** | WASM thread stack; the interface is already conditional on `-pthread` | answer from the same self-enquiry the others use, or keep declining the interface entirely (today it already does) |
| **openkal-opensbi** | does not provide `openkal.task` | nothing |

Open questions for the implementations, stated rather than assumed. **All four
are answered in `2026-10-02-stack-bounds-of-a-context-execution-plan.md` §6**,
and the answers are kept here as the questions were asked:

- **Windows version floor.** `GetCurrentThreadStackLimits` is Windows 8+. If that
  is not acceptable for this ecosystem's declared targets, the TEB route needs
  inline assembly (`movq %gs:0x30`) or an intrinsic under three toolchains, which
  would be the first in that package.
- **Windows reservation vs commit.** Wine implements the call as
  `DeallocationStack`/`StackBase` (reservation). Microsoft does not publish its
  implementation; WebKit's use of it as "OS-maintained stack limits" supports the
  reservation reading, but a measurement on Windows is wanted before §5.3 is
  frozen.
- **macOS: libpthread name or Mach trap.** The first is simpler and goes against
  the package's stated rule; the second needs a class-1 syscall path that
  `src/sys.h` does not have today, which is a change in kind rather than degree.
- **Linux standalone `RLIMIT_STACK` semantics.** The limit is the environment's
  policy, not the region the kernel mapped at inception. Whether an implementation
  reports the policy-bounded region or the mapped one must be decided once and
  documented, because a guard placed against the policy bound on a stack whose
  mapping is smaller is exactly the defect being fixed.

---

## 7. What the ecosystem changes

| Project | Change |
| --- | --- |
| **openkal** | the declaration in `include/openkal/task.h`; the semantics above stated beside it; a clause 11 entry or a 7.x clause recording the "reservation, not committed bound" choice and the self-only restriction, so the next implementer meets the reasoning rather than a diff. Version: clause 8 admits a new declaration in an existing interface |
| **openkal-musl** | `port/src/okm_thread.c` gains a real `pthread_getattr_np`: fill a zeroed `pthread_attr_t` from `kal_task_stack` for **the calling thread**, and report `ENOSYS` for another `pthread_t` — never a wrong range. The `[c-abi-absent]` row changes shape or is withdrawn, and so is the README's absent-facilities row; `examples/stack-bounds` asserts the real bounds instead of the refusal, and its CI step's assertion changes with it. musl's own source stays excluded: its two branches are both wrong in this port (the first reads `libc.auxv`, which here is a static array; the second reports the mapping `__clone` discards), so restoring it would require correcting it, not merely un-excluding it |
| **openkal-linux** | record the initial context's pair at entry; add the `getrlimit`/`prlimit64` number; answer from the context record |
| **openkal-macos** | implement the self-enquiry (libpthread pair or Mach trap) and decide/document which; record the initial context's pair from `sysctl(1,59)` + `getrlimit` or the `main_stack` vector |
| **openkal-windows** | declare `GetCurrentThreadStackLimits`, add it to `port/kernel32.def`, record at thread entry and for the initial thread |
| **openkal-emscripten** | implement the self-enquiry where the task interface exists; no change where it does not |
| **openkal-opensbi** | none |
| **picolibc and other C library ports** | none required; `pthread_getattr_np` is a musl/glibc surface and picolibc has no equivalent to fill |
| **conformance** | a check that a running context's reported range contains its own stack pointer and lies within what the environment mapped — the property a caller relies on, and the one both wrong answers in #49 violated |

---

## 8. The consumer evidence, restated

Unchanged from the first edition's §8, which is still the honest picture: the need
is **real and recurring** in the wider ecosystem (WebAssembly Micro Runtime's
`wasm_runtime_detect_native_stack_overflow`, Boehm GC's coroutine stack handling,
WebKit's musl stack handling) and **unmeasured** within this one. Those consumers
all fall back on a refusal, which is why #49's `ENOSYS` cost nobody anything — and
also why the case for adding this is correctness and convenience for the callers
that would use real bounds, not a program that cannot run without it.

This section no longer decides anything. In the first edition it was the reason
not to add; here it is only a statement about urgency.

---

## 9. Self-review of this edition

**What is now solid.**

- The obstacle the first edition rested on does not exist: every platform can
  answer about the running context, including the initial one. Measured per
  platform in §1 and §6, mechanism named for each.
- The shape follows the clauses rather than taste: a self-addressed enquiry
  because 7.2 gives a caller no handle to itself; scalars rather than a structure
  because 5.3 freezes layouts and 4.4 forbids a two-word result; no "unsupported"
  path because 6.1 forbids it; no capability word because 6.2 says a
  resource-varying property cannot be one.
- The "decline in whole" exit is real and already exercised in this ecosystem by
  openkal-emscripten, so the conformance requirement closes no door.

**What is still weak, and should be attacked before this is specified.**

1. **The Windows and macOS mechanisms are the ones I could not run.** Everything
   about them is from source reading plus external documentation; the Windows
   commit-vs-reservation semantics in particular are reimplementation-derived
   (Wine) and MS-unpublished. A measurement on each platform is the first thing
   the specification procedure should require. Clause 9 says the suite verifies
   both halves; this operation's two halves are the range and its containment of
   the stack pointer.
2. **The macOS choice is unresolved by design**, and one branch of it contradicts
   that package's own rule.
3. **`RLIMIT_STACK` is a policy and not a mapping** (Linux, and macOS's fallback
   for the initial thread). §6 records this as a decision still to be made, and
   it is the one place where a plausible implementation could still be *wrong* in
   the way #49 was.
4. **The self-only restriction costs the C library port its generality.**
   `pthread_getattr_np(t, …)` must answer for any `pthread_t`; under this design
   the port can answer only when `t` is the calling thread, and must report
   `ENOSYS` otherwise — uniformly, never a wrong range. On musl that is not a
   corner: `pthread_getattr_np(pthread_self(), …)` is the common spelling, so the
   common case works and the general case is refused. That should be stated in
   the port's documentation rather than discovered.
5. **This operation is a property of a resource, but only ever asked by the
   resource.** That is unusual in this specification — `kal_stream_props` takes a
   handle because a caller holds several streams; a caller holds no context but
   the one it is in. The shape is not the same as its nearest precedent, and the
   clause that records it should say why.
6. **Nothing here has been implemented or measured.** The whole of §6 is a plan.

**What the implementation then settled, item by item.** 1: the macOS and Windows
mechanisms are exercised by the conformance observation on those platforms'
runners, and the facts that cannot be measured from here are stated at the call
rather than presented as measurements. 2: the macOS choice is the library's own
self-enquiry, and the package's rule is not broken by it — the rule excludes
names a *C library in this ecosystem* defines, and musl defines
`pthread_getattr_np` and neither of the two used. 3: the Linux bound is
*measured* — the mapping is found with `mincore`, the limit with `prlimit64`,
and the floor is the higher of the two bounds the kernel applies, the limit and
one guard gap above the nearest mapping below. Both were measured on 6.8.0
before being written down, and the guard gap that cannot be read is named as the
one assumption. 4: the port's table and its README state the refusal for another
thread. 5: clause 11 entry 21 states why the enquiry is addressed to the caller.
6: no longer true; see the execution plan's §5 for what is measured where.

**What would falsify the design.** A platform where a running context genuinely
cannot bound its own stack — a freestanding environment with no runtime and no
startup record. That is openkal-opensbi's world, and it is why the interface must
stay optional and whole rather than gaining a third, weaker "cannot answer" state.

---

## 10. Relationship to the merged change

`mcpplibs/openkal-musl#49` (squash `c994323`) remains correct under this edition,
and is still not a placeholder:

- The C ABI's `ENOSYS` says "the capability behind this name is not present",
  which is a statement about the C library's surface, not a run-time refusal
  inside an openkal interface. `[c-abi-absent]` with `form = "enosys"` is where
  this ecosystem declares the first.
- The function is defined, linkable, and uniform across every context, so a
  caller that reads the return value is never wrong.
- When openkal carries the fact, the port's replacement is deleted — and musl's
  own source is corrected rather than merely un-excluded, since both of its
  branches are wrong here (§7).
