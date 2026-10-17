/* Run the real List Manager selection loop with a controlled callback. */
#include "rsys/common.h"
#include "ListMgr.h"
#include "EventMgr.h"
#include "OSEvent.h"
#include "ToolboxUtil.h"
#include "rsys/list.h"
#include "rsys/cquick.h"
#include "rsys/pstuff.h"
#undef main
#undef NDEBUG
#include <assert.h>

void *tooltraptable[0x400];
toolstuff_t toolstuff[0x400], pstuff[2];
static int callback_result, callback_calls, mouse_polls;
static GrafPort port;
static HIDDEN_GrafPtr port_pointer;
static HIDDEN_HIDDEN_GrafPtr_Ptr port_handle;
static HIDDEN_RgnPtr region_pointer;
static Region region;
static unsigned char memory[65536];

long CToPascalCall (void *proc, unsigned long magic, ...)
{
  assert (proc == memory + 0x2000);
  assert (magic == CTOP_Button);
  ++callback_calls;
  return callback_result;
}
SignedByte HGetState (Handle h) { return 0; }
void HSetState (Handle h, SignedByte state) {}
LONGINT C_TickCount (void) { return 100; }
BOOLEANRET OSEventAvail (INTEGER mask, EventRecord *event)
{ ++mouse_polls; assert (mask == mUpMask); return TRUE; }
void C_SetPort (GrafPtr p) { port_pointer.p = RM (p); }
RgnHandle C_NewRgn (void) { return &region_pointer; }
void C_DisposeRgn (RgnHandle r) {}
void C_ClipRect (Rect *r) {}
void C_GlobalToLocal (Point *p) { abort (); }
void C_LRect (Rect *r, Cell cell, ListHandle list)
{ *r = HxX (list, rView); }
void ROMlib_listcall (INTEGER msg, BOOLEAN sel, Rect *r, Cell cell,
                      INTEGER off, INTEGER len, ListHandle list)
{ assert (msg == lHiliteMsg); }
void C_LScroll (INTEGER h, INTEGER v, ListHandle list) { abort (); }
void C_LCellSize (Point p, ListHandle list) { abort (); }
INTEGER C_GetCtlValue (ControlHandle c) { abort (); }
void C_SetCtlValue (ControlHandle c, INTEGER value) { abort (); }
INTEGER C_TestControl (ControlHandle c, Point p) { abort (); }
INTEGER C_TrackControl (ControlHandle c, Point p, ProcPtr action) { abort (); }
void _gui_fatal (const char *file, int line, const char *fn,
                 const char *fmt, ...) { abort (); }
INTEGER *ROMlib_getoffp (Cell cell, ListHandle list)
{
  if (cell.h == 0 && cell.v == 0)
    return HxX (list, cellArray);
  return NULL;
}
BOOLEAN C_LGetSelect (BOOLEAN next, Cell *cell, ListHandle list)
{ return !!(CW (HxX (list, cellArray)[0]) & 0x8000); }
LONGINT C_PinRect (Rect *r, Point p)
{
  assert (PtInRect (p, r));
  return ((uint32) (uint16) p.v << 16) | (uint16) p.h;
}
INTEGER C_LoWord (LONGINT value) { return value; }
INTEGER C_HiWord (LONGINT value) { return (uint32) value >> 16; }
Fixed C_FixRatio (INTEGER n, INTEGER d) { abort (); }
Fixed C_FixMul (Fixed a, Fixed b) { abort (); }
INTEGER C_AngleFromSlope (Fixed slope) { abort (); }

int main (void)
{
  HIDDEN_ListPtr handle;
  ListHandle list = &handle;
  ListPtr lp = (ListPtr) (memory + 0x3000);
  Point click = { 15, 15 };
  Cell cell = { 0, 0 };
  int result;

  ROMlib_offset = (uint32) memory;
  syn68k_set_memory_window (sizeof memory, 0, 0);
  a5 = US_TO_SYN68K (&port_handle);
  port_handle.p = RM (&port_pointer);
  port_pointer.p = RM (&port);
  region_pointer.p = RM (&region);
  PORT_CLIP_REGION_X (&port) = RM (&region_pointer);
  handle.p = RM (lp);

  for (result = -1; result <= 1; ++result)
    {
      memset (lp, 0, sizeof *lp);
      SetRect (&lp->rView, 10, 10, 110, 30);
      SetRect (&lp->visible, 0, 0, 1, 1);
      lp->dataBounds = lp->visible;
      lp->cellSize.h = CW (100);
      lp->cellSize.v = CW (20);
      lp->port = RM (&port);
      lp->lClikLoop = result < 0 ? NULL : RM (memory + 0x2000);
      lp->lastClick.h = lp->lastClick.v = CWC (-1);
      callback_result = result;
      callback_calls = mouse_polls = 0;
      assert (!C_LClick (click, 0, list));
      assert (callback_calls == (result >= 0));
      assert (!!C_LGetSelect (FALSE, &cell, list) == (result != 0));
      assert (mouse_polls == (result != 0));
    }
  puts ("List click callback tests passed");
  return 0;
}
