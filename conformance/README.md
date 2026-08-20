# openkal-conformance

The behavioural half of clause 9, as a program an implementation runs against
itself.

```bash
# from an implementation's working tree
mcpp build --features full
./target/*/*/bin/openkal-conformance
```

The implementation under examination is not named in this package's manifest.
It is supplied by whoever runs the suite, exactly as any consumer supplies one —
and `tools/run-conformance.sh` in the parent directory is where that is done, so
that a change to how it is done is one change:

```bash
git clone https://github.com/mcpplibs/openkal .spec
bash .spec/tools/run-conformance.sh openkal-linux . full
#                                   ^ the package    ^ where it is
```

This package is not published to the index separately. It is a program rather
than a library — a dependency that contributed a binary target to its dependant
would be a surprising thing for `[dependencies]` to name — and it is reached by
checking out the specification, which is the package it is the conformance
procedure for.

## It is composable, because openkal is

An interface is the unit of provision. An implementation provides one in whole
or not at all, and one it does not provide is absent as a link-time definition —
so a suite that examined all eight unconditionally would fail to link against a
conforming implementation of five, and would report nothing at all rather than
reporting five.

Each interface is therefore a feature.

| | |
| --- | --- |
| `mcpp build` | the core set: `abort`, `stream`, `memory` |
| `mcpp build --features standard` | every interface version 0.5 defines |
| `mcpp build --features fs,task` | the core set, and these two |
| `mcpp build --features full` | every interface and every kind of examination |

The core set is the default because clause 3 says every implementation provides
it, and there is no arrangement that examines nothing: a run that observed
nothing exits 2 rather than 0.

`process` implies `fs`, because a program is started relative to a directory.
`cost` implies `time`, because a cost is a duration.

## Four kinds of examination

| kind | what it asks | selected by |
| --- | --- | --- |
| behaviour | does the operation do what the specification requires | always |
| abi | are the shapes clause 5.3 freezes the shapes the linked implementation has, and does each capability word contain only positions the specification has assigned | `--features abi` |
| stability | does the operation still work after twenty thousand of it | `--features stability` |
| cost | how long does one take | `--features cost` |

A cost is reported and never asserted. A number that varies with the machine
cannot be a verdict, and a suite that made it one would fail on a loaded runner
and be discarded rather than consulted.

## Three counts, and the third is the one to read

```
observations: 97 held, 0 did not hold, 0 not observed
```

A suite that reported only what held and what did not cannot distinguish an
interface that behaved from an interface it never examined, and a run in which
nothing was examined then reports success. Every unmade observation therefore
carries the reason it was not made:

```
  not observed [behaviour] a wait honours a timeout --- the implementation
                           does not claim prop_wait_timeout
```

## What the observations are written to catch

The suite is written on the assumption that an implementation which is nearly
right passes a suite which is nearly written. Several observations exist because
of a specific way of being nearly right.

**A return value is not an effect.** The three conditions `kal_fs_open` exists
to express are observed by reading the file afterwards. A truncation that did
not happen leaves a longer file, an exclusion that did not happen succeeds, and
an append that did not happen overwrites — and in all three cases every call
returns `kal_ok`.

**A control that can distinguish.** Clause 7.6 requires the argument vector to be
passed unaltered. The suite starts a copy of itself with the first element the
copy expects, and the copy reports through its status that it saw it. It then
starts a second copy with a *different* first element and requires the copy to
report *that*. Without the second, an implementation that ignored the vector
entirely and passed a fixed name would satisfy the first.

**An identity is compared with its peers, not with the observer.** The suite
requires the identities of four contexts that ran at the same time to be distinct
*from each other*. Comparing each with the starting context's identity is
satisfied by an implementation that answers one wrong value for all of them — and
one did: it answered zero for every context it started, which differs from the
starting context's identity and is useless to a consumer, because zero is what a
table keyed on the identity reads as "no entry".

**A claim is checked.** An implementation that claims `prop_wait_timeout` is
required to have a wait that times out; one that claims `prop_thread_local` is
required to give a started context its own instance of a `thread_local`
variable; one that claims `prop_exit_status` is required to report the value the
started program returned. A claim nobody checks is a comment.

**A count, not an outcome.** Four contexts each increment a counter twenty
thousand times under a mutex the suite builds from the suspension primitive. The
observation is that the sum is eighty thousand. A mutex that is unsound loses
increments and produces no other symptom.

**Repetition.** An implementation that packs a generation into a handle and
never reclaims the index satisfies every observation that opens one file. It
stops at twenty thousand.

## What it depends upon

openkal, and the language.

The report is written through `openkal.stream`, the durations come from
`openkal.time`, and there is no `import std`. That is not austerity: the suite
must run against an implementation in either arrangement, including one in a
program that carries no other runtime, where a standard library would not link —
and where it did link, the suite would be reporting on an implementation while
resting on facilities that implementation may be the only supplier of.

The price is the formatting in `okc.report`, and it is sixty lines.

## Layout

One module per concern, interface and implementation separated, as the mcpp
benchmark harness is arranged.

| | |
| --- | --- |
| `src/spec.cppm` | what is examined, expressed as data; the one place that reads the selection macros |
| `src/report.cppm` `.cpp` | the three states, the counts, and the formatting |
| `src/suite.cppm` `.cpp` | the driver: what runs and in what order |
| `src/sections/*.cppm` `.cpp` | one section per interface |
| `src/sections/child.cppm` `.cpp` | the copy the suite starts, for the two requirements that end the program that satisfies them |
| `src/main.cpp` | the entry |

Adding an interface to the specification is a section and a row in
`src/spec.cppm`. It is not an edit to the driver or to the report.
