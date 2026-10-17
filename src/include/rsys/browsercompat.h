#ifndef ROMLIB_BROWSERCOMPAT_H
#define ROMLIB_BROWSERCOMPAT_H

#include <stddef.h>
#include <string.h>

#define GREGS_BROWSER_BOOKMARK_SCAN_OFFSET 0x1140
#define GREGS_BROWSER_BOOKMARK_SCAN_SIZE 28

/* Greg's Browser 2.7 broadcasts bookmark updates to every C++ window,
   including its About and Preferences dialogs. Their refCons describe
   different classes, so interpreting them as browser controls corrupts
   memory. Replace only this scan's window getter, retaining the original
   null-result test and all calling conventions. Never alter the disk fork. */
static int
ROMlib_gregs_browser_bookmark_dialog_p (const unsigned char *title)
{
  static const char *names[] = {
    "About Greg's Browser", "Preferences", "Browser Docs"
  };
  unsigned i;
  if (!title)
    return 0;
  for (i = 0; i < sizeof names / sizeof names[0]; ++i)
    if (title[0] == strlen (names[i])
        && !memcmp (title + 1, names[i], title[0]))
      return 1;
  return 0;
}

static int
ROMlib_patch_gregs_browser_bookmarks (unsigned char *code, size_t size,
                                    unsigned long getter)
{
  static const unsigned char prefix[] = { 0x2f, 0x0a, 0x4e, 0xb9 };
  static const unsigned char suffix[] = {
    0x26, 0x48, 0x20, 0x08, 0x58, 0x4f, 0x67, 0x0c,
    0x3f, 0x03, 0x2f, 0x0b, 0x61, 0xff, 0x00, 0x00,
    0x18, 0x5e, 0x5c, 0x4f
  };
  unsigned char *scan;

  if (size != 37407 || !getter)
    return 0;
  scan = code + GREGS_BROWSER_BOOKMARK_SCAN_OFFSET;
  if (memcmp (scan, prefix, sizeof prefix)
      || memcmp (scan + 8, suffix, sizeof suffix)
      || scan[4] != 0 || scan[5] < 0x41)
    return 0;
  scan[4] = getter >> 24;
  scan[5] = getter >> 16;
  scan[6] = getter >> 8;
  scan[7] = getter;
  return 1;
}

#endif
