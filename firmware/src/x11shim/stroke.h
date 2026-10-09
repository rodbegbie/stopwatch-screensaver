#ifndef X11SHIM_STROKE_H
#define X11SHIM_STROKE_H

#include "core/canvas.h"
#include "x11shim/xshim.h"

/* Wide lines (line_width above 1). Width 0 and 1 never come here: they take
 * the plain canvas_line path.
 *
 * Geometry. A vertex (x, y) is the pixel x, y. For an odd width the line runs
 * through that pixel's centre; for an even width it runs through the pixel's
 * top left corner, so the extra pixel falls above and to the left, as in Xlib
 * (an even width starts width / 2 pixels before the vertex). A pixel is drawn
 * when its centre is inside the shape, with the left and top edges excluded and
 * the right and bottom edges included, so a butt cap covers the pixels from the
 * first vertex to the last. */

/* Cap argument meaning "this end meets another segment": no cap, no extension. */
#define STROKE_NO_CAP (-1)

/* One segment with the GC's width and cap style on both ends. */
void stroke_segment(Canvas *c, const struct XshimGC *gc, int x1, int y1, int x2,
                    int y2);

/* As stroke_segment, with a cap style for each end. */
void stroke_segment_ends(Canvas *c, const struct XshimGC *gc, int x1, int y1,
                         int x2, int y2, int cap_start, int cap_end);

/* x0,y0,x1,y1,...: n vertices joined with the GC's join style, and capped at
 * the two ends unless closed. Repeated vertices are ignored. */
void stroke_polyline(Canvas *c, const struct XshimGC *gc, const int *xy, int n,
                     int closed);

#endif
