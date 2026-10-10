/* The GL context layer. TinyGL draws straight into the canvas, so a "swap"
 * only has to mark the canvas dirty. Not derived from xscreensaver code. */
#include "glshim/glshim.h"

#include <math.h>

#include "core/canvas.h"
#include "zbuffer.h"

static ZBuffer *g_zb;
static Canvas *g_canvas;

static void release_hook(Display *dpy) {
  (void)dpy;
  glshim_close();
}

void glshim_close(void) {
  if (!g_zb) return;
  glClose();
  ZB_close(g_zb);
  g_zb = NULL;
  g_canvas = NULL;
}

GLXContext *glshim_open(Canvas *canvas) {
  static GLXContext handle;

  glshim_close();
  if (!canvas || !canvas->px || canvas->w <= 0 || canvas->h <= 0) return NULL;
  g_zb = ZB_open(canvas->w, canvas->h, ZB_MODE_5R6G5B, canvas->px);
  if (!g_zb) return NULL;
  glInit(g_zb);
  g_canvas = canvas;
  xshim_set_release_hook(release_hook);
  handle = g_zb;
  return &handle;
}

Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx) {
  (void)dpy;
  (void)drawable;
  (void)ctx;
  return 1;
}

void glXSwapBuffers(Display *dpy, GLXDrawable drawable) {
  (void)dpy;
  (void)drawable;
  if (g_canvas) canvas_mark_dirty(g_canvas, 0, 0, g_canvas->w, g_canvas->h);
}

void glMateriali(GLint face, GLint pname, GLint value) {
  GLfloat v[4] = {(GLfloat)value, 0, 0, 1};
  glMaterialfv(face, pname, v);
}

int glIsEnabled(GLint cap) {
  (void)cap;
  return 0;
}

void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble znear,
                    GLdouble zfar) {
  const float top = (float)znear * tanf((float)fovy * 3.14159265f / 360.0f);
  const float right = top * (float)aspect;
  glFrustum(-right, right, -top, top, znear, zfar);
}

static void normalize3(float v[3]) {
  const float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
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
  const float up[3] = {(float)upx, (float)upy, (float)upz};
  float s[3], u[3];
  normalize3(f);
  cross3(f, up, s);
  normalize3(s);
  cross3(s, f, u);
  /* Column-major, as glMultMatrixf takes it. */
  GLfloat m[16] = {s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0,
                   s[2], u[2], -f[2], 0, 0,    0,    0,     1};
  glMultMatrixf(m);
  glTranslatef((GLfloat)-eyex, (GLfloat)-eyey, (GLfloat)-eyez);
}
