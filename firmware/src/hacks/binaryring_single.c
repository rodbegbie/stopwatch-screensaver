/* Builds the unmodified binaryring.c, told the display is 24 bits deep, in
 * single precision (the ESP32-S3's FPU is single-precision only, so `double`
 * maths runs in software) and with its decimal literals made floats at build
 * time (tools/float_literals.py, and tools/binaryring_patch.py for the
 * exponent literal in frand1).
 *
 * The hack blends each anti-aliased hit into its own buffer of pixel values
 * at an alpha of 0.15 times the coverage. In its 16-bit path a hit adds less
 * than one 5-bit step of red or blue, which is dropped, so only green ever
 * builds up and the picture stays a dim green. In its 24-bit path (0xRRGGBB,
 * as on most X displays) the hits accumulate, so the wrapper gives it that
 * depth and packs each pixel to the canvas's byte-swapped RGB565 as it goes
 * into the XImage. */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "screenhack.h"

#include "core/canvas.h"

static int binaryring_put_pixel(XImage *im, int x, int y, unsigned long c) {
  const unsigned r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
  return XPutPixel(im, x, y, px_swap((uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))));
}
#define XPutPixel binaryring_put_pixel
#define visual_depth(screen, visual) 24

#include "hacks/single_precision.h"
#undef M_PI
#define M_PI 3.14159265358979323846f

#include "binaryring_floatlit.c"
