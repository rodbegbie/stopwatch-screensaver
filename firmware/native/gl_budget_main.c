#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "core/canvas.h"
#include "hacks/registry.h"
#include "runner/hack_runner.h"

/* Usage: gl_budget <frames> <seed>
 * Runs hack 0 of the registry, seeded with srandom(seed), and prints the mean
 * per-frame counts that tools/gl_budget.py reads. The counters live inside a
 * private copy of TinyGL built by that tool. */
double gl_budget_counts[6];

enum { WARMUP = 10 };

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
  for (int i = 0; i < WARMUP; i++) runner_step(r);
  for (int i = 0; i < 6; i++) gl_budget_counts[i] = 0;
  const double start = now_ms();
  for (int i = 0; i < frames; i++) runner_step(r);
  const double host_ms = (now_ms() - start) / frames;

  printf("counts frames=%d", frames);
  const char *names[6] = {"vertices", "lit_vertices", "triangles",
                          "lines",    "points",       "pixels"};
  for (int i = 0; i < 6; i++)
    printf(" %s=%.4f", names[i], gl_budget_counts[i] / frames);
  printf(" host_ms=%.4f\n", host_ms);
  runner_destroy(r);
  canvas_free(&cv);
  return 0;
}
