/* Copyright 1986, 1988, 1989, 1990 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

#if !defined (OMIT_RCSID_STRINGS)
char ROMlib_rcsid_qStdRect[] =
	    "$Id: qStdRect.c 63 2004-12-24 18:19:43Z ctm $";
#endif

/* Forward declarations in QuickDraw.h (DO NOT DELETE THIS LINE) */

#include "rsys/common.h"
#include "QuickDraw.h"
#include "CQuickDraw.h"
#include "MemoryMgr.h"

#include "rsys/cquick.h"
#include "rsys/picture.h"

P2(PUBLIC pascal trap, void, StdRect, GrafVerb, v, Rect *, rp)
{
    RgnHandle rh, rh2;
    PAUSEDECL;
    Rect patcheduprect;

    fprintf(stderr, "trace: StdRect begin v=%d rp=%p (%d,%d,%d,%d) port=%p cport=%d\n",
	    v, rp, CW(rp->top), CW(rp->left), CW(rp->bottom), CW(rp->right),
	    thePort, CGrafPort_p(thePort));
    
#define MOREINSANECOMPATIBILITY
#if defined (MOREINSANECOMPATIBILITY)
    if (v == frame && PORT_REGION_SAVE_X (thePort)) {
	if (CW(rp->left) > CW(rp->right)) {
	    patcheduprect = *rp;
	    patcheduprect.left  = rp->right;
	    patcheduprect.right = rp->left;
	    if (CW(rp->top) > CW(rp->bottom)) {
		patcheduprect.top    = rp->bottom;
		patcheduprect.bottom = rp->top;
	    }
	    rp = &patcheduprect;
	} else if (CW(rp->top) > CW(rp->bottom)) {
	    patcheduprect = *rp;
	    patcheduprect.top    = rp->bottom;
	    patcheduprect.bottom = rp->top;
	    rp = &patcheduprect;
	}
	if (rp == &patcheduprect) {
	    rh = NewRgn();
	    RectRgn(rh, rp);
	    XorRgn (rh,
		    (RgnHandle) PORT_REGION_SAVE (thePort),
		    (RgnHandle) PORT_REGION_SAVE (thePort));
	    DisposeRgn(rh);
/*-->*/	    return;
	}
    }
#endif /* MOREINSANECOMPATIBILITY */

    if (EmptyRect(rp))
/*-->*/	return;

    PIC_SAVE_EXCURSION
      ({
	ROMlib_drawingverbrectpicupdate( v, rp );
	PICOP(OP_frameRect + (int) v);
	PICWRITE(rp, sizeof(*rp));
      });

    PAUSERECORDING;
    rh = NewRgn();
    fprintf(stderr, "trace: StdRect NewRgn rh=%p\n", rh);
    RectRgn(rh, rp);
    fprintf(stderr, "trace: StdRect RectRgn rh=%p bbox=(%d,%d,%d,%d) size=%d\n",
	    rh, CW(HxX(rh, rgnBBox.top)), CW(HxX(rh, rgnBBox.left)),
	    CW(HxX(rh, rgnBBox.bottom)), CW(HxX(rh, rgnBBox.right)),
	    CW(HxX(rh, rgnSize)));
    switch (v) {
    case frame:
        if (PORT_REGION_SAVE_X (thePort))
	    XorRgn (rh,
		    (RgnHandle) PORT_REGION_SAVE (thePort),
		    (RgnHandle) PORT_REGION_SAVE (thePort));
        if (PORT_PEN_VIS (thePort) >= 0)
	  {
	    rh2 = NewRgn();
	    RectRgn(rh2, rp);
	    InsetRgn (rh2,
		      Cx (PORT_PEN_SIZE (thePort).h),
		      Cx (PORT_PEN_SIZE (thePort).v));
            XorRgn(rh, rh2, rh);
            StdRgn(paint, rh);
	    DisposeRgn(rh2);
        }
        break;
    case paint:
    case erase:
    case invert:
    case fill:
        fprintf(stderr, "trace: StdRect StdRgn begin v=%d rh=%p\n", v, rh);
        StdRgn(v, rh);
        fprintf(stderr, "trace: StdRect StdRgn complete v=%d rh=%p\n", v, rh);
    }
    DisposeRgn(rh);
    fprintf(stderr, "trace: StdRect complete v=%d\n", v);
    RESUMERECORDING;
}
