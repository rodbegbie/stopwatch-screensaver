#include <stdlib.h>
#include <unity.h>

#include "GL/gl.h"
#include "core/canvas.h"
#include "zbuffer.h"

void setUp(void) {}
void tearDown(void) {}

/* The canvas keeps RGB565 with its bytes swapped, so TinyGL must write that
 * order itself. The width is 66, not a multiple of 4: the last column catches
 * TinyGL rounding the buffer width down to 64. White would be the same either
 * way round, so the colours are red and green. */
void test_clear_writes_canvas_byte_order(void) {
  Canvas cv;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 66, 8, malloc));
  ZBuffer *zb = ZB_open(66, 8, ZB_MODE_5R6G5B, cv.px);
  TEST_ASSERT_NOT_NULL(zb);
  glInit(zb);

  glClearColor(1, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), cv.px[0]);
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), cv.px[65]);

  glClearColor(0, 1, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), cv.px[0]);
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), cv.px[65]);

  glClose();
  ZB_close(zb);
  canvas_free(&cv);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_clear_writes_canvas_byte_order);
  return UNITY_END();
}
