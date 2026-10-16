/* Copyright 1994, 1995, 1996 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

#if !defined (OMIT_RCSID_STRINGS)
char ROMlib_rcsid_sdlwm[] = "$Id: sdlwm.c 88 2005-05-25 03:59:37Z ctm $";
#endif

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "SDL/SDL.h"

#if defined (_WIN32)
#include "SDL/SDL_syswm.h"
#include <propsys.h>

static HICON application_icon;
static HICON application_small_icon;
static WCHAR application_id[64];

void
ROMlib_sdl_apply_application_icon (void)
{
  SDL_SysWMinfo info;
  SDL_VERSION (&info.version);
  if (SDL_GetWMInfo (&info) > 0)
    {
      HICON large = application_icon ? application_icon
        : LoadIcon (NULL, IDI_APPLICATION);
      HICON small = application_small_icon ? application_small_icon : large;
      SendMessage (info.window, WM_SETICON, ICON_BIG, (LPARAM) large);
      SendMessage (info.window, WM_SETICON, ICON_SMALL, (LPARAM) small);
      if (application_id[0])
        {
          HMODULE shell = LoadLibraryA ("shell32.dll");
          if (shell)
            {
              typedef HRESULT (WINAPI *get_store_t) (HWND, REFIID, void **);
              get_store_t get_store = (get_store_t) GetProcAddress
                (shell, "SHGetPropertyStoreForWindow");
              static const IID store_iid = {
                0x886d8eeb, 0x8cf2, 0x4446,
                {0x8d,0x02,0xcd,0xba,0x1d,0xbd,0xcf,0x99}
              };
              static const PROPERTYKEY app_id_key = {
                {0x9f4c2855,0x9f79,0x4b39,
                 {0xa8,0xd0,0xe1,0xd4,0x2d,0xe1,0xd5,0xf3}}, 5
              };
              IPropertyStore *store;
              if (get_store && SUCCEEDED (get_store
                    (info.window, &store_iid, (void **) &store)))
                {
                  PROPVARIANT value;
                  memset (&value, 0, sizeof value);
                  value.vt = VT_LPWSTR;
                  value.pwszVal = application_id;
                  store->lpVtbl->SetValue (store, &app_id_key, &value);
                  store->lpVtbl->Release (store);
                }
              FreeLibrary (shell);
            }
        }
    }
}

void
ROMlib_sdl_set_application_icon (const unsigned char *rgba,
                               unsigned long creator)
{
  HICON old_large = application_icon, old_small = application_small_icon;
  HMODULE shell;
  typedef HRESULT (WINAPI *set_app_id_t) (PCWSTR);
  set_app_id_t set_app_id;

  application_icon = application_small_icon = NULL;
  swprintf (application_id, 64, L"ARDI.Executor.MacApp.%08lx", creator);
  if (rgba)
    {
      BITMAPINFO bmi;
      ICONINFO icon_info;
      HBITMAP color, mask;
      unsigned char mask_bits[128];
      unsigned char *bits;
      int i;

      memset (&bmi, 0, sizeof bmi);
      bmi.bmiHeader.biSize = sizeof bmi.bmiHeader;
      bmi.bmiHeader.biWidth = 32;
      bmi.bmiHeader.biHeight = -32;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      color = CreateDIBSection (NULL, &bmi, DIB_RGB_COLORS,
                               (void **) &bits, NULL, 0);
      memset (mask_bits, 0, sizeof mask_bits);
      if (color)
        {
          for (i = 0; i < 1024; ++i)
            {
              bits[4*i] = rgba[4*i+2];
              bits[4*i+1] = rgba[4*i+1];
              bits[4*i+2] = rgba[4*i];
              bits[4*i+3] = rgba[4*i+3];
              if (!rgba[4*i+3])
                mask_bits[i >> 3] |= 0x80 >> (i & 7);
            }
          mask = CreateBitmap (32, 32, 1, 1, mask_bits);
          if (mask)
            {
              memset (&icon_info, 0, sizeof icon_info);
              icon_info.fIcon = TRUE;
              icon_info.hbmColor = color;
              icon_info.hbmMask = mask;
              application_icon = CreateIconIndirect (&icon_info);
              DeleteObject (mask);
            }
          DeleteObject (color);
          if (application_icon)
            application_small_icon = (HICON) CopyImage
              (application_icon, IMAGE_ICON, GetSystemMetrics (SM_CXSMICON),
               GetSystemMetrics (SM_CYSMICON), 0);
        }
    }
  ROMlib_sdl_apply_application_icon ();
  if (old_small)
    DestroyIcon (old_small);
  if (old_large)
    DestroyIcon (old_large);

  /* Give each Macintosh application its own taskbar identity. */
  shell = LoadLibraryA ("shell32.dll");
  if (shell)
    {
      set_app_id = (set_app_id_t) GetProcAddress
        (shell, "SetCurrentProcessExplicitAppUserModelID");
      if (set_app_id)
        {
          set_app_id (application_id);
        }
      FreeLibrary (shell);
    }
}
#endif

/* Globals */
int host_cursor_depth = 1;

/* Window manager interface functions */

void
ROMlib_SetTitle (char *title)
{
  SDL_WM_SetCaption(title, title);
}

char *
ROMlib_GetTitle (void)
{
  char *retval;

  SDL_WM_GetCaption(&retval, (char **)0);
  return retval;
}

void
ROMlib_FreeTitle (char *title)
{
}

/* This is really inefficient.  We should hash the cursors */
void
host_set_cursor (char *cursor_data,
                 unsigned short cursor_mask[16],
                 int hotspot_x, int hotspot_y)
{
  SDL_Cursor *old_cursor, *new_cursor;

  old_cursor = SDL_GetCursor();
  new_cursor = SDL_CreateCursor((unsigned char *) cursor_data,
				(unsigned char *) cursor_mask,
				16, 16, hotspot_x, hotspot_y);
  if ( new_cursor != NULL )
    {
      SDL_SetCursor(new_cursor);
      SDL_FreeCursor(old_cursor);
    }
}

int
host_set_cursor_visible (int show_p)
{
  return(SDL_ShowCursor(show_p));
}
