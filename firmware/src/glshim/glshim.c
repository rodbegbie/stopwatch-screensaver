/* The GL context layer. TinyGL draws straight into the canvas, so a "swap"
 * only has to mark the canvas dirty. Not derived from xscreensaver code.
 *
 * Dirty rectangle: TinyGL reports the box around everything it draws
 * (glshim_note_box, called from the patched clip.c). glClear then clears only
 * the box drawn since the last clear, and a swap marks only the box drawn
 * this frame plus the box the clear erased, because the rest of the canvas is
 * already the clear colour and already on the display. That holds only while
 * nothing else draws on the canvas, and only after a first full clear of a
 * known colour, so a new context starts with neither assumed. */
#include "glshim/glshim.h"

#include <math.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/canvas.h"
#include "zbuffer.h"

static ZBuffer *g_zb;
static Canvas *g_canvas;

typedef struct {
  int x0, y0, x1, y1; /* inclusive; empty when x0 > x1 */
} Box;

static const Box kEmptyBox = {1 << 30, 1 << 30, -1, -1};

static Box g_color_box;  /* drawn since the last colour clear */
static Box g_z_box;      /* drawn since the last depth clear */
static Box g_swap_drawn; /* drawn since the last swap */
static Box g_swap_erased; /* cleared since the last swap */
static bool g_color_valid;
static bool g_z_valid;
static PIXEL g_clear_color;
static GLint g_clear_z;

static bool box_empty(Box b) { return b.x0 > b.x1 || b.y0 > b.y1; }

static void box_add(Box *b, Box o) {
  if (box_empty(o)) return;
  if (o.x0 < b->x0) b->x0 = o.x0;
  if (o.y0 < b->y0) b->y0 = o.y0;
  if (o.x1 > b->x1) b->x1 = o.x1;
  if (o.y1 > b->y1) b->y1 = o.y1;
}

static Box whole_canvas(void) {
  return (Box){0, 0, g_canvas->w - 1, g_canvas->h - 1};
}

/* The erased box is the whole canvas on a new context: what is on the display
 * is unknown until a full clear and push. */
static void reset_dirty_rectangle(void) {
  g_color_box = g_z_box = kEmptyBox;
  g_color_valid = g_z_valid = false;
  g_swap_drawn = kEmptyBox;
  g_swap_erased = g_canvas ? whole_canvas() : kEmptyBox;
}

/* Puts a box in order, clamps it to a w by h canvas and stores it as
 * {x0, y0, x1, y1}; returns 0 if nothing is left. The one rule for what a drawn
 * primitive covers: the dirty rectangle and, through the observer below,
 * tools/gl_budget.py both get its result. */
static int glshim_clamp_box(GLint x0, GLint y0, GLint x1, GLint y1, int w, int h,
                            int out[4]) {
  if (x0 > x1) {
    const GLint t = x0;
    x0 = x1;
    x1 = t;
  }
  if (y0 > y1) {
    const GLint t = y0;
    y0 = y1;
    y1 = t;
  }
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > w - 1) x1 = w - 1;
  if (y1 > h - 1) y1 = h - 1;
  if (x0 > x1 || y0 > y1) return 0;
  out[0] = x0;
  out[1] = y0;
  out[2] = x1;
  out[3] = y1;
  return 1;
}

void (*glshim_box_observer)(int x0, int y0, int x1, int y1);

/* Called by TinyGL for everything it draws, in canvas pixels. */
void glshim_note_box(GLint x0, GLint y0, GLint x1, GLint y1) {
  if (!g_zb) return;
  int c[4];
  if (!glshim_clamp_box(x0, y0, x1, y1, g_canvas->w, g_canvas->h, c)) return;
  const Box b = {c[0], c[1], c[2], c[3]};
  if (glshim_box_observer) glshim_box_observer(c[0], c[1], c[2], c[3]);
  box_add(&g_color_box, b);
  box_add(&g_z_box, b);
  box_add(&g_swap_drawn, b);
}

/* Two pixels at a time. `p` is 16-bit and not always 4-byte aligned, so one
 * pixel is written first if needed; memcpy then writes each pair without
 * reading the memory through a differently typed pointer (strict aliasing),
 * and the alignment hint lets it compile to one 32-bit store. */
static void fill_pixels(uint16_t *p, uint16_t v, int n) {
  if (n > 0 && ((uintptr_t)p & 2)) {
    *p++ = v;
    n--;
  }
  const uint32_t pair = ((uint32_t)v << 16) | v;
  uint16_t *a = __builtin_assume_aligned(p, 4);
  for (; n >= 2; n -= 2, a += 2) memcpy(a, &pair, sizeof pair);
  if (n) *a = v;
}

void glshim_clear(ZBuffer *zb, GLint clear_z, GLint z, GLint clear_color,
                  GLint r, GLint g, GLint b) {
  if (zb != g_zb) { /* not opened through glshim_open: the stock clear */
    ZB_clear(zb, clear_z, z, clear_color, r, g, b);
    return;
  }
  const Box all = whole_canvas();
  if (clear_color) {
    const PIXEL color = RGB_TO_PIXEL(r, g, b);
    const Box area = g_color_valid && color == g_clear_color ? g_color_box : all;
    for (int y = area.y0; y <= area.y1; y++)
      fill_pixels((uint16_t *)((GLbyte *)zb->pbuf + y * zb->linesize) + area.x0,
                  color, area.x1 - area.x0 + 1);
    box_add(&g_swap_erased, area);
    g_color_box = kEmptyBox;
    g_color_valid = true;
    g_clear_color = color;
  }
  if (clear_z) {
    const Box area = g_z_valid && z == g_clear_z ? g_z_box : all;
    for (int y = area.y0; y <= area.y1; y++)
      fill_pixels((uint16_t *)(zb->zbuf + y * zb->xsize) + area.x0, (uint16_t)z,
                  area.x1 - area.x0 + 1);
    g_z_box = kEmptyBox;
    g_z_valid = true;
    g_clear_z = z;
  }
}

typedef struct {
  const char *data;
  int size;
  int stride; /* bytes between elements */
  int on;
} ClientArray;

static ClientArray g_vertex, g_normal, g_color;

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
  g_color_box = g_z_box = g_swap_drawn = g_swap_erased = kEmptyBox;
  g_color_valid = g_z_valid = false;
}

int glshim_is_open(void) { return g_zb != NULL; }

GLXContext *glshim_open(Canvas *canvas) {
  static GLXContext handle;

  glshim_close();
  if (!canvas || !canvas->px || canvas->w <= 0 || canvas->h <= 0) return NULL;
  g_zb = ZB_open(canvas->w, canvas->h, ZB_MODE_5R6G5B, canvas->px);
  if (!g_zb) return NULL;
  glInit(g_zb);
  g_vertex = g_normal = g_color = (ClientArray){0};
  g_canvas = canvas;
  reset_dirty_rectangle();
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
  if (!g_canvas) return;
  const Box boxes[2] = {g_swap_erased, g_swap_drawn};
  for (int i = 0; i < 2; i++)
    if (!box_empty(boxes[i]))
      canvas_mark_dirty(g_canvas, boxes[i].x0, boxes[i].y0,
                        boxes[i].x1 - boxes[i].x0 + 1,
                        boxes[i].y1 - boxes[i].y0 + 1);
  g_swap_drawn = g_swap_erased = kEmptyBox;
}

static int stride_in_bytes(int stride, int size) {
  return stride ? stride : size * (int)sizeof(GLfloat);
}

void glshim_VertexPointer(GLint size, GLenum type, GLsizei stride,
                          const GLvoid *pointer) {
  (void)type;
  g_vertex.data = pointer;
  g_vertex.size = size;
  g_vertex.stride = stride_in_bytes(stride, size);
}

void glshim_NormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer) {
  (void)type;
  g_normal.data = pointer;
  g_normal.size = 3;
  g_normal.stride = stride_in_bytes(stride, 3);
}

void glshim_ColorPointer(GLint size, GLenum type, GLsizei stride,
                         const GLvoid *pointer) {
  (void)type;
  g_color.data = pointer;
  g_color.size = size;
  g_color.stride = stride_in_bytes(stride, size);
}

void glshim_TexCoordPointer(GLint size, GLenum type, GLsizei stride,
                            const GLvoid *pointer) {
  (void)size;
  (void)type;
  (void)stride;
  (void)pointer;
}

static ClientArray *client_array(GLenum array) {
  switch (array) {
    case GL_VERTEX_ARRAY:
      return &g_vertex;
    case GL_NORMAL_ARRAY:
      return &g_normal;
    case GL_COLOR_ARRAY:
      return &g_color;
    default:
      return NULL;
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

static const GLfloat *element(const ClientArray *a, GLint i) {
  return (const GLfloat *)(a->data + (size_t)i * (size_t)a->stride);
}

void glshim_DrawArrays(GLenum mode, GLint first, GLsizei count) {
  glBegin(mode);
  for (GLint i = first; i < first + count; i++) {
    if (g_color.on) {
      const GLfloat *c = element(&g_color, i);
      glColor4f(c[0], c[1], c[2], g_color.size > 3 ? c[3] : 1.0f);
    }
    if (g_normal.on) {
      const GLfloat *n = element(&g_normal, i);
      glNormal3f(n[0], n[1], n[2]);
    }
    if (g_vertex.on) {
      const GLfloat *v = element(&g_vertex, i);
      glVertex3f(v[0], v[1], g_vertex.size > 2 ? v[2] : 0.0f);
    }
  }
  glEnd();
}

void glMateriali(GLint face, GLint pname, GLint value) {
  GLfloat v[4] = {(GLfloat)value, 0, 0, 1};
  glMaterialfv(face, pname, v);
}

void glPixelStorei(GLint pname, GLint param) {
  (void)pname;
  (void)param;
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
