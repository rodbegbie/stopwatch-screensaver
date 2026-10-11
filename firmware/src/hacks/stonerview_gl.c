/* Builds the unmodified stonerview sources as one translation unit with
 * USE_GL set for this file only, so no other hack sees a GL build. Upstream
 * builds them as four files; they share no static names. HAVE_GL stays
 * undefined: see hacks/gears_gl.c.
 *
 * Upstream never frees its list of oscillators ("how do we free this?"): about
 * 45 small allocations lost each time the hack stops. The list is linked
 * through `next`, so the wrapper frees it before upstream's release runs. */
#define USE_GL
#include "stonerview/stonerview.c"
#include "stonerview/stonerview-osc.c"
#include "stonerview/stonerview-move.c"

#define stonerview_win_release stonerview_win_release_upstream
#include "stonerview/stonerview-view.c"
#undef stonerview_win_release

void stonerview_win_release(stonerview_state *st) {
  for (osc_t *osc = st->oscroot; osc;) {
    osc_t *next = osc->next;
    free(osc);
    osc = next;
  }
  st->oscroot = NULL;
  stonerview_win_release_upstream(st);
}
