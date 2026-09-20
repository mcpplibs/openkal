#!/usr/bin/env bash
# Ecosystem verification for the KAL_TERM_PASS_CONTROL wave, resolved from the
# published index only.
#
#   B64=$(base64 -w0 <this file>)
#   xlings subos new term014 && xlings subos use term014 --sandbox --cmd \
#     "echo $B64 | base64 -d > /tmp/v.sh && MCPP_VERIFY_VERSION=<mcpp> bash /tmp/v.sh"
#
# The subject is what a consumer gets by writing a version number: that openkal
# 0.14.0 and the packages of its wave resolve, that the declarations carry the
# new position in both forms, that an implementation answers `kal_version' with
# the number the package is (it answered 0.11 through two releases), and that a
# program above the C environment enters raw mode and reads the interrupt
# keystroke as data.
#
# A step that cannot run here says so and is counted as not run, never as
# passed.
set -u

VER="${MCPP_VERIFY_VERSION:?set MCPP_VERIFY_VERSION}"
STORE="${MCPP_VERIFY_BIN:-$HOME/.xlings/data/xpkgs/xim-x-mcpp/$VER/bin/mcpp}"
XL="${XLINGS_BIN:-$(command -v xlings)}"
OPENKAL="${OPENKAL_VERSION:-0.14.0}"
LINUX="${OPENKAL_LINUX:-0.14.0}"
MUSL="${OPENKAL_MUSL:-0.16.0}"

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

# -- B. the specification resolves, and says which version it is --------------
# `kal_version' is the constant the header carries, and every implementation
# answers with it. Two releases went out with 11 written there while the package
# was 0.12 and 0.13, so the comparison clause 6.2 gives a consumer bound at load
# was between two equal numbers. This reads it back through an implementation.
section "B. openkal $OPENKAL resolves and states its version"
mkdir -p "$work/ver/src"
cat > "$work/ver/mcpp.toml" <<TOML
[package]
name = "verprobe"
version = "0.1.0"

[dependencies]
openkal = "$OPENKAL"

[target.'cfg(os = "linux")'.dependencies]
openkal-linux = "$LINUX"

[targets.verprobe]
kind = "bin"
main = "src/main.c"
TOML
cat > "$work/ver/src/main.c" <<'C'
#include <openkal/version.h>
#include <openkal/stream.h>
#include <openkal/terminal.h>

static void say(const char* s) {
    kal_uintptr n = 0; while (s[n]) n++;
    kal_stream_write(kal_stdout(), s, n);
}
static void number(kal_u64 v) {
    char b[24]; int i = 0;
    if (v == 0) { say("0"); return; }
    while (v) { b[i++] = (char)('0' + (v % 10)); v /= 10; }
    char o[25]; int j = 0;
    while (i) o[j++] = b[--i];
    o[j] = 0; say(o);
}
int main(void) {
    const kal_u64 v = kal_version();
    say("header ");   number(KAL_VERSION_MAJOR); say(".");
    number(KAL_VERSION_MINOR); say("."); number(KAL_VERSION_PATCH);
    say(" implementation "); number((v >> 32) & 0xffff); say(".");
    number((v >> 16) & 0xffff); say("."); number(v & 0xffff);
    say(" pass_control "); number(KAL_TERM_PASS_CONTROL);
    say("\n");
    return (v >= KAL_VERSION) ? 0 : 1;
}
C
out=$(cd "$work/ver" && "$STORE" run 2>&1); rc=$?
line=$(printf '%s\n' "$out" | grep -E '^header ' | head -1)
if [ $rc -ne 0 ] || [ -z "$line" ]; then
    fail "the version probe did not build or run"; printf '%s\n' "$out" | tail -8
else
    case "$line" in
        "header 0.14.0 implementation 0.14.0 pass_control 4")
            ok "$line" ;;
        *) fail "the version probe reported: $line" ;;
    esac
fi

# -- C. the position is reachable in both forms -------------------------------
# A macro does not cross a module boundary, and every position in this
# specification therefore exists twice. A consumer that imports the module is
# the only thing that establishes the second one.
section "C. the position is reachable as a macro and as a module constant"
mkdir -p "$work/mod/src"
sed 's/name = "verprobe"/name = "modprobe"/; s/\[targets.verprobe\]/[targets.modprobe]/; s|main = "src/main.c"|main = "src/main.cpp"|' \
    "$work/ver/mcpp.toml" > "$work/mod/mcpp.toml"
cat > "$work/mod/src/main.cpp" <<'CPP'
import openkal.terminal;
import openkal.macros;
import openkal.stream;

static_assert(kal::terminal::pass_control.bits == kal::macros::KAL_TERM_PASS_CONTROL_M);
static_assert(kal::terminal::pass_control.bits == 4u);

int main() {
    const char m[] = "module and macro agree\n";
    kal_stream_write(kal_stdout(), m, sizeof m - 1);
    return 0;
}
CPP
out=$(cd "$work/mod" && "$STORE" run 2>&1); rc=$?
if [ $rc -eq 0 ] && printf '%s\n' "$out" | grep -q 'module and macro agree'; then
    ok "kal::terminal::pass_control and KAL_TERM_PASS_CONTROL_M are the same position"
else
    fail "the module form of the position did not compile or run"; printf '%s\n' "$out" | tail -8
fi

# -- D. a program above the C environment enters raw mode ---------------------
# The criterion is a relation: the transcript above openkal-musl is compared
# with the transcript of the same source above this machine's own C library,
# upon a pseudo-terminal of the same kind. An assertion written here --- "0x03
# arrives" --- would pass on a system whose terminal does not deliver it at all.
section "D. the interrupt keystroke arrives as data above openkal-musl $MUSL"
mkdir -p "$work/term/src"
cat > "$work/term/mcpp.toml" <<TOML
[package]
name = "termprobe"
version = "0.1.0"

[dependencies]
openkal-musl = "$MUSL"

[targets.termprobe]
kind = "bin"
main = "src/main.c"

[build]
target = "x86_64-linux-musl"
cxx_runtime = "host-coupled"
TOML
cat > "$work/term/src/main.c" <<'C'
#define _GNU_SOURCE 1
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static void show(const char* tag, const struct termios* t) {
    printf("%s icanon=%d echo=%d isig=%d ixon=%d vmin=%d\r\n", tag,
           (t->c_lflag & ICANON) != 0, (t->c_lflag & ECHO) != 0,
           (t->c_lflag & ISIG) != 0, (t->c_iflag & IXON) != 0,
           (int)t->c_cc[VMIN]);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("isatty %d\r\n", isatty(0));
    struct termios original;
    memset(&original, 0x5a, sizeof original);
    if (tcgetattr(0, &original) != 0) { printf("no terminal\r\n"); return 1; }
    show("before", &original);
    struct termios raw = original;
    cfmakeraw(&raw);
    printf("tcsetattr %d\r\n", tcsetattr(0, TCSANOW, &raw));
    struct termios back;
    memset(&back, 0x5a, sizeof back);
    tcgetattr(0, &back);
    show("readback", &back);
    printf("reading\r\n");
    for (char c; read(0, &c, 1) == 1; ) {
        printf("byte 0x%02x\r\n", (unsigned char)c);
        if (c == 'q') break;
    }
    tcsetattr(0, TCSANOW, &original);
    printf("done\r\n");
    return 0;
}
C
out=$(cd "$work/term" && "$STORE" build 2>&1); rc=$?
bin=$(find "$work/term/target" -type f -name termprobe -perm -u+x 2>/dev/null | head -1)
if [ $rc -ne 0 ] || [ -z "$bin" ]; then
    fail "the terminal probe did not build above openkal-musl $MUSL"
    printf '%s\n' "$out" | tail -10
else
    # NOT NAMED pty.py. Python puts the script's own directory first on the import
    # path, so a helper called pty.py shadows the standard module it imports:
    #
    #   AttributeError: partially initialized module 'pty' has no attribute 'fork'
    #
    # Measured in the sandbox, where it turned D into "not run" and said the
    # machine's own C library was at fault.
    cat > "$work/ptykeys.py" <<'PY'
import os, pty, select, sys, time
marker, keys, cmd = sys.argv[1].encode(), bytes.fromhex(sys.argv[2]), sys.argv[3:]
pid, fd = pty.fork()
if pid == 0:
    os.execvp(cmd[0], cmd); os._exit(127)
out, typed, deadline = bytearray(), False, time.monotonic() + 30
while time.monotonic() < deadline:
    r, _, _ = select.select([fd], [], [], 0.5)
    if r:
        try: chunk = os.read(fd, 4096)
        except OSError: break
        if not chunk: break
        out += chunk
    if not typed and marker in out:
        time.sleep(0.2); os.write(fd, keys); typed = True
os.waitpid(pid, 0)
sys.stdout.buffer.write(bytes(out).replace(b"\r", b""))
PY
    if ! command -v python3 >/dev/null 2>&1; then
        skip "D: no python3 here, so nothing can type at a pseudo-terminal"
    else
        port=$(python3 "$work/ptykeys.py" reading 61620371 "$bin" 2>/dev/null)
        cc="$(command -v cc || command -v gcc || true)"
        if [ -n "$cc" ] && "$cc" "$work/term/src/main.c" -o "$work/control" 2>/dev/null; then
            ctrl=$(python3 "$work/ptykeys.py" reading 61620371 "$work/control" 2>/dev/null)
            if printf '%s\n' "$ctrl" | grep -q 'byte 0x03'; then
                if [ "$port" = "$ctrl" ]; then
                    ok "the transcripts agree, keystroke for keystroke"
                else
                    fail "a terminal above openkal-musl does not behave as this machine's C library"
                    printf -- '--- control\n%s\n--- port\n%s\n' "$ctrl" "$port"
                fi
            else
                skip "D: this machine's own C library did not receive the keystroke as data either"
            fi
        else
            # Without a control the claim is weaker and is stated as such.
            if printf '%s\n' "$port" | grep -q 'byte 0x03' &&
               printf '%s\n' "$port" | grep -q 'readback icanon=0 echo=0 isig=0 ixon=0 vmin=1'; then
                ok "raw mode is entered and the interrupt keystroke arrives as 0x03 (no control available)"
            else
                fail "the terminal probe above openkal-musl did not read the keystroke as data"
                printf '%s\n' "$port"
            fi
        fi
    fi
fi

section "summary"
printf '%d assertion(s) failed\n' "$fails"
[ -n "$skipped" ] && printf 'not run:%s\n' "$skipped"
[ $fails -eq 0 ]
