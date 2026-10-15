/* Exercise real heap allocation, handle relocation and emptying in 24-bit mode. */
#include "rsys/common.h"
#include "MemoryMgr.h"
#include "rsys/mman.h"
#include "rsys/mman_private.h"
#include "ResourceMgr.h"
#include "rsys/system_error.h"
#undef main
#undef NDEBUG
#include <assert.h>

/* UI, resource diagnostics and patched-trap hooks are outside this heap test. */
uint32_t ROMlib_debug_level;
unsigned short mostrecenttrap;
LONGINT ROMlib_creator;
int ROMlib_options;
StringHandle ROMlib_phoney_name_string;
const char NULL_STRING[] = "";
void *tooltraptable[0x400];
toolstuff_t toolstuff[0x400];
bool error_set_enabled (int err, bool enabled_p) { return FALSE; }
void gestalt_set_memory_size (uint32 size) {}
void vdriver_shutdown (void) {}
unsigned long ROMlib_destroy_blocks (syn68k_addr_t start, uint32 count,
                                    BOOLEAN flush_only) { return 0; }
long CToPascalCall (void *proc, unsigned long magic, ...) { abort (); }
void C_GetResInfo (Handle h, INTEGER *id, ResType *type, StringPtr name)
{ abort (); }
void _warning (int err, const char *type, const char *file, int line,
               const char *fn, const char *fmt, ...) {}
void _gui_fatal (const char *file, int line, const char *fn,
                 const char *fmt, ...)
{ fprintf (stderr, "Heap failure at %s:%d in %s: %s\n", file, line, fn, fmt); abort (); }
int system_error (const char *message, int def, const char *b0,
                  const char *b1, const char *b2,
                  system_error_callback_t f0, system_error_callback_t f1,
                  system_error_callback_t f2) { abort (); }

int
main (void)
{
  unsigned char *memory = calloc (1, 16 * 1024 * 1024);
  Handle h, blocker;
  Ptr original_data;
  assert (memory);
  ROMlib_offset = (unsigned long) memory;
  syn68k_set_memory_window (16 * 1024 * 1024, 0x400000, 0x400000);
  SysZone = RM ((THz) (memory + 0x10000));
  ApplZone = RM ((THz) (memory + 0x100000));
  ROMlib_syszone = (unsigned long) memory + 0x10000;
  ROMlib_memtop = (unsigned long) memory + 0x300000;
  InitZone (NULL, 64, (Ptr) memory + 0xF0000, MR (SysZone));
  InitZone (NULL, 64, (Ptr) memory + 0x300000, MR (ApplZone));
  TheZone = ApplZone;
  syn68k_address_mask = 0x00FFFFFF;
  h = NewHandle (32);
  assert (h && STARH (h));
  memset (STARH (h), 0xA5, 32);
  HLock (h);
  assert ((CL ((uint32) h->p) >> 24) & LOCKBIT);
  assert (HGetState (h) & LOCKBIT);
  assert ((unsigned char) STARH (h)[0] == 0xA5);
  HUnlock (h);
  HPurge (h);
  original_data = STARH (h);
  blocker = NewHandle (65536);
  assert (blocker && STARH (blocker));
  HLock (blocker);
  SetHandleSize (h, 4096);
  assert (MemError () == noErr && GetHandleSize (h) == 4096);
  assert (STARH (h) != original_data && (HGetState (h) & PURGEBIT));
  assert ((unsigned char) STARH (h)[0] == 0xA5);
  HPurge (h);
  HSetRBit (h);
  assert ((CL ((uint32) h->p) >> 24) == (PURGEBIT | RSRCBIT));
  ROMlib_sync_handle_flags (TRUE);
  syn68k_address_mask = 0xFFFFFFFF;
  assert (!(CL ((uint32) h->p) >> 24));
  assert (HGetState (h) == (PURGEBIT | RSRCBIT));
  ROMlib_sync_handle_flags (FALSE);
  syn68k_address_mask = 0x00FFFFFF;
  assert ((CL ((uint32) h->p) >> 24) == (PURGEBIT | RSRCBIT));
  HClrRBit (h);
  EmptyHandle (h);
  assert (MemError () == noErr && !h->p && !STARH (h));
  ReallocHandle (h, 128);
  assert (MemError () == noErr && STARH (h) && GetHandleSize (h) == 128);
  DisposHandle (h);
  assert (MemError () == noErr);
  HUnlock (blocker);
  DisposHandle (blocker);
  assert (MemError () == noErr);
  puts ("24-bit memory manager tests passed");
  return 0;
}
