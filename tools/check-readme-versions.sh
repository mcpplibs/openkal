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
# The rule is narrow on purpose: a line of the form `<package> = "<version>"`
# must agree with that package's own manifest where the package is one of this
# ecosystem's. A version of anything else is not this check's business.
#
# ⚠️⚠️ AND IT IS NOT ONLY READMES. This checked READMEs alone until 2026-08-28,
# when a change spanning eight repositories was reviewed and `openkal-musl`'s
# own manifest was found pinning `openkal-windows = "0.3.0"` against a package
# that had moved to 0.4.0. The README beside it was correct, because the README
# was the thing being checked.
#
# ⭐ A MANIFEST PIN IS THE SAME CLASS OF FACT AS A README LINE --- a version of
# a sibling written down here and true somewhere else --- so it is checked by
# the same rule. Example manifests are included: an example is a README a reader
# can build.
set -euo pipefail

here="$(cd "${1:-$(dirname "${BASH_SOURCE[0]}")/..}" && pwd)"
beside="$(cd "$here/.." && pwd)"
readme="$here/README.md"
[ -f "$readme" ] || { echo "no README at $here" >&2; exit 2; }

# The files whose version lines are a promise to somebody: the README a reader
# copies from, and every manifest in this tree that names a sibling. `target'
# is excluded because a build directory holds copies of manifests this tree
# does not own.
# ⚠️ `-L', BECAUSE `$here' MAY BE A SYMBOLIC LINK. `find' does not descend into
# one unless told to, and when this was run against a tree reached by link it
# examined the README and NOTHING ELSE --- reporting "1 files" and passing.
sources() {
    printf '%s\n' "$readme"
    find -L "$here" -name mcpp.toml -not -path '*/target/*' -not -path '*/.spec/*' \
         -not -path '*/.impl/*' | sort
}

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

# ⭐ A DENOMINATOR. The count of pins alone cannot distinguish a tree with few
# pins from a survey that stopped early, which is the defect recorded below.
seen="$(mktemp)"
trap 'rm -f "$seen"' EXIT

fail=0
checked=0
absent=0
while IFS= read -r line; do
    where="${line%%|*}"
    line="${line#*|}"
    pkg="${line%% *}"
    want="$(printf '%s' "$line" | sed -n 's/.*= *"\([^"]*\)".*/\1/p')"
    have="$(version_of "$pkg" || true)"

    # A package whose tree is not beside this one cannot be checked, and saying
    # so is better than passing: a check that is silent when it cannot look is a
    # check whose "no" and whose "did not run" read the same.
    if [ -z "$have" ]; then
        echo "  ?  $where: $pkg = \"$want\" -- no tree beside this one to compare against"
        absent=$((absent + 1))
        continue
    fi
    checked=$((checked + 1))
    if [ "$want" = "$have" ]; then
        echo "  ok $where: $pkg = \"$want\""
    else
        echo "  NO $where: $pkg = \"$want\" -- the package is at $have"
        fail=1
    fi
done < <(
    while IFS= read -r file; do
        [ -f "$file" ] || continue
        # A pin in either form: `pkg = "1.2.3"' and `pkg = { version = "1.2.3" ...'.
        #
        # ⚠️⚠️ `|| true' IS LOAD-BEARING AND ITS ABSENCE TRUNCATED THIS SURVEY
        # WITHOUT SAYING SO. A manifest whose dependencies are all path form ---
        # `examples/substitution/app/mcpp.toml' is one --- matches nothing, so
        # grep exits 1; under `set -e' with `pipefail' that ended the LOOP, and
        # every file sorting after it was never examined. Measured: the check
        # reported nine pins and there were eleven, and it reported them with
        # the same "ok" it uses when it has looked at everything.
        grep -oE '^[[:space:]]*openkal[a-z-]* *= *("[0-9][0-9.]*"|\{ *version *= *"[0-9][0-9.]*")' "$file" |
            sed -e 's/^[[:space:]]*//' -e 's/ *= *{ *version *= */ = /' |
            sed "s|^|${file#"$here"/}\||" || true
        echo "${file#"$here"/}|" >> "$seen"
    done < <(sources) | sort -u
)

if [ "$checked" = 0 ]; then
    echo "no version of this ecosystem's packages could be compared ($absent not beside this tree)" >&2
    exit 0
fi
files="$(sort -u "$seen" | wc -l)"
want="$(sources | wc -l)"
if [ "$files" != "$want" ]; then
    echo "the survey examined $files of $want files; it stopped early" >&2
    exit 1
fi

# ⭐⭐ A DENOMINATOR DRAWN FROM THE SAME ENUMERATION CANNOT REPORT THAT THE
# ENUMERATION IS EMPTY. The count above compares the survey against `sources',
# so an enumeration that found nothing agrees with a survey that examined
# nothing and the check passes. This is the floor that does not come from it:
# every package in this ecosystem has a manifest at its root, so a survey that
# did not reach that file did not reach this package.
if ! sort -u "$seen" | grep -qx 'mcpp.toml|'; then
    echo "the survey did not reach $here/mcpp.toml, so it did not examine this package" >&2
    exit 1
fi
[ "$fail" = 0 ] && echo "every version named here exists: $checked checked in $files files, $absent not beside this tree"
exit "$fail"
