---
subject: process-unit-lifetime
status: draft
---

# Compiler unit lifetime: API inventory and minimum recovery design

This is a design review draft. It does not qualify the current candidate for
noncooperative supervisor loss. No incomplete output-only recovery patch has
been applied.

## Evidence and ownership

The pinned OS 0.2.3 uses OpenKal 0.17.1, Linux 0.18.3, macOS 0.15.1 and
Windows 0.13.3. Its process API separates an owned process unit from a bound
program lifetime. A process handle can end its unit while that handle is owned.
It exposes neither a persistent unit reference nor post-controller observation.

The Linux backend binds the directly started program through PDEATHSIG. It does
not bind every descendant. A controlled two-level compiler in the same unit
remains alive and writes its object after only the supervisor is killed.
Separately, killing a supervisor and its compiler after BMI publication, then
killing the top-level owner, admits the next generation but leaves it waiting
for a nonexistent old status rather than recompiling the interface.

Published releases were independently checked: OS 0.2.9, OpenKal 0.23.0,
Linux 0.24.2, macOS 0.21.3 and Windows 0.19.0. OS 0.2.9 still exposes the same
process/unit lifetime boundary. OpenKal 0.23 includes job_usage, introduced in
0.20, but it requires a held job handle. It does not provide a recoverable unit
reference. A Linux numeric process group and a Windows HANDLE are not portable,
persistent capabilities. Simply upgrading the existing dependency is insufficient.

Core owns build transactions, permits and output recovery. OS/OpenKal own unit
creation, observation and lifetime semantics. Plugins own neither of these
kernel responsibilities. Core must not inspect native PIDs/PGIDs, recreate a
Windows HANDLE from its numeric value, or interpret leader death as unit death.

## Minimum generic OS capability

The needed capability is an observable execution unit with an opaque reference.
The names below describe requirements rather than an approved public spelling.

1. A helper enters a private execution unit before starting any external
   compiler. It receives a reference that another invocation can observe.
2. The reference can report `occupied`, positively proven `empty`, or `unknown`.
   Unsupported queries, stale/unreadable data and permission failures are not
   `empty`. Observation of an expired reference never grants termination
   authority over a different unit that reuses a native identifier.
3. Live owners retain the existing unit termination capability. Recovered
   references may be observation-only. A remaining orphan unit keeps its
   logical permit reserved; a bounded refusal is safer than guessing it died.
4. Actual compiler processes inherit that explicitly created private unit. This
   is a deliberate composition of a declared unit and its child, not an
   unqualified switch to sharing Ninja's unit.
5. Unit observation counts remaining writers after the leader exits. A
   controller's death must not manufacture an empty result. A surviving
   observer may retain the actual kernel reference; if no observer can prove
   that the original unit has ended, recovery remains `unknown`. Closing an
   observer does not itself claim that the unit has stopped.

Creating the private unit inside the existing compiler helper avoids an unknown
spawn-identity window: its reference and output record are committed before the
helper may launch the real compiler. If that publication fails or the helper
ends first, no compiler is permitted to have started. Phase one already launches
its supervisor in a separate unit; ordinary compiler wrappers need the same
explicit bootstrap. The generic launcher needs a way to run a child in a
provided current unit rather than automatically create another unobserved unit.

Candidate backend implementations require experiments before approval:

- POSIX: use the unit's own observer contract. An occupancy check must prove
  emptiness without sending a terminating signal or treating an absent leader
  as an empty group. Identifier reuse and incomplete process enumeration must
  yield conservative results. Native details remain in the backend.
- Windows: a job name alone is insufficient. The native experiment observes
  `ERROR_FILE_NOT_FOUND` after the controller closes its last handle while a
  writer is still alive. Without a lifetime limit, that writer subsequently
  commits output. With `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, a live writer is
  still observable after the name disappears, so asynchronous termination is
  not an emptiness proof either. The pinned backend deliberately leaves this
  flag unset for `own_unit`; `bound_lifetime` instead creates a separate nested
  job with the flag. The experiment compares these arrangements with a compiler
  driver and descendant writer. A retained native handle can query the original
  job; a recovered name that cannot be opened remains `unknown`. Never
  serialize a HANDLE for another process to use, and include loss of every
  proposed handle holder in the fault matrix.

The existing job_usage operation can supply live-owner statistics. It cannot
on its own provide the missing persistent observation guarantee.

## Core transaction and permit protocol

The generation and actor barrier remain runtime state outside build identity.
Each attempt records absolute output identities before its first state change:
BMI, object, slot status/log/marker, dependency output and the verified previous
BMI backup. Resolving these paths occurs in the original execution context;
recovery does not resolve them against its own cwd or a newly changed target
symlink/home.

Phase one publishes its prepared transaction before clearing an old status or
copying a backup. A backup becomes recoverable only after its copy and identity
are complete. The supervisor publishes its observable unit reference before any
external compiler starts. Stage records have separate writers, avoiding a
phase-one update overwriting the supervisor's unit reference.

A compiler slot has both its kernel lock and a durable reservation pointing at
this attempt. A free kernel lock is insufficient when its previous reservation
still names a potentially occupied unit. A new holder observes that unit first;
it never overwrites an occupied or unknown reservation. This applies within one
running generation as well as between generations. It therefore preserves the
actual compiler count even when a supervisor is forcibly killed.

Normal completion clears the reservation only after the external compiler unit
has no remaining writers. The helper's final metadata writes remain covered by
its actor lease. A cooperative cancellation records intent and ends the entire
private unit. If the helper dies before publishing status, its durable record
still protects the outputs and capacity.

A phase-one/join waiter notices an abandoned controller through the attempt
protocol. It reports failure promptly; it does not silently wait two hours for
an rc that no remaining controller can publish. It does not delete an output
while the old unit is occupied or unknown.

A new generation waits for both all old actor leases and all referenced compiler
units. Only then may it recover outputs. A committed successful status with
complete verified outputs is retained. An incomplete published BMI/object/status
is invalidated so Ninja reruns phase one, rather than replaying an orphan join.
The previous verified BMI remains a protected baseline that the next attempt can
restore on failure; it must not be accidentally deleted by the ordinary backup
initialization. Runtime transaction identifiers are not Ninja command hashes.

Recovery is idempotent: interruption after each file operation can be retried
from the durable stage record. Unknown schema, corrupted records, record-write
failure and ambiguous unit state refuse admission and retain evidence. They do
not remove a live reservation or fall back to an unbounded compiler.

## Required fault matrix before one coherent implementation push

| Boundary | Fault | Required observation |
|---|---|---|
| Before first phase-one modification | kill phase one/owner | no unregistered state write; retry remains valid |
| After status reset or during backup copy | kill phase one/owner | incomplete backup is not certified; previous complete BMI preserved |
| Private unit created, reference not committed | kill helper | real compiler never started |
| Reference committed, compiler not started | kill helper | safe unit observation and idempotent empty-attempt recovery |
| Compiler running, BMI absent | kill supervisor alone | descendants counted; slot remains reserved; no premature output deletion |
| BMI published, compiler running | kill supervisor/owner | no orphan await; old writers quiet before invalidation/new compile |
| Compiler leader exits, descendant remains | normal/forced exit | unit remains occupied despite leader death |
| Before rc atomic rename | kill supervisor | missing rc recovered only from the unit/transaction proof |
| After rc atomic rename | kill supervisor/owner | committed successful outputs retained; failed outputs invalidated safely |
| Phase-one settle/restore after compiler exit | delay/kill writer | actor barrier covers the final write, not only compiler capacity |
| Recovery between any two file operations | kill new owner | next recovery is safe and idempotent |
| Record corruption/write refusal | inject failure | no compiler starts without its record; no guessed deletion |
| Cwd/home/symlink changes | restart | original absolute output identities recovered exactly |
| Normal completion and cached normal/quiet replay | no fault | unchanged objects/BMIs/graph; no runtime identity in cache keys |

The matrix must include an actual driver with a descendant, not only a direct
sleeping child. It must exercise caps one and two, late failures and cancellation
without awaiting a new full toolchain build for every local checkpoint.

## Dependency sequence

First approve and test the generic OS/backend observation contract. Reuse the
published job-usage capability where sufficient, but add the missing persistent
reference semantics through OpenKal and the native backends rather than native
helpers in core. Upgrade the OS package to expose it, publish and index that
coherent dependency group, then pin core and run the full matrix and native CI.
Versions follow each repository's release rules; published tags stay unchanged.

If a new process API changes the OpenKal source/ABI floor, the matching runtime
and platform rows used by core's openkal-target CI must be included in that
version group. A source-only host success does not qualify those image rows.
Plugin sidecar work remains prepared independently and receives the final public
core pin only after this release is actually available.
