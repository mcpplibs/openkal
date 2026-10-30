module okc.map;

import openkal.types;
import openkal.fs;
import openkal.stream;
import openkal.map;
import openkal.version;
import okc.child;
import okc.report;
import okc.spec;

namespace okc::map {
namespace {

#if defined(MCPP_FEATURE_MAP)
constexpr char kName[] = "okc-conformance-map.tmp";
constexpr kal_uintptr kSize = 2 * static_cast<kal_uintptr>(kal::map::alignment) + 1000;

kal_dir here() { return kal::fs::working(); }

// The byte at position i of the file: a pattern no zero-filled page matches.
unsigned char pattern(kal_uintptr i) { return static_cast<unsigned char>((i * 7 + 3) % 251 + 1); }

bool make_file() {
    kal_file f{};
    const auto flags = kal::fs::open::write | kal::fs::open::create | kal::fs::open::truncate;
    if (kal::fs::open_file(here(), kName, sizeof kName - 1, flags, &f) != kal_ok) return false;
    unsigned char block[4096];
    bool ok = true;
    for (kal_uintptr at = 0; ok && at < kSize; at += sizeof block) {
        const kal_uintptr n = kSize - at < sizeof block ? kSize - at : sizeof block;
        for (kal_uintptr i = 0; i < n; ++i) block[i] = pattern(at + i);
        ok = kal_stream_write(kal_fs_stream(f), block, n) == static_cast<kal_intptr>(n);
    }
    kal_fs_close_file(f);
    return ok;
}

bool open_as(kal::fs::open_flags flags, kal_file& f) {
    return kal::fs::open_file(here(), kName, sizeof kName - 1, flags, &f) == kal_ok;
}

// One byte of the file through `openkal.fs'.
int file_byte(kal_uintptr at) {
    kal_file f{};
    if (!open_as(kal::fs::open::read, f)) return -1;
    kal_u64 where = 0;
    unsigned char b = 0;
    int r = -1;
    if (kal_fs_seek(f, static_cast<kal_i64>(at), kal::fs::seek_set, &where) == kal_ok
        && kal_stream_read(kal_fs_stream(f), &b, 1) == 1)
        r = b;
    kal_fs_close_file(f);
    return r;
}
#endif

}  // namespace

void run() {
    heading("openkal.map");
#ifndef MCPP_FEATURE_MAP
    unobserved(kind::behaviour, "openkal.map", "the interface was not selected");
    return;
#else
    observe(kind::behaviour, kal::provides(kal::iface::map), "kal_interfaces reports openkal.map");
    if (!make_file()) {
        unobserved(kind::behaviour, "openkal.map", "a file to map could not be written");
        return;
    }
    const kal_uintptr A = static_cast<kal_uintptr>(kal::map::alignment);

    // 1. Read, private: the bytes are the file's, from an aligned offset, and
    //    the last page reads zero past the end.
    {
        kal_file f{};
        if (open_as(kal::fs::open::read, f)) {
            auto r = kal::map::file(f, A, kSize - A, kal::map::read);
            observe(kind::behaviour, r.e == kal_ok && r.region != nullptr, "a part of a file is mapped");
            if (r.e == kal_ok) {
                const auto* b = static_cast<const unsigned char*>(r.region);
                bool same = true;
                for (kal_uintptr i = 0; i < kSize - A && same; ++i) same = b[i] == pattern(A + i);
                observe(kind::behaviour, same, "the region holds the file's bytes from its offset");
                bool zero = true;
                const kal_uintptr end = kSize - A, page_end = (end + 4095) / 4096 * 4096;
                for (kal_uintptr i = end; i < page_end && zero; ++i) zero = b[i] == 0;
                observe(kind::behaviour, zero, "after the file's end the last page reads zero");
            }
            // The region outlives the handle.
            kal_fs_close_file(f);
            if (r.e == kal_ok) {
                observe(kind::behaviour, static_cast<const unsigned char*>(r.region)[5] == pattern(A + 5),
                        "the region remains readable after its file is closed");
                kal::map::release(r.region, kSize - A);
            }
        }
    }

    // 2. Write, private: seen in the region, never in the file.
    {
        kal_file f{};
        if (open_as(kal::fs::open::read, f)) {
            auto r = kal::map::file(f, 0, kSize, kal::map::write);
            observe(kind::behaviour, r.e == kal_ok, "a private region of a file opened to read may be written");
            if (r.e == kal_ok) {
                auto* b = static_cast<unsigned char*>(r.region);
                b[10] = 0;
                observe(kind::behaviour, b[10] == 0, "a private write is seen in its region");
                observe(kind::behaviour, kal::map::sync(r.region, kSize) == kal_ok,
                        "kal_map_sync of a private region does nothing and succeeds");
                observe(kind::behaviour, file_byte(10) == pattern(10), "a private write does not reach the file");
                kal::map::release(r.region, kSize);
            }
            kal_fs_close_file(f);
        }
    }

    // 3. Write, shared: seen at once in a second shared region, and in the
    //    file after kal_map_sync.
    {
        kal_file f{};
        if (open_as(kal::fs::open::read | kal::fs::open::write, f)) {
            auto one = kal::map::file(f, 0, kSize, kal::map::write | kal::map::shared);
            auto two = kal::map::file(f, 0, kSize, kal::map::shared);
            observe(kind::behaviour, one.e == kal_ok && two.e == kal_ok, "two shared regions of one file are mapped");
            if (one.e == kal_ok && two.e == kal_ok) {
                auto* w = static_cast<unsigned char*>(one.region);
                const auto* r = static_cast<const unsigned char*>(two.region);
                const unsigned char v = static_cast<unsigned char>(~pattern(A + 1));
                w[A + 1] = v;
                observe(kind::behaviour, r[A + 1] == v, "a shared write is seen at once in another shared region");
                observe(kind::behaviour, kal::map::sync(one.region, kSize) == kal_ok, "a shared region is synchronised");
                observe(kind::behaviour, file_byte(A + 1) == v, "after kal_map_sync the file holds the write");
            }
            if (one.e == kal_ok) kal::map::release(one.region, kSize);
            if (two.e == kal_ok) kal::map::release(two.region, kSize);
            kal_fs_close_file(f);
        }
    }

    // 4. What is refused.
    {
        kal_file f{};
        if (open_as(kal::fs::open::read, f)) {
            void* p = nullptr;
            observe(kind::behaviour, kal_map_file(f, 4096, 100, 0, &p) == kal_err_invalid,
                    "an offset that is not a multiple of the alignment is refused");
            observe(kind::behaviour, kal_map_file(f, 0, 0, 0, &p) == kal_err_invalid,
                    "a length of zero is refused");
            observe(kind::behaviour, kal_map_file(f, A, kSize, 0, &p) == kal_err_invalid,
                    "a region beyond the file's end is refused");
            observe(kind::behaviour, kal_map_file(f, 0, 100, kal_uintptr{1} << 7, &p) == kal_err_invalid,
                    "a position the specification has not assigned is refused");
            observe(kind::behaviour,
                    kal_map_file(f, 0, 100, (kal::map::write | kal::map::shared).bits, &p) == kal_err_permission,
                    "a shared region to write needs a file opened to write");
            kal_fs_close_file(f);
        }
    }

    // 5. A file shortened beneath a region: refused, or a fault on a page that
    //    lies beyond the new end; never zero in place of what was removed.
    {
        int status = 0, terminated = 0;
        if (start_copy("openkal-conformance-child", argument_for(errand::map_beyond_shortened),
                       status, terminated)) {
            observe(kind::behaviour, terminated != 0 || status == 50,
                    "a page beyond a shortened file's end is a fault, or the shortening is refused");
            observe(kind::behaviour, !(terminated == 0 && status == 51),
                    "the bytes after a shortened file's new end in its page are zero");
        } else {
            unobserved(kind::behaviour, "a page beyond a shortened file's end is a fault",
                       "a copy of this program could not be started");
        }
    }

    kal_fs_remove(here(), kName, sizeof kName - 1);
    kal_fs_remove(here(), "okc-conformance-map-short.tmp", sizeof "okc-conformance-map-short.tmp" - 1);
#endif
}

}  // namespace okc::map
