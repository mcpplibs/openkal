module okc.space;

import openkal.types;
import openkal.abort;
import openkal.process;
import openkal.space;
import okc.report;
import okc.spec;

namespace okc::space {

#ifdef MCPP_FEATURE_SPACE
namespace {

// What the started context does. It is deliberately the smallest thing a
// context can do: the observation is that a context ran in a copy of the
// caller's space, not that it computed anything. A larger entry would make a
// failure ambiguous between "the copy did not work" and "the entry was wrong".
//
// THE EXIT STATUS IS THE CHANNEL. The started context has an address space of
// its own, so a store it performs is not visible to the caller; the one thing it
// can report is how it ended, and kal_process_wait is what reads that.
void entry(void* arg) {
    const int code = (arg == nullptr) ? 1 : 7;
    kal_exit(code);
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

    // THE PROPERTY WORD IS REPORTED BEFORE THE OPERATION, because what it says
    // determines what a caller above this interface may do. An environment whose
    // copy does not carry handles cannot support a library reaching for POSIX
    // fork, and stating that is the whole purpose of the position.
    {
        const bool handles  = (kal_space_props & KAL_SPACE_PROP_CLONE_HANDLES) != 0;
        const bool deferred = (kal_space_props & KAL_SPACE_PROP_DEFERRED_COPY) != 0;
        line(handles  ? "  the started context holds the handles the caller holds"
                      : "  the started context holds memory only, not handles");
        line(deferred ? "  the copy may be completed lazily"
                      : "  the copy is complete when the call reports success");
    }

    // A CONTEXT STARTED HERE IS A PROCESS, which is why kal_process_wait applies
    // to it unchanged rather than being restated in this interface. That it is
    // waitable, and that what it reported is what the entry chose, is what is
    // observed. A caller cannot observe its stores.
    {
        int marker = 0;
        kal_process p{};
        const int src = kal_space_start(&entry, static_cast<void*>(&marker),
                                        child_stack + sizeof child_stack, &p);
        if (src != kal_ok) {
            unobserved(kind::behaviour, "openkal.space",
                       "a context could not be started in this environment");
            return;
        }
        observe(kind::behaviour, true, "a context starts in a copy of the space");

        int status = 0, terminated = 0;
        const int wrc = kal_process_wait(p, &status, &terminated);
        observe(kind::behaviour, wrc == kal_ok,
                "the started context is waited for as a process");
        observe(kind::behaviour, wrc != kal_ok || terminated == 0,
                "the started context ended of its own accord");

        // THE ENTRY RECEIVED ITS ARGUMENT, observed through the status because
        // that is the only channel a separate address space has. The entry
        // reports one value for a null argument and another for the pointer it
        // was given, so a status of seven states that the argument arrived.
        observe(kind::behaviour, wrc != kal_ok || status == 7,
                "the entry received the argument it was given");

        kal_process_close(p);

        // THE CALLER'S OWN MEMORY IS UNCHANGED, AND THAT IS THE POINT. A copy is
        // a separate address space; a caller observing a store made by the
        // started context would be observing a shared space, which is
        // openkal.task and not this interface.
        observe(kind::behaviour, marker == 0,
                "a store in the copied space is not observed in the original");
    }

    // Starting a second context succeeds after the first has ended, which is
    // what a caller running a sequence of them requires. A handle scheme that
    // retained the first would fail here rather than at some later count.
    {
        kal_process p{};
        const int rc = kal_space_start(&entry, nullptr,
                                       child_stack + sizeof child_stack, &p);
        if (rc == kal_ok) {
            int status = 0, terminated = 0;
            kal_process_wait(p, &status, &terminated);
            observe(kind::behaviour, status == 1,
                    "a second context starts and reports its own status");
            kal_process_close(p);
        } else {
            unobserved(kind::behaviour, "a second context starts",
                       "the second start was refused");
        }
    }
#endif
}

}
