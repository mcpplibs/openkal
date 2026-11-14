#!/usr/bin/env bash
# requires: clang
# AND THE TYPE CHECK DETECTS WHAT IT EXISTS TO DETECT. A check that succeeds by
# finding nothing succeeds identically when it reads nothing; check-types.sh
# proves it parsed the headers before reporting, and this proves it would have
# failed.
set -euo pipefail
cd "$(dirname "$0")/../.."
header=include/openkal/env.h
saved="$(mktemp)"
cp "$header" "$saved"
trap 'cp "$saved" "$header"; rm -f "$saved"' EXIT
printf '\nint kal_env_probe_leak(unsigned long n);\n' >> "$header"
if bash tools/check-types.sh "$(command -v clang)" include > /dev/null 2>&1; then
    echo "the type check accepted 'unsigned long' in an interface" >&2
    exit 1
fi
