#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ximage-loader.h"
#include "x11shim/xshim.h"

#define BYTES_PER_PIXEL 4

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
