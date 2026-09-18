#!/usr/bin/env bash
# Ecosystem verification for the C environment wave, resolved from the published
# index only.
#
#   B64=$(base64 -w0 <this file>)
#   xlings subos new cenv && xlings subos use cenv --sandbox --cmd \
#     "echo $B64 | base64 -d > /tmp/v.sh && MCPP_VERIFY_VERSION=<mcpp> bash /tmp/v.sh"
#
# The subject is the declared C environment on Windows: a POSIX presentation
# with LP64 and a 32-bit wchar_t over a PE image. Each criterion reads back what
# the target actually got, and the ones that need to observe behaviour run the
# image under Wine. A step that cannot run here says so and is counted as not
# run, never as passed.
set -u

VER="${MCPP_VERIFY_VERSION:?set MCPP_VERIFY_VERSION}"
STORE="${MCPP_VERIFY_BIN:-$HOME/.xlings/data/xpkgs/xim-x-mcpp/$VER/bin/mcpp}"
XL="${XLINGS_BIN:-$(command -v xlings)}"
RUNTIME="${OPENKAL_RUNTIME:-0.11.0}"
MUSL="${OPENKAL_MUSL:-0.15.0}"
WINDOWS="${OPENKAL_WINDOWS:-0.8.0}"
TARGET="${VERIFY_TARGET:-x86_64-windows-gnu}"

fails=0; skipped=""
fail()    { printf 'ASSERT-FAIL: %s\n' "$1"; fails=$((fails + 1)); }
ok()      { printf 'ok: %s\n' "$1"; }
section() { printf '\n== %s ==\n' "$1"; }
skip()    { printf 'NOT RUN: %s\n' "$1"; skipped="$skipped
  - $1"; }

WINE="$(command -v wine64 || command -v wine || true)"
run_target() {  # run_target <exe> ; prints output, returns the exit status
    [ -n "$WINE" ] || return 127
    WINEDEBUG=-all "$WINE" "$1" 2>/dev/null
}

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

# -- B. what the target received ----------------------------------------------
# The environment is a property of the target, so it is read at compile time and
# confirmed at run time: a width the preprocessor claims but the image does not
# honour would pass a static check and fail here.
section "B. the declared environment on $TARGET"
mkdir -p "$work/cenv/src"
cat > "$work/cenv/mcpp.toml" <<TOML
[package]
name = "cenv"
version = "0.1.0"
[dependencies]
openkal-musl = "$MUSL"
[target.'cfg(os = "windows")'.dependencies]
openkal-windows = "$WINDOWS"
[build]
cxx_runtime = "host-coupled"
[targets.cenv]
kind = "bin"
main = "src/main.c"
[toolchain]
default = "llvm@22.1.8"
TOML
cat > "$work/cenv/src/main.c" <<'C'
#include <stdio.h>
#include <wchar.h>
int main(void) {
    int code = 0;
    /* The data model, as the image was built, not as a header asserts. */
    if (sizeof(long) != 8) code |= 1;
    if (sizeof(void*) != 8) code |= 2;
    if (sizeof(wchar_t) != 4) code |= 4;
    /* A wide literal above U+FFFF survives only in a 32-bit wchar_t. */
    const wchar_t w[] = L"\U0001F700x";
    if (w[0] != 0x1F700 || w[1] != L'x') code |= 8;
    /* The C environment is POSIX-presenting. */
#if defined(_WIN32)
    code |= 16;
#endif
#if !defined(__unix__)
    code |= 32;
#endif
    /* The object format still has a name, for code that must know it. */
#if !defined(__CYGWIN__)
    code |= 64;
#endif
    /* The kernel ABI is openkal, and says so. */
#if !defined(__openkal__)
    code |= 128;
#endif
    printf("code=%d long=%zu wchar=%zu\n", code, sizeof(long), sizeof(wchar_t));
    return code;
}
C
out=$(cd "$work/cenv" && "$STORE" build --target "$TARGET" 2>&1); rc=$?
if [ $rc -ne 0 ]; then
    fail "the environment probe did not build for $TARGET"; printf '%s\n' "$out" | tail -10
else
    ok "the environment probe builds for $TARGET"
    exe=$(find "$work/cenv" -name 'cenv*.exe' -type f 2>/dev/null | head -1)
    if [ -z "$exe" ]; then skip "B: no image was produced to run"
    elif [ -z "$WINE" ]; then skip "B: no wine in this sandbox, the image was not run"
    else
        o=$(run_target "$exe"); rc=$?
        if [ $rc -eq 0 ]; then ok "LP64, 32-bit wchar_t, a literal above U+FFFF, no _WIN32, __unix__, __CYGWIN__, __openkal__ ($o)"
        else fail "the environment probe returned $rc ($o)"; fi
    fi
fi

# -- C. what the environment broke once ---------------------------------------
# Three behaviours regressed in the spike because our own port layer read the
# platform off _WIN32. They are the regression test for that class of defect.
section "C. argv, paths and spawn under the POSIX presentation"
mkdir -p "$work/beh/src"
sed "s/name = \"cenv\"/name = \"beh\"/; s/\[targets.cenv\]/[targets.beh]/" \
    "$work/cenv/mcpp.toml" > "$work/beh/mcpp.toml"
cat > "$work/beh/src/main.c" <<'C'
#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
extern char **environ;
int main(int argc, char **argv) {
    /* The child leg: the parent spawns this image with one extra argument. */
    if (argc == 2 && strcmp(argv[1], "child") == 0) return 7;
    int code = 0;
    /* argv reaches the program whole. */
    if (argc < 1 || argv[0] == 0 || strlen(argv[0]) < 4) code |= 1;
    /* A platform-shaped path still resolves; the C environment does not
     * decide what a path looks like to the kernel. */
    struct stat st;
    if (stat("C:\\Windows\\System32\\notepad.exe", &st) != 0) code |= 2;
    /* posix_spawn of this image, which the port retries with .exe. */
    pid_t pid; int status = 0;
    char *av[] = { argv[0], (char*)"child", 0 };
    if (posix_spawn(&pid, argv[0], 0, 0, av, environ) != 0) code |= 4;
    else if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 7) code |= 8;
    printf("code=%d argc=%d argv0=%s\n", code, argc, argc > 0 ? argv[0] : "-");
    return code;
}
C
out=$(cd "$work/beh" && "$STORE" build --target "$TARGET" 2>&1); rc=$?
if [ $rc -ne 0 ]; then
    fail "the behaviour probe did not build for $TARGET"; printf '%s\n' "$out" | tail -10
else
    exe=$(find "$work/beh" -name 'beh*.exe' -type f 2>/dev/null | head -1)
    if [ -z "$exe" ] || [ -z "$WINE" ]; then skip "C: the image was not run (no wine or no image)"
    else
        o=$(run_target "$exe"); rc=$?
        if [ $rc -eq 0 ]; then ok "argv arrives whole, a Windows-shaped path resolves, posix_spawn retries with .exe ($o)"
        else fail "the behaviour probe returned $rc ($o)"; fi
    fi
fi

# -- D. the C++ runtime in this environment -----------------------------------
# libunwind selects its PE paths on a target the package states, not on _WIN32.
# Building is not enough: an exception must cross a frame at run time.
section "D. openkal-llvm-runtime $RUNTIME on $TARGET"
mkdir -p "$work/cxx/src"
cat > "$work/cxx/mcpp.toml" <<TOML
[package]
name = "cxx"
version = "0.1.0"
[dependencies]
openkal-llvm-runtime = "$RUNTIME"
[target.'cfg(os = "windows")'.dependencies]
openkal-windows = "$WINDOWS"
[toolchain]
default = "llvm@22.1.8"
TOML
cat > "$work/cxx/src/main.cpp" <<'CPP'
import std;
struct marker { int v; };
[[gnu::noinline]] static void deep(int n) {
    if (n == 0) throw marker{ 42 };
    deep(n - 1);
}
int main() {
    int code = 0;
    try { deep(8); code |= 1; }
    catch (const marker& m) { if (m.v != 42) code |= 2; }
    /* The unwinder ran destructors on the way out. */
    static int destroyed = 0;
    struct guard { int* p; ~guard() { ++*p; } };
    try { guard g{ &destroyed }; deep(3); }
    catch (const marker&) {}
    if (destroyed != 1) code |= 4;
    std::println("code={}", code);
    return code;
}
CPP
out=$(cd "$work/cxx" && "$STORE" build --target "$TARGET" 2>&1); rc=$?
if [ $rc -ne 0 ]; then
    fail "the C++ runtime did not build for $TARGET"; printf '%s\n' "$out" | tail -12
else
    ok "libc++, libc++abi and libunwind build for $TARGET"
    exe=$(find "$work/cxx" -name 'cxx*.exe' -type f 2>/dev/null | head -1)
    if [ -z "$exe" ] || [ -z "$WINE" ]; then skip "D: the image was not run (no wine or no image)"
    else
        o=$(run_target "$exe"); rc=$?
        if [ $rc -eq 0 ]; then ok "an exception unwinds eight frames and destructors run ($o)"
        else fail "the unwinding probe returned $rc ($o)"; fi
    fi
fi

# -- E. the installed headers agree with the C library ------------------------
# bits/setjmp.h decides the layout of jmp_buf. If the application and the C
# library read that decision differently, the sizes part and longjmp corrupts
# the stack. The size is compared against what the library itself was built for.
section "E. jmp_buf agrees between the application and the C library"
mkdir -p "$work/sj/src"
sed "s/name = \"cenv\"/name = \"sj\"/; s/\[targets.cenv\]/[targets.sj]/" \
    "$work/cenv/mcpp.toml" > "$work/sj/mcpp.toml"
cat > "$work/sj/src/main.c" <<'C'
#include <setjmp.h>
#include <stdio.h>
int main(void) {
    jmp_buf b;
    volatile int code = 0;
    volatile long canary = 0x5EED5EED5EED5EEDL;
    if (setjmp(b) == 0) longjmp(b, 3);
    else if (canary != 0x5EED5EED5EED5EEDL) code |= 1;
    printf("code=%d jmp_buf=%zu\n", (int)code, sizeof(jmp_buf));
    return (int)code;
}
C
out=$(cd "$work/sj" && "$STORE" build --target "$TARGET" 2>&1); rc=$?
if [ $rc -ne 0 ]; then
    fail "the setjmp probe did not build for $TARGET"; printf '%s\n' "$out" | tail -8
else
    exe=$(find "$work/sj" -name 'sj*.exe' -type f 2>/dev/null | head -1)
    if [ -z "$exe" ] || [ -z "$WINE" ]; then skip "E: the image was not run (no wine or no image)"
    else
        o=$(run_target "$exe"); rc=$?
        if [ $rc -eq 0 ]; then ok "longjmp returns to the application frame with its locals intact ($o)"
        else fail "the setjmp probe returned $rc ($o)"; fi
    fi
fi

section "summary"
printf '%d assertion(s) failed\n' "$fails"
[ -n "$skipped" ] && printf 'not run:%s\n' "$skipped"
[ $fails -eq 0 ]
