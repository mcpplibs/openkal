#!/usr/bin/env bash
# Ecosystem verification for the openkal 0.13 wave, resolved from the published
# index only.
#
#   B64=$(base64 -w0 <this file>)
#   xlings subos new v013 && xlings subos use v013 --sandbox --cmd \
#     "echo $B64 | base64 -d > /tmp/v.sh && MCPP_VERIFY_VERSION=<mcpp> bash /tmp/v.sh"
#
# The sandbox has an empty $HOME and shares the xlings data directory, so mcpp
# is addressed by its store path. Every criterion reads back what it selected:
# a version, a result code, a search path. A step that cannot run here says so
# and is counted as not run, never as passed.
set -u

VER="${MCPP_VERIFY_VERSION:?set MCPP_VERIFY_VERSION}"
STORE="${MCPP_VERIFY_BIN:-$HOME/.xlings/data/xpkgs/xim-x-mcpp/$VER/bin/mcpp}"
XL="${XLINGS_BIN:-$(command -v xlings)}"
RUNTIME="${OPENKAL_RUNTIME:-0.10.0}"
MUSL="${OPENKAL_MUSL:-0.14.0}"
SPEC="${OPENKAL_SPEC:-0.13.0}"
LINUX="${OPENKAL_LINUX:-0.13.0}"
TINYHTTPS="${TINYHTTPS_VERSION:-0.3.1}"

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

# -- B. openkal itself: the executable record and kal_err_not_program ---------
section "B. openkal $SPEC over openkal-linux $LINUX"
mkdir -p "$work/spec/src"
cat > "$work/spec/mcpp.toml" <<TOML
[package]
name = "okv"
version = "0.1.0"
[dependencies]
openkal = "$SPEC"
[target.'cfg(os = "linux")'.dependencies]
openkal-linux = "$LINUX"
TOML
cat > "$work/spec/src/main.cpp" <<'CPP'
import openkal.types;
import openkal.fs;
import openkal.stream;
import openkal.process;
import openkal.abort;
int main() {
    const char* n = "v.txt"; const kal_uintptr l = 5;
    kal_file f{};
    kal::fs::open_file(kal::fs::working(), n, l,
        kal::fs::open::write | kal::fs::open::create | kal::fs::open::truncate, &f);
    kal_stream_write(kal_fs_stream(f), "text\n", 5);
    kal_fs_close_file(f);
    int code = 0;
    if (kal_fs_set_executable_at(kal::fs::working(), n, l, 1) != kal_ok) code |= 1;
    kal_node_info i = kal::fs::info_for_caller();
    kal_fs_info(kal::fs::working(), n, l, 0, kal::fs::field::executable, &i);
    if ((i.present & kal::fs::field::executable) == 0 || i.executable != 1) code |= 2;
    kal_process p{};
    const char* argv[1] = { n }; const kal_uintptr lens[1] = { l };
    const kal_spawn how{ kal::fs::working(), kal::fs::working(), nullptr, nullptr, 0, 0 };
    if (kal_process_spawn(&how, n, l, argv, lens, 1, nullptr, nullptr, 0, nullptr, &p)
        != kal_err_not_program) code |= 4;
    kal_fs_remove(kal::fs::working(), n, l);
    return code;
}
CPP
out=$(cd "$work/spec" && "$STORE" run 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "set and read the executable record; a text file is kal_err_not_program"
else fail "openkal $SPEC probe exited $rc"; printf '%s\n' "$out" | tail -8; fi

# -- C. openkal-musl: chmod of the execute bits, stat, ENOEXEC ----------------
section "C. openkal-musl $MUSL (C)"
mkdir -p "$work/musl/src"
cat > "$work/musl/mcpp.toml" <<TOML
[package]
name = "okm"
version = "0.1.0"
[dependencies]
openkal-musl = "$MUSL"
[build]
cxx_runtime = "host-coupled"
[targets.okm]
kind = "bin"
main = "src/main.c"
[toolchain]
default = "llvm@22.1.8"
TOML
cat > "$work/musl/src/main.c" <<'C'
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
extern char **environ;
int main(void) {
    int code = 0;
    int fd = open("m.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    write(fd, "text\n", 5); close(fd);
    /* The port reports one read, one write and one execute property for all
     * classes, so the reachable modes are 0666 and 0777 for a writable file. */
    struct stat st; stat("m.txt", &st);
    mode_t m = st.st_mode & 0777;
    if (chmod("m.txt", m | ((m & 0444) >> 2)) != 0) code |= 1;
    stat("m.txt", &st);
    if ((st.st_mode & 0111) != ((m & 0444) >> 2)) code |= 2;
    if (access("m.txt", X_OK) != 0) code |= 4;
    errno = 0;
    if (chmod("m.txt", 0700) != -1 || errno != ENOSYS) code |= 8;
    pid_t pid; char *av[] = { "m.txt", 0 };
    if (posix_spawn(&pid, "m.txt", 0, 0, av, environ) != ENOEXEC) code |= 16;
    unlink("m.txt");
    printf("code=%d\n", code);
    return code;
}
C
out=$(cd "$work/musl" && "$STORE" run 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "chmod of the execute bits round-trips through stat and access, a partial mode is ENOSYS, posix_spawn is ENOEXEC"
else fail "openkal-musl $MUSL probe exited $rc"; printf '%s\n' "$out" | tail -8; fi

# -- D. compat.zlib and tinyhttps on openkal, Linux and Windows ---------------
section "D. packages from the index on openkal-llvm-runtime $RUNTIME"
mkdir -p "$work/pk/tests"
cat > "$work/pk/mcpp.toml" <<TOML
[package]
name = "okp"
version = "0.1.0"
[dependencies]
openkal-llvm-runtime = "$RUNTIME"
tinyhttps = "$TINYHTTPS"
[dependencies.compat]
zlib = "1.3.2"
[toolchain]
default = "llvm@22.1.8"
TOML
cat > "$work/pk/tests/zlib.cpp" <<'CPP'
#include <zlib.h>
import std;
import mcpplibs.tinyhttps;
int main() {
    const unsigned long lib = ((zlibCompileFlags() >> 6) & 3u) == 2 ? 8
                            : ((zlibCompileFlags() >> 6) & 3u) == 1 ? 4 : 0;
    std::println("z_off_t library={} consumer={}", lib, sizeof(z_off_t));
    return lib == sizeof(z_off_t) ? 0 : 1;
}
CPP
out=$(cd "$work/pk" && "$STORE" test 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "linux: zlib and tinyhttps build above openkal, z_off_t agrees"
else fail "linux: package probe exited $rc"; printf '%s\n' "$out" | tail -10; fi
out=$(cd "$work/pk" && "$STORE" build --target x86_64-windows-gnu 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "windows: zlib and tinyhttps cross-build above openkal"
else fail "windows: cross build exited $rc"; printf '%s\n' "$out" | tail -10; fi

# The closure: the compiler's own search list for a unit of that build names no
# host directory. Read from the build database, as the compiler receives it.
db=$(cd "$work/pk" && "$STORE" emit build-database --spec compile-commands --target x86_64-windows-gnu 2>/dev/null)
cmd=$(printf '%s' "$db" | python3 -c "
import json,sys,shlex
try: d=json.load(sys.stdin)
except Exception: sys.exit(0)
for u in d:
    f=u.get('file','')
    a=u.get('arguments') or shlex.split(u.get('command',''))
    if 'compat-x-zlib' in f and f.endswith('.c') and a:
        out=[]; skip=False
        for x in a:
            if skip: skip=False; continue
            if x=='-o': skip=True; continue
            if x=='-c': continue
            out.append(x)
        print(shlex.join(out[:1]+['-v','-fsyntax-only']+out[1:])); break
" 2>/dev/null)
if [ -z "$cmd" ]; then
    skip "D: no compat.zlib C unit in the build database"
else
    search=$(cd "$work/pk" && eval "$cmd" 2>&1 | sed -n '/search starts here/,/End of search list/p')
    if printf '%s' "$search" | grep -qE '^ /usr/'; then
        fail "a host directory is on the search list of a compat.zlib unit"; printf '%s\n' "$search"
    elif [ -z "$search" ]; then
        skip "D: the compiler printed no search list"
    else ok "the search list of a compat.zlib unit names no host directory"; fi
fi

section "summary"
printf '%d assertion(s) failed\n' "$fails"
[ -n "$skipped" ] && printf 'not run:%s\n' "$skipped"
[ $fails -eq 0 ]
