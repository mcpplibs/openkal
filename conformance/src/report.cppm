// okc.report --- how an observation is recorded and how the run is summarised.
//
// Three states, and the third is the one that matters. A suite reporting only
// "held" and "did not hold" cannot distinguish an interface that behaved from
// an interface it never examined, and a run in which nothing was examined then
// reports success. Every unmade observation therefore carries the reason it was
// not made, and the count of them is part of the verdict a reader reads.
export module okc.report;

import openkal.types;
import okc.spec;

export namespace okc {

// An observation that held, or did not.
void observe(kind k, bool held, const char* what);

// An observation that was not made. The reason is a parameter rather than an
// option: an unexplained omission is indistinguishable from an oversight.
void unobserved(kind k, const char* what, const char* because);

// A measurement. It is never a verdict; see okc.spec.
void measure(const char* what, __UINT64_TYPE__ total_ns, int iterations);

// A note attached to the report: what the implementation claims about itself,
// written out so that a reader can compare two implementations without running
// either.
void claim(const char* what, kal_uintptr word);

void heading(const char* text);
void line(const char* text);

// The report's own three counts.
int held_count();
int failed_count();
int unobserved_count();

// Writes the inventory, then returns the status the program exits with.
void write_inventory();
int  summarise();

// Formatting, exported because the sections report values as well as verdicts.
void put(const char* s);
void put_signed(long long v);
void put_hex(kal_uintptr v);

}  // namespace okc
