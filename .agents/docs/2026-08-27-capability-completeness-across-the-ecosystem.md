# Capability completeness across the openkal ecosystem

**Date**: 2026-08-27
**Scope**: openkal 0.8 and every repository that implements or consumes it
**Status**: design, for review. Nothing here is implemented.

---

## 0. Why this document exists

Three reports from one user, on one workload — a seven-member C++23 modules
workspace that compiles and links statically through the native path:

| report | subject |
| --- | --- |
| `mcpp-community/mcpp#514` | engine: dependency units missed the target C++-ABI include set; cache key omitted the target axis |
| `mcpplibs/openkal-musl#13` | the port publishes musl's internal `hidden` macro to consumers |
| `mcpplibs/openkal-linux#13` | ENOSYS for fork, pipe, socket, chmod, symlink, copy_file_range |

`#514` is closed: mcpp 2026.8.27.1 carries both halves. The other two are open,
and neither is a single defect. They are two visible points on one line: **the
specification has grown atoms that the layers above it do not yet use, and the
layers below it do not yet all provide.**

This document states what is measured today, what "complete" can mean given
what openkal is, and what each repository would have to do. It does not decide;
it makes the decision reviewable.

---

## 1. The measured baseline

### 1.1 What openkal 0.8 declares

Fifteen interfaces, ninety names in `SURFACE.txt`:

```
abort 2   datagram 6   env 5    exec 4    fs 19    memory 2   net 11
process 8 random 2     space 2  stream 7  task 7   terminal 4 time 5   timeout 6
```

### 1.2 What each backend implements

Measured by searching each backend's sources for definitions of the names
`SURFACE.txt` lists. Every module's `*_props` entry is a `const` object rather
than a function, so a module reported as *n−1 / n* is complete.

| interface | linux | macos | windows | opensbi | uefi |
| --- | :-: | :-: | :-: | :-: | :-: |
| abort, memory, stream | ✅ | ✅ | ✅ | ✅ | ✅ |
| env, time | ✅ | ✅ | ✅ | ✅ | — |
| fs, process, task, random, terminal | ✅ | ✅ | ✅ | — | — |
| **net** | ✅ | **—** | **—** | — | — |
| **datagram** | ✅ | **—** | **—** | — | — |
| **timeout** | ✅ | **—** | **—** | — | — |
| **exec** | ✅ | **—** | **—** | — | — |
| **space** | ✅ | **—** | **—** | — | — |

⭐ **THE FIVE INTERFACES 0.8 ADDED EXIST ONLY ON LINUX.** macos and windows
decline them in whole, which clause 3 permits and clause 6.1 makes honest — a
program that uses one fails at the link, naming the operation. But it means any
capability built upon them is a Linux capability until those two move.

### 1.3 What the C library routes

`openkal-musl` 0.4.0 handles **70** `SYS_` cases. Of the surfaces the reports
name:

| surface | routed | note |
| --- | :-: | --- |
| `pipe`, `pipe2` | ✅ | `kal_process_channel`, weak reference tested before call |
| `copy_file_range` | ✅ | |
| `execve`, `wait4`, `readlink`, `getdents64`, `dup`, `dup3`, `kill` | ✅ | |
| `socket`, `bind`, `listen`, `accept`, `connect` | **0** | falls to `default:` → `-ENOSYS` |
| `clone`, `fork` | **0** | `musl/src/thread/clone.c` excluded |
| `chmod`, `fchmod`, `fchmodat` | **0** | |
| `symlink`, `symlinkat`, `link` | **0** | |
| `poll`, `ppoll`, `select`, `pselect6` | **0** | |

`musl/src/network/*.c` is **not excluded**: those files compile and call
`__socketcall`, which reaches the port's default arm. The gap is in the port,
not in musl's sources.

Excluded sources that bound the surface today:

```
env/__libc_start_main.c  env/__init_tls.c        thread/__set_thread_area.c
thread/clone.c           process/posix_spawn.c   mman/mmap.c
internal/syscall_ret.c   unistd/getcwd.c         ldso/dl_iterate_phdr.c
linux/cache.c            linux/{timerfd,eventfd,signalfd,inotify,epoll}.c
```

### 1.4 What the C++ runtime withholds

`openkal-llvm-runtime` 0.2.0 excludes, on the bare-metal row only,
`libcxx/src/filesystem/*.cpp` and `libcxx/src/random.cpp`, keeping
`_LIBCPP_HAS_RANDOM_DEVICE` at 0 there. The hosted rows build both. So the C++
side is already near its ceiling for hosted targets; what limits it is what the
C library below can answer.

---

## 2. A correction that changes the shape of the problem

An earlier reading of these reports concluded that `fork` was a capability the
specification had deliberately declined, on the grounds that a copy-on-write
duplicate is not a minimal capability every kernel has.

**That was wrong.** `openkal.space` states:

> Starts a context in a copy of the calling address space. The result is a
> process … The copy is taken at this call.

That is fork's semantics. The atom exists, and openkal-linux implements it. What
does not exist is the *route*: `musl/src/thread/clone.c` is excluded and no
`SYS_clone` case is present.

⚠️ The distinction that survives is narrower and still matters. `fork()` returns
twice at the call site; `kal_space_start` begins the copy at an entry function.
Section 4.2 addresses what follows from that.

---

## 3. What "complete" can mean

openkal is not POSIX and is not trying to become it. Clause 3 admits an
interface only when it is a minimal capability every kernel has and cannot be
composed from what is already present. So completeness has to be defined
against the atoms, not against POSIX:

> **A layer is complete when every capability the atoms below it can express is
> reachable through the interface above it, and every capability they cannot is
> refused in a way the caller can act on.**

That gives three categories, and the whole plan is the act of sorting the
reports' items into them:

- **(A) Composable now.** The atoms exist and the layer does not use them. Pure
  adaptation. No specification change.
- **(B) Composable with fidelity loss.** The layer can emulate the POSIX shape,
  but what it reports will not be what the environment holds. Requires a stated
  decision, not silence.
- **(C) Not composable.** The atoms are absent. Either the specification grows,
  or the operation is refused permanently and the refusal is documented.

---

## 4. The sort

### 4.1 (A) Composable now — the largest group

**Sockets, stream and datagram.** Every BSD operation the reports need maps:

| BSD | atom |
| --- | --- |
| `connect` | `kal_net_connect` |
| `bind` + `listen` | `kal_net_listen` |
| `accept` | `kal_net_accept`, or `kal_timeout_accept` for a bounded wait |
| `read`/`write`/`send`/`recv` | `kal_net_stream` → `kal_stream_*`, or `kal_timeout_*` |
| `getsockname` / `getpeername` | `kal_net_local` / `kal_net_peer` |
| `shutdown` | `kal_net_shutdown` |
| UDP, whole surface | `kal_datagram_open/send_to/recv_from/local/close` |

⚠️ **BSD SEPARATES `socket()` FROM `connect()`/`bind()`; openkal DOES NOT.**
`kal_net_connect` produces a connection; there is no unbound socket. The port
must therefore hold a descriptor in a *pending* state carrying domain, type and
protocol, and perform the atom at `connect`, or at `listen` after `bind` has
recorded the local endpoint. This is a port-layer state machine, not a gap.

`sockaddr_in`/`sockaddr_in6` ↔ `kal_endpoint` conversion belongs beside it;
`openkal.kit`'s `parse_v4`/`format_v4` already establish the byte order and the
`addr_len`-selects-the-family convention.

**Non-blocking and readiness.** `O_NONBLOCK`, `SO_RCVTIMEO` and small
`poll`/`select` sets are expressible with `kal_timeout_*` at `timeout_ns = 0`
and at finite bounds. This is O(n) per call over the set, which is correct and
unhurried; `epoll` stays excluded, as it is a Linux facility rather than a
capability.

**`posix_spawn`.** `kal_process_spawn_with` + `kal_process_channel` +
`kal_process_wait` are exactly its three parts. `musl/src/process/posix_spawn.c`
is excluded today; a port implementation restores `posix_spawn`, `system` and
`popen` for every consumer that uses them — which is the majority of the
"shell out to a subprocess" workload the report describes.

### 4.2 (A) with a design decision — `fork()` itself

The atom is `kal_space_start(entry, arg, stack_top, out)`. The obstacle is
return-twice semantics: the child must resume at the call site, not at `entry`.

Two candidate designs, and the choice should be made deliberately:

1. **Trampoline.** The parent records a resume point before the call; `entry`
   transfers to it. The copied address space contains the parent's stack at the
   same addresses, so the recorded context is valid in the child. Cost: the
   mechanism is architecture-aware and must be written per architecture.
2. **Refuse `fork()`, provide `posix_spawn`.** `fork()` returns `-ENOSYS`
   consistently; everything that spawns a subprocess goes through 4.1. Cost:
   programs that call `fork()` directly do not run.

⭐ Option 2 is the smaller step and covers the reported workload. Option 1 is
the complete one. They are not exclusive — 2 can ship first and 1 later, and
neither changes the specification.

### 4.3 (B) Composable with fidelity loss — permission bits

`kal_node_info` carries `{ size, modified_ns, kind, writable }` — **one boolean,
not a mode word** — and `kal_fs_open_file` takes `write` and `create` flags, not
a mode.

So `chmod(path, 0600)` cannot be honoured. Three answers, in decreasing honesty:

1. Refuse: `chmod` returns `-ENOSYS`, `stat` reports a mode synthesised from
   `writable` (`0666`/`0444` masked by nothing). The report's "expected 0600,
   got 0777" becomes "expected 0600, refused" — which a caller can act on.
2. Map the owner-write bit onto `writable` and ignore the rest. `chmod(0600)`
   succeeds and `stat` reports `0600` only if the port remembers it; across
   processes it does not.
3. Ask the specification for a permission atom (see 4.4).

⚠️ Option 2 is the one that silently lies, and it is also the one that makes the
most tests pass. It should not be chosen without saying so in the port's own
source.

### 4.4 (C) Not composable — symbolic links, and the permission question

`SURFACE.txt` has **no operation that creates or reads a symbolic link.** What
it has is an acknowledgement that links exist: `kal_node_link` as a node kind
and `KAL_FS_PROP_LINKS` as a property word. An implementation can therefore
*report* a link it encounters and cannot *make* one.

⭐ That asymmetry is itself a finding. A property word declaring support for a
thing the interface offers no operation upon is a promise with no way to keep
it — the shape this ecosystem has recorded before as a specification's silence
being invisible from inside the specification.

Two candidates for 0.9, each to be judged against clause 3 (minimal, universal,
not composable) and clause 6.4 (an operation some resources can never satisfy
does not belong on the interface):

- `kal_fs_symlink(base, name, len, target, target_len)` and
  `kal_fs_readlink(...)`. Universal? Windows has symbolic links but creating one
  requires a privilege by default; a FAT volume has none. Clause 6.2's property
  word is the established answer to "some resources cannot" — `KAL_FS_PROP_LINKS`
  already exists and would finally have a reader.
- A permission operation. Harder: the concept is not universal in the same way
  (a FAT volume, a UEFI system partition and a Windows ACL do not share a model),
  and clause 6.3 records mechanisms considered and not adopted for exactly this
  reason. **The recommendation of this document is to refuse permissions at the
  specification level and choose 4.3 option 1 in the port.**

### 4.5 Independent of the above — the `hidden` macro

`openkal-musl#13`. `port/include/features.h` defines `hidden`, `weak` and
undefines `weak_alias` for every consumer, so a program cannot use `hidden` as
an ordinary identifier.

Measured: **`musl/include/` — the public headers — contain zero uses of
`hidden`.** The block's stated purpose is a different case, and its own comment
says so: a non-musl C source compiled *with the internal overlay on its command
line*, which is what a board building compiler-rt does.

Since mcpp 2026.8.27.1 added `[build] private_include_dirs` and this package now
uses it, an ordinary consumer no longer has the overlay on its line at all. The
fix is therefore to neutralise **only when the overlay is present** — that is,
only when the macro is already defined after `#include_next <features.h>` —
which preserves the compiler-rt case and releases the name to consumers.

---

## 5. Per-repository work

### 5.1 `openkal-musl` — the largest share

| item | category | depends on |
| --- | :-: | --- |
| `hidden`/`weak` scoping (#13) | — | nothing |
| socket family → `kal_net_*` | A | backend provides `openkal.net` |
| datagram family → `kal_datagram_*` | A | `openkal.datagram` |
| `O_NONBLOCK`, `poll`/`select` → `kal_timeout_*` | A | `openkal.timeout` |
| `posix_spawn` → `kal_process_spawn_with` | A | `openkal.process` |
| `fork` (4.2) | A + decision | `openkal.space` |
| `chmod` family (4.3) | B + decision | — |
| `symlink` family | C | 0.9 |

New descriptor kinds beside the existing `OKM_STREAM / OKM_CHANNEL / OKM_FILE /
OKM_DIR`: a pending socket, a connection, a listener, a datagram endpoint.

⚠️ **EVERY NEW ROUTE MUST TAKE A WEAK REFERENCE AND TEST IT BEFORE CALLING**,
as `kal_process_channel` already does (`if (!kal_process_channel) return
-ENOSYS;`). `openkal.net` is optional; a strong reference would turn "this
backend declines the interface" into "this program does not link", making an
optional interface mandatory — which clause 6.1's link-time absence exists to
avoid, and which this port has been bitten by before.

### 5.2 `openkal-macos`, `openkal-windows` — the gating work

They implement ten of fifteen interfaces. The five they decline are the five
0.8 added, and three of them are what section 4.1 is built on.

| interface | macOS | Windows |
| --- | --- | --- |
| net | BSD sockets, directly | Winsock 2, `WSAStartup` at first use |
| datagram | as above | as above |
| timeout | `poll` with a deadline; `SO_RCVTIMEO` | `WSAPoll`, overlapped I/O |
| exec | `mmap(MAP_JIT)` + `pthread_jit_write_protect_np` | `VirtualAlloc` + `FlushInstructionCache` |
| space | ⚠️ macOS: `fork` exists but is unsafe after threads; Windows: no equivalent | see below |

⭐ **`openkal.space` ON WINDOWS IS THE ONE THAT MAY HAVE TO STAY DECLINED**, and
declining it is a legitimate outcome rather than a failure: clause 3 says in
whole or not at all, and clause 6.1 makes the absence a link error naming
`kal_space_start`. The design should not invent a fake.

Until macos and windows provide net/datagram/timeout, the socket work of 5.1 is
Linux-only in effect. That is acceptable and should be stated in the port's
documentation rather than discovered.

### 5.3 `openkal-linux` — small

Already provides all fifteen. Its share is verification: the conformance suite
and the portable program exercise the atoms, but nothing yet exercises them
*through musl*. A test that opens a listener, connects to it, and transfers
bytes — written against POSIX, not against openkal — is the criterion that the
route in 5.1 works.

### 5.4 `openkal-opensbi`, `openkal-uefi` — none

Freestanding. They decline fs and above, and nothing here changes that. The
work is only to confirm that the new routes in openkal-musl remain absent
rather than undefined on these targets, which the weak-reference rule of 5.1
already guarantees.

### 5.5 `openkal-llvm-runtime` — follows, does not lead

`std::filesystem::create_symlink` and `permissions` become available exactly
when 4.3 and 4.4 are answered; `<random>` is already answered. No change is
needed in this repository for section 4.1 — the C++ layer reaches sockets
through the C library, not directly.

⚠️ One item does belong here: the bare-metal row excludes
`libcxx/src/filesystem/*.cpp` while the hosted rows build it. If the hosted
rows are to report a *complete* `std::filesystem`, the exclusions and the
`__config_site` switches must be read against each other once more, in the way
`_LIBCPP_HAS_TERMINAL` was in 0.2.0.

### 5.6 `openkal` — the specification

Only if 4.4 is accepted: `kal_fs_symlink` / `kal_fs_readlink` in 0.9, with
`KAL_FS_PROP_LINKS` gaining a reader, plus `SURFACE.txt`, `SPEC.md` clause 3's
inventory, the conformance suite, and the five backends each providing or
declining in whole.

**Recommendation: do not add a permission operation.** Section 4.3 option 1 in
the port is the honest answer, and clause 6.3 is where the reasoning belongs.

---

## 6. Order, and why this order

1. **`hidden` scoping** — independent, small, and its evidence is already
   measured. Unblocks any consumer that uses the name.
2. **`posix_spawn` + socket/datagram/timeout routes in openkal-musl** — the
   largest capability gain per unit of work, and it needs no other repository
   to move first, because openkal-linux already provides the atoms.
3. **openkal-macos and openkal-windows: net, datagram, timeout** — turns step 2
   from a Linux capability into an ecosystem one.
4. **`fork` decision (4.2), `chmod` decision (4.3)** — both are decisions before
   they are code.
5. **0.9 symlink atoms, if accepted** — last, because it moves the specification
   and therefore every backend.

⚠️ Steps 2 and 3 are independent and can proceed in parallel. Step 3 is the
larger effort and the one that decides whether this ecosystem's story is "POSIX
programs run on Linux" or "POSIX programs run".

---

## 7. Criteria

Each step is judged by a reading, not by a green run:

| step | criterion |
| --- | --- |
| `hidden` | a consumer TU declaring `static int hidden = 7;` compiles, **and** a compiler-rt-style build with the overlay on its line still compiles |
| sockets | a POSIX program — `socket`/`bind`/`listen`/`accept` + transfer — runs over openkal-musl, written against POSIX and naming no openkal symbol |
| `posix_spawn` | `system("…")` and `popen` return a status the caller can read; the child's output arrives through the channel |
| macos/windows net | the same POSIX program, unchanged, on those hosts |
| declines | a program using a declined interface fails at the **link**, naming the operation — never at runtime, never silently |
| every new route | with the backend's interface absent, the call returns `-ENOSYS` rather than jumping through a null pointer |

⚠️ The last row is the one the report `openkal-linux#13` raised directly: a stub
that is a null pointer produces `PC=0` with an empty backtrace, which names
nothing. A guarded weak reference returning `-ENOSYS` is the difference between
a program that fails and a program that cannot say why.

---

## 8. What this document does not settle

- Whether `fork()` gets a trampoline (4.2) or a permanent refusal.
- Whether permissions are refused (recommended) or emulated.
- Whether symbolic links enter 0.9.
- Whether `openkal.space` on Windows is declined permanently.

Each is a decision about what the specification is for, and none of them should
be made by whichever implementation reaches it first.

---

## 9. What was implemented, and what the implementation corrected

**Status**: sections 4.1 through 4.3, 4.5 and 5.1 through 5.5 are implemented.
Section 4.4's specification change is **not**: neither a permission operation
nor a symbolic-link operation was added, and the specification did not move.

The four decisions of section 8, settled:

| decision | outcome |
| --- | --- |
| `fork()` — trampoline or refusal | **trampoline**, and the specification asked for it. `space.h` states in terms that a library above reaches `fork` "by saving its own execution state before the call and restoring it in the started context". `okm_setjmp.S` already carried the per-architecture half. |
| permissions | **refused**, as recommended. `chmod` reports `ENOSYS`; `stat` reports a mode assembled from `writable`. |
| symbolic links in 0.9 | **not now.** The asymmetry (§4.4) stands recorded; nothing composes them, and the specification is unchanged. |
| `openkal.space` on Windows | **declined permanently.** `CreateProcessW` starts a *named program*, which is `openkal.process`. Constructing a copy of the calling address space out of it would be clause 3.1's simulation. |

### 9.1 Five things this document got wrong

⚠️ **`posix_spawn` was never missing.** §4.1 says
`musl/src/process/posix_spawn.c` is excluded and that a port implementation
would restore `posix_spawn`, `system` and `popen`. The exclusion is real and the
conclusion was not: `port/src/okm_spawn.c` has *replaced* that source since the
port was written, and `system` and `popen` work through it. Measured:
`system("exit 5")` returns an exit status of 5, `popen` carries a line back.
What was missing was a criterion, not a capability — `examples/subprocess` is it.

⚠️⚠️ **`timeout_ns = 0` does not mean "do not wait".** §4.1 says `O_NONBLOCK`
and a zero `poll` timeout are "expressible with `kal_timeout_*` at
`timeout_ns = 0`". They are the *opposite*: `timeout.h` defines zero as **no
bound**, following `kal_task_wait`. Passing a caller's zero straight through
turns the one call that must not wait into the one that never returns —
measured, as a hang, four lines into the network probe. The smallest bound is
`1`, which the environment rounds up to its own granularity.

⚠️ **`SYS_clone` is not the only number `fork` arrives through.** musl's `_Fork`
issues `SYS_fork` where the architecture has it and `SYS_clone` where it does
not, so a dispatcher implementing only the second is reached on aarch64 and
riscv64 and never on x86_64.

⚠️ **The macOS row's `exec` is not the clause 6.5 case.** §5.2 gives
`mmap(MAP_JIT)` + `pthread_jit_write_protect_np`. That pair is what a program
needs for memory writable and executable *at the same time*, which
`openkal.exec` does not offer: a region is writable, then published, then
executable, and never both. The implementation is the ordinary `mmap` +
`mprotect`, and the conformance suite calls the published region, so the reading
is settled by the system rather than by the argument.

⚠️ **`SO_REUSEADDR` is not the same option on all three systems.** On Linux and
macOS it permits a listener whose predecessor is lingering; on Windows it
permits two listeners on one address at once. openkal-windows therefore does not
set it, and setting it "for symmetry" would have made that implementation behave
differently while looking the same.

### 9.2 One thing neither this document nor anything else had noticed

⚠️⚠️ **No continuous integration anywhere selected the interfaces this document
is about.** Every backend ran the conformance suite as `full`, which expands to
`standard,abi,stability,cost` — and `standard` is the *hosted* set. The five
interfaces 0.8 added are in `optional`. So every one of their sections was
compiled with its body removed and reported as *not examined*, in the same
release that added them.

Nothing failed. Nothing was checked either. openkal-linux now runs
`full,optional` (143 observations held, 0 failed, 1 not observed);
openkal-macos the same; openkal-windows enumerates the six it provides, because
`optional` names `space`.

### 9.3 The criteria, as they now stand

| criterion | where |
| --- | --- |
| a POSIX program using sockets, datagrams, `poll` and `select` — naming no openkal symbol | `openkal-musl/examples/net`, 35 observations |
| another program started three ways, and the refusal asserted as a refusal | `openkal-musl/examples/subprocess` |
| `hidden`, `weak`, `weak_alias` usable by a program, in C and in C++ | `openkal-musl/examples/identifiers`, `openkal-llvm-runtime/examples/cxx` |
| `std::filesystem` over the port, and `permissions`/`create_symlink` **refused** | `openkal-llvm-runtime/examples/cxx` |
| every new route references its interface **weakly** | `openkal-musl` CI, eleven names, with `kal_time_sleep` as the strong control |
| the five interfaces examined rather than skipped | every backend's conformance run |

⚠️ The weak-reference check found two apparent failures on its first run, and
both were the check's fault: a search under `target/` reaches the *dependency's*
objects, where openkal-linux's `kal_timeout_accept` refers to its own
`kal_net_accept` strongly — which is correct for an implementation and says
nothing about the port.
