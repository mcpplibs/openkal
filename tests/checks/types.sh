#!/usr/bin/env bash
# requires: clang
# NO DECLARATION NAMES A BACKEND TYPE. The declarations compile (declarations-c.sh);
# this asserts what they are written with: `sizeof(long)' is 8 on x86_64 Linux and
# 4 on x86_64 Windows, so a `long' in an interface is one signature with two
# meanings. Needs a clang-family driver (`-Xclang -ast-print').
set -euo pipefail
cd "$(dirname "$0")/../.."
bash tools/check-types.sh "$(command -v clang)" include
