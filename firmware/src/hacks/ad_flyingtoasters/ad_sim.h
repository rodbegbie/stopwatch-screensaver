#ifndef AD_SIM_H
#define AD_SIM_H

#define AD_MAX_FLYERS 20
#define AD_RECT 70 /* flyer rect: 64 art + 6 px strip */
#define AD_STRIP 6

/* QuickDraw order; right and bottom are exclusive. */
typedef struct {
  int top, left, bottom, right;
} AdRect;

typedef struct {
  int live, toast, speed, frame, flap_down;
  AdRect r;
} AdFlyer;

typedef struct {
  AdRect bounds;
  int n, running;
  AdFlyer f[AD_MAX_FLYERS];
  long (*rnd)(void); /* random() in the hack; scripted in tests */
} AdSim;

/* sprite: 0-3 toaster frame, 4 toast. Called after the flyer's move. */
typedef void (*AdDrawFn)(void *ctx, const AdRect *r, int sprite);

/* Scatter n flyers inside bounds. Does not draw. */
void ad_sim_init(AdSim *, AdRect bounds, int n, long (*rnd)(void));
/* Draw each live flyer once, in slot order (advances toaster frames). */
void ad_sim_draw_all(AdSim *, AdDrawFn, void *ctx);
/* One simulation step: move, respawn, collide, draw. */
void ad_sim_step(AdSim *, AdDrawFn, void *ctx);
/* First live flyer (other than slot i) overlapping cand: sets the nudge. */
int ad_sim_collision(const AdSim *, int i, AdRect cand, int *dv, int *dh);

#endif
