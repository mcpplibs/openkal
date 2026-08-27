module okc.terminal;

import openkal.types;
import openkal.stream;
import openkal.terminal;
import okc.report;
import okc.spec;

namespace okc::terminal {

void run() {
    heading("openkal.terminal");
#ifndef MCPP_FEATURE_TERMINAL
    unobserved(kind::behaviour, "openkal.terminal", "the interface was not selected");
    return;
#else
    // THE RESOURCE THIS INTERFACE APPLIES TO IS NOT ALWAYS PRESENT, and the
    // suite must not report a failure when it is absent. A run under a pipe has
    // no terminal, which is the ordinary case in continuous integration, so
    // every observation below is conditioned on finding one and the condition
    // is reported rather than assumed.
    // The properties are named through the modules and not through the macros:
    // a macro does not cross a module boundary, and these sections are modules.
    const kal_stream out = kal_stdout();
    const bool interactive =
        kal::stream_props{kal_stream_props(out)}.has(kal::stream_prop::interactive);

    if (!interactive) {
        unobserved(kind::behaviour, "openkal.terminal",
                   "no standard stream is interactive in this run");
    }

    // The operations answer for a stream that is not a terminal, and the answer
    // is a refusal rather than a silent success. This is observable whether or
    // not a terminal is present, and it is the half of the contract that a run
    // under a pipe can still examine.
    {
        kal_uintptr mode = 0;
        const int rc = kal_terminal_get_mode(out, &mode);
        if (!interactive) {
            observe(kind::behaviour, rc == kal_err_not_supported,
                    "a stream that is not interactive refuses get_mode");
        } else {
            observe(kind::behaviour, rc == kal_ok,
                    "an interactive stream reports its mode");
        }
    }

    if (!interactive) return;

    // THE PAIR IS AN INVERSE, AND THAT IS WHAT IS OBSERVED HERE. A get followed
    // by a set of what was read leaves the terminal as it was found, which is
    // the property clause 7.11 requires and the reason a setter alone would not
    // suffice. The mode is restored before the section returns whatever the
    // outcome, so that a failing run does not leave the reader's terminal
    // without an echo.
    kal_uintptr original = 0;
    const int got = kal_terminal_get_mode(out, &original);
    if (got != kal_ok) {
        unobserved(kind::behaviour, "get_mode then set_mode restores",
                   "the mode could not be read");
        return;
    }

    {
        const int rc = kal_terminal_set_mode(out, original);
        observe(kind::behaviour, rc == kal_ok,
                "setting the mode that was read succeeds");

        kal_uintptr again = 0;
        const int re = kal_terminal_get_mode(out, &again);
        observe(kind::behaviour, re == kal_ok && again == original,
                "the mode read back is the mode that was set");
    }

    // A position this implementation does not distinguish reads as zero and is
    // ignored when set; neither is an error. The observation is that an
    // unassigned position does not turn a set into a failure, because a program
    // compiled against a later revision will set exactly such a position.
    {
        const kal_uintptr unassigned = (kal_uintptr)1u << 20;
        const int rc = kal_terminal_set_mode(out, original | unassigned);
        observe(kind::behaviour, rc == kal_ok,
                "an unassigned position in the mode word is not an error");
        kal_terminal_set_mode(out, original);
    }

    // The display size, where it is known. An environment that cannot ask
    // reports not_supported and leaves both outputs untouched, so the outputs
    // are pre-set to a value the operation would not produce.
    {
        kal_uintptr cols = 0xDEAD, rows = 0xBEEF;
        const int rc = kal_terminal_size(out, &cols, &rows);
        if (rc == kal_ok) {
            observe(kind::behaviour, cols > 0 && rows > 0,
                    "a reported display size is not zero in either dimension");
        } else {
            observe(kind::behaviour,
                    rc == kal_err_not_supported && cols == 0xDEAD && rows == 0xBEEF,
                    "an unknown display size leaves both outputs untouched");
        }
    }

    // Whichever positions the property word claims, the operations that
    // correspond to them must be answered. A word claiming a facility the
    // implementation refuses is the disagreement clause 6.2 exists to prevent.
    {
        const auto props = kal::terminal::properties(out);
        if (props.has(kal::terminal::has_size)) {
            kal_uintptr c = 0, r = 0;
            observe(kind::behaviour, kal_terminal_size(out, &c, &r) == kal_ok,
                    "a claimed display size is reported when asked for");
        }
        if (props.has(kal::terminal::has_mode)) {
            kal_uintptr m = 0;
            observe(kind::behaviour, kal_terminal_get_mode(out, &m) == kal_ok,
                    "a claimed mode is reported when asked for");
        }
    }

    kal_terminal_set_mode(out, original);
#endif
}

}
