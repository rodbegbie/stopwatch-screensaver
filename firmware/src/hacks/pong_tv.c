/* Builds the unmodified pong.c and analogtv.c as one translation unit. analogtv.c
 * is built from the build directory's copy, with the ghost filter in float
 * (tools/analogtv_patch.py): its taps are doubles, which made the signal passes
 * 209 of the 356 ms step.
 *
 * analogtv.c does not include screenhack.h (it takes `frand` from there), and
 * packs its pixels from the visual's masks as ordinary RGB565. The canvas holds
 * each pixel's bytes swapped, so the colour tables it fills in analogtv_allocate
 * are swapped once here: an OR of swapped values is the swap of the OR, so every
 * pixel it writes afterwards lands in canvas order with no work per pixel. */
#include "screenhack.h"

#include "core/canvas.h"

#define analogtv_allocate analogtv_allocate_upstream
#include "analogtv_patched.c"
#undef analogtv_allocate

analogtv *analogtv_allocate(Display *dpy, Window window);

analogtv *analogtv_allocate(Display *dpy, Window window) {
  analogtv *it = analogtv_allocate_upstream(dpy, window);
  if (it && !it->use_cmap) {
    for (int i = 0; i < ANALOGTV_CV_MAX; i++) {
      it->red_values[i] = px_swap((uint16_t)it->red_values[i]);
      it->green_values[i] = px_swap((uint16_t)it->green_values[i]);
      it->blue_values[i] = px_swap((uint16_t)it->blue_values[i]);
    }
  }
  return it;
}

#include "pong/pong.c"
