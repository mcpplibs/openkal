module okc.suite;

import okc.spec;
import okc.report;
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

namespace okc {

int run_all() {
    write_inventory();

    // The order is clause 3's, so that two runs against different
    // implementations can be read side by side without either being sorted.
    // openkal.abort is last among the core interfaces rather than first,
    // because observing it requires starting a copy and the copy's own report
    // would otherwise appear before this one's heading.
    // First, because it is what a consumer asks before it uses anything, and
    // because it is the one section no arrangement skips.
    version::run();
    stream::run();
    memory::run();
    env::run();
    time::run();
    random::run();
    fs::run();
    process::run();
    task::run();
    exec::run();
    terminal::run();
    net::run();
    datagram::run();
    space::run();
    timeout::run();
    termination::run();

    return summarise();
}

}  // namespace okc
