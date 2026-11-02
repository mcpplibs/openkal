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
export using ::kal_prof_props;
export using ::kal_prof_sample_begin;
export using ::kal_prof_sample_take;
export using ::kal_prof_sample_end;
export using ::kal_prof_sample;

// A sample (0.23): the time, the context, the depth and its reserve, then the
// frames, one machine word each.
static_assert(__builtin_offsetof(kal_prof_sample, depth) == 16, "clause 5.3");
static_assert(__builtin_offsetof(kal_prof_sample, pc) == 24, "clause 5.3");
