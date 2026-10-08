#include "x11shim/pixmap.h"

#include <stdlib.h>
#include <string.h>

#include "ximage-loader.h"

/* The runner's one window is number 1 (see RootWindowOfScreen); a pixmap
 * handle is a pointer, so the two cannot clash. */
#define WINDOW_ID 1UL

static struct XshimPixmap *pixmap_of(Pixmap p) {
  return (p == None || p == WINDOW_ID) ? NULL : (struct XshimPixmap *)(uintptr_t)p;
}

static void pixmap_free(struct XshimPixmap *pm) {
  if (!pm) return;
  free(pm->rgb);
  free(pm->bits);
  free(pm);
}

static struct XshimPixmap *pixmap_new(int w, int h, int depth) {
  struct XshimPixmap *pm = (struct XshimPixmap *)calloc(1, sizeof(*pm));
  if (!pm) return NULL;
  pm->w = w;
  pm->h = h;
  pm->depth = depth;
  pm->stride = (w + 7) / 8;
  if (depth == 1)
    pm->bits = (uint8_t *)malloc((size_t)pm->stride * (size_t)h);
  else
    pm->rgb = (uint16_t *)malloc((size_t)w * (size_t)h * sizeof(uint16_t));
  if (!pm->bits && !pm->rgb) {
    pixmap_free(pm);
    return NULL;
  }
  return pm;
}

static int bit_at(const struct XshimPixmap *pm, int x, int y) {
  return (pm->bits[y * pm->stride + x / 8] >> (7 - x % 8)) & 1;
}

static unsigned le16(const unsigned char *p) { return p[0] | (p[1] << 8); }

Pixmap image_data_to_pixmap(Display *dpy, Window win, const unsigned char *data,
                            unsigned long size, int *width_ret,
                            int *height_ret, Pixmap *mask_ret) {
  (void)dpy, (void)win;
  *width_ret = *height_ret = 0;
  *mask_ret = None;
  if (size < 8 || memcmp(data, "565M", 4) != 0) return None;

  const unsigned w = le16(data + 4), h = le16(data + 6);
  const uint64_t pixel_bytes = (uint64_t)w * h * 2;
  const uint64_t mask_bytes = (uint64_t)((w + 7) / 8) * h;
  if (w == 0 || h == 0 || size != 8 + pixel_bytes + mask_bytes) return None;

  struct XshimPixmap *colour = pixmap_new((int)w, (int)h, 16);
  struct XshimPixmap *mask = pixmap_new((int)w, (int)h, 1);
  if (!colour || !mask) {
    pixmap_free(colour);
    pixmap_free(mask);
    return None;
  }
  for (unsigned i = 0; i < w * h; i++) colour->rgb[i] = px_swap((uint16_t)le16(data + 8 + 2 * i));
  memcpy(mask->bits, data + 8 + pixel_bytes, (size_t)mask_bytes);

  *width_ret = (int)w;
  *height_ret = (int)h;
  *mask_ret = (Pixmap)(uintptr_t)mask;
  return (Pixmap)(uintptr_t)colour;
}

int XFreePixmap(Display *dpy, Pixmap p) {
  (void)dpy;
  pixmap_free(pixmap_of(p));
  return 0;
}

Status XGetGeometry(Display *dpy, Drawable d, Window *root, int *x, int *y,
                    unsigned int *w, unsigned int *h, unsigned int *border,
                    unsigned int *depth) {
  int width, height, bits;
  if (d == WINDOW_ID) {
    width = dpy->canvas->w;
    height = dpy->canvas->h;
    bits = 16;
  } else if (pixmap_of(d)) {
    width = pixmap_of(d)->w;
    height = pixmap_of(d)->h;
    bits = pixmap_of(d)->depth;
  } else {
    return 0;
  }
  if (root) *root = WINDOW_ID;
  if (x) *x = 0;
  if (y) *y = 0;
  if (w) *w = (unsigned)width;
  if (h) *h = (unsigned)height;
  if (border) *border = 0;
  if (depth) *depth = (unsigned)bits;
  return 1;
}

void xshim_gc_release_clip(GC gc) {
  pixmap_free(gc->clip);
  gc->clip = NULL;
}

int XSetClipMask(Display *dpy, GC gc, Pixmap mask) {
  (void)dpy;
  const struct XshimPixmap *src = pixmap_of(mask);
  if (mask != None && (!src || src->depth != 1)) return 0;
  xshim_gc_release_clip(gc);
  if (!src) return 0;
  gc->clip = pixmap_new(src->w, src->h, 1);
  if (gc->clip) memcpy(gc->clip->bits, src->bits, (size_t)src->stride * (size_t)src->h);
  return 0;
}

int XSetClipOrigin(Display *dpy, GC gc, int x, int y) {
  (void)dpy;
  gc->clip_x = x;
  gc->clip_y = y;
  return 0;
}

int XSetBackground(Display *dpy, GC gc, unsigned long pixel) {
  (void)dpy;
  gc->background = pixel;
  return 0;
}

static int clipped_out(const GC gc, int64_t x, int64_t y) {
  if (!gc->clip) return 0;
  const int64_t mx = x - gc->clip_x, my = y - gc->clip_y;
  if (mx < 0 || my < 0 || mx >= gc->clip->w || my >= gc->clip->h) return 1;
  return !bit_at(gc->clip, (int)mx, (int)my);
}

static int64_t min64(int64_t a, int64_t b) { return a < b ? a : b; }
static int64_t max64(int64_t a, int64_t b) { return a > b ? a : b; }

/* Walks only the part of the copy that lies inside both the source and the
 * canvas, so a huge width or height costs nothing. All sums are in 64 bits. */
static void copy_pixels(Display *dpy, GC gc, const struct XshimPixmap *src,
                        int sx, int sy, unsigned int w, unsigned int h, int dx,
                        int dy) {
  const Canvas *cv = dpy->canvas;
  const int64_t i0 = max64(0, max64(-(int64_t)sx, -(int64_t)dx));
  const int64_t i1 = min64(w, min64((int64_t)src->w - sx, (int64_t)cv->w - dx));
  const int64_t j0 = max64(0, max64(-(int64_t)sy, -(int64_t)dy));
  const int64_t j1 = min64(h, min64((int64_t)src->h - sy, (int64_t)cv->h - dy));
  for (int64_t j = j0; j < j1; j++) {
    for (int64_t i = i0; i < i1; i++) {
      const int64_t x = dx + i, y = dy + j;
      if (clipped_out(gc, x, y)) continue;
      const int64_t px = sx + i, py = sy + j;
      const uint16_t colour =
          src->depth == 1
              ? (uint16_t)(bit_at(src, (int)px, (int)py) ? gc->foreground : gc->background)
              : src->rgb[py * src->w + px];
      canvas_point(dpy->canvas, (int)x, (int)y, colour);
    }
  }
}

int XCopyArea(Display *dpy, Drawable src, Drawable dst, GC gc, int sx, int sy,
              unsigned int w, unsigned int h, int dx, int dy) {
  (void)dst;
  const struct XshimPixmap *pm = pixmap_of(src);
  if (pm && pm->depth == 16) copy_pixels(dpy, gc, pm, sx, sy, w, h, dx, dy);
  return 0;
}

int XCopyPlane(Display *dpy, Drawable src, Drawable dst, GC gc, int sx,
               int sy, unsigned int w, unsigned int h, int dx, int dy,
               unsigned long plane) {
  (void)dst;
  const struct XshimPixmap *pm = pixmap_of(src);
  if (pm && pm->depth == 1 && plane == 1)
    copy_pixels(dpy, gc, pm, sx, sy, w, h, dx, dy);
  return 0;
}

int XSync(Display *dpy, Bool discard) {
  (void)dpy, (void)discard;
  return 0;
}
