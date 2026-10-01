#!/usr/bin/env bash
# Ecosystem verification for the wave that states what a started program
# receives (openkal 0.14.1, openkal-linux 0.15.1, openkal-musl 0.19.3,
# openkal-llvm-runtime 0.15.3, tinyhttps 0.3.3), resolved from the published
# index only.
#
#   xlings subos new okl0141
#   cp <this file> ~/.xlings/subos/okl0141/tmp/v.sh
#   xlings subos use okl0141 --sandbox --cmd "MCPP_VERIFY_VERSION=<mcpp> bash /tmp/v.sh"
#   bash <this file> host ~/.xlings/subos/okl0141/tmp/okl-c
#
# Host mode observes what proot cannot run: a program openkal-linux starts with
# execveat (B), and the static program the sandbox built (C).
#
# The script is copied into the sandbox's /tmp rather than passed on its command
# line: under proot a command line of this size was observed to come with
# `ptrace(PEEKDATA): Bad address' and failures of unrelated file operations.
#
# The subject is what a consumer gets by changing one version number. A version
# written without an operator is an exact pin in mcpp (`is_constraint` in
# modules/versioning/src/version_req.cppm), so each manifest below is the
# reporter's own with that one line changed. A step that cannot run here says
# so and is counted as not run, never as passed.
set -u

# -- host mode: section C's observations, on the binary the sandbox built -------
if [ "${1:-}" = host ]; then
    dir="${2:?host <dir holding spawnprobe>}"; bin="$dir/spawnprobe"; fails=0
    w=$(mktemp -d); trap 'rm -rf "$w"' EXIT
    printf '\n== B. a copy started with grants enumerates exactly them (host, from the published index) ==\n'
    mkdir -p "$w/ver/src"
    awk '/^cat > "\$work\/ver\/mcpp.toml" <<.TOML.$/{f=1;next} f&&/^TOML$/{f=0} f' "$0" > "$w/ver/mcpp.toml"
    awk '/^cat > "\$work\/ver\/src\/main.c" <<.C.$/{f=1;next} f&&/^C$/{f=0} f' "$0" > "$w/ver/src/main.c"
    out=$(cd "$w/ver" && mcpp index update >/dev/null 2>&1; mcpp run 2>&1)
    kid=$(printf '%s\n' "$out" | grep -E '^child preopens ' | head -1)
    [ "$kid" = "child preopens 2 [/work] [/]" ] && printf 'ok: a granted copy enumerates: %s\n' "${kid#child preopens }" \
        || { printf 'ASSERT-FAIL: the granted copy reported: %s\n' "$kid"; printf '%s\n' "$out" | tail -5; fails=$((fails + 1)); }
    printf '\n== C. a program started through posix_spawn above openkal-musl receives nothing it was not given (host) ==\n'
    cat > "$w/fds.sh" <<'SH'
for f in /proc/self/fd/*; do
  [ -e "$f" ] || continue; n=${f##*/}
  [ "$n" -gt 2 ] || continue
  t=$(readlink "$f"); [ -d "$f" ] && echo "DIR $n $t"
done
echo RAN
SH
    [ -x "$bin" ] || { printf 'ASSERT-FAIL: no program at %s\n\n1 assertion(s) failed\n' "$bin"; exit 1; }
    elf=$("$bin" /bin/sh "$w/fds.sh" 2>&1)
    if ! printf '%s\n' "$elf" | grep -q '^RAN$'; then
        printf 'ASSERT-FAIL: the ordinary executable did not run: %s\n' "$elf"; fails=$((fails + 1))
    elif printf '%s\n' "$elf" | grep -q '^DIR '; then
        printf 'ASSERT-FAIL: an ordinary executable inherited a directory: %s\n' "$(printf '%s' "$elf" | grep '^DIR ' | head -1)"; fails=$((fails + 1))
    else printf 'ok: an ordinary executable inherits no directory descriptor\n'; fi
    printf '#!/bin/sh\n. %s\necho "zero=$0"\n' "$w/fds.sh" > "$w/script.sh"; chmod +x "$w/script.sh"
    scr=$("$bin" "$w/script.sh" 2>&1)
    if printf '%s\n' "$scr" | grep -q '^DIR '; then
        printf 'ASSERT-FAIL: a script inherited a directory: %s\n' "$(printf '%s' "$scr" | grep '^DIR ' | head -1)"; fails=$((fails + 1))
    elif printf '%s\n' "$scr" | grep -q '^zero=/dev/fd/'; then
        printf 'ok: a script starts, holds no directory, and reads its name as %s\n' "$(printf '%s' "$scr" | grep '^zero=' | cut -d= -f2)"
    else printf 'ASSERT-FAIL: the script did not start: %s\n' "$scr"; fails=$((fails + 1)); fi
    miss=$("$bin" /no/such/program 2>&1)
    [ "$miss" = "posix_spawn: errno 2" ] && printf 'ok: a name that is not there is still ENOENT\n' \
        || { printf 'ASSERT-FAIL: missing program: %s\n' "$miss"; fails=$((fails + 1)); }
    printf '\n%s assertion(s) failed\n' "$fails"
    [ "$fails" -eq 0 ]; exit
fi

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

# -- A. identity and mirror ---------------------------------------------------
section "A. identity and mirror"
got=$("$STORE" --version 2>&1 | head -1)
[ "$got" = "mcpp $VER" ] && ok "$got from $STORE" || fail "version is '$got' at $STORE"
"$STORE" self config --mirror CN >/dev/null 2>&1 || true
"$XL" config --mirror CN >/dev/null 2>&1 || true
xm=$(python3 -c "import json,os;print(json.load(open(os.path.expanduser('~/.xlings/.xlings.json'))).get('mirror',''))" 2>/dev/null)
[ "$xm" = "CN" ] && ok "xlings mirror is CN" || fail "xlings mirror is '$xm'"
"$STORE" index update >/dev/null 2>&1 || true

# -- B. the specification and the implementation resolve as patches -----------
# The implementation answers kal_version with the number the specification
# package is, and a copy started with two grants enumerates exactly them.
section "B. openkal 0.14.1 and openkal-linux 0.15.1 resolve, and a granted copy enumerates its grants"
mkdir -p "$work/ver/src"
cat > "$work/ver/mcpp.toml" <<'TOML'
[package]
name = "verprobe"
version = "0.1.0"

[dependencies]
openkal = "0.14.1"

[target.'cfg(os = "linux")'.dependencies]
openkal-linux = "0.15.1"

[targets.verprobe]
kind = "bin"
main = "src/main.c"
TOML
cat > "$work/ver/src/main.c" <<'C'
#include <openkal/version.h>
#include <openkal/stream.h>
#include <openkal/fs.h>
#include <openkal/process.h>
#include <openkal/env.h>

static void say(const char* s, kal_uintptr n) { kal_stream_write(kal_stdout(), s, n); }
static void put(const char* s) { kal_uintptr n = 0; while (s[n]) n++; say(s, n); }
static void number(kal_u64 v) {
    char b[24]; int i = 0;
    if (v == 0) { put("0"); return; }
    while (v) { b[i++] = (char)('0' + (v % 10)); v /= 10; }
    char o[25]; int j = 0;
    while (i) o[j++] = b[--i];
    o[j] = 0; put(o);
}
static int same(const char* a, kal_uintptr n, const char* b) {
    kal_uintptr m = 0; while (b[m]) m++;
    if (m != n) return 0;
    for (kal_uintptr i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

/* A copy of this program started with grants reports what it enumerates. */
static int child(void) {
    kal_uintptr n = kal_fs_preopen_count();
    put("child preopens "); number(n);
    for (kal_uintptr i = 0; i < n; i++) {
        struct kal_dir d; char nm[256]; kal_uintptr l = 0;
        kal_fs_preopen(i, &d, nm, sizeof nm, &l);
        put(" ["); say(nm, l); put("]");
    }
    put("\n");
    return 0;
}

int main(void) {
    char a1[32];
    kal_intptr l1 = kal_env_arg(1, a1, sizeof a1);
    if (l1 > 0 && same(a1, (kal_uintptr)l1, "--child")) return child();

    const kal_u64 v = kal_version();
    put("header ");   number(KAL_VERSION_MAJOR); put(".");
    number(KAL_VERSION_MINOR); put("."); number(KAL_VERSION_PATCH);
    put(" implementation "); number((v >> 32) & 0xffff); put(".");
    number((v >> 16) & 0xffff); put("."); number(v & 0xffff);
    put("\n");

    /* Start this program again, naming it relative to the working directory
     * (`mcpp run' starts it from there), granting the working directory as
     * "/work" and the root as "/". The copy enumerates exactly those, in that
     * order. The name is taken relative to the working directory and not to
     * the root because a sandbox that emulates bind mounts (proot) resolves a
     * name relative to the root descriptor in the host's tree, where the
     * emulated binds are absent. */
    char self[1024], here[1024];
    kal_intptr sl = kal_env_arg(0, self, sizeof self);
    kal_uintptr hl = 0;
    struct kal_dir root = {0}, cwd = {0};
    for (kal_uintptr i = 0; i < kal_fs_preopen_count(); i++) {
        struct kal_dir d; char nm[1024]; kal_uintptr l = 0;
        if (kal_fs_preopen(i, &d, nm, sizeof nm, &l) != kal_ok) continue;
        if (l == 1 && nm[0] == '/') root = d;
        else if (i == 0 && l < sizeof here) { cwd = d; for (hl = 0; hl < l; hl++) here[hl] = nm[hl]; }
    }
    if (sl > (kal_intptr)hl + 1 && hl > 0 && self[hl] == '/') {
        kal_uintptr k = 0; while (k < hl && self[k] == here[k]) k++;
        if (k == hl) {
            const char* argv[2] = { "verprobe", "--child" };
            kal_uintptr lens[2] = { 8, 7 };
            struct kal_preopen g[2] = { { cwd, "/work", 5 }, { root, "/", 1 } };
            struct kal_spawn how = { cwd, cwd, 0, g, 2, 0 };
            struct kal_process p;
            int rc = kal_process_spawn(&how, self + hl + 1, (kal_uintptr)sl - hl - 1, argv, lens, 2,
                                       0, 0, 0, 0, &p);
            if (rc == kal_ok) {
                int st = -1, te = -1;
                kal_process_wait(p, &st, &te);
                kal_process_close(p);
            } else { put("grant start refused: "); number((kal_u64)rc); put("\n"); }
        } else put("self is not under the working directory\n");
    } else put("self is not under the working directory\n");
    return (v >= KAL_VERSION) ? 0 : 1;
}
C
out=$(cd "$work/ver" && "$STORE" run 2>&1); rc=$?
line=$(printf '%s\n' "$out" | grep -E '^header ' | head -1)
kid=$(printf '%s\n' "$out" | grep -E '^child preopens ' | head -1)
if [ $rc -ne 0 ] || [ -z "$line" ]; then
    fail "the version probe did not build or run"; printf '%s\n' "$out" | tail -8
else
    [ "$line" = "header 0.14.1 implementation 0.14.1" ] && ok "$line" || fail "the version probe reported: $line"
    # The granted copy is observed in host mode. openkal-linux starts a program
    # with execveat, which proot does not intercept, so the kernel resolves the
    # program's interpreter in the host's tree, where the sandbox's toolchain
    # is not; any version fails the same way here.
    printf 'deferred: the granted copy is observed in host mode\n'
fi
lock=$(cat "$work/ver/mcpp.lock" 2>/dev/null)
printf '%s\n' "$lock" | grep -q '0.15.1' && ok "the lock records openkal-linux 0.15.1" || fail "the lock does not record openkal-linux 0.15.1"

# -- C. above the C environment, the issue's own program ------------------------
# openkal-linux#30, as filed: posix_spawn through openkal-musl. The manifest is
# the reporter's with openkal-llvm-runtime 0.15.2 changed to 0.15.3. The link is
# single-threaded because this sandbox runs under proot, where a multi-threaded
# ld.lld was observed to abort (`malloc(): unaligned tcache chunk detected`) and,
# on a second run, to stop making progress; it is a property of the sandbox and
# not of the program.
section "C. a program started through posix_spawn above openkal-musl receives nothing it was not given"
mkdir -p "$work/probe/src"
cat > "$work/probe/mcpp.toml" <<'TOML'
[package]
namespace = "probe"
name      = "dirfd"
version   = "0.0.0"

[toolchain]
default = "llvm@22.1.8"

[build]
ldflags = ["-Wl,--threads=1"]

[targets.spawnprobe]
kind = "bin"
main = "src/main.c"

[target.'cfg(all(os = "linux", env = "musl"))'.dependencies]
openkal-llvm-runtime = "0.15.3"
TOML
cat > "$work/probe/src/main.c" <<'C'
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>
extern char **environ;
int main(int argc, char **argv) {
    pid_t pid; int st = 0;
    if (argc < 2) return 2;
    int e = posix_spawn(&pid, argv[1], 0, 0, argv + 1, environ);
    if (e) { fprintf(stderr, "posix_spawn: errno %d\n", e); return 1; }
    waitpid(pid, &st, 0);
    printf("[child exit %d]\n", WEXITSTATUS(st));
    return 0;
}
C
out=$(cd "$work/probe" && "$STORE" build --target x86_64-linux-musl 2>&1); rc=$?
bin=$(find "$work/probe/target" -type f -name spawnprobe -path '*musl*' 2>/dev/null | head -1)
if [ $rc -ne 0 ] || [ -z "$bin" ]; then
    fail "the issue's program did not build above openkal-musl"; printf '%s\n' "$out" | tail -8
else
    grep -q 'openkal-linux' "$work/probe/mcpp.lock" && \
        { grep -A2 'openkal-linux' "$work/probe/mcpp.lock" | grep -q '0.15.1' \
          && ok "the reporter's manifest resolves openkal-linux 0.15.1" \
          || fail "the reporter's manifest does not resolve openkal-linux 0.15.1"; }
    # The behaviour is observed OUTSIDE the sandbox. The sandbox emulates its
    # bind mounts (proot), and its root is an empty directory on the host, so
    # every name openkal resolves relative to the root descriptor --- /bin/sh
    # included --- is absent there for any version. The program is static, so
    # the binary built here from the published index runs unchanged on the
    # host: `bash <this file> host <dir>` performs the observations below.
    out_dir=/tmp/okl-c; rm -rf "$out_dir"; mkdir -p "$out_dir"
    cp "$bin" "$out_dir/spawnprobe"
    ok "the reporter's program is built from the published index; its behaviour is observed outside (host mode)"
fi

# -- D. tinyhttps: a certificate that does not verify is refused ---------------
# The reporter's project names tinyhttps 0.3.0; changed to 0.3.3, its client
# refuses a self-signed server and accepts one signed by a CA it is told to
# trust.
section "D. tinyhttps 0.3.3 verifies certificates"
if ! command -v openssl >/dev/null 2>&1; then
    skip "openssl is not available to make test certificates"
else
    mkdir -p "$work/tls/src" && cd "$work/tls"
    openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=okl-test-ca" \
        -keyout ca.key -out ca.crt >/dev/null 2>&1
    openssl req -newkey rsa:2048 -nodes -subj "/CN=localhost" -keyout good.key -out good.csr >/dev/null 2>&1
    printf 'subjectAltName=DNS:localhost\n' > san.ext
    openssl x509 -req -in good.csr -CA ca.crt -CAkey ca.key -CAcreateserial -days 2 \
        -extfile san.ext -out good.crt >/dev/null 2>&1
    openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=localhost" \
        -addext "subjectAltName=DNS:localhost" -keyout self.key -out self.crt >/dev/null 2>&1
    cat > mcpp.toml <<'TOML'
[package]
name = "tlsprobe"
version = "0.1.0"

[dependencies]
tinyhttps = "0.3.3"

[targets.tlsprobe]
kind = "bin"
main = "src/main.cpp"
TOML
    cat > src/main.cpp <<'CPP'
import mcpplibs.tinyhttps;
import std;
namespace https = mcpplibs::tinyhttps;
int main(int argc, char** argv) {
    https::Socket::platform_init();
    https::HttpClientConfig cfg;
    https::HttpRequest req;
    req.url = std::format("https://localhost:{}/", argv[2]);
    https::HttpClient client{cfg};
    auto r = client.send(req);
    if (r.statusCode == 0) { std::println("[{}] FAILED {}", argv[1], r.statusText); return 1; }
    std::println("[{}] SUCCESS {}", argv[1], r.statusCode);
}
CPP
    out=$("$STORE" build 2>&1); rc=$?
    tb=$(find target -type f -name tlsprobe | head -1)
    if [ $rc -ne 0 ] || [ -z "$tb" ]; then
        fail "the tinyhttps probe did not build"; printf '%s\n' "$out" | tail -8
    else
        grep -A2 'tinyhttps' mcpp.lock | grep -q '0.3.3' && ok "tinyhttps 0.3.3 resolves" \
            || fail "tinyhttps did not resolve to 0.3.3"
        openssl s_server -accept 127.0.0.1:18443 -cert good.crt -key good.key -www >/dev/null 2>&1 & g=$!
        openssl s_server -accept 127.0.0.1:18444 -cert self.crt -key self.key -www >/dev/null 2>&1 & s=$!
        sleep 1
        good=$(SSL_CERT_FILE=ca.crt "$tb" good 18443 2>&1)
        self=$(SSL_CERT_FILE=ca.crt "$tb" self 18444 2>&1)
        kill $g $s 2>/dev/null
        case "$good" in "[good] SUCCESS 200") ok "$good";; *) fail "trusted server: $good";; esac
        case "$self" in "[self] FAILED certificate verification failed"*) ok "$self";;
                        *) fail "self-signed server: $self";; esac
    fi
    cd - >/dev/null
fi

printf '\n%s assertion(s) failed\n' "$fails"
[ -n "$skipped" ] && printf 'not run:%s\n' "$skipped"
[ "$fails" -eq 0 ]
