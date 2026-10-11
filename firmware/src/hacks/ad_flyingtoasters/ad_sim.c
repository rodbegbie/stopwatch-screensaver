#include "ad_sim.h"

#include <stdlib.h>

#define TRIES 4

static AdRect offset(AdRect r, int dh, int dv) {
  r.left += dh;
  r.right += dh;
  r.top += dv;
  r.bottom += dv;
  return r;
}

static int intersects(const AdRect *a, const AdRect *b) {
  return a->left < b->right && b->left < a->right && a->top < b->bottom && b->top < a->bottom;
}

static long rnd_mod(const AdSim *sim, long k) { return labs(sim->rnd() % k); }

static AdRect random_inside_rect(const AdSim *sim) {
  int w = sim->bounds.right - sim->bounds.left;
  int h = sim->bounds.bottom - sim->bounds.top;
  AdRect r;
  r.top = sim->bounds.top + (int)rnd_mod(sim, h - AD_RECT);
  r.left = sim->bounds.left + (int)rnd_mod(sim, w - AD_RECT);
  r.bottom = r.top + AD_RECT;
  r.right = r.left + AD_RECT;
  return r;
}

static AdRect random_edge_rect(const AdSim *sim) {
  int w = sim->bounds.right - sim->bounds.left;
  int h = sim->bounds.bottom - sim->bounds.top;
  AdRect r;
  if (sim->rnd() % 2 != 0) {
    r.bottom = sim->bounds.top;
    r.left = sim->bounds.left + (int)rnd_mod(sim, w);
    r.top = r.bottom - AD_RECT;
    r.right = r.left + AD_RECT;
  } else {
    r.left = sim->bounds.right;
    r.top = sim->bounds.top + (int)rnd_mod(sim, h);
    r.right = r.left + AD_RECT;
    r.bottom = r.top + AD_RECT;
  }
  return r;
}

int ad_sim_collision(const AdSim *sim, int i, AdRect cand, int *dv, int *dh) {
  for (int k = 0; k < sim->n; k++) {
    const AdFlyer *o = &sim->f[k];
    if (k == i || !o->live || !intersects(&o->r, &cand)) continue;
    int dl = o->r.left - cand.left;
    int dt = o->r.top - cand.top;
    if (abs(dt) < abs(dl)) {
      *dv = -1;
      *dh = 0;
    } else {
      *dv = 0;
      *dh = cand.top < o->r.top ? -1 : 1;
    }
    return 1;
  }
  return 0;
}

static void create_flyer(AdSim *sim, int i) {
  AdRect r = {0, 0, 0, 0};
  int placed = 0;
  for (int t = 0; t < TRIES && !placed; t++) {
    r = sim->running ? random_edge_rect(sim) : random_inside_rect(sim);
    int dv, dh;
    placed = !ad_sim_collision(sim, i, r, &dv, &dh);
  }
  if (!placed) return;
  long roll = rnd_mod(sim, 100);
  AdFlyer *f = &sim->f[i];
  f->live = 1;
  f->toast = roll >= 75;
  f->speed = f->toast ? 1 : roll >= 50 ? 3 : roll >= 25 ? 2 : 1;
  f->frame = 0;
  f->flap_down = 0;
  f->r = r;
}

void ad_sim_init(AdSim *sim, AdRect bounds, int n, long (*rnd)(void)) {
  if (n > AD_MAX_FLYERS) n = AD_MAX_FLYERS;
  if (n < 0) n = 0;
  for (int i = 0; i < AD_MAX_FLYERS; i++) sim->f[i] = (AdFlyer){0};
  sim->bounds = bounds;
  sim->n = n;
  sim->rnd = rnd;
  sim->running = 0;
  for (int i = 0; i < n; i++) create_flyer(sim, i);
  sim->running = 1;
}

static void draw_flyer(AdFlyer *f, AdDrawFn draw, void *ctx) {
  draw(ctx, &f->r, f->toast ? 4 : f->frame);
  if (f->toast) return;
  if (f->flap_down) {
    if (f->frame == 0) {
      f->frame = 1;
      f->flap_down = 0;
    } else {
      f->frame--;
    }
  } else if (f->frame == 3) {
    f->frame = 2;
    f->flap_down = 1;
  } else {
    f->frame++;
  }
}

void ad_sim_draw_all(AdSim *sim, AdDrawFn draw, void *ctx) {
  for (int i = 0; i < sim->n; i++) {
    if (sim->f[i].live) draw_flyer(&sim->f[i], draw, ctx);
  }
}

void ad_sim_step(AdSim *sim, AdDrawFn draw, void *ctx) {
  for (int i = 0; i < sim->n; i++) {
    if (!sim->f[i].live) create_flyer(sim, i);
    if (sim->f[i].live) sim->f[i].r = offset(sim->f[i].r, -2, 1);
  }
  for (int i = 0; i < sim->n; i++) {
    AdFlyer *f = &sim->f[i];
    if (!f->live) continue;
    if (!intersects(&f->r, &sim->bounds)) {
      f->live = 0;
      continue;
    }
    int s = f->speed - 1;
    if (s > 0) {
      AdRect cand = offset(f->r, -2 * s, s);
      int dv, dh;
      if (ad_sim_collision(sim, i, cand, &dv, &dh)) {
        cand = offset(f->r, dh, -2 * dv);
        int dv2, dh2;
        if (!ad_sim_collision(sim, i, cand, &dv2, &dh2)) f->r = cand;
      } else {
        f->r = cand;
      }
    }
    draw_flyer(f, draw, ctx);
  }
}
