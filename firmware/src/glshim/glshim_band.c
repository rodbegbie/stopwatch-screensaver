#include "glshim/glshim_band.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "glshim/glshim.h"
#include "glshim/glshim_prof.h"

#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#include <soc/soc_memory_layout.h>
#include <stdio.h>
#define BAND_INTERNAL_MALLOC(n) \
  heap_caps_malloc((n), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define BAND_PSRAM_MALLOC(n) heap_caps_malloc((n), MALLOC_CAP_SPIRAM)
#else
#define BAND_INTERNAL_MALLOC(n) malloc(n)
#define BAND_PSRAM_MALLOC(n) malloc(n)
#endif

enum {
  BAND_ROWS = 32,
  BAND_CAPACITY = 16384, /* records: 1 MB of PSRAM */
  BAND_MAX_STATES = 255,
  KIND_FILL = 0,
};

/* What a rasteriser reads from the ZBuffer besides its buffers. */
typedef struct {
  GLenum blendeq, sfactor, dfactor;
  GLint enable_blend, depth_test, depth_write, line_smooth;
  GLfloat pointsize;
} BandState;

typedef struct {
  int16_t x, y;
  int32_t z, r, g, b;
} BandVertex;

typedef struct {
  uint8_t kind, fn, state, unused;
  BandVertex v[3];
} GlshimRecord;

#ifdef __XTENSA__
_Static_assert(sizeof(GlshimRecord) == 64, "two cache lines a record");
#endif

static const GlshimFillFn kFills[] = {
    ZB_fillTriangleSmooth,
    ZB_fillTriangleSmoothNOBLEND,
    ZB_fillTriangleFlat,
    ZB_fillTriangleFlatNOBLEND,
};

static int s_w, s_h;
static GlshimRecord *s_records;      /* PSRAM, written once and read per band */
static uint16_t (*s_rows)[2];        /* first and last row of each record */
static PIXEL *s_px;                  /* one band of colour */
static GLushort *s_z;                /* and depth */
static int s_count, s_capacity;
static BandState s_states[BAND_MAX_STATES];
static int s_state_count;
static int s_open, s_enabled = 1, s_holding, s_z_stale, s_fail_next_open;
static PIXEL s_clear_color;
static GLushort s_clear_z;
static unsigned s_held_frames, s_spills;

void glshim_set_banding(int on) { s_enabled = on; }

void glshim_band_close(void) {
  free(s_records);
  free(s_rows);
  free(s_px);
  free(s_z);
  s_records = NULL;
  s_rows = NULL;
  s_px = NULL;
  s_z = NULL;
  s_open = s_holding = s_z_stale = 0;
  s_enabled = 1;
}

int glshim_band_open(int w, int h) {
  glshim_band_close();
#ifdef GL_NO_BANDS /* every frame drawn directly, to measure against */
  return -1;
#endif
  if (s_fail_next_open) {
    s_fail_next_open = 0;
    return -1;
  }
  s_w = w;
  s_h = h;
  s_records = BAND_PSRAM_MALLOC(BAND_CAPACITY * sizeof *s_records);
  s_rows = BAND_INTERNAL_MALLOC(BAND_CAPACITY * sizeof *s_rows);
  s_px = BAND_INTERNAL_MALLOC((size_t)w * BAND_ROWS * sizeof *s_px);
  s_z = BAND_INTERNAL_MALLOC((size_t)w * BAND_ROWS * sizeof *s_z);
  if (!s_records || !s_rows || !s_px || !s_z) {
    glshim_band_close();
    return -1;
  }
  s_capacity = BAND_CAPACITY;
  s_open = 1;
#if defined(GL_PROFILE) && defined(ESP_PLATFORM)
  printf("bands px=%p internal=%d z=%p internal=%d rows=%p records=%p\n",
         (void *)s_px, esp_ptr_internal(s_px), (void *)s_z, esp_ptr_internal(s_z),
         (void *)s_rows, (void *)s_records);
#endif
  return 0;
}

int glshim_band_begin(ZBuffer *zb, PIXEL clear_color, GLint clear_z) {
  (void)zb;
  if (!s_open || !s_enabled) return 0;
  s_holding = 1;
  s_count = s_state_count = 0;
  s_clear_color = clear_color;
  s_clear_z = (GLushort)clear_z;
  s_z_stale = 0;
  s_held_frames++;
  return 1;
}

int glshim_band_holding(void) { return s_holding; }

static BandState state_of(const ZBuffer *zb) {
  BandState s;
  memset(&s, 0, sizeof s);
  s.blendeq = zb->blendeq;
  s.sfactor = zb->sfactor;
  s.dfactor = zb->dfactor;
  s.enable_blend = zb->enable_blend;
  s.depth_test = zb->depth_test;
  s.depth_write = zb->depth_write;
  s.line_smooth = zb->line_smooth;
  s.pointsize = zb->pointsize;
  return s;
}

/* The index of zb's state in this frame's table, or -1 if it is full. */
static int state_index(const ZBuffer *zb) {
  const BandState s = state_of(zb);
  if (s_state_count &&
      !memcmp(&s, &s_states[s_state_count - 1], sizeof s))
    return s_state_count - 1;
  if (s_state_count == BAND_MAX_STATES) return -1;
  s_states[s_state_count] = s;
  return s_state_count++;
}

static int fill_index(GlshimFillFn fn) {
  for (int i = 0; i < (int)(sizeof kFills / sizeof kFills[0]); i++)
    if (kFills[i] == fn) return i;
  return -1;
}

static BandVertex vertex_of(const ZBufferPoint *p) {
  const BandVertex v = {(int16_t)p->x, (int16_t)p->y, p->z, p->r, p->g, p->b};
  return v;
}

static ZBufferPoint point_of(const BandVertex *v, int band_y) {
  ZBufferPoint p;
  memset(&p, 0, sizeof p);
  p.x = v->x;
  p.y = v->y - band_y;
  p.z = v->z;
  p.r = v->r;
  p.g = v->g;
  p.b = v->b;
  return p;
}

int glshim_band_fill(ZBuffer *zb, GlshimFillFn fn, ZBufferPoint *a,
                     ZBufferPoint *b, ZBufferPoint *c) {
  if (!s_holding) {
    /* Drawing against depth that a held frame never wrote. */
    if (s_z_stale && zb->depth_test) s_enabled = 0;
    return 0;
  }
  const int fi = fill_index(fn);
  const int si = (fi < 0 || zb->dostipple || s_count == s_capacity)
                     ? -1
                     : state_index(zb);
  if (si < 0) {
    glshim_band_spill(zb);
    return 0;
  }
  int lo = a->y, hi = a->y;
  if (b->y < lo) lo = b->y;
  if (b->y > hi) hi = b->y;
  if (c->y < lo) lo = c->y;
  if (c->y > hi) hi = c->y;
  if (lo < 0) lo = 0;
  if (hi > s_h - 1) hi = s_h - 1;
  if (lo > hi) return 1; /* wholly off the canvas: draws nothing either way */
  GlshimRecord *r = &s_records[s_count];
  r->kind = KIND_FILL;
  r->fn = (uint8_t)fi;
  r->state = (uint8_t)si;
  r->v[0] = vertex_of(a);
  r->v[1] = vertex_of(b);
  r->v[2] = vertex_of(c);
  s_rows[s_count][0] = (uint16_t)lo;
  s_rows[s_count][1] = (uint16_t)hi;
  s_count++;
  return 1;
}

static void apply_state(ZBuffer *band, const BandState *s) {
  band->blendeq = s->blendeq;
  band->sfactor = s->sfactor;
  band->dfactor = s->dfactor;
  band->enable_blend = s->enable_blend;
  band->depth_test = s->depth_test;
  band->depth_write = s->depth_write;
  band->line_smooth = s->line_smooth;
  band->pointsize = s->pointsize;
}

static void replay(ZBuffer *band, int band_y, int band_h) {
  const int last = band_y + band_h - 1;
  int state = -1;
  for (int i = 0; i < s_count; i++) {
    if (s_rows[i][1] < band_y || s_rows[i][0] > last) continue;
    const GlshimRecord *r = &s_records[i];
    if (r->state != state) {
      state = r->state;
      apply_state(band, &s_states[state]);
    }
    ZBufferPoint p0 = point_of(&r->v[0], band_y);
    ZBufferPoint p1 = point_of(&r->v[1], band_y);
    ZBufferPoint p2 = point_of(&r->v[2], band_y);
    kFills[r->fn](band, &p0, &p1, &p2);
  }
}

void glshim_band_draw(ZBuffer *zb, int x0, int y0, int x1, int y1,
                      int write_depth) {
  if (!s_holding) return;
  s_holding = 0;
  s_z_stale = !write_depth;
  if (write_depth) s_spills++;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > s_w - 1) x1 = s_w - 1;
  if (y1 > s_h - 1) y1 = s_h - 1;
  if (x0 > x1 || y0 > y1) return;
  ZBuffer band = *zb;
  band.pbuf = s_px;
  band.zbuf = s_z;
  band.dostipple = 0;
  const int width = x1 - x0 + 1;
  for (int band_y = y0 - y0 % BAND_ROWS; band_y <= y1; band_y += BAND_ROWS) {
    const int band_h = band_y + BAND_ROWS > s_h ? s_h - band_y : BAND_ROWS;
    band.ysize = band_h;
#ifdef GL_PROFILE
    uint32_t t0 = glshim_prof_now();
#endif
    for (int i = 0; i < s_w * band_h; i++) {
      s_px[i] = s_clear_color;
      s_z[i] = s_clear_z;
    }
#ifdef GL_PROFILE
    glshim_prof_add(GLPROF_BAND_CLEAR, t0);
    t0 = glshim_prof_now();
#endif
    replay(&band, band_y, band_h);
#ifdef GL_PROFILE
    glshim_prof_add(GLPROF_BAND_REPLAY, t0);
    t0 = glshim_prof_now();
#endif
    const int from = band_y > y0 ? band_y : y0;
    const int to = band_y + band_h - 1 < y1 ? band_y + band_h - 1 : y1;
    for (int y = from; y <= to; y++) {
      const int row = (y - band_y) * s_w + x0;
      memcpy((GLbyte *)zb->pbuf + y * zb->linesize + x0 * (int)sizeof(PIXEL),
             s_px + row, width * sizeof(PIXEL));
      if (write_depth)
        memcpy(zb->zbuf + y * zb->xsize + x0, s_z + row,
               width * sizeof(GLushort));
    }
#ifdef GL_PROFILE
    glshim_prof_add(GLPROF_BAND_COPY, t0);
#endif
  }
}

void glshim_band_depth_valid(void) { s_z_stale = 0; }

unsigned glshim_band_held_frames(void) { return s_held_frames; }
unsigned glshim_band_spills(void) { return s_spills; }

void glshim_band_set_capacity(int records) {
  s_capacity = records < BAND_CAPACITY ? records : BAND_CAPACITY;
}

void glshim_band_fail_next_open(void) { s_fail_next_open = 1; }
