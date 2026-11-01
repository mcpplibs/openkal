module okc.region;

import openkal.types;
import openkal.memory;
import openkal.region;
import openkal.time;
import okc.report;
import okc.spec;

namespace okc::region {
namespace {

bool aligned(const void* p, kal_uintptr a) {
    return (reinterpret_cast<kal_uintptr>(p) & (a - 1)) == 0;
}

bool reads_zero(const unsigned char* p, kal_uintptr n) {
    for (kal_uintptr i = 0; i < n; ++i) if (p[i] != 0) return false;
    return true;
}

void fill(unsigned char* p, kal_uintptr n, unsigned char seed) {
    for (kal_uintptr i = 0; i < n; ++i) p[i] = static_cast<unsigned char>(seed + i * 13u);
}

bool holds(const unsigned char* p, kal_uintptr n, unsigned char seed) {
    for (kal_uintptr i = 0; i < n; ++i)
        if (p[i] != static_cast<unsigned char>(seed + i * 13u)) return false;
    return true;
}

}  // namespace

void run() {
    heading("openkal.region");
#ifndef MCPP_FEATURE_REGION
    unobserved(kind::behaviour, "openkal.region", "the feature `region' was not selected");
    return;
#else
    const kal_uintptr g = kal_memory_granularity();
    const kal_uintptr props = kal_region_props();
    claim("kal_region_props", props);

    // A RANGE BACKED AT ONCE. Aligned more coarsely than the granularity, as an
    // allocator of a general kind asks for its segments.
    {
        const kal_uintptr align = g * 16;
        const kal_uintptr size = g * 8;
        auto* p = static_cast<unsigned char*>(kal_region_reserve(size, align, kal::region::commit_now));
        observe(kind::behaviour, p != nullptr, "a range backed at once is reserved");
        if (p) {
            observe(kind::behaviour, aligned(p, align),
                    "the range is at a multiple of the alignment asked for");
            observe(kind::behaviour, reads_zero(p, size),
                    "a range read for the first time reads as zero");
            fill(p, size, 7);
            observe(kind::behaviour, holds(p, size, 7),
                    "every byte of the range keeps what was written");

            // decommit returns the backing; the part reads as zero once backed again.
            observe(kind::behaviour, kal_region_decommit(p + g, 2 * g) == kal_ok,
                    "a part's backing is returned");
            observe(kind::behaviour, kal_region_commit(p + g, 2 * g) == kal_ok,
                    "and the part is backed again");
            observe(kind::behaviour, reads_zero(p + g, 2 * g),
                    "a part backed again after its backing was returned reads as zero");
            observe(kind::behaviour, holds(p, g, 7) && holds(p + 3 * g, size - 3 * g, static_cast<unsigned char>(7 + 3 * g * 13u)),
                    "the parts around it keep what was written");

            // discard leaves the part usable at once; its contents are unspecified.
            observe(kind::behaviour, kal_region_discard(p + 4 * g, 2 * g) == kal_ok,
                    "a part's contents are discarded");
            fill(p + 4 * g, 2 * g, 99);
            observe(kind::behaviour, holds(p + 4 * g, 2 * g, 99),
                    "a discarded part is written at once and keeps what was written");

            // Backing a part already backed leaves its contents.
            observe(kind::behaviour, kal_region_commit(p, g) == kal_ok && holds(p, g, 7),
                    "backing a part already backed leaves what it holds");
            kal_region_release(p, size);
        }
    }

    // A RANGE RESERVED AND BACKED PART BY PART.
    {
        const kal_uintptr size = g * 64;
        auto* p = static_cast<unsigned char*>(kal_region_reserve(size, g, 0));
        observe(kind::behaviour, p != nullptr, "a range is reserved without being backed");
        if (p) {
            const int e = kal_region_commit(p + 8 * g, 4 * g);
            observe(kind::behaviour, e == kal_ok, "a part of it is backed");
            if (e == kal_ok) {
                observe(kind::behaviour, reads_zero(p + 8 * g, 4 * g),
                        "a part backed for the first time reads as zero");
                fill(p + 8 * g, 4 * g, 31);
                observe(kind::behaviour, holds(p + 8 * g, 4 * g, 31),
                        "and keeps what was written");
            }
            kal_region_release(p, size);
        }
        // The addresses may be given out again: a second reservation of the
        // same size succeeds where the first was released.
        void* q = kal_region_reserve(size, g, 0);
        observe(kind::behaviour, q != nullptr, "a range is reserved again after one was released");
        if (q) kal_region_release(q, size);
    }

    // WHAT IS REFUSED.
    {
        observe(kind::behaviour, kal_region_reserve(g + 1, g, 0) == nullptr,
                "a size that is not a multiple of the granularity is refused");
        observe(kind::behaviour, kal_region_reserve(g, g * 3, 0) == nullptr,
                "an alignment that is not a power of two is refused");
        observe(kind::behaviour, kal_region_reserve(g, g, kal_uintptr{1} << 20) == nullptr,
                "a position of `flags' this version does not assign is refused");
        auto* p = static_cast<unsigned char*>(kal_region_reserve(g * 2, g, kal::region::commit_now));
        if (p) {
            observe(kind::behaviour, kal_region_commit(p + 1, g) == kal_err_invalid,
                    "a part that does not begin at a multiple of the granularity is refused");
            kal_region_release(p, g * 2);
        }
    }

    if (performs(kind::abi)) {
        observe(kind::abi, (props & ~kal::region::overcommit.bits) == 0,
                "the capability word contains no position the specification has not assigned");
    }

    if (performs(kind::stability)) {
        bool all = true;
        for (int i = 0; i < repetitions && all; ++i) {
            auto* p = static_cast<unsigned char*>(kal_region_reserve(g * 4, g, kal::region::commit_now));
            if (!p) { all = false; break; }
            p[0] = 1; p[g * 4 - 1] = 2;
            if (kal_region_decommit(p, g) != kal_ok || kal_region_commit(p, g) != kal_ok || p[0] != 0)
                all = false;
            kal_region_release(p, g * 4);
        }
        observe(kind::stability, all, "a range reserved, backed, returned and released many times");
    }

    if (performs(kind::cost)) {
        const kal_duration t0 = kal_time_monotonic();
        for (int i = 0; i < cost_iterations; ++i) {
            void* p = kal_region_reserve(g * 16, g * 16, 0);
            if (!p) break;
            kal_region_release(p, g * 16);
        }
        measure("reserving and releasing a range", kal_time_monotonic() - t0, cost_iterations);
        auto* p = static_cast<unsigned char*>(kal_region_reserve(g * 16, g, kal::region::commit_now));
        if (p) {
            const kal_duration t1 = kal_time_monotonic();
            for (int i = 0; i < cost_iterations; ++i) {
                p[(i % 16) * g] = 1;
                kal_region_discard(p + (i % 16) * g, g);
            }
            measure("writing a part and discarding it", kal_time_monotonic() - t1, cost_iterations);
            kal_region_release(p, g * 16);
        }
    }
#endif
}

}  // namespace okc::region
