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

static void track(Display *dpy, struct XshimPixmap *pm) {
  pm->prev = NULL;
  pm->next = dpy->pixmaps;
  if (dpy->pixmaps) dpy->pixmaps->prev = pm;
  dpy->pixmaps = pm;
}

static void untrack(Display *dpy, struct XshimPixmap *pm) {
  if (pm->prev)
    pm->prev->next = pm->next;
  else
    dpy->pixmaps = pm->next;
  if (pm->next) pm->next->prev = pm->prev;
}

void xshim_release_pixmaps(Display *dpy) {
  while (dpy->pixmaps) {
    struct XshimPixmap *pm = dpy->pixmaps;
    untrack(dpy, pm);
    pixmap_free(pm);
  }
}

static int bit_at(const struct XshimPixmap *pm, int x, int y) {
  return (pm->bits[y * pm->stride + x / 8] >> (7 - x % 8)) & 1;
}

static unsigned le16(const unsigned char *p) { return p[0] | (p[1] << 8); }

Pixmap image_data_to_pixmap(Display *dpy, Window win, const unsigned char *data,
                            unsigned long size, int *width_ret,
                            int *height_ret, Pixmap *mask_ret) {
  (void)win;
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

  track(dpy, colour);
  track(dpy, mask);
  *width_ret = (int)w;
  *height_ret = (int)h;
  *mask_ret = (Pixmap)(uintptr_t)mask;
  return (Pixmap)(uintptr_t)colour;
}

int XFreePixmap(Display *dpy, Pixmap p) {
  struct XshimPixmap *pm = pixmap_of(p);
  if (pm) untrack(dpy, pm);
  pixmap_free(pm);
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

/* The largest pixmap XCreatePixmap will make: 4 million pixels, which is 8 MB
 * of RGB565, more than the board has. */
#define MAX_PIXMAP_PIXELS 4000000ULL

Pixmap XCreatePixmap(Display *dpy, Drawable d, unsigned int w, unsigned int h,
                     unsigned int depth) {
  (void)d;
  if (w == 0 || h == 0 || (uint64_t)w * h > MAX_PIXMAP_PIXELS) return None;
  struct XshimPixmap *pm = pixmap_new((int)w, (int)h, depth == 1 ? 1 : 16);
  if (!pm) return None;
  track(dpy, pm);
  if (pm->bits)
    memset(pm->bits, 0, (size_t)pm->stride * (size_t)pm->h);
  else
    memset(pm->rgb, 0, (size_t)pm->w * (size_t)pm->h * sizeof(uint16_t));
  return (Pixmap)(uintptr_t)pm;
}

static void set_bit(struct XshimPixmap *pm, int x, int y, int on) {
  uint8_t *byte = &pm->bits[y * pm->stride + x / 8];
  const uint8_t mask = (uint8_t)(0x80 >> (x % 8));
  *byte = on ? (uint8_t)(*byte | mask) : (uint8_t)(*byte & ~mask);
}

/* Copies the part of the rectangle that lies inside both the source and the
 * destination (a pixmap, or the canvas when dst is NULL), so a huge width or
 * height costs nothing. All sums are in 64 bits. Between a colour source and a
 * colour destination, or two bitmaps, pixels are copied as they are; a bitmap
 * drawn on the canvas uses the GC's foreground and background. When source and
 * destination are the same pixmap and overlap, rows and columns are walked in
 * the order that reads each pixel before it is overwritten. */
static void copy_region(Display *dpy, GC gc, const struct XshimPixmap *src,
                        struct XshimPixmap *dst, int sx, int sy,
                        unsigned int w, unsigned int h, int dx, int dy) {
  const int64_t dw = dst ? dst->w : dpy->canvas->w;
  const int64_t dh = dst ? dst->h : dpy->canvas->h;
  const int64_t i0 = max64(0, max64(-(int64_t)sx, -(int64_t)dx));
  const int64_t i1 = min64(w, min64((int64_t)src->w - sx, dw - dx));
  const int64_t j0 = max64(0, max64(-(int64_t)sy, -(int64_t)dy));
  const int64_t j1 = min64(h, min64((int64_t)src->h - sy, dh - dy));
  const int same = dst == src;
  const int reverse_x = same && dx > sx, reverse_y = same && dy > sy;
  for (int64_t jj = 0; jj < j1 - j0; jj++) {
    const int64_t j = reverse_y ? j1 - 1 - jj : j0 + jj;
    for (int64_t ii = 0; ii < i1 - i0; ii++) {
      const int64_t i = reverse_x ? i1 - 1 - ii : i0 + ii;
      const int64_t x = dx + i, y = dy + j;
      if (clipped_out(gc, x, y)) continue;
      const int64_t px = sx + i, py = sy + j;
      if (src->depth == 1) {
        const int bit = bit_at(src, (int)px, (int)py);
        if (!dst)
          canvas_point(dpy->canvas, (int)x, (int)y,
                       (uint16_t)(bit ? gc->foreground : gc->background));
        else
          set_bit(dst, (int)x, (int)y, bit);
      } else {
        const uint16_t colour = src->rgb[py * src->w + px];
        if (!dst)
          canvas_point(dpy->canvas, (int)x, (int)y, colour);
        else
          dst->rgb[y * dst->w + x] = colour;
      }
    }
  }
}

/* To the canvas, the source must be a colour pixmap. To a pixmap, source and
 * destination must have the same depth. Anything else draws nothing, including
 * a destination of None: XCreatePixmap returns it when it cannot allocate, and
 * a hack that carries on must not paint its sprites on the screen. */
int XCopyArea(Display *dpy, Drawable src, Drawable dst, GC gc, int sx, int sy,
              unsigned int w, unsigned int h, int dx, int dy) {
  const struct XshimPixmap *pm = pixmap_of(src);
  struct XshimPixmap *target = pixmap_of(dst);
  if (!pm) return 0;
  if (!target && dst != WINDOW_ID) return 0;
  if (target ? pm->depth == target->depth : pm->depth == 16)
    copy_region(dpy, gc, pm, target, sx, sy, w, h, dx, dy);
  return 0;
}

/* Always draws on the canvas, whatever dst is. */
int XCopyPlane(Display *dpy, Drawable src, Drawable dst, GC gc, int sx,
               int sy, unsigned int w, unsigned int h, int dx, int dy,
               unsigned long plane) {
  (void)dst;
  const struct XshimPixmap *pm = pixmap_of(src);
  if (pm && pm->depth == 1 && plane == 1)
    copy_region(dpy, gc, pm, NULL, sx, sy, w, h, dx, dy);
  return 0;
}

int XSync(Display *dpy, Bool discard) {
  (void)dpy, (void)discard;
  return 0;
}
