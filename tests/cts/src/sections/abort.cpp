module okc.abort;

import openkal.types;
import openkal.abort;
import openkal.process;
import openkal.fs;
import openkal.env;
import okc.report;
import okc.spec;
import okc.child;

// Termination cannot be observed by the program that performs it, so it is
// observed in a started copy. That requires three interfaces beyond the one
// under examination --- one to start the copy, one to name the program, and one
// to tell the copy what it is --- and an arrangement that selected none of them
// reports that rather than reporting nothing.
namespace okc::termination {

void run() {
    heading("openkal.abort");
#ifndef MCPP_FEATURE_CORE
    unobserved(kind::behaviour, "openkal.abort", "the core set was not selected");
    return;
#elif !defined(MCPP_FEATURE_PROCESS) || !defined(MCPP_FEATURE_ENV) || !defined(MCPP_FEATURE_FS)
    unobserved(kind::behaviour, "kal_exit terminates with the status it was given",
               "termination ends the observing program, so it is observed in a started copy; "
               "that needs openkal.process to start one, openkal.fs to name it, and "
               "openkal.env to tell it what to do");
    unobserved(kind::behaviour, "kal_exit runs no static destructor",
               "the same");
    unobserved(kind::behaviour, "kal_abort does not return",
               "the same");
#else
    int status = 0, terminated = 0;

    if (start_copy("openkal-conformance-child", argument_for(errand::exit_with_33), status, terminated)) {
        observe(kind::behaviour, terminated == 0 && status == 33,
                "kal_exit terminates with the status it was given");
    } else {
        unobserved(kind::behaviour, "kal_exit terminates with the status it was given",
                   "a copy of this program could not be started; openkal.process reported a failure "
                   "or the program's own name did not resolve against a supplied directory");
    }

    // Clause 7.8: registered exit handlers and static destructors shall not
    // run. The copy arms a destructor that would announce itself, and the
    // observation is that the copy still reported the status: an
    // implementation that ended through a C library's exit would run the
    // destructor first, and the destructor writes to the copy's output.
    if (start_copy("openkal-conformance-child", argument_for(errand::exit_after_writing), status, terminated)) {
        observe(kind::behaviour, terminated == 0 && status == 34,
                "kal_exit terminates immediately, and the status survives");
        line("  (the copy's output above shows BEFORE EXIT and shall not show DESTRUCTOR RAN)");
    } else {
        unobserved(kind::behaviour, "kal_exit runs no static destructor",
                   "a copy of this program could not be started");
    }

    // kal_abort does not return, and what it does instead is the environment's
    // to decide: the specification requires only that the program stop after
    // the message. A status of 33 or 34 would mean it returned into the copy's
    // ordinary path, which is the thing being excluded.
    if (start_copy("openkal-conformance-child", argument_for(errand::abort_with_message), status, terminated)) {
        observe(kind::behaviour, terminated != 0 || (status != 33 && status != 34 && status != 0),
                "kal_abort ends the program rather than returning");
    } else {
        unobserved(kind::behaviour, "kal_abort does not return",
                   "a copy of this program could not be started");
    }
#endif
}

}  // namespace okc::termination
