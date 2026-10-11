/* Builds the unmodified fireworkx.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software), with its decimal
 * literals made floats at build time (tools/float_literals.py), or `0.5 * x`
 * would stay software double. Every particle of every shell is moved in
 * double each frame.
 *
 * It also renders at 1/FIREWORKX_SCALE of the width and height (hacks/
 * lowrez_put.h): the glow blur, the flash pass, the conversion to RGB565 and
 * the 4-byte-a-pixel buffers are full-frame walks over PSRAM, 330 ms a frame
 * at 466 by 466 (7.7 fps at a scale of 2, 11 at 3, 12.4 at 4). The picture is
 * already a soft glow. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#ifndef FIREWORKX_SCALE
#define FIREWORKX_SCALE 3
#endif
#define LOWREZ_SCALE FIREWORKX_SCALE
#include "hacks/lowrez_put.h"

#include "hacks/single_precision.h"
#include "hacks/single_frand.h"

#define XGetWindowAttributes lowrez_get_attributes
#define XPutImage lowrez_put_image

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#undef M_PI
#define M_PI 3.14159265358979323846f

/* Below 24 bits a pixel, resize() gives the image a buffer of its own, and
 * upstream's free sets `data` to NULL before XDestroyImage (it assumes the
 * image shares the render buffer, as it does at 24 bits): 48 KB lost each
 * time Fireworkx stops. */
#define fireworkx_free fireworkx_free_upstream
#include "fireworkx_floatlit.c"
#undef fireworkx_free

static void fireworkx_free(Display *dpy, Window window, void *closure) {
  struct state *st = (struct state *)closure;
  if (st->xim && st->xim->data != (char *)st->palaka2) {
    free(st->xim->data);
    st->xim->data = NULL;
  }
  fireworkx_free_upstream(dpy, window, closure);
}

const HackEntry fireworkx_hack = {"Fireworkx", fireworkx_defaults, fireworkx_init,
                                  fireworkx_draw, fireworkx_free, NULL, NULL};
