/* Stand-in for the parts of Xft, fontconfig and xscreensaver's utils/xft.h that
 * XJack uses. Ours, not derived from xscreensaver code.
 *
 * There is one font (xft_font.h), drawn at the baked size nearest the one a
 * font name asks for; the family is ignored. Text is alpha-blended onto the
 * canvas in the colour's 8-bit channels. Only ASCII has glyphs: any other
 * UTF-8 character, and control characters, draw as '?'. Drawing ignores the
 * GC's clip mask, as Xft does. */
#ifndef X11SHIM_XFT_H
#define X11SHIM_XFT_H

#include "x11shim/xft_font.h"
#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char FcChar8;

typedef struct {
  unsigned short width, height;
  short x, y;
  short xOff, yOff;
} XGlyphInfo;

typedef struct {
  unsigned short red, green, blue, alpha;
} XRenderColor;

typedef struct {
  unsigned long pixel;
  XRenderColor color;
} XftColor;

typedef struct {
  int ascent, descent, height;
  const XshimFontSize *size;
} XftFont;

typedef struct XftDraw XftDraw;

/* The first entry of a comma-separated list such as "Special Elite 24, Courier
 * 24" gives the size (its trailing number); ties between two baked sizes take
 * the smaller. A name with no number takes the font's first size. With no font
 * installed (xshim_set_font) the font is blank: text takes room and has no ink,
 * so a hack that abort()s on no font still runs. Only NULL if out of memory. */
XftFont *load_xft_font_retry(Display *, int screen, const char *name);
void XftFontClose(Display *, XftFont *);

/* False if the name is not a colour. */
Bool XftColorAllocName(Display *, Visual *, Colormap, const char *name,
                       XftColor *);

/* Gives a colour back. The shim's colours are plain pixel values, so nothing
 * is held and this does nothing. */
void XftColorFree(Display *, Visual *, Colormap, XftColor *);

XftDraw *XftDrawCreate(Display *, Drawable, Visual *, Colormap);
void XftDrawDestroy(XftDraw *);

/* Draws len bytes of UTF-8 with the pen at x and the baseline at y. */
void XftDrawStringUtf8(XftDraw *, const XftColor *, XftFont *, int x, int y,
                       const FcChar8 *s, int len);

/* xOff is the pen's travel. x and y place the ink's top-left corner as Xft
 * does: x is minus the ink's left edge, y its top above the baseline. */
void XftTextExtentsUtf8(Display *, XftFont *, const FcChar8 *s, int len,
                        XGlyphInfo *);

int screen_number(Screen *);

#ifdef __cplusplus
}
#endif

#endif
