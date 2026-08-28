#!/usr/bin/env bash
# The versions a README tells a reader to write must be the versions that exist.
#
#   check-readme-versions.sh [<package-directory>]
#
# ⚠️⚠️ A README THAT NAMES A VERSION DRIFTS SILENTLY, AND THIS ONE HAD.
#
# Every README in this ecosystem opens by showing what a program writes in its
# manifest. Those lines are the first thing a reader copies and the last thing
# anyone edits: when this check was written, the specification's own README told
# a reader to ask for `openkal = "0.5.1"` while the package was at 0.9.0 --- four
# minor versions, each of which had changed the surface. A reader following it
# got a version whose declarations do not match the documentation around them,
# and nothing said so.
#
# ⭐ THE POINT IS NOT THE STALENESS, IT IS THAT IT WAS INVISIBLE. Everything else
# in these packages is checked by something: the surface against SURFACE.txt, the
# declarations against both forms, the behaviour against the conformance suite.
# The one thing a reader actually types was checked by nobody.
#
# The rule is narrow on purpose: a line of the form `<package> = "<version>"` in
# a README must agree with that package's own manifest where the package is one
# of this ecosystem's. A version of anything else is not this check's business.
set -euo pipefail

here="$(cd "${1:-$(dirname "${BASH_SOURCE[0]}")/..}" && pwd)"
beside="$(cd "$here/.." && pwd)"
readme="$here/README.md"
[ -f "$readme" ] || { echo "no README at $here" >&2; exit 2; }

version_of() {   # version_of <package> --- from the package's own manifest
    local pkg="$1" at
    for at in "$beside/$pkg" "$here"; do
        if [ -f "$at/mcpp.toml" ] &&
           [ "$(sed -n 's/^name *= *"\([^"]*\)".*/\1/p' "$at/mcpp.toml" | head -1)" = "$pkg" ]; then
            sed -n 's/^version *= *"\([^"]*\)".*/\1/p' "$at/mcpp.toml" | head -1
            return
        fi
    done
}

fail=0
checked=0
while IFS= read -r line; do
    pkg="${line%% *}"
    want="$(printf '%s' "$line" | sed -n 's/.*= *"\([^"]*\)".*/\1/p')"
    have="$(version_of "$pkg" || true)"

    # A package whose tree is not beside this one cannot be checked, and saying
    # so is better than passing: a check that is silent when it cannot look is a
    # check whose "no" and whose "did not run" read the same.
    if [ -z "$have" ]; then
        echo "  ?  $pkg = \"$want\" -- no tree beside this one to compare against"
        continue
    fi
    checked=$((checked + 1))
    if [ "$want" = "$have" ]; then
        echo "  ok $pkg = \"$want\""
    else
        echo "  NO $pkg = \"$want\" -- the package is at $have"
        fail=1
    fi
done < <(grep -oE '^openkal[a-z-]* = "[0-9][0-9.]*"' "$readme" | sort -u)

if [ "$checked" = 0 ]; then
    echo "no version of this ecosystem's packages appears in $readme" >&2
    exit 0
fi
[ "$fail" = 0 ] && echo "the README asks for versions that exist: $checked checked"
exit "$fail"
