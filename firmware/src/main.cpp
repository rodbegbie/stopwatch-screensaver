#include <M5Unified.h>
#include <esp_heap_caps.h>

extern "C" {
#include "core/canvas.h"
#include "hacks/registry.h"
#include "runner/hack_runner.h"
}

/* Hacks run on loopTask, whose 8 KB default stack is too small: Rorschach
 * keeps a 9.6 KB array of rectangles on the stack. */
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static const int kSize = 466;
static const uint32_t kSliceUs = 10000;
static const uint32_t kStatsEveryMs = 5000;

static Canvas canvas;
static HackRunner *runner;
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

/* Returns true if a button switched hacks (which also resets the stats). */
static bool pollButtons() {
  bool switched = false;
  if (M5.BtnA.wasPressed()) {
    runner_next(runner);
    switched = true;
  }
  if (M5.BtnB.wasPressed()) {
    runner_prev(runner);
    switched = true;
  }
  if (switched) {
    Serial.printf("switch -> %s heap=%u psram=%u\n",
                  g_hacks[runner_index(runner)]->name,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    resetStats();
  }
  return switched;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setSwapBytes(false);

  if (canvas_init(&canvas, kSize, kSize, ps_malloc) != 0)
    halt("PSRAM alloc failed");
  runner = runner_create(&canvas);
  if (!runner || runner_start(runner, 0) != 0) halt("hack start failed");
  printStats("boot");
  resetStats();
}

void loop() {
  M5.update();
  pollButtons();

  uint32_t t0 = micros();
  unsigned long delayUs = runner_step(runner);
  uint32_t t1 = micros();
  M5.Display.pushImage(0, 0, kSize, kSize, canvas.px);
  uint32_t t2 = micros();
  frames++;
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
  }
  if (!switched) waitUs += micros() - t2;

  if (millis() - statsAt >= kStatsEveryMs) {
    printStats("run");
    resetStats();
  }
}
