#include <M5Unified.h>
#include <esp_heap_caps.h>

extern "C" {
#include "core/canvas.h"
#include "hacks/registry.h"
#include "runner/hack_runner.h"
}

static const int kSize = 466;
static const uint32_t kSliceUs = 10000;
static const uint32_t kStatsEveryMs = 5000;

static Canvas canvas;
static HackRunner *runner;
static uint32_t frames;
static uint32_t statsAt;

static void halt(const char *msg) {
  Serial.println(msg);
  M5.Display.fillScreen(0x000000);
  M5.Display.setTextColor(0xFFFFFF);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  for (;;) delay(1000);
}

static void printStats(const char *tag) {
  Serial.printf("%s %s fps=%.1f heap=%u psram=%u\n", tag,
                g_hacks[runner_index(runner)]->name,
                frames * 1000.0f / kStatsEveryMs, (unsigned)ESP.getFreeHeap(),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static void pollButtons() {
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
    frames = 0;
    statsAt = millis();
  }
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
  statsAt = millis();
}

void loop() {
  M5.update();
  pollButtons();

  unsigned long delayUs = runner_step(runner);
  M5.Display.pushImage(0, 0, kSize, kSize, canvas.px);
  frames++;

  uint32_t waitedUs = 0;
  while (waitedUs < delayUs) {
    uint32_t slice = delayUs - waitedUs < kSliceUs ? delayUs - waitedUs : kSliceUs;
    delayMicroseconds(slice);
    waitedUs += slice;
    M5.update();
    pollButtons();
  }

  if (millis() - statsAt >= kStatsEveryMs) {
    printStats("run");
    frames = 0;
    statsAt = millis();
  }
}
