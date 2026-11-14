#!/usr/bin/env bash
# requires: cc
# The C form, with the environment's own headers excluded, because the consumer
# this form exists for (a C library being ported onto openkal) is compiled that
# way; under each C compiler this machine has. A toolchain without -nostdinc
# compiles the same declarations in the translation unit the conformance suite
# carries (conformance/src/declarations.c).
set -euo pipefail
cd "$(dirname "$0")/../.."
CC=cc bash tools/check-declarations.sh
if clang --version > /dev/null 2>&1; then CC=clang bash tools/check-declarations.sh; fi
