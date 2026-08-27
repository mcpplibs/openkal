module okc.space;

import openkal.types;
import openkal.process;
import openkal.space;
import okc.report;
import okc.spec;

namespace okc::space {

#ifdef MCPP_FEATURE_SPACE
namespace {

// What the started context does. It is deliberately the smallest thing a
// context can do: the observation is that a context ran in the cloned space,
// not that it computed anything. A larger entry would make a failure ambiguous
// between "the clone did not work" and "the entry was wrong".
void entry(void* arg) {
    if (arg != nullptr) *static_cast<volatile int*>(arg) = 1;
    kal_exit(0);
}

// A stack for the started context. Its top is passed, because that is the end
// execution begins from on every architecture this specification targets.
alignas(16) unsigned char child_stack[64 * 1024];

}  // namespace
#endif

void run() {
    heading("openkal.space");
#ifndef MCPP_FEATURE_SPACE
    unobserved(kind::behaviour, "openkal.space", "the interface was not selected");
    return;
#else
    claim("kal_space_props", kal_space_props);

    // THE PROPERTY WORD IS OBSERVED BEFORE THE OPERATIONS, because what it says
    // determines what a caller above this interface may do. An environment whose
    // clone does not carry handles cannot support a library reaching for POSIX
    // fork, and stating that is the whole purpose of the position.
    {
        const bool handles  = (kal_space_props & KAL_SPACE_PROP_CLONE_HANDLES) != 0;
        const bool deferred = (kal_space_props & KAL_SPACE_PROP_DEFERRED_COPY) != 0;
        line(handles  ? "  a clone carries the handles the original holds"
                      : "  a clone carries memory only, not handles");
        line(deferred ? "  the copy may be completed lazily"
                      : "  the copy is complete when the clone reports success");
    }

    kal_space sp{};
    const int crc = kal_space_clone(&sp);
    if (crc != kal_ok) {
        unobserved(kind::behaviour, "openkal.space",
                   "the address space could not be cloned in this environment");
        return;
    }
    observe(kind::behaviour, true, "the calling address space is cloned");

    // A CONTEXT STARTED IN THE CLONE IS A PROCESS, which is why kal_process_wait
    // applies to it unchanged rather than being restated in this interface. That
    // it is waitable is the observation; what it computed is not, because the
    // clone is a separate space and the caller cannot see its stores.
    {
        volatile int flag = 0;
        kal_process p{};
        const int src = kal_space_start(sp, &entry, (void*)&flag,
                                        child_stack + sizeof child_stack, &p);
        observe(kind::behaviour, src == kal_ok,
                "a context starts in the cloned space");
        if (src == kal_ok) {
            int status = 0, terminated = 0;
            const int wrc = kal_process_wait(p, &status, &terminated);
            observe(kind::behaviour, wrc == kal_ok,
                    "the started context is waited for as a process");
            observe(kind::behaviour, wrc != kal_ok || terminated == 0,
                    "the started context ended of its own accord");
            kal_process_close(p);
        }

        // THE STORE IS NOT VISIBLE HERE, AND THAT IS THE POINT. A clone is a
        // separate address space; a caller observing its own flag set would be
        // observing a shared space, which is openkal.task and not this
        // interface.
        observe(kind::behaviour, flag == 0,
                "a store in the cloned space is not observed in the original");
    }

    kal_space_destroy(sp);

    // The two handles are independent and either may be closed first. Destroying
    // a space whose context has already ended is the ordinary order and is
    // observed by the absence of a failure in the run that follows.
    {
        kal_space second{};
        const int rc = kal_space_clone(&second);
        if (rc == kal_ok) {
            kal_space_destroy(second);
            observe(kind::behaviour, true,
                    "a cloned space with no context started in it is released");
        } else {
            unobserved(kind::behaviour, "a space with no context is released",
                       "a second clone could not be made");
        }
    }
#endif
}

}
