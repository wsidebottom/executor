#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$root/build"
"${CC:-cc}" -std=gnu17 -g -O0 -Wall -Wextra -Werror \
  -I"$root/executor/src/include" "$root/executor/tests/app_icon_test.c" \
  -o "$root/build/app-icon-test"
"$root/build/app-icon-test"
