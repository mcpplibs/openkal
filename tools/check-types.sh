#!/usr/bin/env bash
# Asserts that no type from a concrete backend appears in the specification's
# interface.
#
#   check-types.sh <compiler> [include-dir]
#
# ⭐⭐ WHAT THIS CHECKS, AND WHY IT IS NOT THE SAME AS check-surface.sh.
#
# `check-surface.sh` asks which NAMES an implementation exports. This asks what
# those names are DECLARED WITH. They are different freedoms: an implementation
# can conform to the surface exactly and still take a `size_t`, and then the
# interface has an ABI that depends on the machine's data model rather than on
# this specification.
#
# The rule the specification states:
#
#   > ABI is guaranteed by openkal, and the layer above uses nothing from a
#   > concrete backend — including types.
#
# `openkal/types.h` derives every type from the COMPILER (`__UINTPTR_TYPE__`,
# `__UINT64_TYPE__`, …) rather than from any C library header, which is what
# makes the same `kal_*` signature byte-identical on ELF, Mach-O, PE and bare
# metal. A `long` in a declaration would undo that: `sizeof(long)` is 8 on
# x86_64 Linux and 4 on x86_64 Windows, and the two would be one signature with
# two meanings.
#
# ⚠️ MEASURED, AND IT WENT THE OTHER WAY ROUND. 2026-08-23, cross-compiling for
# `arm64-apple-macos`:
#
#     okm_syscall.c:439: incompatible pointer types passing 'uint64_t *'
#       (aka 'unsigned long *') to 'kal_u64 *' (aka 'unsigned long long *')
#
# Two 64-bit types, same width, not convertible. `kal_u64` came from the
# compiler and was RIGHT — Apple's ABI spells it `unsigned long long`. The
# `uint64_t` came from musl's headers and carried Linux's answer. The type
# discipline caught the C library's mistake, not the other way round; the C
# library is the one layer that must know the target ABI, because it rebuilds
# POSIX and POSIX itself names `long`.
#
# ⚠️ AND WHY THIS ASKS THE COMPILER RATHER THAN grep. A text search over the
# headers can be defeated by a macro, cannot see through an include, and — the
# failure mode that matters — reports success when it matches nothing, which is
# also what it does when the files moved. `-ast-print` prints what the compiler
# actually parsed, with typedef names intact rather than desugared, so a
# declaration written `kal_u64` stays `kal_u64` and one written `uint64_t` stays
# `uint64_t`.
set -euo pipefail

cc="${1:?usage: check-types.sh <compiler> [include-dir]}"
inc="${2:-include}"

[ -d "$inc/openkal" ] || { echo "no $inc/openkal directory" >&2; exit 2; }

tu="$(mktemp -t openkal-types-XXXXXX.c)"
out="$(mktemp -t openkal-ast-XXXXXX.txt)"
trap 'rm -f "$tu" "$out"' EXIT

for h in "$inc"/openkal/*.h; do
    printf '#include <openkal/%s>\n' "$(basename "$h")"
done > "$tu"

"$cc" -fsyntax-only -I "$inc" -Xclang -ast-print "$tu" > "$out" 2>/dev/null || {
    echo "the compiler could not print the AST for the headers" >&2
    exit 2
}

# ⚠️ THE POSITIVE CONTROL, BEFORE THE CHECK AND NOT AFTER IT.
#
# The check below succeeds when it finds nothing, and an empty AST also finds
# nothing. Every previous false green in this ecosystem had this shape, so the
# script proves it read something before it is allowed to report success.
decls="$(grep -c 'kal_' "$out" || true)"
if [ "$decls" -lt 40 ]; then
    echo "only $decls declarations mention kal_; the headers were not parsed" >&2
    exit 1
fi
grep -q 'kal_stream_write' "$out" || {
    echo "a known interface (kal_stream_write) is absent from the AST" >&2
    exit 1
}

# The types a declaration may not name. Everything a backend defines: the
# language's own width-dependent spellings, and the C library's aliases for
# them.
forbidden='\b(long|short|unsigned|signed|float|double|wchar_t|_Bool|'\
'size_t|ssize_t|ptrdiff_t|intptr_t|uintptr_t|intmax_t|uintmax_t|'\
'u?int(8|16|32|64)_t|u?int_(least|fast)(8|16|32|64)_t|'\
'time_t|off_t|mode_t|pid_t|dev_t|ino_t|clock_t|va_list|FILE)\b'

# ⚠️ `typedef` LINES ARE EXCLUDED, AND THAT IS THE POINT RATHER THAN AN
# EXCEPTION. `types.h` is where a naked type is allowed to appear, because that
# is the one place whose job is to name one — and it names the COMPILER's
# (`__UINTPTR_TYPE__`), which `-ast-print` renders as the underlying type.
# Everything that is not a typedef is a declaration in the interface.
leaks="$(grep -vE '^[[:space:]]*typedef' "$out" | grep -nE "$forbidden" || true)"

if [ -n "$leaks" ]; then
    echo "a backend type appears in the openkal interface:" >&2
    printf '%s\n' "$leaks" >&2
    echo >&2
    echo "ABI is guaranteed by openkal; a declaration names kal_* or nothing." >&2
    echo "If the type is genuinely new, it belongs in openkal/types.h, derived" >&2
    echo "from the compiler (__UINT64_TYPE__ and its relatives) and never from" >&2
    echo "a C library header." >&2
    exit 1
fi

echo "interface types conform: $decls declarations, no backend type named"
