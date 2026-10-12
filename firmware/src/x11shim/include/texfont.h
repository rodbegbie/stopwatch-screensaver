/* Stand-in for xscreensaver's hacks/glx/texfont.h. Not derived from
 * xscreensaver code.
 *
 * Upstream renders a string with an X font into a texture and draws one quad.
 * There are no fonts here: the glyphs are TinyGL's public-domain 8x8 bitmap
 * font, drawn as one GL quad per run of lit pixels in a row, in the current
 * colour and transform. A font resource such as "sans-serif 24" sets the size:
 * an 8 by 8 cell becomes half the point size square (12 pixels at 24), since
 * upstream's sizes are for a desktop and the screen here is 466 pixels across.
 * The text's origin is the left end of the first line's baseline, as upstream.
 * Newlines start a new line below; tabs and [n] subscripts are not handled. */
#ifndef XSHIM_TEXFONT_H
#define XSHIM_TEXFONT_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texture_font_data texture_font_data;

/* Reads the font resource `res` (for example "titleFont"); NULL if it is not
 * set or no memory. */
texture_font_data *load_texture_font(Display *, char *res);
void free_texture_font(texture_font_data *);

/* The string's extent from its origin, and the font's ascent and descent. Any
 * argument may be NULL. */
void texture_string_metrics(texture_font_data *, const char *, XCharStruct *metrics_ret,
                            int *ascent_ret, int *descent_ret);

/* Draws the string at the origin of the current modelview. */
void print_texture_string(texture_font_data *, const char *);

#ifdef __cplusplus
}
#endif

#endif
