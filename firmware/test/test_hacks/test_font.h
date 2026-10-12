/* A font for tests that run XJack. Each glyph is a block of solid and
 * half-covered pixels that depends on its character, so frames differ with the
 * text but not with the machine: the real font is rasterised by FreeType, whose
 * output can change between versions, and a pinned hash must not follow it. */
#ifndef TEST_HACKS_TEST_FONT_H
#define TEST_HACKS_TEST_FONT_H

#include <string.h>

#include "x11shim/xft_font.h"

#define TF_GLYPHS (XSHIM_FONT_LAST - XSHIM_FONT_FIRST + 1)

typedef struct {
  int pixel_size, ascent, descent, advance, w, h;
} TfSpec;

static const TfSpec tf_specs[] = {
    {12, 9, 4, 8, 6, 7},
    {16, 12, 5, 10, 8, 9},
};
#define TF_SIZES ((int)(sizeof tf_specs / sizeof tf_specs[0]))

static XshimGlyph tf_glyphs[TF_SIZES][TF_GLYPHS];
static unsigned char tf_alpha[TF_SIZES][TF_GLYPHS * 8 * 9];
static XshimFontSize tf_sizes[TF_SIZES];
static const XshimFont tf_font = {TF_SIZES, tf_sizes};

static void test_font_install(void) {
  for (int s = 0; s < TF_SIZES; s++) {
    const TfSpec *sp = &tf_specs[s];
    unsigned offset = 0;
    for (int i = 0; i < TF_GLYPHS; i++) {
      const int code = XSHIM_FONT_FIRST + i;
      XshimGlyph *g = &tf_glyphs[s][i];
      memset(g, 0, sizeof *g);
      g->advance = (uint8_t)sp->advance;
      if (code == ' ') continue;
      g->w = (uint8_t)sp->w;
      g->h = (uint8_t)sp->h;
      g->left = 1;
      g->top = (int8_t)sp->h;
      g->offset = offset;
      for (int k = 0; k < sp->w * sp->h; k++)
        tf_alpha[s][offset + k] = ((k * 7 + code * 13) % 5 == 0) ? 128 : 255;
      offset += (unsigned)(sp->w * sp->h);
    }
    tf_sizes[s] = (XshimFontSize){sp->pixel_size, sp->ascent, sp->descent,
                                  tf_glyphs[s], tf_alpha[s]};
  }
  xshim_set_font(&tf_font);
}

#endif
