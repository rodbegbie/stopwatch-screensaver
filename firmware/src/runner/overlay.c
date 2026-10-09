#include "runner/overlay.h"

#include <stdio.h>

#define FPS_WINDOW_MS 1000u

void overlay_init(Overlay *o, uint32_t name_ms) {
  *o = (Overlay){.name_ms = name_ms, .battery_pct = -1, .drawn_battery_pct = -1};
}

void overlay_hack_started(Overlay *o, uint32_t now_ms) {
  o->name_armed = true;
  o->name_started_ms = now_ms;
  o->fps_window_start_ms = now_ms;
  o->fps_window_frames = 0;
  o->has_fps = false;
}

void overlay_cycle_info(Overlay *o) {
  o->info = o->info == INFO_BATTERY ? INFO_NONE : o->info + 1;
}

void overlay_set_battery(Overlay *o, int pct) {
  if (pct < 0) return;
  o->battery_pct = pct > 100 ? 100 : pct;
}

void overlay_frame(Overlay *o, uint32_t now_ms) {
  o->fps_window_frames++;
  uint32_t elapsed = now_ms - o->fps_window_start_ms;
  if (elapsed >= FPS_WINDOW_MS) {
    o->fps = o->fps_window_frames * 1000.0f / elapsed;
    o->has_fps = true;
    o->fps_window_start_ms = now_ms;
    o->fps_window_frames = 0;
  }
}

bool overlay_name_visible(const Overlay *o, uint32_t now_ms) {
  return o->info != INFO_NAME && o->name_armed &&
         now_ms - o->name_started_ms < o->name_ms;
}

bool overlay_fps_visible(const Overlay *o) { return o->info == INFO_FPS; }

bool overlay_badge_visible(const Overlay *o) { return o->info == INFO_NAME; }

bool overlay_battery_visible(const Overlay *o) {
  return o->info == INFO_BATTERY;
}

void overlay_fps_text(const Overlay *o, char *buf, size_t size) {
  if (o->has_fps)
    snprintf(buf, size, "%.1f fps", (double)o->fps);
  else
    snprintf(buf, size, "-- fps");
}

void overlay_battery_text(const Overlay *o, char *buf, size_t size) {
  if (o->battery_pct >= 0)
    snprintf(buf, size, "%d%%", o->battery_pct);
  else
    snprintf(buf, size, "--%%");
}

bool overlay_wants_redraw(const Overlay *o, uint32_t now_ms) {
  return o->drawn_name != overlay_name_visible(o, now_ms) ||
         o->drawn_info != o->info ||
         (overlay_battery_visible(o) && o->drawn_battery_pct != o->battery_pct);
}

void overlay_drawn(Overlay *o, uint32_t now_ms) {
  o->drawn_name = overlay_name_visible(o, now_ms);
  o->drawn_info = o->info;
  o->drawn_battery_pct = o->battery_pct;
}
