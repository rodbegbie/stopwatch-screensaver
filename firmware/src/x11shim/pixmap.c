#include "x11shim/pixmap.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "x11shim/inflate.h"
#include "ximage-loader.h"

static struct XshimPixmap *pixmap_of(Pixmap p) {
  return (p == None || p == XSHIM_WINDOW) ? NULL : (struct XshimPixmap *)(uintptr_t)p;
}

static void pixmap_free(struct XshimPixmap *pm) {
  if (!pm) return;
  canvas_free(&pm->canvas);
  free(pm->bits);
  free(pm);
}

/* A colour pixmap starts black; a bitmap's bits are left for the caller. */
static struct XshimPixmap *pixmap_new(int w, int h, int depth) {
  struct XshimPixmap *pm = (struct XshimPixmap *)calloc(1, sizeof(*pm));
  if (!pm) return NULL;
  pm->w = w;
  pm->h = h;
  pm->depth = depth;
  pm->stride = (w + 7) / 8;
  const int ok = depth == 1
                     ? (pm->bits = (uint8_t *)malloc((size_t)pm->stride * (size_t)h)) != NULL
                     : canvas_init(&pm->canvas, w, h, malloc) == 0;
  if (!ok) {
    pixmap_free(pm);
    return NULL;
  }
  return pm;
}

Canvas *xshim_target(Display *dpy, Drawable d) {
  if (d == XSHIM_WINDOW) return dpy->canvas;
  struct XshimPixmap *pm = pixmap_of(d);
  return pm && pm->depth == 16 ? &pm->canvas : NULL;
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

static void (*g_release_hook)(Display *dpy);

void xshim_set_release_hook(void (*hook)(Display *dpy)) { g_release_hook = hook; }

void xshim_release_pixmaps(Display *dpy) {
  /* The display outlives a hack: the window background it set must not
   * become the next hack's XClearWindow colour. */
  dpy->window_background = 0;
  while (dpy->pixmaps) {
    struct XshimPixmap *pm = dpy->pixmaps;
    untrack(dpy, pm);
    pixmap_free(pm);
  }
  if (g_release_hook) {
    void (*hook)(Display *) = g_release_hook;
    g_release_hook = NULL;
    hook(dpy);
  }
}

static int bit_at(const struct XshimPixmap *pm, int x, int y) {
  return (pm->bits[y * pm->stride + x / 8] >> (7 - x % 8)) & 1;
}

static unsigned le16(const unsigned char *p) { return p[0] | (p[1] << 8); }

/* Reads a blob's size and finds its pixels, followed by its mask. A "565M"
 * blob's lie in the blob itself; a "565Z" blob's are inflated into
 * `*unpacked`, which the caller frees (NULL for "565M"). 0 on success. */
static int blob_open(const unsigned char *data, unsigned long size, unsigned *w,
                     unsigned *h, const unsigned char **body,
                     unsigned char **unpacked) {
  *unpacked = NULL;
  if (size < 8) return -1;
  const int packed = memcmp(data, "565Z", 4) == 0;
  if (!packed && memcmp(data, "565M", 4) != 0) return -1;
  *w = le16(data + 4);
  *h = le16(data + 6);
  const uint64_t body_bytes = (uint64_t)*w * *h * 2 + (uint64_t)((*w + 7) / 8) * *h;
  /* The canvas's limit, which also keeps every size below in a 32-bit size_t. */
  if (*w == 0 || *h == 0 || *w > INT16_MAX || *h > INT16_MAX) return -1;
  if (!packed) {
    *body = data + 8;
    return size == 8 + body_bytes ? 0 : -1;
  }
  *unpacked = (unsigned char *)malloc((size_t)body_bytes);
  if (*unpacked && xshim_inflate(data + 8, size - 8, *unpacked, (size_t)body_bytes) == 0) {
    *body = *unpacked;
    return 0;
  }
  free(*unpacked);
  *unpacked = NULL;
  return -1;
}

Pixmap image_data_to_pixmap(Display *dpy, Window win, const unsigned char *data,
                            unsigned long size, int *width_ret,
                            int *height_ret, Pixmap *mask_ret) {
  (void)win;
  if (width_ret) *width_ret = 0;
  if (height_ret) *height_ret = 0;
  if (mask_ret) *mask_ret = None;
  unsigned w, h;
  const unsigned char *body;
  unsigned char *unpacked;
  if (blob_open(data, size, &w, &h, &body, &unpacked) != 0) return None;
  const uint64_t pixel_bytes = (uint64_t)w * h * 2;
  const uint64_t mask_bytes = (uint64_t)((w + 7) / 8) * h;

  struct XshimPixmap *colour = pixmap_new((int)w, (int)h, 16);
  struct XshimPixmap *mask = mask_ret ? pixmap_new((int)w, (int)h, 1) : NULL;
  if (!colour || (mask_ret && !mask)) {
    pixmap_free(colour);
    pixmap_free(mask);
    free(unpacked);
    return None;
  }
  for (unsigned i = 0; i < w * h; i++)
    colour->canvas.px[i] = px_swap((uint16_t)le16(body + 2 * i));
  if (mask) memcpy(mask->bits, body + pixel_bytes, (size_t)mask_bytes);
  free(unpacked);

  track(dpy, colour);
  if (mask) track(dpy, mask);
  if (width_ret) *width_ret = (int)w;
  if (height_ret) *height_ret = (int)h;
  if (mask_ret) *mask_ret = (Pixmap)(uintptr_t)mask;
  return (Pixmap)(uintptr_t)colour;
}

static unsigned expand(unsigned v, unsigned bits) {
  return bits == 5 ? (v << 3) | (v >> 2) : (v << 2) | (v >> 4);
}

XImage *image_data_to_ximage(Display *dpy, Visual *visual, const unsigned char *data,
                             unsigned long size) {
  unsigned w, h;
  const unsigned char *body;
  unsigned char *unpacked;
  if (blob_open(data, size, &w, &h, &body, &unpacked) != 0) return NULL;
  const uint64_t pixel_bytes = (uint64_t)w * h * 2;
  const unsigned stride = (w + 7) / 8;
  XImage *im = XCreateImage(dpy, visual, 32, ZPixmap, 0, NULL, w, h, 32, (int)(4 * w));
  if (im) im->data = (char *)malloc((size_t)4 * w * h);
  if (!im || !im->data) {
    if (im) XDestroyImage(im);
    free(unpacked);
    return NULL;
  }
  const unsigned char *mask = body + pixel_bytes;
  for (unsigned y = 0; y < h; y++) {
    unsigned char *out = (unsigned char *)im->data + (size_t)(h - 1 - y) * 4 * w;
    for (unsigned x = 0; x < w; x++, out += 4) {
      const unsigned v = le16(body + 2 * ((size_t)y * w + x));
      out[0] = (unsigned char)expand(v >> 11, 5);
      out[1] = (unsigned char)expand((v >> 5) & 63, 6);
      out[2] = (unsigned char)expand(v & 31, 5);
      out[3] = (mask[y * stride + x / 8] >> (7 - x % 8)) & 1 ? 255 : 0;
    }
  }
  free(unpacked);
  return im;
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
  if (d == XSHIM_WINDOW) {
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
  if (root) *root = XSHIM_WINDOW;
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
  if (pm->bits) memset(pm->bits, 0, (size_t)pm->stride * (size_t)pm->h);
  return (Pixmap)(uintptr_t)pm;
}

static void set_bit(struct XshimPixmap *pm, int x, int y, int on) {
  uint8_t *byte = &pm->bits[y * pm->stride + x / 8];
  const uint8_t mask = (uint8_t)(0x80 >> (x % 8));
  *byte = on ? (uint8_t)(*byte | mask) : (uint8_t)(*byte & ~mask);
}

/* Sets or clears bits x0 up to x1 (exclusive) of row y, a byte at a time. The
 * caller has clipped the span to the bitmap. */
static void set_span(struct XshimPixmap *pm, int y, int x0, int x1, int on) {
  if (x1 <= x0) return;
  uint8_t *row = pm->bits + (size_t)y * (size_t)pm->stride;
  const int first = x0 / 8, last = (x1 - 1) / 8;
  const uint8_t head = (uint8_t)(0xFF >> (x0 % 8));
  const uint8_t tail = (uint8_t)(0xFF << (7 - (x1 - 1) % 8));
  for (int b = first; b <= last; b++) {
    uint8_t mask = 0xFF;
    if (b == first) mask &= head;
    if (b == last) mask &= tail;
    row[b] = on ? (uint8_t)(row[b] | mask) : (uint8_t)(row[b] & ~mask);
  }
}

int xshim_bitmap_fill_rect(Display *dpy, Drawable d, GC gc, int x, int y,
                           unsigned int w, unsigned int h) {
  (void)dpy;
  struct XshimPixmap *pm = pixmap_of(d);
  if (!pm || pm->depth != 1) return 0;
  const int64_t x0 = max64(0, x), y0 = max64(0, y);
  const int64_t x1 = min64(pm->w, (int64_t)x + w), y1 = min64(pm->h, (int64_t)y + h);
  for (int64_t j = y0; j < y1; j++)
    set_span(pm, (int)j, (int)x0, (int)x1, (int)(gc->foreground & 1));
  return 1;
}

/* Even-odd fill, a pixel being in when its centre is, as the window's fill
 * does. `xy` is n pairs of coordinates, in pixels, of the polygon's corners. */
static void bitmap_fill_polygon_f(struct XshimPixmap *pm, const float *xy, int n,
                                  int on) {
  float stack[32];
  float *cross = n <= 32 ? stack : (float *)malloc((size_t)n * sizeof(float));
  if (!cross) return;
  float ymin = xy[1], ymax = xy[1];
  for (int i = 1; i < n; i++) {
    if (xy[2 * i + 1] < ymin) ymin = xy[2 * i + 1];
    if (xy[2 * i + 1] > ymax) ymax = xy[2 * i + 1];
  }
  const int64_t top = max64(0, (int64_t)floorf(ymin)), bottom = min64(pm->h, (int64_t)ceilf(ymax));
  for (int64_t row = top; row < bottom; row++) {
    const float sample = (float)row + 0.5f;
    int count = 0;
    for (int i = 0; i < n; i++) {
      const int j = (i + 1) % n;
      const float ya = xy[2 * i + 1], yb = xy[2 * j + 1];
      if ((sample < ya) == (sample < yb)) continue;
      const float xa = xy[2 * i], xb = xy[2 * j];
      const float at = xa + (sample - ya) * (xb - xa) / (yb - ya);
      int k = count++;
      while (k > 0 && cross[k - 1] > at) {
        cross[k] = cross[k - 1];
        k--;
      }
      cross[k] = at;
    }
    for (int k = 0; k + 1 < count; k += 2) {
      const int64_t from = max64(0, (int64_t)ceilf(cross[k] - 0.5f));
      const int64_t to = min64(pm->w, (int64_t)ceilf(cross[k + 1] - 0.5f));
      set_span(pm, (int)row, (int)from, (int)to, on);
    }
  }
  if (cross != stack) free(cross);
}

int xshim_bitmap_fill_polygon(Display *dpy, Drawable d, GC gc, const int *xy,
                              int n) {
  (void)dpy;
  struct XshimPixmap *pm = pixmap_of(d);
  if (!pm || pm->depth != 1) return 0;
  float stack[64];
  float *f = n <= 32 ? stack : (float *)malloc((size_t)n * 2 * sizeof(float));
  if (!f) return 1;
  for (int i = 0; i < 2 * n; i++) f[i] = (float)xy[i];
  bitmap_fill_polygon_f(pm, f, n, (int)(gc->foreground & 1));
  if (f != stack) free(f);
  return 1;
}

/* A thin line: one pixel on each step of the longer axis, the other
 * coordinate rounded half up, so the walk can start at the bitmap's edge
 * however far off it the endpoint is. */
static void bitmap_thin_line(struct XshimPixmap *pm, int x1, int y1, int x2, int y2,
                             int on) {
  const int64_t dx = (int64_t)x2 - x1, dy = (int64_t)y2 - y1;
  const int steep = llabs(dy) > llabs(dx);
  const int64_t major0 = steep ? y1 : x1, major1 = steep ? y2 : x2;
  const int64_t minor0 = steep ? x1 : y1;
  const int64_t run = major1 - major0, rise = steep ? dx : dy;
  const int64_t major_max = steep ? pm->h : pm->w, minor_max = steep ? pm->w : pm->h;
  const int64_t step = run < 0 ? -1 : 1;
  const int64_t count = llabs(run);
  int64_t i0 = 0, i1 = count;
  /* Keep the steps whose major coordinate is inside the bitmap. */
  if (step > 0) {
    i0 = max64(0, -major0);
    i1 = min64(count, major_max - 1 - major0);
  } else {
    i0 = max64(0, major0 - (major_max - 1));
    i1 = min64(count, major0);
  }
  const int64_t denom = count ? count : 1;
  for (int64_t i = i0; i <= i1; i++) {
    /* round(rise * i / count), halves up. |rise| <= count < 2^32, so the
     * product fits in 64 bits only unsigned. */
    const uint64_t prod = (uint64_t)llabs(rise) * (uint64_t)i;
    const uint64_t quot = prod / (uint64_t)denom, rem = prod % (uint64_t)denom;
    const int64_t q = rise < 0 ? -(int64_t)(quot + (2 * rem > (uint64_t)denom))
                               : (int64_t)(quot + (2 * rem >= (uint64_t)denom));
    const int64_t minor = minor0 + q;
    const int64_t major = major0 + step * i;
    if (minor < 0 || minor >= minor_max) continue;
    set_bit(pm, (int)(steep ? minor : major), (int)(steep ? major : minor), on);
  }
}

#define CAP_SEGMENTS 16

/* Clips the centre line to the bitmap grown by margin, in double, because the
 * ends can be anywhere in the int range and a float cannot hold them exactly.
 * Returns 0 when nothing is left. */
static int clip_to_bitmap(const struct XshimPixmap *pm, int margin, double *x1,
                          double *y1, double *x2, double *y2) {
  const double xmin = -(double)margin, xmax = (double)pm->w + margin;
  const double ymin = -(double)margin, ymax = (double)pm->h + margin;
  const double dx = *x2 - *x1, dy = *y2 - *y1;
  const double p[4] = {-dx, dx, -dy, dy};
  const double q[4] = {*x1 - xmin, xmax - *x1, *y1 - ymin, ymax - *y1};
  double t0 = 0, t1 = 1;
  for (int i = 0; i < 4; i++) {
    if (p[i] == 0) {
      if (q[i] < 0) return 0;
    } else {
      const double t = q[i] / p[i];
      if (p[i] < 0) {
        if (t > t1) return 0;
        if (t > t0) t0 = t;
      } else {
        if (t < t0) return 0;
        if (t < t1) t1 = t;
      }
    }
  }
  const double ax = *x1 + t0 * dx, ay = *y1 + t0 * dy;
  const double bx = *x1 + t1 * dx, by = *y1 + t1 * dy;
  *x1 = ax, *y1 = ay, *x2 = bx, *y2 = by;
  return 1;
}

/* A wide line is its rectangle, plus a disc at each end for a round cap and
 * a half width more for a projecting one. The coordinates sit where
 * stroke.c puts them: an odd width is centred on the pixel, an even one on its
 * corner. */
static void bitmap_wide_line(struct XshimPixmap *pm, GC gc, int x1, int y1, int x2,
                             int y2) {
  const int w = gc->line_width, on = (int)(gc->foreground & 1);
  const float o = (w & 1) ? 0.5f : 0.0f, half = (float)w / 2.0f;
  double cx1 = x1, cy1 = y1, cx2 = x2, cy2 = y2;
  if (!clip_to_bitmap(pm, w + 2, &cx1, &cy1, &cx2, &cy2)) return;
  float ax = (float)cx1 + o, ay = (float)cy1 + o, bx = (float)cx2 + o, by = (float)cy2 + o;
  float ux = bx - ax, uy = by - ay;
  const float len = sqrtf(ux * ux + uy * uy);
  if (len > 0) {
    ux /= len;
    uy /= len;
  } else {
    ux = 1;
    uy = 0;
  }
  if (gc->cap_style == CapProjecting) {
    ax -= ux * half, ay -= uy * half;
    bx += ux * half, by += uy * half;
  }
  const float nx = -uy * half, ny = ux * half;
  const float quad[8] = {ax + nx, ay + ny, bx + nx, by + ny,
                         bx - nx, by - ny, ax - nx, ay - ny};
  if (len > 0 || gc->cap_style == CapProjecting) bitmap_fill_polygon_f(pm, quad, 4, on);
  if (gc->cap_style != CapRound) return;
  const float centre[2][2] = {{(float)cx1 + o, (float)cy1 + o}, {(float)cx2 + o, (float)cy2 + o}};
  for (int end = 0; end < 2; end++) {
    float disc[2 * CAP_SEGMENTS];
    for (int i = 0; i < CAP_SEGMENTS; i++) {
      const float a = 6.2831853f * (float)i / CAP_SEGMENTS;
      disc[2 * i] = centre[end][0] + half * cosf(a);
      disc[2 * i + 1] = centre[end][1] + half * sinf(a);
    }
    bitmap_fill_polygon_f(pm, disc, CAP_SEGMENTS, on);
  }
}

int xshim_bitmap_draw_line(Display *dpy, Drawable d, GC gc, int x1, int y1, int x2,
                           int y2) {
  (void)dpy;
  struct XshimPixmap *pm = pixmap_of(d);
  if (!pm || pm->depth != 1) return 0;
  if (gc->line_width > 1)
    bitmap_wide_line(pm, gc, x1, y1, x2, y2);
  else
    bitmap_thin_line(pm, x1, y1, x2, y2, (int)(gc->foreground & 1));
  return 1;
}

Pixmap XCreatePixmapFromBitmapData(Display *dpy, Drawable d, char *data,
                                   unsigned int w, unsigned int h,
                                   unsigned long fg, unsigned long bg,
                                   unsigned int depth) {
  Pixmap p = XCreatePixmap(dpy, d, w, h, depth);
  struct XshimPixmap *pm = pixmap_of(p);
  if (!pm || !data) return p;
  const size_t row_bytes = ((size_t)w + 7) / 8;
  for (unsigned y = 0; y < h; y++)
    for (unsigned x = 0; x < w; x++) {
      const int on = ((unsigned char)data[y * row_bytes + x / 8] >> (x % 8)) & 1;
      if (pm->depth == 1)
        set_bit(pm, (int)x, (int)y, on);
      else
        pm->canvas.px[y * w + x] = (uint16_t)(on ? fg : bg);
    }
  return p;
}

/* Copies the part of the rectangle that lies inside both the source and the
 * destination (a pixmap, or the canvas when dst is NULL), so a huge width or
 * height costs nothing. All sums are in 64 bits. Between a colour source and a
 * colour destination, or two bitmaps, pixels are copied as they are; a bitmap
 * drawn on the canvas uses the GC's foreground and background. When source and
 * destination are the same pixmap and overlap, rows and columns are walked in
 * the order that reads each pixel before it is overwritten. Otherwise, with no
 * clip mask, colour rows are copied whole (with memmove, row by row in that
 * order, when the source is the destination: Kumppa copies the window onto
 * itself one pixel over). */
static void copy_region(Display *dpy, GC gc, const struct XshimPixmap *src,
                        struct XshimPixmap *dst, int sx, int sy,
                        unsigned int w, unsigned int h, int dx, int dy) {
  Canvas *const out = dst ? (dst->depth == 16 ? &dst->canvas : NULL) : dpy->canvas;
  const int64_t dw = dst ? dst->w : dpy->canvas->w;
  const int64_t dh = dst ? dst->h : dpy->canvas->h;
  const int64_t i0 = max64(0, max64(-(int64_t)sx, -(int64_t)dx));
  const int64_t i1 = min64(w, min64((int64_t)src->w - sx, dw - dx));
  const int64_t j0 = max64(0, max64(-(int64_t)sy, -(int64_t)dy));
  const int64_t j1 = min64(h, min64((int64_t)src->h - sy, dh - dy));
  if (i1 <= i0 || j1 <= j0) return;
  const int same = dst == src || (out && src->depth == 16 && out->px == src->canvas.px);
  const int reverse_x = same && dx > sx, reverse_y = same && dy > sy;
  if (src->depth == 16 && out && !gc->clip) {
    for (int64_t jj = 0; jj < j1 - j0; jj++) {
      const int64_t j = reverse_y ? j1 - 1 - jj : j0 + jj;
      const uint16_t *from = src->canvas.px + (sy + j) * src->w + sx + i0;
      if (same) {
        memmove(out->px + (dy + j) * out->w + dx + i0, from,
                (size_t)(i1 - i0) * sizeof(uint16_t));
        canvas_mark_dirty(out, (int)(dx + i0), (int)(dy + j), (int)(i1 - i0), 1);
      } else {
        canvas_paste_rect(out, (int)(dx + i0), (int)(dy + j), (int)(i1 - i0), 1, from);
      }
    }
    return;
  }
  for (int64_t jj = 0; jj < j1 - j0; jj++) {
    const int64_t j = reverse_y ? j1 - 1 - jj : j0 + jj;
    for (int64_t ii = 0; ii < i1 - i0; ii++) {
      const int64_t i = reverse_x ? i1 - 1 - ii : i0 + ii;
      const int64_t x = dx + i, y = dy + j;
      if (clipped_out(gc, x, y)) continue;
      const int64_t px = sx + i, py = sy + j;
      if (src->depth == 1) {
        const int bit = bit_at(src, (int)px, (int)py);
        if (out)
          canvas_point(out, (int)x, (int)y,
                       (uint16_t)(bit ? gc->foreground : gc->background));
        else
          set_bit(dst, (int)x, (int)y, bit);
      } else {
        canvas_point(out, (int)x, (int)y, src->canvas.px[py * src->w + px]);
      }
    }
  }
}

int xshim_fill_rect_clipped(Display *dpy, Drawable d, GC gc, int x, int y,
                            unsigned int w, unsigned int h) {
  Canvas *const c = xshim_target(dpy, d);
  if (!c || !gc->clip) return 0;
  const int64_t x0 = max64(0, x), y0 = max64(0, y);
  const int64_t x1 = min64(c->w, (int64_t)x + w), y1 = min64(c->h, (int64_t)y + h);
  for (int64_t j = y0; j < y1; j++) {
    int64_t run = -1;
    for (int64_t i = x0; i <= x1; i++) {
      const int in = i < x1 && !clipped_out(gc, i, j);
      if (in && run < 0) run = i;
      if (!in && run >= 0) {
        canvas_fill_rect(c, (int)run, (int)j, (int)(i - run), 1, (uint16_t)gc->foreground);
        run = -1;
      }
    }
  }
  return 1;
}

/* The window is a colour source like any colour pixmap. To the canvas, the
 * source must be colour. To a pixmap, source and destination must have the
 * same depth. Anything else draws nothing, including a destination of None:
 * XCreatePixmap returns it when it cannot allocate, and a hack that carries on
 * must not paint its sprites on the screen. */
int XCopyArea(Display *dpy, Drawable src, Drawable dst, GC gc, int sx, int sy,
              unsigned int w, unsigned int h, int dx, int dy) {
  struct XshimPixmap window;
  const struct XshimPixmap *pm = pixmap_of(src);
  if (src == XSHIM_WINDOW) {
    window = (struct XshimPixmap){.w = dpy->canvas->w, .h = dpy->canvas->h, .depth = 16,
                                  .canvas = *dpy->canvas};
    pm = &window;
  }
  struct XshimPixmap *target = pixmap_of(dst);
  if (!pm) return 0;
  if (!target && dst != XSHIM_WINDOW) return 0;
  if (target ? pm->depth == target->depth : pm->depth == 16)
    copy_region(dpy, gc, pm, target, sx, sy, w, h, dx, dy);
  return 0;
}

/* Source bits map to the GC's foreground and clear bits to its background; to
 * a bitmap those are their low bits. Draws on the window, a colour pixmap or
 * a bitmap; None as the destination draws nothing. */
int XCopyPlane(Display *dpy, Drawable src, Drawable dst, GC gc, int sx,
               int sy, unsigned int w, unsigned int h, int dx, int dy,
               unsigned long plane) {
  const struct XshimPixmap *pm = pixmap_of(src);
  struct XshimPixmap *target = pixmap_of(dst);
  if (!pm || pm->depth != 1 || plane != 1) return 0;
  if (target && target->depth == 1) {
    const int64_t i0 = max64(0, max64(-(int64_t)sx, -(int64_t)dx));
    const int64_t i1 = min64(w, min64((int64_t)pm->w - sx, (int64_t)target->w - dx));
    const int64_t j0 = max64(0, max64(-(int64_t)sy, -(int64_t)dy));
    const int64_t j1 = min64(h, min64((int64_t)pm->h - sy, (int64_t)target->h - dy));
    const int on = (int)(gc->foreground & 1), off = (int)(gc->background & 1);
    /* Rows are read before they are written, as XCopyArea does on one pixmap. */
    const int reverse_y = pm == target && dy > sy, reverse_x = pm == target && dx > sx;
    for (int64_t jj = 0; jj < j1 - j0; jj++) {
      const int64_t j = reverse_y ? j1 - 1 - jj : j0 + jj;
      for (int64_t ii = 0; ii < i1 - i0; ii++) {
        const int64_t i = reverse_x ? i1 - 1 - ii : i0 + ii;
        if (clipped_out(gc, dx + i, dy + j)) continue;
        set_bit(target, (int)(dx + i), (int)(dy + j),
                bit_at(pm, (int)(sx + i), (int)(sy + j)) ? on : off);
      }
    }
    return 0;
  }
  if (!xshim_target(dpy, dst)) return 0;
  copy_region(dpy, gc, pm, target, sx, sy, w, h, dx, dy);
  return 0;
}

XImage *xshim_bitmap_get_image(Display *dpy, Drawable d, int x, int y,
                               unsigned int w, unsigned int h) {
  const struct XshimPixmap *pm = pixmap_of(d);
  if (!pm || pm->depth != 1) return NULL;
  XImage *im = XCreateImage(dpy, NULL, 1, XYBitmap, 0, NULL, w, h, 32, 0);
  if (!im) return NULL;
  im->data = (char *)calloc(h, (size_t)im->bytes_per_line);
  if (!im->data) {
    free(im);
    return NULL;
  }
  for (unsigned j = 0; j < h; j++)
    for (unsigned i = 0; i < w; i++) {
      const int64_t px = (int64_t)x + i, py = (int64_t)y + j;
      if (px >= 0 && py >= 0 && px < pm->w && py < pm->h && bit_at(pm, (int)px, (int)py))
        XPutPixel(im, (int)i, (int)j, 1);
    }
  return im;
}

int xshim_bitmap_put_image(Display *dpy, Drawable d, GC gc, XImage *im, int src_x,
                           int src_y, int dst_x, int dst_y, unsigned int w,
                           unsigned int h) {
  (void)dpy;
  struct XshimPixmap *pm = pixmap_of(d);
  if (!pm || pm->depth != 1) return 0;
  if (!im || !im->data || im->bits_per_pixel != 1 || !gc) return 1;
  const int64_t i0 = max64(0, max64(-(int64_t)src_x, -(int64_t)dst_x));
  const int64_t i1 = min64(w, min64((int64_t)im->width - src_x, (int64_t)pm->w - dst_x));
  const int64_t j0 = max64(0, max64(-(int64_t)src_y, -(int64_t)dst_y));
  const int64_t j1 = min64(h, min64((int64_t)im->height - src_y, (int64_t)pm->h - dst_y));
  const int on = (int)(gc->foreground & 1), off = (int)(gc->background & 1);
  for (int64_t j = j0; j < j1; j++)
    for (int64_t i = i0; i < i1; i++)
      set_bit(pm, (int)(dst_x + i), (int)(dst_y + j),
              XGetPixel(im, (int)(src_x + i), (int)(src_y + j)) ? on : off);
  return 1;
}

int XSync(Display *dpy, Bool discard) {
  (void)dpy, (void)discard;
  return 0;
}

long XMaxRequestSize(Display *dpy) {
  (void)dpy;
  return 65535;
}
