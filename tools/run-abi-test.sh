#!/usr/bin/env bash
# One binary, built once, run against two implementations.
#
#   run-abi-test.sh <path-to-openkal-linux>
#
# ⚠️⚠️ WHY THIS IS SEPARATE FROM THE CONFORMANCE SUITE, AND WHY IT HAD TO BE
# BUILT BEFORE THE INTERFACE COULD BE CHANGED.
#
# The suite asserts that one artifact, built and run in one place, behaves as the
# specification says. The property a distributed binary rests upon is different
# and stronger: that one binary, BUILT ONCE, behaves as specified against an
# implementation IT WAS NOT COMPILED AGAINST.
#
# Nothing in this ecosystem could observe that. Every target had exactly one
# implementation, the choice was made at dependency resolution, and the
# implementation was linked in --- so the claim was not merely untested, there
# was no artifact of the shape that could test it. This produces one.
#
# The second implementation is built FROM the first: the same objects, with four
# names renamed out of the way and answered by `conformance/abi/interposer.cpp'
# instead. It is a second implementation in the only sense that matters to a
# consumer --- a different shared object, exporting the same surface, answering
# differently --- and it is four hundred lines lighter than a second port.
set -euo pipefail

impl="${1:?usage: run-abi-test.sh <path-to-openkal-linux>}"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
impl="$(cd "$impl" && pwd)"
work="${TMPDIR:-/tmp}/openkal-abi-$$"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/one" "$work/two" "$work/obj"

cxx="${CXX:-}"
if [ -z "$cxx" ]; then
    cxx="$(command -v clang++ || command -v g++ || true)"
fi
[ -n "$cxx" ] || { echo "no C++ compiler" >&2; exit 2; }
cc="${CC:-${cxx%++}}"
command -v "$cc" > /dev/null 2>&1 || cc="$cxx -x c"

objcopy="$(command -v llvm-objcopy || command -v objcopy || true)"
[ -n "$objcopy" ] || { echo "no objcopy, which is what renames the four" >&2; exit 2; }

echo "--- the implementation, as a shared object ---"
for src in "$impl"/src/*.cpp; do
    $cxx -std=c++23 -O1 -fPIC -fno-exceptions -fno-rtti \
         -I"$here/include" -c "$src" -o "$work/obj/$(basename "$src" .cpp).o"
done
$cxx -shared -o "$work/one/libopenkal.so.0" "$work/obj"/*.o -lpthread \
     -Wl,-soname,libopenkal.so.0
ln -sf libopenkal.so.0 "$work/one/libopenkal.so"

echo "--- a second implementation, over the first ---"
mkdir -p "$work/obj2"
for o in "$work/obj"/*.o; do
    # ⚠️ RENAMED AND NOT REMOVED. The four are still there, still doing what they
    # did; what changes is which name reaches them, so the second implementation
    # is the first one plus four answers rather than the first one minus four.
    "$objcopy" \
        --redefine-sym kal_memory_granularity=okabi_under_memory_granularity \
        --redefine-sym kal_fs_info=okabi_under_fs_info \
        --redefine-sym kal_fs_file_info=okabi_under_fs_file_info \
        --redefine-sym kal_version=okabi_under_version \
        --redefine-sym kal_interfaces=okabi_under_interfaces \
        --redefine-sym kal_exec_props=okabi_under_exec_props \
        "$o" "$work/obj2/$(basename "$o")"
done
$cxx -std=c++23 -O1 -fPIC -fno-exceptions -fno-rtti -I"$here/include" \
     -c "$here/conformance/abi/interposer.cpp" -o "$work/obj2/interposer.o"
$cxx -shared -o "$work/two/libopenkal.so.0" "$work/obj2"/*.o -lpthread \
     -Wl,-soname,libopenkal.so.0
ln -sf libopenkal.so.0 "$work/two/libopenkal.so"

echo "--- one probe, built once, against neither by name ---"
$cc -std=c11 -O1 -I"$here/include" "$here/conformance/abi/probe.c" \
    -o "$work/probe" -L"$work/one" -lopenkal -Wl,-rpath,'$ORIGIN'

# ⭐ THE ONE OBSERVATION THAT MAKES THIS A TEST OF DISTRIBUTION. The binary is
# not rebuilt between the two runs, and this is where that is asserted rather
# than assumed --- a script that rebuilt it would be running two builds and
# reporting on one.
before="$(cksum < "$work/probe")"

run() {   # run <directory> ; prints the probe's report
    ( cd "$1" && LD_LIBRARY_PATH="$PWD" "$work/probe" )
}

echo
echo "=== against the implementation it was linked against ==="
one_out="$(run "$work/one")"
printf '%s\n' "$one_out"

echo
echo "=== against one it was not ==="
two_out="$(run "$work/two")"
printf '%s\n' "$two_out"

after="$(cksum < "$work/probe")"
[ "$before" = "$after" ] || { echo "the binary changed between the runs" >&2; exit 1; }

echo
echo "=== what the two runs are required to say ==="
fail=0
expect() {   # expect <output> <key> <value> <why>
    local got
    got="$(printf '%s\n' "$1" | awk -v k="$2" '$1 == k { print $2 }')"
    if [ "$got" = "$3" ]; then
        printf 'held         %-16s %-10s %s\n' "$2" "$3" "$4"
    else
        printf 'DID NOT HOLD %-16s expected %s, got %s -- %s\n' "$2" "$3" "${got:-nothing}" "$4"
        fail=1
    fi
}

expect "$one_out" satisfies-floor yes    "the first is as new as the declarations"
expect "$one_out" granularity     4096   "and reports its own quantum"
expect "$one_out" has-space       yes    "and provides openkal.space"
expect "$one_out" exec-available  yes    "and grants executable memory"
expect "$one_out" knows-identity  yes    "and distinguishes one node from another"
expect "$one_out" kind-is-dir     yes    "and answers an enquiry"

# ⭐ THE SAME BINARY, AND EVERY ONE OF THESE IS THE OPPOSITE. Each is a branch
# that no artifact in this ecosystem had ever taken, because nothing could
# produce an implementation that answers this way.
expect "$two_out" satisfies-floor no     "the second is older than the declarations"
expect "$two_out" granularity     65536  "and reports a coarser quantum"
expect "$two_out" has-space       no     "and declines openkal.space"
expect "$two_out" exec-available  no     "and grants no executable memory"
expect "$two_out" knows-identity  no     "and does not distinguish nodes"
expect "$two_out" kind-is-dir     yes    "while still answering the enquiry"

echo
if [ "$fail" = 0 ]; then
    echo "one binary behaved as specified against both implementations"
else
    echo "the binary did not behave as specified against both" >&2
fi
exit "$fail"
