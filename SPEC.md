# openkal Specification, version 0.1

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
| `openkal.abort` | termination | yes |
| `openkal.stream` | a byte stream | yes |
| `openkal.memory` | a region of memory | yes |
| `openkal.time` | a time source | no |
| `openkal.task` | an execution context | no |
| `openkal.fs` | a file-system descriptor | no |
| `openkal.net` | a network endpoint | no |
| `openkal.channel` | a message channel | no |

Version 0.1 specifies the core interfaces only. The remaining rows are reserved:
their names shall not be used for other purposes, and their contents are not yet
normative.

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
   resources can never satisfy, which clause 6.2 identifies as a defect.
3. The precedent cited in its support was misread. The WebAssembly System
   Interface, second preview, separates resource kinds into distinct interfaces
   and shares only the stream type between them.

The stream is therefore the shared currency of the specification and not its
common entrance.

## 4. Module organisation

Two module names participate, and their ownership differs.

| Module | Provided by | Imported by |
| --- | --- | --- |
| `openkal.decl.<interface>` | the specification package | implementations |
| `openkal.<interface>` | the implementation package | consumers |

An implementation shall provide `openkal.<interface>` and shall re-export
`openkal.decl.<interface>` from it. A consumer shall import
`openkal.<interface>` and shall not import the implementation package's other
modules, if it has any.

### 4.1 Why the implementation owns the consumer-visible name

Argument-dependent lookup does not reach a module that the translation unit has
not imported. An implementation's optional operations are therefore invisible to
a consumer unless they are declared in the module the consumer imports. Placing
the consumer-visible name in the specification package would make optional
capabilities undetectable, and the specification prefers the arrangement in
which they are detectable.

### 4.2 Naming constraint

The declaration module shall be named `openkal.decl.<interface>`. It shall not
be named `openkal.<interface>.decl`.

The prohibition is not stylistic. Build systems that derive module dependencies
from the dotted name treat the second form as a submodule of
`openkal.<interface>` and report a dependency cycle. The constraint is recorded
here with its reason because a reader who is given only the rule will reach for
the prohibited form.

### 4.3 What an implementation may add

An implementation shall not redeclare a type, concept or function template
obtained from `openkal.decl.<interface>`. This prohibition is enforced by the
language: a module that redeclares an imported entity is rejected during
compilation.

The remaining freedom is the addition of overloads. An implementation shall add
only those overloads that this specification lists as optional capabilities of
the interface. An implementation that offers facilities beyond the
specification shall place them in a module whose name is not of the form
`openkal.<interface>`, so that a consumer relying on them does so visibly.

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
version 0.1. The evolution rule of clause 8 admits new declarations and excludes
changes to existing ones; a structure layout is not protected by that rule
unless it is separately declared immutable, and it is so declared here.

## 6. Capability model

### 6.1 Presence of an interface

An interface that an implementation does not provide is absent as a module. A
consumer that imports it is rejected during compilation, and the diagnostic
names the module. A conforming implementation shall not provide an interface
whose operations report a lack of support at run time; the specification treats
run-time refusal as a defect and not as a means of expressing partiality.

### 6.2 Presence of an operation within an interface

Optional operations are declared as function templates in
`openkal.decl.<interface>` whose bodies fail to instantiate and whose return
type is `kal::unsupported_t`. An implementation supplies such an operation by
declaring an overload returning the interface's ordinary result type, and
withholds it by declaring nothing.

A consumer detects the presence of an operation with the concept the interface
provides. The concept examines the return type, because a fallback and a real
implementation that agreed in return type would be indistinguishable: a
requires-expression does not instantiate a function body, so the failing
assertion inside the fallback would never be reached.

A consumer that calls an absent operation without first testing for it is
rejected during compilation, and the diagnostic carries the wording this
specification supplies. That behaviour is the default and requires nothing of
the consumer.

No separate record of capabilities exists, and none shall be introduced. A
record can disagree with the code it describes; a declaration cannot. An
implementation cannot claim an operation it has not declared, and an operation
it has declared but not defined is reported at link time.

### 6.3 Operations that cannot be uniformly present

An operation that some resources of an interface can never satisfy shall not be
placed in that interface. Positioning is the example the specification records.
On a hosted system, whether a stream can be repositioned is a property of the
individual stream rather than of the implementation: the same implementation
succeeds for a regular file and fails for a pipe. An implementation could
therefore neither claim the operation honestly nor withhold it usefully.
Positioning accordingly belongs to `openkal.fs`, whose resource is a descriptor.

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

A consumer declares a dependency upon the specification package in addition to
its dependency upon an implementation. The second dependency is what selects an
implementation; the first is what fixes the version of the contract the consumer
is written against, and it converts a mismatch between consumer and
implementation into a failure of dependency resolution rather than a collection
of signature errors at compile time.

## 9. Conformance procedure

A conformance suite shall verify both halves of an implementation's claim.

1. **Behaviour.** Every operation the implementation declares shall behave as
   this specification requires.
2. **Absence.** Every operation the implementation does not declare shall be
   absent from the exported names of `openkal.<interface>`. This half is a
   static examination of the artefact and is therefore neither slow nor
   susceptible to incomplete coverage.
3. **Surface.** The exported names of `openkal.<interface>`, and their
   signatures, shall be compared against the specification's list. A signature
   that differs in a parameter type is not a benign deviation: it remains
   selected by argument-dependent lookup through an implicit conversion while
   its semantics may differ.

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
   specified, and version 0.1 does not specify it.
2. **Ownership.** Every handle in the core interfaces is borrowed. Owned
   handles arrive with `openkal.fs`, and the rules for their release, including
   the effect of releasing one twice, are deferred to that interface.
3. **Symbol versioning.** The C application binary interface carries no version
   in its symbol names. Clause 8 protects the interface by prohibiting change
   rather than by permitting coexistence, and an ecosystem that outgrows that
   prohibition will require a mechanism this version does not define.
