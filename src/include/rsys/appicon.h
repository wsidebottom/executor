#ifndef RSYS_APPICON_H
#define RSYS_APPICON_H

/* Resource bytes are big endian and need not be aligned. */
static unsigned
ROMlib_icon_word (const unsigned char *p)
{
  return (p[0] << 8) | p[1];
}

static int
ROMlib_bundle_icon_id (const unsigned char *p, unsigned long size,
                      unsigned long creator, unsigned local_id, short *id)
{
  unsigned long offset = 8, types, i;
  if (size < 8 || (((unsigned long) p[0] << 24)
      | ((unsigned long) p[1] << 16) | (p[2] << 8) | p[3]) != creator)
    return 0;
  types = ROMlib_icon_word (p + 6) + 1UL;
  for (i = 0; i < types; ++i)
    {
      unsigned long pairs, j;
      int icons;
      if (offset > size || size - offset < 6)
        return 0;
      icons = p[offset] == 'I' && p[offset+1] == 'C'
        && p[offset+2] == 'N' && p[offset+3] == '#';
      pairs = ROMlib_icon_word (p + offset + 4) + 1UL;
      offset += 6;
      if (pairs > (size - offset) / 4)
        return 0;
      for (j = 0; j < pairs; ++j, offset += 4)
        if (icons && ROMlib_icon_word (p + offset) == local_id)
          {
            *id = (short) ROMlib_icon_word (p + offset + 2);
            return 1;
          }
    }
  return 0;
}

#endif
