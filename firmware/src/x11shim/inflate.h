/* Unpacks one zlib stream (RFC 1950) into a buffer of exactly the size it
 * should fill. Ours; the device uses the ESP32-S3 ROM's tinfl, the host
 * zlib. */
#ifndef XSHIM_INFLATE_H
#define XSHIM_INFLATE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 0 when the stream is whole and unpacks to exactly `want` bytes; -1 when it
 * is corrupt, cut short, shorter or longer than that, or memory runs out. */
int xshim_inflate(const unsigned char *src, size_t src_len, unsigned char *dst,
                  size_t want);

#ifdef __cplusplus
}
#endif

#endif
