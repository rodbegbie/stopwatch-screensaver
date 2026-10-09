#ifndef RUNNER_OVERLAY_H
#define RUNNER_OVERLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The readout shown under the name, cycled by a tap. */
typedef enum { INFO_NONE, INFO_FPS, INFO_BATTERY } OverlayInfo;

/* Decides what text to draw over the hack: its name for a few seconds after it
 * starts, and optionally the frame rate or battery level. Holds no drawing
 * code, so it can be tested on the host. Times are millisecond counter values
 * that may wrap. */
typedef struct {
  uint32_t name_ms;
  uint32_t name_started_ms;
  uint32_t fps_window_start_ms;
  uint32_t fps_window_frames;
  float fps;
  bool name_armed;
  bool has_fps;
  OverlayInfo info;
  int battery_pct;
  bool drawn_name;
  OverlayInfo drawn_info;
  int drawn_battery_pct;
} Overlay;

void overlay_init(Overlay *o, uint32_t name_ms);

/* A hack (re)started: show its name again and measure its frame rate afresh.
 * The chosen readout is kept. */
void overlay_hack_started(Overlay *o, uint32_t now_ms);

/* Next readout: nothing, fps, battery, nothing... Starts at nothing. */
void overlay_cycle_info(Overlay *o);

/* A battery reading in percent. Above 100 is clamped; a negative value means
 * the read failed, and the last good level stays. */
void overlay_set_battery(Overlay *o, int pct);

/* One frame was drawn at now_ms. */
void overlay_frame(Overlay *o, uint32_t now_ms);

bool overlay_name_visible(const Overlay *o, uint32_t now_ms);
bool overlay_fps_visible(const Overlay *o);
bool overlay_battery_visible(const Overlay *o);

/* "12.3 fps", or "-- fps" until a full second of frames has been measured.
 * Always NUL-terminated and truncated to fit. */
void overlay_fps_text(const Overlay *o, char *buf, size_t size);

/* "91%", or "--%" until a level has been read. Always NUL-terminated and
 * truncated to fit. */
void overlay_battery_text(const Overlay *o, char *buf, size_t size);

/* The overlay on screen no longer matches what should be shown (the name
 * expired, the readout changed or the shown battery level moved), so the frame needs repainting. Call
 * overlay_drawn after painting. */
bool overlay_wants_redraw(const Overlay *o, uint32_t now_ms);
void overlay_drawn(Overlay *o, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
