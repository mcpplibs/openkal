module okc.version;

import openkal.types;
import openkal.version;
import openkal.env;
import okc.report;
import okc.spec;

namespace okc::version {

void run() {
    heading("openkal --- what the implementation says about itself");

    // NOT CONDITIONAL ON A FEATURE, AND THAT IS THE POINT. These two are not
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

    // THE WORD AND THE LINKER AGREE. This is the observation the operation
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
#ifdef MCPP_FEATURE_MAP
    if (!kal::provides(kal::iface::map)) agrees = false;
#endif
#ifdef MCPP_FEATURE_REGION
    if (!kal::provides(kal::iface::region)) agrees = false;
#endif
#ifdef MCPP_FEATURE_PROF
    if (!kal::provides(kal::iface::prof)) agrees = false;
#endif
#ifdef MCPP_FEATURE_DEBUG
    if (!kal::provides(kal::iface::debug)) agrees = false;
#endif
    observe(kind::behaviour, agrees,
            "every interface this suite linked against is reported as present");

    // THE IMPLEMENTATION'S MANIFEST, kal_interfaces() AND ITS EXPORTED NAMES
    // STATE ONE SET (0.25). A build system checks a program's requirements
    // against the manifest's `provides-interfaces', and a loader checks an
    // image's against kal_interfaces(); the two must therefore agree, or a
    // program accepted by one is refused by the other. The runner reads the
    // manifest and names its list here (tools/run-conformance.sh); the third
    // statement, the exported names, is compared with the manifest by the same
    // script, which can read the program's symbols and this program cannot.
#ifdef MCPP_FEATURE_ENV
    {
        static const char kVar[] = "OPENKAL_CONFORMANCE_PROVIDES";
        char list[1024];
        const kal_intptr n = kal_env_var(kVar, sizeof kVar - 1, list, sizeof list);
        if (n > 0 && static_cast<kal_uintptr>(n) < sizeof list) {
            struct named { const char* name; kal_u64 bit; };
            static const named kNames[] = {
                {"openkal.abort", kal::iface::abort_.bits},     {"openkal.stream", kal::iface::stream.bits},
                {"openkal.memory", kal::iface::memory.bits},   {"openkal.env", kal::iface::env.bits},
                {"openkal.time", kal::iface::time.bits},       {"openkal.random", kal::iface::random.bits},
                {"openkal.fs", kal::iface::fs.bits},           {"openkal.process", kal::iface::process.bits},
                {"openkal.task", kal::iface::task.bits},       {"openkal.exec", kal::iface::exec.bits},
                {"openkal.terminal", kal::iface::terminal.bits}, {"openkal.net", kal::iface::net.bits},
                {"openkal.datagram", kal::iface::datagram.bits}, {"openkal.space", kal::iface::space.bits},
                {"openkal.timeout", kal::iface::timeout.bits}, {"openkal.map", kal::iface::map.bits},
                {"openkal.region", kal::iface::region.bits},   {"openkal.prof", kal::iface::prof.bits},
                {"openkal.debug", kal::iface::debug.bits},
            };
            kal_u64 stated = 0;
            bool known = true;
            kal_uintptr i = 0;
            while (i < static_cast<kal_uintptr>(n)) {
                while (i < static_cast<kal_uintptr>(n) && (list[i] == ';' || list[i] == ' ')) ++i;
                kal_uintptr b = i;
                while (i < static_cast<kal_uintptr>(n) && list[i] != ';' && list[i] != ' ') ++i;
                if (i == b) break;
                bool found = false;
                for (const named& e : kNames) {
                    kal_uintptr k = 0;
                    while (e.name[k] && b + k < i && e.name[k] == list[b + k]) ++k;
                    if (e.name[k] == 0 && b + k == i) { stated |= e.bit; found = true; break; }
                }
                if (!found) known = false;
            }
            observe(kind::behaviour, known,
                    "every interface the manifest states is one the specification defines");
            observe(kind::behaviour, stated == present,
                    "kal_interfaces() states the interfaces the manifest states");
        } else {
            unobserved(kind::behaviour, "kal_interfaces() states the interfaces the manifest states",
                       "the runner named no manifest list (OPENKAL_CONFORMANCE_PROVIDES)");
        }
    }
#endif

    if (performs(kind::abi)) {
        observe(kind::abi, kal_version() == v && kal_interfaces() == present,
                "the self-description does not change between calls");
    }
}

}  // namespace okc::version
