#!/usr/bin/env bash
# WHAT THE DECLARATIONS SAY THE VERSION IS, AND WHAT THE PACKAGE IS.
# `kal_version' is `return KAL_VERSION;' in every implementation, so the three
# constants of version.h are what a consumer bound at load can ask. They had
# drifted through two releases, and the comparison clause 6.2 gives that consumer
# was between two equal numbers the whole time.
set -euo pipefail
cd "$(dirname "$0")/../.."
bash tools/check-version.sh
