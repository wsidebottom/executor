/* Copyright 1989, 1990, 1995 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

#if !defined (OMIT_RCSID_STRINGS)
char ROMlib_rcsid_qIMIV[] =
	    "$Id: qIMIV.c 63 2004-12-24 18:19:43Z ctm $";
#endif

/* Forward declarations in QuickDraw.h (DO NOT DELETE THIS LINE) */

#include "rsys/common.h"
#include "QuickDraw.h"
#include "CQuickDraw.h"

#include "rsys/quick.h"
#include "rsys/cquick.h"
#include "rsys/stdbits.h"
#include "rsys/region.h"
#include "rsys/safe_alloca.h"
#include "rsys/tempalloc.h"
#include "rsys/dirtyrect.h"
#include "rsys/vdriver.h"

#undef ALLOCABEGIN
#define ALLOCABEGIN	SAFE_DECL();
#undef ALLOCA
#define ALLOCA(n)	SAFE_alloca(n)

#include "seedtables.c"

#define DOLEFT  1
#define DORIGHT 2
#define DOMASK  (DOLEFT|DORIGHT)

typedef struct {
    unsigned char *p;
    unsigned char seed;
    unsigned char flags;
} stackentry;

typedef enum { Copy, Xor, Negate } transferop;

A7(PRIVATE, void, transfer, INTEGER *, srcp, INTEGER *, dstp, INTEGER, srcr,
	       INTEGER, dstr, INTEGER, height, INTEGER, widthw, transferop, op)
{
    register INTEGER sbump, dbump;
    register INTEGER *ep0, *ep1;
    
    sbump = srcr/2 - widthw;
    dbump = dstr/2 - widthw;
    ep0 = dstp + (LONGINT) height * dstr/2;
    switch (op) {
    case Copy:
	while (dstp != ep0) {
	    for (ep1 = srcp + widthw; srcp != ep1; *dstp++ = *srcp++)
		;
	    srcp += sbump;
	    dstp += dbump;
	}
	break;
    case Xor:
	while (dstp != ep0) {
	    for (ep1 = srcp + widthw; srcp != ep1; *dstp++ ^= *srcp++)
		;
	    srcp += sbump;
	    dstp += dbump;
	}
	break;
    case Negate:
	while (dstp != ep0) {
	    for (ep1 = dstp + widthw; dstp != ep1; *dstp++ ^= ~0)
		;
	    dstp += dbump;
	}
	break;
    }
}
#define LEFTBIT	(0x80)
#define RIGHTBIT (1)

#define ADDTOSTACK(ptr, s, f) (sp->p = (ptr), sp->seed = (s), \
							 sp->flags = (f), sp++)

#define EXPAND(p, s) (tempuc = ~*(p), expandtable[binarytotrinary[tempuc] + \
					          binarytotrinary[tempuc&(s)]])

#define CHECKLEFT(lp, op)					\
{								\
    --cur;							\
    if ((expanded & LEFTBIT) && !atleft && !(*cur & RIGHTBIT))	\
      {								\
	if (lp->p op dstr == cur)				\
	    lp->p = cur;					\
	else							\
	    lp = ADDTOSTACK(cur, RIGHTBIT, DOLEFT);		\
       }							\
    ++cur;							\
}

#define CHECKRIGHT(lp, op)					\
{								\
    ++cur;							\
    if ((expanded & RIGHTBIT) && !atright && !(*cur & LEFTBIT))	\
      {								\
	if (lp->p op dstr == cur)				\
	    lp->p = cur;					\
	else							\
	    lp = ADDTOSTACK(cur, LEFTBIT, DORIGHT);		\
      }								\
    --cur;							\
}

A9 (PRIVATE, void, xSeedFill, unsigned char *, srcp, unsigned char *, dstp,
    INTEGER, srcr, INTEGER, dstr, INTEGER, height, INTEGER, width,
    BOOLEAN, useseeds, INTEGER, seedh, INTEGER, seedv)
{
    unsigned char *cur, *savecur, expanded, saveexpanded, seed, *edstp;
    stackentry bogusentry, *topleftp, *toprightp, *bottomleftp, *bottomrightp,
			   *stackp, stacke;
    BOOLEAN atleft, atright;
    unsigned char tempuc;
    stackentry stack[4000], *sp;
    unsigned char *ecur;
    Rect temprect;
    LONGINT byteoff, voff;
    
    /* should be a sanity check of seedh and seedv here with just a zering
       of the destrect and early return if they are bad */

    TRAPBEGIN();
    transfer((INTEGER *) srcp, (INTEGER *) dstp, srcr, dstr, height, width,
									 Copy);
    sp = stack;
    bogusentry.p = 0;
    edstp = dstp + (LONGINT) height * dstr;

    if (useseeds) {
	cur = dstp + (LONGINT) seedv * dstr + seedh / 8;
	seed = LEFTBIT >> (seedh % 8);
	ADDTOSTACK(cur, seed, DOLEFT|DORIGHT);
    } else {
	/* this is setting things up for CalcMask */
	for (cur = dstp, ecur = cur + width*2;  cur != ecur; cur++)
	    if (*cur != 0xFF)
		ADDTOSTACK(cur, 0xFF, DOLEFT|DORIGHT);
	for (cur = dstp + (height-1) * (LONGINT) dstr, ecur = cur + width*2;
							    cur != ecur; cur++)
	    if (*cur != 0xFF)
		ADDTOSTACK(cur, 0xFF, DOLEFT|DORIGHT);
	bottomleftp = &bogusentry;
	for (cur = dstp; cur < edstp; cur += dstr) {
	    if (!(*cur & LEFTBIT))
	      {
		if (bottomleftp->p + dstr == cur)
		    bottomleftp->p = cur;
		else
		    bottomleftp = ADDTOSTACK(cur, LEFTBIT, DORIGHT);
	      }
	}
	bottomrightp = &bogusentry;
	for (cur = dstp + width*2 - 1; cur < edstp; cur += dstr) {
	    if (!(*cur & RIGHTBIT))
	      {
		if (bottomrightp->p + dstr == cur)
		    bottomrightp->p = cur;
		else
		    bottomrightp = ADDTOSTACK(cur, RIGHTBIT, DOLEFT);
	      }
	}
    }
    while ((stackp = --sp) >= stack) {
    	stacke = *stackp;
	topleftp = toprightp = bottomleftp = bottomrightp = &bogusentry;
	cur = stacke.p;
	atleft = !((cur - dstp) % dstr);
	atright =  (cur - dstp) % dstr == width*2 - 1;

	saveexpanded = expanded = EXPAND(cur, stacke.seed);
	savecur = cur;
	cur[0] |= expanded;
	if (stacke.flags & DOLEFT)
	    CHECKLEFT(topleftp, +);	/* op doesn't really matter */
	bottomleftp = topleftp;
	if (stacke.flags & DORIGHT)
	    CHECKRIGHT(toprightp, +);	/* op doesn't really matter */
	bottomrightp = toprightp;

	/* go flying up the top */
	while ((cur -= dstr, cur >= dstp) &&
					  (expanded = EXPAND(cur, expanded))) {
	    cur[0] |= expanded;
	    CHECKLEFT(topleftp, -);
	    CHECKRIGHT(toprightp, -);
	    if ((cur[dstr] & expanded) != expanded)
		ADDTOSTACK(cur+dstr, expanded, DOLEFT|DORIGHT);
	}

	/* go flying down the bottom */
	cur = savecur;
	expanded = saveexpanded;
	while ((cur += dstr, cur < edstp) &&
					  (expanded = EXPAND(cur, expanded))) {
	    cur[0] |= expanded;
	    CHECKLEFT(bottomleftp, +);
	    CHECKRIGHT(bottomrightp, +);
	    if ((cur[-dstr] & expanded) != expanded)
		ADDTOSTACK(cur-dstr, expanded, DOLEFT|DORIGHT);
	}
    }
    transfer((INTEGER *) srcp, (INTEGER *) dstp, srcr, dstr, height, width,
									  Xor);
    if (!useseeds)
	transfer((INTEGER *) 0, (INTEGER *) dstp, 0, dstr, height,
								width, Negate);
    if (dstp >= (unsigned char *) MR(screenBitsX.baseAddr)) {
	byteoff = dstp    - (unsigned char *) MR(screenBitsX.baseAddr);
	voff    = byteoff / CW(screenBitsX.rowBytes);
	if (voff < CW(screenBitsX.bounds.bottom) - CW(screenBitsX.bounds.top))
	  {
	    dirty_rect_accrue (CW (screenBitsX.bounds.top) + voff,
			       (CW (screenBitsX.bounds.left)
				+ (byteoff % CW(screenBitsX.rowBytes) * 8L)),
			       CW (temprect.top) + height,
			       CW (temprect.left) + (LONGINT) width * 16);
	  }
      }
    TRAPEND();
}

#if defined (VDRIVER_SUPPORTS_REAL_SCREEN_BLITS)
static boolean_t
create_scratch_bitmap_if_necessary (uint8 **_fbuf,
				    /* dummy */ int row_words,
				    int height, int word_width,
				    write_back_data_t *write_back_data)
{
  PixMapHandle gd_pmap;
  
  uint8 *screen_fbuf, *fbuf;
  int screen_row_bytes;
  int screen_height;
  
  gd_pmap          = GD_PMAP (MR (MainDevice));
  screen_fbuf      = PIXMAP_BASEADDR (gd_pmap);
  screen_row_bytes = PIXMAP_ROWBYTES (gd_pmap);
  screen_height    = RECT_HEIGHT (&PIXMAP_BOUNDS (gd_pmap));

  fbuf = *_fbuf;

  if (VDRIVER_BYPASS_INTERNAL_FBUF_P ()
      && (screen_fbuf <= fbuf
	  && fbuf < screen_fbuf + (screen_row_bytes * screen_height)))
    {
      PixMap *src_pm, *dst_pm;
      Rect *src_rect, *dst_rect;
      int offset;
      
      src_pm   = &write_back_data->dst_pm;
      src_rect = &write_back_data->dst_rect;
      dst_pm   = &write_back_data->src_pm;
      dst_rect = &write_back_data->src_rect;
      
      offset = fbuf - screen_fbuf;
      
      src_rect->top    = CW (offset / screen_row_bytes);
      src_rect->bottom = CW (offset / screen_row_bytes + height);
      src_rect->left   = CW ((offset % screen_row_bytes) * 8);
      src_rect->right  = CW ((offset % screen_row_bytes) * 8
			     + word_width * 16);
      
      *src_pm = *STARH (gd_pmap);
      
      pixmap_copy (src_pm, src_rect, dst_pm, dst_rect);
      *_fbuf = BITMAP_BASEADDR (dst_pm);
      
      return TRUE;
    }
  
  return FALSE;
}

static void
SeedFill_handle_direct_screen_access (uint8 *srcp, uint8 *dstp,
				      int src_row_words, int dst_row_words,
				      int height, int word_width,
				      boolean_t use_seed_pt_p,
				      int seedh, int seedv)
{
  write_back_data_t write_back_data;
  boolean_t write_back_p = FALSE;
  
  create_scratch_bitmap_if_necessary (&srcp, src_row_words,
				      height, word_width,
				      NULL);
  write_back_p = create_scratch_bitmap_if_necessary (&dstp, dst_row_words,
						     height, word_width,
						     &write_back_data);
  
  xSeedFill (srcp, dstp, src_row_words, dst_row_words,
	     height, word_width, use_seed_pt_p, seedh, seedv);
  
  if (write_back_p)
    {
      CopyBits ((BitMap *) &write_back_data.src_pm,
		(BitMap *) &write_back_data.dst_pm,
		&write_back_data.src_rect, &write_back_data.dst_rect,
		srcCopy, NULL);
      
      pixmap_free_copy (&write_back_data.src_pm);
    }
}
#else
#define SeedFill_handle_direct_screen_access xSeedFill
#endif

P8 (PUBLIC pascal trap, void, SeedFill, Ptr, srcp, Ptr, dstp, 	/* IMIV-24 */
    INTEGER, srcr, INTEGER, dstr, INTEGER, height, INTEGER, width,
    INTEGER, seedh, INTEGER, seedv)
{
  SeedFill_handle_direct_screen_access ((uint8 *) srcp, (uint8 *) dstp,
					srcr, dstr,
					height, width, TRUE, seedh, seedv);
}

P6 (PUBLIC pascal trap, void, CalcMask, Ptr, srcp, Ptr, dstp,	/* IMIV-24 */
    INTEGER, srcr, INTEGER, dstr, INTEGER, height, INTEGER, width)
{
  SeedFill_handle_direct_screen_access ((uint8 *) srcp, (uint8 *) dstp,
					srcr, dstr,
					height, width, FALSE, 0, 0);
}

static void
copy_mask_1 (BitMap *src_bm, BitMap *mask_bm, BitMap *dst_bm,
	     Rect *src_rect, Rect *mask_rect, Rect *dst_rect)
{
  RgnHandle mask_rgn;
  
  mask_rgn = NewRgn ();
  if (BitMapToRegion (mask_rgn, mask_bm) == noErr)
    {
      Handle save_pic_handle, save_graf_procs;
      RgnHandle mask_rect_rgn;
      
      mask_rect_rgn = NewRgn ();
      RectRgn (mask_rect_rgn, mask_rect);
      SectRgn (mask_rgn, mask_rect_rgn, mask_rgn);
      
      MapRgn (mask_rgn, mask_rect, dst_rect);
      
      save_pic_handle = PORT_PIC_SAVE_X (thePort);
      save_graf_procs = (Handle) PORT_GRAF_PROCS_X (thePort);
      
      PORT_PIC_SAVE_X (thePort)   = RM (NULL);
      PORT_GRAF_PROCS_X (thePort) = RM (NULL);
      
      CopyBits (src_bm, dst_bm, src_rect, dst_rect, srcCopy, mask_rgn);
      
      PORT_PIC_SAVE_X (thePort)   = save_pic_handle;
      PORT_GRAF_PROCS_X (thePort) = (void *) save_graf_procs;
      
      DisposeRgn (mask_rect_rgn);
      DisposeRgn (mask_rgn);
    }
  else
    {
      Rect src_top, src_bottom, mask_top, mask_bottom, dst_top, dst_bottom;
      int16 src_half, mask_half, dst_half;
      BitMap mask_top_bm, mask_bottom_bm;
      
      DisposeRgn (mask_rgn);
      
      src_half = RECT_HEIGHT (src_rect) / 2;
      mask_half = RECT_HEIGHT (mask_rect) / 2;
      dst_half = RECT_HEIGHT (dst_rect) / 2;
      
      src_top  = src_bottom = *src_rect;
      mask_top = mask_bottom = *mask_rect;
      dst_top  = dst_bottom = *dst_rect;
      
      src_bottom.top = src_top.bottom
	= CW (CW (src_top.bottom) - src_half);
      mask_bottom.top = mask_top.bottom
	= CW (CW (mask_top.bottom) - mask_half);
      dst_bottom.top = dst_top.bottom
	= CW (CW (dst_top.bottom) - dst_half);
      
      mask_top_bm = *mask_bm;
      mask_bottom_bm = *mask_bm;
      mask_bottom_bm.bounds.top = mask_top_bm.bounds.bottom
	= CW (CW (mask_top_bm.bounds.bottom) - mask_half);
      
      copy_mask_1 (src_bm, &mask_top_bm, dst_bm,
		   &src_top, &mask_top, &dst_top); 
      copy_mask_1 (src_bm, &mask_bottom_bm, dst_bm,
		   &src_bottom, &mask_bottom, &dst_bottom);
    }
  
}

#if defined (CYGWIN32) || defined (__MINGW32__)
static BitMap *
win32_resolve_copy_bitmap (BitMap *b)
{
  int flags;
  PixMapHandle pmh;

  if (!b)
    return NULL;

  flags = CW (b->rowBytes) & ROWBYTES_FLAG_BITS;
  if (flags == ROWBYTES_FLAG_BITS)
    {
      pmh = (PixMapHandle) MR (b->baseAddr);
      if (!pmh)
	return NULL;
      return (BitMap *) STARH (pmh);
    }

  return b;
}

static int
win32_copy_bitmap_bpp (BitMap *b)
{
  if (IS_PIXMAP_PTR_P (b))
    return CW (((PixMap *) b)->pixelSize);

  return 1;
}

static int
win32_copy_bitmap_row_bytes (BitMap *b)
{
  if (IS_PIXMAP_PTR_P (b))
    return CW (((PixMap *) b)->rowBytes) & ROWBYTES_VALUE_BITS;

  return CW (b->rowBytes) & ROWBYTES_VALUE_BITS;
}

static uint8 *
win32_copy_bitmap_base (BitMap *b)
{
  if (IS_PIXMAP_PTR_P (b))
    return (uint8 *) MR (((PixMap *) b)->baseAddr);

  return (uint8 *) MR (b->baseAddr);
}

static int
win32_get_1bpp_pixel (BitMap *b, int row_bytes, uint8 *base, int x, int y)
{
  int left, top;
  uint8 byte;

  left = CW (b->bounds.left);
  top = CW (b->bounds.top);
  byte = base[(y - top) * row_bytes + ((x - left) >> 3)];

  return (byte & (0x80 >> ((x - left) & 7))) != 0;
}

static boolean_t
win32_copy_mask_8bpp (BitMap *src_bogo_map, BitMap *mask_bogo_map,
		      BitMap *dst_bogo_map, Rect *src_rect, Rect *mask_rect,
		      Rect *dst_rect)
{
  BitMap *src_bm, *mask_bm, *dst_bm;
  int src_bpp, mask_bpp, dst_bpp;
  int src_row_bytes, mask_row_bytes, dst_row_bytes;
  int src_width, src_height, mask_width, mask_height;
  int dst_width, dst_height;
  int dst_top, dst_left, dst_bottom, dst_right;
  int clip_top, clip_left, clip_bottom, clip_right;
  uint8 *src_base, *mask_base, *dst_base;
  uint32 fg_pixel, bk_pixel;
  RgnHandle clip_rgn;

  src_bm = win32_resolve_copy_bitmap (src_bogo_map);
  mask_bm = win32_resolve_copy_bitmap (mask_bogo_map);
  dst_bm = win32_resolve_copy_bitmap (dst_bogo_map);

  if (!src_bm || !mask_bm || !dst_bm)
    return FALSE;

  src_bpp = win32_copy_bitmap_bpp (src_bm);
  mask_bpp = win32_copy_bitmap_bpp (mask_bm);
  dst_bpp = win32_copy_bitmap_bpp (dst_bm);

  if ((src_bpp != 1 && src_bpp != 8) || mask_bpp != 1 || dst_bpp != 8)
    return FALSE;

  src_row_bytes = win32_copy_bitmap_row_bytes (src_bm);
  mask_row_bytes = win32_copy_bitmap_row_bytes (mask_bm);
  dst_row_bytes = win32_copy_bitmap_row_bytes (dst_bm);
  src_base = win32_copy_bitmap_base (src_bm);
  mask_base = win32_copy_bitmap_base (mask_bm);
  dst_base = win32_copy_bitmap_base (dst_bm);

  if (!src_base || !mask_base || !dst_base)
    return FALSE;

  src_width = RECT_WIDTH (src_rect);
  src_height = RECT_HEIGHT (src_rect);
  mask_width = RECT_WIDTH (mask_rect);
  mask_height = RECT_HEIGHT (mask_rect);
  dst_width = RECT_WIDTH (dst_rect);
  dst_height = RECT_HEIGHT (dst_rect);

  if (src_width <= 0 || src_height <= 0 || mask_width <= 0 || mask_height <= 0
      || dst_width <= 0 || dst_height <= 0)
    return TRUE;

  dst_top = CW (dst_rect->top);
  dst_left = CW (dst_rect->left);
  dst_bottom = CW (dst_rect->bottom);
  dst_right = CW (dst_rect->right);

  clip_top = MAX (dst_top, CW (dst_bm->bounds.top));
  clip_left = MAX (dst_left, CW (dst_bm->bounds.left));
  clip_bottom = MIN (dst_bottom, CW (dst_bm->bounds.bottom));
  clip_right = MIN (dst_right, CW (dst_bm->bounds.right));

  if (clip_bottom <= clip_top || clip_right <= clip_left)
    return TRUE;

  clip_rgn = NewRgn ();
  {
    Rect clip_rect;

    SetRect (&clip_rect, clip_left, clip_top, clip_right, clip_bottom);
    RectRgn (clip_rgn, &clip_rect);
    SectRgn (clip_rgn, PORT_VIS_REGION (thePort), clip_rgn);
    SectRgn (clip_rgn, PORT_CLIP_REGION (thePort), clip_rgn);
  }

  if (EmptyRgn (clip_rgn))
    {
      DisposeRgn (clip_rgn);
      return TRUE;
    }

  clip_top = CW (RGN_BBOX (clip_rgn).top);
  clip_left = CW (RGN_BBOX (clip_rgn).left);
  clip_bottom = CW (RGN_BBOX (clip_rgn).bottom);
  clip_right = CW (RGN_BBOX (clip_rgn).right);

  ROMlib_fg_bk (&fg_pixel, &bk_pixel, NULL, NULL, NULL, FALSE, TRUE);

  {
    int y;

    for (y = clip_top; y < clip_bottom; ++y)
      {
	int x;
	int sy = CW (src_rect->top) + ((y - dst_top) * src_height) / dst_height;
	int my = CW (mask_rect->top) + ((y - dst_top) * mask_height) / dst_height;
	uint8 *dst_row = dst_base + ((y - CW (dst_bm->bounds.top)) * dst_row_bytes);

	for (x = clip_left; x < clip_right; ++x)
	  {
	    int sx = (CW (src_rect->left)
		      + ((x - dst_left) * src_width) / dst_width);
		int mx = (CW (mask_rect->left)
			  + ((x - dst_left) * mask_width) / dst_width);
		uint8 pixel;
		Point pt;

		pt.h = x;
		pt.v = y;
		if (!PtInRgn (pt, clip_rgn))
		  continue;

		if (!win32_get_1bpp_pixel (mask_bm, mask_row_bytes, mask_base, mx, my))
		  continue;

	    if (src_bpp == 8)
	      pixel = src_base[(sy - CW (src_bm->bounds.top)) * src_row_bytes
			       + (sx - CW (src_bm->bounds.left))];
	    else
	      pixel = (win32_get_1bpp_pixel (src_bm, src_row_bytes, src_base,
					     sx, sy)
		       ? fg_pixel : bk_pixel) & 0xFF;

	    dst_row[x - CW (dst_bm->bounds.left)] = pixel;
	  }
	}
  }

  dirty_rect_accrue (clip_top - CW (dst_bm->bounds.top),
		     clip_left - CW (dst_bm->bounds.left),
		     clip_bottom - CW (dst_bm->bounds.top),
		     clip_right - CW (dst_bm->bounds.left));
  dirty_rect_update_screen ();
  vdriver_flush_display ();
  DisposeRgn (clip_rgn);

  return TRUE;
}
#endif

P6 (PUBLIC pascal trap, void, CopyMask,		/* IMIV-24 */
    BitMap *, src_bogo_map, BitMap *, mask_bogo_map, BitMap *, dst_bogo_map,
    Rect *, src_rect, Rect *, mask_rect, Rect *, dst_rect)
{
  BitMap mask_bm;
  void *mask_bits;
  int row_bytes;
  TEMP_ALLOC_DECL (temp_mask_bits);

  fprintf (stderr,
	   "trace: CopyMask src=%p base=%p rb=%d b=(%d,%d,%d,%d) "
	   "mask=%p base=%p rb=%d b=(%d,%d,%d,%d) "
	   "dst=%p base=%p rb=%d b=(%d,%d,%d,%d) "
	   "sr=(%d,%d,%d,%d) mr=(%d,%d,%d,%d) dr=(%d,%d,%d,%d)\n",
	   (void *) src_bogo_map,
	   src_bogo_map ? (void *) MR (src_bogo_map->baseAddr) : NULL,
	   src_bogo_map ? CW (src_bogo_map->rowBytes) : 0,
	   src_bogo_map ? CW (src_bogo_map->bounds.top) : 0,
	   src_bogo_map ? CW (src_bogo_map->bounds.left) : 0,
	   src_bogo_map ? CW (src_bogo_map->bounds.bottom) : 0,
	   src_bogo_map ? CW (src_bogo_map->bounds.right) : 0,
	   (void *) mask_bogo_map,
	   mask_bogo_map ? (void *) MR (mask_bogo_map->baseAddr) : NULL,
	   mask_bogo_map ? CW (mask_bogo_map->rowBytes) : 0,
	   mask_bogo_map ? CW (mask_bogo_map->bounds.top) : 0,
	   mask_bogo_map ? CW (mask_bogo_map->bounds.left) : 0,
	   mask_bogo_map ? CW (mask_bogo_map->bounds.bottom) : 0,
	   mask_bogo_map ? CW (mask_bogo_map->bounds.right) : 0,
	   (void *) dst_bogo_map,
	   dst_bogo_map ? (void *) MR (dst_bogo_map->baseAddr) : NULL,
	   dst_bogo_map ? CW (dst_bogo_map->rowBytes) : 0,
	   dst_bogo_map ? CW (dst_bogo_map->bounds.top) : 0,
	   dst_bogo_map ? CW (dst_bogo_map->bounds.left) : 0,
	   dst_bogo_map ? CW (dst_bogo_map->bounds.bottom) : 0,
	   dst_bogo_map ? CW (dst_bogo_map->bounds.right) : 0,
	   src_rect ? CW (src_rect->top) : 0, src_rect ? CW (src_rect->left) : 0,
	   src_rect ? CW (src_rect->bottom) : 0, src_rect ? CW (src_rect->right) : 0,
	   mask_rect ? CW (mask_rect->top) : 0, mask_rect ? CW (mask_rect->left) : 0,
	   mask_rect ? CW (mask_rect->bottom) : 0, mask_rect ? CW (mask_rect->right) : 0,
	   dst_rect ? CW (dst_rect->top) : 0, dst_rect ? CW (dst_rect->left) : 0,
	   dst_rect ? CW (dst_rect->bottom) : 0, dst_rect ? CW (dst_rect->right) : 0);
  fflush (stderr);

  if (ROMlib_text_output_disabled_p)
/*-->*/ return;

#if defined (CYGWIN32) || defined (__MINGW32__)
  if (win32_copy_mask_8bpp (src_bogo_map, mask_bogo_map, dst_bogo_map,
			    src_rect, mask_rect, dst_rect))
    return;
  return;
#endif

  if (EmptyRect (mask_rect))
    return;
  
  row_bytes = (RECT_WIDTH (mask_rect) + 31) / 32 * 4;
  TEMP_ALLOC_ALLOCATE (mask_bits, temp_mask_bits,
		       row_bytes * RECT_HEIGHT (mask_rect));
  mask_bm.baseAddr = RM (mask_bits);
  mask_bm.rowBytes = CW (row_bytes);
  mask_bm.bounds   = *mask_rect;
  
  CopyBits (mask_bogo_map, &mask_bm,
	    mask_rect, mask_rect, srcCopy, NULL);
  
  copy_mask_1 (src_bogo_map, &mask_bm, dst_bogo_map,
	       src_rect, mask_rect, dst_rect);
  
  TEMP_ALLOC_FREE (temp_mask_bits);
}

P8 (PUBLIC pascal trap, void, IMVI_CopyDeepMask,
    BitMap *, srcBits,
    BitMap *, maskBits,
    BitMap *, dstBits,
    Rect *, srcRect,
    Rect *, maskRect,
    Rect *, dstRect,
    INTEGER, mode,
    RgnHandle, maskRgn)
{
  warning_unimplemented ("poorly implemented");

  if (ROMlib_text_output_disabled_p)
/*-->*/ return;

  C_CopyMask (srcBits, maskBits, dstBits, srcRect, maskRect, dstRect);
}

/* MeasureText is in qd/qStdText.c */

A0(PUBLIC a0trap, INTEGER *, GetMaskTable)	/* IMIV-25 */
{
    static unsigned char table[] __attribute__ ((aligned (2))) = {
	0x00, 0x00, 0x80, 0x00, 0xC0, 0x00, 0xE0, 0x00,
	0xF0, 0x00, 0xF8, 0x00, 0xFC, 0x00, 0xFE, 0x00,
	0xFF, 0x00, 0xFF, 0x80, 0xFF, 0xC0, 0xFF, 0xE0,
	0xFF, 0xF0, 0xFF, 0xF8, 0xFF, 0xFC, 0xFF, 0xFE,

	0xFF, 0xFF, 0x7F, 0xFF, 0x3F, 0xFF, 0x1F, 0xFF,
	0x0F, 0xFF, 0x07, 0xFF, 0x03, 0xFF, 0x01, 0xFF,
	0x00, 0xFF, 0x00, 0x7F, 0x00, 0x3F, 0x00, 0x1F,
	0x00, 0x0F, 0x00, 0x07, 0x00, 0x03, 0x00, 0x01,

	0x80, 0x00, 0x40, 0x00, 0x20, 0x00, 0x10, 0x00,
	0x08, 0x00, 0x04, 0x00, 0x02, 0x00, 0x01, 0x00,
	0x00, 0x80, 0x00, 0x40, 0x00, 0x20, 0x00, 0x10,
	0x00, 0x08, 0x00, 0x04, 0x00, 0x02, 0x00, 0x01,
    };

    EM_A0 = (LONGINT) US_TO_SYN68K(table);
    return (INTEGER *) table;
}
