/* Builds the unmodified binaryring.c, told the display is 24 bits deep.
 *
 * The hack blends each anti-aliased hit into its own buffer of pixel values
 * at an alpha of 0.15 times the coverage. In its 16-bit path a hit adds less
 * than one 5-bit step of red or blue, which is dropped, so only green ever
 * builds up and the picture stays a dim green. In its 24-bit path (0xRRGGBB,
 * as on most X displays) the hits accumulate, so the wrapper gives it that
 * depth and packs each pixel to the canvas's byte-swapped RGB565 as it goes
 * into the XImage. */
#include <stdint.h>

#include "screenhack.h"

#include "core/canvas.h"

static int binaryring_put_pixel(XImage *im, int x, int y, unsigned long c) {
  const unsigned r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
  return XPutPixel(im, x, y, px_swap((uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))));
}
#define XPutPixel binaryring_put_pixel
#define visual_depth(screen, visual) 24

#include "binaryring/binaryring.c"
