#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/canvas.h"
#include "hacks/registry.h"
#include "runner/hack_runner.h"

/* Usage: dump <hack-index> <frames> <out.raw>  (466x466 little-endian RGB565)
 *        dump stats <hack-index>  (mean host milliseconds per step, after a
 *        warm-up; tools/probe_hacks.py and tools/speed_backtest.py read it) */
static double now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static int stats_main(int index) {
  enum { WARMUP = 100, TIMED = 300 };
  Canvas cv;
  if (canvas_init(&cv, 466, 466, malloc) != 0) return 1;
  HackRunner *r = runner_create(&cv);
  if (!r || runner_start(r, index) != 0) {
    fprintf(stderr, "bad hack index %d (have %d)\n", index, g_hack_count);
    return 1;
  }
  for (int i = 0; i < WARMUP; i++) runner_step(r);
  const double start = now_ms();
  for (int i = 0; i < TIMED; i++) runner_step(r);
  printf("%s host_ms=%.4f\n", g_hacks[index]->name, (now_ms() - start) / TIMED);
  runner_destroy(r);
  canvas_free(&cv);
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 3 && strcmp(argv[1], "stats") == 0) return stats_main(atoi(argv[2]));
  if (argc != 4) {
    fprintf(stderr, "usage: %s <hack-index> <frames> <out.raw>\n", argv[0]);
    return 2;
  }
  int index = atoi(argv[1]), frames = atoi(argv[2]);
  Canvas cv;
  if (canvas_init(&cv, 466, 466, malloc) != 0) return 1;
  HackRunner *r = runner_create(&cv);
  if (!r || runner_start(r, index) != 0) {
    fprintf(stderr, "bad hack index %d (have %d)\n", index, g_hack_count);
    return 1;
  }
  for (int i = 0; i < frames; i++) runner_step(r);
  FILE *f = fopen(argv[3], "wb");
  if (!f) return 1;
  for (size_t i = 0; i < (size_t)cv.w * cv.h; i++) {
    uint16_t v = px_swap(cv.px[i]);
    fwrite(&v, sizeof v, 1, f);
  }
  fclose(f);
  runner_destroy(r);
  canvas_free(&cv);
  return 0;
}
