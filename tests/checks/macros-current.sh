#!/usr/bin/env bash
# EVERY MACRO HAS A MODULE SPELLING, AND THIS IS CHECKED RATHER THAN
# REMEMBERED --- because twice it was not. `KAL_LOCK_*' in 0.10 and
# `KAL_SPAWN_*' in 0.11 were each added to a header, used from a module
# consumer, and failed to compile there, because a macro does not cross a
# module boundary. src/macros.cppm is therefore generated (tools/gen-macros.sh),
# and regenerating and comparing turns "somebody forgot" into a failure here.
set -euo pipefail
cd "$(dirname "$0")/../.."
bash tools/gen-macros.sh
git diff --exit-code src/macros.cppm || {
    echo "src/macros.cppm is out of date: run tools/gen-macros.sh and commit it" >&2
    exit 1
}
