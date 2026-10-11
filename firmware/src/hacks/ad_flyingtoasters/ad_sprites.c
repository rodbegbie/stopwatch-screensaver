#include "ad_sprites.h"

AdChoice ad_choose(uint32_t r) {
  AdChoice c;
  c.colour = (int)(r & 1);
  c.doneness = (AdDoneness)((r >> 1) % 3);
  return c;
}

void ad_apply_doneness(unsigned char *rgb, int pixels, AdDoneness d) {
  static const int k[3][3] = {{0, 0, 0}, {79, 79, 79}, {127, 111, 111}};
  if (d == AD_DONE_LIGHT) return;
  for (int i = 0; i < pixels; i++) {
    for (int c = 0; c < 3; c++) {
      int v = rgb[i * 3 + c] - k[d][c];
      rgb[i * 3 + c] = (unsigned char)(v > 0 ? v : 0);
    }
  }
}

void ad_expand_mono(const unsigned char bits[512], unsigned char rgb[64 * 64 * 3]) {
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 64; x++) {
      int set = (bits[y * 8 + (x >> 3)] >> (7 - (x & 7))) & 1;
      unsigned char v = set ? 0 : 255;
      unsigned char *p = &rgb[(y * 64 + x) * 3];
      p[0] = p[1] = p[2] = v;
    }
  }
}

void ad_build_sprite(const unsigned char rgb[64 * 64 * 3], uint16_t out[70 * 70]) {
  for (int i = 0; i < 70 * 70; i++) out[i] = 0;
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 64; x++) {
      const unsigned char *p = &rgb[(y * 64 + x) * 3];
      out[(y + 6) * 70 + x] = (uint16_t)(((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3));
    }
  }
}
