#!/usr/bin/env bash
# Compares an implementation's exported C surface against SURFACE.txt.
#
# Clause 9.3 of the specification requires this comparison. It examines the
# artefact rather than the behaviour, so it is neither slow nor subject to
# incomplete coverage, and it detects the one freedom an implementation retains
# after the language has removed the others: the addition of names.
#
#   check-surface.sh <surface-list> <object-or-archive>...
set -euo pipefail

list="${1:?usage: check-surface.sh <surface-list> <object>...}"
shift
[ "$#" -gt 0 ] || { echo "no objects given" >&2; exit 2; }

spec="$(grep -vE '^[[:space:]]*(#|$)' "$list" | sort -u)"
found="$(nm --defined-only "$@" | awk '$2=="T"||$2=="W"{print $3}' | grep '^kal_' | sort -u || true)"

status=0
while read -r name; do
    [ -n "$name" ] || continue
    if ! printf '%s\n' "$spec" | grep -qx "$name"; then
        echo "exported name is not in the specification: $name" >&2
        status=1
    fi
done <<< "$found"

# The complementary half is not an error. An implementation provides interfaces
# in whole or not at all, so a name that is absent indicates an interface the
# implementation does not provide, which clause 3 permits.
if [ "$status" -eq 0 ]; then
    echo "exported surface conforms: $(printf '%s\n' "$found" | grep -c .) name(s)"
fi
exit "$status"
