#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root/executor/src"
mkdir -p "$root/build"
i686-w64-mingw32-gcc -std=gnu17 -g -O0 \
  -ffunction-sections -fdata-sections -Wl,--gc-sections -DHAVE_CONFIG_H \
  -I. -Iinclude -Iconfig/front-ends/sdl -Iconfig/os/mingw32 \
  -Iconfig/arch/i386 -Iconfig/os/cygwin32 -I"$root/win32-prefix/include" \
  ../tests/sidecar_test.c -o "$root/build/sidecar-test.exe"
run_dir=$(mktemp -d "$root/build/sidecar-test.XXXXXX")
cd "$run_dir"
"$root/build/sidecar-test.exe" > stdout.txt 2> stderr.txt
cat stdout.txt
printf 'Fixtures and logs: %s\n' "$run_dir"
