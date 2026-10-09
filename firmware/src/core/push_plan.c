#include "core/push_plan.h"

typedef struct {
  int first_y, last_y, x0, x1;
  int64_t rows, pixels;
} Group;

static int64_t group_cost(const Group *g) {
  int64_t h = g->last_y - g->first_y + 1, w = g->x1 - g->x0 + 1;
  return PUSH_FLUSH_NS + h * PUSH_FLUSH_ROW_NS + w * h * PUSH_FLUSH_PIXEL_NS +
         g->rows * PUSH_CALL_NS + g->pixels * PUSH_COPY_PIXEL_NS;
}

static Group merge(const Group *a, const Group *b) {
  Group g = *a;
  g.last_y = b->last_y;
  if (b->x0 < g.x0) g.x0 = b->x0;
  if (b->x1 > g.x1) g.x1 = b->x1;
  g.rows += b->rows;
  g.pixels += b->pixels;
  return g;
}

PushKind push_plan(Canvas *c, PushRow *rows, int *count) {
  int n = 0;
  int64_t total_ns = 0;
  Group cur = {0};
  for (int y = 0; y < c->h; y++) {
    int x0, x1;
    if (!canvas_dirty_row(c, y, &x0, &x1)) continue;
    Group one = {y, y, x0, x1, 1, x1 - x0 + 1};
    if (n == 0) {
      cur = one;
    } else {
      Group merged = merge(&cur, &one);
      if (group_cost(&merged) <= group_cost(&cur) + group_cost(&one)) {
        cur = merged;
      } else {
        rows[n - 1].end_of_group = true;
        total_ns += group_cost(&cur);
        cur = one;
      }
    }
    rows[n++] = (PushRow){y, x0, x1, false};
  }
  canvas_clear_dirty(c);
  *count = 0;
  if (n == 0) return PUSH_NONE;
  rows[n - 1].end_of_group = true;
  total_ns += group_cost(&cur);
  if (total_ns >= (int64_t)c->w * c->h * PUSH_PIXEL_NS) return PUSH_ALL;
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
      for (int i = 0; i < count; i++) {
        if (i == 0 || scratch[i - 1].end_of_group) sink->begin(sink->ctx);
        sink->row(sink->ctx, scratch[i].y, scratch[i].x0, scratch[i].x1);
        if (scratch[i].end_of_group) sink->end(sink->ctx);
      }
      return count;
  }
}
