// The openkal conformance suite.
//
// Clause 9 requires that both halves of an implementation's claim be verified.
// The surface half is a static examination of the artefact and is performed by
// tools/check-surface.sh; the declarations half is performed by
// tools/check-declarations.sh. This program is the behavioural half, and it
// carries three further kinds of examination that clause 9 does not require and
// that an implementer wants: the shapes the specification freezes, the same
// operation many times, and what an operation costs.
//
// It is composable in the same way openkal is. An interface is the unit of
// provision, an implementation provides one in whole or not at all, and one it
// does not provide is absent as a link-time definition --- so a suite that
// examined all eight unconditionally would fail to link against a conforming
// implementation of five, and would report nothing rather than reporting five.
//
//     mcpp run                          the core set
//     mcpp run --features standard      every interface version 0.5 defines
//     mcpp run --features fs,task       the core set, and these two
//     mcpp run --features full          every interface and every kind
//
// The implementation under examination is named by whoever runs the suite, as
// a second dependency, exactly as any consumer names one.
import okc.suite;
import okc.child;

int main() {
    // Two of the specification's requirements end the program that satisfies
    // them, so they are observed in a copy this program starts. A copy is told
    // what to do through its argument vector and answers through its status.
    const okc::errand e = okc::child_errand();
    if (e != okc::errand::none) okc::perform(e);

    return okc::run_all();
}
