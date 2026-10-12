/* Shim versions of the few xscreensaver utils helpers that hacks call.
 * Written for this project; not derived from xscreensaver's utils. */
#include <math.h>
#include <stdlib.h>

#include "alpha.h"
#include "colors.h"
#include "erase.h"
#include "hsv.h"

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

/* Three anchor colours, one per third of the hue wheel so the loop always
 * goes all the way round, blended forward from each anchor to the next and
 * from the last back to the first. */
#define SMOOTH_ANCHORS 3

void make_smooth_colormap(Screen *screen, Visual *visual, Colormap cmap,
                          XColor *colors, int *ncolorsP, Bool allocate_p,
                          Bool *writable_pP, Bool verbose_p) {
  (void)screen;
  (void)visual;
  (void)cmap;
  (void)allocate_p;
  (void)verbose_p;
  if (writable_pP) *writable_pP = False;
  double h[SMOOTH_ANCHORS], s[SMOOTH_ANCHORS], v[SMOOTH_ANCHORS];
  for (int k = 0; k < SMOOTH_ANCHORS; k++) {
    h[k] = (k + frand_unit()) * 360.0 / SMOOTH_ANCHORS;
    s[k] = 0.6 + frand_unit() * 0.4;
    v[k] = 0.75 + frand_unit() * 0.25;
  }
  for (int i = 0; i < *ncolorsP; i++) {
    double t = (double)i * SMOOTH_ANCHORS / *ncolorsP;
    int k = (int)t, next = (k + 1) % SMOOTH_ANCHORS;
    double f = t - k;
    double dh = fmod(h[next] - h[k] + 360.0, 360.0);
    set_colour(&colors[i], h[k] + dh * f, s[k] + (s[next] - s[k]) * f,
               v[k] + (v[next] - v[k]) * f);
  }
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
