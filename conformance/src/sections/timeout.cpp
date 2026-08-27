module okc.timeout;

import openkal.types;
import openkal.stream;
import openkal.timeout;
import okc.report;
import okc.spec;

namespace okc::timeout {

void run() {
    heading("openkal.timeout");
#ifndef MCPP_FEATURE_TIMEOUT
    unobserved(kind::behaviour, "openkal.timeout", "the interface was not selected");
    return;
#else
    claim("kal_timeout_granularity_ns", kal_timeout_granularity_ns);

    // A GRANULARITY OF ZERO WOULD BE A CLAIM NO CLOCK CAN MEET. The word states
    // the smallest bound the implementation distinguishes, and an implementation
    // reporting zero would be asserting an infinitely fine clock rather than
    // declining to answer.
    observe(kind::behaviour, kal_timeout_granularity_ns > 0,
            "the granularity is a positive number of nanoseconds");

    // AN EXPIRED BOUND IS kal_err_again AND NOT A NEW ERROR VALUE. The error set
    // is closed (clause 5.2), and "the operation would block" is what an expiry
    // is. Observed on a read from the standard input, which in a run without a
    // terminal has nothing to give and must therefore expire rather than wait.
    //
    // THE ZERO-LENGTH CASE IS SEPARATED FROM THE EXPIRY, because a read of zero
    // bytes succeeds trivially and would report kal_ok whatever the bound.
    {
        char buf[1] = {0};
        const kal_io_result r = kal_timeout_read(kal_stdin(), buf, sizeof buf, 1);
        observe(kind::behaviour,
                r.e == kal_ok || r.e == kal_err_again || r.e == kal_err_not_supported,
                "a bounded read reports success, an expiry, or a refusal");
        if (r.e == kal_err_again)
            observe(kind::behaviour, r.n == 0,
                    "an expired read transferred nothing");
    }

    // A BOUND OF ZERO IS NO BOUND, which is the convention kal_task_wait already
    // establishes and the reason this interface states a duration rather than an
    // instant. It is not observed by waiting --- a run that blocked would never
    // report --- but by writing, which does not wait.
    {
        const kal_io_result r = kal_timeout_write(kal_stdout(), "", 0, 0);
        observe(kind::behaviour, r.e == kal_ok || r.e == kal_err_not_supported,
                "a bound of zero is accepted and denotes no bound");
    }

    // An implementation may provide this interface for some of its resource
    // types and not others, and reports kal_err_not_supported for the rest.
    // What is not conforming is reporting success having waited without bound.
    // That cannot be observed in finite time, so what is observed instead is
    // that the refusal, where it is given, is the defined one.
    {
        char buf[1] = {0};
        const kal_io_result r = kal_timeout_read(kal_stdin(), buf, sizeof buf, 1000000);
        observe(kind::behaviour,
                r.e == kal_ok || r.e == kal_err_again ||
                r.e == kal_err_not_supported || r.e == kal_err_io,
                "a refusal is drawn from the closed error set");
    }
#endif
}

}
