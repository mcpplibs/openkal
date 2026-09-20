#!/usr/bin/env bash
# The version the declarations state must be the version the package is.
#
#   check-version.sh [<package-directory>]
#
# WHAT THIS CHECKS AND WHY NOTHING ELSE DOES. `KAL_VERSION_MAJOR`, `_MINOR` and
# `_PATCH` in include/openkal/version.h are not documentation. Every conforming
# implementation answers `kal_version' with the constant they compose --- each
# one is literally `return KAL_VERSION;' --- so the number written here is the
# number the whole ecosystem states about itself, and it is the only number a
# consumer bound at load or across a boundary can ask for. Clause 3.2 names that
# consumer; clause 6.2 gives it the comparison to make.
#
# IT HAD DRIFTED, AND THE DRIFT WAS INVISIBLE. Releases 0.12.0 and 0.13.0 both
# went out with 11 written above. The comparison a consumer makes --- refuse an
# implementation older than the declarations I hold --- was therefore between
# two equal numbers on every implementation there is, and passed for the reason
# that neither side had moved. Nothing reported it: the surface is checked
# against SURFACE.txt, the declarations against both forms, the README's pins
# against the manifests, and this one fact against nobody.
#
# THE RULE IS THE SAME ONE tools/check-readme-versions.sh STATES. A version
# written in one file and true in another is a fact with two places to be
# wrong, so the two places are compared. Here the manifest is the authority,
# because it is what the index publishes and what a consumer resolves.
set -euo pipefail

here="$(cd "${1:-$(dirname "${BASH_SOURCE[0]}")/..}" && pwd)"
header="$here/include/openkal/version.h"
manifest="$here/mcpp.toml"
[ -f "$header" ]   || { echo "no include/openkal/version.h at $here" >&2; exit 2; }
[ -f "$manifest" ] || { echo "no mcpp.toml at $here" >&2; exit 2; }

field() {   # field <name> --- the integer a KAL_VERSION_<name> define carries
    sed -n "s/^#define  *KAL_VERSION_$1  *\([0-9][0-9]*\)u\{0,1\}.*/\1/p" "$header" | head -1
}

major="$(field MAJOR)"; minor="$(field MINOR)"; patch="$(field PATCH)"
# A CHECK THAT READS NOTHING MUST NOT REPORT SUCCESS, which is the rule clause 9
# states for the declaration checks and applies here for the same reason: a
# renamed macro would leave all three empty and every comparison vacuous.
for f in major minor patch; do
    [ -n "${!f}" ] || { echo "  no  KAL_VERSION_${f^^} is not declared in version.h"; exit 1; }
done

declared="$major.$minor.$patch"
packaged="$(sed -n 's/^version *= *"\([^"]*\)".*/\1/p' "$manifest" | head -1)"
[ -n "$packaged" ] || { echo "  no  the manifest declares no version"; exit 1; }

if [ "$declared" = "$packaged" ]; then
    echo "  ok  the declarations state $declared, which is what this package is"
    exit 0
fi

echo "  no  version.h states $declared and the package is $packaged"
echo "      every implementation answers kal_version with the first of those,"
echo "      so a consumer comparing them is told the wrong thing about all of"
echo "      them. Edit include/openkal/version.h to $packaged."
exit 1
