#include "core/push_plan.h"

PushKind push_plan(Canvas *c, PushRow *rows, int *count) {
  int n = 0;
  int64_t rows_ns = 0;
  for (int y = 0; y < c->h; y++) {
    int x0, x1;
    if (!canvas_dirty_row(c, y, &x0, &x1)) continue;
    rows[n++] = (PushRow){y, x0, x1};
    rows_ns += PUSH_ROW_NS + (int64_t)(x1 - x0 + 1) * PUSH_PIXEL_NS;
  }
  canvas_clear_dirty(c);
  *count = 0;
  if (n == 0) return PUSH_NONE;
  if (rows_ns >= (int64_t)c->w * c->h * PUSH_PIXEL_NS) return PUSH_ALL;
  *count = n;
  return PUSH_ROWS;
}

int push_present(Canvas *c, PushRow *scratch, const PushSink *sink) {
  int count;
  switch (push_plan(c, scratch, &count)) {
    case PUSH_NONE:
      return 0;
    case PUSH_ALL:
      sink->all(sink->ctx);
      return c->h;
    default:
      sink->begin(sink->ctx);
      for (int i = 0; i < count; i++)
        sink->row(sink->ctx, scratch[i].y, scratch[i].x0, scratch[i].x1);
      sink->end(sink->ctx);
      return count;
  }
}
