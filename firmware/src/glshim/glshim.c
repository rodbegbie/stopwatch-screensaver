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
#include <stdlib.h>
#include <string.h>

#include "core/canvas.h"
#include "zbuffer.h"
#include "glshim/glshim_prof.h"

#ifdef GL_PROFILE
uint64_t glshim_prof_time[GLPROF_SLOTS];
uint32_t glshim_prof_calls[GLPROF_SLOTS];
uint32_t glshim_prof_frames;
int glshim_prof_depth;
const char *const glshim_prof_names[GLPROF_SLOTS] = {
    "clear", "list", "vertex", "shade", "tri", "fill", "point", "line"};

void glshim_prof_reset(void) {
  memset(glshim_prof_time, 0, sizeof glshim_prof_time);
  memset(glshim_prof_calls, 0, sizeof glshim_prof_calls);
  glshim_prof_frames = 0;
}
#endif

static ZBuffer *g_zb;
static Canvas *g_canvas;
static int g_scale = 1, g_next_scale = 1; /* see glshim_set_render_scale */
const int16_t *glshim_span_x0, *glshim_span_x1;
static int16_t *g_half_spans; /* owned at scale 2: x0s then x1s */

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
static bool g_z_deferred; /* a depth clear is waiting for the depth test */
static GLint g_deferred_z;

/* The push overlap gives the canvas a second buffer; this is the colour state
 * of the one TinyGL is not drawing into. */
typedef struct {
  PIXEL *px;
  Box color_box;
  bool color_valid;
  PIXEL clear_color;
} BufferState;

static BufferState g_other_buffer;

static bool box_empty(Box b) { return b.x0 > b.x1 || b.y0 > b.y1; }

TGL_HOT static void box_add(Box *b, Box o) {
  if (box_empty(o)) return;
  if (o.x0 < b->x0) b->x0 = o.x0;
  if (o.y0 < b->y0) b->y0 = o.y0;
  if (o.x1 > b->x1) b->x1 = o.x1;
  if (o.y1 > b->y1) b->y1 = o.y1;
}

/* TinyGL's buffer: the canvas itself, or half its size at scale 2. */
static Box whole_canvas(void) {
  return (Box){0, 0, g_zb->xsize - 1, g_zb->ysize - 1};
}

/* The erased box is the whole canvas on a new context: what is on the display
 * is unknown until a full clear and push. */
static void reset_dirty_rectangle(void) {
  g_color_box = g_z_box = kEmptyBox;
  g_color_valid = g_z_valid = false;
  g_z_deferred = false;
  g_swap_drawn = kEmptyBox;
  g_swap_erased = g_canvas ? whole_canvas() : kEmptyBox;
  g_other_buffer = (BufferState){0};
}

/* The display shows the other buffer, whose drawing lies inside its colour box
 * once that buffer has had a clear; until then nothing about it is known. */
static void follow_canvas(void) {
  if (!g_canvas || g_scale != 1) return;
  PIXEL *now = (PIXEL *)g_canvas->px;
  if (g_zb->pbuf == now) return;
  const BufferState left = {g_zb->pbuf, g_color_box, g_color_valid, g_clear_color};
  box_add(&g_swap_erased, left.color_valid ? left.color_box : whole_canvas());
  if (g_other_buffer.px == now) {
    g_color_box = g_other_buffer.color_box;
    g_color_valid = g_other_buffer.color_valid;
    g_clear_color = g_other_buffer.clear_color;
  } else {
    g_color_box = kEmptyBox;
    g_color_valid = false;
  }
  g_other_buffer = left;
  g_zb->pbuf = now;
}

void glshim_canvas_overwritten(void) {
  if (!g_zb) return;
  g_color_valid = false;
  g_other_buffer.color_valid = false;
  box_add(&g_swap_erased, whole_canvas());
}

/* Puts a box in order, clamps it to a w by h canvas and stores it as
 * {x0, y0, x1, y1}; returns 0 if nothing is left. The one rule for what a drawn
 * primitive covers: the dirty rectangle and, through the observer below,
 * tools/gl_budget.py both get its result. */
TGL_HOT static int glshim_clamp_box(GLint x0, GLint y0, GLint x1, GLint y1, int w, int h,
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
TGL_HOT void glshim_note_box(GLint x0, GLint y0, GLint x1, GLint y1) {
  if (!g_zb) return;
  int c[4];
  follow_canvas();
  if (!glshim_clamp_box(x0, y0, x1, y1, g_zb->xsize, g_zb->ysize, c)) return;
  const Box b = {c[0], c[1], c[2], c[3]};
  if (glshim_box_observer) glshim_box_observer(c[0], c[1], c[2], c[3]);
  box_add(&g_color_box, b);
  box_add(&g_z_box, b);
  box_add(&g_swap_drawn, b);
}

/* Narrows [*x0, *x1] on TinyGL buffer row y to what the panel shows. */
static bool visible_part(int y, int *x0, int *x1) {
  if (!glshim_span_x0) return *x0 <= *x1;
  if (*x0 < glshim_span_x0[y]) *x0 = glshim_span_x0[y];
  if (*x1 > glshim_span_x1[y]) *x1 = glshim_span_x1[y];
  return *x0 <= *x1;
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

static unsigned g_depth_clears;

unsigned glshim_depth_clears(void) { return g_depth_clears; }

/* With the depth test off nothing reads the depth buffer, so a clear that asks
 * for it waits until the test is switched on (glshim_depth_test_changed).
 * g_z_box keeps growing meanwhile: the rasteriser still writes depth. */
static void clear_depth(ZBuffer *zb, GLint z) {
  g_depth_clears++;
  const Box area = g_z_valid && z == g_clear_z ? g_z_box : whole_canvas();
  for (int y = area.y0; y <= area.y1; y++) {
    int x0 = area.x0, x1 = area.x1;
    if (visible_part(y, &x0, &x1))
      fill_pixels((uint16_t *)(zb->zbuf + y * zb->xsize) + x0, (uint16_t)z, x1 - x0 + 1);
  }
  g_z_box = kEmptyBox;
  g_z_valid = true;
  g_clear_z = z;
}

void glshim_clear(ZBuffer *zb, GLint clear_z, GLint z, GLint clear_color,
                  GLint r, GLint g, GLint b) {
  if (zb != g_zb) { /* not opened through glshim_open: the stock clear */
    ZB_clear(zb, clear_z, z, clear_color, r, g, b);
    return;
  }
#ifdef GL_PROFILE
  const uint32_t prof_start = glshim_prof_now();
#endif
  follow_canvas();
  const Box all = whole_canvas();
  if (clear_color) {
    const PIXEL color = RGB_TO_PIXEL(r, g, b);
    const Box area = g_color_valid && color == g_clear_color ? g_color_box : all;
    for (int y = area.y0; y <= area.y1; y++) {
      int x0 = area.x0, x1 = area.x1;
      if (visible_part(y, &x0, &x1))
        fill_pixels((uint16_t *)((GLbyte *)zb->pbuf + y * zb->linesize) + x0, color,
                    x1 - x0 + 1);
    }
    box_add(&g_swap_erased, area);
    g_color_box = kEmptyBox;
    g_color_valid = true;
    g_clear_color = color;
  }
  if (clear_z) {
    if (zb->depth_test) {
      g_z_deferred = false;
      clear_depth(zb, z);
    } else {
      g_z_deferred = true;
      g_deferred_z = z;
    }
  }
#ifdef GL_PROFILE
  glshim_prof_add(GLPROF_CLEAR, prof_start);
#endif
}

void glshim_depth_test_changed(ZBuffer *zb) {
  if (zb != g_zb || !zb->depth_test || !g_z_deferred) return;
  g_z_deferred = false;
  clear_depth(zb, g_deferred_z);
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
  g_next_scale = 1; /* a request is for one open, even one that never came */
  if (!g_zb) return;
  glClose();
  ZB_close(g_zb);
  g_zb = NULL;
  g_canvas = NULL;
  g_scale = 1;
  free(g_half_spans);
  g_half_spans = NULL;
  glshim_span_x0 = glshim_span_x1 = NULL;
  g_color_box = g_z_box = g_swap_drawn = g_swap_erased = kEmptyBox;
  g_color_valid = g_z_valid = false;
}

int glshim_is_open(void) { return g_zb != NULL; }

/* The canvas's disc at TinyGL's size. False only if an allocation fails. */
static bool set_spans(const Canvas *canvas) {
  if (!canvas->vis_x0) return true;
  if (g_scale == 1) {
    glshim_span_x0 = canvas->vis_x0;
    glshim_span_x1 = canvas->vis_x1;
    return true;
  }
  const int h = g_zb->ysize;
  g_half_spans = malloc(2 * (size_t)h * sizeof(int16_t));
  if (!g_half_spans) return false;
  int16_t *x0s = g_half_spans, *x1s = g_half_spans + h;
  for (int y = 0; y < h; y++) {
    int lo = INT16_MAX, hi = -1, a0, a1;
    for (int r = 2 * y; r <= 2 * y + 1; r++)
      if (canvas_visible_span(canvas, r, &a0, &a1)) {
        if (a0 < lo) lo = a0;
        if (a1 > hi) hi = a1;
      }
    x0s[y] = (int16_t)(hi < 0 ? 1 : lo / 2);
    x1s[y] = (int16_t)(hi < 0 ? 0 : hi / 2);
  }
  glshim_span_x0 = x0s;
  glshim_span_x1 = x1s;
  return true;
}

GLXContext *glshim_open(Canvas *canvas) {
  static GLXContext handle;

  const int scale = g_next_scale; /* read before the close below drops it */
  glshim_close();
  if (!canvas || !canvas->px || canvas->w <= 0 || canvas->h <= 0) return NULL;
  g_zb = scale == 2 ? ZB_open(canvas->w / 2, canvas->h / 2, ZB_MODE_5R6G5B, NULL)
                    : ZB_open(canvas->w, canvas->h, ZB_MODE_5R6G5B, canvas->px);
  if (!g_zb) return NULL;
  g_scale = scale;
  if (!set_spans(canvas)) {
    ZB_close(g_zb);
    g_zb = NULL;
    g_scale = 1;
    return NULL;
  }
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

/* Scale 2: every pixel of this frame's erased and drawn boxes becomes a 2 by
 * 2 block of the canvas, and the doubled box is marked dirty. */
static void double_into_canvas(void) {
  Box b = g_swap_erased;
  box_add(&b, g_swap_drawn);
  g_swap_drawn = g_swap_erased = kEmptyBox;
#ifdef GL_PROFILE
  glshim_prof_frames++;
#endif
  if (box_empty(b)) return;
  for (int y = b.y0; y <= b.y1; y++) {
    const PIXEL *src = (const PIXEL *)((const GLbyte *)g_zb->pbuf + y * g_zb->linesize);
    uint16_t *top = g_canvas->px + (size_t)(2 * y) * g_canvas->w;
    uint16_t *bottom = top + g_canvas->w;
    int x0 = b.x0, x1 = b.x1;
    if (!visible_part(y, &x0, &x1)) continue;
    for (int x = x0; x <= x1; x++) {
      const uint32_t pair = (uint32_t)src[x] * 0x10001u;
      memcpy(top + 2 * x, &pair, sizeof pair);
      memcpy(bottom + 2 * x, &pair, sizeof pair);
    }
  }
  canvas_mark_dirty(g_canvas, 2 * b.x0, 2 * b.y0, 2 * (b.x1 - b.x0 + 1),
                    2 * (b.y1 - b.y0 + 1));
}

void glXSwapBuffers(Display *dpy, GLXDrawable drawable) {
  (void)dpy;
  (void)drawable;
  if (!g_canvas) return;
  if (g_scale == 2) {
    double_into_canvas();
    return;
  }
  const Box boxes[2] = {g_swap_erased, g_swap_drawn};
  for (int i = 0; i < 2; i++)
    if (!box_empty(boxes[i]))
      canvas_mark_dirty(g_canvas, boxes[i].x0, boxes[i].y0,
                        boxes[i].x1 - boxes[i].x0 + 1,
                        boxes[i].y1 - boxes[i].y0 + 1);
  g_swap_drawn = g_swap_erased = kEmptyBox;
#ifdef GL_PROFILE
  glshim_prof_frames++;
#endif
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

void glshim_InterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer) {
  int c = 0, n = 0;
  switch (format) {
    case GL_V3F: break;
    case GL_C3F_V3F: c = 3; break;
    case GL_N3F_V3F: n = 3; break;
    default:
      g_vertex.on = g_normal.on = g_color.on = 0;
      return;
  }
  const GLsizei bytes = stride ? stride : (GLsizei)((c + n + 3) * sizeof(GLfloat));
  const GLfloat *p = (const GLfloat *)pointer;
  glshim_ColorPointer(3, GL_FLOAT, bytes, p);
  g_color.on = c != 0;
  glshim_NormalPointer(GL_FLOAT, bytes, p + c);
  g_normal.on = n != 0;
  glshim_VertexPointer(3, GL_FLOAT, bytes, p + c + n);
  g_vertex.on = 1;
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

void glDepthFunc(GLint func) { (void)func; }

void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
  glColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

void glTexGeni(GLint coord, GLint pname, GLint param) { (void)coord, (void)pname, (void)param; }

void glTexEnvf(GLint target, GLint pname, GLfloat param) {
  (void)target, (void)pname, (void)param;
}

void glTexParameterf(GLint target, GLint pname, GLfloat param) {
  glTexParameteri(target, pname, (GLint)param);
}

void glPixelStorei(GLint pname, GLint param) {
  (void)pname;
  (void)param;
}

int glIsEnabled(GLint cap) {
  (void)cap;
  return 0;
}

void glLineWidth(GLfloat width) { (void)width; }

void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
             GLdouble znear, GLdouble zfar) {
  const float w = (float)right - (float)left;
  const float h = (float)top - (float)bottom;
  const float d = (float)zfar - (float)znear;
  const GLfloat m[16] = {2.0f / w,
                         0,
                         0,
                         0,
                         0,
                         2.0f / h,
                         0,
                         0,
                         0,
                         0,
                         -2.0f / d,
                         0,
                         -((float)right + (float)left) / w,
                         -((float)top + (float)bottom) / h,
                         -((float)zfar + (float)znear) / d,
                         1.0f};
  glMultMatrixf(m);
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

void glshim_set_render_scale(int scale) { g_next_scale = scale == 2 ? 2 : 1; }

#undef glTexImage2D
void glshim_TexImage2D(GLint target, GLint level, GLint components, GLint width,
                       GLint height, GLint border, GLint format, GLint type,
                       const void *pixels) {
  (void)components;
  int bytes;
  switch (format) {
    case GL_RGB: bytes = 3; break;
    case GL_RGBA: bytes = 4; break;
    case GL_LUMINANCE: bytes = 1; break;
    case GL_LUMINANCE_ALPHA: bytes = 2; break;
    default: return;
  }
  if (type != GL_UNSIGNED_BYTE || width <= 0 || height <= 0 || !pixels) return;
  const size_t n = (size_t)width * (size_t)height;
  GLubyte *rgb = malloc(n * 3);
  if (!rgb) return;
  const GLubyte *in = pixels;
  for (size_t i = 0; i < n; i++, in += bytes) {
    rgb[3 * i + 0] = in[0];
    rgb[3 * i + 1] = bytes >= 3 ? in[1] : in[0];
    rgb[3 * i + 2] = bytes >= 3 ? in[2] : in[0];
  }
  glTexImage2D(target, level, 3, width, height, border, GL_RGB, GL_UNSIGNED_BYTE, rgb);
  free(rgb);
}

#undef glViewport
void glshim_Viewport(GLint x, GLint y, GLint w, GLint h) {
  glViewport(x / g_scale, y / g_scale, w / g_scale, h / g_scale);
}
