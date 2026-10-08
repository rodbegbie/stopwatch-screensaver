/* Shim versions of the few xscreensaver utils helpers that hacks call.
 * Written for this project; not derived from xscreensaver's utils. */
#include <math.h>
#include <stdlib.h>

#include "colors.h"
#include "erase.h"
#include "hsv.h"

static double frand_unit(void) {
  return (double)random() / ((double)RAND_MAX + 1.0);
}

/* No wipe animation: clear the window and report that erasing is done. */
eraser_state *erase_window(Display *dpy, Window w, eraser_state *st) {
  (void)st;
  XClearWindow(dpy, w);
  return NULL;
}

void eraser_free(eraser_state *st) { (void)st; }

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
