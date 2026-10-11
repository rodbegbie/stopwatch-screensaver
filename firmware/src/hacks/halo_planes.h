#ifndef HACKS_HALO_PLANES_H
#define HACKS_HALO_PLANES_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/canvas.h"
#include "hacks/halo_circles.h"
#include "x11shim/pixmap.h"
#include "x11shim/xshim.h"

/* Halo draws whole circles into depth-1 pixmaps, merges two of them with XOR,
 * and expands the result onto the window in two colours. The shim cannot draw
 * into a bitmap or XOR, and a colour pixmap stood in for each bitmap costs
 * 434 KB in PSRAM and several million slow 16-bit writes a frame (the circles
 * grow until they fill the window). A plane is one bit a pixel, 27 KB, and a
 * run of set bits is a byte fill. The wrapper (halo_direct.c) maps Halo's
 * pixmap calls onto these. The window is XSHIM_WINDOW; any other drawable is a
 * plane, as Halo has no other pixmaps. */
typedef struct {
  int w, h, stride;
  uint8_t bits[];
} HaloPlane;

static HaloPlane *halo_plane_of(Drawable d) {
  return d == XSHIM_WINDOW ? NULL : (HaloPlane *)(uintptr_t)d;
}

static Pixmap halo_create_pixmap(Display *dpy, Drawable d, unsigned int w,
                                 unsigned int h, unsigned int depth) {
  if (depth != 1) return (XCreatePixmap)(dpy, d, w, h, depth);
  if (w == 0 || h == 0 || w > INT16_MAX || h > INT16_MAX) return None;
  const int stride = (int)((w + 7) / 8);
  HaloPlane *p = (HaloPlane *)calloc(1, sizeof(*p) + (size_t)stride * h);
  if (!p) return None;
  p->w = (int)w;
  p->h = (int)h;
  p->stride = stride;
  return (Pixmap)(uintptr_t)p;
}

static int halo_free_pixmap(Display *dpy, Pixmap pm) {
  (void)dpy;
  free(halo_plane_of(pm));
  return 0;
}

static void halo_apply(uint8_t *byte, uint8_t mask, int on) {
  *byte = on ? (uint8_t)(*byte | mask) : (uint8_t)(*byte & ~mask);
}

/* Sets or clears bits x0..x1 (inclusive) of row y, the first pixel being the
 * most significant bit of its byte. */
static void halo_plane_span(HaloPlane *p, int64_t y, int64_t x0, int64_t x1,
                            int on) {
  if (y < 0 || y >= p->h || x1 < 0 || x0 >= p->w) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= p->w) x1 = p->w - 1;
  if (x0 > x1) return;
  uint8_t *row = p->bits + (size_t)y * (size_t)p->stride;
  const int64_t b0 = x0 >> 3, b1 = x1 >> 3;
  const uint8_t m0 = (uint8_t)(0xFF >> (x0 & 7));
  const uint8_t m1 = (uint8_t)(0xFF << (7 - (x1 & 7)));
  if (b0 == b1) {
    halo_apply(&row[b0], (uint8_t)(m0 & m1), on);
    return;
  }
  halo_apply(&row[b0], m0, on);
  if (b1 - b0 > 1) memset(row + b0 + 1, on ? 0xFF : 0, (size_t)(b1 - b0 - 1));
  halo_apply(&row[b1], m1, on);
}

static int halo_get_bit(const HaloPlane *p, int64_t x, int64_t y) {
  return (p->bits[(size_t)y * (size_t)p->stride + (size_t)(x >> 3)] >>
          (7 - (x & 7))) & 1;
}

static int halo_fill_rectangle(Display *dpy, Drawable d, GC gc, int x, int y,
                               unsigned int w, unsigned int h) {
  HaloPlane *p = halo_plane_of(d);
  if (!p) return (XFillRectangle)(dpy, d, gc, x, y, w, h);
  if (d == None) return 0;
  const int on = gc->foreground != 0;
  for (int64_t yy = y; yy < (int64_t)y + h && yy < p->h; yy++)
    halo_plane_span(p, yy, x, (int64_t)x + w - 1, on);
  return 0;
}

static int halo_fill_arc(Display *dpy, Drawable d, GC gc, int x, int y,
                         unsigned int w, unsigned int h, int angle1,
                         int angle2) {
  HaloPlane *p = halo_plane_of(d);
  if (angle2 < 360 * 64 && angle2 > -360 * 64)
    return p ? 0 : (XFillArc)(dpy, d, gc, x, y, w, h, angle1, angle2);
  if (!p) {
    Canvas *c = xshim_target(dpy, d);
    if (c) halo_fill_circle_on_canvas(c, gc, x, y, w, h);
    return 0;
  }
  if (d == None || w == 0 || h == 0 || w > 0x7FFFFFFFu || h > 0x7FFFFFFFu)
    return 0;
  const int on = gc->foreground != 0;
  int64_t y0 = y < 0 ? 0 : y, y1 = (int64_t)y + (int64_t)h - 1;
  if (y1 >= p->h) y1 = p->h - 1;
  for (int64_t yy = y0; yy <= y1; yy++) {
    int64_t xa, xb;
    if (halo_circle_row(x, y, w, h, yy, &xa, &xb))
      halo_plane_span(p, yy, xa, xb, on);
  }
  return 0;
}

/* The part of the copy that lies inside both buffers, as source offsets
 * [i0, i1) by [j0, j1); false when none does. */
static int halo_copy_range(int sw, int sh, int dw, int dh, int sx, int sy,
                           unsigned int w, unsigned int h, int dx, int dy,
                           int64_t *i0, int64_t *i1, int64_t *j0, int64_t *j1) {
  int64_t a0 = 0, b0 = 0;
  if (-(int64_t)sx > a0) a0 = -(int64_t)sx;
  if (-(int64_t)dx > a0) a0 = -(int64_t)dx;
  if (-(int64_t)sy > b0) b0 = -(int64_t)sy;
  if (-(int64_t)dy > b0) b0 = -(int64_t)dy;
  int64_t a1 = w, b1 = h;
  if ((int64_t)sw - sx < a1) a1 = (int64_t)sw - sx;
  if ((int64_t)dw - dx < a1) a1 = (int64_t)dw - dx;
  if ((int64_t)sh - sy < b1) b1 = (int64_t)sh - sy;
  if ((int64_t)dh - dy < b1) b1 = (int64_t)dh - dy;
  *i0 = a0, *i1 = a1, *j0 = b0, *j1 = b1;
  return a1 > a0 && b1 > b0;
}

/* XCopyPlane of a plane. To the window, set bits take the GC's foreground and
 * clear bits its background. To another plane the GC is the hack's GXxor one
 * (the shim ignores the GC function, and Halo copies between planes only for
 * that), so the source's bits are flipped into the destination. */
static int halo_copy_plane(Display *dpy, Drawable src, Drawable dst, GC gc,
                           int sx, int sy, unsigned int w, unsigned int h,
                           int dx, int dy, unsigned long plane) {
  (void)plane;
  const HaloPlane *s = halo_plane_of(src);
  if (!s || src == None || dst == None) return 0;
  HaloPlane *dp = halo_plane_of(dst);
  Canvas *dc = dp ? NULL : xshim_target(dpy, dst);
  if (!dp && !dc) return 0;
  int64_t i0, i1, j0, j1;
  if (!halo_copy_range(s->w, s->h, dp ? dp->w : dc->w, dp ? dp->h : dc->h, sx,
                       sy, w, h, dx, dy, &i0, &i1, &j0, &j1))
    return 0;
  if (dc) {
    const uint16_t fg = (uint16_t)gc->foreground, bg = (uint16_t)gc->background;
    for (int64_t j = j0; j < j1; j++) {
      uint16_t *out = dc->px + (size_t)(dy + j) * (size_t)dc->w + dx;
      for (int64_t i = i0; i < i1;) {
        const int64_t x = sx + i;
        if ((x & 7) == 0 && i + 8 <= i1) {
          const uint8_t b = s->bits[(size_t)(sy + j) * (size_t)s->stride + (size_t)(x >> 3)];
          if (b == 0 || b == 0xFF) {
            const uint16_t v = b ? fg : bg;
            for (int k = 0; k < 8; k++) out[i + k] = v;
          } else {
            for (int k = 0; k < 8; k++) out[i + k] = (b >> (7 - k)) & 1 ? fg : bg;
          }
          i += 8;
        } else {
          out[i] = halo_get_bit(s, x, sy + j) ? fg : bg;
          i++;
        }
      }
    }
    canvas_mark_dirty(dc, (int)(dx + i0), (int)(dy + j0), (int)(i1 - i0), (int)(j1 - j0));
    return 0;
  }
  for (int64_t j = j0; j < j1; j++) {
    const uint8_t *srow = s->bits + (size_t)(sy + j) * (size_t)s->stride;
    uint8_t *drow = dp->bits + (size_t)(dy + j) * (size_t)dp->stride;
    const int64_t x0 = dx + i0, x1 = dx + i1 - 1;
    if (((sx - dx) & 7) == 0) {
      const int64_t shift = (sx - dx) / 8;
      for (int64_t b = x0 >> 3; b <= (x1 >> 3); b++) {
        uint8_t mask = 0xFF;
        if (b == (x0 >> 3)) mask &= (uint8_t)(0xFF >> (x0 & 7));
        if (b == (x1 >> 3)) mask &= (uint8_t)(0xFF << (7 - (x1 & 7)));
        drow[b] ^= (uint8_t)(srow[b + shift] & mask);
      }
    } else {
      for (int64_t i = i0; i < i1; i++)
        if (halo_get_bit(s, sx + i, sy + j))
          drow[(dx + i) >> 3] ^= (uint8_t)(0x80 >> ((dx + i) & 7));
    }
  }
  return 0;
}

#endif
