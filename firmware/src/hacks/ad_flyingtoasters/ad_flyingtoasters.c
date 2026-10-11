/* ad-flyingtoasters: a recreation of Flying Toasters from Berkeley Systems'
 * After Dark 2.0, (c) 1990-91 Berkeley Systems, by Jack Eastman, Bruce
 * Burkhalter, Patrick Beard and Tomoya Ikeda. This code is ours (MIT); the
 * art is read at build time from the user's own copy of the After Dark disk
 * (https://archive.org/details/after-dark-2.0v-800k) and never committed. */

#ifdef HAVE_AFTER_DARK_TOASTERS

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "after_dark/toaster_sprites.h"
#include "core/canvas.h"
#include "doubletime.h"
#include "hacks/ad_flyingtoasters/ad_sim.h"
#include "hacks/ad_flyingtoasters/ad_sprites.h"
#include "runner/hack_entry.h"

#ifdef ESP_PLATFORM
#include <esp_random.h>
#endif

#if defined(AD_TOASTERS_ART) && AD_TOASTERS_ART != 0 && AD_TOASTERS_ART != 1
#error "AD_TOASTERS_ART must be 0 (1-bit) or 1 (colour)"
#endif
#if defined(AD_TOASTERS_DONENESS) && AD_TOASTERS_DONENESS != 0 && \
    AD_TOASTERS_DONENESS != 1 && AD_TOASTERS_DONENESS != 2
#error "AD_TOASTERS_DONENESS must be 0 (light), 1 (medium) or 2 (dark)"
#endif

#define AD_SPRITES 5
#ifndef AD_TOASTERS_SCALE
#define AD_TOASTERS_SCALE 2
#endif
#if AD_TOASTERS_SCALE != 1 && AD_TOASTERS_SCALE != 2
#error "AD_TOASTERS_SCALE must be 1 (original size) or 2 (chunky pixels)"
#endif
#define AD_S AD_TOASTERS_SCALE
#define AD_SIDE (AD_RECT * AD_S)
#define AD_COUNT (AD_S == 1 ? 10 : 6)

typedef struct {
  Display *dpy;
  Window window;
  GC gc;
  XImage *sprite[AD_SPRITES];
  AdSim sim;
} State;

static const char *const ad_flyingtoasters_defaults[] = {".background: black", 0};

/* random() on the device replays the same sequence after every boot, so the
 * picks take real entropy there; host builds (tests, dump) keep random() so
 * their frames repeat. */
static uint32_t ad_entropy(void) {
#ifdef ESP_PLATFORM
  return esp_random();
#else
  return (uint32_t)random();
#endif
}

static void ad_flyingtoasters_free(Display *dpy, Window window, void *closure);

static XImage *make_sprite(Display *dpy, const unsigned char *rgb) {
  XImage *im = XCreateImage(dpy, NULL, 16, ZPixmap, 0, NULL, AD_SIDE, AD_SIDE, 16, 0);
  if (!im) return NULL;
  uint16_t *px = malloc(AD_SIDE * AD_SIDE * sizeof *px);
  uint16_t *one = malloc(AD_RECT * AD_RECT * sizeof *one);
  if (!px || !one) {
    free(one);
    free(px);
    XDestroyImage(im);
    return NULL;
  }
  ad_build_sprite(rgb, one);
  /* XPutImage copies image pixels as they are, and the canvas keeps RGB565
   * with its bytes swapped (xshim.h's comment saying it swaps is stale). */
  ad_scale_sprite(one, AD_RECT, AD_S, px);
  free(one);
  for (int i = 0; i < AD_SIDE * AD_SIDE; i++) px[i] = px_swap(px[i]);
  im->data = (char *)px;
  return im;
}

static void blit(void *ctx, const AdRect *r, int sprite) {
  State *st = ctx;
  XPutImage(st->dpy, st->window, st->gc, st->sprite[sprite], 0, 0, r->left * AD_S,
            r->top * AD_S, AD_SIDE, AD_SIDE);
}

static void *ad_flyingtoasters_init(Display *dpy, Window window) {
  State *st = calloc(1, sizeof *st);
  if (!st) return NULL;
  st->dpy = dpy;
  st->window = window;
  st->gc = XCreateGC(dpy, window, 0, NULL);

  AdChoice pick = ad_choose(ad_entropy());
#ifdef AD_TOASTERS_ART
  pick.colour = AD_TOASTERS_ART;
#endif
#ifdef AD_TOASTERS_DONENESS
  pick.doneness = (AdDoneness)AD_TOASTERS_DONENESS;
#endif

  unsigned char *rgb = malloc(64 * 64 * 3);
  if (!rgb) {
    ad_flyingtoasters_free(dpy, window, st);
    return NULL;
  }
  for (int s = 0; s < AD_SPRITES; s++) {
    if (pick.colour) {
      memcpy(rgb, ad_colour_rgb[s], 64 * 64 * 3);
      if (s == 4) ad_apply_doneness(rgb, 64 * 64, pick.doneness);
    } else {
      ad_expand_mono(ad_mono_bits[s], rgb);
    }
    st->sprite[s] = make_sprite(dpy, rgb);
    if (!st->sprite[s]) {
      free(rgb);
      ad_flyingtoasters_free(dpy, window, st);
      return NULL;
    }
  }
  free(rgb);

  static const char *const doneness[] = {"light", "medium", "dark"};
  fprintf(stderr, "ad-flyingtoasters: %s, doneness=%s\n",
          pick.colour ? "colour" : "1-bit", doneness[pick.doneness]);

  XWindowAttributes xgwa;
  XGetWindowAttributes(dpy, window, &xgwa);
  AdRect bounds = {0, 0, xgwa.height / AD_S, xgwa.width / AD_S};
  ad_sim_init(&st->sim, bounds, AD_COUNT, random);
  ad_sim_draw_all(&st->sim, blit, st);
  return st;
}

static unsigned long ad_flyingtoasters_draw(Display *dpy, Window window, void *closure) {
  (void)dpy;
  (void)window;
  State *st = closure;
  const double started = double_time();
  if (st) ad_sim_step(&st->sim, blit, st);
  /* The runner waits this long after the push and counts only the push
   * against it, so subtract the step to hold 30 steps a second. */
  const long left = 33333 - (long)((double_time() - started) * 1e6);
  return left > 0 ? (unsigned long)left : 0;
}

static void ad_flyingtoasters_free(Display *dpy, Window window, void *closure) {
  (void)window;
  State *st = closure;
  if (!st) return;
  for (int s = 0; s < AD_SPRITES; s++)
    if (st->sprite[s]) XDestroyImage(st->sprite[s]);
  if (st->gc) XFreeGC(dpy, st->gc);
  free(st);
}

const HackEntry ad_flyingtoasters_hack = {
    "ad-flyingtoasters", ad_flyingtoasters_defaults, ad_flyingtoasters_init,
    ad_flyingtoasters_draw, ad_flyingtoasters_free};

#endif
