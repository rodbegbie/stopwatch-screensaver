/* THROWAWAY SPIKE, not for merging. Question: can TinyGL on the ESP32-S3
 * redraw a Pipes-sized scene fast enough? Pipes clears and redraws every
 * piece so far each frame (up to 500 per system), so this does the same:
 * glClear, one glCallList per piece, then a full-frame copy to the canvas.
 * Compiled only with -DTGL_SPIKE. */
#ifdef TGL_SPIKE

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#include <esp_timer.h>
#endif

#include "GL/gl.h"
#include "core/canvas.h"
#include "runner/hack_entry.h"
#include "zbuffer.h"

void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glNormal3f(GLfloat x, GLfloat y, GLfloat z);

#ifndef TGL_SPIKE_RES
#define TGL_SPIKE_RES 464 /* TinyGL wants a multiple of 4 */
#endif
#ifndef SPIKE_WIRE
#define SPIKE_WIRE 0
#endif
#ifndef SPIKE_SIDES
#define SPIKE_SIDES (SPIKE_WIRE ? 5 : 24) /* Pipes' own facet counts */
#endif
#ifndef SPIKE_SPHERE
#define SPIKE_SPHERE 16
#endif
#define MAX_PIECES 500
#define HOLD_FRAMES 30
#define WINDOW_US 5000000
#define PI 3.14159265f

#ifdef ESP_PLATFORM
void spike_log(const char *msg);
static int64_t now_us(void) { return esp_timer_get_time(); }
#else
static void spike_log(const char *msg) { fprintf(stderr, "%s\n", msg); }
static int64_t now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
#endif

typedef struct {
  ZBuffer *zb;
  uint16_t *fb;
  int res;
  GLuint cyl, sph, pieces;
  int idx, hold;
  float angle;
  int64_t win_start, raster_us, copy_us;
  long frames;
  int first_idx;
  char bench[320];
} Spike;

static void *big_alloc(size_t n) {
#ifdef ESP_PLATFORM
  return heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
#else
  return malloc(n);
#endif
}

/* As Pipes' MakeTube: a quad strip (a line strip in wireframe). */
static void build_cylinder(GLuint list, int sides, float radius) {
  glNewList(list, GL_COMPILE);
  glBegin(SPIKE_WIRE ? GL_LINE_STRIP : GL_QUAD_STRIP);
  for (int i = 0; i <= sides; i++) {
    float a = 2 * PI * i / sides, c = cosf(a), s = sinf(a);
    glNormal3f(c, s, 0);
    glVertex3f(radius * c, radius * s, 1);
    glVertex3f(radius * c, radius * s, 0);
  }
  glEnd();
  glEndList();
}

/* As xscreensaver's unit_sphere: per stack, one triangle strip (a line strip
 * of four vertices per slice in wireframe) with the same vertex order. */
static void build_sphere(GLuint list, int stacks, int slices, float radius) {
  float la[3] = {0, -radius, 0}, lb[3] = {0, -radius, 0};
  glNewList(list, GL_COMPILE);
  for (int j = 0; j < stacks; j++) {
    float t1 = j * 2 * PI / (stacks * 2) - PI / 2;
    float t2 = (j + 1) * 2 * PI / (stacks * 2) - PI / 2;
    glBegin(SPIKE_WIRE ? GL_LINE_STRIP : GL_TRIANGLE_STRIP);
    for (int i = slices; i >= 0; i--) {
      float t3 = i * 2 * PI / slices;
      float n2[3] = {cosf(t2) * cosf(t3), sinf(t2), cosf(t2) * sinf(t3)};
      float n1[3] = {cosf(t1) * cosf(t3), sinf(t1), cosf(t1) * sinf(t3)};
      if (SPIKE_WIRE) {
        glVertex3f(lb[0], lb[1], lb[2]);
        glVertex3f(la[0], la[1], la[2]);
      }
      glNormal3f(n2[0], n2[1], n2[2]);
      glVertex3f(radius * n2[0], radius * n2[1], radius * n2[2]);
      for (int k = 0; k < 3 && SPIKE_WIRE; k++) la[k] = radius * n2[k];
      glNormal3f(n1[0], n1[1], n1[2]);
      glVertex3f(radius * n1[0], radius * n1[1], radius * n1[2]);
      for (int k = 0; k < 3 && SPIKE_WIRE; k++) lb[k] = radius * n1[k];
    }
    glEnd();
  }
  glEndList();
}

/* Directions: +x -x +y -y +z -z. The cylinder is built along +z. */
static const int kDir[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                               {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
static const float kRot[6][4] = {{90, 0, 1, 0}, {-90, 0, 1, 0}, {-90, 1, 0, 0},
                                 {90, 1, 0, 0}, {0, 0, 0, 1},   {180, 1, 0, 0}};
static const GLfloat kColors[6][4] = {{0.9f, 0.1f, 0.1f, 1}, {0.1f, 0.8f, 0.1f, 1},
                                      {0.2f, 0.3f, 0.9f, 1}, {0.9f, 0.8f, 0.1f, 1},
                                      {0.1f, 0.8f, 0.8f, 1}, {0.8f, 0.2f, 0.8f, 1}};

/* One display list per piece, with its transforms baked in, as Pipes does. */
static void build_pieces(Spike *s) {
  int pos[3] = {0, 0, 0}, last = -1;
  srandom(1);
  for (int k = 0; k < MAX_PIECES; k++) {
    int d, tries = 0;
    do {
      d = random() % 6;
      tries++;
    } while (tries < 20 && ((last >= 0 && d == (last ^ 1)) ||
                            abs(pos[0] + kDir[d][0]) > 4 ||
                            abs(pos[1] + kDir[d][1]) > 4 ||
                            abs(pos[2] + kDir[d][2]) > 4));
    int elbow = (d != last);
    glNewList(s->pieces + k, GL_COMPILE);
    glPushMatrix();
    glTranslatef(pos[0], pos[1], pos[2]);
    if (SPIKE_WIRE)
      glColor3f(kColors[(k / 40) % 6][0], kColors[(k / 40) % 6][1],
                kColors[(k / 40) % 6][2]);
    else
      glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                   (GLfloat *)kColors[(k / 40) % 6]);
    glPushMatrix();
    glRotatef(kRot[d][0], kRot[d][1], kRot[d][2], kRot[d][3]);
    glCallList(s->cyl);
    glPopMatrix();
    if (elbow) {
      glTranslatef(kDir[d][0], kDir[d][1], kDir[d][2]);
      glCallList(s->sph);
    }
    glPopMatrix();
    glEndList();
    for (int a = 0; a < 3; a++) pos[a] += kDir[d][a];
    last = d;
  }
}

static void setup_camera(void) {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-0.6, 0.6, -0.6, 0.6, 2.0, 60.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void blit(const Spike *s, Canvas *c) {
  int ox = (c->w - s->res) / 2, oy = (c->h - s->res) / 2;
  for (int y = 0; y < s->res; y++) {
    const uint16_t *src = s->fb + (size_t)y * s->res;
    uint16_t *dst = c->px + (size_t)(y + oy) * c->w + ox;
    for (int x = 0; x < s->res; x++) dst[x] = (uint16_t)((src[x] >> 8) | (src[x] << 8));
  }
  canvas_mark_dirty(c, ox, oy, s->res, s->res);
}

static void clear_gl(void) { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }

static long count_lit(const Spike *s) {
  long n = 0;
  for (long i = 0; i < (long)s->res * s->res; i++) n += s->fb[i] != 0;
  return n;
}

static void bench(Spike *s, Canvas *c) {
  char b[300];
  int64_t t;
  const int nsph = 100, ncyl = 200;

  t = now_us();
  for (int i = 0; i < 20; i++) clear_gl();
  double clear_ms = (now_us() - t) / 20000.0;

  setup_camera();
  srandom(7);
  clear_gl();
  t = now_us();
  for (int i = 0; i < nsph; i++) {
    glPushMatrix();
    glTranslatef((random() % 9) - 4, (random() % 9) - 4, -20 - (random() % 5));
    glCallList(s->sph);
    glPopMatrix();
  }
  double sph_ms = (now_us() - t) / 1000.0 / nsph;

  clear_gl();
  t = now_us();
  for (int i = 0; i < ncyl; i++) {
    glPushMatrix();
    glTranslatef((random() % 9) - 4, (random() % 9) - 4, -20 - (random() % 5));
    glCallList(s->cyl);
    glPopMatrix();
  }
  double cyl_ms = (now_us() - t) / 1000.0 / ncyl;

  clear_gl();
  t = now_us();
  glPushMatrix();
  glTranslatef(0, 0, -8);
  glScalef(6, 6, 6);
  glCallList(s->sph);
  glPopMatrix();
  double big_ms = (now_us() - t) / 1000.0;
  long big_px = count_lit(s);

  t = now_us();
  for (int i = 0; i < 5; i++) blit(s, c);
  double copy_ms = (now_us() - t) / 5000.0;

  snprintf(b, sizeof b,
           "TGL bench res=%d clear=%.2fms sph=%.2fms cyl=%.3fms big=%.1fms "
           "(%ld px) copy=%.1fms",
           s->res, clear_ms, sph_ms, cyl_ms, big_ms, big_px, copy_ms);
  spike_log(b);
  snprintf(s->bench, sizeof s->bench, "%s", b);
  clear_gl();
}

static void diag(const Spike *s, const Canvas *c) {
  char b[200];
  long lit = 0, white = 0;
  for (long i = 0; i < (long)s->res * s->res; i++) {
    lit += s->fb[i] != 0;
    white += s->fb[i] == 0xFFFF;
  }
  size_t mid = (size_t)(s->res / 2) * s->res + s->res / 2;
  size_t cmid = (size_t)(c->h / 2) * c->w + c->w / 2;
  snprintf(b, sizeof b, "TGL diag lit=%ld white=%ld fb0=%04x fbmid=%04x canvas0=%04x canvasmid=%04x",
           lit, white, s->fb[0], s->fb[mid], c->px[0], c->px[cmid]);
  spike_log(b);
}

static void *tglspike_init(Display *dpy, Window win) {
  (void)win;
  spike_log("TGL init-start");
  Spike *s = calloc(1, sizeof *s);
  if (!s) return NULL;
  s->res = TGL_SPIKE_RES;
  s->fb = big_alloc((size_t)s->res * s->res * sizeof(uint16_t));
  if (!s->fb) {
    free(s);
    return NULL;
  }
  memset(s->fb, 0, (size_t)s->res * s->res * sizeof(uint16_t));
  s->zb = ZB_open(s->res, s->res, ZB_MODE_5R6G5B, s->fb);
  if (!s->zb) {
    free(s->fb);
    free(s);
    return NULL;
  }
  glInit(s->zb);

  GLfloat pos[4] = {1, 1, 1, 0}, amb[4] = {0.2f, 0.2f, 0.2f, 1},
          dif[4] = {1, 1, 1, 1};
  glViewport(0, 0, s->res, s->res);
  setup_camera();
  glClearColor(0, 0, 0, 0);
  glShadeModel(GL_SMOOTH);
  if (SPIKE_WIRE) {
    glDisable(GL_LIGHTING);
  } else {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glEnable(GL_CULL_FACE);
  }
  glLightfv(GL_LIGHT0, GL_POSITION, pos);
  glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);

  s->cyl = glGenLists(1);
  s->sph = glGenLists(1);
  s->pieces = glGenLists(MAX_PIECES);
  build_cylinder(s->cyl, SPIKE_SIDES, 0.18f);
  build_sphere(s->sph, SPIKE_SPHERE, SPIKE_SPHERE, 0.3f);
  build_pieces(s);

  bench(s, dpy->canvas);
  s->win_start = now_us();
  return s;
}

static unsigned long tglspike_draw(Display *dpy, Window win, void *closure) {
  (void)win;
  Spike *s = closure;
  if (!s) return 1000000;
  int64_t t0 = now_us();

  clear_gl();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0, 0, -20);
  glRotatef(s->angle, 0, 1, 0);
  s->angle += 0.5f;
  for (int i = 0; i < s->idx; i++) glCallList(s->pieces + i);

  int64_t t1 = now_us();
  blit(s, dpy->canvas);
  int64_t t2 = now_us();

  s->raster_us += t1 - t0;
  s->copy_us += t2 - t1;
  s->frames++;

  if (s->idx < MAX_PIECES) {
    s->idx++;
  } else if (++s->hold > HOLD_FRAMES) {
    s->idx = 0;
    s->hold = 0;
  }

  if (t2 - s->win_start >= WINDOW_US) {
    char b[200];
    snprintf(b, sizeof b,
             "TGL run pieces=%d..%d frames=%ld raster=%.1fms copy=%.1fms",
             s->first_idx, s->idx, s->frames, s->raster_us / 1000.0 / s->frames,
             s->copy_us / 1000.0 / s->frames);
    spike_log(b);
    diag(s, dpy->canvas);
    spike_log(s->bench);
    s->win_start = now_us();
    s->raster_us = s->copy_us = 0;
    s->frames = 0;
    s->first_idx = s->idx;
  }
  return 10000;
}

static void tglspike_free(Display *dpy, Window win, void *closure) {
  (void)dpy;
  (void)win;
  Spike *s = closure;
  if (!s) return;
  glClose();
  ZB_close(s->zb);
  free(s->fb);
  free(s);
}

const HackEntry tglspike_hack = {"TglSpike", NULL, tglspike_init, tglspike_draw,
                                 tglspike_free, NULL, NULL};

#endif /* TGL_SPIKE */
