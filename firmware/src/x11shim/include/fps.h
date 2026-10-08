/* Stand-in for xscreensaver's utils/fps.h. Not derived from xscreensaver
 * code. The device has no FPS overlay, so these do nothing. */
#ifndef XSHIM_FPS_H
#define XSHIM_FPS_H

typedef struct fps_state fps_state;

void fps_compute(fps_state *, unsigned long polys, double depth);
void fps_draw(fps_state *);
void fps_free(fps_state *);

#endif
