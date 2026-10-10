#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "core/canvas.h"
#include "glshim/glshim.h"
#include "hacks/registry.h"
#include "runner/hack_runner.h"

/* Usage: gl_budget <frames> <seed>
 * Runs hack 0 of the registry, seeded with srandom(seed), and prints the mean
 * per-frame counts that tools/gl_budget.py reads. The counters live inside a
 * private copy of TinyGL built by that tool. */
double gl_budget_counts[7];
double gl_budget_bbox[4]; /* xmin, ymin, xmax, ymax of what a frame drew */

/* The boxes come from glshim itself, after its clamping and ordering, so this
 * measures what the firmware's dirty rectangle sees and not a copy of its rules. */
static void on_box(int x0, int y0, int x1, int y1) {
  if (x0 < gl_budget_bbox[0]) gl_budget_bbox[0] = x0;
  if (y0 < gl_budget_bbox[1]) gl_budget_bbox[1] = y0;
  if (x1 > gl_budget_bbox[2]) gl_budget_bbox[2] = x1;
  if (y1 > gl_budget_bbox[3]) gl_budget_bbox[3] = y1;
}

enum { WARMUP = 10 };

typedef struct {
  double x0, y0, x1, y1; /* inclusive; x0 > x1 when nothing was drawn */
} Box;

static Box take_bbox(void) {
  Box b = {gl_budget_bbox[0], gl_budget_bbox[1], gl_budget_bbox[2],
           gl_budget_bbox[3]};
  gl_budget_bbox[0] = gl_budget_bbox[1] = 1e9;
  gl_budget_bbox[2] = gl_budget_bbox[3] = -1e9;
  return b;
}

static double area(Box b) {
  if (b.x0 > b.x1 || b.y0 > b.y1) return 0;
  return (b.x1 - b.x0 + 1) * (b.y1 - b.y0 + 1);
}

static double overlap(Box a, Box b) {
  Box i = {a.x0 > b.x0 ? a.x0 : b.x0, a.y0 > b.y0 ? a.y0 : b.y0,
           a.x1 < b.x1 ? a.x1 : b.x1, a.y1 < b.y1 ? a.y1 : b.y1};
  return area(i);
}

static double now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <frames> <seed>\n", argv[0]);
    return 2;
  }
  const int frames = atoi(argv[1]);
  if (frames <= 0) return 2;
  srandom((unsigned)atoi(argv[2]));

  Canvas cv;
  if (canvas_init(&cv, 466, 466, malloc) != 0) return 1;
  HackRunner *r = runner_create(&cv);
  if (!r || runner_start(r, 0) != 0) {
    fprintf(stderr, "could not start the hack\n");
    return 1;
  }
  glshim_box_observer = on_box;
  take_bbox();
  Box prev = {1e9, 1e9, -1e9, -1e9};
  for (int i = 0; i < WARMUP; i++) {
    runner_step(r);
    prev = take_bbox();
  }
  for (int i = 0; i < 7; i++) gl_budget_counts[i] = 0;
  const double screen = (double)cv.w * cv.h;
  double sum_bbox = 0, sum_clear = 0, sum_push = 0;
  const double start = now_ms();
  for (int i = 0; i < frames; i++) {
    runner_step(r);
    const Box cur = take_bbox();
    sum_bbox += area(cur) / screen;
    sum_clear += area(prev) / screen;
    sum_push += (area(prev) + area(cur) - overlap(prev, cur)) / screen;
    prev = cur;
  }
  const double host_ms = (now_ms() - start) / frames;

  printf("counts frames=%d", frames);
  const char *names[7] = {"vertices", "lit_vertices", "triangles", "lines",
                          "points",   "pixels",       "light_terms"};
  for (int i = 0; i < 7; i++)
    printf(" %s=%.4f", names[i], gl_budget_counts[i] / frames);
  printf(" host_ms=%.4f bbox=%.4f clear=%.4f push=%.4f\n", host_ms,
         sum_bbox / frames, sum_clear / frames, sum_push / frames);
  runner_destroy(r);
  canvas_free(&cv);
  return 0;
}
