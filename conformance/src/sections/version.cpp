module okc.version;

import openkal.types;
import openkal.version;
import okc.report;
import okc.spec;

namespace okc::version {

void run() {
    heading("openkal --- what the implementation says about itself");

    // ⚠️ NOT CONDITIONAL ON A FEATURE, AND THAT IS THE POINT. These two are not
    // an interface: they provide no resource, and clause 3.2's closure is of the
    // set of core INTERFACES. Every conforming implementation exports them,
    // including one of a machine with no operating system, so this section is
    // the only one in the suite with no arrangement in which it is skipped.
    const kal_u64 v = kal_version();
    claim("kal_version", static_cast<kal_uintptr>(v));
    claim("kal_interfaces", static_cast<kal_uintptr>(kal_interfaces()));

    observe(kind::behaviour, v != 0,
            "the implementation states the version it was written against");

    // A consumer refuses to proceed against an implementation older than the
    // declarations it holds, because an older one reports conditions the
    // consumer distinguishes as conditions it does not --- a wrong answer rather
    // than a refusal, and a refusal is what a caller can act upon.
    observe(kind::behaviour, v >= kal::header_version,
            "the implementation is at least as new as these declarations");

    const kal_u64 present = kal_interfaces();

    // The three core interfaces are provided by every implementation, so an
    // implementation that did not claim them is one whose word is not the word
    // this position means.
    const auto core = kal::iface::abort_ | kal::iface::stream | kal::iface::memory;
    observe(kind::behaviour, kal::provides(core),
            "the core interfaces are reported as present");

    // ⭐ THE WORD AND THE LINKER AGREE. This is the observation the operation
    // exists for: a consumer that is linked learns an interface's absence from
    // the linker, and one bound at load has only this word --- so the two must
    // say the same thing, and an implementation whose word disagreed with what
    // it exports would mislead exactly the consumer that has no other way to
    // ask. Each feature below is defined when this suite was built against an
    // implementation that provides the interface.
    // Only one direction is an error. A suite built without a feature says
    // nothing about whether the implementation provides the interface, so a
    // position set where the feature is absent is not a disagreement; a
    // position CLEAR where the suite linked against the interface is.
    bool agrees = true;
#ifdef MCPP_FEATURE_ENV
    if (!kal::provides(kal::iface::env)) agrees = false;
#endif
#ifdef MCPP_FEATURE_FS
    if (!kal::provides(kal::iface::fs)) agrees = false;
#endif
#ifdef MCPP_FEATURE_NET
    if (!kal::provides(kal::iface::net)) agrees = false;
#endif
#ifdef MCPP_FEATURE_SPACE
    if (!kal::provides(kal::iface::space)) agrees = false;
#endif
#ifdef MCPP_FEATURE_TASK
    if (!kal::provides(kal::iface::task)) agrees = false;
#endif
#ifdef MCPP_FEATURE_PROCESS
    if (!kal::provides(kal::iface::process)) agrees = false;
#endif
    observe(kind::behaviour, agrees,
            "every interface this suite linked against is reported as present");

    if (performs(kind::abi)) {
        observe(kind::abi, kal_version() == v && kal_interfaces() == present,
                "the self-description does not change between calls");
    }
}

}  // namespace okc::version
