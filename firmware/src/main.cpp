#include <M5Unified.h>
#include <esp_heap_caps.h>

extern "C" {
#include "core/canvas.h"
#include "hacks/registry.h"
#include "runner/button_latch.h"
#include "runner/hack_runner.h"
#include "runner/overlay.h"
}

/* Hacks run on loopTask, whose 8 KB default stack is too small: Rorschach
 * keeps a 9.6 KB array of rectangles on the stack. */
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static const int kSize = 466;
static const uint32_t kSliceUs = 10000;
static const uint32_t kStatsEveryMs = 5000;
static const uint32_t kNameShownMs = 5000;
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
static uint32_t frames;
static uint32_t statsAt;
static uint32_t stepUs, pushUs, waitUs;

static void halt(const char *msg) {
  Serial.println(msg);
  M5.Display.fillScreen(0x000000);
  M5.Display.setTextColor(0xFFFFFF);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  for (;;) delay(1000);
}

static void resetStats() {
  frames = stepUs = pushUs = waitUs = 0;
  statsAt = millis();
}

/* step/push/wait are mean milliseconds per frame spent in the hack, in
 * pushImage, and waiting out what is left of the hack's requested delay after
 * the push (including button polling). */
static void printStats(const char *tag) {
  float n = frames ? (float)frames : 1.0f;
  Serial.printf(
      "%s %s fps=%.1f step=%.1fms push=%.1fms wait=%.1fms heap=%u psram=%u\n",
      tag, g_hacks[runner_index(runner)]->name,
      frames * 1000.0f / kStatsEveryMs, stepUs / n / 1000.0f,
      pushUs / n / 1000.0f, waitUs / n / 1000.0f, (unsigned)ESP.getFreeHeap(),
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/* Text goes straight to the display after each pushImage rather than into the
 * canvas: hacks draw incrementally, so anything stamped into the canvas would
 * stay there. A black outline keeps white text readable on bright hacks. */
static void drawOutlinedText(const char *text, int x, int y,
                             const lgfx::IFont *font) {
  M5.Display.setFont(font);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(0x000000);
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
      if (dx || dy) M5.Display.drawString(text, x + dx, y + dy);
  M5.Display.setTextColor(0xFFFFFF);
  M5.Display.drawString(text, x, y);
}

static void paintOverlay() {
  uint32_t now = millis();
  if (overlay_name_visible(&overlay, now))
    drawOutlinedText(g_hacks[runner_index(runner)]->name, kSize / 2, kNameY,
                     &fonts::DejaVu24);
  if (overlay_fps_visible(&overlay)) {
    char text[16];
    overlay_fps_text(&overlay, text, sizeof text);
    drawOutlinedText(text, kSize / 2, kFpsY, &fonts::DejaVu18);
  }
  overlay_drawn(&overlay, now);
}

static void present() {
  M5.Display.pushImage(0, 0, kSize, kSize, canvas.px);
  paintOverlay();
}

/* Returns true if a button switched hacks (which also resets the stats). */
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
    overlay_hack_started(&overlay, millis());
    resetStats();
  }
  return switched;
}

/* A tap anywhere toggles the fps readout. Call right after M5.update(), which
 * is the only place the touch edge is recorded. */
static void pollTouch() {
  if (M5.Touch.getDetail().wasPressed()) overlay_toggle_fps(&overlay);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setSwapBytes(false);

  button_latch_init(&latchA, kButtonSettleMs);
  button_latch_init(&latchB, kButtonSettleMs);
  xTaskCreatePinnedToCore(buttonTask, "buttons", 2048, nullptr, 2, nullptr, 0);

  if (canvas_init(&canvas, kSize, kSize, ps_malloc) != 0)
    halt("PSRAM alloc failed");
  runner = runner_create(&canvas);
  if (!runner || runner_start(runner, 0) != 0) halt("hack start failed");
  overlay_init(&overlay, kNameShownMs);
  overlay_hack_started(&overlay, millis());
  printStats("boot");
  resetStats();
}

void loop() {
  M5.update();
  pollButtons();
  pollTouch();

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
    switched = pollButtons();
    if (!switched) pollTouch();
    if (!switched && overlay_wants_redraw(&overlay, millis())) present();
  }
  if (!switched) waitUs += micros() - t2;

  if (millis() - statsAt >= kStatsEveryMs) {
    printStats("run");
    resetStats();
  }
}
