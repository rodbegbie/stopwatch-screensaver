/* The ModeInfo-facing half of the GL layer: init_GL, and the framework hooks
 * xlockmore.h expects of a GL build. Not derived from xscreensaver code. */
#define USE_GL
#include "xlockmore.h"

GLXContext *init_GL(ModeInfo *mi) {
  return glshim_open(MI_DISPLAY(mi)->canvas);
}

/* FPS drawing and visual picking mean nothing on this board. */
void xlockmore_gl_compute_fps(Display *dpy, Window window, fps_state *fpst,
                              void *closure) {
  (void)dpy;
  (void)window;
  (void)fpst;
  (void)closure;
}

void xlockmore_gl_free_fps(fps_state *fpst) { (void)fpst; }

void xlockmore_gl_draw_fps(ModeInfo *mi) { (void)mi; }

void xlockmore_gl_draw_fps_color(ModeInfo *mi, const float color[4]) {
  (void)mi;
  (void)color;
}

Visual *xlockmore_pick_gl_visual(Screen *screen) {
  (void)screen;
  return NULL;
}

Bool xlockmore_validate_gl_visual(Screen *screen, const char *name,
                                  Visual *visual) {
  (void)screen;
  (void)name;
  (void)visual;
  return 1;
}

void xlockmore_reset_gl_state(void) {}

void clear_gl_error(void) {}

void check_gl_error(const char *type) { (void)type; }

/* gltrackball.c asks, under HAVE_MOBILE, how far the device is turned. */
double current_device_rotation(void) { return 0; }
