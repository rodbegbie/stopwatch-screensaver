/* Shim versions of the few xscreensaver utils helpers that hacks call.
 * Written for this project; not derived from xscreensaver's utils. */
#include <math.h>
#include <stdlib.h>

#include "alpha.h"
#include "colors.h"
#include "erase.h"
#include "grabclient.h"
#include "hsv.h"
#include "ximage-loader.h"
#include "x11shim/pixmap.h"

static double frand_unit(void) {
  return (double)random() / ((double)RAND_MAX + 1.0);
}

Bool has_writable_cells(Screen *screen, Visual *visual) {
  (void)screen, (void)visual;
  return False;
}

void make_random_colormap(Screen *screen, Visual *visual, Colormap cmap,
                          XColor *colors, int *ncolorsP, Bool bright_p,
                          Bool allocate_p, Bool *writable_pP, Bool verbose_p) {
  (void)screen;
  (void)visual;
  (void)cmap;
  (void)allocate_p;
  (void)verbose_p;
  if (writable_pP) *writable_pP = False;
  for (int i = 0; i < *ncolorsP; i++) {
    double s = bright_p ? 0.6 + frand_unit() * 0.4 : 0.2 + frand_unit() * 0.8;
    double v = bright_p ? 0.8 + frand_unit() * 0.2 : 0.5 + frand_unit() * 0.5;
    hsv_to_rgb((int)(frand_unit() * 360), s, v, &colors[i].red,
               &colors[i].green, &colors[i].blue);
    colors[i].flags = DoRed | DoGreen | DoBlue;
    colors[i].pixel = rgb565_from16(colors[i].red, colors[i].green, colors[i].blue);
  }
}

static void set_colour(XColor *c, double h, double s, double v) {
  h = fmod(h, 360.0);
  if (h < 0) h += 360.0;
  hsv_to_rgb((int)h, s, v, &c->red, &c->green, &c->blue);
  c->flags = DoRed | DoGreen | DoBlue;
  c->pixel = rgb565_from16(c->red, c->green, c->blue);
}

void make_uniform_colormap(Screen *screen, Visual *visual, Colormap cmap,
                           XColor *colors, int *ncolorsP, Bool allocate_p,
                           Bool *writable_pP, Bool verbose_p) {
  (void)screen;
  (void)visual;
  (void)cmap;
  (void)allocate_p;
  (void)verbose_p;
  if (writable_pP) *writable_pP = False;
  double base = frand_unit() * 360.0;
  for (int i = 0; i < *ncolorsP; i++)
    set_colour(&colors[i], base + i * 360.0 / *ncolorsP, 1.0, 1.0);
}

void make_color_ramp(Screen *screen, Visual *visual, Colormap cmap, int h1,
                     double s1, double v1, int h2, double s2, double v2,
                     XColor *colors, int *ncolorsP, Bool closed_p,
                     Bool allocate_p, Bool *writable_pP) {
  (void)screen;
  (void)visual;
  (void)cmap;
  (void)allocate_p;
  if (writable_pP) *writable_pP = False;
  const int n = *ncolorsP;
  int dh = ((h2 - h1) % 360 + 360) % 360;
  if (dh > 180) dh -= 360;
  for (int i = 0; i < n; i++) {
    double t = n > 1 ? (double)i / (n - 1) : 0.0;
    if (closed_p) {
      t = (double)i * 2.0 / n;
      if (t > 1.0) t = 2.0 - t;
    }
    set_colour(&colors[i], h1 + dh * t, s1 + (s2 - s1) * t, v1 + (v2 - v1) * t);
  }
}

void make_color_loop(Screen *screen, Visual *visual, Colormap cmap, int h1,
                     double s1, double v1, int h2, double s2, double v2, int h3,
                     double s3, double v3, XColor *colors, int *ncolorsP,
                     Bool allocate_p, Bool *writable_pP) {
  (void)screen;
  (void)visual;
  (void)cmap;
  (void)allocate_p;
  if (writable_pP) *writable_pP = False;
  const int h[4] = {h1, h2, h3, h1};
  const double s[4] = {s1, s2, s3, s1}, v[4] = {v1, v2, v3, v1};
  const int n = *ncolorsP;
  for (int k = 0; k < 3; k++) {
    const int start = k * n / 3, end = (k + 1) * n / 3;
    int dh = ((h[k + 1] - h[k]) % 360 + 360) % 360;
    if (dh > 180) dh -= 360;
    for (int i = start; i < end; i++) {
      const double t = (double)(i - start) / (end - start);
      set_colour(&colors[i], h[k] + dh * t, s[k] + (s[k + 1] - s[k]) * t,
                 v[k] + (v[k + 1] - v[k]) * t);
    }
  }
}

/* A closed path through two to five random colours, as xscreensaver's smooth
 * colour maps are, so a palette can be muted, dark or cover only part of the
 * hue wheel. Two points go there and back; more are spaced along the path in
 * proportion to the length of each edge in hue, saturation and value. Written
 * for this project; the shape of the choice follows upstream's behaviour. */
#define SMOOTH_MAX_POINTS 5

static double hue_gap(int a, int b) {
  double d = fabs((double)(a - b)) / 360.0;
  return d > 0.5 ? 1.0 - d : d;
}

static double colour_distance(int h1, double s1, double v1, int h2, double s2,
                              double v2) {
  const double dh = hue_gap(h1, h2), ds = s1 - s2, dv = v1 - v2;
  return sqrt(dh * dh + ds * ds + dv * dv);
}

static int smooth_point_count(void) {
  const int n = random() % 20;
  return n <= 5 ? 2 : n <= 15 ? 3 : n <= 18 ? 4 : 5;
}

void make_smooth_colormap(Screen *screen, Visual *visual, Colormap cmap,
                          XColor *colors, int *ncolorsP, Bool allocate_p,
                          Bool *writable_pP, Bool verbose_p) {
  (void)screen;
  (void)visual;
  (void)cmap;
  (void)allocate_p;
  (void)verbose_p;
  if (writable_pP) *writable_pP = False;
  const int total = *ncolorsP;
  if (total <= 0) return;
  const int npoints = smooth_point_count();
  int h[SMOOTH_MAX_POINTS];
  double s[SMOOTH_MAX_POINTS], v[SMOOTH_MAX_POINTS];
  for (int tries = 0;; tries++) {
    double total_s = 0, total_v = 0;
    for (int i = 0; i < npoints; i++) {
      for (int again = 0; again < 1000; again++) {
        h[i] = (int)(frand_unit() * 360.0);
        s[i] = frand_unit();
        v[i] = 0.2 + frand_unit() * 0.8;
        if (i == 0) break;
        const int j = i + 1 == npoints ? 0 : i - 1;
        if (colour_distance(h[i], s[i], v[i], h[j], s[j], v[j]) >= 0.2) break;
      }
      total_s += s[i];
      total_v += v[i];
    }
    if ((total_s / npoints >= 0.2 && total_v / npoints >= 0.3) || tries > 1000)
      break;
  }
  if (npoints == 2) {
    make_color_ramp(NULL, NULL, 0, h[0], s[0], v[0], h[1], s[1], v[1], colors,
                    ncolorsP, True, True, NULL);
    return;
  }
  double edge[SMOOTH_MAX_POINTS], circumference = 0;
  for (int i = 0; i < npoints; i++) {
    const int j = (i + 1) % npoints;
    edge[i] = colour_distance(h[i], s[i], v[i], h[j], s[j], v[j]);
    circumference += edge[i];
  }
  int out = 0;
  double walked = 0;
  for (int i = 0; i < npoints; i++) {
    const int j = (i + 1) % npoints;
    walked += edge[i];
    const int end = i + 1 == npoints || circumference <= 0
                        ? total
                        : (int)(total * walked / circumference);
    int dh = ((h[j] - h[i]) % 360 + 360) % 360;
    if (dh > 180) dh -= 360;
    const int count = end - out > 0 ? end - out : 1;
    for (int k = 0; k < count && out < total; k++, out++) {
      const double t = (double)k / count;
      set_colour(&colors[out], h[i] + dh * t, s[i] + (s[j] - s[i]) * t,
                 v[i] + (v[j] - v[i]) * t);
    }
  }
  for (; out < total; out++) colors[out] = colors[out - 1];
}

void free_colors(Screen *screen, Colormap cmap, XColor *colors, int ncolors) {
  (void)screen;
  (void)cmap;
  (void)colors;
  (void)ncolors;
}

#ifdef ARDUINO
#include <signal.h>

/* The ESP32 C library has no signal(), but flame.c calls
 * signal(SIGFPE, SIG_IGN). The Xtensa FPU never traps, so ignoring the signal
 * is already what happens. */
_sig_func_ptr signal(int sig, _sig_func_ptr handler) {
  (void)sig;
  (void)handler;
  return SIG_DFL;
}
#endif

void allocate_alpha_colors(Screen *screen, Visual *visual, Colormap cmap,
                           int *nplanesP, Bool additive_p,
                           unsigned long **plane_masks,
                           unsigned long *base_pixelP) {
  (void)screen, (void)visual, (void)cmap, (void)additive_p;
  *nplanesP = 0;
  *plane_masks = NULL;
  if (base_pixelP) *base_pixelP = 0;
}

static const unsigned char *g_grab_blob;
static unsigned long g_grab_size;

void xshim_set_grab_image(const unsigned char *blob, unsigned long size) {
  g_grab_blob = blob;
  g_grab_size = blob ? size : 0;
}

/* Scales the picture to cover the whole target, keeping its shape, and crops
 * what overhangs evenly on both sides (a 4:3 picture on a square screen loses
 * its left and right edges). Nearest pixel, a row at a time. The blob's mask
 * is ignored: a grabbed screen has no holes. */
static void paint_grab_image(Display *dpy, Window top_level, Drawable target) {
  Canvas *c = xshim_target(dpy, target);
  if (!c || c->w <= 0 || c->h <= 0) return;
  int w = 0, h = 0;
  Pixmap image = image_data_to_pixmap(dpy, top_level, g_grab_blob, g_grab_size,
                                      &w, &h, NULL);
  if (image == None) return;
  const struct XshimPixmap *pm = (const struct XshimPixmap *)(uintptr_t)image;
  const float sx = (float)c->w / (float)w, sy = (float)c->h / (float)h;
  const float scale = sx > sy ? sx : sy;
  const float left = ((float)c->w - (float)w * scale) / 2.0f;
  const float top = ((float)c->h - (float)h * scale) / 2.0f;
  uint16_t *row = (uint16_t *)malloc((size_t)c->w * sizeof(uint16_t));
  int *column = (int *)malloc((size_t)c->w * sizeof(int));
  if (row && column) {
    for (int x = 0; x < c->w; x++) {
      const int from = (int)floorf(((float)x + 0.5f - left) / scale);
      column[x] = from < 0 ? 0 : from >= w ? w - 1 : from;
    }
    for (int y = 0; y < c->h; y++) {
      int line = (int)floorf(((float)y + 0.5f - top) / scale);
      line = line < 0 ? 0 : line >= h ? h - 1 : line;
      const uint16_t *from = pm->canvas.px + (int64_t)line * w;
      for (int x = 0; x < c->w; x++) row[x] = from[column[x]];
      canvas_paste_rect(c, 0, y, c->w, 1, row);
    }
  }
  free(row);
  free(column);
  XFreePixmap(dpy, image);
}

async_load_state *load_image_async_simple(async_load_state *state, Screen *screen,
                                          Window top_level, Drawable target,
                                          char **filename_ret,
                                          XRectangle *geometry_ret) {
  (void)screen;
  if (filename_ret) *filename_ret = NULL;
  if (geometry_ret) *geometry_ret = (XRectangle){0, 0, 0, 0};
  if (!state && g_grab_blob) paint_grab_image(xshim_current_display(), top_level, target);
  return NULL;
}
