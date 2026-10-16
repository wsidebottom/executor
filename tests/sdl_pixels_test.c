/* Check all clipping ranges against known palette indices at each depth. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "rsys/sdlpixels.h"

static void
check_depth (const unsigned char *packed, const unsigned char *expected,
             int width, int bpp)
{
  unsigned char guarded[24];
  int left, right, x;

  for (left = 0; left <= width; ++left)
    for (right = left; right <= width; ++right)
      {
        memset (guarded, 0xCC, sizeof guarded);
        ROMlib_sdl_copy_indexed_row (guarded + 2, packed, left, right, bpp);
        for (x = 0; x < (int) sizeof guarded; ++x)
          assert (guarded[x] == (x >= left + 2 && x < right + 2
                                ? expected[x - 2] : 0xCC));
      }
}

int
main (void)
{
  static const unsigned char mono[] = { 0xA5, 0x5A };
  static const unsigned char mono_pixels[] =
    { 1,0,1,0,0,1,0,1, 0,1,0,1,1,0,1,0 };
  static const unsigned char two[] = { 0x1B, 0xE4 };
  static const unsigned char two_pixels[] = { 0,1,2,3,3,2,1,0 };
  static const unsigned char four[] =
    { 0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF };
  static const unsigned char four_pixels[] =
    { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
  static const unsigned char eight[] = { 7,3,254,0,99 };

  check_depth (mono, mono_pixels, 16, 1);
  check_depth (two, two_pixels, 8, 2);
  check_depth (four, four_pixels, 16, 4);
  check_depth (eight, eight, 5, 8);
  puts ("SDL packed-pixel tests passed (1, 2, 4 and 8 bpp)");
  return 0;
}
