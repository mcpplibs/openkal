# Substitution

The property this example exists to demonstrate is that an application depends
upon the specification and not upon an implementation. Two implementations are
provided. The application source is compiled against each without modification
and without conditional compilation.

| Implementation | Behaviour |
| --- | --- |
| `impl-fd` | writes to descriptors |
| `impl-discard` | accepts every transfer and discards it |

## Procedure

```sh
cd app
mcpp run                      # the line appears
sed -i 's/openkal-fd = { path = "..\/impl-fd" }/openkal-discard = { path = "..\/impl-discard" }/' mcpp.toml
mcpp run                      # the line does not appear
```

The source file is unchanged between the two invocations, which may be verified
with a checksum. The dependency named in `mcpp.toml` is the only difference.

## What the example does not show

It does not show that an implementation may be placed above or below a C
library. That property follows from the opacity of the handle and is
demonstrated by `openkal-linux`, whose handles are descriptors, together with a
freestanding implementation, whose handles are indices into a driver table.
