#include <M5Unified.h>
#include <esp_heap_caps.h>

static const uint32_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE};
static int step = 0;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
}

void loop() {
  M5.Display.fillScreen(colors[step % 3]);
  Serial.printf("hello step=%d heap=%u psram=%u\n", step,
                (unsigned)ESP.getFreeHeap(),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  step++;
  delay(1000);
}
