#!/usr/bin/env bash
# Clause 4.3. The specification distributes its declarations in two forms, and
# a reader is entitled to assume that the two declare the same entities.
#
# This tool examines the C form. It does not compare the two forms with each
# other: it compiles a translation unit that names every entity in
# SURFACE.txt, which is normative, so that a name the header does not declare
# fails to compile and the diagnostic names it. The C++ form is examined the
# same way, against the same list, by a conformance test in each implementation
# package, where a build of the modules already exists. Neither form is
# therefore the other's source.
#
#   check-declarations.sh [--write] [<surface-list>] [<include-dir>]
#
# The translation unit is compiled with -nostdinc, because the consumer this
# header exists for --- a C library being ported onto openkal --- is compiled
# that way, and a header that required one of the environment's own would be
# unusable by it. A check that permitted the environment's headers would pass
# for a header that cannot be used.
set -euo pipefail

write=0
if [ "${1:-}" = "--write" ]; then write=1; shift; fi

here="$(cd "$(dirname "$0")/.." && pwd)"
list="${1:-$here/SURFACE.txt}"
incdir="${2:-$here/include}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

names="$(grep -vE '^[[:space:]]*(#|$)' "$list" | sort -u)"
count="$(printf '%s\n' "$names" | grep -c .)"
[ "$count" -gt 0 ] || { echo "the surface list is empty" >&2; exit 1; }

{
  echo '#include <openkal.h>'
  echo '/* Naming each entity is what makes an absent declaration a compile'
  echo '   error. Taking the address additionally rejects a name introduced as'
  echo '   a macro, which would satisfy a textual search and satisfy nothing'
  echo '   else. */'
  echo 'const void *const okc_surface[] = {'
  while read -r n; do [ -n "$n" ] && echo "    (const void *)&$n,"; done <<< "$names"
  echo '};'
} > "$work/surface.c"

cc="${CC:-cc}"
builtin_inc="$("$cc" -print-file-name=include)"
"$cc" -std=c11 -ffreestanding -nostdinc -isystem "$builtin_inc" \
      -Wall -Wextra -Werror -Wno-unused-parameter \
      -I"$incdir" -c "$work/surface.c" -o "$work/surface.o"

echo "the C declarations are complete and compile without the environment's headers: $count name(s)"

# The same list, as a translation unit the conformance suite carries.
#
# This tool needs a driver it can pass -nostdinc to, and one of the three
# toolchains the suite is built with has no such spelling; a header that failed
# to compile as C under that toolchain would go unnoticed here. So the suite
# carries a file naming the same entities, every toolchain compiles it, and this
# is where the file is written --- and where a file that has fallen behind the
# list is detected, since a stale copy would be compiled happily by all three.
carried="$here/conformance/src/declarations.c"

if [ "$write" -eq 1 ]; then
    {
        sed -n '1,/^ \*\/$/p' "$carried"
        echo '#include <openkal.h>'
        echo
        echo 'void okc_declarations_c(void);'
        echo 'void okc_declarations_c(void)'
        echo '{'
        while read -r n; do
            [ -n "$n" ] && printf '    (void)sizeof(&%s);\n' "$n"
        done <<< "$names"
        echo '}'
    } > "$carried.next"
    mv "$carried.next" "$carried"
    echo "wrote $carried: $count name(s)"
    exit 0
fi

[ -f "$carried" ] || exit 0

carried_names="$(grep -oE '&kal_[a-z_0-9]+' "$carried" | cut -c2- | sort -u)"
if [ "$carried_names" != "$names" ]; then
    echo "conformance/src/declarations.c does not name the surface list." >&2
    echo "only in SURFACE.txt:" >&2
    comm -23 <(printf '%s\n' "$names") <(printf '%s\n' "$carried_names") >&2
    echo "only in declarations.c:" >&2
    comm -13 <(printf '%s\n' "$names") <(printf '%s\n' "$carried_names") >&2
    echo "regenerate it: tools/check-declarations.sh --write" >&2
    exit 1
fi
echo "the translation unit the suite carries names the same $count entities"
