#include "utf8wc.h"

#include <stdlib.h>
#include <string.h>

char *utf8_to_latin1(const char *string, int ascii_p) {
  (void)ascii_p;
  for (const unsigned char *p = (const unsigned char *)string; *p; p++)
    if (*p > 127) return NULL;
  const size_t n = strlen(string) + 1;
  char *copy = (char *)malloc(n);
  if (copy) memcpy(copy, string, n);
  return copy;
}

int utf8_encode(unsigned long uc, char *out, long length) {
  const int n = uc < 0x80 ? 1 : uc < 0x800 ? 2 : uc < 0x10000 ? 3 : 4;
  if (n > length) return 0;
  if (n == 1) {
    out[0] = (char)uc;
  } else {
    static const unsigned char lead[] = {0, 0, 0xC0, 0xE0, 0xF0};
    for (int i = n - 1; i > 0; i--) {
      out[i] = (char)(0x80 | (uc & 0x3F));
      uc >>= 6;
    }
    out[0] = (char)(lead[n] | uc);
  }
  out[n] = 0;
  return n;
}

long utf8_decode(const unsigned char *in, long length, unsigned long *unicode_ret) {
  if (length < 1) return 0;
  const unsigned char b = in[0];
  const int n = b < 0xC0 ? 1 : b < 0xE0 ? 2 : b < 0xF0 ? 3 : 4;
  if (b < 0xC0) {
    *unicode_ret = b;
    return 1;
  }
  if (n > length) return 0;
  unsigned long uc = b & (0x3F >> (n - 1));
  for (int i = 1; i < n; i++) uc = uc << 6 | (in[i] & 0x3F);
  *unicode_ret = uc;
  return n;
}
