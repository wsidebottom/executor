/* Execute Greg's Browser's corrected scan against dialog/document fixtures. */
#include "config.h"
#include "syn68k_public.h"
#include "rsys/browsercompat.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static syn68k_addr_t
window_getter (syn68k_addr_t pc, void *unused)
{
  syn68k_addr_t ret = POPADDR ();
  uint32 window = READUL (EM_A7);
  const unsigned char *title = (const unsigned char *) SYN68K_TO_US (window + 200);
  EM_A0 = ROMlib_gregs_browser_bookmark_dialog_p (title)
    ? 0 : READUL (window + 152);
  return ret;
}

int main (void)
{
  static const unsigned char original[] = {
    0x2f, 0x0a, 0x4e, 0xb9, 0x00, 0x41, 0x20, 0x00,
    0x26, 0x48, 0x20, 0x08, 0x58, 0x4f, 0x67, 0x0c,
    0x3f, 0x03, 0x2f, 0x0b, 0x61, 0xff, 0x00, 0x00,
    0x18, 0x5e, 0x5c, 0x4f
  };
  unsigned char *memory = calloc (1, 16 * 1024 * 1024);
  unsigned char *resource = memory + 0x10000;
  uint32 scan = 0x10000 + GREGS_BROWSER_BOOKMARK_SCAN_OFFSET;
  uint32 update = scan + 24 + 0x185c;
  uint32 getter;
  unsigned i;
  static const char *titles[] = {
    "About Greg's Browser", "Browser", "Preferences", "Browser Docs",
    "Document", "Browser"
  };

  assert (memory);
  ROMlib_offset = (ptr_sized_uint) memory;
  syn68k_callback_address_space = (uint16 *) (memory + 0x8000);
  syn68k_set_memory_window (16 * 1024 * 1024, 0x800000, 0x400000);
  initialize_68k_emulator (NULL, 0, (uint32 *) memory, 0);
  getter = callback_install (window_getter, NULL);
  memcpy (resource + GREGS_BROWSER_BOOKMARK_SCAN_OFFSET,
          original, sizeof original);
  assert (!ROMlib_patch_gregs_browser_bookmarks (resource, 100, getter));
  assert (ROMlib_patch_gregs_browser_bookmarks (resource, 37407, getter));
  assert (!ROMlib_patch_gregs_browser_bookmarks (resource, 37407, getter));
  /* Record the updater's arguments and number of calls. */
  WRITEUW (update, 0x282f); WRITEUW (update + 2, 4);
  WRITEUW (update + 4, 0x3a2f); WRITEUW (update + 6, 8);
  WRITEUW (update + 8, 0x5286); WRITEUW (update + 10, 0x4e75);
  WRITEUW (scan + GREGS_BROWSER_BOOKMARK_SCAN_SIZE, 0x4e75);
  for (i = 0; i < sizeof titles / sizeof titles[0]; ++i)
    {
      WRITEUW (0x6000 + 108, i == 4 ? 8 : 2);
      WRITEUL (0x6000 + 152, i == 5 ? 0 : 0x7000);
      memory[0x6000 + 200] = strlen (titles[i]);
      memcpy (memory + 0x6000 + 201, titles[i], strlen (titles[i]));
      EM_A2 = 0x6000;
      EM_A7 = 0x9000;
      EM_D3 = 7;
      EM_D4 = EM_D5 = 0xdeadbeef;
      EM_D6 = 0;
      WRITEUL (EM_A7, MAGIC_EXIT_EMULATOR_ADDRESS);
      interpret_code (hash_lookup_code_and_create_if_needed (scan));
      assert (EM_A7 == 0x9004);
      if (i != 1 && i != 4)
        assert (EM_D6 == 0 && EM_D4 == 0xdeadbeef);
      else
        assert (EM_D6 == 1 && EM_D4 == 0x7000 && (EM_D5 & 65535) == 7);
    }
  puts ("Browser bookmark compatibility tests passed");
  return 0;
}
