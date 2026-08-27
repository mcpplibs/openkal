#!/usr/bin/env bash
# Runs openkal-kit's tests against a working tree of an implementation.
#
#   run-kit-tests.sh <implementation-package> <path-to-implementation>
#
# The kit's manifest names the specification by a path --- it is in the same
# repository --- and an implementation by version, which is what a published
# manifest must say. Neither form is what a run spanning two branches needs, so
# both are rewritten here to name the working trees, exactly as
# run-conformance.sh does and for the same reason: a change spanning the
# specification and an implementation must be tested against both halves as
# written rather than against whichever half is published.
#
# THE REWRITE MUST NOT SURVIVE THE RUN. run-conformance.sh records that leaving
# one behind put a local absolute path into a public repository; the trap below
# is why that cannot happen here.
set -euo pipefail

package="${1:?usage: run-kit-tests.sh <package> <path-to-implementation>}"
implementation="${2:?usage: run-kit-tests.sh <package> <path-to-implementation>}"
# Consumed, so that "$@" below carries only what the caller meant for the build
# tool. Without this the two arguments above are handed to `mcpp test` as test
# filters, no test matches, and the run reports success having examined nothing.
shift 2

here="$(cd "$(dirname "$0")/.." && pwd)"
impl="$(cd "$implementation" && pwd)"

native() {   # a path the build tool understands, on every system it runs on
    if command -v cygpath > /dev/null 2>&1; then cygpath -m "$1"; else printf '%s\n' "$1"; fi
}
here_native="$(native "$here")"
impl_native="$(native "$impl")"

kit="$here/kit"
manifests=("$kit/mcpp.toml" "$impl/mcpp.toml")

restore() {
    for m in "${manifests[@]}"; do
        [ -f "$m.orig" ] || continue
        mv -f "$m.orig" "$m"
    done
}
trap restore EXIT

for m in "${manifests[@]}"; do cp "$m" "$m.orig"; done

# The implementation reaches the specification by whatever its own manifest says
# --- a released version, or a branch. Both are replaced by this working tree, so
# that what is tested is what is written here.
sed -i.bak -E "s|^openkal = .*$|openkal = { path = \"$here_native\" }|" "$impl/mcpp.toml"
rm -f "$impl/mcpp.toml.bak"

# And the kit reaches the implementation by version; that becomes this tree.
sed -i.bak -E "s|^${package} = \{ version = \"[^\"]*\"(.*)$|${package} = { path = \"$impl_native\"\1|" \
    "$kit/mcpp.toml"
rm -f "$kit/mcpp.toml.bak"

# ⚠️ AND THE KIT REACHES THE SPECIFICATION BY VERSION TOO, WHICH IT DID NOT USED
# TO. While that line read `path = ".."` it already named this working tree and
# needed no substitution. It now names a published version --- because a path
# there made the package unusable alongside any implementation --- so without
# this line these tests would run against the PUBLISHED specification while
# claiming to test the one written here, and a change to the specification would
# be invisible to them.
sed -i.bak -E "s|^openkal = .*$|openkal = { path = \"$here_native\" }|" "$kit/mcpp.toml"
rm -f "$kit/mcpp.toml.bak"

# ⚠️ ASSERTED RATHER THAN ASSUMED. A substitution that matched nothing leaves the
# manifest naming a version, the resolver fetches a published implementation, and
# the run reports on that one while appearing to report on this branch.
grep -q "path = \"$impl_native\"" "$kit/mcpp.toml" \
    || { echo "the implementation substitution matched nothing in kit/mcpp.toml" >&2; exit 2; }
grep -q "path = \"$here_native\"" "$impl/mcpp.toml" \
    || { echo "the specification substitution matched nothing in $impl/mcpp.toml" >&2; exit 2; }
grep -q "path = \"$here_native\"" "$kit/mcpp.toml" \
    || { echo "the specification substitution matched nothing in kit/mcpp.toml" >&2; exit 2; }

echo "--- the kit's dependencies ---"
sed -n '/^\[dependencies\]/,/^$/p;/dev-dependencies/,+2p' "$kit/mcpp.toml"
echo

( cd "$kit" && mcpp test "$@" )
