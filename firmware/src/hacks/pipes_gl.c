/* Builds the unmodified pipes.c with USE_GL set for this file only (see
 * hacks/gears_gl.c), with two changes made from outside (issue #44).
 *
 * An exact incremental redraw. Pipes clears and replays every piece's display
 * list each frame, in order, under one camera, and adds one piece a frame. With
 * a fixed camera (`*rotatepipes: False`, registered) the canvas and z-buffer
 * already hold last frame's lists, so when the clear, both matrices and the
 * lists so far match last frame, the clear and those lists are skipped and
 * only the new ones are replayed. Anything else is a real clear and a full
 * replay. Compiling any list starts over, since list names are reused.
 *
 * No fade. A screen ends with 25 frames that shrink the whole picture, each a
 * full redraw; here the next screen is generated as soon as the fade would
 * start. */
#define USE_GL
#include <string.h>

#include "screenhackI.h"
#include "GL/gl.h"
#include "glshim/glshim.h"

#define PIPES_MAX_CALLS 4096

static struct {
  int compiling; /* inside glNewList: calls are recorded, not drawn */
  GLint pending; /* a clear asked for and not done yet */
  int valid;     /* last frame's record can be continued */
  GLint mask;    /* last frame's clear */
  GLfloat model[16], proj[16];
  int n, prev_n; /* lists called this frame, and last frame */
  int skip;      /* calls this frame that last frame already drew */
  GLuint ids[PIPES_MAX_CALLS];
  long skipped;
} pi;

int pipes_full_redraw = 0;

long pipes_lists_skipped(void) { return pi.skipped; }

static void pipes_replay_skipped(void) {
  for (int i = 0; i < pi.n; i++) glCallList(pi.ids[i]);
}

static void pipes_clear(GLint mask) {
  if (pi.compiling || pipes_full_redraw) {
    glClear(mask);
    return;
  }
  if (pi.pending) glClear(pi.pending); /* two clears, nothing between */
  pi.pending = mask;
  pi.n = 0;
  pi.skip = 0;
}

static void pipes_call_list(GLuint list) {
  if (pi.compiling || pipes_full_redraw) {
    glCallList(list);
    return;
  }
  if (pi.pending) { /* the first list after a clear decides */
    GLfloat model[16], proj[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, model);
    glGetFloatv(GL_PROJECTION_MATRIX, proj);
    const int same = pi.valid && pi.pending == pi.mask && pi.prev_n > 0 &&
                     memcmp(model, pi.model, sizeof model) == 0 &&
                     memcmp(proj, pi.proj, sizeof proj) == 0;
    if (same) {
      pi.skip = pi.prev_n;
    } else {
      glClear(pi.pending);
      pi.skip = 0;
    }
    pi.mask = pi.pending;
    memcpy(pi.model, model, sizeof model);
    memcpy(pi.proj, proj, sizeof proj);
    pi.pending = 0;
  }
  if (pi.n < pi.skip) {
    if (list == pi.ids[pi.n]) {
      pi.n++;
      pi.skipped++;
      return;
    }
    /* Not last frame's sequence after all: clear, and draw what was skipped. */
    glClear(pi.mask);
    pipes_replay_skipped();
    pi.skip = 0;
  }
  if (pi.n < PIPES_MAX_CALLS) pi.ids[pi.n] = list;
  pi.n++;
  glCallList(list);
}

static void pipes_swap(Display *dpy, GLXDrawable w) {
  if (pi.pending) { /* a clear with no lists after it */
    glClear(pi.pending);
    pi.pending = 0;
    pi.n = 0;
  }
  pi.valid = pi.n <= PIPES_MAX_CALLS;
  pi.prev_n = pi.n;
  glXSwapBuffers(dpy, w);
}

static void pipes_new_list(GLuint list, GLenum mode) {
  pi.compiling = 1;
  pi.valid = 0;
  glNewList(list, mode);
}

static void pipes_end_list(void) {
  glEndList();
  pi.compiling = 0;
}

#define glClear pipes_clear
#define glCallList pipes_call_list
#define glXSwapBuffers pipes_swap
#define glNewList pipes_new_list
#define glEndList pipes_end_list

/* pipes.c registers itself at its end, under these names; this file
 * registers the wrapped functions under the usual ones. */
#define pipes_xlockmore_function_table pipes_upstream_xlockmore_table
#define pipes_xscreensaver_function_table pipes_upstream_xscreensaver_table

#include "pipes/pipes.c"

#undef pipes_xlockmore_function_table
#undef pipes_xscreensaver_function_table

#undef glClear
#undef glCallList
#undef glXSwapBuffers
#undef glNewList
#undef glEndList

static void pipes_init_fresh(ModeInfo *mi) {
  memset(&pi, 0, sizeof pi);
  init_pipes(mi);
}

/* Upstream regenerates inside a draw, before its clear: doing it after the
 * swap would let generate_system's clear erase the frame just shown. */
static void pipes_draw_no_fade(ModeInfo *mi) {
  pipesstruct *pp = &pipes[MI_SCREEN(mi)];
  if (pp->fadeout == 100) {
    pp->fadeout = 0;
    generate_system(mi);
  }
  draw_pipes(mi);
}

#define init_pipes pipes_init_fresh
#define draw_pipes pipes_draw_no_fade
XSCREENSAVER_MODULE_2("Pipes", pipes, pipes)
