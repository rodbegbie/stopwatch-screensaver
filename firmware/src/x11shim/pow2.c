#include "pow2.h"

int i_log2(size_t x) {
  int log = -1;
  while (x) {
    log++;
    x >>= 1;
  }
  return log;
}

size_t to_pow2(size_t x) {
  size_t p = 1;
  while (p < x) p <<= 1;
  return p;
}
