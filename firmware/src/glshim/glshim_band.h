/* Held frames, drawn in bands (issue #43). Not derived from xscreensaver code.
 *
 * A frame that begins with a clear of colour and depth is held: TinyGL still
 * clips, culls, lights and offsets, but where it would call a rasteriser it
 * calls glshim_band_fill, which stores the call. When the frame is drawn
 * (glshim_band_draw, at the swap), each band of rows is cleared in internal
 * RAM, every stored call that touches it is replayed through the same
 * rasteriser into a band-tall ZBuffer, and the band is copied to the canvas.
 * Small triangles touch new PSRAM cache lines on every row; in internal RAM
 * they do not. Policy (when to hold, the dirty box, spills) is in glshim.c. */
#ifndef GLSHIM_BAND_H
#define GLSHIM_BAND_H

#include "zbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*GlshimFillFn)(ZBuffer *, ZBufferPoint *, ZBufferPoint *,
                             ZBufferPoint *);

/* 0, or -1 if internal RAM is short; banding is then off until reopened. */
int glshim_band_open(int w, int h);
void glshim_band_close(void); /* drops any held calls */

/* Starts holding; 0 if banding is off or not open. A second begin while
 * holding drops what was held: a full clear erases it anyway. */
int glshim_band_begin(ZBuffer *zb, PIXEL clear_color, GLint clear_z);
int glshim_band_holding(void);

/* Stores the call and returns 1, or returns 0 for the caller to draw it.
 * Anything it cannot hold spills the frame first (glshim_band_spill). */
int glshim_band_fill(ZBuffer *zb, GlshimFillFn fn, ZBufferPoint *a,
                     ZBufferPoint *b, ZBufferPoint *c);

/* Draws the held calls into the canvas box (inclusive) and stops holding.
 * With write_depth the box's depth goes to zb->zbuf too. */
void glshim_band_draw(ZBuffer *zb, int x0, int y0, int x1, int y1,
                      int write_depth);

/* The PSRAM z-buffer is whole again (a direct depth clear ran). */
void glshim_band_depth_valid(void);

/* In glshim.c: draws what is held, colour and depth, so the rest of the frame
 * can be drawn directly. Called for text pixels, glDrawPixels and anything
 * glshim_band_fill cannot hold. */
void glshim_band_spill(ZBuffer *zb);

/* For tests. */
unsigned glshim_band_held_frames(void);
unsigned glshim_band_spills(void);
void glshim_band_set_capacity(int records);
void glshim_band_fail_next_open(void);

#ifdef __cplusplus
}
#endif

#endif
