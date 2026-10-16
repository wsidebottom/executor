/* Expand Macintosh packed pixels into SDL's byte-sized palette indices. */
#ifndef RSYS_SDLPIXELS_H
#define RSYS_SDLPIXELS_H

#include <string.h>

static inline void
ROMlib_sdl_copy_indexed_row (unsigned char *dst, const unsigned char *src,
                            int left, int right, int bpp)
{
  int x;

  if (bpp == 8)
    memcpy (dst + left, src + left, right - left);
  else
    for (x = left; x < right; ++x)
      dst[x] = (src[(x * bpp) >> 3] >> (8 - bpp - ((x * bpp) & 7)))
               & ((1 << bpp) - 1);
}

#endif
