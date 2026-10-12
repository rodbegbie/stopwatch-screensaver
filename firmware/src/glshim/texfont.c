/* The bitmap stand-in for texfont.h. Written for this project; the glyphs are
 * TinyGL's font8x8_basic.h (public domain, Daniel Hepper). */
#include "texfont.h"

#include <stdlib.h>
#include <string.h>

#include "GL/gl.h"
#include "tinygl/src/font8x8_basic.h"

/* Rows 0 to 6 of a cell sit on or above the baseline, row 7 below it. */
enum { kCell = 8, kAscentRows = 7 };

struct texture_font_data {
  GLfloat scale; /* pixels per font pixel */
};

texture_font_data *load_texture_font(Display *dpy, char *res) {
  char *spec = get_string_resource(dpy, res, "Font");
  if (!spec) return NULL;
  const char *digits = spec + strlen(spec);
  while (digits > spec && digits[-1] >= '0' && digits[-1] <= '9') digits--;
  const int points = atoi(digits);
  free(spec);
  texture_font_data *font = calloc(1, sizeof(*font));
  if (!font) return NULL;
  font->scale = (points > 0 ? points : 16) / 2.0f / kCell;
  return font;
}

void free_texture_font(texture_font_data *font) { free(font); }

static int pixels(const texture_font_data *font, int font_pixels) {
  return (int)(font_pixels * font->scale + 0.5f);
}

void texture_string_metrics(texture_font_data *font, const char *s, XCharStruct *metrics_ret,
                            int *ascent_ret, int *descent_ret) {
  int lines = 1, column = 0, widest = 0;
  for (; *s; s++) {
    if (*s == '\n') {
      lines++;
      column = 0;
    } else if (++column > widest) {
      widest = column;
    }
  }
  const int ascent = pixels(font, kAscentRows), descent = pixels(font, kCell - kAscentRows);
  if (metrics_ret) {
    memset(metrics_ret, 0, sizeof(*metrics_ret));
    metrics_ret->width = metrics_ret->rbearing = (short)pixels(font, widest * kCell);
    metrics_ret->ascent = (short)ascent;
    metrics_ret->descent = (short)(descent + pixels(font, (lines - 1) * kCell));
  }
  if (ascent_ret) *ascent_ret = ascent;
  if (descent_ret) *descent_ret = descent;
}

void print_texture_string(texture_font_data *font, const char *s) {
  const GLfloat k = font->scale;
  int line = 0, column = 0;
  glBegin(GL_QUADS);
  for (; *s; s++) {
    if (*s == '\n') {
      line++;
      column = 0;
      continue;
    }
    const GLbyte *glyph = font8x8_basic[(unsigned char)*s & 0x7f];
    for (int row = 0; row < kCell; row++) {
      const unsigned bits = (unsigned char)glyph[row];
      const GLfloat top = (GLfloat)(kAscentRows - row - line * kCell) * k;
      for (int x = 0; x < kCell;) {
        if (!(bits >> x & 1)) {
          x++;
          continue;
        }
        const int start = x;
        while (x < kCell && (bits >> x & 1)) x++;
        const GLfloat x0 = (GLfloat)(column * kCell + start) * k;
        const GLfloat x1 = (GLfloat)(column * kCell + x) * k;
        glVertex3f(x0, top - k, 0);
        glVertex3f(x1, top - k, 0);
        glVertex3f(x1, top, 0);
        glVertex3f(x0, top, 0);
      }
    }
    column++;
  }
  glEnd();
}
