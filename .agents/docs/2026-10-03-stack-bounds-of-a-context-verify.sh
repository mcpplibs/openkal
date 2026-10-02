#!/usr/bin/env bash
# Ecosystem verification for the wave that lets a running context say where it
# stands (openkal 0.15.0, openkal-linux 0.16.1, openkal-macos 0.13.0,
# openkal-windows 0.11.0, openkal-emscripten 0.4.0, openkal-opensbi 0.8.2,
# openkal-musl 0.20.1), resolved from the published index only.
#
#   xlings subos new okl015
#   cp <this file> ~/.xlings/subos/okl015/tmp/v.sh
#   xlings subos use okl015 --sandbox --cmd "MCPP_VERIFY_VERSION=<mcpp> bash /tmp/v.sh"
#
# The script is copied into the sandbox's /tmp rather than passed on its command
# line: under proot a command line of this size was observed to come with
# `ptrace(PEEKDATA): Bad address` and failures of unrelated file operations.
#
# The subject is what a consumer gets by naming the released versions. A version
# written without an operator is an exact pin in mcpp (`is_constraint` in
# modules/versioning/src/version_req.cppm), so each manifest below is what a
# consumer writes. A step that cannot run here says so and is counted as not
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

# -- A. identity and mirror ---------------------------------------------------
section "A. identity and mirror"
got=$("$STORE" --version 2>&1 | head -1)
[ "$got" = "mcpp $VER" ] && ok "$got from $STORE" || fail "version is '$got' at $STORE"
"$STORE" self config --mirror CN >/dev/null 2>&1 || true
"$XL" config --mirror CN >/dev/null 2>&1 || true
xm=$(python3 -c "import json,os;print(json.load(open(os.path.expanduser('~/.xlings/.xlings.json'))).get('mirror',''))" 2>/dev/null)
[ "$xm" = "CN" ] && ok "xlings mirror is CN" || fail "xlings mirror is '$xm'"
"$STORE" index update >/dev/null 2>&1 || true

# -- B. the implementation beneath answers the context that asks --------------
# The manifest names the specification and one implementation, which is the
# published shape. The program carries no runtime, so the region it asks about
# is the one openkal-linux measured: a running context can always say where it
# stands, which is the whole argument for clause 6.4 admitting the operation.
section "B. openkal 0.15.0 and openkal-linux 0.16.1 resolve, and a context is told its own region"
mkdir -p "$work/ver/src"
cat > "$work/ver/mcpp.toml" <<'TOML'
[package]
name = "stackprobe"
version = "0.1.0"

[dependencies]
openkal = "0.15.0"

[target.'cfg(os = "linux")'.dependencies]
openkal-linux = "0.16.1"

[targets.stackprobe]
kind = "bin"
main = "src/main.c"
TOML
cat > "$work/ver/src/main.c" <<'C'
#include <openkal/task.h>
#include <openkal/version.h>
#include <openkal/stream.h>

static void say(const char* s, kal_uintptr n) { kal_stream_write(kal_stdout(), s, n); }
static void put(const char* s) { kal_uintptr n = 0; while (s[n]) n++; say(s, n); }
static void number(kal_uintptr v) {
    char b[24]; int i = 0;
    if (v == 0) { put("0"); return; }
    while (v) { b[i++] = (char)('0' + (v % 10)); v /= 10; }
    char o[24]; int j = 0;
    while (i) o[j++] = b[--i];
    o[j] = 0; put(o);
}

/* The property a caller relies on and not the shape of the numbers: the region
 * contains a local of the context that asked. */
static int holds(void* base, kal_uintptr size, void* here) {
    const kal_uintptr b = (kal_uintptr)base;
    const kal_uintptr at = (kal_uintptr)here;
    return size != 0 && b + size > b && at >= b && at - b < size;
}

static volatile int g_started = -1;

static void entry(void* arg) {
    char here = 0;
    void* base = 0;
    kal_uintptr size = 0;
    const int e = kal_task_stack(&base, &size);
    g_started = (e == kal_ok && holds(base, size, &here)) ? 1 : 0;
    *(int*)arg = 1;
}

int main(void) {
    char here = 0;
    void* base = 0;
    kal_uintptr size = 0;
    const int e = kal_task_stack(&base, &size);

    put("version ");
    number((kal_uintptr)KAL_VERSION_MAJOR); put(".");
    number((kal_uintptr)KAL_VERSION_MINOR); put(".");
    number((kal_uintptr)KAL_VERSION_PATCH);
    put(" first e="); number((kal_uintptr)e);
    put(" contains="); number((kal_uintptr)(e == kal_ok && holds(base, size, &here)) ? 1u : 0u);
    put(" size="); number(size);
    put("\n");

    int ran = 0;
    struct kal_task t = {0};
    if (kal_task_start(entry, &ran, &t) == kal_ok) {
        kal_task_join(t);
        put("started contains="); number((kal_uintptr)(g_started == 1) ? 1u : 0u);
        put(" ran="); number((kal_uintptr)ran);
        put("\n");
    } else {
        put("started refused\n");
    }
    return 0;
}
C
out=$(cd "$work/ver" && "$STORE" run 2>&1); rc=$?
first=$(printf '%s\n' "$out" | grep -E '^version ' | head -1)
started=$(printf '%s\n' "$out" | grep -E '^started ' | head -1)
if [ $rc -ne 0 ] || [ -z "$first" ]; then
    fail "the stack probe did not build or run"; printf '%s\n' "$out" | tail -8
else
    printf '%s\n' "$first" | grep -q '^version 0.15.0 first e=0 contains=1 ' \
        && ok "$first" || fail "the probe reported: $first"
    [ "$started" = "started contains=1 ran=1" ] \
        && ok "$started" || fail "the started context reported: $started"
fi
lock=$(cat "$work/ver/mcpp.lock" 2>/dev/null)
printf '%s\n' "$lock" | grep -q '0.16.1' && ok "the lock records openkal-linux 0.16.1" \
    || fail "the lock does not record openkal-linux 0.16.1"

# -- C. the C library above it answers, and refuses where it must -------------
# `pthread_getattr_np' was the reason for the change: above openkal it described
# a range that was not the stack. It now answers for the calling thread with the
# region the implementation measured, and refuses for any other thread --- a
# refusal a program can read being the only alternative to a range it cannot
# check. The program is C, so nothing here needs the C++ runtime.
section "C. openkal-musl 0.20.1: the calling thread is told its region, another thread is refused"
mkdir -p "$work/musl/src"
cat > "$work/musl/mcpp.toml" <<'TOML'
[package]
name = "muslstack"
version = "0.1.0"

[dependencies]
openkal-musl = "0.20.1"

[targets.muslstack]
kind = "bin"
main = "src/main.c"
TOML
cat > "$work/musl/src/main.c" <<'C'
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

static int holds(void* base, size_t size, void* here) {
    const uintptr_t b = (uintptr_t)base;
    const uintptr_t at = (uintptr_t)here;
    return size != 0 && b + size > b && at >= b && at - b < size;
}

struct started { int contained; int refused; size_t size; };

static void* body(void* arg) {
    struct started* s = arg;
    char here = 0;
    pthread_attr_t a;
    int e = pthread_getattr_np(pthread_self(), &a);
    void* base = 0;
    size_t size = 0;
    if (e == 0 && pthread_attr_getstack(&a, &base, &size) != 0) e = -1;
    s->contained = (e == 0 && holds(base, size, &here)) ? 1 : 0;
    s->size = size;
    /* The identifier of another thread, asked from here: the port compares it
     * and never reads it, so a refusal is the only thing this can produce. */
    pthread_attr_t other;
    s->refused = pthread_getattr_np((pthread_t)(uintptr_t)0x1234, &other) == ENOSYS;
    return 0;
}

int main(void) {
    char here = 0;
    pthread_attr_t a;
    int e = pthread_getattr_np(pthread_self(), &a);
    void* base = 0;
    size_t size = 0;
    if (e == 0 && pthread_attr_getstack(&a, &base, &size) != 0) e = -1;
    printf("musl first e=%d contains=%d size=%zu\n", e,
           (e == 0 && holds(base, size, &here)) ? 1 : 0, size);

    struct started s = {0, 0, 0};
    pthread_t t;
    if (pthread_create(&t, 0, body, &s) != 0 || pthread_join(t, 0) != 0) {
        printf("musl started did not run\n");
        return 1;
    }
    printf("musl started contains=%d size=%zu another thread refused=%d\n",
           s.contained, s.size, s.refused);
    return 0;
}
C
out=$(cd "$work/musl" && "$STORE" build 2>&1); rc=$?
bin=$(find "$work/musl/target" -type f -name muslstack 2>/dev/null | head -1)
if [ $rc -ne 0 ] || [ -z "$bin" ]; then
    fail "the musl probe did not build"; printf '%s\n' "$out" | tail -8
else
    run=$(timeout 120 "$bin" 2>&1); rc=$?
    first=$(printf '%s\n' "$run" | grep -E '^musl first ' | head -1)
    started=$(printf '%s\n' "$run" | grep -E '^musl started ' | head -1)
    if [ $rc -ne 0 ] || [ -z "$first" ]; then
        fail "the musl probe did not run"; printf '%s\n' "$run" | tail -8
    else
        printf '%s\n' "$first" | grep -q '^musl first e=0 contains=1 ' \
            && ok "$first" || fail "the first context reported: $first"
        printf '%s\n' "$started" | grep -q '^musl started contains=1 .*another thread refused=1$' \
            && ok "$started" || fail "the started context reported: $started"
    fi
    lk=$(cat "$work/musl/mcpp.lock" 2>/dev/null)
    printf '%s\n' "$lk" | grep -q '0.20.1' && ok "the lock records openkal-musl 0.20.1" \
        || fail "the lock does not record openkal-musl 0.20.1"
fi

# -- D. what this sandbox cannot observe --------------------------------------
skip "macOS, Windows and WebAssembly: their implementations answer on their own runners (openkal-macos#26, openkal-windows#30, openkal-emscripten#5), and the two cross links are made on this machine by their own continuous integration"
skip "a machine with firmware and no operating system: openkal-opensbi 0.8.2 moves a pin and provides no openkal.task"

printf '\n%s assertion(s) failed\n' "$fails"
[ -n "$skipped" ] && printf 'not run:%s\n' "$skipped"
exit "$fails"
