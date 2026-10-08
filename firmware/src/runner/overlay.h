#ifndef RUNNER_OVERLAY_H
#define RUNNER_OVERLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decides what text to draw over the hack: its name for a few seconds after it
 * starts, and optionally the frame rate. Holds no drawing code, so it can be
 * tested on the host. Times are millisecond counter values that may wrap. */
typedef struct {
  uint32_t name_ms;
  uint32_t name_started_ms;
  uint32_t fps_window_start_ms;
  uint32_t fps_window_frames;
  float fps;
  bool name_armed;
  bool has_fps;
  bool fps_on;
  bool drawn_name;
  bool drawn_fps;
} Overlay;

void overlay_init(Overlay *o, uint32_t name_ms);

/* A hack (re)started: show its name again and measure its frame rate afresh.
 * The fps on/off choice is kept. */
void overlay_hack_started(Overlay *o, uint32_t now_ms);

void overlay_toggle_fps(Overlay *o);

/* One frame was drawn at now_ms. */
void overlay_frame(Overlay *o, uint32_t now_ms);

bool overlay_name_visible(const Overlay *o, uint32_t now_ms);
bool overlay_fps_visible(const Overlay *o);

/* "12.3 fps", or "-- fps" until a full second of frames has been measured.
 * Always NUL-terminated and truncated to fit. */
void overlay_fps_text(const Overlay *o, char *buf, size_t size);

/* The overlay on screen no longer matches what should be shown (the name
 * expired or fps was toggled), so the frame needs repainting. Call
 * overlay_drawn after painting. */
bool overlay_wants_redraw(const Overlay *o, uint32_t now_ms);
void overlay_drawn(Overlay *o, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
