/* The colour of a pixel, for a hack that asks for a lot of them. Ours, not derived
 * from xscreensaver code.
 *
 * XQueryColor fills a colour's channels from its pixel. analogtv asks for two a
 * sample (about 250,000 a picture), so the call is inline here too: the same
 * function the shim's XQueryColor runs, for the one file that is built to use it
 * (hacks/analogtv_tv.c). The canvas holds each RGB565 pixel's bytes swapped. */
#ifndef XSHIM_QUERY_COLOR_H
#define XSHIM_QUERY_COLOR_H

#include <stdint.h>

#include "core/canvas.h"
#include "x11shim/xshim.h"

/* A channel of n bits widened to 16: value * 65535 / (2^n - 1). Looked up, not
 * divided, because analogtv asks for two colours a sample (about 250,000 a
 * picture) and the divides were most of the 150 ms that cost on the board. */
static const uint16_t k_expand5[] = {
    0, 2114, 4228, 6342, 8456, 10570, 12684, 14798,
    16912, 19026, 21140, 23254, 25368, 27482, 29596, 31710,
    33824, 35938, 38052, 40166, 42280, 44394, 46508, 48622,
    50736, 52850, 54964, 57078, 59192, 61306, 63420, 65535,
};
static const uint16_t k_expand6[] = {
    0, 1040, 2080, 3120, 4160, 5201, 6241, 7281,
    8321, 9362, 10402, 11442, 12482, 13523, 14563, 15603,
    16643, 17684, 18724, 19764, 20804, 21845, 22885, 23925,
    24965, 26005, 27046, 28086, 29126, 30166, 31207, 32247,
    33287, 34327, 35368, 36408, 37448, 38488, 39529, 40569,
    41609, 42649, 43690, 44730, 45770, 46810, 47850, 48891,
    49931, 50971, 52011, 53052, 54092, 55132, 56172, 57213,
    58253, 59293, 60333, 61374, 62414, 63454, 64494, 65535,
};

static inline void xshim_query_rgb565(XColor *c) {
  const uint16_t p = px_swap((uint16_t)c->pixel);
  c->red = k_expand5[(p >> 11) & 0x1F];
  c->green = k_expand6[(p >> 5) & 0x3F];
  c->blue = k_expand5[p & 0x1F];
  c->flags = DoRed | DoGreen | DoBlue;
}

#endif
