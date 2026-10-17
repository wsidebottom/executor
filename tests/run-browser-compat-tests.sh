#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$root/build"
i686-w64-mingw32-gcc -g -O2 \
  -I"$root/syn68k/include" -I"$root/executor/src/include" \
  "$root/executor/tests/browser_compat_test.c" \
  "$root/win32-prefix/lib/libsyn68k.a" -lm \
  -o "$root/build/browser-compat-test.exe"
"$root/build/browser-compat-test.exe" \
  > "$root/build/browser-compat-test.stdout.txt" \
  2> "$root/build/browser-compat-test.stderr.txt"
cat "$root/build/browser-compat-test.stdout.txt"
