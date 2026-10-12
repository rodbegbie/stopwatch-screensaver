/* Shim replacement for xscreensaver's utils/erase.h. */
#ifndef XSHIM_ERASE_H
#define XSHIM_ERASE_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct eraser_state eraser_state;

void eraser_free(eraser_state *st);
/* Like upstream's: the first call (st NULL) draws nothing and returns a state;
 * the second starts a one-second wipe; each later call draws the next step;
 * the call after the second is up clears the window and returns NULL. */
eraser_state *erase_window(Display *, Window, eraser_state *);

#ifdef __cplusplus
}
#endif

#endif
