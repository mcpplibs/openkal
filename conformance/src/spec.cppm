// okc.spec --- WHAT is examined, expressed as data.
//
// The inventory lives here so that adding an interface to the specification
// means adding a row and a section, and means editing neither the driver nor
// the report. The driver knows how to run a section; this module knows which
// sections exist, which of them the build selected, and what each one is for.
export module okc.spec;

import openkal.types;

export namespace okc {

// The four kinds of examination.
//
// They are distinguished because they answer different questions and because a
// reader must be able to tell which question a line in the report belongs to.
// The fourth is the reason the distinction is not decoration: a cost is a
// number that varies with the machine, so it can be reported and cannot be a
// verdict, and a suite that failed on a loaded runner would be discarded rather
// than consulted.
enum class kind {
    behaviour,   // what the specification requires of the operation
    abi,         // the shapes clause 5.3 freezes, and the words clause 6.2 defines
    stability,   // the same operation many times
    cost,        // reported, never asserted
};

constexpr const char* name_of(kind k) {
    switch (k) {
        case kind::behaviour: return "behaviour";
        case kind::abi:       return "abi";
        case kind::stability: return "stability";
        case kind::cost:      return "cost";
    }
    return "?";
}

// Which interface a section examines. The order is the order clause 3 lists
// them in, and the report follows it, so two runs of different implementations
// can be read side by side.
enum class interface_id {
    abort, stream, memory, env, time, fs, process, task, count
};

struct interface_row {
    const char*  name;
    bool         core;      // clause 3: every implementation provides it
    bool         selected;  // this build examined it
    const char*  feature;   // what to pass to examine it
};

// Selection is decided once, here, by the macros the build defines for the
// features it activated. Every other translation unit reads the answer rather
// than asking the question, so a section added later cannot forget to be
// listed and a feature renamed later cannot leave a stale conditional behind.
inline constexpr interface_row inventory[] = {
    { "openkal.abort",   true,
#ifdef MCPP_FEATURE_CORE
      true,
#else
      false,
#endif
      "core" },
    { "openkal.stream",  true,
#ifdef MCPP_FEATURE_CORE
      true,
#else
      false,
#endif
      "core" },
    { "openkal.memory",  true,
#ifdef MCPP_FEATURE_CORE
      true,
#else
      false,
#endif
      "core" },
    { "openkal.env",     false,
#ifdef MCPP_FEATURE_ENV
      true,
#else
      false,
#endif
      "env" },
    { "openkal.time",    false,
#ifdef MCPP_FEATURE_TIME
      true,
#else
      false,
#endif
      "time" },
    { "openkal.fs",      false,
#ifdef MCPP_FEATURE_FS
      true,
#else
      false,
#endif
      "fs" },
    { "openkal.process", false,
#ifdef MCPP_FEATURE_PROCESS
      true,
#else
      false,
#endif
      "process" },
    { "openkal.task",    false,
#ifdef MCPP_FEATURE_TASK
      true,
#else
      false,
#endif
      "task" },
};

// Which kinds of examination this build performs. Behaviour is unconditional:
// a run that examined no behaviour would be a run that answered nothing.
inline constexpr bool performs(kind k) {
    switch (k) {
        case kind::behaviour: return true;
        case kind::abi:
#ifdef MCPP_FEATURE_ABI
            return true;
#else
            return false;
#endif
        case kind::stability:
#ifdef MCPP_FEATURE_STABILITY
            return true;
#else
            return false;
#endif
        case kind::cost:
#ifdef MCPP_FEATURE_COST
            return true;
#else
            return false;
#endif
    }
    return false;
}

// How many times a stability section repeats an operation. Large enough that a
// handle scheme which never reuses a slot exhausts, and small enough that the
// suite remains a thing one runs rather than schedules.
inline constexpr int repetitions = 20000;

// How many times a cost section repeats an operation before dividing.
inline constexpr int cost_iterations = 2000;

}  // namespace okc
