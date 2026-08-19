# openkal

openkal is a portable kernel application binary interface. This repository
contains the normative specification and the C++ modules that declare it.

The specification is [`SPEC.md`](SPEC.md).

## What this package contains

Declarations, and no definitions. Every function declared here is supplied by an
implementation package; building this package alone produces a library with
undefined references, which is the intended outcome.

| Module | Interface |
| --- | --- |
| `openkal.decl.types` | shared definitions: machine word, error values, transfer result |
| `openkal.decl.abort` | termination |
| `openkal.decl.stream` | byte streams |
| `openkal.decl.memory` | allocation |

## How a program uses openkal

A program declares two dependencies. The first fixes the version of the contract
it is written against; the second selects an implementation and is ordinarily
conditional on the target.

```toml
[dependencies]
openkal = "0.1.0"

[target.'cfg(os = "linux")'.dependencies]
openkal-linux = "0.1.0"
```

The program imports the interface and names no implementation.

```cpp
import openkal.stream;

int main() {
    const char greeting[] = "hello\n";
    kal::write(kal::out(), greeting, sizeof(greeting) - 1);
    return 0;
}
```

Changing the implementation is a change to the second dependency. The source
does not change, and this property is the reason the specification exists.

## How an implementation is written

An implementation provides `openkal.<interface>` and re-exports
`openkal.decl.<interface>` from it. The reference implementation is
[`openkal-linux`](https://github.com/mcpplibs/openkal-linux), which is
maintained as a worked example in addition to being usable.

Clause 4 of the specification states the module naming rules, clause 6 states
how optional capabilities are expressed, and clause 7 states the requirements an
implementation must satisfy.

## License

Apache-2.0.
