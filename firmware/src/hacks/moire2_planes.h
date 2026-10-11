#ifndef HACKS_MOIRE2_PLANES_H
#define HACKS_MOIRE2_PLANES_H

#include <stdint.h>

#include "hacks/halo_planes.h"

/* Moire2 draws circle outlines into four depth-1 pixmaps, merges them into a
 * fifth with OR or XOR, and expands that onto the window in two colours. It
 * uses the bit planes of halo_planes.h (create, free, rectangle and the
 * expansion to the window) and adds the three calls Halo does not make: an
 * outline circle, an XOR rectangle, and a copy between planes that honours
 * the GC's function. Any drawable that is not XSHIM_WINDOW or None is a plane,
 * as Moire2 has no other pixmaps. Source and destination of a copy must be
 * different planes. */

/* Flips the bits of one row's span x0..x1 (inclusive), clipped to the plane. */
static void moire2_xor_span(HaloPlane *p, int64_t y, int64_t x0, int64_t x1) {
  if (y < 0 || y >= p->h || x1 < 0 || x0 >= p->w) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= p->w) x1 = p->w - 1;
  if (x0 > x1) return;
  uint8_t *row = p->bits + (size_t)y * (size_t)p->stride;
  const int64_t b0 = x0 >> 3, b1 = x1 >> 3;
  const uint8_t m0 = (uint8_t)(0xFF >> (x0 & 7));
  const uint8_t m1 = (uint8_t)(0xFF << (7 - (x1 & 7)));
  if (b0 == b1) {
    row[b0] ^= (uint8_t)(m0 & m1);
    return;
  }
  row[b0] ^= m0;
  for (int64_t b = b0 + 1; b < b1; b++) row[b] ^= 0xFF;
  row[b1] ^= m1;
}

/* XFillRectangle: a plane takes the GC's function (XOR with a foreground of
 * 1 flips the rectangle; any other function sets or clears it, as Halo's). */
static int moire2_fill_rectangle(Display *dpy, Drawable d, GC gc, int x, int y,
                                 unsigned int w, unsigned int h) {
  HaloPlane *p = halo_plane_of(d);
  if (!p || d == None || gc->function != GXxor)
    return halo_fill_rectangle(dpy, d, gc, x, y, w, h);
  if (gc->foreground == 0) return 0;
  for (int64_t yy = y; yy < (int64_t)y + h && yy < p->h; yy++)
    moire2_xor_span(p, yy, x, (int64_t)x + w - 1);
  return 0;
}

/* XDrawArc of a whole circle or ellipse in a plane: a ring `line_width` thick
 * (a width of 0 is 1) between an outer and an inner ellipse, each found a row
 * at a time as halo_fill_arc does. A partial arc draws nothing: Moire2 has
 * none. */
static int moire2_draw_arc(Display *dpy, Drawable d, GC gc, int x, int y,
                           unsigned int w, unsigned int h, int angle1,
                           int angle2) {
  HaloPlane *p = halo_plane_of(d);
  if (!p) return (XDrawArc)(dpy, d, gc, x, y, w, h, angle1, angle2);
  if (d == None || w == 0 || h == 0 || w > 0x7FFFFFFFu || h > 0x7FFFFFFFu ||
      angle2 < 360 * 64)
    return 0;
  const int64_t t = gc->line_width > 1 ? gc->line_width : 1;
  const int64_t grow = t / 2, shrink = t - t / 2;
  const int on = gc->foreground != 0;
  const int64_t ow = (int64_t)w + 2 * grow, oh = (int64_t)h + 2 * grow;
  const int64_t iw = (int64_t)w - 2 * shrink, ih = (int64_t)h - 2 * shrink;
  if (ow > 0x7FFFFFFF || oh > 0x7FFFFFFF) return 0;
  int64_t y0 = (int64_t)y - grow, y1 = y0 + oh - 1;
  if (y0 < 0) y0 = 0;
  if (y1 >= p->h) y1 = p->h - 1;
  for (int64_t yy = y0; yy <= y1; yy++) {
    int64_t oa, ob, ia, ib;
    if (!halo_circle_row((int)(x - grow), (int)(y - grow), (unsigned)ow,
                         (unsigned)oh, yy, &oa, &ob))
      continue;
    if (iw > 0 && ih > 0 &&
        halo_circle_row((int)(x + shrink), (int)(y + shrink), (unsigned)iw,
                        (unsigned)ih, yy, &ia, &ib)) {
      halo_plane_span(p, yy, oa, ia - 1, on);
      halo_plane_span(p, yy, ib + 1, ob, on);
    } else {
      halo_plane_span(p, yy, oa, ob, on);
    }
  }
  return 0;
}

/* The eight bits of `row` starting at bit position `at` (the first pixel is
 * the most significant bit of its byte); bits outside the row read as 0. */
static uint8_t moire2_bits_at(const uint8_t *row, int stride, int64_t at) {
  const int64_t b = at >> 3;
  const int sh = (int)(at & 7);
  const int hi = b >= 0 && b < stride ? row[b] : 0;
  const int lo = b + 1 >= 0 && b + 1 < stride ? row[b + 1] : 0;
  return (uint8_t)(((hi << sh) | (lo >> (8 - sh))) & 0xFF);
}

/* XCopyArea between two planes. The GC's function says how the source meets
 * what is there: GXor sets bits, GXxor flips them, anything else copies. A
 * copy that is not between two planes goes to the shim. */
static int moire2_copy_area(Display *dpy, Drawable src, Drawable dst, GC gc,
                            int sx, int sy, unsigned int w, unsigned int h,
                            int dx, int dy) {
  const HaloPlane *s = halo_plane_of(src);
  HaloPlane *dp = halo_plane_of(dst);
  if (!s || !dp) return (XCopyArea)(dpy, src, dst, gc, sx, sy, w, h, dx, dy);
  if (src == None || dst == None) return 0;
  int64_t i0, i1, j0, j1;
  if (!halo_copy_range(s->w, s->h, dp->w, dp->h, sx, sy, w, h, dx, dy, &i0, &i1,
                       &j0, &j1))
    return 0;
  /* Clipped to both planes, everything below fits an int, and the S3 is a
   * 32-bit core: 64-bit index maths here cost 13 ms a copy. */
  const int x0 = (int)(dx + i0), x1 = (int)(dx + i1 - 1);
  const int bs = x0 >> 3, be = x1 >> 3;
  const int function = gc->function;
  const int shift = (int)((int64_t)sx - dx);
  const int byteoff = shift >> 3, sh = shift & 7;
  for (int j = (int)j0; j < (int)j1; j++) {
    const uint8_t *srow = s->bits + (size_t)(sy + j) * (size_t)s->stride;
    uint8_t *drow = dp->bits + (size_t)(dy + j) * (size_t)dp->stride;
    for (int b = bs; b <= be; b++) {
      uint8_t mask = 0xFF;
      if (b == bs) mask &= (uint8_t)(0xFF >> (x0 & 7));
      if (b == be) mask &= (uint8_t)(0xFF << (7 - (x1 & 7)));
      uint8_t bits;
      if (mask == 0xFF) {
        /* All eight bits are inside the clip, so both source bytes exist. */
        const uint8_t *sp = srow + b + byteoff;
        bits = sh ? (uint8_t)((sp[0] << sh) | (sp[1] >> (8 - sh))) : sp[0];
      } else {
        bits = (uint8_t)(moire2_bits_at(srow, s->stride, (int64_t)sx + b * 8 - dx) &
                         mask);
      }
      if (function == GXor)
        drow[b] |= bits;
      else if (function == GXxor)
        drow[b] ^= bits;
      else
        drow[b] = (uint8_t)((drow[b] & ~mask) | bits);
    }
  }
  return 0;
}

/* XCopyPlane of a plane to the window: set bits are drawn in the GC's
 * foreground and clear bits in its background, as halo_copy_plane does, but a
 * byte at a time from a table of its eight pixels (rebuilt for each call, as
 * Moire2 changes a colour every frame) and in 32-bit stores. As copied it
 * took 27 ms for the 217,000 pixels of a frame. Anything else goes to
 * halo_copy_plane. */
typedef union {
  uint16_t px[8];
  uint32_t pair[4];
} Moire2Eight;

static int moire2_copy_plane(Display *dpy, Drawable src, Drawable dst, GC gc,
                             int sx, int sy, unsigned int w, unsigned int h,
                             int dx, int dy, unsigned long plane) {
  const HaloPlane *s = halo_plane_of(src);
  Canvas *dc = dst == XSHIM_WINDOW ? xshim_target(dpy, dst) : NULL;
  if (!s || src == None || !dc)
    return halo_copy_plane(dpy, src, dst, gc, sx, sy, w, h, dx, dy, plane);
  int64_t i0, i1, j0, j1;
  if (!halo_copy_range(s->w, s->h, dc->w, dc->h, sx, sy, w, h, dx, dy, &i0, &i1,
                       &j0, &j1))
    return 0;
  static Moire2Eight table[256];
  const uint16_t fg = (uint16_t)gc->foreground, bg = (uint16_t)gc->background;
  for (int b = 0; b < 256; b++)
    for (int k = 0; k < 8; k++) table[b].px[k] = (b >> (7 - k)) & 1 ? fg : bg;
  const int a = (int)i0, z = (int)i1;
  for (int j = (int)j0; j < (int)j1; j++) {
    const uint8_t *srow = s->bits + (size_t)(sy + j) * (size_t)s->stride;
    uint16_t *out = dc->px + (size_t)(dy + j) * (size_t)dc->w + dx;
    int i = a;
    for (; i < z && ((sx + i) & 7); i++)
      out[i] = (srow[(sx + i) >> 3] >> (7 - ((sx + i) & 7))) & 1 ? fg : bg;
    for (; i + 8 <= z; i += 8) {
      uint16_t *o = out + i;
      const Moire2Eight *e = &table[srow[(sx + i) >> 3]];
      if (((uintptr_t)o & 3) == 0)
        memcpy(__builtin_assume_aligned(o, 4), e, sizeof(*e));
      else
        memcpy(o, e, sizeof(*e));
    }
    for (; i < z; i++)
      out[i] = (srow[(sx + i) >> 3] >> (7 - ((sx + i) & 7))) & 1 ? fg : bg;
  }
  canvas_mark_dirty(dc, (int)(dx + i0), (int)(dy + j0), (int)(i1 - i0),
                    (int)(j1 - j0));
  return 0;
}

#endif
