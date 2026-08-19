# openkal Specification, version 0.3

## 1. Scope

openkal defines an interface between a program and the environment that
executes it. The interface is stated as a C application binary interface and is
distributed as a set of C++ modules that declare it.

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
| `openkal.event` | readiness of a set of resources | reserved |

Version 0.3 specifies the core and standard interfaces. The optional rows are
specified; the reserved row is not, and its name shall not be used for other
purposes.

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

### 3.2 Interfaces the specification declines to define

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

## 4. Module organisation

The specification package provides one module per interface. An implementation
provides no module at all.

| Module | Provided by | Imported by |
| --- | --- | --- |
| `openkal.types` | the specification package | the other interface modules |
| `openkal.abort` | the specification package | consumers, implementations |
| `openkal.stream` | the specification package | consumers, implementations |
| `openkal.memory` | the specification package | consumers, implementations |

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

### 4.2 Absence of an implementation

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

### 5.3 Structure layouts

The layout of every structure declared by this specification is frozen at
version 0.3. The evolution rule of clause 8 admits new declarations and excludes
changes to existing ones; a structure layout is not protected by that rule
unless it is separately declared immutable, and it is so declared here.

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

### 6.5 Concurrency

An implementation shall permit concurrent operations upon distinct handles.

Concurrent operations upon one handle shall not damage the implementation's own
state. The order in which they take effect, and whether the bytes of one
transfer may be separated by those of another, are unspecified.

Atomicity below a threshold, which some systems guarantee for some resources, is
not required. It is not universally implementable, and requiring it would oblige
an implementation to introduce buffering it does not otherwise need.

### 6.6 Ownership

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

### 7.6 Termination

`kal_exit` shall terminate immediately. Registered exit handlers and static
destructors shall not run. An implementation that runs them prevents a caller
from reasoning about what executes after the call.

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

A conformance suite shall verify both halves of an implementation's claim.

1. **Behaviour.** Every operation the implementation declares shall behave as
   this specification requires.
2. **Surface.** The names an implementation exports beginning with `kal_` shall
   be compared against `SURFACE.txt`. An implementation exports the names of the
   interfaces it provides and no other. This half is a static examination of the
   artefact and is therefore neither slow nor susceptible to incomplete
   coverage, and it is the half that detects an implementation which has
   extended the interface rather than implemented it.

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
