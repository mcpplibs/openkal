module okc.suite;

import okc.spec;
import okc.report;
import okc.protocol;
import okc.version;
import okc.abort;
import okc.stream;
import okc.memory;
import okc.env;
import okc.time;
import okc.random;
import okc.fs;
import okc.process;
import okc.task;
import okc.exec;
import okc.terminal;
import okc.net;
import okc.datagram;
import okc.space;
import okc.timeout;
import okc.map;
import okc.region;
import okc.prof;
import okc.debug;

namespace okc {

namespace {

// The cases a runner lists and selects (the mtest protocol): one per section,
// in the order the sections run. A section's case begins at its heading.
// `measures': the section reports its cost (kind::cost), which a runner selects
// as the case `<id>.cost' and therefore has to find listed.
struct section_entry { const char* id; void (*run)(); bool measures; };
constexpr section_entry sections[] = {
    { "openkal.version",  version::run, false },
    { "openkal.stream",   stream::run, true },
    { "openkal.memory",   memory::run, true },
    { "openkal.env",      env::run, false },
    { "openkal.time",     time::run, true },
    { "openkal.random",   random::run, false },
    { "openkal.fs",       fs::run, true },
    { "openkal.process",  process::run, true },
    { "openkal.task",     task::run, true },
    { "openkal.exec",     exec::run, false },
    { "openkal.terminal", terminal::run, false },
    { "openkal.net",      net::run, false },
    { "openkal.datagram", datagram::run, false },
    { "openkal.space",    space::run, false },
    { "openkal.timeout",  timeout::run, false },
    { "openkal.map",      map::run, false },
    { "openkal.region",   region::run, true },
    { "openkal.prof",     prof::run, true },
    { "openkal.debug",    debug::run, false },
    { "openkal.abort",    termination::run, false },
};

}  // namespace

int run_all() {
    protocol::begin();
    if (protocol::listing()) {
        for (const section_entry& e : sections) protocol::list_case(e.id, e.measures && performs(kind::cost));
        return protocol::finish(0, 0, 0);
    }

    write_inventory();
    for (const section_entry& e : sections)
        if (protocol::wanted(e.id)) e.run();

    return summarise();
}

}  // namespace okc
