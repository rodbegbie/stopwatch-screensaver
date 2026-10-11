#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ximage-loader.h"
#include "x11shim/pixmap.h"
#include "x11shim/xshim.h"

#define BYTES_PER_PIXEL 4

static int64_t min_i64(int64_t a, int64_t b) { return a < b ? a : b; }
static int64_t max_i64(int64_t a, int64_t b) { return a > b ? a : b; }

XImage *XCreateImage(Display *dpy, Visual *visual, unsigned int depth,
                     int format, int offset, char *data, unsigned int w,
                     unsigned int h, int bitmap_pad, int bytes_per_line) {
  (void)dpy;
  (void)visual;
  (void)offset;
  if (format != ZPixmap || (depth != 24 && depth != 32)) return NULL;
  if (w == 0 || h == 0 || w > INT32_MAX / BYTES_PER_PIXEL) return NULL;

  int64_t pad = bitmap_pad >= 8 ? bitmap_pad : 32;
  int64_t tight = ((int64_t)w * BYTES_PER_PIXEL * 8 + pad - 1) / pad * pad / 8;
  if (bytes_per_line == 0) bytes_per_line = (int)tight;
  if (bytes_per_line < (int64_t)w * BYTES_PER_PIXEL) return NULL;
  if ((int64_t)bytes_per_line * h > INT32_MAX) return NULL;

  XImage *im = (XImage *)calloc(1, sizeof(*im));
  if (!im) return NULL;
  im->width = (int)w;
  im->height = (int)h;
  im->depth = (int)depth;
  im->bits_per_pixel = BYTES_PER_PIXEL * 8;
  im->bytes_per_line = bytes_per_line;
  im->data = data;
  return im;
}

static char *pixel_at(const XImage *im, int x, int y) {
  if (!im->data || x < 0 || y < 0 || x >= im->width || y >= im->height)
    return NULL;
  return im->data + (size_t)y * (size_t)im->bytes_per_line +
         (size_t)x * BYTES_PER_PIXEL;
}

unsigned long XGetPixel(XImage *im, int x, int y) {
  const char *p = pixel_at(im, x, y);
  uint32_t v = 0;
  if (p) memcpy(&v, p, sizeof(v));
  return v;
}

int XPutPixel(XImage *im, int x, int y, unsigned long pixel) {
  char *p = pixel_at(im, x, y);
  uint32_t v = (uint32_t)pixel;
  if (p) memcpy(p, &v, sizeof(v));
  return 1;
}

int XDestroyImage(XImage *im) {
  if (!im) return 1;
  free(im->data);
  free(im);
  return 1;
}

XImage *file_to_ximage(Display *dpy, Visual *visual, const char *filename) {
  (void)dpy;
  (void)visual;
  (void)filename;
  return NULL;
}

XImage *XGetImage(Display *dpy, Drawable d, int x, int y, unsigned int w,
                  unsigned int h, unsigned long plane_mask, int format) {
  (void)plane_mask;
  Canvas *src = xshim_target(dpy, d);
  if (!src || format != ZPixmap) return NULL;
  XImage *im = XCreateImage(dpy, NULL, 24, ZPixmap, 0, NULL, w, h, 32, 0);
  if (!im) return NULL;
  im->data = (char *)calloc(h, (size_t)im->bytes_per_line);
  if (!im->data) {
    free(im);
    return NULL;
  }
  for (unsigned int j = 0; j < h; j++) {
    const int64_t sy = (int64_t)y + j;
    if (sy < 0 || sy >= src->h) continue;
    for (unsigned int i = 0; i < w; i++) {
      const int64_t sx = (int64_t)x + i;
      if (sx < 0 || sx >= src->w) continue;
      XPutPixel(im, (int)i, (int)j, src->px[sy * src->w + sx]);
    }
  }
  return im;
}

int XPutImage(Display *dpy, Drawable d, GC gc, XImage *im, int src_x,
              int src_y, int dst_x, int dst_y, unsigned int w,
              unsigned int h) {
  (void)gc;
  Canvas *dst = xshim_target(dpy, d);
  if (!dst || !im || !im->data) return 0;
  const int64_t i0 = max_i64(0, max_i64(-(int64_t)src_x, -(int64_t)dst_x));
  const int64_t j0 = max_i64(0, max_i64(-(int64_t)src_y, -(int64_t)dst_y));
  const int64_t i1 = min_i64(w, min_i64((int64_t)im->width - src_x,
                                        (int64_t)dst->w - dst_x));
  const int64_t j1 = min_i64(h, min_i64((int64_t)im->height - src_y,
                                        (int64_t)dst->h - dst_y));
  if (i1 <= i0 || j1 <= j0) return 0;
  for (int64_t j = j0; j < j1; j++)
    for (int64_t i = i0; i < i1; i++)
      dst->px[(dst_y + j) * dst->w + dst_x + i] =
          (uint16_t)XGetPixel(im, (int)(src_x + i), (int)(src_y + j));
  canvas_mark_dirty(dst, (int)(dst_x + i0), (int)(dst_y + j0), (int)(i1 - i0),
                    (int)(j1 - j0));
  return 0;
}

int XLookupString(XKeyEvent *event, char *buffer, int bytes, KeySym *keysym,
                  void *status) {
  (void)event, (void)buffer, (void)bytes, (void)status;
  if (keysym) *keysym = 0;
  return 0;
}
