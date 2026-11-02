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
    const kal_intptr r = kal_stream_write(st, s, n);
    return r == static_cast<kal_intptr>(n);
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
    kal_uintptr have = 0;
    while (have < cap) {
        const kal_intptr r = kal_stream_read(st, buf + have, cap - have);
        if (r <= 0) break;
        have += static_cast<kal_uintptr>(r);
    }
    kal_fs_close_file(f);
    return static_cast<long>(have);
}

// Whether `entry' (`<kind><name>\0') appears in the record `rec' of `n'
// bytes, and how many times.
int count_entry(const char* rec, long n, char kind, const char* name) {
    int seen = 0;
    const kal_uintptr nl = length(name);
    for (long at = 0; at < n;) {
        long end = at;
        while (end < n && rec[end] != 0) ++end;
        if (end - at == static_cast<long>(nl) + 1 && rec[at] == kind) {
            bool same = true;
            for (kal_uintptr i = 0; i < nl && same; ++i) same = rec[at + 1 + static_cast<long>(i)] == name[i];
            if (same) ++seen;
        }
        at = end + 1;
    }
    return seen;
}

// A LISTING WITH ATTRIBUTES (0.23): what it gives is what an enquiry gives, and
// it states no more than it gave.
void listing_with_attributes() {
    put_file(kName, "four");
    kal_uintptr iter = 0;
    if (kal_fs_list_begin(here(), &iter) != kal_ok) {
        unobserved(kind::behaviour, "a listing gives what an enquiry gives", "the directory could not be listed");
        kal_fs_remove(here(), kName, length(kName));
        return;
    }
    const kal_u32 wanted = kal::fs::field::kind | kal::fs::field::size | kal::fs::field::modified | kal::fs::field::identity;
    bool agrees = true, within = true, found = false;
    for (int guard = 0; iter != 0 && guard < 100000; ++guard) {
        char name[512]; kal_uintptr nlen = 0;
        kal_node_info li = kal::fs::info_for_caller();
        if (kal_fs_list_next_info(here(), &iter, name, sizeof name, &nlen, wanted, &li) != kal_ok) break;
        if (iter == 0 && nlen == 0) break;
        if ((li.present & ~(wanted | kal::fs::field::kind)) != 0 || (li.present & kal::fs::field::kind) == 0) within = false;
        if (nlen == length(kName)) {
            bool same = true;
            for (kal_uintptr i = 0; i < nlen && same; ++i) same = name[i] == kName[i];
            if (same) {
                found = true;
                kal_node_info fi = kal::fs::info_for_caller();
                if (kal_fs_info(here(), name, nlen, 0, wanted, &fi) != kal_ok || fi.kind != li.kind) agrees = false;
                if ((li.present & kal::fs::field::size) && li.size != 4) agrees = false;
                if ((li.present & kal::fs::field::identity) && (fi.present & kal::fs::field::identity)
                    && (li.identity[0] != fi.identity[0] || li.identity[1] != fi.identity[1]))
                    agrees = false;
            }
        }
    }
    observe(kind::behaviour, within, "a listing states the entry's kind, and no field it was not asked for");
    observe(kind::behaviour, found && agrees, "what a listing gives of a file is what an enquiry gives");
    if ((kal_fs_props(here()) & kal::fs::list_info.bits) != 0) {
        kal_uintptr it = 0;
        bool gives = kal_fs_list_begin(here(), &it) == kal_ok;
        while (gives && it != 0) {
            char name[512]; kal_uintptr nlen = 0;
            kal_node_info li = kal::fs::info_for_caller();
            if (kal_fs_list_next_info(here(), &it, name, sizeof name, &nlen, wanted, &li) != kal_ok) break;
            if (it == 0 && nlen == 0) break;
            if (li.kind == kal_node_file
                && (li.present & (kal::fs::field::size | kal::fs::field::modified | kal::fs::field::identity))
                       != (kal::fs::field::size | kal::fs::field::modified | kal::fs::field::identity))
                gives = false;
        }
        observe(kind::behaviour, gives, "a volume that states KAL_FS_PROP_LIST_INFO gives size, time and identity");
    } else {
        unobserved(kind::behaviour, "a volume that states KAL_FS_PROP_LIST_INFO gives size, time and identity",
                   "the volume does not state it");
    }
    kal_fs_remove(here(), kName, length(kName));
}

// ATTENUATION (0.23): a handle with less, which takes away and never gives
// back; what it records.
void attenuation() {
    kal_dir_rights r{};
    r.self_size = sizeof r;
    r.flags = kal::fs::dir_read_only.bits;
    kal_dir ro{};
    if ((kal_fs_props(here()) & kal::fs::attenuate.bits) == 0) {
        observe(kind::behaviour, kal_fs_dir_attenuate(here(), &r, &ro) == kal_err_not_supported,
                "attenuation is refused where the volume does not state it");
        return;
    }
    put_file(kName, "x");
    kal_fs_remove(here(), kDir, length(kDir));
    kal_fs_mkdir(here(), kDir, length(kDir));
    kal_dir_rights bad = r;
    bad.flags = kal_uintptr{1} << 9;
    observe(kind::behaviour, kal_fs_dir_attenuate(here(), &bad, &ro) == kal_err_invalid,
            "a position of the rights this version does not assign is refused");
    bad = r; bad.self_size = 4;
    observe(kind::behaviour, kal_fs_dir_attenuate(here(), &bad, &ro) == kal_err_invalid,
            "rights shorter than their fixed fields are refused");
    bad = r; bad.flags = kal::fs::dir_recorded.bits;
    observe(kind::behaviour, kal_fs_dir_attenuate(here(), &bad, &ro) == kal_err_invalid,
            "a record asked for without a file to write it to is refused");

    if (kal_fs_dir_attenuate(here(), &r, &ro) != kal_ok) {
        observe(kind::behaviour, false, "a directory handle is attenuated read-only");
        kal_fs_remove(here(), kName, length(kName));
        kal_fs_remove(here(), kDir, length(kDir));
        return;
    }
    observe(kind::behaviour, (kal_fs_props(ro) & kal::fs::read_only.bits) != 0
                             && (kal_fs_props(here()) & kal::fs::read_only.bits) == 0,
            "the attenuated handle states it is read-only, and the one it came from does not");
    kal_file f{};
    const bool reads = kal::fs::open_file(ro, kName, length(kName), kal::fs::open::read, &f) == kal_ok;
    if (reads) kal_fs_close_file(f);
    observe(kind::behaviour, reads, "a read-only handle opens a file for reading");
    observe(kind::behaviour,
            kal::fs::open_file(ro, kOther, length(kOther), kal::fs::open::write | kal::fs::open::create, &f)
                == kal_err_permission,
            "a read-only handle refuses to create a file");
    observe(kind::behaviour,
            kal::fs::open_file(ro, kName, length(kName), kal::fs::open::write, &f) == kal_err_permission,
            "and to open one for writing");
    observe(kind::behaviour, kal_fs_remove(ro, kName, length(kName)) == kal_err_permission,
            "and to remove a name");
    observe(kind::behaviour, kal_fs_rename(ro, kName, length(kName), ro, kOther, length(kOther)) == kal_err_permission,
            "and to rename one");
    observe(kind::behaviour, kal_fs_mkdir(ro, kOther, length(kOther)) == kal_err_permission,
            "and to make a directory");
    kal_dir sub{};
    if (kal_fs_open_dir(ro, kDir, length(kDir), &sub) == kal_ok) {
        observe(kind::behaviour, (kal_fs_props(sub) & kal::fs::read_only.bits) != 0
                                 && kal::fs::open_file(sub, kName, length(kName),
                                                       kal::fs::open::write | kal::fs::open::create, &f)
                                        == kal_err_permission,
                "a directory opened beneath a read-only handle is read-only");
        kal_fs_close_dir(sub);
    }
    kal_dir_rights none{};
    none.self_size = sizeof none;
    kal_dir again{};
    if (kal_fs_dir_attenuate(ro, &none, &again) == kal_ok) {
        observe(kind::behaviour, (kal_fs_props(again) & kal::fs::read_only.bits) != 0,
                "attenuation takes away and does not give back");
        kal_fs_close_dir(again);
    }
    kal_fs_close_dir(ro);

    kal_dir_rights u{};
    u.self_size = sizeof u;
    u.flags = kal::fs::dir_unchanging.bits;
    kal_dir un{};
    if (kal_fs_dir_attenuate(here(), &u, &un) == kal_ok) {
        kal_node_info i = kal::fs::info_for_caller();
        observe(kind::behaviour, (kal_fs_props(un) & kal::fs::unchanging.bits) != 0
                                 && kal_fs_info(un, kOther, length(kOther), 0, kal::fs::field::kind, &i) == kal_ok
                                 && i.kind == kal_node_absent,
                "a handle that states nothing beneath changes answers as before");
        kal_fs_close_dir(un);
    }

    // THE RECORD: each name once, with what was found; written when the
    // handle is closed.
    static const char kRecord[] = "okc-conformance.record";
    static const char kAbsent[] = "okc-conformance-absent.tmp";
    kal_file rec{};
    if (kal::fs::open_file(here(), kRecord, sizeof kRecord - 1,
                           kal::fs::open::write | kal::fs::open::create | kal::fs::open::truncate, &rec) == kal_ok) {
        kal_dir_rights w{};
        w.self_size = sizeof w;
        w.flags = kal::fs::dir_recorded.bits;
        w.record = rec;
        kal_dir rd{};
        if (kal_fs_dir_attenuate(here(), &w, &rd) == kal_ok) {
            for (int twice = 0; twice < 2; ++twice) {
                if (kal::fs::open_file(rd, kName, length(kName), kal::fs::open::read, &f) == kal_ok) kal_fs_close_file(f);
                kal_node_info i = kal::fs::info_for_caller();
                kal_fs_info(rd, kAbsent, sizeof kAbsent - 1, 0, kal::fs::field::kind, &i);
            }
            kal_uintptr it = 0;
            if (kal_fs_list_begin(rd, &it) == kal_ok)
                while (it != 0) {
                    char name[512]; kal_uintptr nlen = 0; int kind = 0;
                    if (kal_fs_list_next(rd, &it, name, sizeof name, &nlen, &kind) != kal_ok) break;
                    if (it == 0 && nlen == 0) break;
                }
            kal_fs_close_dir(rd);
            char buf[4096];
            const long n = read_file(kRecord, buf, sizeof buf);
            observe(kind::behaviour, n > 0 && count_entry(buf, n, 'r', kName) == 1,
                    "a file opened through a recorded handle is recorded once, as read");
            observe(kind::behaviour, n > 0 && count_entry(buf, n, 'a', kAbsent) == 1,
                    "a name asked about and absent is recorded once, as absent");
            observe(kind::behaviour, n > 0 && count_entry(buf, n, 'l', ".") == 1,
                    "the directory listed is recorded, by the name `.'");
        } else {
            observe(kind::behaviour, false, "a handle is attenuated to record what it reaches");
        }
        kal_fs_close_file(rec);
        kal_fs_remove(here(), kRecord, sizeof kRecord - 1);
    }
    kal_fs_remove(here(), kName, length(kName));
    kal_fs_remove(here(), kOther, length(kOther));
    kal_fs_remove(here(), kDir, length(kDir));
}
#endif

}  // namespace

// Every enquiry states how much of the structure exists on this side, and asks
// for everything. Written once, so that a call site says what it is asking and
// not how the asking works.
kal_node_info fresh() { return kal::fs::info_for_caller(); }

void run() {
    heading("openkal.fs");
#ifndef MCPP_FEATURE_FS
    unobserved(kind::behaviour, "openkal.fs", "the interface was not selected");
    return;
#else
    claim("kal_fs_props(here())", kal_fs_props(here()));

    // Every operation is relative to a directory the environment supplied, so
    // the first observation is that it supplied one.
    {
        const kal_uintptr n = kal_fs_preopen_count();
        observe(kind::behaviour, n >= 1, "the environment supplied at least one directory");
        kal_dir d{}; char name[1024]; kal_uintptr len = 0;
        const int e = kal_fs_preopen(0, &d, name, sizeof name, &len);
        observe(kind::behaviour, e == kal_ok && len > 0,
                "the first supplied directory has a name");
        if (e == kal_ok && len > 0 && len < sizeof name) {
            put("  the program was started in: ");
            kal_stream_write(kal_stdout(), name, len); put("\n");
        }

        // THE NAME IS COPIED AND THE LENGTH IS THE NAME'S. A capacity of zero
        // reports the length without writing, which is what lets a caller size
        // a buffer before it has one.
        {
            kal_dir probe{}; kal_uintptr sized = 0;
            const int se = kal_fs_preopen(0, &probe, nullptr, 0, &sized);
            observe(kind::behaviour, se == kal_ok && sized == len,
                    "a capacity of zero reports the name's length without writing");
        }

        // Names are the environment's and this specification requires only that
        // they be distinct. A caller that resolves a global name against them
        // cannot do so if two are the same.
        bool distinct = true;
        for (kal_uintptr i = 0; i < n && distinct; ++i)
            for (kal_uintptr j = i + 1; j < n && distinct; ++j) {
                kal_dir a{}, b{}; char an[1024], bn[1024];
                kal_uintptr al = 0, bl = 0;
                kal_fs_preopen(i, &a, an, sizeof an, &al);
                kal_fs_preopen(j, &b, bn, sizeof bn, &bl);
                if (al >= sizeof an || bl >= sizeof bn) continue;
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
        kal_node_info info = fresh();
        observe(kind::behaviour,
                kal_fs_info(here(), self, length(self), 0, kal::fs::field::all, &info) == kal_ok
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
            kal_node_info seen = fresh();
            observe(kind::behaviour,
                    kal_fs_info(again, probe, length(probe), 0, kal::fs::field::all, &seen) == kal_ok
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
        kal_node_info info = fresh();
        observe(kind::behaviour,
                kal_fs_info(here(), kName, length(kName), 0, kal::fs::field::all, &info) == kal_ok
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
            const kal_intptr r = kal_stream_read(st, buf, 4);
            observe(kind::behaviour,
                    r == 4 && buf[0] == '3' && buf[3] == '6',
                    "a transfer after positioning reads from where it was positioned");

            kal_node_info info = fresh();
            observe(kind::behaviour,
                    kal_fs_file_info(f, kal::fs::field::all, &info) == kal_ok && info.size == 10
                        && info.kind == kal_node_file,
                    "enquiry about the open file reports its length and what it is");

            observe(kind::behaviour, kal_fs_truncate(f, 4) == kal_ok
                        && kal_fs_file_info(f, kal::fs::field::all, &info) == kal_ok && info.size == 4,
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
            if (kal::fs::has(here(), kal::fs::modified_time)) {
                // Opened again for writing, because that is what the operation
                // requires: one environment decides at the point of opening
                // what may afterwards be done with a file.
                const kal_u64 chosen = 1600000000ull * 1000000000ull;  // 2020-09-13
                kal_file w{};
                const int opened = kal::fs::open_file(here(), kName, length(kName),
                                                      kal::fs::open::read | kal::fs::open::write,
                                                      &w);
                const int e = opened == kal_ok ? kal_fs_set_modified(w, chosen) : opened;
                kal_node_info after = fresh();
                const int read_back = opened == kal_ok ? kal_fs_file_info(w, kal::fs::field::all, &after) : opened;
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

    // TRANSFERS AT AN OFFSET (version 0.20). Each is observed with the
    // position somewhere other than the offset, so that an implementation that
    // composed the transfer from a seek, or moved the position as a side
    // effect, reports itself: the sequential transfer afterwards reads from
    // where the position was left, not from beyond the offset.
    {
        observe(kind::behaviour, put_file(kName, "0123456789"), "a file is written for transfers at offsets");
        kal_file f{};
        const int e = kal::fs::open_file(here(), kName, length(kName),
                                         kal::fs::open::read | kal::fs::open::write, &f);
        if (e == kal_ok) {
            kal_u64 at = 0;
            kal_fs_seek(f, 2, kal::fs::seek_set, &at);
            char buf[16] = {};
            const kal_intptr r = kal_fs_read_at(f, buf, 3, 6);
            observe(kind::behaviour, r == 3 && buf[0] == '6' && buf[2] == '8',
                    "a read at an offset reads from the offset");
            kal_u64 now = 0;
            observe(kind::behaviour, kal_fs_seek(f, 0, kal::fs::seek_current, &now) == kal_ok && now == 2,
                    "and leaves the position where it was");
            const kal_intptr w = kal_fs_write_at(f, "XY", 2, 8);
            observe(kind::behaviour, w == 2, "a write at an offset writes the whole buffer");
            kal_fs_seek(f, 0, kal::fs::seek_current, &now);
            observe(kind::behaviour, now == 2, "and leaves the position where it was");
            char seq[4] = {};
            const kal_intptr s1 = kal_stream_read(kal_fs_stream(f), seq, 2);
            observe(kind::behaviour, s1 == 2 && seq[0] == '2' && seq[1] == '3',
                    "a sequential transfer afterwards continues from the position");
            char tail[4] = {};
            observe(kind::behaviour, kal_fs_read_at(f, tail, 4, 7) == 3
                        && tail[0] == '7' && tail[1] == 'X' && tail[2] == 'Y',
                    "a read at an offset sees what a write at an offset placed, up to the end");
            observe(kind::behaviour, kal_fs_read_at(f, tail, 4, 10) == 0
                        && kal_fs_read_at(f, tail, 4, 1000) == 0,
                    "a read at or beyond the end reads nothing");
            observe(kind::behaviour, kal_fs_write_at(f, "Z", 1, 12) == 1,
                    "a write beyond the end extends the file");
            kal_node_info info = fresh();
            observe(kind::behaviour, kal_fs_file_info(f, kal::fs::field::all, &info) == kal_ok && info.size == 13
                        && kal_fs_read_at(f, tail, 3, 10) == 3 && tail[0] == 0 && tail[1] == 0 && tail[2] == 'Z',
                    "and what lies between the old end and the write reads as zero");
            kal_fs_close_file(f);
        }

        if (performs(kind::cost) && put_file(kName, "0123456789") &&
            kal::fs::open_file(here(), kName, length(kName), kal::fs::open::read, &f) == kal_ok) {
            char b[4];
            const kal_duration t0 = kal_time_monotonic();
            for (int i = 0; i < cost_iterations; ++i) kal_fs_read_at(f, b, 4, static_cast<kal_u64>(i % 6));
            measure("reading at an offset", kal_time_monotonic() - t0, cost_iterations);
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
            bool ended_well = false;
            if (kal_fs_list_begin(d, &iter) == kal_ok) {
                for (;;) {
                    char name[512]; kal_uintptr len = 99; int knd = 0;
                    if (kal_fs_list_next(d, &iter, name, sizeof name, &len, &knd) != kal_ok) break;
                    if (iter == 0) { ended_well = len == 0; break; }
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
            observe(kind::behaviour, ended_well,
                    "the end of an enumeration is kal_ok, the iterator zero and the name empty");
            if (seen != 2) { line("  what it reported: "); line(reported); line("\n"); }

            kal_fs_remove(d, "a", 1);
            kal_fs_remove(d, "b", 1);
            kal_fs_close_dir(d);
        }
        observe(kind::behaviour, kal_fs_remove(here(), kDir, length(kDir)) == kal_ok,
                "the directory is removed");
    }

    // A name held open is removed at once (clause 7.14, version 0.17), where
    // the volume claims it: the name is absent, the file stays usable, and the
    // directory that held it is empty. An implementation that deferred the
    // removal to the close left a C++ program's `remove_all' unable to remove
    // a tree holding its own lock file.
    if ((kal_fs_props(here()) & kal::fs::remove_open.bits) != 0) {
        const char* kHeld = "okc-conformance-held.tmp";
        kal_fs_remove(here(), kHeld, length(kHeld));
        kal_fs_mkdir(here(), kHeld, length(kHeld));
        kal_dir d{};
        if (kal_fs_open_dir(here(), kHeld, length(kHeld), &d) == kal_ok) {
            kal_file f{};
            const auto flags = kal::fs::open::read | kal::fs::open::write | kal::fs::open::create;
            if (kal::fs::open_file(d, "h", 1, flags, &f) == kal_ok) {
                const int removed = kal_fs_remove(d, "h", 1);
                kal_node_info gone = fresh();
                const int ge = kal_fs_info(d, "h", 1, kal::fs::no_resolve, kal::fs::field::all, &gone);
                observe(kind::behaviour, removed == kal_ok && ge == kal_ok && gone.kind == kal_node_absent,
                        "a file removed while it is open loses its name at once");
                observe(kind::behaviour, write_bytes(f, "held", 4),
                        "and stays usable through the handle that holds it");
                kal_fs_close_dir(d);
                d = {};
                observe(kind::behaviour, kal_fs_remove(here(), kHeld, length(kHeld)) == kal_ok,
                        "and the directory that held it is removed");
                kal_fs_close_file(f);
            }
            if (d.h) kal_fs_close_dir(d);
        }
        kal_fs_remove(here(), kHeld, length(kHeld));
    } else {
        unobserved(kind::behaviour, "a name held open is removed at once",
                   "the volume does not claim KAL_FS_PROP_REMOVE_OPEN");
    }

    // THE NAME WITH EVERY LINK FOLLOWED (0.23): absolute, the same for a file
    // reached through a link as reached directly, and absent for a name that is
    // not there; refused on a volume that does not claim it.
    {
        const char* kResolvedLink = "okc-conformance-resolved.tmp";
        const char* kMissing      = "okc-conformance-missing.tmp";
        char a[4096], b[4096];
        if ((kal_fs_props(here()) & kal::fs::resolved_name.bits) != 0) {
            put_file(kName, "x");
            const kal_intptr na = kal_fs_resolved_name(here(), kName, length(kName), a, sizeof a);
            observe(kind::behaviour, na > 0 && na < static_cast<kal_intptr>(sizeof a),
                    "a file's name with every link followed is given");
            const bool absolute = na > 0 && (a[0] == '/' || a[0] == '\\' || (na > 2 && a[1] == ':'));
            bool ends = na >= static_cast<kal_intptr>(length(kName));
            for (kal_uintptr i = 0; ends && i < length(kName); ++i)
                ends = a[na - static_cast<kal_intptr>(length(kName)) + static_cast<kal_intptr>(i)] == kName[i];
            observe(kind::behaviour, absolute && ends, "and it is absolute, and ends with the file's own name");
            if ((kal_fs_props(here()) & kal::fs::make_links.bits) != 0) {
                kal_fs_remove(here(), kResolvedLink, length(kResolvedLink));
                if (kal_fs_link_create(here(), kResolvedLink, length(kResolvedLink), kName, length(kName), 0) == kal_ok) {
                    const kal_intptr nb = kal_fs_resolved_name(here(), kResolvedLink, length(kResolvedLink), b, sizeof b);
                    bool same = nb == na;
                    for (kal_intptr i = 0; same && i < na; ++i) same = a[i] == b[i];
                    observe(kind::behaviour, same, "a link to the file is given the file's name");
                    kal_fs_remove(here(), kResolvedLink, length(kResolvedLink));
                }
            } else {
                unobserved(kind::behaviour, "a link to the file is given the file's name",
                           "the volume does not make links");
            }
            observe(kind::behaviour,
                    kal_fs_resolved_name(here(), kMissing, length(kMissing), a, sizeof a) == -kal_err_not_found,
                    "a name that is not there is absent");
        } else {
            observe(kind::behaviour,
                    kal_fs_resolved_name(here(), kName, length(kName), a, sizeof a) == -kal_err_not_supported,
                    "a resolved name is refused on a volume that does not claim it");
        }
    }

    // Renaming, and that the old name is then absent.
    {
        put_file(kName, "x");
        kal_fs_remove(here(), kOther, length(kOther));
        observe(kind::behaviour,
                kal_fs_rename(here(), kName, length(kName), here(), kOther, length(kOther)) == kal_ok,
                "a name is renamed");
        kal_node_info info = fresh();
        observe(kind::behaviour,
                kal_fs_info(here(), kName, length(kName), 0, kal::fs::field::all, &info) == kal_ok
                    && info.kind == kal_node_absent,
                "the name it was renamed from is then absent");
        kal_fs_remove(here(), kOther, length(kOther));
    }

    // A rename that does not replace (version 0.17): refused with
    // kal_err_exists and changing nothing when the name is there; a rename
    // when it is not. Asked of the volume first, as clause 6.2 says.
    if ((kal_fs_props(here()) & kal::fs::rename_no_replace.bits) != 0) {
        put_file(kName, "first");
        put_file(kOther, "second");
        observe(kind::behaviour,
                kal_fs_rename_flags(here(), kName, length(kName), here(), kOther, length(kOther),
                                    kal::fs::no_replace.bits) == kal_err_exists,
                "a rename that does not replace is refused when the name is there");
        char buf[16] = {};
        observe(kind::behaviour,
                read_file(kOther, buf, sizeof buf) == 6 && buf[0] == 's'
                    && read_file(kName, buf, sizeof buf) == 5,
                "and changes neither name");
        kal_fs_remove(here(), kOther, length(kOther));
        observe(kind::behaviour,
                kal_fs_rename_flags(here(), kName, length(kName), here(), kOther, length(kOther),
                                    kal::fs::no_replace.bits) == kal_ok,
                "a rename that does not replace renames when the name is free");
        kal_fs_remove(here(), kOther, length(kOther));
    } else {
        unobserved(kind::behaviour, "a rename that does not replace",
                   "the volume does not claim KAL_FS_PROP_RENAME_NO_REPLACE");
    }
    {
        put_file(kName, "x");
        observe(kind::behaviour,
                kal_fs_rename_flags(here(), kName, length(kName), here(), kOther, length(kOther),
                                    (kal_uintptr)1u << 30) == kal_err_invalid,
                "a rename flag the specification has not assigned is refused");
        kal_fs_remove(here(), kName, length(kName));
        kal_fs_remove(here(), kOther, length(kOther));
    }

    // --- what an enquiry answers, and what it does not --------------------
    //
    // THREE MECHANISMS, THREE OBSERVATIONS. The size the caller states, the
    // fields the implementation filled, and the fields the caller asked for are
    // three different questions, and the defect this shape replaces was a
    // structure that answered none of them.
    {
        observe(kind::behaviour, kal_fs_max_name() >= 255,
                "the greatest name this implementation accepts is stated and is usable");

        put_file(kName, "0123456789");

        // The implementation reports what it filled. `kind` is the point of the
        // enquiry, so an implementation that reported none of it is one that
        // answered nothing.
        kal_node_info full = fresh();
        const int e = kal_fs_info(here(), kName, length(kName), 0, kal::fs::field::all, &full);
        observe(kind::behaviour, e == kal_ok && (full.present & kal::fs::field::kind) != 0,
                "an enquiry reports which of the fields it filled");
        observe(kind::behaviour, e == kal_ok
                    && (full.present & ~kal::fs::field::all) == 0,
                "it reports no position the specification has not assigned");

        // AN IMPLEMENTATION WRITES NO MORE THAN THE CALLER SAID EXISTS. A
        // consumer built against a later revision holds a larger structure than
        // an earlier implementation knows; a consumer built against an earlier
        // one holds a smaller structure than a later implementation would fill,
        // and THAT is the direction that corrupts the caller. The observation
        // is made with a guard beyond a deliberately understated size.
        {
            struct { kal_node_info info; unsigned char guard[64]; } probe{};
            for (auto& c : probe.guard) c = 0x7f;
            probe.info.self_size = static_cast<kal_u32>(
                __builtin_offsetof(kal_node_info, modified_ns));
            const int se = kal_fs_info(here(), kName, length(kName), 0,
                                       kal::fs::field::all, &probe.info);
            bool untouched = true;
            for (auto c : probe.guard) if (c != 0x7f) untouched = false;
            bool tail_untouched = true;
            const auto* raw = reinterpret_cast<const unsigned char*>(&probe.info);
            for (kal_uintptr i = probe.info.self_size; i < sizeof(kal_node_info); ++i)
                if (raw[i] != 0) tail_untouched = false;
            observe(kind::behaviour, se == kal_ok && untouched && tail_untouched,
                    "an enquiry writes no more of the structure than the caller stated");
        }

        // The identity is comparable and nothing else. Two enquiries about one
        // name agree; two names that were separately created do not.
        if ((full.present & kal::fs::field::identity) != 0) {
            kal_node_info again = fresh();
            kal_fs_info(here(), kName, length(kName), 0, kal::fs::field::identity, &again);
            observe(kind::behaviour,
                    again.identity[0] == full.identity[0]
                        && again.identity[1] == full.identity[1],
                    "one node has the same identity on a second enquiry");

            put_file(kOther, "x");
            kal_node_info other = fresh();
            kal_fs_info(here(), kOther, length(kOther), 0, kal::fs::field::identity, &other);
            observe(kind::behaviour,
                    other.identity[0] != full.identity[0]
                        || other.identity[1] != full.identity[1],
                    "two nodes that are not the same node have different identities");
            kal_fs_remove(here(), kOther, length(kOther));
        } else {
            unobserved(kind::behaviour, "a node's identity distinguishes it from another",
                       "the implementation does not report an identity for this resource");
        }
        kal_fs_remove(here(), kName, length(kName));
    }

    // --- nodes whose content is another name ------------------------------
    //
    // WHETHER THIS VOLUME HAS THEM IS ASKED FIRST, WHICH IS WHY THE ENQUIRY
    // TAKES THE DIRECTORY. The same implementation succeeds on one volume and
    // fails on another, so a word per implementation could state neither
    // honestly, and an operation that cannot be performed here is not clause
    // 6.2's defect precisely because a caller is able to ask.
    {
        const kal_uintptr p = kal_fs_props(here());
        const char* kLink   = "okc-conformance-link.tmp";
        kal_fs_remove(here(), kLink, length(kLink));

        if ((p & kal::fs::make_links.bits) != 0) {
            put_file(kName, "0123456789");
            const int made = kal_fs_link_create(here(), kLink, length(kLink),
                                                kName, length(kName), 0);
            observe(kind::behaviour, made == kal_ok,
                    "a node whose content is another name is made where the volume has them");

            char target[512];
            const kal_intptr n = kal_fs_link_read(here(), kLink, length(kLink),
                                                  target, sizeof target);
            bool matches = n == static_cast<kal_intptr>(length(kName));
            for (kal_intptr i = 0; matches && i < n; ++i)
                if (target[i] != kName[i]) matches = false;
            observe(kind::behaviour, matches, "its content reads back as it was written");

            // THE OBSERVATION THE WHOLE OF THIS EXISTS FOR. Asking resolves
            // and opening resolves, so the two agree; asking with
            // KAL_FS_NO_RESOLVE reports the node itself. An implementation in
            // which asking did not resolve while opening did reported a link
            // where a caller would have reached a file, and one such node made
            // a whole tree uncopyable for a C library above it.
            kal_node_info followed = fresh(), itself = fresh();
            const int fe = kal_fs_info(here(), kLink, length(kLink), 0,
                                       kal::fs::field::all, &followed);
            const int ie = kal_fs_info(here(), kLink, length(kLink),
                                       kal::fs::no_resolve, kal::fs::field::all, &itself);
            observe(kind::behaviour,
                    fe == kal_ok && followed.kind == kal_node_file && followed.size == 10,
                    "an enquiry resolves, and reports what the name finally refers to");
            observe(kind::behaviour, ie == kal_ok && itself.kind == kal_node_link,
                    "an enquiry that declines to resolve reports the node itself");

            // A name that finally refers to nothing is answered, not refused.
            const char* kDangling = "okc-conformance-dangling.tmp";
            kal_fs_remove(here(), kDangling, length(kDangling));
            if (kal_fs_link_create(here(), kDangling, length(kDangling),
                                   "okc-no-such-target", 18, 0) == kal_ok) {
                kal_node_info gone = fresh();
                const int ge = kal_fs_info(here(), kDangling, length(kDangling), 0,
                                           kal::fs::field::all, &gone);
                observe(kind::behaviour, ge == kal_ok && gone.kind == kal_node_absent,
                        "a name that finally refers to nothing is reported as absent");
                kal_fs_remove(here(), kDangling, length(kDangling));
            }

            // The length is the content's, so a caller may size before it copies.
            const kal_intptr sized = kal_fs_link_read(here(), kLink, length(kLink), nullptr, 0);
            observe(kind::behaviour, sized == static_cast<kal_intptr>(length(kName)),
                    "a capacity of zero reports the content's length without writing");

            // THE NAME IS RENAMED AND REMOVED, NOT WHAT IT LEADS TO. An
            // implementation that followed the link removed the file it named
            // and left the link, and could not remove a link to a directory
            // at all; a C++ program's `remove_all' then failed on the one
            // environment that did so.
            const char* kMoved = "okc-conformance-link-moved.tmp";
            kal_fs_remove(here(), kMoved, length(kMoved));
            const int moved = kal_fs_rename(here(), kLink, length(kLink), here(), kMoved, length(kMoved));
            kal_node_info after = fresh();
            kal_fs_info(here(), kMoved, length(kMoved), kal::fs::no_resolve, kal::fs::field::all, &after);
            observe(kind::behaviour, moved == kal_ok && after.kind == kal_node_link,
                    "renaming a link renames the link");
            const int removed = kal_fs_remove(here(), kMoved, length(kMoved));
            kal_node_info target_after = fresh();
            const int te = kal_fs_info(here(), kName, length(kName), 0, kal::fs::field::all, &target_after);
            observe(kind::behaviour, removed == kal_ok && te == kal_ok && target_after.kind == kal_node_file,
                    "removing a link removes the link and leaves the file it named");

            // And a link to a directory, which one environment makes as a
            // different kind of node and which is removed as its name too.
            const char* kDir     = "okc-conformance-linked-dir.tmp";
            const char* kDirLink = "okc-conformance-dir-link.tmp";
            kal_fs_remove(here(), kDirLink, length(kDirLink));
            kal_fs_mkdir(here(), kDir, length(kDir));
            {
                if (kal_fs_link_create(here(), kDirLink, length(kDirLink), kDir, length(kDir),
                                       kal::fs::link_directory) == kal_ok) {
                    const int gone = kal_fs_remove(here(), kDirLink, length(kDirLink));
                    kal_node_info dir_after = fresh();
                    const int de = kal_fs_info(here(), kDir, length(kDir), 0, kal::fs::field::all, &dir_after);
                    observe(kind::behaviour, gone == kal_ok && de == kal_ok && dir_after.kind == kal_node_directory,
                            "removing a link to a directory removes the link and leaves the directory");
                }
                kal_fs_remove(here(), kDir, length(kDir));
            }

            kal_fs_remove(here(), kName, length(kName));
        } else {
            unobserved(kind::behaviour,
                       "a node whose content is another name is made and read",
                       "this volume does not have them, which the enquiry reports");
            // AND THE REFUSAL IS OBSERVED RATHER THAN ASSUMED. An
            // implementation that did not claim the position and then performed
            // the operation anyway would be claiming less than it does, which
            // misleads a caller in the direction of doing without.
            observe(kind::behaviour,
                    kal_fs_link_create(here(), kLink, length(kLink), "x", 1, 0) != kal_ok,
                    "an operation the enquiry did not claim is refused");
        }
    }

    // --- exclusion upon a range of a file, version 0.10 ---------------------
    //
    // THE OBSERVATION THAT TELLS THE TWO FORMS OF THIS APART NEEDS NO SECOND
    // PROGRAM, AND THAT IS WHY IT IS WRITTEN THIS WAY.
    //
    // openkal states the holder as the open FILE. One environment's oldest form
    // holds it by the PROCESS and releases every lock upon a node as soon as the
    // program closes any descriptor for it --- so a library that opened one file
    // twice destroyed its own lock. An implementation built on that form passes
    // "a lock can be taken" and "it can be released" and fails only here: a
    // SECOND OPEN FILE of one name, in this program, must be refused.
    if (performs(kind::behaviour)) {
        if ((kal_fs_props(here()) & kal::fs::locks.bits) != 0) {
            put_file(kName, "xxxx");
            kal_file a{}, b{};
            const int oa = kal::fs::open_file(here(), kName, length(kName),
                                              kal::fs::open::read | kal::fs::open::write, &a);
            const int taken = oa == kal_ok
                ? kal::fs::lock_range(a, 0, 0, kal::fs::lock::exclusive) : oa;
            observe(kind::behaviour, taken == kal_ok,
                    "an exclusive lock upon a whole file is taken");

            const int ob = kal::fs::open_file(here(), kName, length(kName),
                                              kal::fs::open::read | kal::fs::open::write, &b);
            const int second = ob == kal_ok
                ? kal::fs::lock_range(b, 0, 0, kal::fs::lock::exclusive) : ob;
            observe(kind::behaviour, second == kal_err_again,
                    "and a second open file of the same name is refused, not granted");

            const int freed = taken == kal_ok ? kal_fs_unlock(a, 0, 0) : kal_err_invalid;
            observe(kind::behaviour, freed == kal_ok, "the lock is released");
            const int again = ob == kal_ok
                ? kal::fs::lock_range(b, 0, 0, kal::fs::lock::exclusive) : ob;
            observe(kind::behaviour, again == kal_ok,
                    "and once released, another open file may take it");
            if (again == kal_ok) kal_fs_unlock(b, 0, 0);

            // Neither kind and both kinds are the same mistake: a caller that
            // asked for neither did not say what it wanted.
            observe(kind::behaviour,
                    oa != kal_ok || kal_fs_lock(a, 0, 0, 0) == kal_err_invalid,
                    "asking for a lock that is neither shared nor exclusive is refused");

            if (ob == kal_ok) kal_fs_close_file(b);
            if (oa == kal_ok) kal_fs_close_file(a);
            kal_fs_remove(here(), kName, length(kName));
        } else {
            unobserved(kind::behaviour,
                       "an exclusive lock upon a whole file is taken",
                       "the implementation does not claim prop_locks for this volume");
        }

        // --- how much the volume holds --------------------------------------
        if ((kal_fs_props(here()) & kal::fs::capacity.bits) != 0) {
            kal_u64 total = 0, available = 0;
            const int e = kal_fs_capacity(here(), &total, &available);
            observe(kind::behaviour, e == kal_ok && total > 0,
                    "the volume reports how much it holds");
            observe(kind::behaviour, e == kal_ok && available <= total,
                    "and what is available is no more than that");
            // Either pointer may be null, for a caller that wants one of the two.
            kal_u64 one = 0;
            observe(kind::behaviour,
                    kal_fs_capacity(here(), nullptr, &one) == kal_ok,
                    "and a caller may ask for one of the two");
        } else {
            unobserved(kind::behaviour, "the volume reports how much it holds",
                       "the implementation does not claim prop_capacity for this volume");
        }

        // --- the modification time of a NAME, including a directory ----------
        //
        // THE DIRECTORY IS THE POINT. `kal_fs_set_modified' takes a `kal_file'
        // and a directory is a `kal_dir', so before this declaration there was no
        // route to a directory's time at all --- and an implementation reached one
        // anyway, outside anything this specification stated.
        if ((kal_fs_props(here()) & kal::fs::modified_time.bits) != 0) {
            kal_fs_mkdir(here(), kDir, length(kDir));
            const kal_u64 chosen = 1600000000ull * 1000000000ull;
            const int e = kal_fs_set_modified_at(here(), kDir, length(kDir), chosen);
            kal_node_info after = fresh();
            const int read_back =
                kal_fs_info(here(), kDir, length(kDir), 0, kal::fs::field::all, &after);
            observe(kind::behaviour,
                    e == kal_ok && read_back == kal_ok
                        && after.modified_ns / 1000000000u == chosen / 1000000000u,
                    "the time a DIRECTORY reports as its last modification is set by name");
            kal_fs_remove(here(), kDir, length(kDir));

            put_file(kName, "x");
            const int fe = kal_fs_set_modified_at(here(), kName, length(kName), chosen);
            kal_node_info fa = fresh();
            const int fr = kal_fs_info(here(), kName, length(kName), 0, kal::fs::field::all, &fa);
            observe(kind::behaviour,
                    fe == kal_ok && fr == kal_ok
                        && fa.modified_ns / 1000000000u == chosen / 1000000000u,
                    "and so is a file's, by the same operation");
            kal_fs_remove(here(), kName, length(kName));
        } else {
            unobserved(kind::behaviour,
                       "the time a DIRECTORY reports as its last modification is set by name",
                       "the implementation does not claim prop_modified_time");
        }
        // --- whether a node may be started, version 0.13 ---------------------
        //
        // A property of the node, answered per volume. Where the volume records
        // it, it is set, read back and cleared by name; where it does not, the
        // operation is refused and the enquiry does not report it. A directory
        // is refused in either case, because on the environments that store the
        // property for a directory it means something else.
        {
            const char* kRunnable = "okc-conformance-runnable.tmp";
            put_file(kRunnable, "not a program\n");
            if ((kal_fs_props(here()) & kal::fs::executable.bits) != 0) {
                const int on = kal_fs_set_executable_at(here(), kRunnable,
                                                        length(kRunnable), 1);
                kal_node_info a = fresh();
                const int ra = kal_fs_info(here(), kRunnable, length(kRunnable), 0,
                                           kal::fs::field::all, &a);
                observe(kind::behaviour,
                        on == kal_ok && ra == kal_ok
                            && (a.present & kal::fs::field::executable) != 0
                            && a.executable != 0,
                        "a node is recorded as one that may be started, by name");

                const int off = kal_fs_set_executable_at(here(), kRunnable,
                                                         length(kRunnable), 0);
                kal_node_info b = fresh();
                const int rb = kal_fs_info(here(), kRunnable, length(kRunnable), 0,
                                           kal::fs::field::executable, &b);
                observe(kind::behaviour,
                        off == kal_ok && rb == kal_ok
                            && (b.present & kal::fs::field::executable) != 0
                            && b.executable == 0,
                        "and the record is cleared by the same operation");

                // A caller whose structure ends before the field is not given
                // the position, and is not written to beyond what it stated.
                kal_node_info old{};
                old.self_size = static_cast<kal_u32>(
                    __builtin_offsetof(kal_node_info, executable));
                const int ro = kal_fs_info(here(), kRunnable, length(kRunnable), 0,
                                           kal::fs::field::all, &old);
                observe(kind::behaviour,
                        ro == kal_ok && (old.present & kal::fs::field::executable) == 0
                            && old.executable == 0,
                        "a caller whose structure predates the field is not given it");
            } else {
                unobserved(kind::behaviour,
                           "a node is recorded as one that may be started, by name",
                           "the implementation does not claim prop_executable for this volume");
                observe(kind::behaviour,
                        kal_fs_set_executable_at(here(), kRunnable, length(kRunnable), 1)
                            == kal_err_not_supported,
                        "recording a property the volume does not store is refused");
                kal_node_info c = fresh();
                kal_fs_info(here(), kRunnable, length(kRunnable), 0,
                            kal::fs::field::all, &c);
                observe(kind::behaviour, (c.present & kal::fs::field::executable) == 0,
                        "and the enquiry does not report a property the volume does not store");
            }
            kal_fs_remove(here(), kRunnable, length(kRunnable));

            kal_fs_mkdir(here(), kDir, length(kDir));
            const int d = kal_fs_set_executable_at(here(), kDir, length(kDir), 1);
            observe(kind::behaviour,
                    d == kal_err_is_directory || d == kal_err_not_supported,
                    "a directory is not recorded as a program");
            kal_fs_remove(here(), kDir, length(kDir));
        }
    }

    if (performs(kind::abi)) {
        observe(kind::abi, sizeof(kal_dir) == sizeof(kal_uintptr)
                        && sizeof(kal_file) == sizeof(kal_uintptr),
                "a directory and a file handle each occupy one machine word");
        const kal_uintptr assigned = (kal::fs::case_sensitive | kal::fs::links
                                    | kal::fs::modified_time | kal::fs::atomic_rename
                                    | kal::fs::make_links
                                    | kal::fs::locks | kal::fs::capacity
                                    | kal::fs::executable | kal::fs::rename_no_replace
                                    | kal::fs::local_endpoint | kal::fs::remove_open
                                    | kal::fs::resolved_name | kal::fs::attenuate
                                    | kal::fs::read_only | kal::fs::unchanging
                                    | kal::fs::list_info).bits;
        observe(kind::abi, (kal_fs_props(here()) & ~assigned) == 0,
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
            kal_node_info info = fresh();
            observe(kind::abi, kal_fs_file_info(released, kal::fs::field::all, &info) != kal_ok,
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

    listing_with_attributes();
    attenuation();
#endif
}

}  // namespace okc::fs
