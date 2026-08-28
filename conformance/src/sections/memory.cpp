module okc.memory;

import openkal.types;
import openkal.memory;
import openkal.time;
import okc.report;
import okc.spec;

namespace okc::memory {
namespace {

bool aligned(const void* p, kal_uintptr a) {
    return (reinterpret_cast<kal_uintptr>(p) & (a - 1)) == 0;
}

// Writes a pattern across the whole extent and reads it back. An allocator that
// returned a region shorter than it promised would satisfy every other
// observation and fail this one, and it would fail it at the end rather than at
// the start, which is why the whole extent is written rather than the first
// word.
bool writable_throughout(unsigned char* p, kal_uintptr n) {
    for (kal_uintptr i = 0; i < n; ++i) p[i] = static_cast<unsigned char>(i * 31u + 7u);
    for (kal_uintptr i = 0; i < n; ++i)
        if (p[i] != static_cast<unsigned char>(i * 31u + 7u)) return false;
    return true;
}

}  // namespace

void run() {
    heading("openkal.memory");
#ifndef MCPP_FEATURE_CORE
    unobserved(kind::behaviour, "openkal.memory", "the core set was not selected");
    return;
#else
    // The quantum this environment allocates and protects memory in.
    //
    // ⭐ AN OPERATION AND NOT A CONSTANT. A C library above this reports it as
    // its own page size, and one that fixed it when it was built is wrong on
    // every machine whose quantum differs from the one it was built for --- which
    // is what a distributed binary meets.
    {
        const kal_uintptr g = kal_memory_granularity();
        claim("kal_memory_granularity", g);
        observe(kind::behaviour, g >= 1, "the granularity is at least one byte");
        observe(kind::behaviour, (g & (g - 1)) == 0,
                "the granularity is a power of two");

        // ⚠️ THE VALUE IS THE ONE THE INTERFACE'S OWN OPERATIONS ACCEPT, which
        // is the whole of what it promises: an address and a length that are
        // multiples of it are acceptable. An implementation reporting a
        // quantum its allocator then refused would be reporting a fact about
        // the machine rather than about this interface.
        void* p = kal_alloc(g, g);
        observe(kind::behaviour, p != nullptr && aligned(p, g),
                "a region of the granularity, aligned to it, is obtained");
        if (p) kal_free(p, g, g);
    }

    {
        auto* p = static_cast<unsigned char*>(kal_alloc(1024, 16));
        observe(kind::behaviour, p != nullptr, "a region is obtained");
        if (p) {
            observe(kind::behaviour, writable_throughout(p, 1024),
                    "the whole of the region is writable and reads back");
            kal_free(p, 1024, 16);
        }
    }

    // The alignment is part of the request, and an environment that could not
    // honour it would have to report failure rather than return a region that
    // is nearly right.
    const kal_uintptr alignments[] = { 16, 64, 256, 4096 };
    for (kal_uintptr a : alignments) {
        void* p = kal_alloc(a * 2, a);
        observe(kind::behaviour, p != nullptr && aligned(p, a),
                a == 16   ? "a region aligned to sixteen bytes"
              : a == 64   ? "a region aligned to sixty-four bytes"
              : a == 256  ? "a region aligned to two hundred and fifty-six bytes"
                          : "a region aligned to a page");
        if (p) kal_free(p, a * 2, a);
    }

    // Two live regions do not overlap. An allocator that returned the same
    // region twice would pass every observation that examined one region.
    {
        auto* a = static_cast<unsigned char*>(kal_alloc(512, 16));
        auto* b = static_cast<unsigned char*>(kal_alloc(512, 16));
        bool distinct = a && b;
        if (distinct) {
            for (int i = 0; i < 512; ++i) { a[i] = 0xA5; b[i] = 0x5A; }
            for (int i = 0; i < 512 && distinct; ++i)
                if (a[i] != 0xA5 || b[i] != 0x5A) distinct = false;
        }
        observe(kind::behaviour, distinct, "two live regions do not overlap");
        kal_free(a, 512, 16);
        kal_free(b, 512, 16);
    }

    if (performs(kind::abi)) {
        // The size and alignment are carried by the interface so that an
        // implementation need keep no record of its own. An implementation that
        // ignored them and kept a header would still be conforming; one that
        // required them to be wrong would not.
        void* p = kal_alloc(4096, 4096);
        observe(kind::abi, p != nullptr && aligned(p, 4096),
                "the alignment the caller stated is the alignment obtained");
        kal_free(p, 4096, 4096);
    }

    if (performs(kind::stability)) {
        // Exhaustion is a defined outcome and a leak is not. An allocator that
        // never reused a region would satisfy every observation above and would
        // stop here.
        bool all = true;
        for (int i = 0; i < repetitions && all; ++i) {
            const kal_uintptr n = 16u + static_cast<kal_uintptr>(i % 512) * 8u;
            void* p = kal_alloc(n, 16);
            if (!p) { all = false; break; }
            static_cast<unsigned char*>(p)[n - 1] = 1;
            kal_free(p, n, 16);
        }
        observe(kind::stability, all, "a region obtained and released many times is obtainable again");

        void* big = kal_alloc(1u << 20, 4096);
        observe(kind::stability, big != nullptr,
                "a large region is still obtainable after many small ones");
        kal_free(big, 1u << 20, 4096);
    }

    if (performs(kind::cost)) {
        const kal_duration t0 = kal_time_monotonic();
        for (int i = 0; i < cost_iterations; ++i) {
            void* p = kal_alloc(64, 16);
            kal_free(p, 64, 16);
        }
        measure("obtaining and releasing sixty-four bytes",
                kal_time_monotonic() - t0, cost_iterations);
    }
#endif
}

}  // namespace okc::memory
