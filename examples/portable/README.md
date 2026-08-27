# The portable program

One program that exercises the eight interfaces the specification defines. It
names no implementation, no operating system, no descriptor number and no
system call.

Its purpose is to be built over every implementation. The specification's own
continuous integration does that: the conformance job builds this source against
each implementation it tests and requires the final count to be zero, so the one
source is exercised over all of them rather than copied into each.

That sentence used to describe the implementations' own workflows doing it. None
of them did, and the manifest here had drifted three specification versions
behind without anything noticing --- which is what a claim with no criterion
behind it costs.

It prints one line per interface, each beginning `openkal: `, and a final line
reporting the number of observations that did not hold. An implementation
passes when that number is zero.

The program requires an implementation that provides every interface. An
implementation that omits one fails to link, and the linker names the operations
it did not define — which is the diagnostic clause 4.2 describes.
