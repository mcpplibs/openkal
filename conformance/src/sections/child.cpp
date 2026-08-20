module okc.child;

// Every openkal module is imported unconditionally, in this and in every other
// section. An import creates no link-time dependency of its own --- what
// creates one is a call --- so the conditionals that select what is examined
// guard the calls and never the imports. They could not guard the imports in
// any case: an import inside a conditional block is refused by the build, and
// refused for a good reason, since a module graph that depends on the
// preprocessor is a module graph that cannot be scanned.
import openkal.types;
import openkal.abort;
import openkal.env;
import openkal.stream;
import openkal.time;

namespace okc {
namespace {

bool same(const char* a, kal_uintptr alen, const char* b) {
    kal_uintptr n = 0; while (b[n]) ++n;
    if (n != alen) return false;
    for (kal_uintptr i = 0; i < n; ++i) if (a[i] != b[i]) return false;
    return true;
}

}  // namespace

const char* argument_for(errand e) {
    switch (e) {
        case errand::exit_with_33:       return "--child-exit";
        case errand::exit_after_writing: return "--child-exit-after-writing";
        case errand::abort_with_message: return "--child-abort";
        case errand::wait_to_be_terminated: return "--child-wait";
        case errand::none:               break;
    }
    return "";
}

errand child_errand() {
#ifdef MCPP_FEATURE_ENV
    for (kal_uintptr i = 1; i < kal_env_arg_count(); ++i) {
        kal_uintptr len = 0;
        const char* a = kal_env_arg(i, &len);
        if (!a) continue;
        if (same(a, len, argument_for(errand::exit_with_33)))       return errand::exit_with_33;
        if (same(a, len, argument_for(errand::exit_after_writing))) return errand::exit_after_writing;
        if (same(a, len, argument_for(errand::abort_with_message))) return errand::abort_with_message;
        if (same(a, len, argument_for(errand::wait_to_be_terminated))) return errand::wait_to_be_terminated;
    }
#endif
    return errand::none;
}

namespace {
// Something the specification requires shall NOT run after kal_exit. A static
// object whose destructor writes is the shortest thing that would betray an
// implementation calling a C library's exit instead of terminating.
struct after {
    bool armed = false;
    ~after() {
#ifdef MCPP_FEATURE_CORE
        if (armed) kal_stream_write(kal_stdout(), "DESTRUCTOR RAN\n", 15);
#endif
    }
};
after g_after;
}  // namespace

[[noreturn]] void perform(errand e) {
    switch (e) {
        case errand::exit_with_33:
            // Clause 7.6: the first element of the vector is the name the
            // started program observes as its own, passed unaltered. The copy
            // reports which one it saw through its status, because that is the
            // only channel openkal.process defines --- and reporting it is what
            // makes the parent's observation an observation rather than an
            // assumption.
#ifdef MCPP_FEATURE_ENV
            {
                kal_uintptr len = 0;
                const char* self = kal_env_arg(0, &len);
                if (!self || !same(self, len, "openkal-conformance-child")) kal_exit(36);
            }
#endif
            kal_exit(33);
        case errand::exit_after_writing:
            // The status is 34, and the parent additionally requires that this
            // program's output contain the written bytes and not the
            // destructor's: an implementation that terminated by way of a C
            // library's exit would run the destructor, and clause 7.8 says it
            // shall not.
            g_after.armed = true;
#ifdef MCPP_FEATURE_CORE
            kal_stream_write(kal_stdout(), "BEFORE EXIT\n", 12);
#endif
            kal_exit(34);
        case errand::abort_with_message:
            kal_abort("openkal-conformance: the message kal_abort was given\n", 52);
        case errand::wait_to_be_terminated:
#ifdef MCPP_FEATURE_TIME
            for (;;) kal_time_sleep(5u * 1000u * 1000u);
#else
            for (;;) { }
#endif
        case errand::none:
            break;
    }
    kal_exit(35);
}

}  // namespace okc
