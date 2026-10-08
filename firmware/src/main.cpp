#include <M5Unified.h>
#include <esp_heap_caps.h>

extern "C" {
#include "core/canvas.h"
#include "hacks/registry.h"
#include "runner/button_latch.h"
#include "runner/hack_runner.h"
}

/* Hacks run on loopTask, whose 8 KB default stack is too small: Rorschach
 * keeps a 9.6 KB array of rectangles on the stack. */
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static const int kSize = 466;
static const uint32_t kSliceUs = 10000;
static const uint32_t kStatsEveryMs = 5000;

/* M5Unified reads the buttons as active-low GPIOs, and only inside
 * M5.update(), so a press during a long hack step would go unseen. Interrupts
 * on the falling edge latch every press instead. */
static const int kPinButtonA = 2;
static const int kPinButtonB = 1;
static const uint32_t kButtonDebounceMs = 30;

static ButtonLatch latchA, latchB;

/* The last edges seen on either button, kept so a double switch can be
 * diagnosed from the serial log. Both interrupts run on one core and cannot
 * nest, so the single counter needs no lock. */
struct EdgeRecord {
  volatile uint32_t ms;
  volatile uint8_t button;
  volatile uint8_t released;
};
static const uint32_t kEdgeLogSize = 32;
static EdgeRecord edgeLog[kEdgeLogSize];
static volatile uint32_t edgeCount;
static volatile uint32_t lastEdgeMs;
static uint32_t edgePrinted;

static void IRAM_ATTR onButtonEdge(ButtonLatch *latch, int pin, char name) {
  uint32_t now = millis();
  bool pressed = gpio_get_level((gpio_num_t)pin) == 0;
  EdgeRecord &e = edgeLog[edgeCount % kEdgeLogSize];
  e.ms = now;
  e.button = (uint8_t)name;
  e.released = pressed ? 0 : 1;
  edgeCount = edgeCount + 1;
  lastEdgeMs = now;
  button_latch_edge(latch, pressed, now);
}
static void IRAM_ATTR onButtonA() { onButtonEdge(&latchA, kPinButtonA, 'A'); }
static void IRAM_ATTR onButtonB() { onButtonEdge(&latchB, kPinButtonB, 'B'); }

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

/* Prints edges not yet reported as "A v-120 A ^-70": v is the button going
 * down, ^ going up, and the number is milliseconds before now. */
static void printEdges() {
  uint32_t total = edgeCount;
  uint32_t from = edgePrinted;
  if (total - from > kEdgeLogSize) from = total - kEdgeLogSize;
  uint32_t now = millis();
  Serial.print("edges:");
  for (uint32_t i = from; i < total; i++) {
    const EdgeRecord &e = edgeLog[i % kEdgeLogSize];
    Serial.printf(" %c%s-%u", (char)e.button, e.released ? "^" : "v",
                  (unsigned)(now - e.ms));
  }
  Serial.println();
  edgePrinted = total;
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
    resetStats();
  }
  return switched;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setSwapBytes(false);

  button_latch_init(&latchA, kButtonDebounceMs);
  button_latch_init(&latchB, kButtonDebounceMs);
  attachInterrupt(digitalPinToInterrupt(kPinButtonA), onButtonA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(kPinButtonB), onButtonB, CHANGE);

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

  if (edgeCount != edgePrinted && millis() - lastEdgeMs > 400) printEdges();

  if (millis() - statsAt >= kStatsEveryMs) {
    printStats("run");
    resetStats();
  }
}
