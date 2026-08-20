#!/usr/bin/env bash
# Runs the conformance suite against one implementation.
#
#   run-conformance.sh <implementation-package> <path-to-implementation> [features] [extra mcpp arguments]
#
# The suite's manifest names openkal and does not name an implementation: the
# implementation is supplied by whoever runs the suite, which is what makes the
# suite an instrument rather than an implementation's dependant. Supplying it is
# a few lines of manifest, and those lines are here rather than repeated in
# every implementation's continuous integration, so that a change to how it is
# done is one change.
#
# Both working trees are modified in place. That is intended: this runs in
# checkouts that exist for the length of one job.
set -euo pipefail

package="${1:?usage: run-conformance.sh <package> <path> [features] [mcpp arguments...]}"
implementation="${2:?usage: run-conformance.sh <package> <path> [features] [mcpp arguments...]}"
features="${3:-full}"
shift 3 2>/dev/null || shift $#

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
suite="$here/conformance"
[ -f "$suite/mcpp.toml" ] || { echo "no suite at $suite" >&2; exit 2; }

implementation="$(cd "$implementation" && pwd)"
[ -f "$implementation/mcpp.toml" ] || {
    echo "$implementation is not a package" >&2; exit 2; }

# The specification is taken from this working tree rather than from the version
# the manifests name, so that a run asserts what it is for: that the
# specification as written here and the implementation as written there agree
# today.
#
# It is rewritten in both manifests and not in one. A package may be reached by
# a path or by a version and not by both at once, and the two manifests reach
# openkal independently; rewriting one of them produces a resolution failure
# rather than a build.
#
# Not `sed -i`: BSD sed reads the next word as a backup suffix and so the GNU
# form fails on macOS. This form is the same on every system.
point_at_the_specification() {
    sed "s|^openkal = .*$|openkal = { path = \"$here\" }|" "$1" > "$1.next"
    mv "$1.next" "$1"
}
point_at_the_specification "$suite/mcpp.toml"
point_at_the_specification "$implementation/mcpp.toml"

if ! grep -q "^$package = " "$suite/mcpp.toml"; then
    # Appended immediately after openkal, which is inside [dependencies]. A
    # plain append would land under [features].
    awk -v line="$package = { path = \"$implementation\" }" '
        { print }
        /^openkal = / && !done { print line; done = 1 }
    ' "$suite/mcpp.toml" > "$suite/mcpp.toml.next"
    mv "$suite/mcpp.toml.next" "$suite/mcpp.toml"
fi

echo "--- the suite's dependencies ---"
sed -n '/^\[dependencies\]/,/^$/p' "$suite/mcpp.toml"

cd "$suite"
mcpp build --features "$features" "$@"

binary="$(find target -type f \( -name 'openkal-conformance' -o -name 'openkal-conformance.exe' \) | head -1)"
[ -n "$binary" ] || { echo "the suite was not produced" >&2; exit 2; }

# openkal.env reads variables and does not set them, so the one observation that
# requires a variable whose value is empty requires the runner to supply it.
# Supplying it here rather than leaving it to each caller is the difference
# between ninety-one observations and ninety.
export OPENKAL_CONFORMANCE_EMPTY=""

# The suite's own exit status is the verdict: 0 when every observation held, 1
# when one did not, 2 when nothing was observed. The last is the one a run that
# selected no interface would otherwise pass silently.
"./$binary"
