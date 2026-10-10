#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <string.h>

extern "C" {
#include "core/canvas.h"
#include "core/push_plan.h"
#include "hacks/registry.h"
#include "runner/button_latch.h"
#include "runner/hack_runner.h"
#include "runner/overlay.h"
#include "runner/qr_layout.h"
#include "runner/rotation.h"
#include "runner/start_pick.h"
}
#include "lgfx/utility/lgfx_qrcode.h"
/* The encoder's header defines generic names that clash with the rest of the
 * code. */
#undef MODE_NUMERIC
#undef MODE_ALPHANUMERIC
#undef MODE_BYTE
#undef ECC_LOW
#undef ECC_MEDIUM
#undef ECC_QUARTILE
#undef ECC_HIGH
#undef LOCK_VERSION

/* Hacks run on loopTask, whose 8 KB default stack is too small: Rorschach
 * keeps a 9.6 KB array of rectangles on the stack. */
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static const int kSize = 466;
static const uint32_t kSliceUs = 10000;
/* Build with -DSTART_HACK=\"galaxy\" (see AGENTS.md) to always start on one
 * hack; otherwise boot picks one at random. */
#ifdef START_HACK
static const char *const kForcedStart = START_HACK;
#else
static const char *const kForcedStart = nullptr;
#endif

/* The name badge text; build with -DBADGE_NAME=\"Ann\" to change it. Text wider
 * than the screen is clipped. */
#ifndef BADGE_NAME
#define BADGE_NAME "Rod"
#endif
static const char *const kBadgeName = BADGE_NAME;
static const int kBadgeScale = 10;

/* The QR code overlay's target; build with -DQR_URL=\"https://example.com\". */
#ifndef QR_URL
#define QR_URL "https://linkedin.com/in/rodbegbie"
#endif
static const char *const kQrUrl = QR_URL;
/* The screen is a circle, so the biggest square whose corners all show is the
 * one inscribed in it: 466 / sqrt(2) = 329.5, rounded down to an even number.
 * Two modules of white around the code (the standard asks for four, but phone
 * cameras cope) keep it as big as the circle allows. */
static const int kQrSide = 328;
static const int kQrQuietModules = 2;

static const uint32_t kStatsEveryMs = 5000;
static const uint32_t kNameShownMs = 5000;
static const uint32_t kBatteryEveryMs = 30000;

/* Seconds before moving on to the next hack; build with -DROTATE_SECONDS=5 to
 * shorten it for leak testing, or 0 to stay put. */
#ifndef ROTATE_SECONDS
#define ROTATE_SECONDS 90
#endif
static const uint32_t kRotateMs = ROTATE_SECONDS * 1000u;
static const int kNameY = kSize / 2;
static const int kFpsY = kSize - 40;

/* M5Unified reads the buttons as active-low GPIOs, and only inside
 * M5.update(), so a press during a long hack step would go unseen. A small
 * task on the otherwise idle core 0 samples both pins every few milliseconds
 * (it preempts nothing the hacks run on) and a latch keeps the presses. The
 * pin must hold its new level for the settle time, which rejects bounce. */
static const gpio_num_t kPinButtonA = GPIO_NUM_2;
static const gpio_num_t kPinButtonB = GPIO_NUM_1;
static const uint32_t kButtonPollMs = 5;
static const uint32_t kButtonSettleMs = 30;

static ButtonLatch latchA, latchB;

static void buttonTask(void *) {
  for (;;) {
    uint32_t now = millis();
    button_latch_sample(&latchA, gpio_get_level(kPinButtonA) == 0, now);
    button_latch_sample(&latchB, gpio_get_level(kPinButtonB) == 0, now);
    vTaskDelay(pdMS_TO_TICKS(kButtonPollMs));
  }
}

static Canvas canvas;
static HackRunner *runner;
static Overlay overlay;
static Rotation rotation;
static uint32_t frames;
static uint32_t statsAt;
static uint32_t stepUs, pushUs, waitUs, rowsPushed;
static PushRow pushRows[kSize];

/* The panel's tearing-effect line (OLED_TE, GPIO 38) pulses once a refresh, 60
 * times a second. A full push takes 32 ms: 20.6 ms copying into M5GFX's
 * framebuffer, then 11.7 ms flushing that to the panel. Started at a random
 * moment, the flush overlaps the panel's scan, so for one refresh the top of
 * the screen shows one picture and the rest the other, and a thin line that
 * moved a few pixels can lie in the gap and vanish for a frame. A hack with
 * HackEntry::sync_to_refresh starts every push kTeSyncOffsetUs after the TE
 * rising edge instead (see runner/te_sync.h).
 *
 * The offset came from filming a sweep of eight offsets across one refresh at
 * 120 fps and counting lines missing across the screen. 3.2 to 9.5 ms after the
 * edge had none in 39 updates; 15.8 and 1.1 ms had a seam, about 5 ms either
 * side of the pulse. 7.4 ms is the middle of the clean 12 ms. It holds for a
 * full-frame push, of which the copy is 20.6 ms: a push of another size moves
 * the flush, and the offset would have to be found again. */
#include <esp_timer.h>
extern "C" {
#include "runner/te_sync.h"
}
static const int kPinTE = 38;
static const uint32_t kTeSyncOffsetUs = 7400;
static const uint32_t kTeTimeoutUs = 100000;
static TeSync teSync;

#ifdef TE_DIAG
/* Build with -DTE_DIAG to log, every few seconds, the TE period and where in
 * the cycle each full push starts and flushes. */
static const int kTeRing = 512;
static volatile uint32_t teAt[kTeRing];
static volatile uint8_t teLevel[kTeRing];
static volatile uint32_t teHead;
static const int kPushLog = 64;
static uint32_t pushStart[kPushLog], pushFlush[kPushLog], pushEnd[kPushLog];
static int pushCount;
static uint32_t flushBeganUs;
static uint32_t teReportedHead;
#endif

static void IRAM_ATTR teIsr() {
  const uint32_t now = (uint32_t)esp_timer_get_time();
  const int level = gpio_get_level((gpio_num_t)kPinTE);
  te_sync_edge(&teSync, now, level);
#ifdef TE_DIAG
  uint32_t i = teHead;
  teAt[i % kTeRing] = now;
  teLevel[i % kTeRing] = (uint8_t)level;
  teHead = i + 1;
#endif
}

static void teInit() {
  te_sync_init(&teSync, kTeSyncOffsetUs, kTeTimeoutUs);
  pinMode(kPinTE, INPUT);
  attachInterrupt(kPinTE, teIsr, CHANGE);
}

#ifdef TE_DIAG
struct TeStat {
  uint32_t sum, min, max;
  int n;
};
static void teAdd(TeStat *s, uint32_t v) {
  if (!s->n || v < s->min) s->min = v;
  if (!s->n || v > s->max) s->max = v;
  s->sum += v;
  s->n++;
}
static void teShow(const char *name, const TeStat *s) {
  Serial.printf(" %s mean=%u min=%u max=%u", name,
                s->n ? (unsigned)(s->sum / s->n) : 0u, (unsigned)s->min,
                (unsigned)s->max);
}

/* Time since the latest rising edge at or before t, if there was one. */
static bool teSinceRise(const uint32_t *rise, int nr, uint32_t t, uint32_t *out) {
  bool found = false;
  for (int i = 0; i < nr; i++)
    if ((int32_t)(rise[i] - t) <= 0) {
      *out = t - rise[i];
      found = true;
    }
  return found;
}

static void teReport() {
  uint32_t head = teHead;
  uint32_t n = head - teReportedHead;
  if (n > (uint32_t)kTeRing) n = kTeRing;
  static uint32_t rise[kTeRing];
  int nr = 0;
  TeStat period = {}, startAt = {}, flushAt = {}, flushLen = {}, copyLen = {};
  for (uint32_t k = head - n; k != head; k++)
    if (teLevel[k % kTeRing]) rise[nr++] = teAt[k % kTeRing];
  for (int i = 1; i < nr; i++) teAdd(&period, rise[i] - rise[i - 1]);
  for (int p = 0; p < pushCount; p++) {
    uint32_t v;
    if (teSinceRise(rise, nr, pushStart[p], &v)) teAdd(&startAt, v);
    if (teSinceRise(rise, nr, pushFlush[p], &v)) teAdd(&flushAt, v);
    teAdd(&flushLen, pushEnd[p] - pushFlush[p]);
    teAdd(&copyLen, pushFlush[p] - pushStart[p]);
  }
  Serial.printf("te rises=%d period_us", nr);
  Serial.printf(" mean=%u", period.n ? (unsigned)(period.sum / period.n) : 0u);
  Serial.printf(" | pushes=%d", pushCount);
  teShow("start_after_rise_us", &startAt);
  teShow("flush_after_rise_us", &flushAt);
  teShow("copy_us", &copyLen);
  teShow("flush_us", &flushLen);
  Serial.printf(" synced_period_us=%u off_us=%u\n",
                (unsigned)te_sync_period_us(&teSync), (unsigned)teSync.offset_us);
  teReportedHead = head;
  pushCount = 0;
}
#endif

static void halt(const char *msg) {
  Serial.println(msg);
  M5.Display.fillScreen(0x000000);
  M5.Display.setTextColor(0xFFFFFF);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  for (;;) delay(1000);
}

static void resetStats() {
  frames = stepUs = pushUs = waitUs = rowsPushed = 0;
  statsAt = millis();
}

/* step/push/wait are mean milliseconds per frame spent in the hack, in
 * pushImage, and waiting out what is left of the hack's requested delay after
 * the push (including button polling). rows is the mean number of canvas rows
 * sent to the display per frame (466 is a full push), counting overlay redraws
 * made while waiting. */
static void printStats(const char *tag) {
  float n = frames ? (float)frames : 1.0f;
  Serial.printf(
      "%s %s fps=%.1f step=%.1fms push=%.1fms wait=%.1fms rows=%.0f heap=%u "
      "psram=%u\n",
      tag, g_hacks[runner_index(runner)]->name,
      frames * 1000.0f / kStatsEveryMs, stepUs / n / 1000.0f,
      pushUs / n / 1000.0f, waitUs / n / 1000.0f, rowsPushed / n,
      (unsigned)ESP.getFreeHeap(),
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/* Overlay text is stamped into the canvas just for the push, then the pixels
 * under it are put back. Drawing on the display after the push flickered,
 * because the next push wiped the text for a moment. The canvas must end up
 * as the hack left it, since hacks draw incrementally. M5Canvas wraps the
 * canvas buffer so M5GFX's fonts can draw into it; the DejaVu fonts are 1-bit,
 * so only pure black and white are written (the same in either byte order). */
static const int kPatchMaxH = 100;

struct Patch {
  int x, y, w, h;
  uint16_t *saved;
};

static M5Canvas stamp;
static Patch namePatch, infoPatch;
/* The finished QR code, black on white, drawn once so a frame only copies it. */
static uint16_t *qrPixels;

/* The outline is a black copy of the text at every offset within `outline`
 * pixels, which grows with the text size so it stays visible. */
static void stampText(Patch *p, const char *text, int cx, int cy,
                      const lgfx::IFont *font, int scale = 1) {
  int outline = 1 + scale / 4;
  int margin = outline + 1;
  stamp.setFont(font);
  stamp.setTextSize(scale);
  stamp.setTextDatum(middle_center);
  int w = stamp.textWidth(text) + 2 * margin;
  int h = stamp.fontHeight() + 2 * margin;
  p->w = w < kSize ? w : kSize;
  p->h = h < kPatchMaxH ? h : kPatchMaxH;
  p->x = cx - p->w / 2;
  p->y = cy - p->h / 2;
  canvas_copy_rect(&canvas, p->x, p->y, p->w, p->h, p->saved);
  stamp.setTextColor(0x000000);
  for (int dy = -outline; dy <= outline; dy++)
    for (int dx = -outline; dx <= outline; dx++)
      if (dx || dy) stamp.drawString(text, cx + dx, cy + dy);
  stamp.setTextColor(0xFFFFFF);
  stamp.drawString(text, cx, cy);
  stamp.setTextSize(1);
  canvas_mark_dirty(&canvas, p->x, p->y, p->w, p->h);
}

/* Encodes kQrUrl at the lowest version that holds it (M5GFX's own encoder, as
 * its qrcode() uses, with the lowest error correction so the modules stay big)
 * and paints it into qrPixels. Leaves qrPixels null if the URL is too long for
 * any version, or no memory is left. */
static void buildQr() {
  qrPixels = (uint16_t *)ps_malloc(kQrSide * kQrSide * sizeof(uint16_t));
  if (!qrPixels) return;
  for (uint8_t version = 1; version <= 40; version++) {
    uint8_t *modules = (uint8_t *)malloc(lgfx_qrcode_getBufferSize(version));
    if (!modules) break;
    QRCode code;
    int8_t err = lgfx_qrcode_initText(&code, modules, version, 0, kQrUrl);
    if (err == 0) {
      QrLayout l = qr_layout(code.size, kQrQuietModules, kQrSide);
      memset(qrPixels, 0xFF, kQrSide * kQrSide * sizeof(uint16_t));
      for (int my = 0; my < code.size && l.scale; my++)
        for (int mx = 0; mx < code.size; mx++) {
          if (!lgfx_qrcode_getModule(&code, mx, my)) continue;
          for (int py = 0; py < l.scale; py++)
            memset(qrPixels + (l.origin + my * l.scale + py) * kQrSide +
                       l.origin + mx * l.scale,
                   0, l.scale * sizeof(uint16_t));
        }
      free(modules);
      if (l.scale) {
        Serial.printf("QR %s: version %u, %d modules at %d px\n", kQrUrl,
                      (unsigned)version, (int)code.size, l.scale);
        return;
      }
      break;
    }
    free(modules);
  }
  Serial.printf("QR %s does not fit; no QR overlay\n", kQrUrl);
  free(qrPixels);
  qrPixels = nullptr;
}

static void stampQr(Patch *p) {
  p->w = p->h = kQrSide;
  p->x = p->y = (kSize - kQrSide) / 2;
  canvas_copy_rect(&canvas, p->x, p->y, p->w, p->h, p->saved);
  for (int row = 0; row < kQrSide; row++)
    memcpy(canvas.px + (p->y + row) * kSize + p->x, qrPixels + row * kQrSide,
           kQrSide * sizeof(uint16_t));
  canvas_mark_dirty(&canvas, p->x, p->y, p->w, p->h);
}

static void unstamp(Patch *p) {
  if (p->w) canvas_paste_rect(&canvas, p->x, p->y, p->w, p->h, p->saved);
  p->w = 0;
}

/* Whole rows are sent inside one startWrite: each call then costs about 5 us
 * instead of 16. */
static void sinkBegin(void *) { M5.Display.startWrite(); }
static void sinkEnd(void *) {
#ifdef TE_DIAG
  flushBeganUs = (uint32_t)esp_timer_get_time();
#endif
  M5.Display.endWrite();
}
static void sinkRow(void *, int y, int x0, int x1) {
  M5.Display.pushImage(x0, y, x1 - x0 + 1, 1, canvas.px + y * kSize + x0);
}
static void sinkAll(void *) {
#ifdef TE_DIAG
  /* An outer transaction holds the flush to the panel until endWrite, so the
   * copy into M5GFX's framebuffer and the flush can be timed apart. */
  M5.Display.startWrite();
  M5.Display.pushImage(0, 0, kSize, kSize, canvas.px);
  flushBeganUs = (uint32_t)esp_timer_get_time();
  M5.Display.endWrite();
#else
  M5.Display.pushImage(0, 0, kSize, kSize, canvas.px);
#endif
}
static const PushSink pushSink = {nullptr, sinkBegin, sinkRow, sinkAll, sinkEnd};

static void present() {
  uint32_t now = millis();
  if (overlay_name_visible(&overlay, now))
    stampText(&namePatch, g_hacks[runner_index(runner)]->name, kSize / 2,
              kNameY, &fonts::DejaVu24);
  if (overlay_badge_visible(&overlay))
    stampText(&namePatch, kBadgeName, kSize / 2, kSize / 2,
              &fonts::Font8x8C64, kBadgeScale);
  if (overlay_qr_visible(&overlay) && qrPixels) stampQr(&infoPatch);
  if (overlay_fps_visible(&overlay) || overlay_battery_visible(&overlay)) {
    char text[16];
    if (overlay_fps_visible(&overlay))
      overlay_fps_text(&overlay, text, sizeof text);
    else
      overlay_battery_text(&overlay, text, sizeof text);
    stampText(&infoPatch, text, kSize / 2, kFpsY, &fonts::DejaVu18);
  }
  overlay_drawn(&overlay, now);
  if (g_hacks[runner_index(runner)]->sync_to_refresh) {
    /* After the overlay is stamped, so stamping does not move the start. */
    const uint32_t syncWaitUs =
        te_sync_wait_us(&teSync, (uint32_t)esp_timer_get_time());
    if (syncWaitUs) delayMicroseconds(syncWaitUs);
  }
#ifdef TE_DIAG
  uint32_t pushT0 = (uint32_t)esp_timer_get_time();
#endif
  int pushedRows = push_present(&canvas, pushRows, &pushSink);
  rowsPushed += pushedRows;
#ifdef TE_DIAG
  if (pushedRows > 300 && pushCount < kPushLog) {
    pushStart[pushCount] = pushT0;
    pushFlush[pushCount] = flushBeganUs;
    pushEnd[pushCount] = (uint32_t)esp_timer_get_time();
    pushCount++;
  }
#endif
  unstamp(&infoPatch);
  unstamp(&namePatch);
}

/* Whatever started the hack, the name shows again, the rotation countdown
 * restarts and the stats begin afresh. */
static void hackStarted() {
  uint32_t now = millis();
  overlay_hack_started(&overlay, now);
  rotation_reset(&rotation, now);
  resetStats();
}

/* Returns true if a button switched hacks. */
static bool pollButtons() {
  bool switched = false;
  uint32_t pressedAt = 0;
  if (button_latch_take(&latchA, &pressedAt)) {
    runner_next(runner);
    switched = true;
  }
  if (button_latch_take(&latchB, &pressedAt)) {
    runner_prev(runner);
    switched = true;
  }
  if (switched) {
    Serial.printf("switch -> %s press_waited=%ums heap=%u psram=%u\n",
                  g_hacks[runner_index(runner)]->name,
                  (unsigned)(millis() - pressedAt),
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    hackStarted();
  }
  return switched;
}

/* Returns true if the timer moved on to the next hack. */
static bool pollRotation() {
  if (!rotation_due(&rotation, millis())) return false;
  runner_next(runner);
  Serial.printf("rotate -> %s heap=%u psram=%u\n",
                g_hacks[runner_index(runner)]->name, (unsigned)ESP.getFreeHeap(),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  hackStarted();
  return true;
}

/* A tap anywhere cycles the readout: nothing, name badge, QR code, fps,
 * battery. Call
 * right after M5.update(), which is the only place the touch edge is
 * recorded. */
static void pollTouch() {
  if (M5.Touch.getDetail().wasPressed()) overlay_cycle_info(&overlay);
}

/* The fuel gauge is read over I2C through the power chip, which can fail the
 * first transaction after idle, so read it every 30 s, not every frame. A
 * failed read is negative and the overlay keeps the last good level. */
static void pollBattery() {
  static uint32_t lastReadMs;
  static bool readOnce;
  uint32_t now = millis();
  if (readOnce && now - lastReadMs < kBatteryEveryMs) return;
  readOnce = true;
  lastReadMs = now;
  overlay_set_battery(&overlay, M5.Power.getBatteryLevel());
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setSwapBytes(false);
  teInit();

  button_latch_init(&latchA, kButtonSettleMs);
  button_latch_init(&latchB, kButtonSettleMs);
  xTaskCreatePinnedToCore(buttonTask, "buttons", 2048, nullptr, 2, nullptr, 0);

  if (canvas_init(&canvas, kSize, kSize, ps_malloc) != 0)
    halt("PSRAM alloc failed");
  stamp.setBuffer(canvas.px, kSize, kSize, 16);
  namePatch.saved = (uint16_t *)ps_malloc(kSize * kPatchMaxH * sizeof(uint16_t));
  size_t infoPixels = kQrSide * kQrSide > kSize * kPatchMaxH
                          ? kQrSide * kQrSide
                          : kSize * kPatchMaxH;
  infoPatch.saved = (uint16_t *)ps_malloc(infoPixels * sizeof(uint16_t));
  if (!namePatch.saved || !infoPatch.saved) halt("PSRAM alloc failed");
  buildQr();
  runner = runner_create(&canvas);
  if (!runner) halt("hack start failed");
  int first = start_pick_index(g_hacks, g_hack_count, kForcedStart, esp_random());
  if (kForcedStart && start_find_hack(g_hacks, g_hack_count, kForcedStart) < 0)
    Serial.printf("START_HACK \"%s\" not found, starting at random\n",
                  kForcedStart);
  if (runner_start(runner, first) != 0) halt("hack start failed");
  overlay_init(&overlay, kNameShownMs, qrPixels != nullptr);
  rotation_init(&rotation, kRotateMs, millis());
  hackStarted();
  printStats("boot");
  resetStats();
}

void loop() {
  M5.update();
  if (!pollButtons()) pollRotation();
  pollTouch();
  pollBattery();

  uint32_t t0 = micros();
  unsigned long delayUs = runner_step(runner);
  uint32_t t1 = micros();
  present();
  uint32_t t2 = micros();
  frames++;
  overlay_frame(&overlay, millis());
  stepUs += t1 - t0;
  pushUs += t2 - t1;

  /* Only the push counts against the delay. A hack's delay is the pause after
   * it draws, so a hack that asks for a one-second hold must still get it
   * however long its own step took. */
  unsigned long waitUsTarget = runner_remaining_delay_us(delayUs, t2 - t1);
  uint32_t waitedUs = 0;
  bool switched = false;
  while (!switched && waitedUs < waitUsTarget) {
    uint32_t slice =
        waitUsTarget - waitedUs < kSliceUs ? waitUsTarget - waitedUs : kSliceUs;
    delayMicroseconds(slice);
    waitedUs += slice;
    M5.update();
    switched = pollButtons() || pollRotation();
    if (!switched) {
      pollTouch();
      pollBattery();
    }
    if (!switched && overlay_wants_redraw(&overlay, millis())) present();
  }
  if (!switched) waitUs += micros() - t2;

  if (millis() - statsAt >= kStatsEveryMs) {
    printStats("run");
#ifdef TE_DIAG
    teReport();
#endif
    resetStats();
  }
}
