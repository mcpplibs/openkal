# openkal Specification, version 0.11

## 1. Scope

openkal defines an interface between a program and the environment that
executes it. The interface is stated as a C application binary interface. It is
distributed as a set of C headers that declare it and a set of C++ modules that
export those declarations, so that a consumer written in either language
reaches the same entities. Clause 4.2 records why the second way of reaching
them is not a convenience.

The specification has two audiences. An *implementation* supplies the functions
declared here and is judged by whether it can do so without constructing a
compatibility layer. A *consumer* — an application, a C library, or a C++
standard library port — depends on those declarations and is judged by whether
it can do so without knowing which implementation is present.

openkal does not describe an operating system, and it does not describe a
program. It describes the boundary between them, and it is written so that the
boundary can be crossed in either direction: an implementation may be built upon
a C library, beneath one, or without one.

## 2. Conformance vocabulary

The words *shall*, *shall not* and *may* carry their usual normative force. A
conforming implementation satisfies every *shall* that applies to the interfaces
it provides. An implementation is not required to provide every interface.

## 3. Interface inventory

An interface is the unit of versioning and of provision. An implementation
provides an interface in whole or not at all.

| Interface | Resource | Tier | S | L | X |
| --- | --- | --- | --- | --- | --- |
| `openkal.abort` | termination | core | ✓ | ✓ | ✓ |
| `openkal.stream` | a byte stream | core | ✓ | ✓ | ✓ |
| `openkal.memory` | a region of the address space | core | ✓ | ✓ | ✓ |
| `openkal.env` | the parameters a program receives at inception | optional | ✓ | ✓ | ✓ |
| `openkal.time` | a time source | optional | ✓ | ✓ | ✓ |
| `openkal.random` | a source of unpredictable bytes | optional | ✓ | ✓ | ✓ |
| `openkal.fs` | a directory, and an open file | optional | ✓ | ✓ | ✓ |
| `openkal.process` | a program image that has been started | optional | ✓ | ✓ | ✓ |
| `openkal.task` | an execution context, and a suspension primitive | optional | ✓ | ✓ | ✓ |
| `openkal.exec` | a region of the address space a program may execute | optional | ✓ | ✓ | ✓ |
| `openkal.terminal` | an interactive stream's treatment of what is typed | optional | ✓ | ✓ | ✓ |
| `openkal.net` | a connection, and a listener for connections | optional | ✓ | ✓ | ✓ |
| `openkal.datagram` | a message with a boundary, sent without a connection | optional | ✓ | ✓ | ✓ |
| `openkal.space` | an address space, and a context executing in one | optional | ✓ | ✓ | ✓ |
| `openkal.timeout` | a bound upon operations that would otherwise wait | optional | ✓ | ✓ | ✓ |
| `openkal.event` | readiness of a set of resources | reserved | | | |

Version 0.11 specifies the core and optional interfaces. The reserved row is not
specified, and its name shall not be used for other purposes.

The S, L and X columns state which boundaries an interface's declarations can
cross. They are defined in clause 4.4 and are a statement about the shape of the
declarations, not a requirement upon implementations: an implementation cannot
violate them, and only a declaration can.

The five interfaces added in version 0.8 are optional in the sense clause 3
defines, and their optionality is not a concession. An environment with no
network is not deficient for providing neither `openkal.net` nor
`openkal.datagram`; an environment with no memory management unit cannot
provide `openkal.space` and is not deficient either. Clause 6.1 expresses each
absence as the absence of a definition at the link, so a program that requires
one of them is refused when it is built rather than when it runs.

*Core* denotes an interface every implementation provides. *Optional* denotes
every other, and how its absence reaches a consumer is one of two things:

| how an optional interface is resolved | mechanism |
| --- | --- |
| the implementation does not provide it at all | clause 6.1 — the definitions are absent, and a consumer that uses one fails to link |
| the implementation provides it only to an artifact produced in a particular way | a feature of the implementation's package, resolved at dependency resolution — clause 6.5 |

⚠️ There were three tiers, and the middle one said that an implementation
*hosting a C library* provides `openkal.env`, `openkal.time`, `openkal.fs`,
`openkal.process` and `openkal.task`. It was false, and falsified within this
specification's own ecosystem: an implementation for a machine with firmware and
no operating system provides none of the last three, and a C library is hosted
above it. The tier stated a fact that was not one, and it stated it about the
consumer rather than about the interface.

The honest classification is the *Resource* column beside it. An interface
exists where its resource exists — storage, a second image, a scheduler, a
network, an address space that can be copied, entropy, an interactive stream, a
bound upon a wait. Clause 6.1 makes the absence a fact a consumer learns; no
tier is needed to predict it, and the prediction was wrong.

*Informative, and carrying no requirement*: an implementation of a system with
storage and a scheduler ordinarily provides `openkal.env`, `openkal.time`,
`openkal.fs`, `openkal.process`, `openkal.task` and `openkal.timeout`. This
sentence describes what is common; nothing follows from it, and an
implementation is not measured against it.

### 3.1 The basis of the core set

An interface belongs to the core set when every plausible implementation can
supply it without simulating a facility the environment lacks. The distinction
between supplying and simulating is drawn as follows.

> An implementation that would leave its callers silently wrong is a simulation.
> An implementation that merely bounds what is available is a supply.

A bump allocator over a fixed region can be exhausted, and exhaustion is a
defined outcome on every platform; such an allocator therefore supplies
`openkal.memory`. A counter that does not advance in real time would make every
timed wait incorrect without reporting it; such a counter would simulate
`openkal.time`, and `openkal.time` is accordingly not in the core set.

It follows that the presence of a heap is not a property of the hardware. Any
environment with writable memory can supply one, and the specification places
`openkal.memory` in the core set on that basis.

### 3.2 The core set does not grow

An interface admitted to the core set is required of every implementation
thereafter, including implementations of environments no one has yet written
one for. The set is therefore closed at `openkal.abort`, `openkal.stream` and
`openkal.memory`, and a later version shall place a new interface outside it
even where every implementation that exists at the time would satisfy it.

The asymmetry is the reason. An interface wrongly placed in the core set
excludes environments — a bare machine, a supervisor, firmware — from
conformance altogether, and that exclusion is discovered by whoever tries to
write the implementation rather than by the specification. An interface wrongly
placed outside it costs a consumer one declared dependency. The first error is
found late and by the wrong party; the second is found at dependency resolution
and by the party that can act on it.

⚠️ **What this closes is the set of INTERFACES.** `kal_version` and
`kal_interfaces` are exported by every conforming implementation and belong to
no interface: they provide no resource, and an implementation answers both with
constants. They are the specification's own self-description, required by clause
9, and requiring them adds nothing to the core set that a bare machine would
have to supply. A consumer that is linked never needs to call them; a consumer
bound at load or across a boundary has no linker to ask, and asking an interface
by calling into it is the act that must not happen first.

### 3.3 Naming a set of interfaces

An interface is provided or not provided, and clause 6.1 makes that a fact a
consumer learns from the linker. A consumer must nevertheless be able to *state*
what it needs before it is built, and stating it one interface at a time does
not scale: the sets a program can be written against are not arbitrary subsets,
and an ecosystem that could only enumerate them could not say "this package
needs an environment of such-and-such a kind".

⚠️ **The specification names no such set, and the one it did name has been
withdrawn.** Version 0.8 named `hosted` — the core interfaces together with
`openkal.env`, `openkal.time`, `openkal.fs`, `openkal.process` and
`openkal.task`. The name described a class of environment, and a name that
describes a class of environment is falsified by an environment nobody had in
mind. This one was falsified inside its own ecosystem within a release: a C
library is hosted above an implementation that provides none of the last three.

The scaling argument is also weaker than it read. There are fifteen interfaces,
and a consumer that needs five names five. That is five lines, not a problem of
scale, and it is what the ecosystem already does — a consumer states its
required set per target, in its own package, which is where a convention among
consumers belongs.

`core` remains, and is not a set a consumer states: it is the requirement upon
implementations stated in clause 3.2. Nothing in this clause alters clause 6.1:
an interface a consumer uses and an implementation does not provide is an
undefined symbol.

### 3.4 Interfaces the specification declines to define

Naming is not factored into a separate interface. An earlier draft replaced
`openkal.fs` and `openkal.net` with a single interface mapping names to
resources, on the grounds that a file, a network connection and a serial port
all yield a byte stream. That decomposition was withdrawn for three reasons.

1. It required every implementation to parse an unbounded set of name schemes,
   including implementations that support none of them. That requirement is a
   compatibility layer, and clause 7.1 excludes it.
2. It merged the operations of unrelated resources. Positioning applies to a
   file and not to a connection; half-closure applies to a connection and not to
   a file. The merged interface would have contained operations that some of its
   resources can never satisfy, which clause 6.4 identifies as a defect.
3. The precedent cited in its support was misread. The WebAssembly System
   Interface, second preview, separates resource kinds into distinct interfaces
   and shares only the stream type between them.

The stream is therefore the shared currency of the specification and not its
common entrance.

## 4. Organisation of the declarations

The specification package provides one header per interface, which is where the
declarations are, and one module per interface, which exports them. An
implementation provides neither.

| Interface | Module | Header |
| --- | --- | --- |
| shared definitions | `openkal.types` | `openkal/types.h` |
| `openkal.abort` | `openkal.abort` | `openkal/abort.h` |
| `openkal.stream` | `openkal.stream` | `openkal/stream.h` |
| `openkal.memory` | `openkal.memory` | `openkal/memory.h` |
| `openkal.env` | `openkal.env` | `openkal/env.h` |
| `openkal.time` | `openkal.time` | `openkal/time.h` |
| `openkal.fs` | `openkal.fs` | `openkal/fs.h` |
| `openkal.process` | `openkal.process` | `openkal/process.h` |
| `openkal.task` | `openkal.task` | `openkal/task.h` |
| `openkal.random` | `openkal.random` | `openkal/random.h` |
| `openkal.exec` | `openkal.exec` | `openkal/exec.h` |

`openkal.h` includes every header, for a consumer that uses several.

A consumer imports the interface. An implementation imports the same interface,
because it needs the declarations it is defining, and it exports nothing.

    consumer ──imports──► openkal ◄──imports── implementation
         │                                          │
         └──────────── links against ───────────────┘

### 4.1 What an implementation contributes

Definitions of the functions the interface declares, and nothing else. An
implementation that declares a module has misunderstood its role: the interface
is the specification's, and an implementation that exported one would place a
name the consumer relies upon under the control of a party the specification
does not govern.

The consequence is that an implementation cannot extend the interface. This is
not a restriction that must be enforced; it follows from the arrangement. An
implementation may of course publish additional facilities, and it does so in
its own modules, which a consumer that uses them must import by name.

### 4.2 One statement of the declarations, two ways to reach it

The contract is a C application binary interface. The specification package
states it once, as a C header per interface, and provides a C++ module per
interface that includes that header in its global module fragment and exports
the names it declares.

The second way of reaching the declarations is not a convenience. A C++
consumer imports the module for the interface it uses; a consumer written in C
cannot, because a C translation unit has no import. The canonical consumer
written in C is a C library being ported onto openkal, which is the case clause
1 names first, so a specification reachable only by import would be unusable by
the consumer it exists for. Version 0.4 was reachable only by import, and the
omission was found by attempting that port.

The arrangement is deliberately not two statements that agree. A module that
re-declared what the header declares would be a second declaration of the same
contract, and two declarations drift; here there is one declaration, and the
module exports it. A consumer that imports and a consumer that includes
therefore obtain the same entities, and no procedure is required to keep them
equal because they are not two things.

The headers include no header of their own, name no library, and obtain the
width of a machine word from the compiler. The second way of reaching the
declarations therefore adds nothing to what the specification package requires,
which remains nothing.

#### What the module adds

The module is not a translation of the header. It carries what C++ can check
and C cannot, and both are checks rather than facilities.

**The frozen layouts are asserted.** Clause 5.3 declares the layout of every
structure immutable. A declaration that something shall not change is not a
mechanism; `static_assert` is. A handle being one machine word is the
difference between a value returned in a register and one returned through a
hidden pointer, which is a change of calling convention that no declaration
would report and that a consumer built against the earlier layout would not
survive. `kal_node_info` is the one structure that is permitted to grow, and it
carries `self_size` for precisely that reason — the assertion on it fixes the
offsets of the fields that exist, not the size of the whole.

**The capability words become types that cannot be mixed.** Clause 6.2 gives
each interface a word and positions within it, and every such word is a
`kal_uintptr`. A program that tests a file-system position against the task
word therefore compiles, runs, and answers a question nobody asked; the
position numbers are small and several interfaces have assigned the same ones,
so the answer is frequently the plausible one. In the module each interface's
positions carry the interface in their type, the operations that compose them
are defined only within one interface, and the mistake becomes a diagnostic.
Nothing is stored beyond the word and every operation is constant-evaluated.

The same reasoning applies to the flags of `kal_fs_open`, which are an intent
rather than a property and are a distinct type from either.

#### Arrangements considered and not adopted

**Two declarations, compared by a procedure.** Not adopted: it is the
arrangement this clause exists to avoid, and a procedure that compares them is
a thing that can be omitted, skipped or made to pass.

**Generating the header from the modules.** Not adopted: it makes the module
form normative, which contradicts clause 1, and it introduces a generator,
which is a build-time dependency the specification package is written to avoid
having.

**Leaving the C form to each implementation.** Not adopted for the reason
clause 4.1 gives: a name the consumer relies upon would be under the control of
a party the specification does not govern, and a C library ported onto openkal
would take its declarations from whichever implementation it happened to be
built against.

**A second package carrying the C form.** Not adopted: one contract in two
packages can be resolved at two versions, and the property a contract has is
that there is one of it.

### 4.3 Absence of an implementation

A consumer that depends upon the specification and upon no implementation
compiles and fails to link, and the diagnostic names the undefined functions:

    undefined reference to `kal_stream_write'
    undefined reference to `kal_stdout'

The failure is later than a compilation failure would be and is legible. An
arrangement that reported it during compilation was considered and is described
in clause 6.3, together with the reason it was not adopted.

### 4.4 The boundaries a declaration can cross

Compile-time portability is the floor of this specification and not its goal.
Every interface, operation and structure is designed so that it can also be
crossed at run time, and where one cannot the reason is recorded in the table of
clause 3 rather than left to be discovered.

Three boundaries are distinguished, because "crossed at run time" gives opposite
answers for two of them:

| | boundary | the implementation is | what it additionally forbids |
| --- | --- | --- | --- |
| **S** | static | chosen when the artifact is built, and linked into it | nothing |
| **L** | late-bound | a separate object resolved at load, in the caller's address space; the artifact does not change when the implementation does | a structure whose size the consumer fixed when it was built; a report whose value the consumer fixed when it was built |
| **X** | crossed | in another address space or at another privilege level; a call is a trap or a message | a returned pointer into the implementation; a result wider than one machine word; a call back into the consumer |

Returning a pointer into the implementation is admissible at **L** and
inadmissible at **X**; so is a result of two machine words. An interface marked
for one is not thereby marked for the other.

**An interface is X-capable when all six of the following hold.**

1. It exports no object. Everything an implementation reports about itself is an
   operation (clause 6.2).
2. No operation returns a pointer into the implementation's storage. A value is
   copied into a buffer the caller supplies.
3. No result is wider than one machine word. An operation whose whole result is
   a count returns that count or the negated error value; an operation that
   produces a resource returns `int` and writes the resource through a pointer.
4. Every structure that crosses carries its own size, or states why it cannot
   grow.
5. An entry is an address at which a context begins, not a function that is
   called. No return address exists in the started context.
6. Its absence is discoverable by an operation and not only by a linker
   (`kal_interfaces`, clause 3.2).

The first four are decidable from `SURFACE.txt` and the declarations, and clause
9 asserts them. **A new interface is designed against this rule before it is
specified.** The reserved `openkal.event` is the first to which that applies.

## 5. Common definitions

### 5.1 Machine word

`kal_uintptr` is an unsigned integer type wide enough to hold a pointer. It is
obtained from the compiler rather than from a header, because a freestanding
target may provide no header that defines it.

### 5.2 Error values

The set of error values is closed. An implementation maps the error conditions
of its environment onto this set; it does not extend the set.

Mapping is not simulation. A table that translates one error value into another
preserves the property that the implementation is natural for its environment,
whereas reproducing a foreign namespace of names, descriptors or paths does not.

Detail beyond these values is not available. A per-thread channel carrying the
environment's own error value was considered and rejected: such a channel is
global mutable state with the defects of `errno`, and control flow that consults
it is not portable.

Version 0.5 adds five values, and records why, because the set being closed
makes an addition to it the kind of change a reader is entitled to see argued.

| Value | Why the set could not express it |
| --- | --- |
| `kal_err_not_found` | Clause 7.7 already required it. The value did not exist, so both implementations reported a missing name as `kal_err_invalid`, and a caller could not distinguish a name that is absent from a handle that is wrong. The defect was in the enumeration rather than in the implementations. |
| `kal_err_exists` | `kal_fs_open` with `exclusive`, and `kal_fs_mkdir`, fail because the name is already there. It is an expected outcome, and mapping it to `kal_err_io` would report a medium failure for one. |
| `kal_err_not_empty` | Removing a directory that is not empty. Same reasoning. |
| `kal_err_is_directory` | A file operation applied to a directory. |
| `kal_err_not_directory` | A directory operation applied to a file, and a component of a name that is not a directory. |

The addition is governed by clause 8, which admits new declarations. A value
already assigned retains its meaning, so a program compiled against version 0.4
observes the same values for the same conditions as before.

### 5.2.1 How an operation reports its result

There is one rule, and it holds for every interface.

> An operation whose whole result is a **count of bytes** returns `kal_intptr`:
> the count, or the negated error value when no byte was produced. A count of
> zero is a count and not an error; for a read it denotes end of input.
>
> An operation that produces a **resource**, or that produces nothing, returns
> `int` from `kal_error` and writes what it produced through a pointer.

Version 0.8 returned a structure of a count and an error from the operations
that transfer. It is withdrawn, and the measurement that withdrew it is that
every consumer of it collapsed the pair by hand and by this rule — report what
was moved, or the condition when nothing was. The pair was never the shape a
caller wanted, and a result of two words cannot be carried by a boundary that
returns one (clause 4.4).

The collapse is sound because the error set of clause 5.2 is **closed**: a
negated error occupies a small known range and can never be mistaken for a
count. An open-ended error space would not permit it.

### 5.3 Structure layouts

The layout of every structure declared by this specification is frozen at
version 0.3. The evolution rule of clause 8 admits new declarations and excludes
changes to existing ones; a structure layout is not protected by that rule
unless it is separately declared immutable, and it is so declared here.

### 5.4 The interface names its own types and no others

Every type that crosses this interface is one this specification declares.
`kal_uintptr`, `kal_u32`, `kal_u64`, `kal_i64` and the structures of clause 5.3
are the whole vocabulary, together with `void`, `char` and `int`, which have one
meaning everywhere.

A declaration does not name `long`, `size_t`, `uint64_t`, or any other type
whose definition belongs to a C library or to a data model. This is not a
stylistic preference. `sizeof(long)` is eight on one common target and four on
another; a signature naming it would be one signature with two meanings, and the
property clause 1 exists for — that a program's source is invariant under a
change of implementation — would hold only among implementations that shared a
data model.

Clause 5.1 gives the reason the types are derived from the compiler rather than
from a header. This clause states the consequence for everything built above:
**a layer above openkal uses nothing from a concrete backend, including its
types.** The question "how wide is `long` here" does not arise at this interface,
and it does not arise because it cannot be asked.

The one layer for which this does not hold is a C library being ported onto
openkal, and it does not hold there for a reason that is not an exception: such
a library reconstructs POSIX, and POSIX itself names `long`. Its answer is a
property of the target rather than of openkal, and it is configured per target
accordingly.

This clause adds no declaration and alters none, so clause 8's rule is not
engaged and the version does not advance on its account. Every declaration of
version 0.6 already satisfies it — one hundred and one of them, examined by the
procedure below — and what is new is that the property is now stated and
checked rather than held by the care of whoever wrote each header. The five
interfaces added in version 0.8 were examined by the same procedure and satisfy
it also; the count is now one hundred and forty-six.

Clause 9's procedure examines this.

## 6. Capability model

### 6.1 Presence of an interface

An interface that an implementation does not provide is absent as a link-time
definition, and a consumer that uses it fails to link. A conforming
implementation shall not provide an interface whose operations report a lack of
support at run time; the specification treats run-time refusal as a defect and
not as a means of expressing partiality.

An interface that the specification does not define is absent as a module, and a
consumer that imports it fails to compile.

### 6.2 Optional operations and varying properties

An **operation** an implementation may lack becomes an interface of its own, so
that its absence is reported by the linker. A **property** that varies between
implementations is reported by a capability word.

The two are not alike, and the distinction is the one clause 6.4 already draws.
An operation that is present and always fails is a defect; the remedy is that its
absence be expressed by its absence. A property cannot be called: whether names
are compared case-sensitively, whether a clock advances while the machine is
suspended, what the granularity of a measurement is. A program adapts to a
property rather than invoking it.

Each interface that has properties declares a word named `kal_<interface>_props`
and the positions within it. A position, once assigned, retains its meaning; a
position that has not been assigned reads as zero, so that a program compiled
against a later specification behaves correctly against an earlier
implementation.

A property that varies between the *resources* of an interface rather than
between implementations cannot be a word, because there is no one answer to
record in it. Such a property is reported by an enquiry taking the resource.
`kal_stream_props` is the example: the same implementation answers differently
for a terminal and for a file. The enquiry is not the defect clause 6.4
describes, because it is not an operation upon the resource --- nothing is
transferred, and no resource can fail to answer.

Information therefore becomes available at three times, each being the earliest
at which it exists.

| Time | Mechanism | Question answered |
| --- | --- | --- |
| dependency resolution | the implementation package declares what it provides | may this program be built against this implementation |
| link | an undefined symbol | was an interface used that the implementation does not provide |
| run | a capability word | how does this implementation behave within an interface it provides |

### 6.3 Mechanisms considered and not adopted

The rule of clause 6.2 replaced three arrangements that were built before it was
recognised. They are recorded because each is the arrangement a reader is likely
to propose, and because the measurements that exclude them remain useful.

**A module supplied by the implementation**, declaring the optional operation, so
that a consumer importing it without a supporting implementation fails to
compile with the module named. Not adopted: it reintroduces, for the optional
operation, the arrangement clause 4.1 rejects for the interface.

**Fallback overloads in the interface**, displaced by an implementation
declaring its own, with presence detected through argument-dependent lookup. Not
adopted: the arrangement requires an implementation's declarations to be visible
to the consumer, which requires the implementation to own the module the
consumer imports, which contradicts clause 4.

**A record of capability flags**, accompanied by a file in which an
implementation declares them. Not adopted: a record can disagree with the code
it describes, and the file is a second place in which a package is configured.

Two further arrangements were weighed while specifying version 0.8 and are
recorded on the same basis.

**Readiness notification.** An interface reporting that a stream may be read, by
waking a word as `kal_task_wake` does, was considered as the remedy for a context
that would otherwise wait without end. It composes better than the bound this
specification adopted: one operation covers every waitable resource, a single
context may await many sources, and a library above it needs no read-ahead buffer
because a notification consumes nothing.

It was not adopted because of what it asks of an implementation. On an
environment whose readiness is discovered by polling a set of descriptors, an
implementation would have to maintain that set and a context of its own to watch
it. That is a mechanism reconstructed rather than a facility conveyed, which
clause 7.1 excludes. `openkal.timeout` asks the same environment only for what it
already does at the point of the call.

**A space as a handle.** An earlier form of `openkal.space` separated the copying
of an address space from the starting of a context in it, so that a caller held a
space and could start a context in it afterwards. It was withdrawn while the
first implementation was being written.

No environment this specification targets has that pair as a primitive. The copy
and the start are one act, and an implementation asked to separate them would
have to start a context anyway, park it upon a waiting primitive, and build a
channel by which to tell it what to run. Clause 7.1 identifies that as a fault in
the shape of the specification rather than in the implementation, and the
separated form was the shape at fault. The single operation that replaced it is
what every such environment already performs.

**An instant rather than a duration.** `openkal.timeout` states a duration
because `kal_task_wait` does. An instant would not accumulate drift when a caller
retries in a loop, and was considered for that reason. It was not adopted because
it would give one specification two spellings of one idea. A caller that requires
an instant computes the remaining duration from `kal_time_monotonic`, so the cost
falls upon the caller that has the requirement rather than upon every
implementation.

The measurements that constrain any future proposal:

1. A requires-expression naming a qualified entity that does not exist is
   ill-formed. Detection through unqualified lookup and argument-dependent
   lookup is well-formed and evaluates to false.
2. Argument-dependent lookup does not reach a module the translation unit has
   not imported.
3. A module whose name extends another module's name by a further component is
   treated by some build systems as a submodule of it, and the two then form a
   dependency cycle.

### 6.4 Operations that cannot be uniformly present

An operation that some resources of an interface can never satisfy shall not be
placed in that interface. Positioning is the example the specification records.
On a hosted system, whether a stream can be repositioned is a property of the
individual stream rather than of the implementation: the same implementation
succeeds for a regular file and fails for a pipe. An implementation could
therefore neither claim the operation honestly nor withhold it usefully.
Positioning accordingly belongs to `openkal.fs`, whose resource is a descriptor.

### 6.5 Availability settled by how the artifact is produced

An interface may be one the environment can provide only to a program that was
produced in a particular way. `openkal.exec` is the instance: one system grants
a program memory it may execute only when the program carries a signed
declaration that it will ask for such memory, and that declaration is applied
after the link, by whoever produces the artifact.

This is neither of the two partialities clause 6.2 distinguishes. The
implementation does not lack the operation, so withholding the interface
outright would deny it to programs for which it does work. Nor does it vary
between the resources of the interface, which is clause 6.4's case; every region
in a given program behaves alike.

Such an interface shall be provided or withheld at **dependency resolution** —
the first of the three times clause 6.2 tabulates, and the earliest at which the
question can be answered, because the party that decides how the artifact is
produced is the party that resolves its dependencies. An implementation
expresses it as a feature of its package. A program that asks for the feature
gets the interface and an artifact produced so that it works; a program that
does not ask gets neither, and using the interface fails to link, which is
clause 6.1's ordinary report.

The rule this states is general: **an operation whose availability is decided by
how the artifact is produced is not reported at run time.** Reporting it at run
time would make every caller carry a path that most artifacts never take — and
a path no artifact takes is a path nothing has verified.

⚠️ **The rule holds where the artifact is produced for its own environment, and
inverts where it is not.** An artifact that is distributed is produced once, by
a party that decided for every environment it will meet, possibly long before
and for a different system; the consumer resolves nothing. So availability
settled this way is *also* reported at run time, by a position in the
interface's property word — `KAL_EXEC_PROP_AVAILABLE`. The interface is provided
either way, and whether it can be exercised is read.

This is not the shape clause 6.2 forbids. An operation that is present and
always fails is a defect *because the caller cannot tell*, and a position the
caller reads first is what tells it.

The objection above survives and is answered by clause 9 rather than dismissed:
a path few artifacts take is a path little verified, so the conformance
arrangement answers that position both ways. Until it did, the unavailable path
had no producer at all.

### 6.6 Concurrency

An implementation shall permit concurrent operations upon distinct handles.

Concurrent operations upon one handle shall not damage the implementation's own
state. The order in which they take effect, and whether the bytes of one
transfer may be separated by those of another, are unspecified.

Atomicity below a threshold, which some systems guarantee for some resources, is
not required. It is not universally implementable, and requiring it would oblige
an implementation to introduce buffering it does not otherwise need.

### 6.7 Ownership

Handles obtained from the core interfaces are borrowed. They are not released,
and no operation releases them.

Handles obtained from `openkal.fs` and `openkal.process` are owned. Each of those
interfaces provides the operation that releases one, and an implementation shall
not treat a released handle as valid.

The recommended construction divides the handle word into an index and a
generation, incrementing the generation upon release. The specification requires
the property and not the construction; this one achieves it without a lookup
table, and therefore without the compatibility layer clause 7.1 excludes.

## 7. Requirements upon implementations

### 7.1 Naturalness

An implementation shall not require a compatibility layer. The test is
mechanical: an implementation that must maintain a translation table, a
registry, or a name resolver in order to satisfy this specification indicates
that the specification has taken a shape borrowed from one environment, and the
shape is at fault rather than the implementation.

### 7.2 Handles

A handle occupies one machine word and is opaque. An implementation stores
whatever it uses natively: a descriptor, an operating-system handle, a pointer
to a driver structure, or a capability index.

The width is fixed and the interpretation is not, and this is the property that
allows an implementation to be placed above a C library, beneath one, or without
one. A handle declared as a descriptor would oblige an implementation whose
environment has no descriptors to invent a table; a handle declared as a
pointer to a library structure would oblige an implementation with no such
library to invent one.

An implementation shall not assume a process model, a division between
privileged and unprivileged execution, or a namespace shared by all callers. A
handle shall be meaningful in the context of the caller that obtained it.

**An implementation shall refuse a word that is not a handle it issued**, and
shall not act upon it as though it were one it did. The preceding paragraph
constrains a handle's *scope*; this one constrains its *validity*, and the two
are not the same requirement. Under a static link the distinction is academic —
the caller and the implementation are one program. Where the implementation is
on the far side of a boundary it is the boundary itself, and an implementation
that acted upon an arbitrary word would let a caller name a resource it was
never given. The division of a handle into an index and a generation, which
clause 7.2 already recommends for release, satisfies this as well.

### 7.3 Allocation

Where the environment already provides an allocator, `kal_alloc` shall be built
upon it and shall not be built beside it.

The requirement addresses a defect that appears only under load. A C library's
formatted output is commonly coupled to that library's own allocator; an
implementation that introduces a second allocator therefore places two
independent claimants on one region of memory, and on a system whose heap grows
by extending a single region the two will interfere.

### 7.4 Transfers

`kal_stream_write` shall transfer the whole buffer or report the condition that
prevented it. A partial transfer is not a successful outcome.

The alternative convention, under which the caller inspects a count and repeats
the call, places one loop in every caller. Interfaces that adopted it have been
a recurring source of defects. The loop belongs in the implementation, which is
written once.

`kal_stream_read` shall report the number of bytes transferred, which may be
fewer than requested. A result of zero denotes end of input. Unlike a partial
write, a partial read carries information the caller requires.

Both report that number, or the negated error value when no byte was moved, in
one signed machine word — clause 5.2.1. An operation that moved bytes and then
met a condition reports the bytes; the condition is reported by the next call,
which is what every consumer of the earlier two-word form already did by hand.

### 7.5 Interruption

An implementation shall retry an operation that its environment interrupts, and
shall not report the interruption. A caller cannot distinguish an interrupted
call from a genuine failure without knowledge of the environment, and an
implementation that reports it produces short transfers on any system that
delivers asynchronous notifications.

### 7.6 Argument vectors

The vector supplied to `kal_process_spawn` is complete: its first element is the
name the started program shall observe as its own, and an implementation shall
pass the vector unaltered. An implementation shall not derive the first element
from the `path` argument, and shall not prepend, append or reorder elements.

The rule exists because the two sides must agree. A started program reads its
own name through `kal_env_arg(0)`, so a caller that did not supply it could not
predict what the program would read; and the name a program observes as its own
is behaviour on every environment that has an argument vector, so the choice
belongs to the caller rather than to the implementation.

An implementation whose environment has no argument vector shall report
`kal_err_unsupported` from `kal_process_spawn` rather than discard the vector.

This rule was absent from version 0.3, and both implementations existing at the
time prepended the path. They agreed only because they shared an author; a third
implementer had nothing to consult. The defect was found by writing one program
against the specification and running it, which is what clause 9 exists to
require.

### 7.7 Absence as an answer

`kal_fs_info` shall report a name that does not exist by returning `kal_ok` and
setting `kind` to `kal_node_absent`. It shall not report absence as an error.

Enquiry and access are different operations. A caller that asks what a name
refers to has been answered when told that it refers to nothing, and an
implementation that reports absence as a failure obliges every caller to
distinguish that failure from the ones that denote a broken enquiry — a
directory it may not read, a name it may not resolve.

`kal_fs_open` and `kal_fs_open_dir` are access rather than
enquiry, and shall report a name that does not exist as `kal_err_not_found`.

The distinction is the same one `openkal.env` draws between a variable that is
absent and one whose value is empty, and it is drawn for the same reason: a
caller that cannot tell them apart cannot act correctly upon either.

### 7.8 Stating the whole of an intent when opening

`kal_fs_open` takes a flags word. The two-flag form of version 0.3 remains
specified and remains available, and an implementation ordinarily defines it in
terms of the flags form.

The reason for the addition is that three of the conditions a C library must
express cannot be reconstructed above the two-flag form without making the
caller silently wrong, which clause 3.1 identifies as the boundary between a
supply and a simulation.

| Condition | What reconstruction above the two-flag form produces |
| --- | --- |
| truncate | Opening and then setting the length is two operations. A program that stops between them leaves the tail of a longer previous contents behind, and the file is neither the old one nor the new one. |
| exclusive | Enquiring and then opening is not exclusion. Between the two, another context creates the name, and the caller that asked to be the creator is not. |
| append | Seeking to the end and then writing is not appending. A second writer between the two produces an overwrite, and neither writer observes that it happened. |

`kal_fs_truncate` and `kal_fs_file_info` are added for the same reason and are
recorded here rather than in a list of conveniences. Setting the length of an
open file is not expressible through any other operation. Enquiring about an
open file is not expressible through `kal_fs_info`: the name a file was opened
by may since have been removed or given to another file, so a C library
answering the enquiry from the name would answer about a different file than
the one the caller holds.

### 7.9 Termination

`kal_exit` shall terminate immediately. Registered exit handlers and static
destructors shall not run. An implementation that runs them prevents a caller
from reasoning about what executes after the call.

### 7.10 Thread-local storage in a started context

`openkal.task` reports, through `kal_task_props`, whether a context started by
`kal_task_start` observes the thread-local storage of the toolchain that
compiled the program.

It is reported rather than provided. The register convention that delivers
thread-local storage belongs to openarch (clause 10), and an operation here
that installed a thread pointer would place a processor's calling convention in
an interface that is meant to be independent of it.

The property is reported because a C library ported onto openkal keeps its
per-context state --- its error value, its locale, its cancellation state --- in
one such variable, and therefore cannot be ported onto an implementation whose
contexts lack it. Reporting the property allows the library to state that
requirement. An implementation whose contexts are the host's threads has the
property without doing anything; an implementation upon a scheduler of its own
has it only if it establishes the convention, and one that does not shall report
the position as zero rather than leave the question to be discovered.

### 7.11 The inverse of an enquiry

An interface that reports a property of a resource and offers no way to set it
is incomplete wherever the property is one the environment records rather than
derives.

`kal_fs_file_info` reports the time at which a file was last modified.
`kal_fs_set_modified` sets it. The operation was added in version 0.5 because
three ordinary programs — one that copies a file and preserves its dates, one
that extracts an archive, and one whose whole purpose is to mark a file as
current — could not be written above the interface without it, and each of them
is a program a C library above openkal is expected to host. The absence was not
visible in the specification text; it became visible when such programs were
compiled above an implementation.

It takes the open file rather than the name, for the reason given at
`kal_fs_file_info`: the name may since refer to something else, and setting the
time of the wrong file is a worse outcome than not setting it.

The three environments openkal has been implemented on record the time to a
nanosecond, to a microsecond and to a hundred nanoseconds respectively. The
interface states the value in nanoseconds and does not require that it be
returned unchanged; the conformance procedure compares whole seconds, which is
the resolution every environment that records the time at all agrees upon. An
interface that required more would be requiring of every environment what one of
them happens to provide.

An implementation whose environment does not record the time at all does not
claim `KAL_FS_PROP_MODIFIED_TIME` and reports `kal_err_not_supported`. An
implementation that claims the position is required to perform the operation:
clause 6.2 exists so that a claim is a claim about what can be done.

### 7.12 One reserved name

`openkal.fs` names things relative to a directory the program holds, and every
operation that answers a question about a thing takes a name. A program holding
a directory therefore had no way to ask a question about *that* directory: what
it is, when it last changed, whether it can be written. The handle is not a
name, and no operation took a handle alone.

The remedy is one reserved word rather than five more operations. `"."` denotes
the directory itself, wherever a name is accepted. It does not introduce a way
to ascend, so the confinement clause 7.1 depends upon is unaffected; and every
environment openkal has been implemented on can express it, two of them because
their own naming already reserves the same word and the third because its
object manager expresses the same thing as an empty name beside the directory's
handle.

`".."` remains invalid. The asymmetry is the point: the first names the thing
the program already holds, and the second names something it does not.

## 8. Evolution

Each interface is versioned independently. A revision may add declarations and
shall not alter existing ones. Structure layouts are immutable as stated in
clause 5.3.

A consumer declares a dependency upon the specification package, which is the
package it imports, and a dependency upon an implementation, which is the
package that supplies the definitions. The first fixes the version of the
contract; the second is ordinarily conditional upon the target, so that changing
implementation is a change to one line of the manifest and to no line of the
source.

## 9. Conformance procedure

A conformance suite shall verify both halves of an implementation's claim, and
the specification package shall verify the third.

1. **Behaviour.** Every operation the implementation declares shall behave as
   this specification requires.
2. **Surface.** The names an implementation exports beginning with `kal_` shall
   be compared against `SURFACE.txt`. An implementation exports the names of the
   interfaces it provides and no other. This half is a static examination of the
   artefact and is therefore neither slow nor susceptible to incomplete
   coverage, and it is the half that detects an implementation which has
   extended the interface rather than implemented it.

3. **Declarations.** The declarations, clause 4.2, shall be compared against
   `SURFACE.txt` by compiling a translation unit that names every entity the
   list contains. This half belongs to the specification package rather than to
   an implementation. It does not compare the two ways of reaching the
   declarations with each other, because there is one declaration and no
   comparison to make; what it detects is a name the list requires and the
   header does not declare. The translation unit is compiled without the
   environment's headers, because the consumer the header exists for is.

4. **Types.** The declarations shall be examined for the types they are written
   with, and shall name none that clause 5.4 excludes. This half also belongs to
   the specification package. It is distinct from the second: an implementation
   may export exactly the names `SURFACE.txt` requires and still declare one of
   them taking a `size_t`, and the surface comparison would report conformance.

   The examination asks a compiler what it parsed rather than searching the
   text, so that a type reached through a macro or an include is seen, and the
   typedef names are read as written rather than resolved to the underlying
   type — a declaration written `kal_u64` and one written `uint64_t` denote the
   same type on one target and different types on another, which is the whole
   subject of clause 5.4.

   ⚠️ A check that reports conformance by finding nothing reports it identically
   when it has read nothing. The procedure therefore establishes that the
   declarations were parsed before it is permitted to report success, and the
   specification package demonstrates that the check fails when an excluded type
   is present.

Behavioural conformance cannot be established exhaustively, and this
specification does not claim otherwise. An implementation may export the correct
names, satisfy every test, and fail on an untested input. The residue is the
coverage of the suite, and no arrangement of declarations removes it.

## 10. Relationship to adjacent specifications

openkal is one of three boundaries and is distinguished from the other two by
the number of implementations that coexist within one program.

| Specification | Implementations in one program | Form of the contract |
| --- | --- | --- |
| openarch | one, being the processor | concepts, with link-time optimisation required |
| openkal | one per interface | a C application binary interface |
| openhal | many, being the devices | concepts |

openarch and openhal both express their contracts as concepts, and for opposite
reasons. An openarch operation such as disabling interrupts is a single
instruction, and the cost of a function call is accordingly prohibitive.
An openhal contract must admit several implementations within one program, which
a set of C functions cannot. openkal is subject to neither pressure: an openkal
operation ordinarily crosses a protection boundary, so the cost of a call is
immaterial, and one implementation per interface suffices.

A device supplied through openhal may serve as the implementation of an openkal
interface. The relationship is one of connection rather than of layering, and
the connection is made by the package that describes the board.

## 11. Matters this version does not settle

The following are recorded so that they are not mistaken for oversights.

1. **Concurrency.** The behaviour of concurrent operations upon one handle is
   unspecified. The question becomes unavoidable when `openkal.task` is
   specified, and version 0.3 does not specify it.
2. **Ownership.** Every handle in the core interfaces is borrowed. Owned
   handles arrive with `openkal.fs`, and the rules for their release, including
   the effect of releasing one twice, are deferred to that interface.
3. **Symbol versioning.** The C application binary interface carries no version
   in its symbol names. Clause 8 protects the interface by prohibiting change
   rather than by permitting coexistence, and an ecosystem that outgrows that
   prohibition will require a mechanism this version does not define.
4. **Readiness.** Awaiting one of several sources is not an operation of this
   specification. It is reached above the interface, from `openkal.task` and a
   bound upon each wait; clause 6.3 records the alternative that was weighed and
   the property of implementations that excluded it.
5. **Name resolution.** `openkal.net` and `openkal.datagram` carry an address and
   a port. Turning a name into one is excluded by clause 3.4 and remains so: an
   implementation shall not be required to parse an unbounded set of name
   schemes.
6. **Permission and ownership of files.** Not defined, and not a deferral. A
   permission presupposes an identity, and the environments this specification
   targets do not agree that one exists.

   ⭐ **And what a program should write instead, which was not recorded and is
   now.** A program that means "only I may read this" is stating it in a
   vocabulary this environment does not have. It has three answers, and none of
   them is a mode:

   - against another part of the same program, the capability already does it:
     a handle not given cannot be reached;
   - against another user of the machine, it is the *environment's*
     responsibility — the party that starts the program supplies a preopen that
     others do not have;
   - against a location the program does not trust, it encrypts the contents.

   ⚠️ The measurement that settles the alternative: a mode word is not stored by
   several filesystems a hosted implementation will meet, and on such a volume
   `chmod` **reports success and changes nothing**, which is the outcome this
   specification exists to refuse. `kal_node_info.writable` — one boolean — is
   not a simplification of a mode word; it is the intersection of what those
   formats store.

7. **Creation and reading of links.** ⚠️ **Settled in 0.9.** `kal_fs_link_create`
   and `kal_fs_link_read` are operations of `openkal.fs`, and they are operations
   of it rather than an interface of their own for exactly the reason this entry
   used to give: whether a volume has such nodes is a property of the format and
   not of the environment, so the variability is between the *resources* of one
   implementation. Clause 6.2 says such a property is answered by an enquiry
   taking the resource, and `kal_fs_props` is now that enquiry — which is what
   makes the operations admissible, because a caller can ask before it calls.

   Resolution is stated in `fs.h` beside each operation it governs, and no
   longer only here. It was here alone, and two call sites of one implementation
   consequently chose opposite directions: opening resolved a link while asking
   did not, so a program was told that a name referred to a link when opening it
   would have reached a file.
8. **Duplication of the calling image.** `fork` is refused by clause 7.1 and that
   refusal stands. It is refused as an OPERATION. The atomic capabilities from
   which a library may compose it are specified: `openkal.space` clones an
   address space and starts a context in one, and `KAL_SPACE_PROP_CLONE_HANDLES`
   states whether the handles accompany the memory. What this specification
   declines to do is duplicate execution state, which a library above the
   interface performs with the compiler's own facilities. A sentence reading
   "openkal will not have fork" would have buried that distinction, and this
   entry exists so that it is not written.
9. **Transfer of a handle between address spaces.** `kal_space_start` conveys no
   handle, and `kal_process_channel` conveys a stream only across a spawn. A
   general mechanism for passing a handle to a context in another space is not
   defined by this version. It is the question `openkal.space` reaches first and
   is not peculiar to it.
10. **Exclusion upon a range of a file.** ⚠️ **Settled in 0.10.** `kal_fs_lock`
    and `kal_fs_unlock` are operations of `openkal.fs`, admitted on exactly the
    grounds entry 7 records for links: whether a *volume* can exclude is a
    property of the format rather than of the environment, so `kal_fs_props`
    answers it and a caller asks before it calls.

    ⭐ **And this one is the opposite of entry 6, which is why the two are next
    to each other.** Permission was declined because the environments do not
    agree that an identity exists. Every environment this specification targets
    locks a byte range, and spells it almost identically. What was missing was a
    word, not a capability.

    ⚠️ **What its absence cost, and it was not a refusal.** A C library above
    this interface answers `fcntl(F_SETLK)`. With nothing here to answer it
    with, one returned success and took no lock — **two programs held one
    exclusive lock and neither could find out**. Measured against a host. This
    entry exists so that "a specification with no operation for X" is not
    mistaken for "consumers of it will simply not do X".

    ⭐ **Release upon the holder's end is required rather than observed**, and
    that requirement is the whole reason the operation cannot be composed above
    the line: a caller can build exclusion out of `KAL_OPEN_EXCLUSIVE` and a
    name, and nothing then releases that name when its holder dies.
11. **A started program that does not outlive its caller.** ⚠️ **Settled in
    0.10** as `kal_process_spawn_bound`, **and respelled in 0.11** as
    `KAL_SPAWN_BOUND_LIFETIME` — see entry 14.

    Clause 7.1 declines to replace a running image, and that stands. The
    consequence, which this entry did not previously record, is that a C library
    asked for `execve` composes it — and the composition leaves **three** images
    where a system with the operation has two: the caller, a copy that waits,
    and the program.

    ⚠️ **A signal reaches the middle one.** `kal_process_terminate` upon the
    identifier the caller holds terminates the waiter; measured with a host as
    control, the caller is told the program died on the signal it sent while the
    program runs to completion, unsupervised. The termination operation was not
    at fault — it was asked to terminate one started program and did. What was
    missing was a way to *say* the thing `execve` means.
12. **How many contexts run at once.** ⚠️ **Settled in 0.10.**
    `kal_task_parallelism`. `KAL_TASK_PROP_PARALLEL` says *whether* and not *how
    many*, and a C library above had nowhere else to look: `hardware_concurrency`
    answered 1 with no error, so a program sizing a pool of workers got one
    worker and no way to know. ⭐ Zero means "cannot say" and is distinct from
    one, because an environment with one processor and an environment that will
    not answer call for different behaviour.
13. **The time of a name, and the size of a volume.** ⚠️ **Settled in 0.10.**
    `kal_fs_set_modified_at` and `kal_fs_capacity`. The first exists because
    `kal_fs_set_modified` is stated on an open *file* while a directory is
    opened as a `kal_dir` — so this interface had no route to a directory's time
    at all, and a consumer that stamps a lock directory drove one implementation
    to open the directory for *reading* and set the time on that, outside
    anything stated here. A divergence caused by a missing declaration is a
    defect of the specification and is recorded as one.

    The second, `kal_fs_capacity`, exists for the reason that keeps it in this
    interface rather than in `openkal.space`: how much room a volume has is a
    property of the *names* a caller can already reach, not of the memory a
    program runs in. A C library above answers `statvfs`, and with nothing here
    to answer it with it reported a fixed number — ⚠️ **which is worse than
    refusing, because a program that checks for room before writing was told
    there was room.**

    ⭐ It answers bytes and not blocks, and that is the whole of the design
    decision. Every environment this specification targets states a block count
    and a block size, in units of its own choosing, and every one of them
    differs; a caller wanting bytes multiplies two numbers whose meaning it must
    first look up. Bytes are what the caller is deciding about, so bytes are
    what this returns and the multiplication happens once, in the
    implementation, where the units are known.
14. **Starting a program was becoming a family, and 0.11 stopped it.** ⚠️ **This
    is the first entry that records a REMOVAL**, and it is recorded here for the
    same reason the additions are: so that the next reader meets the reasoning in
    the specification rather than in a diff.

    0.10 ended with three declarations — `kal_process_spawn`,
    `kal_process_spawn_with` and `kal_process_spawn_bound` — which were never
    three operations. They were one operation and three *modifiers* of it,
    spelled apart for exactly one reason: clause 8 forbids adding an argument to
    a declaration that exists, so each modifier had to arrive as a new name.

    ⭐ **The 0.10 text on `..._bound` predicted where that ends, in terms**:
    "Declaring every combination is how an interface acquires four spawns and
    then eight." Two more modifiers then arrived together — the directory a
    program runs in, and terminating what a started program itself started — and
    the evidence that they arrive *together* was a single consumer signature
    whose child calls `setpgid(0, 0)` and `chdir(cwd)` on adjacent lines. Four
    orthogonal modifiers is sixteen declarations, each written in every
    implementation.

    ⇒ So the modifiers moved into `struct kal_spawn` and the family collapsed to
    one declaration. **`..._with` and `..._bound` were removed rather than kept
    beside it.** openkal has no users outside this ecosystem, so there was no one
    to keep a second spelling for; an interface that offers two ways to say one
    thing must explain the difference for ever, and this one had no difference to
    explain. A caller written against the old shape fails to **compile** —
    different arity, different types — which is the loud failure and not the
    quiet one.

    ⚠️ **What this costs, stated rather than glossed.** `struct kal_spawn` is
    frozen by clause 5.3, so a future modifier that carries a *parameter* rather
    than a flag cannot be added to it and will need its own declaration after
    all. This entry does not claim to have solved that; it claims that one
    general form plus a flag word is a better place to meet the problem than
    sixteen names, and that the modifiers wanted so far are all flags.

    ⚠️ **And clause 8 was set aside to do it**, deliberately and once. Clause 8
    exists to protect written code, and 0.11 is the last version at which there
    is none to protect. An equivalent change after this specification has
    consumers is not permitted by clause 8 and this entry is not a precedent for
    one.
