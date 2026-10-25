#!/usr/bin/env bash
# Ecosystem verification for openkal 0.16.0 and the packages that move with it
# (openkal-linux 0.17.0, openkal-macos 0.14.1, openkal-windows 0.12.0,
# openkal-emscripten 0.5.0, openkal-opensbi 0.8.3, openkal-musl 0.21.1,
# openkal-llvm-runtime 0.15.6), resolved from the published index only.
#
#   xlings subos new okl016
#   cp <this file> ~/.xlings/subos/okl016/tmp/v.sh
#   xlings subos use okl016 --sandbox --cmd "MCPP_VERIFY_VERSION=<mcpp> bash /tmp/v.sh"
#
# The subject is what a consumer gets by naming the released versions, and the
# programs are the reporters' own: the directory scan of mcpplibs/openkal-linux#34,
# the recursion of mcpplibs/openkal-musl#48, and the raw-mode reader of
# mcpplibs/openkal#48. A step that cannot run here says so and is counted as not
# run, never as passed.
set -u

VER="${MCPP_VERIFY_VERSION:?set MCPP_VERIFY_VERSION}"
STORE="${MCPP_VERIFY_BIN:-$HOME/.xlings/data/xpkgs/xim-x-mcpp/$VER/bin/mcpp}"
XL="${XLINGS_BIN:-$(command -v xlings)}"

fails=0; skipped=""
fail()    { printf 'ASSERT-FAIL: %s\n' "$1"; fails=$((fails + 1)); }
ok()      { printf 'ok: %s\n' "$1"; }
section() { printf '\n== %s ==\n' "$1"; }
skip()    { printf 'NOT RUN: %s\n' "$1"; skipped="$skipped
  - $1"; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# A pseudo terminal, typed at once the program has said `reading'. The same
# harness openkal-musl's continuous integration uses (tools/pty-keys.py).
cat > "$work/keys.py" <<'PY'
import os, pty, select, sys, time
keys, cmd = bytes.fromhex(sys.argv[1]), sys.argv[2:]
pid, fd = pty.fork()
if pid == 0:
    os.execvp(cmd[0], cmd)
out, typed, deadline = bytearray(), False, time.monotonic() + 30
while time.monotonic() < deadline:
    r, _, _ = select.select([fd], [], [], 0.2)
    if r:
        try: chunk = os.read(fd, 4096)
        except OSError: break
        if not chunk: break
        out += chunk
        if not typed and b"reading" in out:
            time.sleep(0.2); os.write(fd, keys); typed = True
    elif typed and os.waitpid(pid, os.WNOHANG)[0] == pid:
        break
sys.stdout.write(out.decode("latin-1"))
PY

# -- A. identity and mirror ---------------------------------------------------
section "A. identity and mirror"
got=$("$STORE" --version 2>&1 | head -1)
[ "$got" = "mcpp $VER" ] && ok "$got from $STORE" || fail "version is '$got' at $STORE"
# In a SubOS of its own the mirror is configured; anywhere else it is chosen for
# these calls alone (MCPP_VERIFY_NO_CONFIG=1), so the environment is not changed.
if [ "${MCPP_VERIFY_NO_CONFIG:-0}" = 1 ]; then
    export MCPP_INDEX_MIRROR=CN
    ok "the CN mirror is selected for this run only"
else
    "$STORE" self config --mirror CN >/dev/null 2>&1 || true
    "$XL" config --mirror CN >/dev/null 2>&1 || true
    xm=$(python3 -c "import json,os;print(json.load(open(os.path.expanduser('~/.xlings/.xlings.json'))).get('mirror',''))" 2>/dev/null)
    [ "$xm" = "CN" ] && ok "xlings mirror is CN" || fail "xlings mirror is '$xm'"
fi
"$STORE" index update >/dev/null 2>&1 || true

# -- B. the implementation beneath -------------------------------------------
section "B. openkal 0.16.0 and openkal-linux 0.17.0: a least stack size, an enumeration ended by its close, unaltered input"
mkdir -p "$work/impl/src"
cat > "$work/impl/mcpp.toml" <<'TOML'
[package]
name = "implprobe"
version = "0.1.0"

[dependencies]
openkal = "0.16.0"

[target.'cfg(os = "linux")'.dependencies]
openkal-linux = "0.17.0"

[targets.implprobe]
kind = "bin"
main = "src/main.c"
TOML
cat > "$work/impl/src/main.c" <<'C'
#define _GNU_SOURCE 1
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openkal/fs.h>
#include <openkal/stream.h>
#include <openkal/task.h>
#include <openkal/terminal.h>
#include <openkal/version.h>

static long recurse(long n);
static long (*volatile g_recurse)(long) = recurse;
static long recurse(long n) { volatile char pad[1000]; pad[0] = (char)n; return n ? g_recurse(n - 1) + pad[0] : 0; }

struct probe { kal_uintptr told; int done; };
static void room(void* p) {
    struct probe* q = p; void* base = 0; kal_uintptr size = 0;
    if (kal_task_stack(&base, &size) == kal_ok) q->told = size;
    g_recurse(12 * 1024);
    q->done = 1;
}

static int fds(void) {
    DIR* d = opendir("/proc/self/fd"); int n = 0;
    if (!d) return -1;
    while (readdir(d)) n++;
    closedir(d);
    return n;
}

int main(void) {
    printf("version %u.%u.%u\n", KAL_VERSION_MAJOR, KAL_VERSION_MINOR, KAL_VERSION_PATCH);

    struct probe q = {0, 0};
    struct kal_task t;
    const int e = kal_task_start_sized(room, &q, 16u << 20, &t);
    if (e == kal_ok) kal_task_join(t);
    printf("sized e=%d told>=16MiB=%d room=%d\n", e, q.told >= (16u << 20), q.done);

    struct kal_dir root;
    kal_fs_preopen(0, &root, 0, 0, 0);
    kal_fs_mkdir(root, "v016.d", 6);
    const int before = fds();
    for (int i = 0; i < 64; i++) {
        struct kal_dir d; kal_uintptr it = 0;
        if (kal_fs_open_dir(root, "v016.d", 6, &d) != kal_ok) break;
        if (kal_fs_list_begin(d, &it) == kal_ok) { char n[64]; kal_uintptr l; int k; kal_fs_list_next(d, &it, n, sizeof n, &l, &k); }
        kal_fs_close_dir(d);
    }
    printf("enumeration before=%d after=%d\n", before, fds());
    kal_fs_remove(root, "v016.d", 6);

    const int m = posix_openpt(O_RDWR | O_NOCTTY);
    if (m < 0 || grantpt(m) || unlockpt(m)) { printf("pty none\n"); return 0; }
    const int s = open(ptsname(m), O_RDWR | O_NOCTTY);
    struct kal_stream st = { (kal_uintptr)s };
    kal_terminal_set_mode(st, KAL_TERM_PASS_CONTROL | KAL_TERM_PASS_INPUT);
    kal_uintptr mode = 0; kal_terminal_get_mode(st, &mode);
    write(m, "\r\n", 2);
    unsigned char b[2] = {0, 0}; int got = 0;
    while (got < 2) { kal_intptr r = kal_stream_read(st, b + got, 2 - got); if (r <= 0) break; got += (int)r; }
    printf("pty position=%d bytes=%02x %02x\n", (mode & KAL_TERM_PASS_INPUT) != 0, b[0], b[1]);
    return 0;
}
C
out=$(cd "$work/impl" && "$STORE" run 2>&1); rc=$?
if [ $rc -ne 0 ] || ! printf '%s\n' "$out" | grep -q '^version '; then
    fail "the implementation probe did not build or run"; printf '%s\n' "$out" | tail -8
else
    printf '%s\n' "$out" | grep -q '^version 0.16.0$' && ok "version 0.16.0" || fail "$(printf '%s\n' "$out" | grep '^version')"
    l=$(printf '%s\n' "$out" | grep '^sized ')
    [ "$l" = "sized e=0 told>=16MiB=1 room=1" ] && ok "$l" || fail "the sized start reported: $l"
    l=$(printf '%s\n' "$out" | grep '^enumeration ')
    b=$(printf '%s\n' "$l" | sed -n 's/.*before=\([0-9-]*\).*/\1/p'); a=$(printf '%s\n' "$l" | sed -n 's/.*after=\([0-9-]*\).*/\1/p')
    [ -n "$b" ] && [ "$b" = "$a" ] && [ "$b" -gt 0 ] && ok "$l" || fail "the enumeration probe reported: $l"
    l=$(printf '%s\n' "$out" | grep '^pty ')
    if [ "$l" = "pty none" ]; then skip "this sandbox offers no pseudo terminal to the program"
    else [ "$l" = "pty position=1 bytes=0d 0a" ] && ok "$l" || fail "the terminal probe reported: $l"; fi
fi
grep -q '0.17.0' "$work/impl/mcpp.lock" 2>/dev/null && ok "the lock records openkal-linux 0.17.0" || fail "the lock does not record openkal-linux 0.17.0"

# -- C. the C library above it: the reporters' programs -----------------------
section "C. openkal-musl 0.21.1: the reporters' three programs"
mkdir -p "$work/musl/src"
cat > "$work/musl/mcpp.toml" <<'TOML'
[package]
name = "muslprobe"
version = "0.1.0"

[dependencies]
openkal-musl = "0.21.1"

[targets.muslprobe]
kind = "bin"
main = "src/main.c"
TOML
cat > "$work/musl/src/main.c" <<'C'
#define _GNU_SOURCE 1
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

/* mcpplibs/openkal-linux#34, as reported. */
static int open_descriptors(void) {
    DIR* d = opendir("/proc/self/fd"); if (!d) return -1;
    int n = 0; while (readdir(d)) n++; closedir(d); return n;
}
static int dirleak(void) {
    char name[64];
    mkdir("dirleak.d", 0755);
    for (int i = 0; i < 300; i++) { snprintf(name, sizeof name, "dirleak.d/e%03d", i); close(open(name, O_WRONLY | O_CREAT, 0644)); }
    const int before = open_descriptors();
    for (int i = 0; i < 64; i++) { DIR* d = opendir("dirleak.d"); readdir(d); closedir(d); }
    const int after = open_descriptors();
    for (int i = 0; i < 300; i++) { snprintf(name, sizeof name, "dirleak.d/e%03d", i); unlink(name); }
    rmdir("dirleak.d");
    printf("descriptors before=%d after=%d\n", before, after);
    return 0;
}

/* mcpplibs/openkal-musl#48, as reported. */
static long recurse(long n) { volatile char pad[1000]; pad[0] = 1; return n ? recurse(n - 1) + pad[0] : 0; }
static void* body(void* arg) { printf("recursed %ld levels of about 1 KiB\n", recurse(*(long*)arg)); return 0; }
static int stack(long kib, long depth) {
    pthread_attr_t attr; pthread_t t;
    pthread_attr_init(&attr);
    printf("pthread_attr_setstacksize(%ld KiB) -> %d\n", kib, pthread_attr_setstacksize(&attr, kib * 1024));
    printf("pthread_create -> %d\n", pthread_create(&t, &attr, body, &depth));
    pthread_join(t, 0);
    static char region[1 << 20];
    pthread_attr_setstack(&attr, region, sizeof region);
    printf("pthread_create with a supplied stack -> %d\n", pthread_create(&t, &attr, body, &depth));
    return 0;
}

/* mcpplibs/openkal#48, as reported. */
static int icrnl(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    struct termios old, raw, back;
    if (tcgetattr(0, &old) != 0) { perror("tcgetattr"); return 1; }
    raw = old; cfmakeraw(&raw);
    int rc = tcsetattr(0, TCSANOW, &raw);
    tcgetattr(0, &back);
    printf("ICRNL before=%d requested=%d readback=%d (tcsetattr rc=%d)\r\n",
           (old.c_iflag & ICRNL) != 0, (raw.c_iflag & ICRNL) != 0, (back.c_iflag & ICRNL) != 0, rc);
    printf("reading: press Enter, Ctrl+J, then q\r\n");
    for (unsigned char c; read(0, &c, 1) == 1; ) { printf("byte 0x%02x\r\n", c); if (c == 'q') break; }
    tcsetattr(0, TCSANOW, &old);
    return 0;
}

/* mcpplibs/openkal-musl#55: a program duplicated by fork starts a thread. */
#include <sys/wait.h>
static void* nothing(void* p) { return p; }
static int forkthread(void) {
    const pid_t c = fork();
    if (c == 0) { pthread_t t; if (pthread_create(&t, 0, nothing, 0) != 0) _exit(4); pthread_join(t, 0); _exit(3); }
    int st = 0; waitpid(c, &st, 0);
    printf("fork then thread: status=%d\n", WIFEXITED(st) ? WEXITSTATUS(st) : -1);
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && !strcmp(argv[1], "forkthread")) return forkthread();
    if (argc > 1 && !strcmp(argv[1], "dirleak")) return dirleak();
    if (argc > 3 && !strcmp(argv[1], "stack")) return stack(atol(argv[2]), atol(argv[3]));
    if (argc > 1 && !strcmp(argv[1], "icrnl")) return icrnl();
    return 2;
}
C
out=$(cd "$work/musl" && "$STORE" build 2>&1); rc=$?
bin=$(find "$work/musl/target" -type f -name muslprobe 2>/dev/null | head -1)
if [ $rc -ne 0 ] || [ -z "$bin" ]; then
    fail "the musl probe did not build"; printf '%s\n' "$out" | tail -8
else
    l=$(cd "$work" && timeout 60 "$bin" dirleak 2>&1 | grep '^descriptors')
    b=$(printf '%s\n' "$l" | sed -n 's/.*before=\([0-9-]*\).*/\1/p'); a=$(printf '%s\n' "$l" | sed -n 's/.*after=\([0-9-]*\).*/\1/p')
    [ -n "$b" ] && [ "$b" = "$a" ] && [ "$b" -gt 0 ] && ok "#34: $l" || fail "#34 reported: $l"
    for spec in "1024 900" "4096 3900"; do
        set -- $spec
        r=$(timeout 60 "$bin" stack "$1" "$2" 2>&1); s=$?
        printf '%s\n' "$r" | grep -q "^recursed $2 levels" && [ $s -eq 0 ] \
            && ok "musl#48: $1 KiB asked, $2 levels recursed" || { fail "musl#48: $1 KiB / $2 levels (status $s)"; printf '%s\n' "$r" | tail -4; }
        printf '%s\n' "$r" | grep -q "^pthread_create with a supplied stack -> 22$" \
            && ok "musl#48: a supplied stack is refused with EINVAL" || fail "musl#48: a supplied stack was not refused"
    done
    l=$(timeout 60 "$bin" forkthread 2>&1 | grep '^fork then thread')
    [ "$l" = "fork then thread: status=3" ] && ok "musl#55: a program duplicated by fork starts a thread" || fail "musl#55: $l"
    if command -v python3 >/dev/null 2>&1; then
        t=$(python3 "$work/keys.py" 0d0a71 "$bin" icrnl 2>&1 | tr -d '\r')
        printf '%s\n' "$t" | grep -q 'readback=0' && ok "#48: ICRNL reads back cleared after cfmakeraw" || fail "#48: readback: $(printf '%s\n' "$t" | grep ICRNL)"
        bytes=$(printf '%s\n' "$t" | grep '^byte ' | tr '\n' ' ')
        [ "$bytes" = "byte 0x0d byte 0x0a byte 0x71 " ] && ok "#48: Enter, Ctrl+J, q arrive as $bytes" || fail "#48: the bytes were: $bytes"
    else
        skip "#48: no python3 in this sandbox to drive a pseudo terminal"
    fi
fi
grep -q '0.21.1' "$work/musl/mcpp.lock" 2>/dev/null && ok "the lock records openkal-musl 0.21.1" || fail "the lock does not record openkal-musl 0.21.1"

# -- D. the runtime the reporters build against -------------------------------
section "D. openkal-llvm-runtime 0.15.6 on x86_64-linux-musl, the reporters' arrangement"
mkdir -p "$work/rt/src"
cat > "$work/rt/mcpp.toml" <<'TOML'
[package]
namespace = "probe"
name      = "rtprobe"
version   = "0.0.0"

[toolchain]
default = "llvm@23.1.3"

[targets.rtprobe]
kind = "bin"
main = "src/main.c"

[target.'cfg(all(os = "linux", env = "musl"))'.dependencies]
openkal-llvm-runtime = "0.15.6"
TOML
cp "$work/musl/src/main.c" "$work/rt/src/main.c"
out=$(cd "$work/rt" && "$STORE" build --target x86_64-linux-musl 2>&1); rc=$?
rbin=$(find "$work/rt/target" -type f -name rtprobe 2>/dev/null | head -1)
if [ $rc -ne 0 ] || [ -z "$rbin" ]; then
    fail "the runtime probe did not build"; printf '%s\n' "$out" | tail -8
else
    grep -q 'openkal-musl' "$work/rt/mcpp.lock" && grep -q '0.21.1' "$work/rt/mcpp.lock" \
        && ok "the lock records openkal-llvm-runtime 0.15.6 over openkal-musl 0.21.1" || fail "the runtime lock does not reach openkal-musl 0.21.1"
    l=$(cd "$work" && timeout 60 "$rbin" dirleak 2>&1 | grep '^descriptors')
    b=$(printf '%s\n' "$l" | sed -n 's/.*before=\([0-9-]*\).*/\1/p'); a=$(printf '%s\n' "$l" | sed -n 's/.*after=\([0-9-]*\).*/\1/p')
    [ -n "$b" ] && [ "$b" = "$a" ] && ok "#34 through the runtime: $l" || fail "#34 through the runtime: $l"
    r=$(timeout 60 "$rbin" stack 4096 3900 2>&1)
    printf '%s\n' "$r" | grep -q '^recursed 3900 levels' && ok "musl#48 through the runtime: 4 MiB, 3900 levels" || fail "musl#48 through the runtime"
    if command -v python3 >/dev/null 2>&1; then
        bytes=$(python3 "$work/keys.py" 0d0a71 "$rbin" icrnl 2>&1 | tr -d '\r' | grep '^byte ' | tr '\n' ' ')
        [ "$bytes" = "byte 0x0d byte 0x0a byte 0x71 " ] && ok "#48 through the runtime: $bytes" || fail "#48 through the runtime: $bytes"
    fi
fi

# -- E. what this sandbox cannot observe --------------------------------------
skip "macOS, Windows and WebAssembly: their implementations answered on their own runners (openkal-macos#28 and #29, openkal-windows#32, openkal-emscripten#7, openkal#50; a program duplicated by fork starting a thread on macOS, openkal-musl#56)"
skip "a machine with firmware and no operating system: openkal-opensbi 0.8.3 moves a pin and provides none of the three interfaces"

printf '\n%s assertion(s) failed\n' "$fails"
[ -n "$skipped" ] && printf 'not run:%s\n' "$skipped"
exit "$fails"
