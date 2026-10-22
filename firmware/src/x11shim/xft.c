/* The Xft stand-in. See xft.h. Ours, not derived from xscreensaver code. */
#include "x11shim/xft.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/canvas.h"
#include "x11shim/pixmap.h"

struct XftDraw {
  Display *dpy;
  Drawable drawable;
};

static const XshimFont *g_font;

void xshim_set_font(const XshimFont *font) { g_font = font; }

/* The trailing number of the list's first entry, or 0 if it has none. */
static int requested_size(const char *name) {
  if (!name) return 0;
  size_t end = strcspn(name, ",");
  while (end > 0 && isspace((unsigned char)name[end - 1])) end--;
  size_t start = end;
  while (start > 0 && isdigit((unsigned char)name[start - 1])) start--;
  if (start == end || (start > 0 && !isspace((unsigned char)name[start - 1])))
    return 0;
  return atoi(name + start);
}

/* What a hack gets when no font is installed: every glyph advances and none
 * has ink. XJack abort()s on no font, and a test that replays every hack in
 * the registry would take the whole run down with it. The device always
 * installs one (main.cpp). */
static const XshimFont *blank_font(void) {
  static XshimGlyph glyphs[XSHIM_FONT_LAST - XSHIM_FONT_FIRST + 1];
  static XshimFontSize size = {16, 12, 5, glyphs, NULL};
  static const XshimFont font = {1, &size};
  if (!glyphs[0].advance)
    for (size_t i = 0; i < sizeof glyphs / sizeof glyphs[0]; i++)
      glyphs[i].advance = 8;
  return &font;
}

XftFont *load_xft_font_retry(Display *dpy, int screen, const char *name) {
  (void)dpy;
  (void)screen;
  const XshimFont *family =
      g_font && g_font->nsizes > 0 ? g_font : blank_font();
  const XshimFontSize *best = &family->sizes[0];
  const int want = requested_size(name);
  if (want > 0) {
    for (int i = 1; i < family->nsizes; i++) {
      const XshimFontSize *s = &family->sizes[i];
      const int d = abs(s->pixel_size - want);
      const int bd = abs(best->pixel_size - want);
      if (d < bd || (d == bd && s->pixel_size < best->pixel_size)) best = s;
    }
  }
  XftFont *f = malloc(sizeof *f);
  if (!f) return NULL;
  f->ascent = best->ascent;
  f->descent = best->descent;
  f->height = best->ascent + best->descent;
  f->size = best;
  return f;
}

void XftFontClose(Display *dpy, XftFont *font) {
  (void)dpy;
  free(font);
}

void XftColorFree(Display *dpy, Visual *visual, Colormap cmap, XftColor *color) {
  (void)dpy;
  (void)visual;
  (void)cmap;
  (void)color;
}

Bool XftColorAllocName(Display *dpy, Visual *visual, Colormap cmap,
                       const char *name, XftColor *out) {
  (void)visual;
  XColor c;
  memset(&c, 0, sizeof c);
  if (!XParseColor(dpy, cmap, name, &c)) return False;
  XAllocColor(dpy, cmap, &c);
  out->pixel = c.pixel;
  out->color.red = c.red;
  out->color.green = c.green;
  out->color.blue = c.blue;
  out->color.alpha = 0xFFFF;
  return True;
}

XftDraw *XftDrawCreate(Display *dpy, Drawable d, Visual *visual,
                       Colormap cmap) {
  (void)visual;
  (void)cmap;
  XftDraw *draw = malloc(sizeof *draw);
  if (!draw) return NULL;
  draw->dpy = dpy;
  draw->drawable = d;
  return draw;
}

void XftDrawDestroy(XftDraw *draw) { free(draw); }

int screen_number(Screen *screen) {
  (void)screen;
  return 0;
}

/* The next character's glyph, advancing *s past it. A multi-byte character
 * takes its lead byte and as many continuation bytes as follow, up to the end
 * of the text; anything without an ASCII glyph is '?'. */
static const XshimGlyph *next_glyph(const XshimFontSize *size,
                                    const FcChar8 **s, const FcChar8 *end) {
  const FcChar8 c = **s;
  int extra = 0;
  if (c >= 0xF0 && c <= 0xF7) extra = 3;
  else if (c >= 0xE0) extra = c <= 0xEF ? 2 : 0;
  else if (c >= 0xC0) extra = 1;
  (*s)++;
  while (extra-- > 0 && *s < end && (**s & 0xC0) == 0x80) (*s)++;
  const int code = (c >= XSHIM_FONT_FIRST && c <= XSHIM_FONT_LAST) ? c : '?';
  return &size->glyphs[code - XSHIM_FONT_FIRST];
}

void XftTextExtentsUtf8(Display *dpy, XftFont *font, const FcChar8 *s, int len,
                        XGlyphInfo *out) {
  (void)dpy;
  memset(out, 0, sizeof *out);
  if (!font || !s || len <= 0) return;
  int64_t pen = 0, left = INT64_MAX, right = INT64_MIN;
  int top = INT_MIN, lowest = INT_MAX;
  const FcChar8 *end = s + len;
  while (s < end) {
    const XshimGlyph *g = next_glyph(font->size, &s, end);
    if (g->w && g->h) {
      if (pen + g->left < left) left = pen + g->left;
      if (pen + g->left + g->w > right) right = pen + g->left + g->w;
      if (g->top > top) top = g->top;
      if (g->top - g->h < lowest) lowest = g->top - g->h;
    }
    pen += g->advance;
  }
  out->xOff = (short)pen;
  if (left > right) return;
  out->width = (unsigned short)(right - left);
  out->height = (unsigned short)(top - lowest);
  out->x = (short)-left;
  out->y = (short)top;
}

/* dst is a canvas pixel (bytes swapped). Weighs the colour by a out of 255. */
static uint16_t blend(uint16_t dst, int r, int g, int b, int a) {
  const uint16_t p = px_swap(dst);
  const int r5 = (p >> 11) & 0x1F, g6 = (p >> 5) & 0x3F, b5 = p & 0x1F;
  const int dr = (r5 << 3) | (r5 >> 2);
  const int dg = (g6 << 2) | (g6 >> 4);
  const int db = (b5 << 3) | (b5 >> 2);
  return rgb565((uint8_t)((r * a + dr * (255 - a) + 127) / 255),
                (uint8_t)((g * a + dg * (255 - a) + 127) / 255),
                (uint8_t)((b * a + db * (255 - a) + 127) / 255));
}

static void draw_glyph(Canvas *cv, const XshimFontSize *size,
                       const XshimGlyph *g, int64_t gx, int64_t gy,
                       const XRenderColor *c) {
  const int64_t x0 = gx > 0 ? gx : 0;
  const int64_t y0 = gy > 0 ? gy : 0;
  const int64_t x1 = gx + g->w < cv->w ? gx + g->w : cv->w;
  const int64_t y1 = gy + g->h < cv->h ? gy + g->h : cv->h;
  if (x0 >= x1 || y0 >= y1) return;
  const int r = c->red >> 8, gr = c->green >> 8, b = c->blue >> 8;
  for (int64_t y = y0; y < y1; y++) {
    const unsigned char *row = size->alpha + g->offset + (y - gy) * g->w;
    uint16_t *dst = cv->px + y * cv->w;
    for (int64_t x = x0; x < x1; x++) {
      const int a = row[x - gx];
      if (a) dst[x] = blend(dst[x], r, gr, b, a);
    }
  }
  canvas_mark_dirty(cv, (int)x0, (int)y0, (int)(x1 - x0), (int)(y1 - y0));
}

void XftDrawStringUtf8(XftDraw *draw, const XftColor *color, XftFont *font,
                       int x, int y, const FcChar8 *s, int len) {
  if (!draw || !color || !font || !s || len <= 0) return;
  Canvas *cv = xshim_target(draw->dpy, draw->drawable);
  if (!cv) return;
  int64_t pen = x;
  const FcChar8 *end = s + len;
  while (s < end) {
    const XshimGlyph *g = next_glyph(font->size, &s, end);
    if (g->w && g->h)
      draw_glyph(cv, font->size, g, pen + g->left, (int64_t)y - g->top,
                 &color->color);
    pen += g->advance;
  }
}
