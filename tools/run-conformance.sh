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

# A path this script hands to the build tool rather than to the shell.
#
# On one of the three systems the shell and the build tool disagree about what
# a path is: the shell reports /d/a/openkal, and the tool --- which is a program
# of the system rather than of the shell --- reads that as a directory named `d'
# at the root of the current volume. The translation exists on that system and
# is a no-op everywhere else.
native() {
    if command -v cygpath > /dev/null 2>&1; then cygpath -m "$1"; else printf '%s\n' "$1"; fi
}

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
suite="$here/conformance"
[ -f "$suite/mcpp.toml" ] || { echo "no suite at $suite" >&2; exit 2; }

implementation="$(cd "$implementation" && pwd)"
[ -f "$implementation/mcpp.toml" ] || {
    echo "$implementation is not a package" >&2; exit 2; }

here_native="$(native "$here")"
implementation_native="$(native "$implementation")"

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
    sed "s|^openkal = .*$|openkal = { path = \"$here_native\" }|" "$1" > "$1.next"
    mv "$1.next" "$1"
}
# ⚠️ THE REWRITE IS UNDONE ON THE WAY OUT, AND THAT IS NOT TIDINESS.
#
# Both manifests are rewritten to name this working tree by an absolute path,
# which is right for the run and wrong for everything after it. Before this
# trap the rewrite was permanent: the tree was left holding a path from the
# machine that ran the script, and whoever committed next published it.
#
# Measured 2026-08-22: that is what happened. `conformance/mcpp.toml' reached a
# pull request naming /home/<user>/... and continuous integration failed on
# every row with `path dependency has no mcpp.toml' --- a message about a
# missing file, on a machine where nothing was missing.
#
# A local absolute path in a public repository is also the thing a standing
# constraint forbids, so the remedy is not to remember to revert.
restore_the_manifests() {
    [ -f "$suite/mcpp.toml.orig" ] && mv "$suite/mcpp.toml.orig" "$suite/mcpp.toml"
    [ -f "$implementation/mcpp.toml.orig" ] && mv "$implementation/mcpp.toml.orig" "$implementation/mcpp.toml"
    return 0
}
cp "$suite/mcpp.toml" "$suite/mcpp.toml.orig"
cp "$implementation/mcpp.toml" "$implementation/mcpp.toml.orig"
trap restore_the_manifests EXIT INT TERM

point_at_the_specification "$suite/mcpp.toml"
point_at_the_specification "$implementation/mcpp.toml"

if ! grep -q "^$package = " "$suite/mcpp.toml"; then
    # Appended immediately after openkal, which is inside [dependencies]. A
    # plain append would land under [features].
    awk -v line="$package = { path = \"$implementation_native\" }" '
        { print }
        /^openkal = / && !done { print line; done = 1 }
    ' "$suite/mcpp.toml" > "$suite/mcpp.toml.next"
    mv "$suite/mcpp.toml.next" "$suite/mcpp.toml"
fi

echo "--- the suite's dependencies ---"
sed -n '/^\[dependencies\]/,/^$/p' "$suite/mcpp.toml"

cd "$suite"
# ⚠️ A CHANGE OF FEATURE SET DOES NOT INVALIDATE THE BUILD.
#
# Measured 2026-08-22, three runs in one working tree with nothing else changed:
#
#     run-conformance.sh … full,optional   103 held, 0 not observed   (cold)
#     run-conformance.sh … full            103 held, 0 not observed   ← wrong
#     rm -rf target && … full               97 held, 1 not observed   ← right
#
# The second run reported on the first run's build. The feature set reaches the
# suite's own translation units as defines, and those defines are what decide
# which sections examine anything --- so a run that switched sets examined the
# previous set and said so in neither its output nor its exit status.
#
# This matters here more than it would elsewhere, because this workflow runs the
# suite twice in one checkout with different sets, and the second run is the
# composability check --- whose entire purpose is to observe that a smaller set
# is examined as a smaller set. It could not have observed that.
#
# The remedy is to remember which set the build in `target' was made for, and to
# discard the build when the answer changes. Re-running the same set still
# builds incrementally, which is what the record is for.
stamp="target/.features"
if [ ! -f "$stamp" ] || [ "$(cat "$stamp")" != "$features" ]; then
    rm -rf target
fi
mcpp build --features "$features" "$@"
mkdir -p target && printf '%s' "$features" > "$stamp"

binary="$(find target -type f \( -name 'openkal-conformance' -o -name 'openkal-conformance.exe' \) | head -1)"
[ -n "$binary" ] || { echo "the suite was not produced" >&2; exit 2; }

# openkal.env reads variables and does not set them, so the one observation that
# requires a variable whose value is empty requires the runner to supply it.
# Supplying it here rather than leaving it to each caller is the difference
# between ninety-one observations and ninety.
export OPENKAL_CONFORMANCE_EMPTY=""

# A suite built for a system other than the one building it is run through
# whatever runs it, named in the environment. The suite starts a copy of itself
# for the three observations that end the program that makes them, and it does
# so through openkal.process rather than through anything this script arranges,
# so the copy is started by the same mechanism as the original.
runner="${OPENKAL_CONFORMANCE_RUNNER:-}"

# The suite's own exit status is the verdict: 0 when every observation held, 1
# when one did not, 2 when nothing was observed. The last is the one a run that
# selected no interface would otherwise pass silently.
$runner "./$binary"
