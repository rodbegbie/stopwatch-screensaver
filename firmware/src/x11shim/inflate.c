#include "x11shim/inflate.h"

#ifdef ESP_PLATFORM

#include <stdlib.h>

#include "rom/miniz.h"

int xshim_inflate(const unsigned char *src, size_t src_len, unsigned char *dst,
                  size_t want) {
  /* About 11 KB: too big for the 16 KB loop stack. */
  tinfl_decompressor *d = (tinfl_decompressor *)malloc(sizeof(*d));
  if (!d) return -1;
  tinfl_init(d);
  size_t in = src_len, out = want;
  const tinfl_status status =
      tinfl_decompress(d, src, &in, dst, dst, &out,
                       TINFL_FLAG_PARSE_ZLIB_HEADER |
                           TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  free(d);
  return status == TINFL_STATUS_DONE && out == want ? 0 : -1;
}

#else

#include <zlib.h>

int xshim_inflate(const unsigned char *src, size_t src_len, unsigned char *dst,
                  size_t want) {
  uLongf out = want;
  return uncompress(dst, &out, src, src_len) == Z_OK && out == want ? 0 : -1;
}

#endif
