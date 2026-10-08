/* Shim versions of the few xscreensaver utils helpers that hacks call.
 * Written for this project; not derived from xscreensaver's utils. */
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
