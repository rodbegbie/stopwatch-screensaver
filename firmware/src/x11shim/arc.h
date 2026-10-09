#ifndef X11SHIM_ARC_H
#define X11SHIM_ARC_H

/* Points along an elliptical arc, for drawing it as a polyline or filling it
 * as a pie slice.
 *
 * The ellipse is inscribed in the box x, y, w, h: centre (x + w / 2,
 * y + h / 2), radii w / 2 and h / 2. Angles are in 64ths of a degree, from
 * three o'clock, positive counter-clockwise (so y decreases going up), as in
 * Xlib. A sweep (angle2) of 360 degrees or more in either direction is a full
 * ellipse, which comes back to its first point. Coordinates are rounded to
 * ints, and a point's meaning is the caller's: as a pixel (outlines) or as a
 * corner of the pixel grid (fills). */

/* How many points arc_points gives: 0 for no sweep, otherwise at least 2 and
 * at most ARC_MAX_POINTS. */
#define ARC_MAX_POINTS 1024
int arc_point_count(unsigned w, unsigned h, int angle2);

/* Writes up to max_points x,y pairs into xy and returns how many it wrote:
 * arc_point_count(w, h, angle2) when max_points allows. */
int arc_points(int x, int y, unsigned w, unsigned h, int angle1, int angle2,
               int *xy, int max_points);

#endif
