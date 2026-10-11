#ifndef HACKS_LOWREZ_PUT_H
#define HACKS_LOWREZ_PUT_H

#include <stdint.h>
#include <string.h>

#include "core/canvas.h"
#include "x11shim/pixmap.h"
#include "x11shim/xshim.h"

/* Lets a hack that renders every pixel of a window-sized image itself run at
 * 1/LOWREZ_SCALE of the width and height: it is told the window is that much
 * smaller, and its XPutImage draws each pixel as a LOWREZ_SCALE square,
 * centred. The image passes cost 1/LOWREZ_SCALE^2 as much, and so does the
 * memory they walk, which is PSRAM. Include it after defining LOWREZ_SCALE,
 * then map XGetWindowAttributes and XPutImage onto these. */
#ifndef LOWREZ_SCALE
#error "define LOWREZ_SCALE before including lowrez_put.h"
#endif

static int lowrez_get_attributes(Display *dpy, Window w, XWindowAttributes *a) {
  const int ok = (XGetWindowAttributes)(dpy, w, a);
  a->width /= LOWREZ_SCALE;
  a->height /= LOWREZ_SCALE;
  return ok;
}

/* The image must be 16-bit (ordinary little-endian RGB565, as the hack builds
 * it for a 16-bit visual); anything else draws nothing. */
static int lowrez_put_image(Display *dpy, Drawable d, GC gc, XImage *im,
                            int src_x, int src_y, int dst_x, int dst_y,
                            unsigned int w, unsigned int h) {
  (void)gc;
  Canvas *const c = xshim_target(dpy, d);
  if (!c || !im || !im->data || im->bits_per_pixel != 16) return 0;
  const int64_t S = LOWREZ_SCALE;
  if (src_x < 0 || src_y < 0 || src_x >= im->width || src_y >= im->height) return 0;
  if ((int64_t)w > im->width - src_x) w = (unsigned int)(im->width - src_x);
  if ((int64_t)h > im->height - src_y) h = (unsigned int)(im->height - src_y);
  const int64_t ox = ((int64_t)c->w - (int64_t)im->width * S) / 2;
  const int64_t oy = ((int64_t)c->h - (int64_t)im->height * S) / 2;
  int64_t xa = ox + (int64_t)dst_x * S, xb = ox + ((int64_t)dst_x + w) * S;
  if (xa < 0) xa = 0;
  if (xb > c->w) xb = c->w;
  if (xb <= xa) return 0;
  int64_t ya = INT64_MAX, yb = INT64_MIN;
  for (unsigned int j = 0; j < h; j++) {
    const uint8_t *src = (const uint8_t *)im->data +
                         (size_t)(src_y + j) * (size_t)im->bytes_per_line +
                         (size_t)src_x * 2;
    const int64_t y0 = oy + ((int64_t)dst_y + j) * S;
    uint16_t *first = NULL;
    for (int64_t k = 0; k < S; k++) {
      const int64_t y = y0 + k;
      if (y < 0 || y >= c->h) continue;
      uint16_t *row = c->px + (size_t)y * (size_t)c->w;
      if (first) {
        memcpy(row + xa, first + xa, (size_t)(xb - xa) * sizeof(uint16_t));
      } else {
        for (unsigned int i = 0; i < w; i++) {
          const uint16_t v = px_swap((uint16_t)(src[2 * i] | (src[2 * i + 1] << 8)));
          const int64_t x0 = ox + ((int64_t)dst_x + i) * S;
          for (int64_t kk = 0; kk < S; kk++) {
            const int64_t x = x0 + kk;
            if (x >= xa && x < xb) row[x] = v;
          }
        }
        first = row;
      }
      if (y < ya) ya = y;
      if (y + 1 > yb) yb = y + 1;
    }
  }
  if (yb > ya) canvas_mark_dirty(c, (int)xa, (int)ya, (int)(xb - xa), (int)(yb - ya));
  return 0;
}

#endif
