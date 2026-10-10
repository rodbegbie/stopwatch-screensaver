/* Our GL context layer for hacks built with USE_GL. TinyGL draws straight
 * into the canvas, so a "swap" only has to mark the whole canvas dirty.
 * Not derived from xscreensaver code. */
#ifdef TGL_SPIKE

#define USE_GL
#include "xlockmore.h"

#include <math.h>

#include "core/canvas.h"
#include "glshim/glshim.h"
#include "zbuffer.h"

#include <stdint.h>
#include <stdio.h>
#ifdef ESP_PLATFORM
#include <esp_timer.h>
void spike_log(const char *msg);
static int64_t now_us(void) { return esp_timer_get_time(); }
#else
#include <time.h>
static void spike_log(const char *msg) { fprintf(stderr, "%s\n", msg); }
static int64_t now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
#endif

#ifndef ESP_PLATFORM
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
/* Host debugging only: say where an abort() came from. */
static void on_abort(int sig) {
  void *frames[24];
  (void)sig;
  backtrace_symbols_fd(frames, backtrace(frames, 24), 2);
  _exit(134);
}
__attribute__((constructor)) static void install_abort_trace(void) {
  signal(SIGABRT, on_abort);
}
#endif

static struct {
  ZBuffer *zb;
  Canvas *canvas;
} g_gl;

/* One context for the whole firmware; a hack that restarts gets a fresh one. */
GLXContext *init_GL(ModeInfo *mi) {
  static GLXContext handle;
  Canvas *canvas = MI_DISPLAY(mi)->canvas;

  if (g_gl.zb) {
    glClose();
    ZB_close(g_gl.zb);
    g_gl.zb = NULL;
  }
  g_gl.zb = ZB_open(canvas->w, canvas->h, ZB_MODE_5R6G5B, canvas->px);
  if (!g_gl.zb) return NULL;
  glInit(g_gl.zb);
  g_gl.canvas = canvas;
#ifdef GL_SPIKE_SEED
  srandom(GL_SPIKE_SEED); /* repeatable scenes for timing comparisons */
#endif
  handle = &g_gl;
  return &handle;
}

Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx) {
  (void)dpy;
  (void)drawable;
  (void)ctx;
  return True;
}

#ifdef GL_SPIKE_QUARTER
#undef glViewport
void glshim_Viewport(GLint x, GLint y, GLint width, GLint height) {
  glViewport(x, y, width / 2, height / 2);
}
#endif

#undef glClear
extern long spike_verts, spike_tris;

static struct {
  int64_t frame_start, window_start, clear_us, gl_us;
  long frames, verts0, tris0;
} g_stat;

void glshim_Clear(GLint mask) {
  int64_t t0 = now_us();
  g_stat.frame_start = t0;
  glClear(mask);
  g_stat.clear_us += now_us() - t0;
}

static void report_frame_stats(int64_t now) {
  char b[200];
  double n = (double)g_stat.frames;
  snprintf(b, sizeof b,
           "GL frames=%ld verts/frame=%.0f tris/frame=%.0f clear=%.1fms "
           "gl=%.1fms",
           g_stat.frames, (spike_verts - g_stat.verts0) / n,
           (spike_tris - g_stat.tris0) / n, g_stat.clear_us / 1000.0 / n,
           g_stat.gl_us / 1000.0 / n);
  spike_log(b);
  g_stat.window_start = now;
  g_stat.frames = 0;
  g_stat.clear_us = g_stat.gl_us = 0;
  g_stat.verts0 = spike_verts;
  g_stat.tris0 = spike_tris;
}

void glXSwapBuffers(Display *dpy, GLXDrawable drawable) {
  int64_t now = now_us();
  (void)dpy;
  (void)drawable;
  canvas_mark_dirty(g_gl.canvas, 0, 0, g_gl.canvas->w, g_gl.canvas->h);
  g_stat.gl_us += now - g_stat.frame_start;
  g_stat.frames++;
  if (!g_stat.window_start) g_stat.window_start = now;
  if (now - g_stat.window_start >= 5000000) report_frame_stats(now);
}

/* The macros in glshim.h rename the hacks' calls; here the real names are
 * needed again for the TinyGL calls this file makes. */
#undef glVertexPointer
#undef glNormalPointer
#undef glColorPointer
#undef glTexCoordPointer
#undef glEnableClientState
#undef glDisableClientState
#undef glDrawArrays

typedef struct {
  const char *data;
  int size;
  int stride; /* bytes between elements */
  int on;
} ClientArray;

static ClientArray g_vertex, g_normal, g_color;

static int stride_bytes(int stride, int size) {
  return stride ? stride : size * (int)sizeof(GLfloat);
}

void glshim_VertexPointer(GLint size, GLenum type, GLsizei stride,
                          const GLvoid *pointer) {
  (void)type;
  g_vertex.data = pointer;
  g_vertex.size = size;
  g_vertex.stride = stride_bytes(stride, size);
}

void glshim_NormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer) {
  (void)type;
  g_normal.data = pointer;
  g_normal.size = 3;
  g_normal.stride = stride_bytes(stride, 3);
}

void glshim_ColorPointer(GLint size, GLenum type, GLsizei stride,
                         const GLvoid *pointer) {
  (void)type;
  g_color.data = pointer;
  g_color.size = size;
  g_color.stride = stride_bytes(stride, size);
}

/* Texture coordinates are not drawn, so the pointer is only accepted. */
void glshim_TexCoordPointer(GLint size, GLenum type, GLsizei stride,
                            const GLvoid *pointer) {
  (void)size;
  (void)type;
  (void)stride;
  (void)pointer;
}

static ClientArray *client_array(GLenum array) {
  switch (array) {
    case GL_VERTEX_ARRAY: return &g_vertex;
    case GL_NORMAL_ARRAY: return &g_normal;
    case GL_COLOR_ARRAY: return &g_color;
    default: return NULL;
  }
}

void glshim_EnableClientState(GLenum array) {
  ClientArray *a = client_array(array);
  if (a) a->on = 1;
}

void glshim_DisableClientState(GLenum array) {
  ClientArray *a = client_array(array);
  if (a) a->on = 0;
}

void glshim_DrawArrays(GLenum mode, GLint first, GLsizei count) {
  glBegin(mode);
  for (GLint i = first; i < first + count; i++) {
    if (g_color.on) {
      const GLfloat *c = (const GLfloat *)(g_color.data + (size_t)i * g_color.stride);
      glColor4f(c[0], c[1], c[2], g_color.size > 3 ? c[3] : 1.0f);
    }
    if (g_normal.on) {
      const GLfloat *n = (const GLfloat *)(g_normal.data + (size_t)i * g_normal.stride);
      glNormal3f(n[0], n[1], n[2]);
    }
    if (g_vertex.on) {
      const GLfloat *v = (const GLfloat *)(g_vertex.data + (size_t)i * g_vertex.stride);
      glVertex3f(v[0], v[1], g_vertex.size > 2 ? v[2] : 0.0f);
    }
  }
  glEnd();
}

void glMateriali(GLint face, GLint pname, GLint value) {
  GLfloat v[4] = {(GLfloat)value, 0, 0, 1};
  glMaterialfv(face, pname, v);
}

/* The only caller asks about GL_TEXTURE_2D, which no hack here enables. */
int glIsEnabled(GLint cap) {
  (void)cap;
  return 0;
}

void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble znear,
                    GLdouble zfar) {
  float top = (float)znear * tanf((float)fovy * 3.14159265f / 360.0f);
  float right = top * (float)aspect;
  glFrustum(-right, right, -top, top, znear, zfar);
}

static void normalize3(float v[3]) {
  float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (len > 0) {
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
  }
}

static void cross3(const float a[3], const float b[3], float out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

void gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez, GLdouble cx,
               GLdouble cy, GLdouble cz, GLdouble upx, GLdouble upy,
               GLdouble upz) {
  float f[3] = {(float)(cx - eyex), (float)(cy - eyey), (float)(cz - eyez)};
  float up[3] = {(float)upx, (float)upy, (float)upz};
  float s[3], u[3];
  normalize3(f);
  cross3(f, up, s);
  normalize3(s);
  cross3(s, f, u);
  GLfloat m[16] = {s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0,
                   s[2], u[2], -f[2], 0, 0,    0,    0,     1};
  glMultMatrixf(m);
  glTranslatef((GLfloat)-eyex, (GLfloat)-eyey, (GLfloat)-eyez);
}

/* The framework's GL hooks. FPS drawing and visual picking have no meaning
 * here. */
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
  return True;
}
/* gltrackball.c asks, under HAVE_MOBILE, how far the device is turned. */
double current_device_rotation(void) { return 0; }

void xlockmore_reset_gl_state(void) {}
void clear_gl_error(void) {}
void check_gl_error(const char *type) { (void)type; }

#endif /* TGL_SPIKE */
