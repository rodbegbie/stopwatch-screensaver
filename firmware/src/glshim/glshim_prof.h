/* Build switches for TinyGL speed work (issue #43).
 *
 * GL_LAZY_SHADE (on unless GL_EAGER_SHADE is defined): a vertex is lit when
 * a triangle, line or point that uses it is drawn, not as it arrives, so
 * culled triangles cost no lighting. Measured on the board: Gears 61.1 to
 * 57.7 ms a frame, Morph3D 61.4 to 58.4 ms. Frame hashes are unchanged.
 * GL_EAGER_SHADE keeps TinyGL's own order, for comparison.
 *
 * GL_IRAM: TinyGL's per-vertex and per-triangle functions go in IRAM, so
 * they never wait on the flash cache. Costs internal RAM.
 *
 * GL_PROFILE: cycle counts for the stages of a frame, printed by main.cpp
 * after each `run` line (GLPROF_SCOPE in the patched TinyGL functions; see
 * HOT_FUNCTIONS in tools/tinygl_patch.py). Times are inclusive: VERTEX contains SHADE and the
 * triangles, points and lines a vertex completes, TRIANGLE contains FILL,
 * and LIST contains everything a display list replays. */
#ifndef GLSHIM_PROF_H
#define GLSHIM_PROF_H

#if !defined(GL_EAGER_SHADE) && !defined(GL_LAZY_SHADE)
#define GL_LAZY_SHADE 1
#endif

#if defined(GL_IRAM) && defined(ESP_PLATFORM)
#include "esp_attr.h"
#define TGL_HOT IRAM_ATTR
#else
#define TGL_HOT
#endif

#ifdef GL_PROFILE
#include <stdint.h>
#ifndef __XTENSA__
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
  GLPROF_CLEAR,
  GLPROF_LIST,
  GLPROF_VERTEX,
  GLPROF_SHADE,
  GLPROF_TRIANGLE,
  GLPROF_FILL,
  GLPROF_POINT,
  GLPROF_LINE,
  GLPROF_SLOTS
};

extern uint64_t glshim_prof_time[GLPROF_SLOTS]; /* CPU cycles (ns on a host) */
extern uint32_t glshim_prof_calls[GLPROF_SLOTS];
extern uint32_t glshim_prof_frames;
extern const char *const glshim_prof_names[GLPROF_SLOTS];
void glshim_prof_reset(void);

static inline uint32_t glshim_prof_now(void) {
#ifdef __XTENSA__
  uint32_t c;
  __asm__ volatile("rsr %0, ccount" : "=a"(c));
  return c;
#else
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint32_t)(t.tv_sec * 1000000000ull + t.tv_nsec);
#endif
}

static inline void glshim_prof_add(int slot, uint32_t start) {
  glshim_prof_time[slot] += (uint32_t)(glshim_prof_now() - start);
  glshim_prof_calls[slot]++;
}

/* A timer that stops when the function returns, by whichever return. */
typedef struct {
  int slot; /* negative: an inner call of a reentrant function, not timed */
  uint32_t start;
} GlProfScope;

extern int glshim_prof_depth;

static inline void glshim_prof_end(GlProfScope *s) {
  if (s->slot >= 0) glshim_prof_add(s->slot, s->start);
}

static inline void glshim_prof_end_outer(GlProfScope *s) {
  glshim_prof_depth--;
  glshim_prof_end(s);
}

#define GLPROF_SCOPE(slot)                                        \
  __attribute__((cleanup(glshim_prof_end))) GlProfScope glprof = { \
      (slot), glshim_prof_now()}
#define GLPROF_SCOPE_OUTER(slot)                                         \
  __attribute__((cleanup(glshim_prof_end_outer))) GlProfScope glprof = { \
      glshim_prof_depth++ ? -1 : (slot), glshim_prof_now()}

#ifdef __cplusplus
}
#endif
#else
#define GLPROF_SCOPE(slot) \
  do {                     \
  } while (0)
#define GLPROF_SCOPE_OUTER(slot) GLPROF_SCOPE(slot)
#endif /* GL_PROFILE */

#endif
