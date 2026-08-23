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

// ⚠️ C LINKAGE, BECAUSE ON A MACHINE WITH NO OPERATING SYSTEM `main` IS AN
// ORDINARY FUNCTION.
//
// A hosted implementation makes `main` the reserved entry point, and a startup
// object that refers to it by that name finds it whatever language the program
// is written in. A freestanding one does not: `-ffreestanding` says the library
// is absent, so `main` loses its special status and is mangled like anything
// else. Measured 2026-08-23, building this suite for `riscv64-none-elf` against
// `openkal-opensbi`:
//
//     ld.lld: error: undefined symbol: main
//     >>> referenced by start.cpp:204 … obj/…/start.o:(__okb_start_c)
//     >>> did you mean to declare main() as extern "C"?
//
// while `nm` on this file's object showed `_Z4mainv`.
//
// ⇒ The declaration says what is true of this program rather than working
// around something. It is harmless where `main` is reserved, because there the
// two spellings are the same symbol; and clause 9's behavioural half is
// required of every implementation, including one of a machine with no
// operating system, so this program has to be linkable in both arrangements.
extern "C" int main() {
    // Two of the specification's requirements end the program that satisfies
    // them, so they are observed in a copy this program starts. A copy is told
    // what to do through its argument vector and answers through its status.
    const okc::errand e = okc::child_errand();
    if (e != okc::errand::none) okc::perform(e);

    return okc::run_all();
}
