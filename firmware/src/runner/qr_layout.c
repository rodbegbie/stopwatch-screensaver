#include "runner/qr_layout.h"

QrLayout qr_layout(int modules, int quiet, int side) {
  QrLayout l = {0, 0, 0};
  int scale = side / (modules + 2 * quiet);
  if (scale < 1) return l;
  l.scale = scale;
  l.size = modules * scale;
  l.origin = (side - l.size) / 2;
  return l;
}
