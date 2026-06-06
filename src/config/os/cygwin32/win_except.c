/* Copyright 1998 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

#if !defined (OMIT_RCSID_STRINGS)
char ROMlib_rcsid_win_except[] = "$Id: win_except.c 63 2004-12-24 18:19:43Z ctm $";
#endif

#define USE_WINDOWS_NOT_MAC_TYPEDEFS_AND_DEFINES

#include "rsys/common.h"

#include <stdint.h>

#include "SegmentLdr.h"
#include "rsys/launch.h"
#include "win_except.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>

#if defined (SDL)
#include <SDL/SDL.h>
#include "rsys/segment.h"
#endif

extern unsigned short mostrecenttrap;

#if defined (__WIN32__) && (__GNUC__ == 2 && __GNUC_MINOR__ < 95)

typedef LONG (*LPTOP_LEVEL_EXCEPTION_FILTER)
     (struct _EXCEPTION_POINTERS *ExceptionInfo);

LPTOP_LEVEL_EXCEPTION_FILTER STDCALL
SetUnhandledExceptionFilter (LPTOP_LEVEL_EXCEPTION_FILTER
			     lpTopLevelExceptionFilter);

static LONG
my_fault_proc( struct _EXCEPTION_POINTERS *ExceptionInfo )
#else

static CALLBACK LONG my_fault_proc (LPEXCEPTION_POINTERS exception_info)
#endif
{
  DWORD code = 0;
  void *address = 0;
  unsigned long info0 = 0;
  void *info1 = 0;

  if (exception_info && exception_info->ExceptionRecord)
    {
      code = exception_info->ExceptionRecord->ExceptionCode;
      address = exception_info->ExceptionRecord->ExceptionAddress;
      if (exception_info->ExceptionRecord->NumberParameters > 0)
	info0 = (unsigned long) exception_info->ExceptionRecord->ExceptionInformation[0];
      if (exception_info->ExceptionRecord->NumberParameters > 1)
	info1 = (void *) exception_info->ExceptionRecord->ExceptionInformation[1];
    }
  fprintf (stderr, "trace: win_exception code=0x%08lx address=%p info0=0x%08lx info1=%p mostrecenttrap=0x%04x\n",
	   (unsigned long) code, address, info0, info1, mostrecenttrap);
  fflush (stderr);

  ++ROMlib_uaf;
  if (ROMlib_uaf > 1)
    {
      if (ROMlib_uaf <= 2)
	uninstall_exception_handler ();
      exit (1);
    }

#if defined (SDL)
  if (ROMlib_fullscreen_p)
    {
      SDL_Quit ();
      ROMlib_exit = TRUE;
    }
#endif
  MessageBox (NULL, "Unexpected Application Failure",
	      "Application Failure", MB_OK);

  C_ExitToShell ();
}

PRIVATE LPTOP_LEVEL_EXCEPTION_FILTER old_filter;

void
install_exception_handler (void)
{
  old_filter = SetUnhandledExceptionFilter (my_fault_proc);
}

void
uninstall_exception_handler (void)
{
  int i;
  static int fatal_signals[] =
  {
    SIGSEGV,
#ifdef SIGBUS
    SIGBUS,
#endif
#ifdef SIGFPE
    SIGFPE,
#endif
#ifdef SIGQUIT
    SIGQUIT,
#endif
#ifdef SIGPIPE
    SIGPIPE,
#endif
  };

  SetUnhandledExceptionFilter (old_filter);
  /* Set a handler for any fatal signal not already handled */
  for ( i=0; i < (int) NELEM (fatal_signals); ++i )
    signal(fatal_signals[i], SIG_DFL);
}
