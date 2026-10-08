#ifndef CORE_CANVAS_H
#define CORE_CANVAS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int w, h;
  uint16_t *px; /* px[y * w + x], native RGB565 */
} Canvas;

/* Returns 0 on success, -1 if alloc returns NULL. */
int canvas_init(Canvas *c, int w, int h, void *(*alloc)(size_t));
void canvas_free(Canvas *c);

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
uint16_t rgb565_from16(uint16_t r, uint16_t g, uint16_t b);

/* All drawing is clipped to the canvas; any int coordinates are safe. */
void canvas_clear(Canvas *c, uint16_t color);
void canvas_point(Canvas *c, int x, int y, uint16_t color);
void canvas_line(Canvas *c, int x0, int y0, int x1, int y1, uint16_t color);
void canvas_fill_rect(Canvas *c, int x, int y, int w, int h, uint16_t color);
void canvas_fill_ellipse(Canvas *c, int x, int y, int w, int h, uint16_t color);
/* Copies the w*h rect at x,y into dst (row stride w), or writes it back from
 * src. Parts of the rect outside the canvas are skipped: their dst entries are
 * left alone on copy, and nothing is pasted for them. Lets a caller stamp
 * something over the canvas, push it, then put the hack's pixels back. */
void canvas_copy_rect(const Canvas *c, int x, int y, int w, int h, uint16_t *dst);
void canvas_paste_rect(Canvas *c, int x, int y, int w, int h, const uint16_t *src);
/* xy: x0,y0,x1,y1,... ; fewer than 3 points draws nothing. */
void canvas_fill_polygon(Canvas *c, const int *xy, int npoints, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif
