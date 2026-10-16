#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$root/build"
i686-w64-mingw32-gcc -std=gnu17 -g -O0 \
  "$root/executor/tests/mace_test.c" -o "$root/build/mace-test.exe"
"$root/build/mace-test.exe"
