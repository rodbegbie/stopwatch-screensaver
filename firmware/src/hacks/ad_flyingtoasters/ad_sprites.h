#ifndef AD_SPRITES_H
#define AD_SPRITES_H

#include <stdint.h>

typedef enum { AD_DONE_LIGHT, AD_DONE_MEDIUM, AD_DONE_DARK } AdDoneness;
typedef struct {
  int colour;
  AdDoneness doneness;
} AdChoice;

AdChoice ad_choose(uint32_t r); /* bit 0: colour; (r >> 1) % 3: doneness */
void ad_apply_doneness(unsigned char *rgb, int pixels, AdDoneness d);
/* 512 bytes of 64x64 bits, MSB first; a set bit is black, a clear bit white. */
void ad_expand_mono(const unsigned char bits[512], unsigned char rgb[64 * 64 * 3]);
/* 70x70 RGB565 (what a 16-bit XImage holds): art at x 0..63, y 6..69; rows
 * 0..5 and columns 64..69 are black. */
void ad_build_sprite(const unsigned char rgb[64 * 64 * 3], uint16_t out[70 * 70]);

#endif
