// openkal.prof --- what a program states about its own work, for whoever is
// measuring it (version 0.20).
//
// A counter is the caller's own cell, incremented without a call and read by the
// measurement when it records; a phase is a pair of marks. Where nothing
// measures, `kal_prof_active' answers zero once and a layer registers nothing.
module;
#include <openkal/prof.h>

export module openkal.prof;
export import openkal.types;

export using ::kal_prof_active;
export using ::kal_prof_counter;
export using ::kal_prof_mark;
