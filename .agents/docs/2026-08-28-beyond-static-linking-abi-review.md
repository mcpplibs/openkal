# openkal beyond static linking: an ABI review, and what each repository does about it

**Date**: 2026-08-28
**Scope**: openkal 0.8, its five implementations, and the two consumers on it
**Status**: proposal for review. Nothing here is implemented.

⚠️ **Sections 0–8 were written before the premise was stated in full, and are
left as they were.** Section 9 is a self-review against the premise that a
program built on this interface may one day be **distributed as a binary** and
meet an implementation it was not compiled against. Three of the earlier
recommendations do not survive that premise; section 9 says which, and why.
A document edited to agree with its own review is one nobody can learn from.

Every claim marked **measured** was run on one machine on 2026-08-28 against
openkal-musl 0.6.0 (`f5c8524`), openkal-linux 0.6.0 (`98ee440`),
openkal-llvm-runtime 0.3.1, mcpp 2026.8.28.2, target `x86_64-linux-musl`.
Claims not so marked are readings of source or of `SPEC.md`, and one is marked
**unverified**.

---

## 0. Why this document exists

Four inputs, and the fourth reorders the other three.

1. **A consumer's third round of reports** on `mcpplibs/openkal-linux#13`. One
   of the two items that had resisted two rounds of analysis is now root-caused
   to a single line and a single caller.
2. **The question of what a C library must synthesise and what the interface
   must supply.** The consumer's symptom was `std::filesystem` disagreeing with
   the same source on the host. Part of that is the port's; part is the
   interface's.
3. **The interface is early.** No third party has shipped against a frozen
   openkal ABI. A redundant declaration can be deleted rather than carried, and
   a structure can gain a field. This window is open now and will not reopen:
   clause 8 forbids altering a declaration, and clause 5.3 freezes layouts.
4. ⚠️⚠️ **openkal is intended to be more than a set of symbols a program links.
   It is intended to become a kernel ABI — a runtime interface crossed by a
   trap, a dynamic import, or a service boundary.**

Item 4 changes which *shapes* are admissible, not which *operations*. Three
shapes in openkal 0.8 are shapes only an in-process library can have. They work
today because every consumer statically links its implementation, and each of
them stops working the moment the interface is crossed rather than linked.
**Section 3 is therefore the part of this document with a deadline**, and the
rest is ordered behind it.

---

## 1. What was measured

Seven findings. Two are the consumer's open items; five were found by reading
and then confirmed by running.

### 1.1 `signal(SIGABRT, …)` destroys its caller's stack frame — openkal-musl

**Measured.** A three-line C program reproduces it. No threads, no `fork`, no
terminal library.

```c
#include <signal.h>
static void h(int s) { (void)s; }
int main(void) { signal(SIGABRT, h); return 0; }      /* SIGSEGV, rip = 0 */
```

Every other signal number returns `SIG_ERR` and exits 0. Only 6 dies. The
register state matches the consumer's report field for field: `rip 0x0`, all
general registers zero but one, and the top of the stack is zero rather than a
return address.

The cause is `port/src/okm_syscall.c`:

```c
case SYS_rt_sigprocmask: {
    sigset_t* old = (sigset_t*)a3;
    if (old) for (unsigned i = 0; i < sizeof *old; i++) ((char*)old)[i] = 0;
```

`a4` is the caller's *sigsetsize* — eight bytes on x86_64. This zeroes
`sizeof(sigset_t)`, which is 128. Measured directly: **the caller asks for 8
bytes and 128 are zeroed, destroying 120 bytes it does not own.**

Seventeen callers in musl pass an old-set. Sixteen pass a 128-byte `sigset_t`.
**One does not** — `src/signal/sigaction.c:65` declares
`unsigned long set[_NSIG/(8*sizeof(long))]`, which is one word, and reaches it
only when `sig == SIGABRT`. It sits at `-0x20(%rbp)` in a frame of `0x30`, so
the write reaches the saved frame pointer and the return address. `__sigaction`
then returns to zero.

⚠️ The blast radius is larger than installing a handler. Measured: a plain
**query**, `sigaction(SIGABRT, NULL, &old)`, dies as well, as does
`signal(SIGABRT, SIG_IGN)`. Any program that reads or writes the disposition of
SIGABRT — a test framework's death tests, a crash reporter, a terminal UI
library — ends here.

**Verified fix.** Honouring `a4` removes it. A copy of the released package was
patched and rebuilt; `signal(SIGABRT, h)` then returns `SIG_ERR` like every
other signal, and a C++ probe of the reported shape (six signals, a global
`std::stack<std::function<void()>>`, twelve forks, four threads) runs to
completion.

⭐ **Why the port's own probe did not catch it.** `examples/subprocess` has
thirty-six observations, three of them about `abort`. It contains **no call to
`signal` or `sigaction` anywhere.** It tested whether `abort` ends the program;
it did not test whether a program may touch SIGABRT's disposition. This is the
same shape as the redirection gap of the previous round, whose conclusion was
"seven out of seven was answering a different question".

### 1.2 The sixty-fifth started program reports `EAGAIN` — openkal-musl

**Measured.**

```
80 posix_spawn without waiting        -> #64 fails, errno 11
80 fork without waiting               -> #64 fails, errno 11
200 x (posix_spawn + blocking wait)   -> ok
200 x (posix_spawn + one WNOHANG poll) -> #64 fails, errno 11
```

`okm_syscall.c:392` bounds the started-program table at 64 and releases an entry
only in `wait4`. The last line is the one that matters: a `WNOHANG` poll that
finds the program still running **holds the entry for ever**, and `WNOHANG` only
became usable in 0.6.0.

This is not yet proven to be the consumer's `EAGAIN`, and the earlier claim that
the port has "exactly three producers of EAGAIN" was wrong: `okm_errno` maps
`kal_err_again` to `EAGAIN` for **every** operation, and openkal-linux's
`translate()` maps every kernel `EAGAIN` to `kal_err_again`. The candidate set is
larger than was stated to the consumer.

Also measured, which narrows their search: `std::filesystem::copy` of a tree with
no links produces `ec = 0`, and `create_symlink` produces `ENOSYS`, not `EAGAIN`.
Their suspicion that system call 88 was involved can be dropped.

### 1.3 Enquiry does not resolve a link; opening does — openkal-linux and openkal-musl

**Measured**, same source built twice, links created by the host:

| observation | host | openkal-musl 0.6.0 |
| --- | --- | --- |
| `stat("link-to-file")` | `REG` | **`LNK`** |
| `open("link-to-file")` reads | `CONTENT` | `CONTENT` |
| `fs::is_regular_file` | 1 | **0** |
| `fs::file_size` | 8 | **`ENOTSUP`** |
| `fs::exists("dangling")` | **0** | **1** |
| `it->is_directory()` on a link to a directory | 1 | **0** |
| `fs::copy(recursive)` on a tree with one link | **succeeds** | **`ENOSYS`** |

Isolated: **one symbolic link makes an entire tree uncopyable.** Remove it and
both option sets succeed on both.

Two causes, and neither is the absence of a link operation:

- `okm_syscall.c`'s `do_fstatat` **ignores `flag` entirely**, so `stat` and
  `lstat` are one call;
- `openkal-linux/src/fs.cpp:188` passes `AT_SYMLINK_NOFOLLOW` **always**, while
  `kal_fs_open` at `fs.cpp:127` does not set `O_NOFOLLOW` — the constant
  `o_nofollow` is declared in `sys.h:217` and used nowhere.

⇒ In one program, **opening resolves a link and asking does not.** That is not a
missing operation. It is an operation that reports success having answered a
different question, which is the one outcome this ecosystem's own rule forbids.

⭐ **And it is fixable with no new atom.** `kal_fs_open` resolves the link and
`kal_fs_file_info` then answers about what was opened. Composing the two gives
`stat` its POSIX meaning. **Measured** on a patched build: every row above
matches the host except the last, and `fs::copy(recursive)` — the form that does
*not* ask for `copy_symlinks` — succeeds. Only `copy_symlinks`, which preserves
the link rather than following it, still needs an operation that does not exist.

⚠️ But the composition rests on `kal_fs_open` resolving a link, which
`fs.h` does not state. See §4.6.

### 1.4 Two different files are `equivalent` — openkal-linux

**Measured.** `fs::equivalent("a.txt", "b.txt")` answers **true with `ec = 0`**.

`struct kal_node_info` carries `{size, modified_ns, kind, writable}` and no
identity, so `fill_kstat` writes `st_ino = 0, st_dev = 1` for every node.
`hard_link_count` is a constant 1 for the same reason.

This is the only silently wrong answer in the filesystem surface. `fs::space` and
`fs::create_hard_link` report `ENOSYS`, which a program can act on.

⭐ Identity is not permission and not links. It presupposes no principal and no
format feature: **no resource can fail to answer whether it is the same resource
as another**, so clause 6.4 does not exclude it. Every format has a form of it —
inode, NTFS file index, APFS object id.

*(Unverified: WASI's `filestat`/`descriptor-stat` carry `dev` and `ino` and carry
no mode, and WASI has no `path_chmod`. If that holds it is a second capability
interface reaching the same two conclusions — identity in, permission out. Worth
confirming before it is cited.)*

### 1.5 A property word claimed unconditionally, and one never claimed at all

`kal_fs_props` is one word per implementation. Two of its four positions are
properties of the **format**, not of the implementation:

- **`KAL_FS_PROP_CASE_SENSITIVE`** — openkal-linux claims it unconditionally.
  On the machine this was written on, `/media/…` is exFAT and `/boot/efi` is
  vfat; on a preopen rooted in either, the claim is false.
- **`KAL_FS_PROP_LINKS`** — **no implementation claims it**, yet all three meet
  links and report `kal_node_link`. `SPEC.md` clause 11 item 7 says resolution
  follows a link "where the property is claimed", so openkal-linux resolves one
  while declining to claim it.
- `KAL_FS_PROP_ATOMIC_RENAME` has the same problem for a cross-device rename.

⭐ The specification already names the right mechanism and does not apply it
here. Clause 6.2: *"A property that varies between the resources of an interface
rather than between implementations cannot be a word… Such a property is
reported by an enquiry taking the resource. `kal_stream_props` is the example."*

### 1.6 `getpgrp()` answers −38 — openkal-musl

**Measured.**

```
openkal-musl: no operation for system call 121
getpgrp() = -38
```

System call 121 (`getpgid`) has no case. musl's `getpgrp` is
`return __syscall(SYS_getpgid, 0);` — it does **not** pass through
`__syscall_ret`, because POSIX says `getpgrp` cannot fail. So `-ENOSYS` is
handed to the caller as a process-group identifier. Neither −1 nor an errno: a
value that is wrong and reported as ordinary.

### 1.7 `rt_sigaction` writes 24 of 32 bytes — openkal-musl

The old-action is cleared for `sizeof *act` where `act` is a locally declared
three-field structure. `struct k_sigaction` on x86_64 is four fields and 32
bytes; `mask[2]` is left uninitialised and `__libc_sigaction` copies it to the
caller on the `SIG_DFL`/`SIG_IGN` path. **Read, not measured** — three attempts
on this build read zero, because the stack happened to be zero.

---

## 2. The rule the seven share

Two sentences cover all of them.

> ⭐⭐ **A size or a semantic taken from the type declared locally, rather than
> from the contract being answered.**
> §1.1 takes 128 from `sizeof(sigset_t)` when the caller said 8. §1.7 takes 24
> from a local struct when the ABI says 32. §1.3 takes "do not follow" from one
> call site and "follow" from another. §1.5 takes one answer per implementation
> for a question whose answer is per resource.

> ⭐⭐ **An operation that reports success having answered a different question.**
> §1.3's `stat`, §1.4's `equivalent`, §1.6's `getpgrp`, §1.5's
> `CASE_SENSITIVE`. Each is worse than a refusal, because a refusal is
> actionable and a wrong answer is not.

Both are failures of *criteria*, not of implementation. A conformance test that
asserted "the bytes written equal the bytes the caller declared" would have
caught §1.1 and §1.7 together, and one that compared answers against the same
source on a hosted target would have caught §1.3, §1.4 and §1.6 together. §6
proposes both.

---

## 3. ⚠️⚠️ Shapes that do not survive becoming a kernel ABI

An interface that is *linked* may pass anything the C ABI permits. An interface
that is *crossed* — a trap, a dynamic import, a call into another address space —
may pass only what the boundary can carry. openkal 0.8 has four shapes of the
first kind. All four work today, and all four stop working on the day the
interface stops being linked.

### 3.1 Nine of the eleven property words are exported **data**, not operations

```c
extern const kal_uintptr kal_fs_props;        /* and net, datagram, time,
                                                 random, task, process, space,
                                                 exec */
```

`kal_stream_props` and `kal_terminal_props` are functions and take the resource.
The other nine are objects.

An exported object cannot be trapped, cannot be intercepted by a service layer,
cannot differ per resource, and — where the implementation is not statically
linked — obliges the consumer to carry a copy relocation or an indirection the
interface does not describe. It also cannot answer differently after the
environment changes, which a runtime interface must be able to do.

⇒ **Every `*_props` becomes a function.** Where the property is a property of
the resource, it takes the resource. This is one change and it discharges §1.5
as a side effect.

### 3.2 Four operations return a pointer into the implementation's memory

```c
const char* kal_env_arg   (kal_uintptr index, kal_uintptr* len);
const char* kal_env_var   (const char* name, kal_uintptr name_len, kal_uintptr* value_len);
const char* kal_env_var_at(kal_uintptr index, …);
int         kal_fs_preopen(kal_uintptr index, struct kal_dir* out,
                           const char** name, kal_uintptr* len);
```

and `struct kal_preopen` — whose layout clause 5.3 freezes — holds a
`const char*` for the same reason.

A pointer into the implementation is meaningful only when the implementation is
in the caller's address space. Across a boundary the implementation must copy
into a buffer the caller owns.

⇒ **Copy-out forms**: the caller supplies a buffer and a capacity, and the
operation reports the length it needed. This is the form
`kal_fs_preopen`'s siblings already use for names elsewhere in the interface.

### 3.3 Two operations take a function pointer

```c
int kal_task_start (void (*entry)(void*), void* arg, struct kal_task* out);
int kal_space_start(void (*entry)(void*), void* arg, void* stack_top,
                    struct kal_process* out);
```

A kernel does not call into a program. Starting a context across a boundary is
expressed as a program counter and a stack pointer, not as a callback.

⚠️ This one is not a mechanical change and this document does not settle it.
`openkal.task`'s present shape is the *reason* clause 7.1 gives for not taking a
stack: an environment that does not allocate stacks separately cannot honour a
request for one. A kernel-ABI form must take the stack, and a bare-metal form
cannot. **The two may not be the same operation.** Recorded as the largest open
question in §8.

### 3.4 A two-word structure is returned by value

`struct kal_io_result { kal_uintptr n; int e; }` is returned in two registers by
the C ABI. A trap returns one word. The comment on the structure already states
the constraint it was designed against — "two machine words are returned in
registers on the architectures openkal targets" — which is a statement about a
*call*, not about a *boundary*.

⇒ Either the transfer operations gain an out-parameter form, or the boundary
specifies how a two-word result is carried. Cheaper than §3.3 and should be
decided at the same time.

### 3.5 Clause 6.1's presence mechanism is link-time only

> "An interface that an implementation does not provide is absent as a link-time
> definition, and a consumer that uses it fails to link."

That is the whole of the optionality mechanism, and openkal-musl depends on it:
twenty-seven weak declarations in `port/src` at 0.6.0, each tested before it is
called. Across a runtime boundary there is no link, and a weak reference has
nothing to be weak against.

⇒ A runtime form of the same question is needed. The natural one, given §3.1, is
that a property enquiry for an absent interface is itself the absence — but that
inverts clause 6.2's table, which assigns "was an interface used that the
implementation does not provide" to the linker and "how does this implementation
behave within an interface it provides" to a property. **A third row is needed,
and adding it is a specification change, not an implementation one.**

---

## 4. Corrections to make while nothing is owed

Ordered by cost to a consumer, cheapest first. Items 4.1–4.5 are deletions and
signature changes that are only possible before an ABI is frozen.

### 4.1 Every property word becomes an operation, and takes its resource where the property has one

Discharges §3.1 and §1.5 together.

```c
kal_uintptr kal_fs_props(struct kal_dir d);      /* was: extern const object */
kal_uintptr kal_net_props(void);
…
```

`CASE_SENSITIVE`, `ATOMIC_RENAME` and `LINKS` become answerable honestly; today
none of the three is.

### 4.2 `struct kal_node_info` gains identity, and `size` becomes 64-bit

```c
struct kal_node_info {
    kal_u64 size;          /* was kal_uintptr: 32-bit hosts truncate a large file
                              silently, while kal_fs_seek is already kal_i64 */
    kal_u64 modified_ns;
    kal_u64 volume;        /* identity, with node. Two nodes are the same node
                              when both words agree. Zero in both denotes that
                              the implementation does not distinguish, which a
                              caller must not read as "the same". */
    kal_u64 node;
    int     kind;
    int     writable;
};
```

Discharges §1.4. ⚠️ The `size` change is not cosmetic: `kal_uintptr` on a 32-bit
target truncates at 4 GiB with nothing reporting it, while `kal_fs_seek` on the
same target is already 64-bit. The two disagree today.

### 4.3 A handle should not escape its type

`kal_uintptr kal_fs_stream(struct kal_file)` returns a raw word where every
other operation carries `struct kal_stream`, and `struct kal_spawn_streams`
holds three raw words for the same reason. These are the two places a handle
crosses between interfaces, and they are exactly the places the type is dropped.

The zero-means-two-things defect already recorded against `kal_spawn_streams` —
zero denotes "inherit" while `kal_stdin()` legitimately answers zero — is a
consequence of the same untyping, and the note in `process.h` says it cannot be
repaired because clause 8 forbids altering the declaration. **While nothing is
owed, it can.**

### 4.4 Delete `kal_fs_open_file`

`fs.h` states in terms that the two-flag form "cannot express three conditions a
C library must express", and `kal_fs_open` replaces it. openkal-linux defines it
in terms of the newer form. It is a compatibility remnant of an interface that
has no compatibility to keep.

Ninety names become eighty-nine. Whether other pairs qualify is a question this
document asks and does not answer: a sweep for "operation defined in terms of
another operation of the same interface" should be run before the ABI settles.

### 4.5 `kal_preopen` and the environment operations gain copy-out forms

Discharges §3.2. `struct kal_preopen`'s frozen layout is the one that must
change first, because clause 5.3 will hold it afterwards.

### 4.6 Links: two operations inside `openkal.fs`, not a new interface

⚠️ **An earlier draft of this analysis proposed `openkal.link` as an optional
interface. That was wrong by the specification's own reasoning and is withdrawn.**

Clause 6.2 sorts variability three ways: an operation an *implementation* may
lack becomes its own interface; a property that varies between *implementations*
becomes a word; a property that varies between the *resources* of one
implementation can be neither, and is answered by an enquiry taking the resource.

Clause 11 item 7 places links in the third row: *"whether a filesystem has links
is a property of the format rather than of the environment."* The same
openkal-linux succeeds on ext4 and fails on vfat. Clause 6.4's own worked example
— positioning, which "succeeds for a regular file and fails for a pipe", and
which "accordingly belongs to `openkal.fs`" — is the same shape.

⇒ `kal_fs_link_create` and `kal_fs_link_read` belong to `openkal.fs`, and their
availability is answered by 4.1's per-resource `kal_fs_props`. That enquiry is
what makes the arrangement legal: clause 6.2 objects to an operation that is
present and always fails **because the caller cannot tell**, and a per-resource
property tells it.

Two facts the divergence table must carry, neither of which is a Linux fact:

- Windows requires `SeCreateSymbolicLinkPrivilege` or developer mode, so the
  operation **exists and may still fail** — which a compile-time arrangement
  cannot express and a runtime error can;
- Windows distinguishes a file link from a directory link at creation, and POSIX
  does not, so **a directory link to a target that does not yet exist cannot be
  created there.**

And the sentence clause 11 item 7 already contains — that resolution follows a
link — must move into `fs.h`, beside `kal_fs_info` and `kal_fs_open`, where an
implementer reads it. It is in the specification and in neither header, which is
why two call sites in one file chose opposite directions.

### 4.7 Permissions stay out, and the substitute is recorded

⚠️ **A `kal_fs_restrict` operation was proposed in the same earlier draft and is
also withdrawn.** Clause 11 item 6 is stronger than it was given credit for, and
this measurement is why:

```
exFAT volume, this machine
  created:      -rwxr-xr-x speak:speak
  chmod 0600 -> exit 0                     <- reported success
  after:        -rwxr-xr-x speak:speak     <- nothing happened
```

Mode is not stored by FAT, exFAT or ISO9660, and NTFS stores an access-control
list rather than a mode. On this machine the mount options say it outright:
`/boot/efi` is `vfat fmask=0077,dmask=0077`, the exFAT volume is
`uid=1000,gid=1000,fmask=0022,dmask=0022`, and the ntfs3 volume is
`uid=1000,gid=1000` — so on the first two the whole of the mode, and on all
three the owner, is a mount parameter and not data on the medium. Consequently
every file on such a volume reports the same mode. Permission is not a property of the
format and not a property of the application: it belongs to the kernel's
access-control layer, which openkal does not have, because it is a capability
interface and a capability interface has no principal for a permission to name.

`kal_node_info.writable` — one boolean — is not a simplification of a mode word.
It is the **intersection** of what these formats store.

⇒ The three answers a program actually has, none of which is a mode:

1. against another part of the same program — the capability already does it;
   a handle not given cannot be reached;
2. against another user of the machine — the **environment's** responsibility;
   the party that starts the program supplies a private preopen;
3. against an untrusted location — **encrypt the contents**. This is the only
   one that is an application-layer answer, and it is the only one that holds on
   exFAT, on a container overlay, and under `CAP_DAC_OVERRIDE`.

⇒ `SPEC.md` item 6 should carry these three sentences. Today it records the
refusal without recording the substitute, so each consumer rediscovers it.

---

## 5. Per repository

### openkal

| | change | §ecti | breaks ABI |
| --- | --- | --- | --- |
| 1 | property words become operations; fs takes a `kal_dir` | 4.1, 3.1 | yes |
| 2 | `kal_node_info`: identity, 64-bit size | 4.2, 1.4 | yes |
| 3 | typed handles at `kal_fs_stream` and `kal_spawn_streams` | 4.3 | yes |
| 4 | delete `kal_fs_open_file`; sweep for other superseded forms | 4.4 | yes |
| 5 | copy-out forms for `env` ×3 and `kal_fs_preopen`/`kal_preopen` | 4.5, 3.2 | yes |
| 6 | link resolution stated in `fs.h`; `kal_fs_link_{create,read}` | 4.6, 1.3 | additive |
| 7 | item 6 records the substitute for permissions | 4.7 | no |
| 8 | a runtime form of clause 6.1 | 3.5 | specification |
| 9 | `kal_task_start` / `kal_space_start` across a boundary | 3.3 | open |

Items 1–5 are the ones the open window exists for. They should land as one
version, because five separate ABI breaks cost five migrations and one costs one.

### openkal-linux

- claim `KAL_FS_PROP_LINKS` where the resource has links, per 4.1; stop claiming
  `CASE_SENSITIVE` unconditionally (§1.5);
- supply identity in `fill_info` from `st_dev`/`st_ino` (§1.4);
- state, at `kal_fs_open` and `kal_fs_info`, which of the two resolves a link and
  why — the constant `o_nofollow` is declared and unused, which reads as an
  intention nobody carried out (§1.3);
- implement `kal_fs_link_{create,read}` on `symlinkat`/`readlinkat`.

### openkal-macos

- the same four, over `symlinkat`/`readlinkat` and `st_dev`/`st_ino`;
- ⚠️ it is the row where the fork composition already diverges
  (`kal_task_current` answers a new value in a copy), so it is the row where
  §3.3's decision must be tested, not the Linux one.

### openkal-windows

- identity from `FILE_ID_INFO` (`GetFileInformationByHandleEx`);
- links via `CreateSymbolicLinkW` and `FSCTL_GET_REPARSE_POINT`, with the two
  divergences of §4.6 recorded rather than smoothed;
- it is the row that already declines `openkal.space`, so it is the row that
  demonstrates whether §3.5's runtime absence form is adequate.

### openkal-musl

Independent of everything above, and shippable first:

1. ⚠️ **`SYS_rt_sigprocmask` honours `a4`** (§1.1). One line. Highest priority in
   the ecosystem: it is a crash, it is reached by ordinary C++ programs, and the
   fix is verified.
2. `SYS_rt_sigaction` takes its size from `struct k_sigaction`, not from a local
   declaration (§1.7).
3. `do_fstatat` composes the follow form from `kal_fs_open` +
   `kal_fs_file_info` (§1.3). **Verified**: this alone brings six of seven
   `std::filesystem` answers into agreement with the host and makes
   `fs::copy(recursive)` succeed on a tree containing a link. It should be
   written against 4.6's sentence rather than against openkal-linux's behaviour.
4. a case for `SYS_getpgid` answering the identity `getpid` reports, so
   `getpgrp` stops returning −38 (§1.6);
5. the started-program table: release an entry for a program that has ended, or
   state the bound of 64 in the README beside the 1024 descriptors and 512 open
   descriptions that are already stated (§1.2);
6. a case for `SYS_membarrier` returning `ENOSYS` with the reason beside it. The
   call comes from musl's `pthread_create` calling `__membarrier_init`, **whose
   result is assigned to nothing**; it is trace noise and nothing else. One of
   the six numbers the consumer reported is a false alarm, and the cost of that
   falls on them.
7. ⭐ a probe that calls `sigaction` for every signal number, in all three forms —
   install, ignore, query — and admits only two answers: success, or −1 with
   `ENOSYS`. The third answer, which is what happens today, is that the process
   no longer exists.

### openkal-llvm-runtime

No change required by any item above. It is, however, the layer at which the
criterion of §6.2 is cheapest to run, because it is the layer that has libc++.

### openkal-opensbi, openkal-uefi

Neither provides `openkal.fs` — `openkal-uefi` has no `kal_fs` name at all, and
`openkal-opensbi` deliberately defines no `kal_fs_props` so that clause 6.1
reports the absence at the link. **They are therefore not in the denominator for
links or identity**, and an earlier version of this analysis wrongly cited them
as the reason links must be optional. The reason is a future format-limited
implementation, not these two.

⚠️ They are, however, the rows that §3.3 and §3.5 must not break. A bare machine
has no boundary to cross and no loader to negotiate with; a change made for the
kernel-ABI case that costs the bare-metal case its static form has traded the
whole premise.

---

## 6. Criteria

Two, and each would have caught a group of §1 rather than one item.

### 6.1 A written byte count equals the declared byte count

For every operation that fills a caller's buffer, assert that the bytes written
equal the bytes the caller said it owned. §1.1 and §1.7 are one criterion apart.

In openkal-musl this is cheap: a probe places a canary after an object of the
size musl declares, calls through, and counts leading zeros. Measured today, that
probe reports `120 bytes clobbered beyond the object the caller owns`.

### 6.2 The same source, two targets, compared field by field

A test that builds one program for a hosted openkal target and for the host's own
toolchain, over a tree it creates itself — a file, a directory, a link to each,
a dangling link — and asserts that `stat`, `lstat`, `readlink`, `fs::exists`,
`fs::is_regular_file`, `fs::file_size`, `fs::equivalent` and `fs::copy` **agree
field for field**.

⭐ This criterion needs no ability to *create* a link, so it can be added before
4.6 and keeps its value afterwards. It catches §1.3, §1.4 and §1.6 at once, and
it is a criterion a consumer can contribute — which is worth more than one the
implementer writes, because it does not inherit the implementer's assumptions.

---

## 7. Sequencing

1. **openkal-musl §1.1**, alone, released immediately. It is a crash with a
   verified one-line fix and it blocks a consumer today.
2. **openkal-musl §1.3, §1.6, §1.7, and criterion 6.1.** No interface change.
3. **openkal: items 1–5 of §5 as one version.** One ABI break, not five.
4. **The three implementations adopt it**, and criterion 6.2 goes into whichever
   repository can run libc++.
5. **Links (4.6)** after the per-resource property exists, because they depend on
   it for legality.
6. **§3.3, §3.4 and §3.5** — the kernel-ABI questions — as a separate document.
   They are not fixes; they are a design, and the design has a prerequisite this
   document cannot supply: what the boundary actually is.

---

## 8. What this document does not settle

1. ⚠️⚠️ **What "kernel ABI" means concretely.** A trap, a dynamic import and a
   call into a service are three different boundaries with three different sets
   of admissible shapes. §3 lists what fails under *any* of them; it cannot list
   what is required until one is chosen. **This is the prerequisite for
   everything in §3 except 3.1 and 3.2, which fail under all three.**
2. **Starting a context across a boundary** (§3.3). The present shape is the one
   clause 7.1 argued for. A kernel-ABI shape needs a stack and a program counter.
   Whether these are one operation with a property, two operations, or two
   interfaces, is undecided.
3. **Whether the consumer's `EAGAIN` is §1.2.** Their `ec.value()`, `path1`,
   `path2` and `what()` are still needed, plus one `strace` to tell a kernel
   `EAGAIN` from one this layer manufactured.
4. **The consumer's remaining test failures.** Their permission-bit group is
   §4.7's third answer and is theirs to change; the rest was not reproduced here.
5. **Whether any other superseded operation exists besides `kal_fs_open_file`**
   (§4.4). A sweep is proposed and has not been run.
6. **The WASI comparison** cited in §1.4 is unverified.

---

## 9. Self-review: the premise is binary distribution, not a kernel ABI

Sections 0–8 read "kernel ABI" as *the interface is crossed rather than linked*.
The premise is stronger: **a program built against openkal may one day be
shipped as a binary and meet an implementation it was not compiled against.**
Crossing a boundary is a property of one call. Binary distribution is a property
of the whole relationship, and it constrains version skew, discovery,
extension and testing — none of which sections 0–8 address.

Twelve findings. Three overturn a recommendation this document already made.

### 9.1 ⚠️⚠️ The document proposes a cleanup where a mechanism is needed — and one mechanism replaces three of its proposals

§4.2 adds fields to `kal_node_info` "while nothing is owed". That is correct for
today and answers nothing about tomorrow: a program compiled against a 48-byte
`kal_node_info` calling an implementation that writes 24 bytes reads its own
uninitialised stack, which is §1.1 again with the roles reversed.

The window is worth spending on a **shape**, not on a field. The shape is the
one `statx` uses:

```c
int kal_fs_info(struct kal_dir base, const char* name, kal_uintptr len,
                kal_uintptr want,          /* which answers the caller needs   */
                struct kal_node_info* out, /* whose first field is its size    */
                kal_uintptr* got);         /* which answers were in fact given */
```

⭐ **This single shape subsumes three separate proposals in §4.** Extension:
the implementation writes no more than `out->struct_size` and reports what it
filled. Per-resource variability (§4.1, §1.5): "does this resource have links,
is it case-sensitive" is answered by the same `got` word, per call, on the
resource the caller named. Availability of a link operation (§4.6): the same.
And it is cheaper — an implementation need not compute an identity nobody asked
for.

⇒ **§4.1, §4.2 and part of §4.6 should be withdrawn and replaced by this.**
The per-resource `kal_fs_props(kal_dir)` of §4.1 is still needed for properties
that are not about a node, but the node-level ones move here.

### 9.2 ⚠️⚠️ Nothing in the ecosystem has the shape the premise requires, and the criterion cannot be constructed today

openkal's conformance CI is a matrix of `{os, toolchain, implementation}`:

```
ubuntu-24.04  gcc@16.1.0   openkal-linux    full,optional
ubuntu-24.04  llvm@22.1.8  openkal-linux    full,optional
macos-14      llvm@20.1.7  openkal-macos    full
windows-2022  llvm@20.1.7  openkal-windows  full
```

**Every row recompiles.** There is no row in which one artifact meets a second
implementation, and §6.2's criterion — same source, two targets — does not test
that either. The property binary distribution *promises* is: **the same binary,
two implementations of one target.**

⚠️ And it cannot be written today, because **there is exactly one implementation
per target.** Late binding has no second party. Worse, the choice is made at
dependency resolution and compiled in — openkal-musl names
`openkal-linux = { features = ["standalone"] }` in its manifest — so there is no
artifact in this ecosystem whose implementation is late-bound. The property is
not merely untested; **no artifact of that shape exists to test.**

⇒ Before the ABI is broken, one of two things must happen: a second
implementation for one target (an instrumented pass-through over openkal-linux
is enough, and would be cheap), or an explicit statement that binary
distribution is unverified. This ecosystem's recorded failure mode is the second
without the statement — every repository's CI substitutes the working tree, and
the published form was found to differ from the development form only after
eight packages had shipped.

### 9.3 ⚠️⚠️ Clause 3.2 forbids the one primitive a late-bound consumer needs, and clause 6.2 contradicts a trap ABI

§3.5 said "a third row is needed". The sharper statement is that two existing
clauses now conflict with the premise.

**Clause 3.2**: *"The set is therefore closed at `openkal.abort`,
`openkal.stream` and `openkal.memory`, and a later version shall place a new
interface outside it even where every implementation that exists at the time
would satisfy it."*

A late-bound consumer must ask **before calling** whether an interface is there.
Under a link that question is the linker's. Under a trap there are no symbols,
so it must be an operation — and an operation every implementation provides is
a core operation, which clause 3.2 forbids adding.

**Clause 6.2**: *"An operation that is present and always fails is a defect; the
remedy is that its absence be expressed by its absence."* Under a trap, absence
*can only* be expressed as a returned error. So the rule as written excludes the
arrangement the premise requires.

⇒ These are not gaps to fill later. They are two rules that the premise
falsifies, and the specification must either qualify them or record that the
distributed form is governed by a different clause. §3.2's reasoning — that a
wrongly-placed core interface excludes a bare machine from conformance — still
holds and is not being argued against; what is being argued is that a
*negotiation* operation is not an interface in that sense.

### 9.4 ⚠️ Clause 6.5 inverts, and the document never mentions it

*"An operation whose availability is decided by how the artifact is produced is
not reported at run time."* `openkal.exec` is the case: memory a program may
execute is granted only to an artifact carrying a signed declaration, so the
interface is granted or withheld at dependency resolution.

Under binary distribution the artifact is produced **once, by the producer, for
every environment**. The consumer no longer resolves it; the producer decided
for them, possibly years earlier and for a different system. The clause's own
justification — *"a path no artifact takes is a path nothing has verified"* —
argues for compile-time resolution precisely because the artifact is built for
its environment. Remove that assumption and the argument reverses.

⇒ Either `openkal.exec` is not distributable, or 6.5 needs a distributed form.
Not decided here; recorded because §3 omitted it entirely.

### 9.5 ⚠️ The artifact must carry its own floor, and nothing does

Clause 5.2 grew the error set by five values in 0.5. A program compiled against
0.8 that distinguishes `kal_err_not_found` from `kal_err_invalid` gets
`kal_err_invalid` for both from a 0.4 implementation — **a wrong answer, not a
refusal**, and precisely the shape §2 names.

Today the floor lives in `mcpp.toml`. A manifest does not travel with a binary.
⇒ A distributed artifact must declare the interface versions it requires in the
artifact itself — an ELF note, or a first call that states the requirement and
is refused — so that skew is a refusal at load rather than a wrong answer at
run. §4 has no item for this and should.

### 9.6 ⚠️ A binary-distribution defect that already exists: no operation reports the mapping granularity

`openkal.memory` is `kal_alloc` and `kal_free` and nothing else. `exec.h` refers
to *"the environment's page granularity"* and does not expose it. **No operation
anywhere in openkal reports a page size.**

So openkal-musl hardcodes it — `okm_start.c`: `libc.page_size = 4096`, and the
synthetic auxiliary vector carries `AT_PAGESZ = 4096`. Correct for a build aimed
at a known machine. **Wrong on any machine with 16 KiB or 64 KiB pages**, which
is exactly what a distributed binary meets: Apple Silicon, aarch64 servers
configured for 64 KiB, ppc64le.

⇒ A new §1 item, and an argument for an enquiry rather than a constant. It is
also the clearest small example of the whole premise: a value that is a property
of the *build* today and must become a property of the *run*.

### 9.7 §3.1 undercounts

Ten exported data objects, not nine: the nine `*_props` plus
`extern const kal_uintptr kal_timeout_granularity_ns`. The tenth is the same
defect and the same fix.

### 9.8 `struct kal_endpoint` freezes a needless per-target difference

`kal_uintptr addr_len` holds a value that is 4, 16 or 20. On a 32-bit target the
frozen layout differs from the 64-bit one for no reason the interface needs.
`kal_u32` costs nothing and removes one axis from the distributed form. Same
window as §4.

### 9.9 Clause 7.2 does not require a forged handle to be refused

*"A handle occupies one machine word and is opaque… A handle shall be meaningful
in the context of the caller that obtained it."* That is a statement about
**scope**, not about **validity**. Nothing requires that a word the caller never
obtained be refused rather than acted upon.

Under a static link this is academic. Under a kernel ABI it is the security
boundary. openkal-linux happens to be safe — `handle.h` packs an index with a
generation and `unpack` rejects a mismatch — but it is safe by choice, not by
requirement, and the other two implementations were not examined for it.

⇒ Clause 7.2 needs one sentence: an implementation shall refuse a word that is
not a handle it issued, and shall not treat it as one it did.

### 9.10 The §4.4 sweep is now run, and it found one

Searching every implementation for an operation defined in terms of another
operation of the same interface returns exactly one supersession:
`kal_fs_open_file`, which builds a flag word and calls `kal_fs_open`.
`kal_timeout_accept` calling `kal_net_accept` after a bounded wait is
composition across two interfaces, which is the arrangement working. So §4.4's
open question closes: **one deletion, ninety names to eighty-nine.**

### 9.11 One thing survives late binding, and it survives by accident

openkal-musl's twenty-seven weak declarations were written for bare metal, where
an absent interface must not break the link of a program that never calls it.
Under dynamic linking a weak undefined symbol resolves to zero, and the port
already tests every one before calling — with a CI assertion over eleven names
and a required interface as the control.

⇒ **This is the only part of the optionality mechanism that already works under
the premise**, and it works because it was written for the opposite extreme.
Worth stating in §3.5, which presently reads as though nothing survives.

### 9.12 ⚠️⚠️ §7's sequencing is wrong

§7 puts the ABI break (items 1–5) at step 3 and the boundary questions at step 6,
"as a separate document". Under this premise that spends the one window on a
design the boundary has not yet constrained — and §9.1 has already shown the
boundary changes what the break should contain.

**Corrected order:**

| | | why |
| --- | --- | --- |
| 1 | openkal-musl §1.1 alone, released now | a crash, one line, verified, blocks a consumer |
| 2 | openkal-musl §1.3, §1.6, §1.7, §9.6, criterion 6.1 | no interface change |
| 3 | **decide the boundary far enough to fix the enquiry shape** (§9.1) and the negotiation question (§9.3) | these decide what the break contains |
| 4 | **build the second implementation** for one target (§9.2) | otherwise the break cannot be verified |
| 5 | one ABI break: §9.1's shape, §4.3, §4.4, §4.5, §9.5, §9.7, §9.8, §9.9 | one migration, not five |
| 6 | links (§4.6) over the new enquiry; three implementations adopt | depends on 5 |

Step 4 is the one this ecosystem is most likely to skip, and §9.2 is the reason
it must not be.

---

## 10. What the self-review does not settle

1. **Which boundary.** Unchanged from §8.1, and now load-bearing for the
   sequencing rather than only for §3.
2. **Whether `openkal.exec` is distributable at all** (§9.4).
3. **Whether a negotiation operation can exist without reopening clause 3.2**
   (§9.3). A trap-number bootstrap degrades — an unknown number answers
   `ENOSYS` — but a dynamic-symbol form has no such fallback for the query
   symbol itself.
4. **What the second implementation of §9.2 should be.** An instrumented
   pass-through over openkal-linux is the cheapest thing that makes the property
   testable; whether it is worth maintaining is a judgement this document does
   not make.
5. **Whether a distributed artifact declares its floor as a note or as a call**
   (§9.5). A note is checkable by a loader and invisible to a trap ABI; a call
   is the reverse.

---

## 11. Design, restated from first principles

Sections 0–10 were repair. This section is design, under four statements of
intent that arrived after them and that change several answers:

- openkal is to be **compatible, general, simple and elegant**;
- **static linking now, a runtime ABI later**, and the second must not be a
  break of the first;
- ⚠️⚠️ **no operating system decides how openkal is designed.** openkal states a
  model; others implement it and use it;
- the design is to be **good for the implementer and good for the user**, and
  where those pull apart the pull is itself the thing to design against;
- there is **no compatibility burden today**. The parties are openkal, the
  implementations this community maintains, and one C library.

Three of this document's own recommendations do not survive that, and one of its
findings was simply wrong.

### 11.0 ⚠️ §9.2 was wrong

§9.2 said the binary-distribution criterion "cannot be constructed today,
because there is exactly one implementation per target". That conclusion rested
on an assumption that was not checked: that an implementation must be linked in.

**mcpp supports `kind = "shared"`.** An openkal implementation can be a shared
object, a consumer can be linked against it dynamically, and the pairing can
then be changed without recompiling the consumer. The criterion is constructible
now, and §11.7 states it.

⭐ The failure was the one this document names in §2 — a conclusion drawn from
the shape of what exists rather than from what the tools can express. It was
reached without reading mcpp, which is one command away.

### 11.1 Five rules, each with the check that enforces it

The intent above is not directly checkable. These five are.

**R1 — No environment's shape.**
*Check*: for each operation, name two environments that satisfy it differently.
If satisfying it on the second requires simulating the first, the shape is the
first environment's and the operation is wrong.
*What it catches today*: §11.2 (one page size is Linux's shape; Windows has
two), §11.5 (`dev`/`ino` is Unix's shape), §11.6 (an ELF note is one object
format's shape).

**R2 — Everything an implementation reports about itself is an operation.**
Bits go in a property word; a magnitude gets an operation of its own; anything
that varies between resources takes the resource.
*Check*: the surface contains no exported data object.
*What it catches today*: ten data objects — nine `*_props` and
`kal_timeout_granularity_ns` — against two that are already operations
(`kal_stream_props`, `kal_terminal_props`) and one that already is
(`kal_time_monotonic_granularity`). **The interface already contains both
spellings of the same idea.** One rule removes the inconsistency, makes every
report boundary-agnostic, and lets an answer differ per resource where it must.

**R3 — A value a consumer would otherwise fix at build time must be obtainable
at run time.**
*Check*: grep a consumer for constants describing the environment. Each is
either a fact about the target's ABI (word size, endianness — legitimately
fixed) or a fact about the machine (granularity, a bound — a gap).
*What it catches today*: `libc.page_size = 4096` and `AT_PAGESZ = 4096` in
openkal-musl's `okm_start.c`, and `OKM_PAGE` in `fill_kstat`.

**R4 — A bound is part of the contract or it is a defect.**
A limit a caller cannot learn produces a failure on an operation that has
nothing to do with it.
*Check*: every fixed-size table in an implementation is either reported by an
operation or documented with the error it produces when exhausted.
*What it catches today*: §11.3.

**R5 — The interface reports what its own operations need, not what the machine
has.**
*Check*: for each reported value, name the operation of this interface that a
caller uses it for. If there is none, it is a fact about the environment that
this interface has no business carrying.
*What it catches today*: it is why §11.2 reports a granularity and not a page
size, and why it does not report a protection granularity.

### 11.2 The granularity, designed rather than patched

**Start from the question, not from the value.** Who needs it?

| asker | needs |
| --- | --- |
| `openkal.exec` | the alignment an executable region has — already stated as "the environment's page granularity or coarser", and **already the implementation's job**, so the caller does not need the number |
| `kal_alloc(size, align)` | the caller states an alignment; the implementation satisfies it. The caller does not need the number |
| a C library | `sysconf(_SC_PAGESIZE)`, `getpagesize()`, `st_blksize`, and rounding before a mapping request |

⇒ **Only the third is real**, and it is real not because openkal's operations
need the number but because a C library's own standard obliges it to answer.
That is a legitimate reason and a narrower one, and it argues for the smallest
honest answer rather than a memory-parameters interface.

**Is it one value? R1 says check a second environment.**

| environment | allocation granularity | protection granularity |
| --- | --- | --- |
| Linux, macOS | 4 KiB / 16 KiB / 64 KiB | the same |
| **Windows** | **64 KiB** (`VirtualAlloc`) | **4 KiB** |
| a machine with no memory management unit | none | none |

⚠️ **A design derived from Linux reports one number and is wrong on Windows.**
This is the smallest complete demonstration of R1 in the whole document.

**The design.** One operation, one number, defined so that it is always safe:

```c
/* openkal.memory, core.
 *
 * The quantum this environment allocates and protects memory in. An address
 * and a length that are multiples of it are acceptable to every operation of
 * this specification that takes memory; a smaller quantum may or may not be.
 *
 * An implementation with more than one such quantum reports THE COARSEST, so
 * that a caller which rounds to this value is never wrong. An environment with
 * no such quantum reports 1, which is the same statement: every address and
 * every length is acceptable. */
kal_uintptr kal_memory_granularity(void);
```

- ⭐ **One number and no explanation of when it applies.** Windows answers
  64 KiB; Linux answers its page size; a bare machine answers 1. A caller that
  rounds to it is correct everywhere, and there is no second number to get
  wrong.
- ⭐ **The name is not "page".** "Page" is an operating system's word for a
  mechanism openkal does not have. The value is a granularity, and that is what
  it is called (R1).
- ⭐ **No protection granularity is reported**, because openkal has no operation
  upon a mapping's protection — the divergence table already records `mprotect`
  as absent. Reporting a number no operation of this interface can act upon
  would be reporting a fact about the machine (R5).
- ⭐ **Cheap for the implementer.** A constant is a legal answer, and 1 is a
  legal answer. Nothing has to be discovered, cached or invalidated.

⚠️ **The honest cost, stated rather than hidden.** On Windows a C library above
this reports `_SC_PAGESIZE` as 64 KiB, which is coarser than the machine's page.
A program that uses it to *align* is correct. A program that uses it to *size a
buffer* allocates sixteen times what it needed. openkal chooses coarse-and-always-correct over exact-and-sometimes-wrong, and the divergence table says so.
The alternative — two operations — moves the choice to every caller, and most
callers will pick wrong.

**What the consumer does.** openkal-musl deletes both 4096s and answers
`sysconf(_SC_PAGESIZE)`, `AT_PAGESZ` and `st_blksize` from the operation. That is
§9.6 closed.

### 11.3 The same rule applied: bounds that are invisible today (R4)

| bound | where | what a caller sees today | verdict |
| --- | --- | --- | --- |
| **maximum name length** | openkal-linux `terminated`'s `char buf[4096]`; `g_cwd[4096]` | `kal_err_invalid` — **indistinguishable from a malformed name**, and openkal says nothing about a bound at all | ⚠️ an openkal-level gap: an operation, or a documented refusal value distinct from "invalid" |
| started programs | openkal-musl, 64 | `EAGAIN` on the next spawn, which the caller cannot attribute | implementation-level; state it, or release entries for programs that have ended |
| execution contexts | openkal-musl, `OKM_CONTEXTS 512` | `kal_abort` with a message naming the cause | acceptable — it fails loudly and says why; state it anyway |
| descriptors, open descriptions | openkal-musl, 1024 / 512 | stated in the README | already right |

Only the first is openkal's. It is the same defect as the granularity in a
different dress: a build-time constant in the implementation that the caller
cannot learn and cannot distinguish from its own error.

### 11.4 The enquiry shape, re-derived without naming another system

§9.1 proposed a `want`/`got` shape and justified it as "the one `statx` uses".
Under R1 that justification is inadmissible even if the shape is right. Here is
the derivation from openkal's own situation:

1. **What a name refers to is not one fact.** Size and kind exist in every
   environment. Modification time, identity and whether the resource has links
   exist in some. §1.5 established that these vary *between the resources of one
   implementation*, which clause 6.2 says can be neither an interface nor a
   word.
2. **A caller rarely wants all of them**, and an implementation that computes an
   identity for a caller that asked for a size has done work nobody wanted.
3. **A caller built against a later revision must be able to ask an earlier
   implementation and be told what it received** — otherwise it reads its own
   uninitialised memory, which is §1.1 with the roles exchanged.

Three requirements, three mechanisms, and ⭐ **they must not be conflated —
which is what §4.1 and §4.2 did**:

| requirement | mechanism | answers |
| --- | --- | --- |
| 3 | a size the caller writes into the structure | *how much of this exists on your side* |
| 1 | a word the implementation writes | *which of it is true for this resource* |
| 2 | a word the caller writes | *which of it I need* |

```c
struct kal_node_info {
    kal_uintptr self_size;   /* the caller sets it; the implementation writes no more */
    kal_u64     present;     /* what the implementation filled                        */
    kal_u64     size;
    kal_u64     modified_ns;
    kal_u64     identity[2];
    int         kind;
    int         writable;
};

int kal_fs_info(struct kal_dir base, const char* name, kal_uintptr len,
                kal_u64 wanted, struct kal_node_info* out);
```

⚠️ **The implementer's side must stay one line.** An implementation that always
has everything writes a constant into `present` and ignores `wanted`. If it does
not stay one line, the shape is wrong. This is the "good for the implementer"
half of the intent made into a test.

⭐ And `size` becomes `kal_u64` rather than `kal_uintptr` here rather than as a
separate item: a file's length is not a property of the caller's word size, and
`kal_fs_seek` already agrees.

### 11.5 Identity, redesigned away from `dev` and `ino`

§4.2 proposed `kal_u64 volume` and `kal_u64 node`. Those are one family's words
(R1). What a caller actually needs is to answer *"is this the same object as
that one"* — for `fs::equivalent`, and for cycle detection in a recursive walk.
It needs a **value**, because a walk puts it in a set; a predicate would not do.

```c
kal_u64 identity[2];
/* Opaque. Two nodes are the same node when both words are equal. Not
 * interpretable, not ordered, and not required to survive a restart. An
 * implementation that cannot distinguish nodes does not set the bit for it in
 * `present', and a caller is then told that it does not know rather than being
 * told that two nodes are the same. */
```

Better for the implementer: an inode pair, a file index, an object id, or
nothing — and nothing is a legal answer. Better for the user: a value with a
stated meaning, and an honest absence in place of §1.4's silent `true`.

### 11.6 Audit: which of this document's own proposals wore an environment's shape

R1 applied to §3, §4 and §9.

| proposal | whose shape | verdict |
| --- | --- | --- |
| `want`/`got` enquiry | named after one system; justified by three properties of openkal | **keep**, re-derived in 11.4 |
| identity as `volume`/`node` | Unix | **replaced** — 11.5 |
| one page size | Linux (Windows has two) | **replaced** — 11.2 |
| version floor as an **ELF note** (§9.5) | ⚠️ **ELF is one object format**; openkal targets Mach-O and PE and intends a trap | **replaced** — the floor must be an *operation* the consumer performs before it uses anything, because every boundary has operations and only one has notes |
| `stat` follows / `lstat` does not | POSIX vocabulary, but the two questions are genuinely distinct | **keep**, renamed: the flag says *resolve* or *do not resolve*, not *stat* or *lstat* |
| `kal_fs_link_{create,read}` | a format's concept, not a kernel's; clause 11 item 7 already reasons this way | **keep** |
| `kal_fs_restrict` | POSIX mode thinking | already withdrawn — §4.7 |
| `kal_io_result` → out-parameters | justified as "a trap returns one word", which is a kernel's fact | **keep on a different ground**: openkal decides which boundaries it intends to be expressible across, and that decision is its own |
| props become operations | openkal's own inconsistency, not anyone's shape | **keep** — R2 |

⭐ Four of nine wore someone else's shape, and three of those were found only by
applying R1 deliberately. That is the argument for R1 being written down rather
than assumed.

### 11.7 The ABI test, constructible today (replaces §9.2)

Because mcpp builds shared libraries:

1. build openkal-linux with `kind = "shared"`;
2. build one probe program against openkal, linked dynamically, **once**;
3. run that one binary against two shared objects:
   - openkal-linux itself;
   - ⭐ **a thin interposer over it** that answers a *different* granularity,
     fills a *different* `present` word, declines one optional interface, and
     reports an *older* version floor;
4. assert the binary behaves as specified against both — including refusing to
   run against the older floor.

⭐ **The interposer is the second implementation §9.2 claimed did not exist, and
it is one file.** It is also the only way to exercise the paths that have no
other producer: negotiation, an absent optional interface at run time, a version
floor that is too low, and a `present` word with a bit clear.

⚠️ It must be a *separate artifact*, not a build feature of openkal-linux. A
feature is chosen at dependency resolution and compiled in, which is exactly the
arrangement this test exists to escape.

### 11.8 Static now, runtime later, without a break in between

The question is not "how do we build the runtime ABI". It is **"what must be
true now so that the runtime ABI is an addition and not a break"**. Three
categories:

**Decide now, costs nothing later, and is right for static linking anyway:**

- every report is an operation (R2);
- no operation returns a pointer into the implementation's memory;
- every structure that crosses carries its own size (11.4);
- an environment value is obtained at run time, not fixed at build time (R3);
- every bound is in the contract (R4);
- the version floor is an operation, not an object-format artefact (11.6).

Each of these makes the static case *better*, not merely future-proof. That is
the test for whether a change belongs in this category: **if it only pays off
later, it is not in this list.**

**Cannot be decided now, and need not be:**

- how a context is started across a boundary (`kal_task_start`,
  `kal_space_start` take a function pointer, §3.3);
- how a two-word result crosses (§3.4);
- how absence is reported where there is no linker (§3.5, §9.3).

⭐ **The mechanism that lets these wait is a profile.** Rather than redesigning
`openkal.task` before the boundary is known, the specification names the set of
interfaces a **distributable** artifact may use, and that set initially excludes
the ones whose shapes are not boundary-safe. Static linking keeps all of them
and loses nothing; binary distribution starts with what already works and grows.
Clause 3.3 already names sets (`core`, `hosted`) and says a name "is a shorthand
and confers nothing" — a third name costs one row in that table and defers three
designs that cannot be done well yet.

**Must not be foreclosed:**

- a bare machine has no boundary to cross and no loader to negotiate with. Every
  change above must leave openkal-opensbi's static, no-negotiation form intact.
  A negotiation operation that a bare-metal implementation must *implement* is
  acceptable only if answering it is a constant.

### 11.9 What 11 leaves open

1. Whether the floor operation (11.6) is per-interface or one for the
   specification. Per-interface is more precise and is more to carry.
2. Whether `present` and `wanted` are one word each or one word per interface's
   own enumeration. This document assumes `openkal.fs` only.
3. The name-length bound of 11.3: an operation, or a distinct error value, or a
   requirement that implementations impose none. The third is the simplest for
   the user and the hardest for an implementation with a fixed buffer.
4. Whether the interposer of 11.7 lives in openkal's repository (where the
   conformance runner is) or its own.

---

## 12. The interface table, remade

Two instructions arrived together and they change the same table: the tier
column is wrong, and the table must state **now** which interfaces cross which
boundary. This section does both, and adds the finding that makes the second
tractable: **all fifteen can be made to cross every boundary, and only five
distinct causes stand in the way.**

### 12.1 ⚠️ `standard` is falsified by this ecosystem's own C library

Clause 3 defines the middle tier as:

> *Standard* denotes one an implementation hosting a C library provides.

openkal-opensbi provides `openkal.abort`, `openkal.stream`, `openkal.memory`,
`openkal.env` and `openkal.time`, and **not** `openkal.fs`, `openkal.process` or
`openkal.task` — deliberately, so that clause 6.1 reports the absence at the
link. And openkal-musl — **a C library** — is built above it, with those three
compiled out by `#ifndef`-guarded macros in its own manifest.

⇒ A C library hosts, today, above an implementation that provides **not one**
of the three `standard` interfaces. The tier states a fact that is not one.

The manifest that does this already knows: *"THE TARGET IS A PROXY FOR THE
IMPLEMENTATION, AND AN IMPERFECT ONE."* A package-level convention is already
correcting a specification-level tier, which is the shape of a tier that should
not exist.

### 12.2 Two tiers, and the resource column already does the rest

| tier | meaning |
| --- | --- |
| `core` | every implementation provides it. Closed at `abort`, `stream`, `memory` by clause 3.2, which stands |
| `optional` | everything else. Absence is reported by clause 6.1 |

There is **no enforcement difference today** between `standard` and `optional` —
clause 6.1 treats them identically — so the third tier carried only an advisory
claim, and §12.1 shows the claim is false.

⭐ The honest classification is already in the table, in the column next to it:
an interface exists where **its resource** exists. Storage, a second image, a
scheduler, a network, an address space that can be copied, entropy, an
interactive stream, a bound upon a wait. The tier column was a second, worse
statement of the resource column.

### 12.3 `hosted` goes; nothing about an environment is named

A set name that describes a *class of environment* will be falsified by an
environment nobody had in mind. `hosted` was falsified inside its own ecosystem
within one release (§12.1).

Clause 3.3's argument for naming sets was that stating a need one interface at a
time "does not scale". Re-examined: there are fifteen interfaces, and a consumer
that needs five names five. That is five lines, not a scaling problem. And the
ecosystem **already** states it per package and per target — openkal-musl's
`OKM_HAS_*` macros are exactly a consumer declaring its required set, in the
place where a convention among consumers belongs.

⇒ The specification names **no environment sets**. `core` stops being a "set"
and is what it already is: a requirement upon implementations.

⚠️ One name stays, and it is not about environments — the **boundary marking**
of §12.4. A boundary is a property of a declaration's shape, so a new
environment cannot falsify it.

### 12.4 The three boundaries, and why one column is not enough

"Runtime cross-platform" is ambiguous, and the ambiguity produces opposite
answers, so the table needs two columns rather than one.

| | boundary | what it means | what it forbids |
| **S** | static | the implementation is chosen when the artifact is built and linked into it | nothing |
| **L** | late-bound, one address space | the implementation is a separate object resolved at load; the artifact does not change when the implementation does | a structure whose size the consumer baked in; a report whose value the consumer baked in |
| **X** | crossed | the implementation is in another address space or another privilege level; a call is a trap or a message | additionally: a returned pointer into the implementation; a result wider than one word; a call back into the consumer |

Returning an internal pointer is **fine at L and fatal at X**. A two-word struct
return is **fine at L and fatal at X**. Conflating the two gives the wrong answer
for nine interfaces.

### 12.5 The marking, as it would appear in clause 3

`L?` and `X?` are the current state. `X after` is the state after the changes
this document proposes.

| Interface | Resource | Tier | L? | X? | what blocks X | X after |
| --- | --- | --- | --- | --- | --- | --- |
| `openkal.abort` | termination | core | ✓ | ✓ | — | ✓ |
| `openkal.stream` | a byte stream | core | ✓ | ✗ | 2 operations return a two-word result | ✓ |
| `openkal.memory` | a region of the address space | core | ✓ | ✓ | — | ✓ |
| `openkal.env` | the parameters a program receives | optional | ✓ | ✗ | **3 of 5 operations return a pointer into the implementation** | ✓ |
| `openkal.time` | a time source | optional | ✓ | ✗ | a property word is an exported object | ✓ |
| `openkal.random` | unpredictable bytes | optional | ✓ | ✗ | a property word is an exported object | ✓ |
| `openkal.fs` | a directory, an open file | optional | ✓ | ✗ | 2 operations return an internal pointer; a property word is an object; no structure carries its size | ✓ |
| `openkal.process` | a started program image | optional | ✓ | ✗ | a property word is an exported object | ✓ |
| `openkal.task` | an execution context | optional | ✓ | ✗ | a property word; **the entry is worded as a call** | ✓ (12.6) |
| `openkal.exec` | memory a program may execute | optional | ✓ | ✗ | a property word; **clause 6.5 decides availability at production time** | ✓ (12.7) |
| `openkal.terminal` | an interactive stream's mode | optional | ✓ | ✓ | — (its properties are already operations) | ✓ |
| `openkal.net` | a connection, a listener | optional | ✓ | ✗ | a property word is an exported object | ✓ |
| `openkal.datagram` | a message with a boundary | optional | ✓ | ✗ | a property word; 2 operations return a two-word result | ✓ |
| `openkal.space` | an address space, a context in one | optional | ✓ | ✗ | a property word; **the entry is worded as a call** | ✓ (12.6) |
| `openkal.timeout` | a bound upon a wait | optional | ✓ | ✗ | a granularity is an exported object; 3 operations return a two-word result | ✓ |

⭐ **Fifteen interfaces, thirteen currently blocked at X, and five causes.**

| # | cause | how many | where |
| 1 | an exported data object | **10** | nine `*_props`, plus `kal_timeout_granularity_ns` |
| 2 | an operation returns a pointer into the implementation | **5** | `kal_env_arg`, `kal_env_var`, `kal_env_var_at`, `kal_fs_preopen`, `kal_fs_list_next` |
| 3 | an operation returns a result wider than one word | **7** | `kal_stream_read`/`write`, `kal_datagram_send_to`/`recv_from`, `kal_timeout_read`/`write`/`recv_from` |
| 4 | an entry is worded as a call | **2** | `kal_task_start`, `kal_space_start` |
| 5 | availability decided at production time | **1 clause** | clause 6.5, `openkal.exec` |

Causes 1 and 2 are already on this document's change list (R2, §3.2). Cause 3 is
§3.4. Causes 4 and 5 were listed as *undecided* in §8 and §9.4; §12.6 and §12.7
resolve both, and neither requires a signature to change.

⚠️ **L is a different and quieter problem.** Every interface operates correctly
at L today. **None of them can evolve at L**, because no structure carries its
size and every property is a value the consumer's copy fixes at load. So the
honest marking for L today is not fifteen ticks — it is *"operates, cannot
evolve"*, and §11.4's `self_size` and R2 are what turn it into a tick.

### 12.6 `openkal.task` and `openkal.space` cross X, and the change is words

The C declaration is not the obstacle. `void (*entry)(void*)` is an address and
an argument, and an implementation on the far side of a boundary does not need
to *call* it — it needs to **begin a context at it**. A kernel does that by
setting a program counter and a stack pointer and returning to the caller's
privilege level, which is what `clone` is.

The obstacle is the specification's verb. And the intent is already the right
one in two places:

- `space.h`: *"The entry is not required to return, and if it does the context
  ends."*
- openkal-linux's own clone trampoline: *"The child of a clone begins on a stack
  of its own with no return address, so the transfer cannot be written in C."*

⇒ Both operations are respecified as: **the entry is an address at which a
context begins. No return address exists. Returning from it ends the context,
and the context's status says that it returned rather than choosing one.**

Nothing about the declaration changes; `openkal.task` and `openkal.space` become
X-capable; and clause 7.1's refusal to require a stack is untouched, because the
stack argument keeps the status it already has — honoured by an implementation
that allocates stacks and ignored by one that does not, with the caller unable to
observe which.

⚠️ One consequence to write down: an implementation's own trampoline may still
return (openkal-linux's does, into `clone`). The rule binds the **consumer's**
entry, which must not rely on returning to anything.

### 12.7 `openkal.exec` crosses X, and clause 6.5 becomes a bit

Clause 6.5 resolves availability at dependency resolution, on the ground that
*"a path no artifact takes is a path nothing has verified."* §9.4 showed that
under distribution the producer decides for every environment, so the consumer
cannot resolve it.

⇒ `kal_exec_props()` — an operation, by R2 — carries a position meaning
*executable memory is available to this artifact in this environment*.

This is not clause 6.2's forbidden shape. 6.2 objects to an operation that is
present and always fails **because the caller cannot tell**; a property the
caller reads first is exactly what tells it. It is the same argument that
legalises the link operations of §11.4, applied to a different partiality.

⚠️ And 6.5's own worry survives and is answered by §11.7: the interposer can
answer that bit both ways, which is the **only** way the unavailable path gets
exercised at all. Today it is a path nothing has verified because nothing can
produce it.

### 12.8 The rule this leaves behind

> ⭐ **An interface is X-capable when: it exports no object; no operation returns
> a pointer into the implementation; no result is wider than one machine word;
> every structure that crosses carries its own size; an entry is an address at
> which a context begins rather than a function that is called; and its absence
> is discoverable by an operation rather than only by a linker.**

Six clauses, mechanically checkable, and a conformance step can assert the first
four from the surface file and the declarations alone.

⇒ **New interfaces are designed X-capable from the start**, and the table marks
S/L/X for each. The one openkal already reserves — `openkal.event` — should be
designed against this rule before it is specified rather than after.

---

## 13. One landing

The instruction is that this is not a sequence of pull requests but **one
change, agreed first and landed together**. Two things follow.

### 13.1 ⚠️⚠️ One decision is not one publication

The graph forbids it:

```
openkal
  └─ openkal-linux · -macos · -windows · -opensbi · -uefi
       └─ openkal-musl
            └─ openkal-llvm-runtime
                 └─ consumers
```

Every edge is an **exact** version requirement — measured in this ecosystem
already: a manifest saying `openkal-musl = "0.5.0"` resolves to `0.5.0` and
nothing floats up. So the landing is *one decision, one reviewed change set,
and a topologically ordered publication* in which each step is green before the
next begins.

⚠️ The recorded failure mode is precisely here. A version was published and the
index's `latest` moved before the consumers had been repinned; every clean
environment went red while every development machine stayed green — **because
each repository's CI substitutes the working tree and therefore never resolves
the published form.**

⇒ Two requirements on the landing, neither of which is a matter of taste:

1. **Every repository's change is written and reviewed before any is published.**
   One branch per repository, all open simultaneously, cross-referenced.
2. ⭐ **A step that resolves the *published* packages must exist and must be the
   gate.** Not a job that substitutes working trees. Otherwise the landing is
   verified against a graph no user has.

### 13.2 What must be agreed before code is written

Fourteen decisions. Each is a yes/no; none is an implementation detail.

| # | decision | §ection |
| 1 | two tiers; `standard` and the `hosted` set name deleted | 12.1–12.3 |
| 2 | the interface table gains S / L / X marking, normative | 12.4, 12.5 |
| 3 | the six-clause X rule, and new interfaces designed against it | 12.8 |
| 4 | every report becomes an operation; ten data objects go | R2 |
| 5 | `kal_memory_granularity`, coarsest-safe, in `openkal.memory` | 11.2 |
| 6 | five operations gain copy-out forms | 12.5 cause 2 |
| 7 | seven operations gain a one-word result form | 12.5 cause 3 |
| 8 | `kal_task_start` / `kal_space_start` respecified as *begins at* | 12.6 |
| 9 | clause 6.5 becomes a property position on `openkal.exec` | 12.7 |
| 10 | `kal_fs_info` gains `self_size` / `wanted` / `present`; `kal_node_info` gains an opaque identity and a 64-bit size | 11.4, 11.5 |
| 11 | links enter `openkal.fs`; resolution stated in the header | 4.6 |
| 12 | the version floor is an **operation**, not an object-format note | 11.6 |
| 13 | small ABI corrections: delete `kal_fs_open_file`; typed handles at `kal_fs_stream` and `kal_spawn_streams`; `kal_endpoint.addr_len` to `kal_u32`; clause 7.2 gains the forged-handle sentence; a name-length bound | 4.3, 4.4, 9.8, 9.9, 11.3 |
| 14 | the interposer and the shared-library ABI test ship **in this change set** | 11.7 |

⚠️ **14 is the one most likely to be dropped and the one that must not be.** An
ABI break that lands without an artifact that can be run against two
implementations is a break verified only in the form it is replacing.

### 13.3 What is *not* in this landing

- **The C library defects of §1** — `rt_sigprocmask`, `do_fstatat`, `getpgid`,
  `rt_sigaction`, the started-program table, the `membarrier` case. None touches
  the interface, all are verified, and one is a crash a consumer is hitting
  today. ⚠️ **These should not wait for the landing**, and holding them back to
  make the landing "one thing" would keep a crash in a released package for the
  sake of tidiness.
- **The negotiation mechanism** (§9.3, clause 3.2 versus 6.2). Decision 2 marks
  which interfaces are X-capable; discovering *whether an implementation
  provides one* without a linker is a separate design, and the marking does not
  depend on it.
- **A trap encoding.** Decisions 6, 7 and 8 make the shapes admissible. Which
  boundary is actually built, and how a call is encoded on it, is not settled
  and is not blocked by this landing.

### 13.4 The change set, by repository

| repository | changes | ABI |
| --- | --- | --- |
| **openkal** | decisions 1–13; `SPEC.md` clauses 3, 3.2, 3.3, 5.3, 6.2, 6.5, 7.2, 8, 11; `SURFACE.txt`; every header | yes |
| **openkal-linux** | adopt; supply identity and links; claim properties per resource; state which operations resolve a link; `kal_memory_granularity`; build as a shared object for decision 14 | — |
| **openkal-macos** | adopt; identity from `st_dev`/`st_ino`; links; ⚠️ the row where the fork composition already diverges, so the row where decision 8 is tested | — |
| **openkal-windows** | adopt; identity from `FILE_ID_INFO`; links with the two divergences recorded; ⚠️ the row that reports a **64 KiB** granularity and so the row that proves decision 5 | — |
| **openkal-opensbi** | adopt the reports it must answer with constants; ⚠️ the row that must stay static and negotiation-free | — |
| **openkal-uefi** | adopt likewise | — |
| **openkal-musl** | the §1 defects **ahead of the landing**; then delete the two `4096`s; the `OKM_HAS_*` macros re-derived once the tiers change; the composed follow form; link operations | — |
| **openkal-llvm-runtime** | repin only, unless the interposer lives here | — |
| **interposer** (new) | decision 14: one shared object over openkal-linux answering a different granularity, a different `present`, one interface declined, an older floor | — |

### 13.5 The three risks that have precedent in this ecosystem

1. ⚠️ **The publication order.** §13.1. It has gone wrong before, in exactly this
   graph.
2. ⚠️ **The bare-metal row has no continuous integration hardware** and is the
   row most likely to break silently under a change that assumes a loader.
3. ⚠️ **`OKM_HAS_*` encodes the tier that is being deleted.** openkal-musl's
   per-target interface set was written against `standard` versus `optional`;
   after decision 1 it must be re-derived from what each implementation actually
   provides, and the target is — in its own words — an imperfect proxy for that.

### 13.6 The fourteen, with the evidence, the cost, and what "no" means

Ordered so that a decision never precedes one it depends on. **Three are genuine
judgements (7, 9, 12); the rest follow from evidence already in this document.**

---

**1 — Two tiers. `standard` and the `hosted` set name are deleted.**
*Evidence*: §12.1. openkal-opensbi provides none of the three `standard`
interfaces and openkal-musl, a C library, hosts above it.
*Cost*: clause 3, 3.3 rewritten; openkal-musl's `OKM_HAS_*` set re-derived from
what each implementation provides rather than from a tier.
*Against*: the tier had communication value — a newcomer read "standard" as
"you will usually have this".
*Answer*: keep that value as an **informative** sentence naming what
implementations of systems with storage usually provide, marked non-normative,
so it cannot be mistaken for a rule the way the tier was.
*If no*: the specification keeps a claim its own ecosystem contradicts.
⇒ **Recommend yes.**

---

**2 — The interface table gains S / L / X marking, normative.**
*Evidence*: §12.4. "Runtime cross-platform" gives opposite answers for L and X,
so one column would be wrong for nine interfaces.
⚠️ *The subsidiary decision that matters*: the marking is a statement about **the
shape of the declarations**, not a requirement upon implementations. An
implementation cannot violate it; only a declaration can. Without that sentence
the column reads as "implementations must support traps".
*Cost*: near zero — it is derivable from the surface file and can be asserted.
⇒ **Recommend yes, with the sentence.**

---

**3 — The six-clause X rule; new interfaces designed against it.**
*Evidence*: §12.8. Four of the six are mechanically checkable from the
declarations.
*Against*: the sixth clause — absence discoverable by an operation — has no
mechanism yet (§9.3 is undecided).
*Answer*: adopt the first five now, mark the sixth as pending, and apply the
rule to `openkal.event` **before** it is specified rather than after.
⇒ **Recommend yes, five of six effective now.**

---

**4 — Every report becomes an operation. Ten exported data objects go.**
*Evidence*: R2, §3.1, §12.5 cause 1. The interface already contains both
spellings: `kal_stream_props(s)` and `kal_terminal_props(s)` are operations,
`kal_fs_props` and eight others are objects, and `kal_time_monotonic_granularity()`
is an operation while `kal_timeout_granularity_ns` is an object.
*Cost*: the largest mechanical change — ten names across five implementations
and every read site.
*Buys*: evolution at L, crossing at X, and per-resource answers, which is the
fix for §1.5 (`CASE_SENSITIVE` claimed unconditionally, `LINKS` never claimed).
*Against*: a load becomes a call.
*Answer*: every one of these values is read once at startup or once per resource.
⚠️ *Subsidiary*: once `kal_fs_props` takes a `kal_dir`, a program with no
directory in hand cannot ask. That is correct — with no resource there is no
question — but it should be stated, not discovered.
⇒ **Recommend yes.**

---

**5 — `kal_memory_granularity()`, coarsest-safe, in `openkal.memory`.**
*Evidence*: §11.2, §9.6. Windows has two granularities (64 KiB allocation,
4 KiB page) and Linux one, so a design taken from Linux is wrong on Windows.
openkal-musl hardcodes 4096 in two places today and is wrong on any 16 KiB or
64 KiB machine.
*Cost*: one operation; a constant is a legal answer; `1` is a legal answer.
⚠️ *Subsidiary A*: one number or two? One, defined as the coarsest that is always
safe. The price is that `_SC_PAGESIZE` reads 64 KiB on Windows, so a program
sizing a buffer by it over-allocates sixteenfold, while a program aligning by it
is correct. Two numbers move the choice to every caller and most will choose
wrong.
⚠️ *Subsidiary B*: adding an operation to a **core** interface obliges every
implementation, including bare metal. This is not a violation of clause 3.2,
which closes the set of core **interfaces**; clause 8 admits new declarations
within one. Say so in the change, or it will read as a violation.
⇒ **Recommend yes.**

---

**6 — Five operations gain copy-out forms.**
`kal_env_arg`, `kal_env_var`, `kal_env_var_at`, `kal_fs_preopen`,
`kal_fs_list_next` return a pointer into the implementation.
*Evidence*: §3.2, §12.5 cause 2.
⚠️ *Subsidiary*: add a form, or replace? With no compatibility burden, replace —
two forms of one operation is precisely what decision 13 deletes elsewhere.
*Cost to the user*: `kal_fs_list_next` gains a buffer and a copy at the call
site. Real, and small.
⇒ **Recommend yes, replacing.**

---

**7 — ⚠️ JUDGEMENT. Seven operations gain a one-word result form.**
`kal_stream_read`/`write`, `kal_datagram_send_to`/`recv_from`,
`kal_timeout_read`/`write`/`recv_from` return `struct kal_io_result`, two words.
*Evidence*: §3.4, §12.5 cause 3.
⚠️ **This one is fine at L and only fails at X**, and it is the change that costs
the *user* most: `auto r = kal_stream_write(...); if (r.e)` becomes an
out-parameter at every call site.
*An alternative was considered and rejected on evidence*: merge the two words
into one signed word — negative is an error, non-negative is a count. **It loses
the count on failure**, which clause 7.4 and `stream.h` both require ("on
failure, n reports how many bytes were transferred before the failure") and which
openkal-musl actually reads — `okm_poll.c:81` returns `io.n ? io.n : -EAGAIN`.
*The real choice*:
- **now**: the call sites get uglier and the three interfaces are X-ready;
- **defer**: the call sites stay as they are and `openkal.stream`,
  `openkal.datagram` and `openkal.timeout` cannot be ticked in the X column —
  including `openkal.stream`, which is **core**.
⇒ **My recommendation is to do it now**, on the ground that a core interface
that cannot cross makes the marking of every other interface academic. But this
is the decision where the cost falls on the user rather than on the implementer,
and it is yours.

---

**8 — `kal_task_start` and `kal_space_start` respecified as "begins at".**
*Evidence*: §12.6. The declaration does not change. The intent is already
written twice — `space.h`'s "the entry is not required to return", and
openkal-linux's clone trampoline comment.
*Cost*: words.
*Checked*: on X, `kal_task_start` has no stack parameter and the implementation
allocates — which is what clause 7.1 designed for, so the crossed form is if
anything more natural than the linked one.
⇒ **Recommend yes.**

---

**9 — ⚠️ JUDGEMENT. Clause 6.5 becomes a property position on `openkal.exec`.**
*Evidence*: §12.7, §9.4. Under distribution the producer decides for every
environment, so a consumer cannot resolve availability at dependency resolution.
*Against, and it is a strong objection*: 6.5's own reason is *"a path no artifact
takes is a path nothing has verified"*. Making it a runtime property puts a
branch in every caller that almost no artifact takes.
*Answer*: the interposer of decision 14 can answer that bit both ways, which is
the only way the path is exercised at all — today it is unverified because
nothing can produce it.
⇒ **Recommend yes, but strictly conditional on 14.** If 14 is dropped, this
should be dropped with it, or 6.5's objection stands unanswered.

---

**10 — `kal_fs_info` gains `self_size` / `wanted` / `present`; `kal_node_info`
gains an opaque identity and a 64-bit size.**
*Evidence*: §11.4, §11.5, §1.4. Three requirements, three mechanisms, which
§4.1 and §4.2 wrongly conflated: a size the caller writes (version skew), a
`present` word the implementation writes (per-resource variability), a `wanted`
word the caller writes (do not compute what nobody asked for).
*Buys*: §1.4's silent `fs::equivalent(a,b) == true` for two different files; the
per-resource property of §1.5; evolution at L for the whole interface.
⚠️ *Subsidiary*: is `wanted` worth it? An implementation that always has
everything ignores it — one line — so the implementer's cost is zero, and it is
what lets an implementation skip computing an identity nobody asked for.
*Test to keep*: **if the cheap implementation is not one line, the shape is
wrong.**
⇒ **Recommend yes.** This is the largest semantic change and the one that pays
for the most.

---

**11 — Split in two. 11a: link resolution stated in `fs.h`. 11b: link
create/read enter `openkal.fs`.**
*Evidence*: §1.3, §4.6.
⭐ **11a is the one that matters and it does not depend on 11b.** Measured: with
resolution correct, six of seven `std::filesystem` answers agree with the host
and `fs::copy(recursive)` succeeds on a tree containing a link — with **no link
operation at all**. Clause 11 item 7 already states the rule; it is in neither
header, which is why two call sites in one file chose opposite directions.
11b depends on decision 10 for its legality (availability answered per resource).
⇒ **Recommend yes to both, 11a first and separable.**

---

**12 — ⚠️ JUDGEMENT. The version floor is an operation, not an object-format
note.**
*Evidence*: §11.6, §9.5. An ELF note is one object format's shape; openkal
targets Mach-O and PE and intends a trap.
⚠️ *The judgement*: this and the undecided negotiation mechanism (§9.3) are two
halves of one question — both are "ask something before using anything". Decided
separately they will produce two entry points.
⇒ **Recommend deciding the shape now and the mechanism later**: one operation,
performed before anything else, that both states the floor and reports which
interfaces are present. Whether it is per-interface or one for the specification
is the open part.

---

**13 — Five small corrections, packaged.**
- delete `kal_fs_open_file` — the sweep is run (§9.10); it is the only
  supersession in the whole surface, and `fs.h` already says the two-flag form
  cannot express what a C library needs;
- `kal_fs_stream` and `kal_spawn_streams` carry `struct kal_stream` rather than
  a raw word — these are the two places a handle crosses between interfaces and
  the two places its type is dropped, and the recorded "zero means both inherit
  and stdin" defect is a consequence;
- `kal_endpoint.addr_len` becomes `kal_u32` — it holds 4, 16 or 20 and today
  freezes a per-word-size difference for nothing;
- clause 7.2 gains one sentence: an implementation shall refuse a word that is
  not a handle it issued. Today 7.2 constrains **scope**, not **validity**;
  openkal-linux is safe by choice (index and generation), not by requirement;
- a name-length bound: openkal says nothing, and openkal-linux's `char buf[4096]`
  refuses a longer name as `kal_err_invalid` — the same reading as a malformed
  name.
⇒ **Recommend yes, as one package.** No two of them interact.

---

**14 — ⚠️ The interposer and the shared-library ABI test ship in this change set.**
*Evidence*: §11.7, and §11.0 — the earlier claim that this could not be built
was wrong, because mcpp supports `kind = "shared"`.
*What it is*: one shared object over openkal-linux that answers a different
granularity, fills a different `present`, declines one optional interface, and
reports an older floor. One probe binary, built once, run against both.
*Cost*: one file and one CI job.
*What it is the only way to test*: negotiation, an absent optional interface at
run time, a floor that is too low, a `present` bit that is clear, and decision
9's unavailable-executable-memory path.
⚠️ *It must be a separate artifact, not a feature of openkal-linux.* A feature is
chosen at dependency resolution and compiled in, which is the arrangement this
test exists to escape.
⇒ **Recommend yes, and treat it as non-negotiable.** An ABI break that lands
without an artifact runnable against two implementations is verified only in the
form it replaces.

---

**Dependencies**: 11b requires 10. 9 requires 14. 2's X column requires 6, 7 and
8 to be decided. Everything else is independent.

**If time forces a subset**: 1, 4, 8, 10, 11a, 13, 14 are the ones that improve
the static case on their own merits and are therefore not a bet on a boundary
that has not been chosen.

---

## 14. Decisions taken

Reviewed 2026-08-29. **All fourteen are accepted.** Four were amended in review,
and one of the amendments replaced this document's own recommendation with a
better answer. The amendments are recorded here rather than folded back into
§13.6, so that what changed and why remains readable.

### 14.1 The four amendments

**A. Decision 1 gains a second mechanism: `core`, and everything else optional
*or governed by a feature*.**

Two tiers, and "optional" is resolved in one of two places rather than one:

| how an optional interface is resolved | mechanism | example |
| --- | --- | --- |
| the implementation does not provide it | clause 6.1 — an undefined symbol at the link | `openkal.space` on Windows |
| the implementation provides it only to an artifact produced a certain way | a **feature** of the implementation's package, resolved at dependency resolution | `openkal.exec`; `openkal-linux`'s `standalone` |

This is not a new mechanism — clause 6.5 already describes the second, and
`openkal-linux` already ships a feature that decides which of two forms of
`openkal.task` a program gets. What the amendment does is **stop treating the
two as one thing**. The deleted `standard` tier was an attempt to describe both
with a single word, and it described neither.

⚠️ Consequence for decision 9: `openkal.exec` is then resolved **both** ways —
by a feature when the artifact is produced, and by a property position when the
artifact is distributed. That is not a contradiction; it is the same question
answered at the earliest time each form of distribution allows.

**B. Decision 2 gains a design obligation, not only a marking.**

> **Compile-time portability is the floor, not the goal. Every interface,
> operation and structure is designed so that it can also cross at run time,
> unless a reason is recorded for why it cannot.**

The S/L/X column therefore has two jobs: it states what is true, and it makes
the exceptions visible. An interface marked "X: no" is carrying a debt, and the
column is where the debt is written down. §12.5's "X after" column is the target
state and every row in it is a tick.

**C. Decision 4 is not only about boundaries. It is about semantic consistency.**

The ten data objects are one instance of a larger defect: **one idea with two
spellings**. The surface has five, and four of the fourteen decisions each remove
one without the connection having been stated:

| one idea | two spellings | removed by |
| --- | --- | --- |
| an implementation reports something about itself | `kal_stream_props(s)`, `kal_terminal_props(s)` are operations; nine `*_props` are objects | 4 |
| a magnitude an implementation reports | `kal_time_monotonic_granularity()` is an operation; `kal_timeout_granularity_ns` is an object | 4, 5 |
| opening a file | `kal_fs_open(flags)` and `kal_fs_open_file(write, create)` | 13 |
| a handle crossing between interfaces | `struct kal_stream` everywhere; a raw word at `kal_fs_stream` and in `kal_spawn_streams` | 13 |
| the result of a transfer | `struct kal_io_result` in three interfaces; `int` and an out-parameter everywhere else | 7 |

⇒ The rule to state once, so that a sixth is not introduced:

> ⭐ **One idea has one spelling. A report is an operation; a bit-set is
> `kal_<interface>_props`; a magnitude has a name of its own; a handle carries
> its type; a transfer reports one signed word; a name is passed as a pointer
> and a length and returned by copying into the caller's buffer.**

⚠️ And the amendment surfaces a sixth that decision 10 would otherwise
*introduce*: after 10, `kal_node_info` carries its own size and no other
structure does. The rule that keeps this consistent without taxing every
structure:

> **A structure that may gain fields carries its size. A structure that is
> complete by construction states why it will not grow.** `kal_endpoint` already
> states its own — the set of address lengths may grow while the structure stays
> fixed, because the length is a value rather than a layout.

**D. ⭐⭐ Decision 7 is answered by a form that is simpler for both sides, and
this document's own recommendation was the worse one.**

The question put in review was whether the goal could be reached while making
both the call and the implementation *simpler*, and if not, to prefer openkal's
design and let the caller compose.

It can, and the evidence is that **the only consumer of the two-word result is
already doing the conversion by hand, at every site**:

```
okm_poll.c:81   return io.n ? (long)io.n : -EAGAIN;
okm_poll.c:82   return io.n ? (long)io.n : -okm_errno(io.e);
okm_net.c:581   return io.n ? (long)io.n : -okm_errno(io.e);
okm_poll.c:72   return (long)io.n;
okm_net.c:571   return (long)io.n;
```

Every one collapses `{n, e}` into a single signed word by the same rule. ⇒

> **A transfer returns one signed machine word: the number of bytes transferred,
> or the negated error value when none were transferred.**

- `kal_stream_read`: a non-negative count; **zero denotes end of input**, which
  is the convention clause 7.4 already states;
- `kal_stream_write`: the count, which is the whole buffer in the ordinary case
  by clause 7.4;
- the bounded forms in `openkal.timeout` and `openkal.datagram`: the count moved
  within the bound, which is exactly what `okm_poll.c:81` computes today.

**`struct kal_io_result` is deleted.** One frozen layout fewer (clause 5.3), one
fewer spelling (amendment C), and:

| | today | after |
| the caller writes | `auto r = f(...); if (r.e) …; use r.n;` | `long n = f(...); if (n < 0) …; use n;` |
| the implementer returns | a two-field structure | one value |
| crossing at X | impossible | one word |

⚠️ **Three things this rests on, each checked rather than assumed:**

1. *The count is bounded by the caller's buffer*, which cannot exceed half the
   address space, so a signed machine word is always sufficient.
2. *The error set is closed* (clause 5.2, values 1–13), so a negated error can
   never be mistaken for a count. ⭐ The closed set is what makes the collapse
   safe; an open-ended error space would not permit it.
3. *Nothing loses information a caller uses.* The case "transferred some bytes
   and then failed" reports the bytes, and the condition arrives on the next
   call — which is what all five sites above already do, deliberately.

⚠️ The earlier rejection of this shape in §13.6 was wrong. It reasoned from the
header's sentence — "on failure, n reports how many bytes were transferred" —
without reading what any caller does with it. **The declaration was consulted and
the call sites were not**, which is the same error §2 names.

### 14.2 The fourteen, as decided

| # | decision | as amended |
| --- | --- | --- |
| 1 | two tiers | ✓ + optional is resolved by the link **or by a feature** (14.1 A) |
| 2 | S / L / X marking | ✓ + **X is the design target, S is the floor** (14.1 B) |
| 3 | the six-clause X rule | ✓ five effective now; the sixth pending §9.3 |
| 4 | every report becomes an operation | ✓ + **one idea, one spelling** (14.1 C) |
| 5 | `kal_memory_granularity()` | ✓ coarsest-safe; in `openkal.memory`; clause 8 admits it |
| 6 | five copy-out forms, replacing | ✓ |
| 7 | transfers cross in one word | ✓ **by deleting `kal_io_result`, not by an out-parameter** (14.1 D) |
| 8 | `begins at`, not `calls` | ✓ wording only |
| 9 | clause 6.5 becomes a property position | ✓ conditional on 14; and see 14.1 A |
| 10 | `self_size` / `wanted` / `present`; identity; 64-bit size | ✓ + the size rule of 14.1 C |
| 11 | 11a resolution in the header; 11b link operations | ✓ 11a first and separable |
| 12 | the floor is an operation | ✓ shape now, mechanism with §9.3 |
| 13 | five small corrections | ✓ as one package |
| 14 | the interposer ships in this change set | ✓ non-negotiable |

### 14.3 What the decisions change in §12.5's table

Decision 7's new form removes cause 3 from three rows without an out-parameter,
so the table's "X after" column is unchanged and the route to it is shorter:

| cause | count | status after 14.1 |
| an exported data object | 10 | decision 4 |
| an operation returns an internal pointer | 5 | decision 6 |
| a result wider than one word | 7 | **decision 7, by deletion** |
| an entry worded as a call | 2 | decision 8, wording |
| availability decided at production time | 1 clause | decisions 9 and 1 together |

### 14.4 What is still open after this review

Unchanged from §13.3, and now the whole of it:

1. **The negotiation mechanism** — clause 3.2 forbids a new core interface and
   clause 6.2 forbids an operation that always fails; a trap ABI needs one of
   them to give. Decision 12 fixes the *shape* and leaves the mechanism.
2. **Which boundary is built**, and how a call is encoded on it. Decisions 6, 7
   and 8 make the shapes admissible; none of them depends on the answer.
3. **Whether the floor operation is per-interface or one for the specification.**
4. **Where the interposer lives** — openkal's repository, where the conformance
   runner is, or its own.
