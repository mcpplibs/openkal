module okc.exec;

import openkal.types;
import openkal.exec;
import openkal.memory;
import openkal.macros;
import openkal.stream;
import openkal.prof;
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

fn function_at(void* p) {
    fn f = nullptr;
    auto* d = reinterpret_cast<unsigned char*>(&f);
    const auto* b = reinterpret_cast<const unsigned char*>(&p);
    for (unsigned long i = 0; i < sizeof f; ++i) d[i] = b[i];
    return f;
}

#if defined(MCPP_FEATURE_PROF)
// TOUCHES (0.23): the order in which the program first executes the pages of a
// part, recorded by the environment while the program runs as it would.
void touches() {
    const kal_uintptr g = kal_exec_granularity();
    if ((kal_prof_props() & kal_uintptr{2}) == 0) {
        observe(kind::behaviour, kal_prof_touch_begin(nullptr, g) == kal_err_not_supported,
                "touches are refused where the implementation does not state them");
        return;
    }
    if (g == 0 || !(kal_exec_props() & kal::exec::available.bits)) {
        unobserved(kind::behaviour, "the first execution of each page is recorded, in order",
                   "executable memory is not available");
        return;
    }
    auto* r = static_cast<unsigned char*>(kal_exec_alloc(3 * g));
    if (r == nullptr) return;
    for (kal_uintptr page = 0; page < 3; ++page)
        for (unsigned long i = 0; i < kReturns42Size; ++i) r[page * g + i] = kReturns42[i];
    observe(kind::behaviour, kal_prof_touch_begin(r, 3 * g) == kal_err_invalid,
            "a part that is not executable is refused");
    if (kal_exec_publish(r, 3 * g) != kal_ok) { kal_exec_free(r, 3 * g); return; }
#if defined(__GNUC__) || defined(__clang__)
    __builtin___clear_cache(reinterpret_cast<char*>(r), reinterpret_cast<char*>(r) + 3 * g);
#endif
    observe(kind::behaviour, kal_prof_touch_begin(r, 0) == kal_err_invalid
                             && kal_prof_touch_begin(r, g / 2 == 0 ? 1 : g / 2) == kal_err_invalid,
            "a size of zero, or not of whole pages, is refused");
    observe(kind::behaviour, kal_prof_touch_begin(r, 3 * g) == kal_ok, "touches are recorded in a part");
    const int a = function_at(r + 2 * g)(), b = function_at(r)(), c = function_at(r + 2 * g)();
    observe(kind::behaviour, a == 42 && b == 42 && c == 42, "the program runs as it would");
    kal_uintptr got[8] = {};
    const kal_intptr n = kal_prof_touch_take(got, 8);
    observe(kind::behaviour,
            n == 2 && got[0] == reinterpret_cast<kal_uintptr>(r + 2 * g) && got[1] == reinterpret_cast<kal_uintptr>(r),
            "the first execution of each page is recorded once, in order");
    observe(kind::behaviour, kal_prof_touch_end() == kal_ok && function_at(r + g)() == 42
                             && kal_prof_touch_take(got, 8) == 0,
            "recording ends, and the part is as it was");
    kal_exec_free(r, 3 * g);
}
#endif

#if defined(MCPP_FEATURE_FS)
fn as_function(void* p) {
    fn f = nullptr;
    auto* d = reinterpret_cast<unsigned char*>(&f);
    const auto* b = reinterpret_cast<const unsigned char*>(&p);
    for (unsigned long i = 0; i < sizeof f; ++i) d[i] = b[i];
    return f;
}

// INSTRUCTIONS FROM A FILE (version 0.21). The file is two alignments long: the
// first holds a pattern, the second begins with the function and continues
// with the pattern.
constexpr char kFile[] = "okc-conformance-exec.tmp";
constexpr kal_uintptr kAlign = static_cast<kal_uintptr>(kal::macros::KAL_MAP_ALIGNMENT_M);

unsigned char pattern(kal_uintptr i) { return static_cast<unsigned char>((i * 13 + 5) % 251 + 1); }

bool make_file() {
    kal_file f{};
    const auto flags = kal::fs::open::write | kal::fs::open::create | kal::fs::open::truncate;
    if (kal::fs::open_file(kal::fs::working(), kFile, sizeof kFile - 1, flags, &f) != kal_ok) return false;
    unsigned char block[4096];
    bool ok = true;
    for (kal_uintptr at = 0; ok && at < 2 * kAlign; at += sizeof block) {
        for (kal_uintptr i = 0; i < sizeof block; ++i) block[i] = pattern(at + i);
        if (at == kAlign)
            for (unsigned long i = 0; i < kReturns42Size; ++i) block[i] = kReturns42[i];
        ok = kal_stream_write(kal_fs_stream(f), block, sizeof block) == static_cast<kal_intptr>(sizeof block);
    }
    kal_fs_close_file(f);
    return ok;
}

// A region at the address asked for (0.23): an address just released is given
// again, and one a region holds is refused with nothing reserved; where the
// position is zero, every request is refused. An address that is not a multiple
// of the reservation quantum is refused either way.
void at_an_address() {
    const kal_uintptr q = kal_memory_granularity();
    const kal_uintptr size = 4 * q;
    void* r = kal_exec_alloc(size);
    if (r == nullptr) {
        unobserved(kind::behaviour, "a region is reserved at the address asked for",
                   "no region could be reserved to find a free address");
        return;
    }
    auto* bytes = static_cast<unsigned char*>(r);
    observe(kind::behaviour, kal_exec_alloc_at(bytes + 1, size) == nullptr,
            "an address that is not a multiple of the reservation quantum is refused");
    if (!kal::exec::has(kal::exec::place)) {
        kal_exec_free(r, size);
        observe(kind::behaviour, kal_exec_alloc_at(r, size) == nullptr,
                "a region at an address is refused where the implementation does not claim it");
        return;
    }
    observe(kind::behaviour, kal_exec_alloc_at(r, size) == nullptr,
            "an address a region holds is refused");
    bytes[0] = 5;
    observe(kind::behaviour, bytes[0] == 5, "and the region there is as it was");
    kal_exec_free(r, size);
    void* again = kal_exec_alloc_at(r, size);
    observe(kind::behaviour, again == r, "a region is reserved at an address just released");
    if (again == r) {
        auto* b = static_cast<unsigned char*>(again);
        b[size - 1] = 6;
        observe(kind::behaviour, b[size - 1] == 6, "and is written as a region kal_exec_alloc returned");
        kal_exec_free(again, size);
    }
}

// A REGION IS ZERO WHEN IT IS RETURNED, AND A PART OF IT IS SEALED READ-ONLY
// (0.23): what it held stays readable; sealed for ever, it is not published or
// sealed again.
void zero_and_sealed() {
    const kal_uintptr g = kal_exec_granularity();
    const kal_uintptr size = 4 * g;
    void* r = kal_exec_alloc(size);
    if (r == nullptr || g == 0) {
        unobserved(kind::behaviour, "a region is zero when it is returned", "no region could be reserved");
        return;
    }
    auto* c = static_cast<unsigned char*>(r);
    bool zero = true;
    for (kal_uintptr at = 0; at < size; at += 509) if (c[at] != 0) zero = false;
    observe(kind::behaviour, zero, "a region is zero when it is returned");
    observe(kind::behaviour, kal_exec_seal_part(r, 1, g, 0) == kal_err_invalid,
            "a part to seal that is not at a multiple of the granularity is refused");
    observe(kind::behaviour, kal_exec_seal_part(r, g, g, kal_uintptr{1} << 9) == kal_err_invalid,
            "a position of the sealing flags this version does not assign is refused");
    c[g] = 11;
    c[g + g - 1] = 12;
    observe(kind::behaviour, kal_exec_seal_part(r, g, g, 0) == kal_ok && c[g] == 11 && c[g + g - 1] == 12,
            "a part is sealed read-only, and holds what it held");
    c[2 * g] = 13;
    const int forever = kal_exec_seal_part(r, 2 * g, g, kal::exec::forever.bits);
    observe(kind::behaviour, forever == kal_ok && c[2 * g] == 13, "a part is sealed for the life of the program");
    if (kal::exec::has(kal::exec::seal_forever)) {
        observe(kind::behaviour, kal_exec_publish_part(r, 2 * g, g) == kal_err_permission
                                 && kal_exec_seal_part(r, 2 * g, g, 0) == kal_err_permission,
                "a part sealed for ever is neither published nor sealed again");
        // The region keeps a part sealed for ever, and is not released.
    } else {
        unobserved(kind::behaviour, "a part sealed for ever is neither published nor sealed again",
                   "the implementation does not state KAL_EXEC_PROP_SEAL_FOREVER");
        c[3 * g] = 14;
        observe(kind::behaviour, c[3 * g] == 14, "the rest of the region is still writable");
        kal_exec_free(r, size);
    }
}

void from_a_file() {
    const kal_uintptr g = kal_exec_granularity();
    if (g == 0 || kAlign % g != 0) {
        unobserved(kind::behaviour, "part of a region is made a file's",
                   "the protection granularity does not divide KAL_MAP_ALIGNMENT");
        return;
    }
    if (!make_file()) {
        unobserved(kind::behaviour, "part of a region is made a file's", "a file could not be written");
        return;
    }
    kal_file f{};
    if (kal::fs::open_file(kal::fs::working(), kFile, sizeof kFile - 1, kal::fs::open::read, &f) != kal_ok) {
        unobserved(kind::behaviour, "part of a region is made a file's", "the file could not be opened");
        kal_fs_remove(kal::fs::working(), kFile, sizeof kFile - 1);
        return;
    }
    void* r = kal_exec_alloc(3 * kAlign);
    if (r == nullptr) {
        kal_fs_close_file(f);
        kal_fs_remove(kal::fs::working(), kFile, sizeof kFile - 1);
        return;
    }
    auto* c = static_cast<unsigned char*>(r);
    if (!kal::exec::has(kal::exec::file)) {
        c[0] = 9;
        observe(kind::behaviour,
                kal_exec_map_part(r, 0, kAlign, f, 0, 0) == kal_err_not_supported && c[0] == 9,
                "an implementation that does not report instructions from a file refuses them,"
                " and the region is unchanged");
        unobserved(kind::behaviour, "instructions in a file are executed from a part of a region",
                   "the implementation does not report KAL_EXEC_PROP_FILE");
    } else {
        // The arguments the operation refuses, each leaving the region as it was.
        c[0] = 9;
        observe(kind::behaviour, kal_exec_map_part(r, 0, kAlign, f, 1, 0) == kal_err_invalid && c[0] == 9,
                "an offset into the file that is not a multiple of KAL_MAP_ALIGNMENT is refused");
        observe(kind::behaviour, kal_exec_map_part(r, 0, 2 * kAlign, f, kAlign, 0) == kal_err_invalid && c[0] == 9,
                "a part that reaches beyond the end of the file is refused");
        if (g > 1)
            observe(kind::behaviour, kal_exec_map_part(r, 0, g / 2, f, 0, 0) == kal_err_invalid && c[0] == 9,
                    "a part that is not a multiple of the granularity is refused");
        observe(kind::behaviour, kal_exec_map_part(r, 0, kAlign, f, 0, kal_uintptr{1} << 9) == kal_err_invalid,
                "a position of `how' this version does not assign is refused");

        observe(kind::behaviour,
                kal_exec_map_part(r, 0, kAlign, f, 0, (kal::exec::read | kal::exec::prefetch | kal::exec::bti_marked).bits)
                    == kal_ok,
                "part of a region is made a file's bytes, to be read, and read in now");
        observe(kind::behaviour, kal_exec_map_part(r, kAlign, kAlign, f, kAlign, kal::exec::execute.bits) == kal_ok,
                "part of a region is made a file's bytes, to be executed");
        // WRITTEN PRIVATELY (0.23): the file's bytes until the program writes
        // them, its own copy then, and the file unchanged.
        if (void* w = kal_exec_alloc(2 * kAlign)) {
            auto* d = static_cast<unsigned char*>(w);
            observe(kind::behaviour,
                    kal_exec_map_part(w, 0, kAlign, f, 0, (kal::exec::write | kal::exec::execute).bits) == kal_err_invalid,
                    "a part both written and executed is refused");
            const bool mapped = kal_exec_map_part(w, 0, kAlign, f, 0, kal::exec::write.bits) == kal_ok;
            observe(kind::behaviour, mapped && d[5] == pattern(5), "part of a region is made a file's bytes, to be written");
            if (mapped) {
                const auto mine = static_cast<unsigned char>(pattern(5) + 1);
                d[5] = mine;
                unsigned char back = 0;
                const bool read = kal_fs_read_at(f, &back, 1, 5) == 1;
                observe(kind::behaviour, d[5] == mine && read && back == pattern(5),
                        "what the program writes there is its own, and the file is unchanged");
                observe(kind::behaviour, kal_exec_seal_part(w, 0, kAlign, 0) == kal_ok && d[5] == mine,
                        "and the part is sealed as it was written");
            }
            kal_exec_free(w, 2 * kAlign);
        }
        // The handle may go: the parts outlive it.
        kal_fs_close_file(f);
        f = kal_file{};
        bool same = true;
        for (kal_uintptr i = 0; i < kAlign; i += 997)
            if (c[i] != pattern(i)) same = false;
        observe(kind::behaviour, same, "the part read holds the file's bytes, after the file's handle is closed");
        observe(kind::behaviour, as_function(c + kAlign)() == 42,
                "instructions in a file are executed from a part of a region");
        c[2 * kAlign] = 7;
        c[2 * kAlign + 1] = static_cast<unsigned char>(c[2 * kAlign] + 1);
        observe(kind::behaviour, c[2 * kAlign + 1] == 8, "the rest of the region is still writable");
    }
    kal_exec_free(r, 3 * kAlign);
    if (f.h) kal_fs_close_file(f);
    kal_fs_remove(kal::fs::working(), kFile, sizeof kFile - 1);
}
#endif

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

    claim("kal_exec_props()", kal_exec_props());
    observe(kind::abi,
            (kal_exec_props() & ~(kal::exec::republish | kal::exec::available | kal::exec::file | kal::exec::place
                                  | kal::exec::seal_forever | kal::exec::bti).bits) == 0,
            "no position the specification has not assigned is reported");

    // WHETHER THE INTERFACE CAN BE EXERCISED BY THIS ARTIFACT IS READ BEFORE
    // IT IS EXERCISED, AND BOTH ANSWERS ARE OBSERVED.
    //
    // Clause 6.5 settles availability at dependency resolution and says it is
    // not reported at run time, because a path no artifact takes is a path
    // nothing has verified. That reasoning holds for an artifact produced for
    // its own machine and inverts for one produced once and run in many: the
    // producer decided for every environment, and the consumer resolves
    // nothing. So the position exists, and this is the observation that keeps
    // it from being the unverified path the clause warns about.
    const bool available = (kal_exec_props() & kal::exec::available.bits) != 0;

    void* p = kal_exec_alloc(kReturns42Size);
    if (!available) {
        observe(kind::behaviour, p == nullptr,
                "an implementation that does not claim availability reserves nothing");
        unobserved(kind::behaviour, "instructions written into a region are executed",
                   "this artifact was not produced so that it may execute memory,"
                   " which the capability word reports");
        return;
    }
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
    //
    // THE BUILTIN IS NOT UNIVERSAL, AND THIS SUITE IS BUILT BY THREE
    // COMPILERS. Measured on the MSVC row of openkal-windows, the first run in
    // which this section was selected at all:
    //
    //     exec.cpp(78): error C3861: '__builtin___clear_cache': identifier not
    //     found
    //
    // The guard is on the COMPILER and not on the architecture, because what
    // varies is which compiler spells the operation this way. Where it is
    // absent, the architectures that compiler targets here keep the two paths
    // coherent in hardware and there is nothing to do.
#if defined(__GNUC__) || defined(__clang__)
    __builtin___clear_cache(static_cast<char*>(p),
                            static_cast<char*>(p) + kReturns42Size);
#endif

    // A BYTE COPY RATHER THAN `__builtin_memcpy', FOR THE SAME REASON AND
    // FOUND IN THE SAME RUN. The suite reaches no C library --- it is written to
    // run against an implementation that may be the only supplier of one --- and
    // a builtin is not a C library, but it is not a language feature either.
    fn f = nullptr;
    {
        auto* d = reinterpret_cast<unsigned char*>(&f);
        const auto* b = reinterpret_cast<const unsigned char*>(&p);
        for (unsigned long i = 0; i < sizeof f; ++i) d[i] = b[i];
    }
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

    // PART OF A REGION (version 0.17). Two granules: the first published and
    // executed, the second written AFTER the first was published --- which is
    // what a loader does with an image's data beside its instructions.
    const kal_uintptr g = kal_exec_granularity();
    observe(kind::behaviour, g != 0 && (g & (g - 1)) == 0,
            "the protection granularity is a power of two");
    if (g != 0) {
        observe(kind::behaviour, kal_exec_publish_part(nullptr, 0, g) != kal_ok,
                "publishing part of no region is refused");
        void* r = kal_exec_alloc(2 * g);
        observe(kind::behaviour, r != nullptr, "a region of two granules is reserved");
        if (r != nullptr) {
            auto* c = static_cast<unsigned char*>(r);
            for (unsigned long i = 0; i < kReturns42Size; ++i) c[i] = kReturns42[i];
            observe(kind::behaviour, kal_exec_publish_part(r, 0, g / 2 == 0 ? 1 : g / 2) == kal_err_invalid,
                    "a part that is not a multiple of the granularity is refused");
            observe(kind::behaviour, kal_exec_publish_part(r, 0, g) == kal_ok,
                    "the first granule is published");
#if defined(__GNUC__) || defined(__clang__)
            __builtin___clear_cache(static_cast<char*>(r), static_cast<char*>(r) + kReturns42Size);
#endif
            c[g] = 7;
            c[g + 1] = static_cast<unsigned char>(c[g] + 1);
            observe(kind::behaviour, c[g + 1] == 8,
                    "the part not published is still writable");
            fn h = nullptr;
            {
                auto* d = reinterpret_cast<unsigned char*>(&h);
                const auto* b = reinterpret_cast<const unsigned char*>(&r);
                for (unsigned long i = 0; i < sizeof h; ++i) d[i] = b[i];
            }
            observe(kind::behaviour, h() == 42, "the published part is executed");
            kal_exec_free(r, 2 * g);
        }
    }

    claim("KAL_EXEC_PROP_FILE", kal_exec_props() & kal::exec::file.bits);
    claim("KAL_EXEC_PROP_PLACE", kal_exec_props() & kal::exec::place.bits);
    claim("KAL_EXEC_PROP_SEAL_FOREVER", kal_exec_props() & kal::exec::seal_forever.bits);
    claim("KAL_EXEC_PROP_BTI", kal_exec_props() & kal::exec::bti.bits);
    at_an_address();
    zero_and_sealed();
#if defined(MCPP_FEATURE_PROF)
    touches();
#else
    unobserved(kind::behaviour, "the first execution of each page is recorded", "the prof interface was not selected");
#endif
#if defined(MCPP_FEATURE_FS)
    from_a_file();
#else
    unobserved(kind::behaviour, "part of a region is made a file's", "the fs interface was not selected");
#endif
#endif
}

}  // namespace okc::exec
