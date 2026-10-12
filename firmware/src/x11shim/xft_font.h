/* The font the Xft stand-in (xft.h) draws from. Ours, not derived from
 * xscreensaver code.
 *
 * The device has no font renderer, so a font is a set of sizes drawn in
 * advance (tools/make_font_atlas.py writes them into a header), each holding
 * 8-bit coverage for printable ASCII, space (32) to tilde (126). main.cpp hands
 * the font to the shim with xshim_set_font; tests install one built by hand. */
#ifndef X11SHIM_XFT_FONT_H
#define X11SHIM_XFT_FONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XSHIM_FONT_FIRST 32
#define XSHIM_FONT_LAST 126

/* One glyph's ink: w by h bytes of coverage at alpha + offset, row by row. Its
 * top-left pixel is `left` right of the pen and `top` above the baseline. A
 * glyph with no ink (space) has w and h of 0. `advance` moves the pen. */
typedef struct {
  uint8_t w, h;
  int8_t left, top;
  uint8_t advance;
  uint32_t offset;
} XshimGlyph;

/* glyphs holds XSHIM_FONT_LAST - XSHIM_FONT_FIRST + 1 entries, in code order. */
typedef struct {
  int pixel_size;
  int ascent, descent;
  const XshimGlyph *glyphs;
  const unsigned char *alpha;
} XshimFontSize;

typedef struct {
  int nsizes;
  const XshimFontSize *sizes;
} XshimFont;

/* NULL removes the font, and load_xft_font_retry then finds none. The font
 * must outlive every XftFont opened from it. */
void xshim_set_font(const XshimFont *);

#ifdef __cplusplus
}
#endif

#endif
