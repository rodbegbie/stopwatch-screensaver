#ifndef X11SHIM_STROKE_H
#define X11SHIM_STROKE_H

#include "core/canvas.h"
#include "x11shim/xshim.h"

/* Wide lines (line_width above 1). Width 0 and 1 never come here: they take
 * the plain canvas_line path. */

/* One segment with the GC's width and cap style on both ends. */
void stroke_segment(Canvas *c, const struct XshimGC *gc, int x1, int y1, int x2,
                    int y2);

/* As stroke_segment, with a cap style for each end. A polyline passes CapButt
 * for the ends that meet another segment. */
void stroke_segment_ends(Canvas *c, const struct XshimGC *gc, int x1, int y1,
                         int x2, int y2, int cap_start, int cap_end);

#endif
