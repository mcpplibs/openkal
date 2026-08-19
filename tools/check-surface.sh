#!/usr/bin/env bash
# Compares an implementation's exported C surface against SURFACE.txt.
#
# Clause 9 of the specification requires this comparison. It examines the
# artefact rather than the behaviour, so it is neither slow nor subject to
# incomplete coverage, and it detects the one freedom an implementation retains
# after the language has removed the others: the addition of names.
#
#   check-surface.sh [--complete] <surface-list> <object-or-archive>...
#
# Without --complete, an absent name denotes an interface the implementation
# does not provide, which clause 3 permits. With --complete, every name in the
# list is required, which is what an implementation claiming the whole
# specification asserts.
set -euo pipefail

complete=0
if [ "${1:-}" = "--complete" ]; then complete=1; shift; fi

list="${1:?usage: check-surface.sh [--complete] <surface-list> <object>...}"
shift
[ "$#" -gt 0 ] || { echo "no objects given" >&2; exit 2; }

spec="$(grep -vE '^[[:space:]]*(#|$)' "$list" | sort -u)"

# T and W are functions; R, D, B and S are data. All are exported and all are
# therefore part of the surface. A checker that inspected only text would report
# a conforming surface for an implementation that had added a capability word.
#
# A leading underscore is removed before comparison. Some systems prefix C
# symbols with one and some do not, and a checker that ignored the difference
# would find no names at all on one of them — and would report that as success,
# because an empty surface contains nothing unspecified. The defect was found by
# writing a second implementation, which is what a second implementation is for.
found="$(nm --defined-only "$@" \
  | awk '$2=="T"||$2=="W"||$2=="R"||$2=="D"||$2=="B"||$2=="S"{print $3}' \
  | sed 's/^_//' \
  | grep '^kal_' | sort -u || true)"

# An empty surface is never a conforming one. Every implementation exports
# something, so nothing found means the objects were wrong or the symbols were
# not recognised, and reporting success would conceal both.
if [ -z "$found" ]; then
    echo "no exported name beginning with kal_ was found; the objects or the symbol format are wrong" >&2
    exit 1
fi

status=0
while read -r name; do
    [ -n "$name" ] || continue
    if ! printf '%s\n' "$spec" | grep -qx "$name"; then
        echo "exported name is not in the specification: $name" >&2
        status=1
    fi
done <<< "$found"

if [ "$complete" -eq 1 ]; then
    while read -r name; do
        [ -n "$name" ] || continue
        if ! printf '%s\n' "$found" | grep -qx "$name"; then
            echo "specified name is not exported: $name" >&2
            status=1
        fi
    done <<< "$spec"
fi

if [ "$status" -eq 0 ]; then
    n=$(printf '%s\n' "$found" | grep -c .)
    if [ "$complete" -eq 1 ]; then
        echo "exported surface is complete and conforms: $n name(s)"
    else
        echo "exported surface conforms: $n name(s)"
    fi
fi
exit "$status"
