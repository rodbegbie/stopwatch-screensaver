/* Builds the unmodified unknownpleasures.c with USE_GL set for this file only,
 * so no other hack sees a GL build. See hacks/gears_gl.c.
 *
 * Upstream adds a row at the end of a draw, once a frame finds the interval
 * over, and restarts the clock at that frame's time. That frame was drawn at
 * the scroll's clamp, so the time past the interval never scrolls and the
 * picture stands still for a push (issue #134). Here the row is added before
 * the draw and the clock moves on by exactly one interval, so the surplus
 * scrolls the next frame. */
#define USE_GL

/* The hack registers itself at its end, with its own draw. Its two tables take
 * other names here, and the tables below take the registry's. */
#define draw_unk draw_unk_upstream
#define unknownpleasures_xlockmore_function_table up_upstream_xlockmore_table
#define unknownpleasures_xscreensaver_function_table \
  up_upstream_xscreensaver_table
#include "unknownpleasures/unknownpleasures.c"
#undef draw_unk
#undef unknownpleasures_xlockmore_function_table
#undef unknownpleasures_xscreensaver_function_table

ENTRYPOINT void
draw_unk (ModeInfo *mi)
{
  unk_configuration *bp = &bps[MI_SCREEN(mi)];
  double interval = (0.6 / bp->speed) * (80.0 / bp->count);

  if (bp->glx_context && !bp->button_down_p &&
      bp->tick + interval <= double_time ())
    {
      glXMakeCurrent (MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
      tick_unk (mi);
      bp->tick += interval;
    }
  draw_unk_upstream (mi);
}

XSCREENSAVER_MODULE_2 ("UnknownPleasures", unknownpleasures, unk)
