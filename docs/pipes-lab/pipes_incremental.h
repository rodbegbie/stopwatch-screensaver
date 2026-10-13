/* Prototype for issue #44: an exact incremental redraw for Pipes.
 *
 * Pipes clears and replays every piece's display list each frame, in order,
 * under one camera matrix, and adds one piece a frame. When the clear, the
 * camera and the first lists are the same as last frame, the canvas and the
 * z-buffer already hold the result of replaying last frame's lists, so this
 * skips the clear and those lists and replays only the new ones. Rendering is
 * deterministic, so the frame is the same as a full redraw, pixel for pixel.
 * Anything else (a new camera, a list out of order, a clear with nothing
 * drawn) falls back to a real clear and a full replay.
 *
 * Included before pipes.c, so the hack's own calls go through these. */
#include <string.h>

#include "GL/gl.h"
#include "glshim/glshim.h"

#define PIPES_MAX_CALLS 4096

static struct {
  int compiling;   /* inside glNewList: calls are recorded, not drawn */
  GLint pending; /* a clear asked for and not done yet */
  int valid;       /* last frame's record can be continued */
  GLint mask; /* last frame's clear */
  GLfloat model[16], proj[16];
  int n, prev_n;   /* lists called this frame, and last frame */
  int skip;        /* calls this frame that last frame already drew */
  GLuint ids[PIPES_MAX_CALLS];
  long drawn, skipped; /* totals, for the lab */
} pi;

static void pipes_replay_skipped(void) {
  for (int i = 0; i < pi.n; i++) glCallList(pi.ids[i]);
  pi.skip = 0;
}

static void pipes_real_clear(GLint mask) { glClear(mask); }

static void pipes_clear(GLint mask) {
  if (pi.compiling) {
    glClear(mask);
    return;
  }
  if (pi.pending) pipes_real_clear(pi.pending); /* two clears, nothing between */
  pi.pending = mask;
  pi.n = 0;
  pi.skip = 0;
}

static void pipes_call_list(GLuint list) {
  if (pi.compiling) {
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
      pipes_real_clear(pi.pending);
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
    pipes_real_clear(pi.mask);
    pipes_replay_skipped();
  }
  if (pi.n < PIPES_MAX_CALLS) pi.ids[pi.n] = list;
  else pi.valid = 0;
  pi.n++;
  pi.drawn++;
  glCallList(list);
}

static void pipes_swap(Display *dpy, GLXDrawable w) {
  if (pi.pending) { /* a clear with no lists after it */
    pipes_real_clear(pi.pending);
    pi.pending = 0;
    pi.n = 0;
  }
  pi.valid = pi.n <= PIPES_MAX_CALLS;
  pi.prev_n = pi.n;
  glXSwapBuffers(dpy, w);
}

static void pipes_new_list(GLuint list, GLenum mode) {
  pi.compiling = 1;
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
