#!/usr/bin/env bash
#
# THE ECOSYSTEM AS A CONSUMER RECEIVES IT.
#
#   sandbox-closure.sh <subos-name> [NAME=VERSION]...
#
# ⚠️⚠️ EVERY CONTINUOUS-INTEGRATION WORKFLOW IN THIS ECOSYSTEM SUBSTITUTES ITS
# SIBLINGS' WORKING TREES FOR THE VERSIONS ITS MANIFESTS NAME. That is
# deliberate --- these repositories change together, and a run must assert what
# is written today rather than what agreed when it was published. The
# consequence is that NO WORKFLOW ANYWHERE RESOLVES A PUBLISHED PACKAGE, so a
# defect belonging to the published FORM is invisible until someone outside
# meets it.
#
# ⚠️ That is not hypothetical. `openkal-kit` 0.1.0 shipped naming the
# specification by a path, which is true inside its own tarball and false for
# any consumer that also names an implementation: the specification was then
# reached by two routes, and the engine refused. Eight packages published, nine
# workflows green, eight repositories green on their own `main` --- and the
# first resolution of the published set failed immediately.
#
# ⇒ This script is the only thing that asks the question, which is why it lives
# in the repository rather than in somebody's scratch directory.
#
# WHY A SANDBOX. A machine that has been developing these packages has every one
# of them installed, and would answer for its own state rather than for the
# index. `xlings subos <name> --sandbox --cmd` gives a fresh environment, and a
# fresh `/tmp` with it --- so nothing may be staged outside.
#
# It asks two questions, because they fail independently:
#
#   ① Does the engine SAY the right things --- the layers, with the versions
#      that were PUBLISHED rather than any that resolve?
#   ② Does a PROGRAM depending on every one of them build and RUN? A layer table
#      can be right while the artefact is wrong, and the published-and-verified
#      assets of this ecosystem have been reachable and still unusable through a
#      supported path before.
set -euo pipefail

subos="${1:?usage: sandbox-closure.sh <subos-name> [NAME=VERSION]...}"
shift

# The versions under examination. Named here so that a reader sees them, and
# overridable one at a time so that a release moves one line rather than the
# script.
MCPP=2026.8.27.1
KIT=0.1.1
MUSL=0.5.0
LINUX=0.6.0
RUNTIME=0.3.0

for pair in "$@"; do
    case "$pair" in
        mcpp=*)    MCPP="${pair#*=}"    ;;
        kit=*)     KIT="${pair#*=}"     ;;
        musl=*)    MUSL="${pair#*=}"    ;;
        linux=*)   LINUX="${pair#*=}"   ;;
        runtime=*) RUNTIME="${pair#*=}" ;;
        *) echo "unknown pair: $pair" >&2; exit 2 ;;
    esac
done

xlings subos list 2>/dev/null | grep -q "  $subos " \
  || { echo "::error::the environment $subos does not exist"; exit 1; }

cat <<REPORT
the closure under examination:
  mcpp                 $MCPP
  openkal-kit          $KIT
  openkal-musl         $MUSL
  openkal-linux        $LINUX
  openkal-llvm-runtime $RUNTIME
REPORT

read -r -d '' SCRIPT <<'INNER' || true
set -euo pipefail
say() { printf '\n=== %s ===\n' "$*"; }

say "the engine, from the index"

# ⚠️⚠️ `xlings update' REPORTS SUCCESS WITHOUT HAVING REFRESHED ANYTHING, and
# this script is the one place where that matters most.
#
# Measured three times in one day, in three shapes: the artifact pointer served
# from a cache; `xlings update' exiting zero while the index directory still
# held the previous artifact; and a registry reporting the current hash while
# holding older content. The second is the one that bites here --- deleting the
# refresh MARKER does not help, because `update' re-derives it from the
# directory that is already there and writes the same hash back.
#
# ⇒ THE DIRECTORY IS REMOVED, not the marker. A script whose whole purpose is to
# read what was just published cannot begin by reading what was published last
# time, and "the index looked fresh" is indistinguishable from "the index was
# fresh" in every output either produces.
for base in "${XLINGS_HOME:-}" "$HOME/.xlings"; do
    [ -n "$base" ] || continue
    [ -d "$base/data/xim-pkgindex" ] && rm -rf "$base/data/xim-pkgindex" \
        && echo "  removed $base/data/xim-pkgindex so that the index is fetched again"
done
xlings update > /dev/null 2>&1 || true
# ⚠️ THE INDEX IS NAMED. `mcpp@<v>` alone is AMBIGUOUS wherever more than one
# index repository carries the name --- measured: local, scode and xim all
# answer, and xlings refuses rather than choosing. The engine is published in
# xim.
xlings install "xim:mcpp@__MCPP__" -y -g

# ⚠️⚠️ INSTALLING IS NOT BECOMING WHAT RUNS. In an environment that already has
# another mcpp the install succeeds and `mcpp` keeps resolving to the previous
# one --- xlings says so plainly and carries on. Without the assertion below
# this check would build the whole ecosystem with the OLD engine and report the
# release as verified.
xlings use mcpp __MCPP__ || true
mcpp self config --mirror GLOBAL
got="$(mcpp --version | awk '{print $2}')"
[ "$got" = "__MCPP__" ] || { echo "::error::mcpp is $got, not __MCPP__"; exit 1; }
echo "  mcpp is $got"

say "a project that names only published versions"
rm -rf /tmp/closure && mkdir -p /tmp/closure/src && cd /tmp/closure
cat > mcpp.toml <<TOML
[package]
name    = "closure"
version = "0.1.0"

[dependencies]
openkal-llvm-runtime = "__RUNTIME__"
# ⭐ THE SUBDIRECTORY PACKAGE, WHICH IS THE POINT OF NAMING IT HERE.
#
# openkal-kit is published out of the specification's own tarball through
# \`mcpp = "*/kit/mcpp.toml"\`, so its manifest is located within an archive
# rather than at its root. Nothing else in this ecosystem resolves that shape.
openkal-kit = "__KIT__"

[build]
cxx_runtime = "self-contained"
TOML

# ⭐ ONE PROGRAM, EVERY CHANGE OF THIS RELEASE.
cat > src/main.cpp <<'CPP'
// ⚠️ THE C HEADERS ARE INCLUDED ON PURPOSE. They are what pull musl's headers
// in, which is what makes the include path under examination the one used.
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <openkal/stream.h>
#include <openkal/terminal.h>
import std;
import openkal.kit.endpoint;

// ⭐ THE THREE NAMES A PROGRAM ABOVE THIS STACK MAY USE. musl's internal
// overlay defines them, and openkal-musl used to publish the directory that
// does (openkal-musl#13). If any is a macro again this file does not compile.
static int hidden = 7;
static int weak = 11;
struct weak_alias { int value; };

int main() {
    std::vector<int> v{4, 2, 7};
    std::ranges::sort(v);
    std::print("sorted:");
    for (int x : v) std::print(" {}", x);
    std::println("");
    std::println("names: {} {} {}", hidden, weak, weak_alias{3}.value);

    // openkal 0.8's terminal interface. Clause 6.1 makes absence a LINK-TIME
    // absence, so an older specification cannot satisfy this quietly.
    const kal_uintptr tp = kal_terminal_props(kal_stdout());
    std::println("openkal 0.8 terminal interface linked: {}",
                 static_cast<unsigned long long>(tp));

    // The kit, used rather than merely named.
    const auto ep  = kal::kit::parse_v4("10.0.0.255:8080", 15);
    const auto bad = kal::kit::parse_v4("300.1.1.1", 9);
    std::println("kit parses {} and refuses {}: {} {}",
                 ep.ok, !bad.ok, static_cast<unsigned>(ep.ep.addr[3]),
                 static_cast<unsigned long long>(ep.ep.port));

    // ⭐⭐ AND THE ROUTES THIS RELEASE ADDS, THROUGH POSIX AND NAMING NO OPENKAL
    // SYMBOL. A published C library whose sockets do not work would satisfy
    // every line above.
    const int lis = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(0x7f000001u);
    ::bind(lis, reinterpret_cast<sockaddr*>(&a), sizeof a);
    ::listen(lis, 4);
    socklen_t alen = sizeof a;
    ::getsockname(lis, reinterpret_cast<sockaddr*>(&a), &alen);

    const int cli = ::socket(AF_INET, SOCK_STREAM, 0);
    const bool connected = ::connect(cli, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0;
    pollfd pf{ lis, POLLIN, 0 };
    const bool readable = ::poll(&pf, 1, 2000) == 1;
    const int srv = ::accept(lis, nullptr, nullptr);
    ::write(cli, "ping", 4);
    char in[8] = {};
    size_t have = 0;
    while (have < 4) { const auto r = ::read(srv, in + have, 4 - have); if (r <= 0) break; have += r; }
    std::println("sockets: port {} connected {} readable {} carried {}",
                 static_cast<unsigned>(ntohs(a.sin_port)), connected, readable,
                 have == 4 && memcmp(in, "ping", 4) == 0);
    ::close(srv); ::close(cli); ::close(lis);

    const pid_t kid = ::fork();
    if (kid == 0) ::_exit(23);
    int status = 0;
    const bool reaped = kid > 0 && ::waitpid(kid, &status, 0) == kid
                     && WIFEXITED(status) && WEXITSTATUS(status) == 23;
    std::println("the calling image is duplicated: {}", reaped);
    return 0;
}
CPP

say "the program builds and runs"
mcpp run 2>&1 | tee /tmp/out.log

grep -q 'sorted: 2 4 7'                              /tmp/out.log
grep -q 'names: 7 11 3'                              /tmp/out.log
grep -q 'openkal 0.8 terminal interface linked:'     /tmp/out.log
grep -q 'kit parses true and refuses true: 255 8080' /tmp/out.log
# ⚠️ THE PORT IS NOT ASSERTED AS A VALUE --- the environment chooses it --- but
# everything else on the line is, and a port of zero would mean `getsockname`
# reported nothing.
grep -qE 'sockets: port [1-9][0-9]* connected true readable true carried true' /tmp/out.log
grep -q 'the calling image is duplicated: true'      /tmp/out.log

say "what the engine believes each layer is"
# ⭐ THE VERSION IS THE FIELD THAT DISCRIMINATES. That the engine knows a layer
# called `c-abi` says nothing about which package supplies it, and an older
# install answers the layer question exactly as this one does.
mcpp why toolchain 2>&1 | tee /tmp/layers.log
grep -qE "openkal-musl@__MUSL__"            /tmp/layers.log \
  || { echo "::error::the c-abi layer is not openkal-musl@__MUSL__"; exit 1; }
grep -qE "openkal-llvm-runtime@__RUNTIME__" /tmp/layers.log \
  || { echo "::error::the c++ layer is not openkal-llvm-runtime@__RUNTIME__"; exit 1; }
grep -qE "openkal-linux@__LINUX__"          /tmp/layers.log \
  || { echo "::error::the kernel-abi layer is not openkal-linux@__LINUX__"; exit 1; }

# ⭐⭐ private_include_dirs: THE CRITERION IS THE DIRECTORY, AND IT IS PER UNIT.
#
# ⚠️ A grep over the whole compile database can only ever fail: openkal-musl's
# OWN sources are built in this same graph and appear in the same file, and they
# MUST carry those directories --- that is what the overlay is for. The question
# is whether a unit that is NOT openkal-musl's sees them.
cdb=$(find . -name compile_commands.json | head -1)
[ -n "$cdb" ] || { echo "::error::no compile database was written"; exit 1; }
python3 - "$cdb" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
PRIV = ("musl/src/include", "musl/src/internal", "musl-generated/internal")
OWN  = "openkal-musl"
outside, leaked, own_with = 0, [], 0
for e in d:
    cmd = e.get("command") or " ".join(e.get("arguments", []))
    has = any(p in a for a in cmd.split() for p in PRIV)
    if OWN in e["file"]:
        own_with += has
    else:
        outside += 1
        if has: leaked.append(e["file"])
# ⚠️ DENOMINATORS BOTH WAYS. Zero units outside the package makes the absence
# vacuous; zero inside it carrying the directories means the overlay was never
# in use and the comparison is empty.
print(f"  {len(d)} entries: {outside} outside openkal-musl, {own_with} of its own carry the private directories")
if outside == 0 or own_with == 0:
    print("::error::the comparison has no denominator"); sys.exit(1)
if leaked:
    print(f"::error::{len(leaked)} unit(s) outside openkal-musl see its private directories")
    for f in leaked[:5]: print("      ", f)
    sys.exit(1)
print("  no translation unit outside openkal-musl sees musl/src/include")
PY

echo
echo "the closure holds: published packages, resolved from the index, built and run"
INNER

SCRIPT="${SCRIPT//__MCPP__/$MCPP}"
SCRIPT="${SCRIPT//__KIT__/$KIT}"
SCRIPT="${SCRIPT//__MUSL__/$MUSL}"
SCRIPT="${SCRIPT//__RUNTIME__/$RUNTIME}"
SCRIPT="${SCRIPT//__LINUX__/$LINUX}"

xlings subos use "$subos" --sandbox --cmd "$SCRIPT"
