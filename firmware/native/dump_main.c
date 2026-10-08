#include <stdio.h>
#include <stdlib.h>

#include "core/canvas.h"
#include "hacks/registry.h"
#include "runner/hack_runner.h"

/* Usage: dump <hack-index> <frames> <out.raw>  (466x466 little-endian RGB565) */
int main(int argc, char **argv) {
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
  fwrite(cv.px, sizeof(uint16_t), (size_t)cv.w * cv.h, f);
  fclose(f);
  runner_destroy(r);
  canvas_free(&cv);
  return 0;
}
