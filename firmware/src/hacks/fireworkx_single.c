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

/* screenhack.h's frand has a `1.0` inside its macro body, which the float
 * literal pass cannot reach: it divided in double twice a particle, and a
 * light map's 54,000 cells call it too. */
#undef frand
#define frand(f) (((float)random() / ((float)RAND_MAX + 1.0f)) * (float)(f))

#define XGetWindowAttributes lowrez_get_attributes
#define XPutImage lowrez_put_image

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#undef M_PI
#define M_PI 3.14159265358979323846f

#include "fireworkx_floatlit.c"

const HackEntry fireworkx_hack = {"Fireworkx", fireworkx_defaults, fireworkx_init,
                                  fireworkx_draw, fireworkx_free, NULL, NULL};
