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

static Group one_row(const PushRow *r) {
  return (Group){r->y, r->y, r->x0, r->x1, 1, r->x1 - r->x0 + 1};
}

int64_t push_plan_cost(const PushRow *rows, int count) {
  int64_t total = 0;
  Group cur = {0};
  for (int i = 0; i < count; i++) {
    const Group one = one_row(&rows[i]);
    cur = i == 0 || rows[i - 1].end_of_group ? one : merge(&cur, &one);
    if (rows[i].end_of_group) total += group_cost(&cur);
  }
  return total;
}

int64_t push_group_greedy(PushRow *rows, int count) {
  int64_t total = 0;
  Group cur = {0};
  for (int i = 0; i < count; i++) {
    const Group one = one_row(&rows[i]);
    rows[i].end_of_group = false;
    if (i == 0) {
      cur = one;
      continue;
    }
    const Group merged = merge(&cur, &one);
    if (group_cost(&merged) <= group_cost(&cur) + group_cost(&one)) {
      cur = merged;
    } else {
      rows[i - 1].end_of_group = true;
      total += group_cost(&cur);
      cur = one;
    }
  }
  if (count) {
    rows[count - 1].end_of_group = true;
    total += group_cost(&cur);
  }
  return total;
}

/* The cheapest split into consecutive groups of at most PUSH_DP_LOOKBACK rows.
 * Longer groups are left to the greedy split, which the caller also runs. */
#define PUSH_DP_LOOKBACK 64
#define PUSH_DP_MAX_ROWS 512
static int64_t dp_best[PUSH_DP_MAX_ROWS + 1];
static int16_t dp_from[PUSH_DP_MAX_ROWS + 1];

static int64_t cheapest_split(const PushRow *rows, int count) {
  dp_best[0] = 0;
  for (int j = 1; j <= count; j++) {
    Group g = one_row(&rows[j - 1]);
    dp_best[j] = dp_best[j - 1] + group_cost(&g);
    dp_from[j] = (int16_t)(j - 1);
    const int stop = j > PUSH_DP_LOOKBACK ? j - PUSH_DP_LOOKBACK : 0;
    for (int i = j - 2; i >= stop; i--) {
      const PushRow *r = &rows[i];
      g.first_y = r->y;
      if (r->x0 < g.x0) g.x0 = r->x0;
      if (r->x1 > g.x1) g.x1 = r->x1;
      g.rows++;
      g.pixels += r->x1 - r->x0 + 1;
      const int64_t cost = dp_best[i] + group_cost(&g);
      if (cost < dp_best[j]) {
        dp_best[j] = cost;
        dp_from[j] = (int16_t)i;
      }
    }
  }
  return dp_best[count];
}

static void mark_split(PushRow *rows, int count) {
  for (int i = 0; i < count; i++) rows[i].end_of_group = false;
  for (int j = count; j > 0; j = dp_from[j]) rows[j - 1].end_of_group = true;
}

PushKind push_plan(Canvas *c, PushRow *rows, int *count, bool use_disc) {
  int n = 0;
  for (int y = 0; y < c->h; y++) {
    int x0, x1;
    if (!canvas_dirty_row(c, y, &x0, &x1)) continue;
    if (use_disc) {
      int v0, v1;
      if (!canvas_visible_span(c, y, &v0, &v1)) continue;
      if (x0 < v0) x0 = v0;
      if (x1 > v1) x1 = v1;
      if (x0 > x1) continue;
    }
    rows[n++] = (PushRow){y, x0, x1, false};
  }
  canvas_clear_dirty(c);
  *count = 0;
  if (n == 0) return PUSH_NONE;
  int64_t total_ns = push_group_greedy(rows, n);
  if (use_disc && n <= PUSH_DP_MAX_ROWS) {
    const int64_t split_ns = cheapest_split(rows, n);
    if (split_ns < total_ns) {
      mark_split(rows, n);
      total_ns = split_ns;
    }
  }
  if (total_ns >= (int64_t)c->w * c->h * PUSH_PIXEL_NS) return PUSH_ALL;
  *count = n;
  return PUSH_ROWS;
}

int push_present(Canvas *c, PushRow *scratch, const PushSink *sink, bool use_disc) {
  int count;
  switch (push_plan(c, scratch, &count, use_disc)) {
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
