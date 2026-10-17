/* Windows regression test for the host-side fork formats.
 * Build from src with the same include flags as fileDouble.c, adding
 * -ffunction-sections -fdata-sections -Wl,--gc-sections and this test source.
 * Run in a disposable working directory.
 */
#include "rsys/common.h"
#include "rsys/file.h"
#include "rsys/filedouble.h"
#include "rsys/suffix_maps.h"
#include "rsys/osutil.h"
#include <assert.h>

#undef warning_unexpected
#define warning_unexpected(...) ((void) 0)
#undef gui_assert
#define gui_assert(x) assert(x)
#undef fs_err_hook
#define fs_err_hook(x) ((void) 0)

/* Use the Windows CRT for this isolated test; Executor's wrappers normalize
 * host paths in production. Forward slashes work in the CRT too. */
#define Ustat stat
#define Uopen open
#define Uclose close
#define Uopendir opendir
#define Uunlink unlink
#define Urename rename
#define Urmdir rmdir
static int test_mkdir (const char *path, int mode) { return mkdir (path); }
#define Umkdir test_mkdir

#undef ROMlib_fcblocks
static fcbrec test_fcblocks[NFCB];
#define ROMlib_fcblocks test_fcblocks

#include "../src/fileDouble.c"
#undef main

LONGINT ROMlib_GMTcorrect;
uint32 ROMlib_offset;
uint32 syn68k_address_mask = 0xFFFFFFFFUL;
unsigned syn68k_host_alias_count;
uint32 syn68k_host_alias_base, syn68k_host_alias_size;
void *syn68k_guest_to_host (uint32 address) { return SYN68K_TO_US_RAW (address); }
uint32 syn68k_host_to_guest (ptr_sized_uint address) { return US_TO_SYN68K_RAW (address); }
OSErr ROMlib_maperrno (void) { return ioErr; }
boolean_t ROMlib_creator_and_type_from_filename
  (int len, const char *name, uint32 *creator, uint32 *type) { return FALSE; }

static void putfile (const char *name, const void *bytes, int length)
{
  int fd = open (name, O_BINARY|O_CREAT|O_TRUNC|O_RDWR, 0666);
  assert (fd >= 0);
  assert (write (fd, bytes, length) == length);
  close (fd);
}

static void expect_name (const char *data, const char *expected)
{
  const char *leaf = strrchr (data, '/') + 1;
  char *name = ROMlib_sidecar_name (data, leaf);
  assert (name && !strcmp (name, expected));
  free (name);
}

static OSErr newfork (const char *path, LONGINT *fd)
{
  char *name = strdup (path);
  OSErr err = ROMlib_newresfork (name, fd, FALSE);
  free (name);
  return err;
}

int main (void)
{
  unsigned char compact[18] = "APPLTEST";
  unsigned char apple[98] = { 0 }, check[98];
  Single_header *head = (Single_header *) apple;
  Single_descriptor *entries = (Single_descriptor *) (apple + 26), d;
  Single_finfo info;
  Single_dates dates;
  FInfo finfo;
  FXInfo fxinfo;
  LONGINT data_length, resource_length, fd;
  ULONGINT capacity;
  fcbrec fcb;
  char *name;

  apple_double_fork_prefix = ".AppleDouble/";
  apple_double_fork_prefix_length = strlen (apple_double_fork_prefix);
  netatalk_conventions_p = 1;
  assert (mkdir ("basilisk") == 0);
  assert (mkdir ("basilisk/.finf") == 0);
  assert (mkdir ("basilisk/.rsrc") == 0);
  putfile ("basilisk/App", "data", 4);
  putfile ("basilisk/.finf/App", compact, 18);
  putfile ("basilisk/.rsrc/App", "resource", 8);
  expect_name ("basilisk/App", "basilisk/.finf/App");
  expect_name ("basilisk/New", "basilisk/.finf/New");
  name = ROMlib_sidecar_resource_name ("basilisk/.finf/App");
  assert (!strcmp (name, "basilisk/.rsrc/App"));
  free (name);
  assert (ROMlib_hiddenbyname (Get, "basilisk/App", "basilisk/.finf/App",
                              &dates, &finfo, &fxinfo, &data_length,
                              &resource_length) == noErr);
  assert (!memcmp (&finfo, "APPLTEST", 8));
  assert (CL (data_length) == 4 && CL (resource_length) == 8);
  memset (&info, 0x77, sizeof (info));
  fd = open ("basilisk/.finf/App", O_BINARY|O_RDWR);
  assert (getsetentry (Get, fd, Finder_Info_ID, &d, NULL));
  assert (getsetpiece (Set, fd, &d, (char *) &info, sizeof (info)));
  lseek (fd, 16, SEEK_SET);
  assert (read (fd, check, 2) == 2 && check[0] == 0 && check[1] == 0);
  close (fd);
  assert (newfork ("basilisk/.finf/New", &fd) == noErr);
  assert (lseek (fd, 0, SEEK_END) == 18);
  close (fd);
  assert (newfork ("basilisk/.rsrc/New", &fd) == noErr);
  memset (&fcb, 0, sizeof (fcb));
  fcb.fcfd = fd;
  fcb.hiddenfd = -1;
  fcb.fcflags = fcfisres;
  assert (ROMlib_FORKOFFSET (&fcb) == 0);
  assert (lseek (fd, 0, SEEK_END) == 0);
  close (fd);
  putfile ("basilisk/.rsrc/OnlyResource", "resource", 8);
  expect_name ("basilisk/OnlyResource", "basilisk/.finf/OnlyResource");

  assert (mkdir ("osx") == 0);
  putfile ("osx/.DS_Store", "", 0);
  expect_name ("osx/New", "osx/._New");
  assert (newfork ("osx/._New", &fd) == noErr);
  assert (ROMlib_resource_fork_present (fd));
  close (fd);
  /* The descriptor order deliberately differs from the payload order. */
  head->magic = CLC (DOUBLEMAGIC);
  head->version = CLC (SINGLEVERSION);
  head->nentries = CWC (2);
  entries[0].id = CLC (Resource_Fork_ID);
  entries[0].offset = CLC (50);
  entries[0].length = CLC (32);
  entries[1].id = CLC (Finder_Info_ID);
  entries[1].offset = CLC (82);
  entries[1].length = CLC (16);
  memcpy (apple + 82, "APPLTEST", 8);
  putfile ("osx/._App", apple, sizeof (apple));
  expect_name ("osx/App", "osx/._App");
  fd = open ("osx/._App", O_BINARY|O_RDWR);
  assert (getsetentry (Get, fd, Resource_Fork_ID, &d, &capacity));
  assert (CL (d.offset) == 50 && capacity == 32);
  assert (getsetentry (Get, fd, Finder_Info_ID, &d, NULL));
  assert (getsetpiece (Get, fd, &d, (char *) &info, sizeof (info)));
  assert (!memcmp (&info, "APPLTEST", 8));
  { FXInfo empty = { 0 }; assert (!memcmp (&info.fxinfo, &empty, sizeof (empty))); }
  memset (&info, 0x77, sizeof (info));
  assert (getsetpiece (Set, fd, &d, (char *) &info, sizeof (info)));
  lseek (fd, 0, SEEK_SET);
  assert (read (fd, check, sizeof (check)) == sizeof (check));
  assert (!memcmp (check, apple, 82));
  assert (getsetentry (Get, fd, Resource_Fork_ID, &d, NULL));
  lseek (fd, CL (d.offset) + 3, SEEK_SET);
  assert (grow_resource_entry (fd, &d, 65536));
  assert (CL (d.offset) == sizeof (apple));
  assert (lseek (fd, 0, SEEK_CUR) == sizeof (apple) + 3);
  lseek (fd, 82, SEEK_SET);
  assert (read (fd, check, 16) == 16 && check[0] == 0x77);
  close (fd);
  assert (mkdir ("unmarked") == 0);
  putfile ("unmarked/._ordinary", "ordinary text", 13);
  expect_name ("unmarked/New", "unmarked/.AppleDouble/New");
  /* A header with no room for its declared table must be rejected. */
  head->nentries = CWC (20);
  putfile ("osx/._Bad", apple, sizeof (apple));
  fd = open ("osx/._Bad", O_BINARY|O_RDONLY);
  assert (!getsetentry (Get, fd, Resource_Fork_ID, &d, NULL));
  close (fd);
  assert (mkdir ("netatalk") == 0);
  assert (mkdir ("netatalk/.AppleDouble") == 0);
  expect_name ("netatalk/New", "netatalk/.AppleDouble/New");
  assert (newfork ("netatalk/.AppleDouble/New", &fd) == noErr);
  assert (ROMlib_resource_fork_present (fd));
  /* Browser copies allocate a resource fork, then set its EOF before
     writing it. Both calls must leave a physically valid descriptor. */
  memset (&fcb, 0, sizeof fcb);
  fcb.fcfd = fcb.hiddenfd = fd;
  fcb.fcflags = fcfisres;
  fcb.fcleof = CL (436697);
  test_fcblocks[0] = fcb;
  assert (lseek (fd, 17, SEEK_SET) == 17);
  assert (ROMlib_seteof (&fcb) == noErr);
  assert (lseek (fd, 0, SEEK_CUR) == 17);
  assert (getsetentry (Get, fd, Resource_Fork_ID, &d, NULL));
  assert (CL (d.offset) == 512 && CL (d.length) == 436697);
  assert (lseek (fd, 0, SEEK_END) == 512 + 436697);
  assert (ROMlib_seteof (&fcb) == noErr);
  assert (ROMlib_geteofostype (&fcb) == noErr);
  assert (CL (fcb.fcleof) == 436697);
  fcb.fcleof = CL (8);
  assert (ROMlib_seteof (&fcb) == noErr);
  assert (getsetentry (Get, fd, Resource_Fork_ID, &d, NULL));
  assert (CL (d.length) == 8);
  close (fd);
  /* Out-of-file payloads cannot masquerade as usable resource forks. */
  head->nentries = CWC (2);
  entries[0].offset = CLC (4096);
  putfile ("osx/._OutOfBounds", apple, sizeof (apple));
  fd = open ("osx/._OutOfBounds", O_BINARY|O_RDONLY);
  assert (!ROMlib_resource_fork_present (fd));
  close (fd);
  { const char *target = "netatalk/App";
    name = ROMlib_sidecar_rename_name ("osx/._App", target, strrchr (target, '/') + 1); }
  assert (!strcmp (name, "netatalk/._App"));
  free (name);
  /* A move preserves both companions, even into a Netatalk directory. */
  {
    const char *target = "netatalk/Moved";
    char *source = strdup ("basilisk/.finf/App");
    char *destination = ROMlib_sidecar_rename_name
      (source, target, strrchr (target, '/') + 1);
    assert (ROMlib_sidecar_move (source, destination) == 0);
    assert (!sidecar_exists (source, FALSE));
    assert (!sidecar_exists ("basilisk/.rsrc/App", FALSE));
    assert (sidecar_exists (destination, FALSE));
    fd = open ("netatalk/.rsrc/Moved", O_BINARY|O_RDONLY);
    assert (fd >= 0 && read (fd, check, 8) == 8);
    assert (!memcmp (check, "resource", 8));
    close (fd);
    expect_name (target, "netatalk/.finf/Moved");
    assert (ROMlib_sidecar_delete (destination) == 0);
    assert (!sidecar_exists (destination, FALSE));
    assert (!sidecar_exists ("netatalk/.rsrc/Moved", FALSE));
    free (source);
    free (destination);
  }
  assert (mkdir ("basilisk/Empty") == 0);
  assert (mkdir ("basilisk/Empty/.finf") == 0);
  assert (mkdir ("basilisk/Empty/.rsrc") == 0);
  putfile ("basilisk/Empty/.DS_Store", "cache", 5);
  name = strdup ("basilisk/Empty");
  double_dir_op (name, rmdir_op);
  free (name);
  assert (rmdir ("basilisk/Empty") == 0);
  puts ("Sidecar regression tests passed");
  return 0;
}
