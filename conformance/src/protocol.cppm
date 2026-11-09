// The mtest test protocol, version 1, as this suite speaks it.
//
// A runner (mtest, a host's own test runner, a continuous-integration step)
// learns what was examined from the protocol's events rather than from the
// suite's text: one case per interface, of kind `cts', naming the interface as
// the clause it covers; each observation a case within it (protocol P3.4) that
// passed, failed or was not observed; each measurement of cost a metric. The
// interfaces are cases because they are what a selection names and what an
// implementation provides in whole or not at all; the observations are found
// only by running, so they are reported as they run and listed by none.
//
// WRITTEN WITHOUT A LIBRARY, AS THE SUITE IS. The suite runs beneath a C
// library and on a machine with none, so the events are composed here and
// written through openkal.fs; where that interface or openkal.env was not
// selected, no event is written and a runner judges the program by its exit
// status alone (protocol P1.1), which is what the suite reported before.
export module okc.protocol;

import openkal.types;
import okc.spec;

export namespace okc::protocol {

// Reads the handshake (protocol P2). True when a runner speaks the protocol and
// the report could be opened.
bool begin();
bool active();
bool listing();               // MTEST_MODE=list: list and run nothing

// Whether an interface's case is selected (MTEST_SELECT, P4); every case when
// no selection was given or the protocol is not spoken.
bool wanted(const char* id);

void list_case(const char* id);
void section(const char* heading_text);     // ends the previous case, begins the next
void observation(kind k, const char* what, bool held, const char* because);
void metric(const char* what, kal_u64 ns);
// Ends the last case, reports the selections that named no case (P4.1) and
// writes `done'. Returns the exit status the protocol requires.
int finish(int held, int failed, int unobserved);

}  // namespace okc::protocol
