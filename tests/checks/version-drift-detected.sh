#!/usr/bin/env bash
# AND THE VERSION CHECK DETECTS WHAT IT EXISTS TO DETECT: a check that reports
# success by finding nothing reports it identically when it has read nothing.
# The header is written beside and moved back, since `sed -i' is not portable.
set -euo pipefail
cd "$(dirname "$0")/../.."
header=include/openkal/version.h
saved="$(mktemp)"
cp "$header" "$saved"
trap 'cp "$saved" "$header"; rm -f "$saved"' EXIT
sed 's/^#define KAL_VERSION_MINOR .*/#define KAL_VERSION_MINOR 11u/' "$saved" > "$header"
if bash tools/check-version.sh > /dev/null 2>&1; then
    echo "the version check accepted a number the package is not" >&2
    exit 1
fi
