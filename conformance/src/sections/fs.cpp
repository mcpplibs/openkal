module okc.fs;

import openkal.types;
import openkal.fs;
import openkal.stream;
import openkal.time;
import okc.report;
import okc.spec;

namespace okc::fs {
namespace {

kal_uintptr length(const char* s) { kal_uintptr n = 0; while (s && s[n]) ++n; return n; }

constexpr const char* kName = "okc-conformance.tmp";
constexpr const char* kDir  = "okc-conformance.dir";
constexpr const char* kOther = "okc-conformance-2.tmp";

#ifdef MCPP_FEATURE_FS
kal_dir here() { return kal::fs::working(); }

bool write_bytes(kal_file f, const char* s, kal_uintptr n) {
    kal_stream st{ kal_fs_stream(f) };
    const kal_io_result r = kal_stream_write(st, s, n);
    return r.e == kal_ok && r.n == n;
}

// Opens, writes and releases in one step, so that the observations that follow
// read as what they are about rather than as file handling.
bool put_file(const char* name, const char* text) {
    kal_file f{};
    const auto flags = kal::fs::open::write | kal::fs::open::create | kal::fs::open::truncate;
    if (kal::fs::open_file(here(), name, length(name), flags, &f) != kal_ok) return false;
    const bool ok = write_bytes(f, text, length(text));
    kal_fs_close_file(f);
    return ok;
}

long read_file(const char* name, char* buf, kal_uintptr cap) {
    kal_file f{};
    if (kal::fs::open_file(here(), name, length(name), kal::fs::open::read, &f) != kal_ok)
        return -1;
    kal_stream st{ kal_fs_stream(f) };
    const kal_io_result r = kal_stream_read(st, buf, cap);
    kal_fs_close_file(f);
    return r.e == kal_ok ? static_cast<long>(r.n) : -1;
}
#endif

}  // namespace

void run() {
    heading("openkal.fs");
#ifndef MCPP_FEATURE_FS
    unobserved(kind::behaviour, "openkal.fs", "the interface was not selected");
    return;
#else
    claim("kal_fs_props", kal_fs_props);

    // Every operation is relative to a directory the environment supplied, so
    // the first observation is that it supplied one.
    {
        const kal_uintptr n = kal_fs_preopen_count();
        observe(kind::behaviour, n >= 1, "the environment supplied at least one directory");
        kal_dir d{}; const char* name = nullptr; kal_uintptr len = 0;
        const int e = kal_fs_preopen(0, &d, &name, &len);
        observe(kind::behaviour, e == kal_ok && name != nullptr && len > 0,
                "the first supplied directory has a name");
        if (name) { put("  the program was started in: ");
                    kal_stream_write(kal_stdout(), name, len); put("\n"); }

        // Names are the environment's and this specification requires only that
        // they be distinct. A caller that resolves a global name against them
        // cannot do so if two are the same.
        bool distinct = true;
        for (kal_uintptr i = 0; i < n && distinct; ++i)
            for (kal_uintptr j = i + 1; j < n && distinct; ++j) {
                kal_dir a{}, b{}; const char* an = nullptr; const char* bn = nullptr;
                kal_uintptr al = 0, bl = 0;
                kal_fs_preopen(i, &a, &an, &al);
                kal_fs_preopen(j, &b, &bn, &bl);
                if (al != bl) continue;
                bool same = true;
                for (kal_uintptr k = 0; k < al; ++k) if (an[k] != bn[k]) { same = false; break; }
                if (same) distinct = false;
            }
        observe(kind::behaviour, distinct, "the supplied directories have distinct names");
    }

    // A name that ascends, and a name that begins with a separator, are
    // refused. A program able to ascend from the directory it was given would
    // not be confined by having been given it, and confinement is a property of
    // what the environment supplied rather than of the program's cooperation.
    {
        kal_file f{};
        const char* up = "../okc-conformance-escape";
        observe(kind::behaviour,
                kal::fs::open_file(here(), up, length(up), kal::fs::open::read, &f) != kal_ok,
                "a name that ascends is refused");
        const char* rooted = "/etc/passwd";
        observe(kind::behaviour,
                kal::fs::open_file(here(), rooted, length(rooted), kal::fs::open::read, &f) != kal_ok,
                "a name that begins with a separator is refused");
    }

    // Clause 7.12: the one reserved name. It is observed in both of the ways a
    // program uses it --- asking a question about the directory it holds, and
    // obtaining a second reference to it --- because an implementation may
    // accept it in one operation and not in the other, and one of the three
    // accepted it in neither until this was written.
    {
        const char* self = ".";
        kal_node_info info{};
        observe(kind::behaviour,
                kal_fs_info(here(), self, length(self), &info) == kal_ok
                    && info.kind == kal_node_directory,
                "the reserved name denotes the directory itself");

        kal_dir again{};
        const int e = kal_fs_open_dir(here(), self, length(self), &again);
        observe(kind::behaviour, e == kal_ok,
                "the directory itself can be opened through the reserved name");
        if (e == kal_ok) {
            // The second reference is a directory in its own right: a name
            // created through the original is found through it.
            const char* probe = "okc-self.tmp";
            put_file(probe, "x");
            kal_node_info seen{};
            observe(kind::behaviour,
                    kal_fs_info(again, probe, length(probe), &seen) == kal_ok
                        && seen.kind == kal_node_file,
                    "the second reference reaches what the first reaches");
            kal_fs_remove(here(), probe, length(probe));
            kal_fs_close_dir(again);
        } else {
            unobserved(kind::behaviour, "the second reference reaches what the first reaches",
                       "the directory itself could not be opened");
        }
    }

    // Clause 7.7: enquiry about a name that does not exist is answered, and
    // access to it is refused with the value that says which condition held.
    {
        kal_fs_remove(here(), kName, length(kName));
        kal_node_info info{};
        observe(kind::behaviour,
                kal_fs_info(here(), kName, length(kName), &info) == kal_ok
                    && info.kind == kal_node_absent,
                "enquiry about a name that does not exist succeeds and reports absence");
        kal_file f{};
        observe(kind::behaviour,
                kal::fs::open_file(here(), kName, length(kName), kal::fs::open::read, &f)
                    == kal_err_not_found,
                "opening a name that does not exist reports that it does not exist");
    }

    // Creation, transfer, positioning, length and enquiry about the handle.
    {
        observe(kind::behaviour, put_file(kName, "0123456789"), "a file is created and written");

        kal_file f{};
        const int e = kal::fs::open_file(here(), kName, length(kName),
                                         kal::fs::open::read | kal::fs::open::write, &f);
        observe(kind::behaviour, e == kal_ok, "the file is opened again");
        if (e == kal_ok) {
            kal_u64 at = 0;
            observe(kind::behaviour, kal_fs_seek(f, 3, kal::fs::seek_set, &at) == kal_ok && at == 3,
                    "positioning reports where it arrived");
            char buf[8] = {};
            kal_stream st{ kal_fs_stream(f) };
            const kal_io_result r = kal_stream_read(st, buf, 4);
            observe(kind::behaviour,
                    r.e == kal_ok && r.n == 4 && buf[0] == '3' && buf[3] == '6',
                    "a transfer after positioning reads from where it was positioned");

            kal_node_info info{};
            observe(kind::behaviour,
                    kal_fs_file_info(f, &info) == kal_ok && info.size == 10
                        && info.kind == kal_node_file,
                    "enquiry about the open file reports its length and what it is");

            observe(kind::behaviour, kal_fs_truncate(f, 4) == kal_ok
                        && kal_fs_file_info(f, &info) == kal_ok && info.size == 4,
                    "the length of an open file is set");

            // The inverse of the enquiry above, and it is checked by reading
            // back rather than by the value returned: an implementation that
            // reported success and set nothing would satisfy the return value.
            //
            // Whole seconds, not nanoseconds. The three environments openkal is
            // implemented on record a modification time to a nanosecond, to a
            // microsecond and to a hundred nanoseconds respectively, so an
            // observation that required the value to come back unchanged would
            // be requiring a resolution the interface does not claim. A second
            // is the resolution every environment that records the time at all
            // agrees upon.
            if (kal::fs::has(kal::fs::modified_time)) {
                // Opened again for writing, because that is what the operation
                // requires: one environment decides at the point of opening
                // what may afterwards be done with a file.
                const kal_u64 chosen = 1600000000ull * 1000000000ull;  // 2020-09-13
                kal_file w{};
                const int opened = kal::fs::open_file(here(), kName, length(kName),
                                                      kal::fs::open::read | kal::fs::open::write,
                                                      &w);
                const int e = opened == kal_ok ? kal_fs_set_modified(w, chosen) : opened;
                kal_node_info after{};
                const int read_back = opened == kal_ok ? kal_fs_file_info(w, &after) : opened;
                if (opened == kal_ok) kal_fs_close_file(w);
                observe(kind::behaviour,
                        e == kal_ok && read_back == kal_ok
                            && after.modified_ns / 1000000000u == chosen / 1000000000u,
                        "the time an open file reports as its last modification is set");
            } else {
                unobserved(kind::behaviour,
                           "the time an open file reports as its last modification is set",
                           "the implementation does not claim prop_modified_time");
            }
            kal_fs_close_file(f);
        }
    }

    // The three conditions clause 7.8 records, each observed by its effect. A
    // return value alone would accept an implementation that reported success
    // and did nothing, which is exactly the outcome those flags exist to
    // exclude.
    {
        char buf[64];
        observe(kind::behaviour, put_file(kName, "0123456789") && put_file(kName, "abc")
                    && read_file(kName, buf, sizeof buf) == 3,
                "opening with truncate discards what lay beyond");

        kal_file f{};
        const auto excl = kal::fs::open::write | kal::fs::open::create | kal::fs::open::exclusive;
        observe(kind::behaviour,
                kal::fs::open_file(here(), kName, length(kName), excl, &f) == kal_err_exists,
                "creating a name that exists, exclusively, reports that it exists");

        put_file(kName, "one");
        const auto app = kal::fs::open::write | kal::fs::open::append;
        if (kal::fs::open_file(here(), kName, length(kName), app, &f) == kal_ok) {
            kal_u64 at = 0;
            kal_fs_seek(f, 0, kal::fs::seek_set, &at);   // and it shall still append
            write_bytes(f, "two", 3);
            kal_fs_close_file(f);
        }
        const long n = read_file(kName, buf, sizeof buf);
        observe(kind::behaviour, n == 6 && buf[0] == 'o' && buf[3] == 't',
                "a transfer to a file opened for appending goes to the end");
    }

    // Directories: creation, enumeration, removal.
    {
        kal_fs_remove(here(), kDir, length(kDir));
        observe(kind::behaviour, kal_fs_mkdir(here(), kDir, length(kDir)) == kal_ok,
                "a directory is created");
        kal_dir d{};
        const int e = kal_fs_open_dir(here(), kDir, length(kDir), &d);
        observe(kind::behaviour, e == kal_ok, "the directory is opened");
        if (e == kal_ok) {
            kal_file f{};
            const auto flags = kal::fs::open::write | kal::fs::open::create;
            if (kal::fs::open_file(d, "a", 1, flags, &f) == kal_ok) kal_fs_close_file(f);
            if (kal::fs::open_file(d, "b", 1, flags, &f) == kal_ok) kal_fs_close_file(f);

            // What was reported is kept, not only how much of it. A count that
            // does not match tells a reader that something is wrong and not
            // what, and the difference between "one entry too many" and "the
            // wrong two entries" is the whole of the diagnosis.
            int seen = 0;
            char reported[256]; kal_uintptr at = 0;
            kal_uintptr iter = 0;
            if (kal_fs_list_begin(d, &iter) == kal_ok) {
                for (;;) {
                    const char* name = nullptr; kal_uintptr len = 0; int knd = 0;
                    if (kal_fs_list_next(d, &iter, &name, &len, &knd) != kal_ok) break;
                    if (!name) break;
                    ++seen;
                    if (at + len + 2 < sizeof reported) {
                        if (at) reported[at++] = ' ';
                        for (kal_uintptr k = 0; k < len; ++k) reported[at++] = name[k];
                        reported[at] = '\0';
                    }
                }
            }
            reported[at < sizeof reported ? at : sizeof reported - 1] = '\0';
            observe(kind::behaviour, seen == 2,
                    "enumeration reports the entries that were created and nothing else");
            if (seen != 2) { line("  what it reported: "); line(reported); line("\n"); }

            kal_fs_remove(d, "a", 1);
            kal_fs_remove(d, "b", 1);
            kal_fs_close_dir(d);
        }
        observe(kind::behaviour, kal_fs_remove(here(), kDir, length(kDir)) == kal_ok,
                "the directory is removed");
    }

    // Renaming, and that the old name is then absent.
    {
        put_file(kName, "x");
        kal_fs_remove(here(), kOther, length(kOther));
        observe(kind::behaviour,
                kal_fs_rename(here(), kName, length(kName), here(), kOther, length(kOther)) == kal_ok,
                "a name is renamed");
        kal_node_info info{};
        observe(kind::behaviour,
                kal_fs_info(here(), kName, length(kName), &info) == kal_ok
                    && info.kind == kal_node_absent,
                "the name it was renamed from is then absent");
        kal_fs_remove(here(), kOther, length(kOther));
    }

    if (performs(kind::abi)) {
        observe(kind::abi, sizeof(kal_dir) == sizeof(kal_uintptr)
                        && sizeof(kal_file) == sizeof(kal_uintptr),
                "a directory and a file handle each occupy one machine word");
        const kal_uintptr assigned = (kal::fs::case_sensitive | kal::fs::links
                                    | kal::fs::modified_time | kal::fs::atomic_rename).bits;
        observe(kind::abi, (kal_fs_props & ~assigned) == 0,
                "the capability word contains no position the specification has not assigned");

        // Clause 6.6: an implementation shall not treat a released handle as
        // valid. The recommended construction divides the word into an index
        // and a generation; whatever the construction, the property is the
        // same and it is what this observes.
        put_file(kName, "x");
        kal_file f{};
        if (kal::fs::open_file(here(), kName, length(kName), kal::fs::open::read, &f) == kal_ok) {
            const kal_file released = f;
            kal_fs_close_file(f);
            kal_node_info info{};
            observe(kind::abi, kal_fs_file_info(released, &info) != kal_ok,
                    "a released handle is not treated as valid");
        }
        kal_fs_remove(here(), kName, length(kName));
    }

    if (performs(kind::stability)) {
        // Where a handle scheme that never reuses a slot stops. An
        // implementation that packed a generation into the word and never
        // reclaimed the index would satisfy every observation above.
        bool all = true;
        put_file(kName, "x");
        for (int i = 0; i < repetitions && all; ++i) {
            kal_file f{};
            if (kal::fs::open_file(here(), kName, length(kName), kal::fs::open::read, &f) != kal_ok)
                all = false;
            else kal_fs_close_file(f);
        }
        observe(kind::stability, all, "a file opened and released many times is openable again");
        kal_fs_remove(here(), kName, length(kName));
    }

    if (performs(kind::cost)) {
        put_file(kName, "x");
        const kal_duration t0 = kal_time_monotonic();
        for (int i = 0; i < cost_iterations; ++i) {
            kal_file f{};
            if (kal::fs::open_file(here(), kName, length(kName), kal::fs::open::read, &f) == kal_ok)
                kal_fs_close_file(f);
        }
        measure("opening and releasing a file", kal_time_monotonic() - t0, cost_iterations);
        kal_fs_remove(here(), kName, length(kName));
    }
#endif
}

}  // namespace okc::fs
