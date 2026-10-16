#include <assert.h>
#include <stdio.h>
#include "rsys/appicon.h"

int
main (void)
{
  /* The application icon is local ID 7, mapped to resource 300. */
  unsigned char bundle[] = {
    'T','E','S','T', 0,0, 0,1,
    'F','R','E','F', 0,0, 0,0, 0,128,
    'I','C','N','#', 0,1, 0,0, 0,129, 0,7, 1,44
  };
  short id = -1;
  unsigned long size;
  assert (ROMlib_bundle_icon_id (bundle, sizeof bundle, 0x54455354UL, 7, &id));
  assert (id == 300);
  for (size = 0; size < sizeof bundle; ++size)
    assert (!ROMlib_bundle_icon_id (bundle, size, 0x54455354UL, 7, &id));
  assert (!ROMlib_bundle_icon_id (bundle, sizeof bundle, 0x4e4f5045UL, 7, &id));
  assert (!ROMlib_bundle_icon_id (bundle, sizeof bundle, 0x54455354UL, 6, &id));
  bundle[22] = bundle[23] = 255; /* Invalid entry count. */
  assert (!ROMlib_bundle_icon_id (bundle, sizeof bundle, 0x54455354UL, 7, &id));
  puts ("Application icon bundle mapping and bounds checks passed");
  return 0;
}
