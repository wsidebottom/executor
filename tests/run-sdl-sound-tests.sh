#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root/executor/src"
mkdir -p "$root/build"
i686-w64-mingw32-gcc -std=gnu17 -g -O0 \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -DHAVE_CONFIG_H -DNDEBUG \
  -I. -Iinclude -Iconfig/front-ends/sdl -Iconfig/sound/sdl \
  -Iconfig/os/mingw32 -Iconfig/arch/i386 -Iconfig/os/cygwin32 \
  -I"$root/win32-prefix/include" ../tests/sdl_sound_test.c \
  -L"$root/win32-prefix/lib" -lSDL -lwinpthread \
  -o "$root/build/sdl-sound-test.exe"
cd "$root/windows-binaries/bin"
"$root/build/sdl-sound-test.exe"
