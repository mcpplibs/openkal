# openkal Specification, version 0.6

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

| Interface | Resource | Core |
| --- | --- | --- |
| `openkal.abort` | termination | core |
| `openkal.stream` | a byte stream | core |
| `openkal.memory` | a region of the address space | core |
| `openkal.env` | the parameters a program receives at inception | standard |
| `openkal.time` | a time source | standard |
| `openkal.fs` | a directory, and an open file | standard |
| `openkal.process` | a program image that has been started | standard |
| `openkal.task` | an execution context, and a suspension primitive | standard |
| `openkal.exec` | a region of the address space a program may execute | optional |
| `openkal.event` | readiness of a set of resources | reserved |

Version 0.6 specifies the core, standard and optional interfaces. The reserved
row is not specified, and its name shall not be used for other purposes.

*Core* denotes an interface every implementation provides. *Standard* denotes
one an implementation hosting a C library provides. *Optional* denotes one it
may omit without ceasing to host a C library, at the cost of the facilities
built upon it.

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

### 3.3 Naming a set of interfaces

An interface is provided or not provided, and clause 6.1 makes that a fact a
consumer learns from the linker. A consumer must nevertheless be able to *state*
what it needs before it is built, and stating it one interface at a time does
not scale: the sets a program can be written against are not arbitrary subsets,
and an ecosystem that could only enumerate them could not say "this package
needs an environment of such-and-such a kind".

The specification therefore names sets, and the names are for stating a
requirement rather than for measuring conformance:

| Name | Interfaces |
| --- | --- |
| `core` | `openkal.abort`, `openkal.stream`, `openkal.memory` |
| `hosted` | `core`, and `openkal.env`, `openkal.time`, `openkal.fs`, `openkal.process`, `openkal.task` |

A name is a shorthand and confers nothing. **An implementation does not claim a
name**; it provides interfaces, and whether it satisfies a consumer that asked
for `hosted` follows from which interfaces it provides. Nothing in this clause
alters clause 6.1: an interface a consumer uses and an implementation does not
provide is an undefined symbol, whichever names either of them mentions.

Optional interfaces are deliberately not gathered into a name. A set that
required every optional interface would make *optional* mean nothing, and a
consumer that needs one names that one.

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
mechanism; `static_assert` is. `kal_io_result` being two machine words is the
difference between a result returned in registers and one returned through a
hidden pointer, which is a change of calling convention that no declaration
would report and that a consumer built against the earlier layout would not
survive.

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
checked rather than held by the care of whoever wrote each header.

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
fewer than requested. A result of zero bytes with `kal_ok` denotes end of input.
Unlike a partial write, a partial read carries information the caller requires.

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

`kal_fs_open_file`, `kal_fs_open` and `kal_fs_open_dir` are access rather than
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
