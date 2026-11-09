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
struct section_entry { const char* id; void (*run)(); };
constexpr section_entry sections[] = {
    { "openkal.version",  version::run },
    { "openkal.stream",   stream::run },
    { "openkal.memory",   memory::run },
    { "openkal.env",      env::run },
    { "openkal.time",     time::run },
    { "openkal.random",   random::run },
    { "openkal.fs",       fs::run },
    { "openkal.process",  process::run },
    { "openkal.task",     task::run },
    { "openkal.exec",     exec::run },
    { "openkal.terminal", terminal::run },
    { "openkal.net",      net::run },
    { "openkal.datagram", datagram::run },
    { "openkal.space",    space::run },
    { "openkal.timeout",  timeout::run },
    { "openkal.map",      map::run },
    { "openkal.region",   region::run },
    { "openkal.prof",     prof::run },
    { "openkal.debug",    debug::run },
    { "openkal.abort",    termination::run },
};

}  // namespace

int run_all() {
    protocol::begin();
    if (protocol::listing()) {
        for (const section_entry& e : sections) protocol::list_case(e.id);
        return protocol::finish(0, 0, 0);
    }

    write_inventory();
    for (const section_entry& e : sections)
        if (protocol::wanted(e.id)) e.run();

    return summarise();
}

}  // namespace okc
