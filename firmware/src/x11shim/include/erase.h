/* Shim replacement for xscreensaver's utils/erase.h. */
#ifndef XSHIM_ERASE_H
#define XSHIM_ERASE_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct eraser_state eraser_state;

void eraser_free(eraser_state *st);
/* Clears the window; always returns NULL (nothing left to animate). */
eraser_state *erase_window(Display *, Window, eraser_state *);

#ifdef __cplusplus
}
#endif

#endif
