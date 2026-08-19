# The portable program

One program that exercises the eight interfaces the specification defines. It
names no implementation, no operating system, no descriptor number and no
system call.

Its purpose is to be built by every implementation. An implementation's
continuous integration fetches this source from the specification repository at
the version its manifest names, builds it against itself, and asserts the lines
below. The source is therefore identical everywhere by construction rather than
by a copy that can drift.

It prints one line per interface, each beginning `openkal: `, and a final line
reporting the number of observations that did not hold. An implementation
passes when that number is zero.

The program requires an implementation that provides every interface. An
implementation that omits one fails to link, and the linker names the operations
it did not define — which is the diagnostic clause 4.2 describes.
