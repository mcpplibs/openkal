module okc.exec;

import openkal.types;
import openkal.exec;
import okc.report;
import okc.spec;

namespace okc::exec {
namespace {

// A function that returns 42, in the instructions of the architectures this
// suite is built for. There is no portable way to obtain machine code from
// within a program, and there is no way to observe that a region is executable
// except by executing something in it, so the bytes are written out here.
//
// The value is checked rather than merely the fact of returning, because a
// region that was published without its contents reaching memory would still
// transfer control and would return whatever the register happened to hold.
#if defined(__x86_64__)
const unsigned char kReturns42[] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3 };
#elif defined(__aarch64__)
const unsigned char kReturns42[] = { 0x40, 0x05, 0x80, 0x52,   // mov w0, #42
                                     0xC0, 0x03, 0x5F, 0xD6 }; // ret
#elif defined(__riscv) && __riscv_xlen == 64
const unsigned char kReturns42[] = { 0x13, 0x05, 0xA0, 0x02,   // li a0, 42
                                     0x67, 0x80, 0x00, 0x00 }; // ret
#else
const unsigned char* const kReturns42 = nullptr;
const unsigned long kReturns42Size = 0;
#endif

#if defined(__x86_64__) || defined(__aarch64__) || (defined(__riscv) && __riscv_xlen == 64)
constexpr unsigned long kReturns42Size = sizeof(kReturns42);
#endif

using fn = int (*)(void);

}  // namespace

void run() {
    heading("openkal.exec");
#ifndef MCPP_FEATURE_EXEC
    unobserved(kind::behaviour, "openkal.exec", "the exec interface was not selected");
    return;
#else
    if (kReturns42Size == 0) {
        unobserved(kind::behaviour, "openkal.exec",
                   "this architecture has no instructions written into this suite");
        return;
    }

    claim("kal_exec_props", kal_exec_props);
    observe(kind::abi, (kal_exec_props & ~kal::exec::republish.bits) == 0,
            "no position the specification has not assigned is reported");

    void* p = kal_exec_alloc(kReturns42Size);
    observe(kind::behaviour, p != nullptr, "a region is reserved");
    if (p == nullptr) return;

    // Writing before publishing is the order the interface states. A region
    // that refused this write would be one reserved executable rather than
    // writable, which the interface does not permit.
    auto* b = static_cast<unsigned char*>(p);
    for (unsigned long i = 0; i < kReturns42Size; ++i) b[i] = kReturns42[i];
    bool wrote_back = true;
    for (unsigned long i = 0; i < kReturns42Size; ++i)
        if (b[i] != kReturns42[i]) wrote_back = false;
    observe(kind::behaviour, wrote_back, "the region is writable before it is published");

    const int rc = kal_exec_publish(p, kReturns42Size);
    observe(kind::behaviour, rc == kal_ok, "the region is published");
    if (rc != kal_ok) { kal_exec_free(p, kReturns42Size); return; }

    // On an architecture with a separate instruction cache, bytes written
    // through the data path are not yet visible to the fetch path. The
    // specification does not place this upon an implementation, because the
    // program is the party that knows which bytes it wrote.
    __builtin___clear_cache(static_cast<char*>(p),
                            static_cast<char*>(p) + kReturns42Size);

    fn f = nullptr;
    __builtin_memcpy(&f, &p, sizeof f);
    observe(kind::behaviour, f() == 42,
            "instructions written into the region and published are executed");

    kal_exec_free(p, kReturns42Size);

    // The property, not the operation. A caller that must change published
    // bytes acts differently depending on it, so it is read rather than assumed.
    if (kal::exec::has(kal::exec::republish)) {
        void* q = kal_exec_alloc(kReturns42Size);
        if (q != nullptr) {
            auto* c = static_cast<unsigned char*>(q);
            for (unsigned long i = 0; i < kReturns42Size; ++i) c[i] = kReturns42[i];
            const bool first = kal_exec_publish(q, kReturns42Size) == kal_ok;
            const bool again = kal_exec_publish(q, kReturns42Size) == kal_ok;
            observe(kind::behaviour, first && again,
                    "a region reported as republishable is published twice");
            kal_exec_free(q, kReturns42Size);
        }
    } else {
        unobserved(kind::behaviour, "openkal.exec",
                   "the implementation does not report republishing");
    }
#endif
}

}  // namespace okc::exec
